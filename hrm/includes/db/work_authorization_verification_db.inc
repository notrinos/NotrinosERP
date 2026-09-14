<?php
/** HRM-FND-007 immutable Work Authorization checker verification writer. */
if (!function_exists('hrm_work_authorization_writer_document_matches_worker'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_writer_db.inc');
if (!function_exists('hrm_person_worker_actor_id'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_work_authorization_user_can_verify()
{
    return function_exists('user_check_access')
        && hrm_user_can_access_sensitive_field(
            HRM_FIELD_RESTRICTED_WORK_AUTHORIZATION,
            HRM_FIELD_ACTION_VERIFY
        );
}

function hrm_work_authorization_verification_row_details($row)
{
    if (!is_array($row) || empty($row['key_id']) || !isset($row['details_ciphertext']))
        return false;
    $key = hrm_person_sensitive_key((string)$row['key_id']);
    $context = hrm_work_authorization_crypto_context(
        (int)$row['worker_id'], (int)$row['version_no'],
        $row['predecessor_id'] === null ? null : (int)$row['predecessor_id']
    );
    if ($key === false || $context === false)
        return false;
    $plain = hrm_person_sensitive_decrypt($row['details_ciphertext'], $context, $key);
    $details = is_string($plain) ? json_decode($plain, true) : null;
    $canonical = is_array($details) ? hrm_work_authorization_canonical_details($details) : false;
    return $canonical === false ? false : $canonical;
}

function hrm_work_authorization_verification_successor_matches($row, $target, $details, $checker_id)
{
    if (!is_array($row) || !is_array($target) || !is_array($details)
        || (int)$row['worker_id'] !== (int)$target['worker_id']
        || (int)$row['predecessor_id'] !== (int)$target['authorization_id']
        || (string)$row['authorization_type'] !== (string)$target['authorization_type']
        || (string)$row['issuing_jurisdiction'] !== (string)$target['issuing_jurisdiction']
        || (string)$row['authorization_status'] !== (string)$target['authorization_status']
        || (string)$row['valid_from'] !== (string)$target['valid_from']
        || (($row['valid_to'] === null ? null : (string)$row['valid_to'])
            !== ($target['valid_to'] === null ? null : (string)$target['valid_to']))
        || (string)$row['masked_identifier'] !== (string)$target['masked_identifier']
        || (($row['employee_document_id'] === null ? null : (int)$row['employee_document_id'])
            !== ($target['employee_document_id'] === null ? null : (int)$target['employee_document_id']))
        || (string)$row['verification_status'] !== 'verified'
        || $row['verified_at'] === null
        || (int)$row['verified_by'] !== (int)$checker_id
        || (int)$row['created_by'] !== (int)$checker_id)
        return false;
    $successor_details = hrm_work_authorization_verification_row_details($row);
    return is_array($successor_details) && $successor_details === $details;
}

/**
 * Verify one current unverified authorization leaf by appending an immutable
 * verified successor. Returns successor authorization_id, including exact retry.
 */
function hrm_work_authorization_verify_current_version($authorization_id)
{
    $authorization_id = hrm_work_authorization_positive_id($authorization_id, false);
    $checker_id = function_exists('hrm_person_worker_actor_id') ? hrm_person_worker_actor_id() : 0;
    if ($authorization_id === false || $checker_id <= 0 || !hrm_work_authorization_user_can_verify())
        return false;

    $active_key = hrm_person_sensitive_active_key(null, false);
    if (!is_array($active_key) || empty($active_key['key_id']) || empty($active_key['key']))
        return false;

    // Resolve immutable routing keys before the transaction so checker and maker
    // acquire locks in the same Worker -> evidence -> authorization order.
    $lookup = db_query('SELECT worker_id,employee_document_id FROM '.TB_PREF.'hrm_work_authorizations '
        .'WHERE authorization_id='.(int)$authorization_id.' LIMIT 2',
        'could not resolve Work Authorization verification target');
    if (!$lookup || db_num_rows($lookup) !== 1) return false;
    $lookup_row = db_fetch_assoc($lookup);
    $worker_id = isset($lookup_row['worker_id']) ? (int)$lookup_row['worker_id'] : 0;
    $document_id = $lookup_row['employee_document_id'] === null ? null : (int)$lookup_row['employee_document_id'];
    if ($worker_id <= 0 || $document_id === null || $document_id <= 0) return false;

    begin_transaction();
    $ok = true; $return_id = false; $target = false; $details = false; $max_version = 0; $successors = array(); $rows = array();
    $worker = db_query('SELECT worker_id FROM '.TB_PREF.'hrm_workers WHERE worker_id='.(int)$worker_id
        .' LIMIT 1 FOR UPDATE', 'could not lock Work Authorization verification Worker');
    if (!$worker || db_num_rows($worker) !== 1) $ok = false;
    if ($ok && !hrm_work_authorization_writer_document_matches_worker($worker_id, $document_id)) $ok = false;
    $r = $ok ? db_query('SELECT authorization_id,worker_id,version_no,predecessor_id,authorization_type,'
        .'issuing_jurisdiction,authorization_status,valid_from,valid_to,masked_identifier,key_id,'
        .'details_ciphertext,verification_status,verified_at,verified_by,employee_document_id,row_version,created_by '
        .'FROM '.TB_PREF.'hrm_work_authorizations WHERE worker_id='.(int)$worker_id
        .' ORDER BY version_no,authorization_id FOR UPDATE', 'could not lock Work Authorization verification chain') : false;
    if (!$r) $ok = false;
    while ($ok && ($row = db_fetch_assoc($r))) {
        $id = (int)$row['authorization_id']; $rows[$id] = $row;
        $max_version = max($max_version, (int)$row['version_no']);
        if ($id === (int)$authorization_id) $target = $row;
        if ($row['predecessor_id'] !== null) {
            $pred = (int)$row['predecessor_id'];
            if (isset($successors[$pred])) { $ok = false; break; }
            $successors[$pred] = $row;
        }
    }
    if ($ok && (!is_array($target)
        || (int)$target['worker_id'] !== $worker_id
        || $target['employee_document_id'] === null
        || (int)$target['employee_document_id'] !== $document_id)) $ok = false;
    if ($ok) {
        $leaf_count = 0;
        foreach ($rows as $id=>$row) {
            if (!isset($successors[$id])
                && (string)$row['authorization_type'] === (string)$target['authorization_type']
                && (string)$row['issuing_jurisdiction'] === (string)$target['issuing_jurisdiction'])
                $leaf_count++;
        }
        if ($leaf_count !== 1) $ok = false;
    }
    if ($ok) {
        $metadata = array(
            'authorization_type'=>(string)$target['authorization_type'],
            'issuing_jurisdiction'=>(string)$target['issuing_jurisdiction'],
            'authorization_status'=>(string)$target['authorization_status'],
            'verification_status'=>(string)$target['verification_status'],
            'valid_from'=>(string)$target['valid_from'],
            'valid_to'=>$target['valid_to'] === null ? null : (string)$target['valid_to'],
        );
        $details = hrm_work_authorization_verification_row_details($target);
        if (!hrm_work_authorization_validate_metadata($metadata) || $details === false) $ok = false;
    }

    if ($ok && isset($successors[$authorization_id])) {
        $candidate = $successors[$authorization_id];
        if (hrm_work_authorization_verification_successor_matches($candidate, $target, $details, $checker_id))
            $return_id = (int)$candidate['authorization_id'];
        else
            $ok = false;
    }

    if ($ok && $return_id === false) {
        if ((string)$target['verification_status'] !== 'unverified'
            || $target['verified_at'] !== null || $target['verified_by'] !== null
            || (int)$target['created_by'] === (int)$checker_id
            || $target['employee_document_id'] === null
            || isset($successors[$authorization_id])) {
            $ok = false;
        }
    }

    if ($ok && $return_id === false) {
        $version_no = $max_version + 1;
        $context = hrm_work_authorization_crypto_context((int)$target['worker_id'], $version_no, $authorization_id);
        $cipher = $context === false ? false : hrm_work_authorization_seal_details(
            (int)$target['worker_id'], $version_no, $authorization_id, $details, $active_key['key']);
        if ($cipher === false) $ok = false;
        if ($ok) {
            $sql = 'INSERT INTO '.TB_PREF.'hrm_work_authorizations '
                .'(worker_id,version_no,predecessor_id,authorization_type,issuing_jurisdiction,authorization_status,'
                .'valid_from,valid_to,masked_identifier,key_id,details_ciphertext,verification_status,verified_at,'
                .'verified_by,employee_document_id,row_version,created_by) VALUES ('
                .(int)$target['worker_id'].','.(int)$version_no.','.(int)$authorization_id.','
                .db_escape($target['authorization_type']).','.db_escape($target['issuing_jurisdiction']).','
                .db_escape($target['authorization_status']).','.db_escape($target['valid_from']).','
                .($target['valid_to']===null?'NULL':db_escape($target['valid_to'])).','
                .db_escape($target['masked_identifier']).','.db_escape($active_key['key_id']).','.db_escape($cipher)
                .",'verified',UTC_TIMESTAMP(),".(int)$checker_id.','.(int)$target['employee_document_id']
                .',1,'.(int)$checker_id.')';
            $ok = db_query($sql, 'could not append verified Work Authorization successor') !== false;
        }
        if ($ok) {
            $return_id = (int)db_insert_id();
            $receipt = append_domain_audit_event(array(
                'company_id'=>function_exists('user_company')?(int)user_company():0,
                'legal_entity_id'=>0,'entity_type'=>'hrm_work_authorization',
                'entity_key'=>array('worker_id'=>(int)$target['worker_id'],
                    'authorization_id'=>$return_id,'predecessor_id'=>(int)$authorization_id,
                    'version_no'=>(int)$version_no),
                'action'=>'hrm.work_authorization.verified',
                'reason_code'=>'hrm_fnd_007_work_authorization_verification',
                'before'=>array('authorization_id'=>(int)$authorization_id,'verification_status'=>'unverified'),
                'after'=>array('authorization_id'=>$return_id,'verification_status'=>'verified'),
                'restricted_diff'=>array('immutable_successor'=>true,'checker_distinct_from_maker'=>true,
                    'same_worker_evidence_document_required'=>true,'eligibility_inferred'=>false,
                    'sensitive_plaintext_logged'=>false)
            ));
            if ($receipt === false) { $ok = false; $return_id = false; }
        }
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $return_id : false;
}
