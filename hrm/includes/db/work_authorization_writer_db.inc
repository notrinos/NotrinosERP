<?php
/** HRM-FND-007 atomic immutable Work Authorization writer. */
if (!function_exists('hrm_work_authorization_canonical_command'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_command_db.inc');
if (!function_exists('hrm_person_worker_actor_id'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_work_authorization_writer_row_matches($row, $command, $worker_id, $actor_id)
{
    if (!is_array($row) || !is_array($command)
        || (int)$row['worker_id'] !== (int)$worker_id
        || (int)$row['created_by'] !== (int)$actor_id
        || (string)$row['authorization_type'] !== $command['authorization_type']
        || (string)$row['issuing_jurisdiction'] !== $command['issuing_jurisdiction']
        || (string)$row['authorization_status'] !== $command['authorization_status']
        || (string)$row['valid_from'] !== $command['valid_from']
        || (($row['valid_to'] === null ? null : (string)$row['valid_to']) !== $command['valid_to'])
        || (($row['employee_document_id'] === null ? null : (int)$row['employee_document_id']) !== $command['employee_document_id'])
        || (($row['predecessor_id'] === null ? null : (int)$row['predecessor_id']) !== $command['predecessor_id'])
        || (string)$row['verification_status'] !== 'unverified'
        || $row['verified_at'] !== null || $row['verified_by'] !== null)
        return false;
    $key = hrm_person_sensitive_key((string)$row['key_id']);
    $context = hrm_work_authorization_crypto_context(
        (int)$worker_id, (int)$row['version_no'],
        $row['predecessor_id'] === null ? null : (int)$row['predecessor_id']);
    $plain = $key === false || $context === false ? false
        : hrm_person_sensitive_decrypt($row['details_ciphertext'], $context, $key);
    $details = is_string($plain) ? json_decode($plain, true) : null;
    return is_array($details) && $details === $command['details']
        && (string)$row['masked_identifier'] === hrm_work_authorization_mask_identifier(
            $command['details']['permit_identifier']);
}

function hrm_work_authorization_writer_document_matches_worker($worker_id, $document_id)
{
    if ($document_id === null) return true;
    $r = db_query('SELECT d.doc_id,d.employee_id,d.content_state FROM '.TB_PREF.'employee_documents d '
        .'INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.employee_id=d.employee_id '
        .'WHERE m.worker_id='.(int)$worker_id.' AND m.retired_at IS NULL '
        .'AND d.doc_id='.(int)$document_id.' ORDER BY m.mapping_id DESC LIMIT 2 FOR UPDATE',
        'could not validate Work Authorization evidence-document custody');
    if (!$r || db_num_rows($r) !== 1) return false;
    $row = db_fetch_assoc($r);
    return !in_array((string)$row['content_state'], array('quarantined','migration_conflict'), true);
}

/**
 * Create or supersede one authorization chain identified by type+jurisdiction.
 * Returns authorization_id on success, including exact idempotent retry.
 */
function hrm_work_authorization_append_version($worker_id, $input)
{
    $worker_id = hrm_work_authorization_positive_id($worker_id, false);
    $command = hrm_work_authorization_canonical_command($input);
    $actor_id = function_exists('hrm_person_worker_actor_id') ? hrm_person_worker_actor_id() : 0;
    if ($worker_id === false || $command === false || $actor_id <= 0
        || !hrm_work_authorization_user_can_manage())
        return false;
    $active_key = hrm_person_sensitive_active_key(null, false);
    if (!is_array($active_key) || empty($active_key['key_id']) || empty($active_key['key']))
        return false;

    begin_transaction();
    $ok = true; $return_id = false;
    $worker = db_query('SELECT worker_id FROM '.TB_PREF.'hrm_workers WHERE worker_id='.(int)$worker_id
        .' LIMIT 1 FOR UPDATE', 'could not lock Work Authorization Worker');
    if (!$worker || db_num_rows($worker) !== 1) $ok = false;
    if ($ok && !hrm_work_authorization_writer_document_matches_worker(
        $worker_id, $command['employee_document_id'])) $ok = false;

    $rows = array(); $successor = array(); $max_version = 0;
    if ($ok) {
        $r = db_query('SELECT authorization_id,worker_id,version_no,predecessor_id,authorization_type,'
            .'issuing_jurisdiction,authorization_status,valid_from,valid_to,masked_identifier,key_id,'
            .'details_ciphertext,verification_status,verified_at,verified_by,employee_document_id,row_version,created_by '
            .'FROM '.TB_PREF.'hrm_work_authorizations WHERE worker_id='.(int)$worker_id
            .' ORDER BY version_no,authorization_id FOR UPDATE', 'could not lock Work Authorization versions');
        if (!$r) $ok = false;
        while ($ok && ($row = db_fetch_assoc($r))) {
            $id=(int)$row['authorization_id']; $rows[$id]=$row;
            $max_version=max($max_version,(int)$row['version_no']);
            if ($row['predecessor_id'] !== null) {
                $pred=(int)$row['predecessor_id'];
                if (isset($successor[$pred])) { $ok=false; break; }
                $successor[$pred]=$id;
            }
        }
    }

    $predecessor = $command['predecessor_id'];
    $chain_leaves=array();
    if ($ok) {
        foreach ($rows as $id=>$row) {
            if (!isset($successor[$id])
                && (string)$row['authorization_type'] === $command['authorization_type']
                && (string)$row['issuing_jurisdiction'] === $command['issuing_jurisdiction'])
                $chain_leaves[]=$row;
        }
        // The schema permits multiple authorization chains per Worker, but one
        // type+jurisdiction chain must never have parallel current leaves.
        if (count($chain_leaves) > 1) $ok=false;
    }
    if ($ok && $predecessor !== null) {
        if (!isset($rows[$predecessor])
            || (string)$rows[$predecessor]['authorization_type'] !== $command['authorization_type']
            || (string)$rows[$predecessor]['issuing_jurisdiction'] !== $command['issuing_jurisdiction']) {
            $ok=false;
        } elseif (isset($successor[$predecessor])) {
            $candidate=$rows[$successor[$predecessor]];
            if (count($chain_leaves) === 1
                && (int)$chain_leaves[0]['authorization_id'] === (int)$candidate['authorization_id']
                && hrm_work_authorization_writer_row_matches($candidate,$command,$worker_id,$actor_id))
                $return_id=(int)$candidate['authorization_id'];
            else $ok=false;
        } elseif (count($chain_leaves) !== 1
            || (int)$chain_leaves[0]['authorization_id'] !== (int)$predecessor) {
            $ok=false;
        }
    } elseif ($ok && count($chain_leaves) === 1) {
        if (hrm_work_authorization_writer_row_matches($chain_leaves[0],$command,$worker_id,$actor_id))
            $return_id=(int)$chain_leaves[0]['authorization_id'];
        else $ok=false;
    }

    if ($ok && $return_id === false) {
        $version_no=$max_version+1;
        $context=hrm_work_authorization_crypto_context($worker_id,$version_no,$predecessor);
        $cipher=$context === false ? false : hrm_work_authorization_seal_details(
            $worker_id,$version_no,$predecessor,$command['details'],$active_key['key']);
        $digest=hrm_work_authorization_command_digest($worker_id,$command,$active_key['key']);
        if ($cipher === false || $digest === false) $ok=false;
        if ($ok) {
            $sql='INSERT INTO '.TB_PREF.'hrm_work_authorizations '
                .'(worker_id,version_no,predecessor_id,authorization_type,issuing_jurisdiction,authorization_status,'
                .'valid_from,valid_to,masked_identifier,key_id,details_ciphertext,verification_status,verified_at,'
                .'verified_by,employee_document_id,row_version,created_by) VALUES ('
                .(int)$worker_id.','.(int)$version_no.',' .($predecessor===null?'NULL':(int)$predecessor).','
                .db_escape($command['authorization_type']).','.db_escape($command['issuing_jurisdiction']).','
                .db_escape($command['authorization_status']).','.db_escape($command['valid_from']).','
                .($command['valid_to']===null?'NULL':db_escape($command['valid_to'])).','
                .db_escape(hrm_work_authorization_mask_identifier($command['details']['permit_identifier'])).','
                .db_escape($active_key['key_id']).','.db_escape($cipher).",'unverified',NULL,NULL,"
                .($command['employee_document_id']===null?'NULL':(int)$command['employee_document_id'])
                .',1,'.(int)$actor_id.')';
            $ok=db_query($sql,'could not append Work Authorization version') !== false;
        }
        if ($ok) {
            $return_id=(int)db_insert_id();
            $receipt=append_domain_audit_event(array(
                'company_id'=>function_exists('user_company')?(int)user_company():0,
                'legal_entity_id'=>0,'entity_type'=>'hrm_work_authorization',
                'entity_key'=>array('worker_id'=>(int)$worker_id,'authorization_id'=>$return_id,
                    'version_no'=>(int)$version_no,'command_digest'=>$digest),
                'action'=>'hrm.work_authorization.version_created',
                'reason_code'=>'hrm_fnd_007_work_authorization_writer',
                'before'=>$predecessor===null?null:array('predecessor_id'=>(int)$predecessor),
                'after'=>array('authorization_id'=>$return_id,'version_no'=>(int)$version_no),
                'restricted_diff'=>array('predecessor_present'=>$predecessor!==null,
                    'evidence_document_linked'=>$command['employee_document_id']!==null,
                    'verification_initialized_unverified'=>true,'sensitive_plaintext_logged'=>false)
            ));
            if ($receipt === false) { $ok=false; $return_id=false; }
        }
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $return_id : false;
}
