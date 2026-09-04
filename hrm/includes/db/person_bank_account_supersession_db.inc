<?php
/**
 * HRM-FND-004 governed Person bank-account supersession maker command.
 *
 * This internal command creates an unverified/unapproved normalized successor
 * only. It never changes legacy employee bank/payment authority, never reveals
 * a stored bank value, and never promotes verification or approval state.
 */
$hrm_person_bank_supersession_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_person_bank_supersession_dependencies = array(
    array('/hrm/includes/db/employee_person_bank_db.inc', 'hrm_person_bank_payload_json'),
    array('/hrm/includes/hrm_security.inc', 'hrm_user_can_access_sensitive_field'),
    array('/includes/federation_assurance_consumer.inc', 'federation_assurance_consumer_current_session_id'),
    array('/includes/db/domain_audit_db.inc', 'append_domain_audit_event'),
);
foreach ($hrm_person_bank_supersession_dependencies as $dependency) {
    if (function_exists($dependency[1]))
        continue;
    $candidate = $hrm_person_bank_supersession_root.$dependency[0];
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_person_bank_supersession_root, $hrm_person_bank_supersession_dependencies, $dependency, $candidate);

if (!defined('HRM_PERSON_BANK_SUPERSESSION_ACTION_CLASS'))
    define('HRM_PERSON_BANK_SUPERSESSION_ACTION_CLASS', 'hrm.person_bank_account.supersession.create');
if (!defined('HRM_PERSON_BANK_SUPERSESSION_HASH_CONTRACT'))
    define('HRM_PERSON_BANK_SUPERSESSION_HASH_CONTRACT', 'notrinoserp.hrm-fnd-004.person-bank-supersession');
if (!defined('HRM_PERSON_BANK_SUPERSESSION_HASH_VERSION'))
    define('HRM_PERSON_BANK_SUPERSESSION_HASH_VERSION', '1');

function hrm_person_bank_supersession_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $integer = (int)$value;
    return $integer > 0 && (string)$integer === $value ? $integer : false;
}

function hrm_person_bank_supersession_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

function hrm_person_bank_supersession_permission()
{
    return function_exists('hrm_user_can_access_sensitive_field')
        && defined('HRM_FIELD_ACTION_SUPERSEDE')
        && hrm_user_can_access_sensitive_field(
            HRM_FIELD_RESTRICTED_FINANCIAL,
            HRM_FIELD_ACTION_SUPERSEDE
        );
}

/**
 * Purpose-separated keyed token over the complete canonical bank payload.
 * This is used only for action binding and is never a payment routing token.
 */
function hrm_person_bank_supersession_payload_token($payload, $master_key)
{
    $json = hrm_person_bank_payload_json($payload);
    if ($json === false || !is_string($master_key) || strlen($master_key) !== 32)
        return false;
    $key = hash_hmac('sha256', 'person-bank-supersession-action-v1', $master_key, true);
    return hash_hmac('sha256', $json, $key);
}

function hrm_person_bank_supersession_action_hash($company, $predecessor_id, $predecessor_row_version, $payload_token)
{
    $predecessor_id = hrm_person_bank_supersession_positive_int($predecessor_id);
    $predecessor_row_version = hrm_person_bank_supersession_positive_int($predecessor_row_version);
    if ((int)$company < 0 || $predecessor_id === false || $predecessor_row_version === false
        || !is_string($payload_token) || preg_match('/^[a-f0-9]{64}$/D', $payload_token) !== 1)
        return false;
    return hash('sha256', HRM_PERSON_BANK_SUPERSESSION_HASH_CONTRACT
        .'|v='.HRM_PERSON_BANK_SUPERSESSION_HASH_VERSION.'|company='.(int)$company
        .'|action='.HRM_PERSON_BANK_SUPERSESSION_ACTION_CLASS.'|predecessor_id='.$predecessor_id
        .'|predecessor_row_version='.$predecessor_row_version.'|payload_token='.$payload_token);
}

function hrm_person_bank_supersession_assurance_event_for_update($session_id, $action_hash)
{
    if ((int)$session_id <= 0 || !federation_assurance_consumer_hex64($action_hash))
        return false;
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,issued_at,expires_at,consumed_at FROM "
        .TB_PREF."user_session_assurance_events WHERE session_id=".(int)$session_id." AND action_class="
        .db_escape(HRM_PERSON_BANK_SUPERSESSION_ACTION_CLASS)." AND action_hash=".db_escape($action_hash)
        ." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE", 'could not lock bank supersession assurance event');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    return db_fetch_assoc($result) === false ? $row : false;
}

function hrm_person_bank_supersession_consume_assurance($company, $action_hash, $now_epoch)
{
    $session_id = federation_assurance_consumer_current_session_id();
    if ($session_id <= 0)
        return false;
    $custody = federation_assurance_consumer_custody_for_update($session_id);
    if (!federation_assurance_consumer_current_custody_valid($custody, $now_epoch, true))
        return false;
    $event = hrm_person_bank_supersession_assurance_event_for_update($session_id, $action_hash);
    if (!is_array($event) || (int)$event['assurance_level'] !== 2
        || (string)$event['assurance_method'] !== 'oidc_acr_amr'
        || (string)$event['action_class'] !== HRM_PERSON_BANK_SUPERSESSION_ACTION_CLASS
        || !session_registry_hash_equals((string)$event['action_hash'], $action_hash)
        || $event['consumed_at'] !== null)
        return false;
    $issued = strtotime((string)$event['issued_at'].' UTC');
    $expires = strtotime((string)$event['expires_at'].' UTC');
    if ($issued === false || $expires === false || $now_epoch < $issued || $now_epoch >= $expires)
        return false;
    $consumed_at = gmdate('Y-m-d H:i:s', $now_epoch);
    if (!db_query("UPDATE ".TB_PREF."user_session_assurance_events SET consumed_at=".db_escape($consumed_at)
        ." WHERE assurance_event_id=".(int)$event['assurance_event_id']." AND consumed_at IS NULL AND expires_at>".db_escape($consumed_at),
        'could not consume bank supersession assurance') || (int)db_num_affected_rows() !== 1)
        return false;
    $restore = federation_assurance_consumer_restore_level_one_sql($custody, $now_epoch);
    if ($restore === false || !db_query("UPDATE ".TB_PREF."user_sessions SET ".$restore." WHERE id=".$session_id
        ." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'",
        'could not reset bank supersession session assurance') || (int)db_num_affected_rows() !== 1)
        return false;
    if (append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,'entity_type'=>'security_session_assurance',
        'entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),
        'action'=>'security.session.assurance.consumed','reason_code'=>'hrm_fnd_004_person_bank_supersession',
        'before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),
        'restricted_diff'=>array('action_class'=>HRM_PERSON_BANK_SUPERSESSION_ACTION_CLASS,'action_hash'=>$action_hash,
            'single_use_event_consumed'=>true,'session_assurance_reset_to_level_one'=>true,
            'bank_values_stored'=>false,'bank_values_revealed'=>false,'payment_authority_changed'=>false,
            'payroll_authority_changed'=>false,'accounting_authority_changed'=>false))) === false)
        return false;
    return (int)$event['assurance_event_id'];
}

/**
 * Authenticate/decrypt one stored bank row and return its canonical payload.
 * The helper is internal to exact integrity/retry checks and never returns it
 * from the public command.
 */
function hrm_person_bank_supersession_stored_payload($row, $person_id)
{
    if (!is_array($row) || empty($row['key_id']) || !isset($row['details_ciphertext']))
        return false;
    $key = hrm_person_sensitive_key((string)$row['key_id']);
    $context = hrm_person_bank_context($person_id);
    if ($key === false || $context === false)
        return false;
    $plaintext = hrm_person_sensitive_decrypt((string)$row['details_ciphertext'], $context, $key);
    if ($plaintext === false)
        return false;
    $payload = json_decode($plaintext, true);
    $canonical = hrm_person_bank_payload_json($payload);
    if ($canonical === false || !domain_audit_hash_equals($canonical, $plaintext))
        return false;
    $account = (string)$payload['bank_account'];
    $routing = (string)$payload['bank_routing'];
    $lookup = trim($account) === '' ? '' : hrm_person_sensitive_lookup_token('bank_account', $account, $key);
    if ($lookup === false || (string)$row['account_lookup_token'] !== $lookup
        || (string)$row['masked_account'] !== hrm_person_sensitive_mask($account)
        || (string)$row['masked_routing'] !== hrm_person_sensitive_mask($routing)
        || (int)$row['payment_method'] !== (int)$payload['payment_method'])
        return false;
    return array('payload'=>$payload, 'canonical'=>$canonical);
}

/**
 * Close one current verified/approved normalized bank row and create exactly
 * one lineage-bound unverified/unapproved successor.
 *
 * Exact retry after success returns only successor identifiers/status without
 * consuming another assurance event. Any payload/actor/state drift fails closed.
 */
function supersede_hrm_person_bank_account($predecessor_id, $predecessor_row_version, $new_payload, $now_epoch = null)
{
    $predecessor_id = hrm_person_bank_supersession_positive_int($predecessor_id);
    $predecessor_row_version = hrm_person_bank_supersession_positive_int($predecessor_row_version);
    $actor = hrm_person_bank_supersession_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $now_epoch = $now_epoch === null ? time() : (int)$now_epoch;
    $canonical = hrm_person_bank_payload_json($new_payload);
    if ($predecessor_id === false || $predecessor_row_version === false || $actor <= 0 || $company < 0
        || $now_epoch <= 0 || $canonical === false || !hrm_person_bank_payload_has_details($new_payload)
        || !hrm_person_bank_supersession_permission())
        return false;

    $active_key = hrm_person_sensitive_active_key(null, false);
    if (!is_array($active_key) || !isset($active_key['key_id'], $active_key['key']))
        return false;
    $payload_token = hrm_person_bank_supersession_payload_token($new_payload, $active_key['key']);
    $action_hash = hrm_person_bank_supersession_action_hash($company, $predecessor_id,
        $predecessor_row_version, $payload_token);
    if ($payload_token === false || $action_hash === false)
        return false;

    begin_transaction();
    $ok = false;
    $return_value = false;
    do {
        $fields = "person_bank_account_id,person_id,details_ciphertext,masked_account,masked_routing,account_lookup_token,key_id,"
            ."payment_method,verification_status,verification_method,approval_status,row_version,effective_from,effective_to,source,"
            ."supersedes_person_bank_account_id,created_by";
        $result = db_query("SELECT ".$fields." FROM ".TB_PREF."hrm_person_bank_accounts WHERE person_bank_account_id="
            .$predecessor_id." LIMIT 2 FOR UPDATE", 'could not lock bank supersession predecessor');
        $predecessor = $result ? db_fetch_assoc($result) : false;
        if (!$predecessor || db_fetch_assoc($result) !== false
            || (int)$predecessor['row_version'] !== $predecessor_row_version
            || (string)$predecessor['verification_status'] !== 'verified'
            || (string)$predecessor['approval_status'] !== 'approved'
            || (string)$predecessor['verification_method'] === '')
            break;
        $predecessor_payload = hrm_person_bank_supersession_stored_payload($predecessor, (int)$predecessor['person_id']);
        if (!is_array($predecessor_payload) || domain_audit_hash_equals($predecessor_payload['canonical'], $canonical))
            break;

        $successor_result = db_query("SELECT ".$fields." FROM ".TB_PREF."hrm_person_bank_accounts WHERE supersedes_person_bank_account_id="
            .$predecessor_id." ORDER BY person_bank_account_id LIMIT 2 FOR UPDATE", 'could not lock bank supersession successor');
        if (!$successor_result || db_num_rows($successor_result) > 1)
            break;
        $successor = db_num_rows($successor_result) === 1 ? db_fetch_assoc($successor_result) : false;
        if ($successor) {
            $stored = hrm_person_bank_supersession_stored_payload($successor, (int)$predecessor['person_id']);
            if ($predecessor['effective_to'] === null || !is_array($stored)
                || !domain_audit_hash_equals($stored['canonical'], $canonical)
                || (int)$successor['person_id'] !== (int)$predecessor['person_id']
                || $successor['effective_to'] !== null
                || (string)$successor['source'] !== 'governed_supersession'
                || (string)$successor['verification_status'] !== 'unverified'
                || (string)$successor['verification_method'] !== ''
                || (string)$successor['approval_status'] !== 'unapproved'
                || (int)$successor['row_version'] !== 1
                || (int)$successor['created_by'] !== $actor)
                break;
            $ok = true;
            $return_value = array('person_bank_account_id'=>(int)$successor['person_bank_account_id'],
                'supersedes_person_bank_account_id'=>$predecessor_id,'row_version'=>1,'status'=>'already_superseded');
            break;
        }
        if ($predecessor['effective_to'] !== null)
            break;

        $current_result = db_query("SELECT person_bank_account_id FROM ".TB_PREF."hrm_person_bank_accounts WHERE person_id="
            .(int)$predecessor['person_id']." AND effective_to IS NULL ORDER BY person_bank_account_id DESC LIMIT 2 FOR UPDATE",
            'could not lock current bank supersession custody');
        if (!$current_result || db_num_rows($current_result) !== 1)
            break;
        $current = db_fetch_assoc($current_result);
        if ((int)$current['person_bank_account_id'] !== $predecessor_id)
            break;

        $assurance_event_id = hrm_person_bank_supersession_consume_assurance($company, $action_hash, $now_epoch);
        if ($assurance_event_id === false)
            break;
        $now = gmdate('Y-m-d H:i:s', $now_epoch);
        if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_accounts SET effective_to=".db_escape($now)
            ." WHERE person_bank_account_id=".$predecessor_id." AND row_version=".$predecessor_row_version
            ." AND effective_to IS NULL AND verification_status='verified' AND approval_status='approved' AND verification_method<>''",
            'could not close superseded bank account') || (int)db_num_affected_rows() !== 1)
            break;

        $context = hrm_person_bank_context((int)$predecessor['person_id']);
        $ciphertext = $context === false ? false : hrm_person_sensitive_encrypt($canonical, $context, $active_key['key']);
        $ciphertext_sql = $ciphertext === false ? false : hrm_person_bank_binary_sql_literal($ciphertext);
        $account = (string)$new_payload['bank_account'];
        $routing = (string)$new_payload['bank_routing'];
        $lookup = trim($account) === '' ? '' : hrm_person_sensitive_lookup_token('bank_account', $account, $active_key['key']);
        if ($ciphertext_sql === false || $lookup === false)
            break;
        $sql = "INSERT INTO ".TB_PREF."hrm_person_bank_accounts "
            ."(person_id,details_ciphertext,masked_account,masked_routing,account_lookup_token,key_id,payment_method,"
            ."verification_status,verification_method,approval_status,row_version,supersedes_person_bank_account_id,"
            ."effective_from,effective_to,source,created_by) VALUES ("
            .(int)$predecessor['person_id'].",".$ciphertext_sql.",".db_escape(hrm_person_sensitive_mask($account)).",".
            db_escape(hrm_person_sensitive_mask($routing)).",".db_escape($lookup).",".db_escape((string)$active_key['key_id']).",".
            (int)$new_payload['payment_method'].",'unverified','','unapproved',1,".$predecessor_id.",".db_escape($now)
            .",NULL,'governed_supersession',".$actor.")";
        if (!db_query($sql, 'could not create governed bank-account successor'))
            break;
        $successor_id = function_exists('db_insert_id') ? (int)db_insert_id() : 0;
        if ($successor_id <= 0)
            break;
        if (append_domain_audit_event(array(
            'company_id'=>$company,'legal_entity_id'=>0,'entity_type'=>'hrm_person_bank_account_supersession',
            'entity_key'=>array('predecessor_person_bank_account_id'=>$predecessor_id,
                'successor_person_bank_account_id'=>$successor_id),
            'action'=>'hrm.person_bank_account.supersession.created','reason_code'=>'hrm_fnd_004_person_bank_supersession',
            'before'=>array('predecessor_row_version'=>$predecessor_row_version,'predecessor_current'=>true,
                'predecessor_verification_status'=>'verified','predecessor_approval_status'=>'approved'),
            'after'=>array('predecessor_current'=>false,'successor_row_version'=>1,
                'successor_verification_status'=>'unverified','successor_approval_status'=>'unapproved'),
            'restricted_diff'=>array('assurance_event_id'=>$assurance_event_id,'action_hash'=>$action_hash,
                'one_direct_successor'=>true,'bank_specific_promotion_still_required'=>true,
                'legacy_bank_authority_changed'=>false,'bank_values_stored'=>true,'bank_values_audited'=>false,
                'bank_values_revealed'=>false,'verification_promoted'=>false,'approval_promoted'=>false,
                'payment_election_changed'=>false,'payment_allocation_changed'=>false,'payment_binding_changed'=>false,
                'payment_authority_changed'=>false,'payroll_authority_changed'=>false,'accounting_authority_changed'=>false))) === false)
            break;
        $ok = true;
        $return_value = array('person_bank_account_id'=>$successor_id,
            'supersedes_person_bank_account_id'=>$predecessor_id,'row_version'=>1,'status'=>'created');
    } while (false);

    if ($ok)
        commit_transaction();
    else
        cancel_transaction();
    return $ok ? $return_value : false;
}


/**
 * Resolve one exact verified/approved Person bank-account predecessor through
 * the selected employee mapping. Browser callers receive masked/governance
 * metadata only; protected bank ciphertext, lookup tokens and key ids never
 * leave this helper.
 */
function hrm_person_bank_supersession_browser_predecessor($employee_ref, $person_bank_account_id, $row_version, $allow_superseded = false)
{
    if (!hrm_person_bank_supersession_permission() || !defined('TB_PREF')) return false;
    $person_bank_account_id = hrm_person_bank_supersession_positive_int($person_bank_account_id);
    $row_version = hrm_person_bank_supersession_positive_int($row_version);
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee = is_numeric($employee_ref) ? (int)$employee_ref : trim((string)$employee_ref);
    if ($person_bank_account_id === false || $row_version === false
        || ($column === 'employee_number' && $employee <= 0)
        || ($column === 'employee_id' && ($employee === '' || strlen($employee) > 64))) return false;
    $sql = "SELECT b.person_bank_account_id,b.person_id,b.masked_account,b.masked_routing,b.payment_method,"
        ."b.effective_from,b.effective_to,b.verification_status,b.verification_method,b.approval_status,b.row_version,b.source "
        ."FROM ".TB_PREF."hrm_employee_worker_map m INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_id=w.person_id "
        ."WHERE m.".$column."=".db_escape($employee)." AND m.retired_at IS NULL AND b.person_bank_account_id=".$person_bank_account_id
        ." AND b.row_version=".$row_version.($allow_superseded ? "" : " AND b.effective_to IS NULL")
        ." AND b.verification_status='verified' AND b.approval_status='approved' AND b.verification_method<>'' LIMIT 2";
    $result = db_query($sql, 'could not resolve governed Person bank-account supersession predecessor');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false) return false;
    return array(
        'person_bank_account_id'=>(int)$row['person_bank_account_id'], 'person_id'=>(int)$row['person_id'],
        'masked_account'=>(string)$row['masked_account'], 'masked_routing'=>(string)$row['masked_routing'],
        'payment_method'=>(int)$row['payment_method'], 'verification_status'=>(string)$row['verification_status'],
        'verification_method'=>(string)$row['verification_method'], 'approval_status'=>(string)$row['approval_status'],
        'row_version'=>(int)$row['row_version'], 'source'=>(string)$row['source']
    );
}

/** Return current verified/approved bank supersession candidates as masked metadata only. */
function get_hrm_person_bank_supersession_candidates_masked($employee_ref)
{
    if (!hrm_person_bank_supersession_permission() || !defined('TB_PREF')) return false;
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee = is_numeric($employee_ref) ? (int)$employee_ref : trim((string)$employee_ref);
    if (($column === 'employee_number' && $employee <= 0)
        || ($column === 'employee_id' && ($employee === '' || strlen($employee) > 64))) return false;
    $sql = "SELECT b.person_bank_account_id,b.masked_account,b.masked_routing,b.payment_method,b.verification_status,"
        ."b.verification_method,b.approval_status,b.row_version,b.source FROM ".TB_PREF."hrm_employee_worker_map m "
        ."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_id=w.person_id "
        ."WHERE m.".$column."=".db_escape($employee)." AND m.retired_at IS NULL AND b.effective_to IS NULL "
        ."AND b.verification_status='verified' AND b.approval_status='approved' AND b.verification_method<>'' "
        ."ORDER BY b.person_bank_account_id";
    $result = db_query($sql, 'could not list governed Person bank-account supersession candidates');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = array('person_bank_account_id'=>(int)$row['person_bank_account_id'],
            'masked_account'=>(string)$row['masked_account'], 'masked_routing'=>(string)$row['masked_routing'],
            'payment_method'=>(int)$row['payment_method'], 'verification_status'=>(string)$row['verification_status'],
            'verification_method'=>(string)$row['verification_method'], 'approval_status'=>(string)$row['approval_status'],
            'row_version'=>(int)$row['row_version'], 'source'=>(string)$row['source']);
    }
    return $rows;
}

/**
 * Build the exact action hash for browser step-up without retaining raw bank
 * details. The returned object intentionally omits plaintext bank fields,
 * payload_token, ciphertext, lookup tokens and key material.
 */
function hrm_person_bank_supersession_prepare_browser_action($employee_ref, $person_bank_account_id, $row_version,
    $new_payload, $allow_superseded = false)
{
    $predecessor = hrm_person_bank_supersession_browser_predecessor(
        $employee_ref, $person_bank_account_id, $row_version, $allow_superseded
    );
    $canonical = hrm_person_bank_payload_json($new_payload);
    if (!is_array($predecessor) || $canonical === false || !hrm_person_bank_payload_has_details($new_payload)) return false;
    $active_key = hrm_person_sensitive_active_key(null, false);
    if (!is_array($active_key) || !isset($active_key['key'])) return false;
    $payload_token = hrm_person_bank_supersession_payload_token($new_payload, $active_key['key']);
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $action_hash = $payload_token === false ? false : hrm_person_bank_supersession_action_hash(
        $company, (int)$predecessor['person_bank_account_id'], (int)$predecessor['row_version'], $payload_token
    );
    if ($action_hash === false) return false;
    return array('company'=>$company, 'employee_ref'=>(string)$employee_ref,
        'person_bank_account_id'=>(int)$predecessor['person_bank_account_id'],
        'bank_account_row_version'=>(int)$predecessor['row_version'],
        'masked_account'=>(string)$predecessor['masked_account'], 'masked_routing'=>(string)$predecessor['masked_routing'],
        'payment_method'=>(int)$predecessor['payment_method'],
        'action_class'=>HRM_PERSON_BANK_SUPERSESSION_ACTION_CLASS, 'action_hash'=>$action_hash);
}
