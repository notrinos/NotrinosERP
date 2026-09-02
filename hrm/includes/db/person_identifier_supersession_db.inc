<?php
/** HRM-FND-004 governed identifier supersession command. */
$hrm_identifier_supersession_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
foreach (array(
    '/hrm/includes/db/employee_person_sensitive_db.inc',
    '/hrm/includes/db/person_identifier_verification_request_db.inc'
) as $hrm_identifier_supersession_dep) {
    $candidate = $hrm_identifier_supersession_root.$hrm_identifier_supersession_dep;
    if (is_file($candidate)) include_once($candidate);
}
unset($hrm_identifier_supersession_root, $hrm_identifier_supersession_dep, $candidate);

if (!defined('HRM_IDENTIFIER_SUPERSESSION_ACTION_CLASS'))
    define('HRM_IDENTIFIER_SUPERSESSION_ACTION_CLASS', 'hrm.person.identifier.supersession.create');
if (!defined('HRM_IDENTIFIER_SUPERSESSION_HASH_CONTRACT'))
    define('HRM_IDENTIFIER_SUPERSESSION_HASH_CONTRACT', 'notrinoserp.hrm-fnd-004.identifier-supersession');
if (!defined('HRM_IDENTIFIER_SUPERSESSION_HASH_VERSION'))
    define('HRM_IDENTIFIER_SUPERSESSION_HASH_VERSION', '1');

function hrm_identifier_supersession_valid_date($value, $allow_null = true)
{
    if ($value === null || $value === '') return $allow_null ? null : false;
    if (!is_string($value)) return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    return $date && $date->format('Y-m-d') === $value ? $value : false;
}

function hrm_identifier_supersession_action_hash($company, $predecessor_id, $predecessor_row_version,
    $identifier_type, $issuing_jurisdiction, $lookup_token, $valid_to)
{
    $predecessor_id = hrm_identifier_verification_request_positive_int($predecessor_id);
    $predecessor_row_version = hrm_identifier_verification_request_positive_int($predecessor_row_version);
    $identifier_type = is_string($identifier_type) && array_key_exists($identifier_type, hrm_person_identifier_field_map())
        ? $identifier_type : false;
    $issuing_jurisdiction = hrm_person_sensitive_issuing_jurisdiction($issuing_jurisdiction, false);
    $valid_to = hrm_identifier_supersession_valid_date($valid_to, true);
    if ((int)$company < 0 || $predecessor_id === false || $predecessor_row_version === false
        || $identifier_type === false || $issuing_jurisdiction === false || $valid_to === false
        || !is_string($lookup_token) || preg_match('/^[a-f0-9]{64}$/D', $lookup_token) !== 1)
        return false;
    return hash('sha256', HRM_IDENTIFIER_SUPERSESSION_HASH_CONTRACT
        .'|v='.HRM_IDENTIFIER_SUPERSESSION_HASH_VERSION.'|company='.(int)$company
        .'|action='.HRM_IDENTIFIER_SUPERSESSION_ACTION_CLASS.'|predecessor_id='.$predecessor_id
        .'|predecessor_row_version='.$predecessor_row_version.'|identifier_type='.$identifier_type
        .'|issuing_jurisdiction='.$issuing_jurisdiction.'|lookup_token='.$lookup_token
        .'|valid_to='.($valid_to === null ? '' : $valid_to));
}

function hrm_identifier_supersession_assurance_event_for_update($session_id, $action_hash)
{
    if ((int)$session_id <= 0 || !federation_assurance_consumer_hex64($action_hash)) return false;
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,issued_at,expires_at,consumed_at FROM "
        .TB_PREF."user_session_assurance_events WHERE session_id=".(int)$session_id." AND action_class="
        .db_escape(HRM_IDENTIFIER_SUPERSESSION_ACTION_CLASS)." AND action_hash=".db_escape($action_hash)
        ." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE", 'could not lock identifier supersession assurance event');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row) return null;
    return db_fetch_assoc($result) === false ? $row : false;
}

function hrm_identifier_supersession_consume_assurance($company, $action_hash, $now_epoch)
{
    $session_id = federation_assurance_consumer_current_session_id();
    if ($session_id <= 0) return false;
    $custody = federation_assurance_consumer_custody_for_update($session_id);
    if (!federation_assurance_consumer_current_custody_valid($custody, $now_epoch, true)) return false;
    $event = hrm_identifier_supersession_assurance_event_for_update($session_id, $action_hash);
    if (!is_array($event) || (int)$event['assurance_level'] !== 2
        || (string)$event['assurance_method'] !== 'oidc_acr_amr'
        || (string)$event['action_class'] !== HRM_IDENTIFIER_SUPERSESSION_ACTION_CLASS
        || !session_registry_hash_equals((string)$event['action_hash'], $action_hash)
        || $event['consumed_at'] !== null) return false;
    $issued = strtotime((string)$event['issued_at'].' UTC');
    $expires = strtotime((string)$event['expires_at'].' UTC');
    if ($issued === false || $expires === false || $now_epoch < $issued || $now_epoch >= $expires) return false;
    $consumed_at = gmdate('Y-m-d H:i:s', $now_epoch);
    if (!db_query("UPDATE ".TB_PREF."user_session_assurance_events SET consumed_at=".db_escape($consumed_at)
        ." WHERE assurance_event_id=".(int)$event['assurance_event_id']." AND consumed_at IS NULL AND expires_at>".db_escape($consumed_at),
        'could not consume identifier supersession assurance') || (int)db_num_affected_rows() !== 1) return false;
    $restore = federation_assurance_consumer_restore_level_one_sql($custody, $now_epoch);
    if ($restore === false || !db_query("UPDATE ".TB_PREF."user_sessions SET ".$restore." WHERE id=".$session_id
        ." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'",
        'could not reset identifier supersession session assurance') || (int)db_num_affected_rows() !== 1) return false;
    if (append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,'entity_type'=>'security_session_assurance',
        'entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),
        'action'=>'security.session.assurance.consumed','reason_code'=>'hrm_fnd_004_identifier_supersession',
        'before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),
        'restricted_diff'=>array('action_class'=>HRM_IDENTIFIER_SUPERSESSION_ACTION_CLASS,'action_hash'=>$action_hash,
            'single_use_event_consumed'=>true,'session_assurance_reset_to_level_one'=>true,
            'identifier_value_stored'=>false,'identifier_value_revealed'=>false,'payroll_authority_changed'=>false,
            'payment_authority_changed'=>false,'accounting_authority_changed'=>false))) === false) return false;
    return (int)$event['assurance_event_id'];
}

/**
 * Replace one current verified identifier with a new unverified successor.
 * The successor must later traverse the existing verification request/evidence/promotion workflow.
 */
function supersede_hrm_person_identifier($predecessor_id, $predecessor_row_version, $new_value,
    $issuing_jurisdiction, $valid_to = null, $now_epoch = null)
{
    $predecessor_id = hrm_identifier_verification_request_positive_int($predecessor_id);
    $predecessor_row_version = hrm_identifier_verification_request_positive_int($predecessor_row_version);
    $actor = hrm_identifier_verification_request_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $now_epoch = $now_epoch === null ? time() : (int)$now_epoch;
    $new_value = hrm_person_sensitive_value($new_value, 255);
    $jurisdiction = hrm_person_sensitive_issuing_jurisdiction($issuing_jurisdiction, false);
    $normalized_valid_to = hrm_identifier_supersession_valid_date($valid_to, true);
    if ($predecessor_id === false || $predecessor_row_version === false || $actor <= 0 || $new_value === false
        || $new_value === '' || $jurisdiction === false || $normalized_valid_to === false || $now_epoch <= 0
        || !hrm_identifier_verification_request_permission()) return false;

    $active_key = hrm_person_sensitive_active_key(null, false);
    if (!is_array($active_key) || !isset($active_key['key_id'],$active_key['key'])) return false;

    begin_transaction(); $ok = false; $result_value = false;
    do {
        $r = db_query("SELECT identifier_id,person_id,identifier_type,effective_from,effective_to,valid_to,verification_status,verification_method,approval_status,row_version FROM "
            .TB_PREF."hrm_person_identifiers WHERE identifier_id=".$predecessor_id." LIMIT 2 FOR UPDATE",
            'could not lock identifier supersession predecessor');
        $predecessor = $r ? db_fetch_assoc($r) : false;
        if (!$predecessor || db_fetch_assoc($r) !== false || (int)$predecessor['row_version'] !== $predecessor_row_version
            || (string)$predecessor['verification_status'] !== 'verified' || (string)$predecessor['approval_status'] !== 'approved'
            || (string)$predecessor['verification_method'] === '') break;
        $identifier_type = (string)$predecessor['identifier_type'];
        if (!hrm_person_sensitive_semantic_value_is_valid('identifier',$identifier_type,$new_value,$jurisdiction)) break;
        $masked = hrm_person_sensitive_mask($new_value);
        if ($masked === false) break;

        $s = db_query("SELECT identifier_id,person_id,identifier_type,issuing_jurisdiction,value_ciphertext,masked_value,lookup_token,key_id,valid_to,effective_from,effective_to,source,verification_status,verification_method,approval_status,row_version,created_by FROM "
            .TB_PREF."hrm_person_identifiers WHERE supersedes_identifier_id=".$predecessor_id." LIMIT 2 FOR UPDATE",
            'could not lock identifier supersession successor');
        $successor = $s ? db_fetch_assoc($s) : false;
        if ($successor && db_fetch_assoc($s) !== false) break;
        if ($successor) {
            $context = hrm_person_sensitive_context((int)$predecessor['person_id'],'identifier',$identifier_type);
            $successor_key = (string)$successor['key_id'] === (string)$active_key['key_id']
                ? $active_key['key'] : hrm_person_sensitive_key((string)$successor['key_id']);
            $successor_plaintext = ($context === false || $successor_key === false) ? false
                : hrm_person_sensitive_decrypt($successor['value_ciphertext'],$context,$successor_key);
            $successor_token = $successor_plaintext === false ? false
                : hrm_person_sensitive_lookup_token($identifier_type,$successor_plaintext,$successor_key);
            if ($predecessor['effective_to'] === null || (int)$successor['person_id'] !== (int)$predecessor['person_id']
                || (string)$successor['identifier_type'] !== $identifier_type || (string)$successor['issuing_jurisdiction'] !== $jurisdiction
                || $successor_plaintext === false || !domain_audit_hash_equals($successor_plaintext,$new_value)
                || $successor_token === false || !domain_audit_hash_equals((string)$successor['lookup_token'],$successor_token)
                || (string)$successor['masked_value'] !== $masked || (string)$successor['source'] !== 'governed_supersession'
                || (string)$successor['verification_status'] !== 'unverified' || (string)$successor['verification_method'] !== ''
                || (string)$successor['approval_status'] !== 'unapproved' || (int)$successor['row_version'] !== 1
                || (int)$successor['created_by'] !== $actor || (($successor['valid_to'] === null ? null : (string)$successor['valid_to']) !== $normalized_valid_to)) break;
            $ok = true; $result_value = array('identifier_id'=>(int)$successor['identifier_id'],
                'supersedes_identifier_id'=>$predecessor_id,'row_version'=>1,'status'=>'already_superseded'); break;
        }
        if ($predecessor['effective_to'] !== null) break;
        $token = hrm_person_sensitive_lookup_token($identifier_type,$new_value,$active_key['key']);
        if ($token === false) break;
        $action_hash = hrm_identifier_supersession_action_hash($company,$predecessor_id,$predecessor_row_version,
            $identifier_type,$jurisdiction,$token,$normalized_valid_to);
        if ($action_hash === false) break;
        $assurance_event_id = hrm_identifier_supersession_consume_assurance($company,$action_hash,$now_epoch);
        if ($assurance_event_id === false) break;
        $now = gmdate('Y-m-d H:i:s',$now_epoch);
        if (!db_query("UPDATE ".TB_PREF."hrm_person_identifiers SET effective_to=".db_escape($now)
            ." WHERE identifier_id=".$predecessor_id." AND row_version=".$predecessor_row_version." AND effective_to IS NULL"
            ." AND verification_status='verified' AND approval_status='approved'", 'could not close superseded identifier')
            || (int)db_num_affected_rows() !== 1) break;
        $context = hrm_person_sensitive_context((int)$predecessor['person_id'],'identifier',$identifier_type);
        $ciphertext = hrm_person_sensitive_encrypt($new_value,$context,$active_key['key']);
        if ($context === false || $ciphertext === false) break;
        $sql = "INSERT INTO ".TB_PREF."hrm_person_identifiers (person_id,identifier_type,issuing_jurisdiction,value_ciphertext,masked_value,lookup_token,key_id,valid_to,effective_from,effective_to,source,verification_status,verification_method,approval_status,row_version,supersedes_identifier_id,created_by) VALUES ("
            .(int)$predecessor['person_id'].','.db_escape($identifier_type).','.db_escape($jurisdiction).','.db_escape($ciphertext).','
            .db_escape($masked).','.db_escape($token).','.db_escape((string)$active_key['key_id']).','
            .($normalized_valid_to===null?'NULL':db_escape($normalized_valid_to)).','.db_escape($now).",NULL,'governed_supersession','unverified','', 'unapproved',1,"
            .$predecessor_id.','.$actor.")";
        if (!db_query($sql,'could not create governed identifier successor')) break;
        $successor_id = function_exists('db_insert_id') ? (int)db_insert_id() : 0;
        if ($successor_id <= 0) break;
        if (append_domain_audit_event(array(
            'company_id'=>$company,'legal_entity_id'=>0,'entity_type'=>'hrm_person_identifier_supersession',
            'entity_key'=>array('predecessor_identifier_id'=>$predecessor_id,'successor_identifier_id'=>$successor_id),
            'action'=>'hrm.person.identifier.supersession.created','reason_code'=>'hrm_fnd_004_identifier_supersession',
            'before'=>array('predecessor_row_version'=>$predecessor_row_version,'predecessor_current'=>true,
                'predecessor_verification_status'=>'verified','predecessor_approval_status'=>'approved'),
            'after'=>array('predecessor_current'=>false,'successor_row_version'=>1,'successor_verification_status'=>'unverified',
                'successor_approval_status'=>'unapproved'),
            'restricted_diff'=>array('assurance_event_id'=>$assurance_event_id,'action_hash'=>$action_hash,
                'one_direct_successor'=>true,'successor_requires_existing_verification_workflow'=>true,
                'identifier_value_stored'=>true,'identifier_value_audited'=>false,'identifier_value_revealed'=>false,
                'verification_promoted'=>false,'approval_promoted'=>false,'payment_authority_changed'=>false,
                'payroll_authority_changed'=>false,'accounting_authority_changed'=>false))) === false) break;
        $ok = true; $result_value = array('identifier_id'=>$successor_id,'supersedes_identifier_id'=>$predecessor_id,
            'row_version'=>1,'status'=>'created');
    } while (false);
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $result_value : false;
}
