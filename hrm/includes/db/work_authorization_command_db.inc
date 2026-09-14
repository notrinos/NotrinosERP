<?php
/**
 * HRM-FND-007 Work Authorization command custody.
 *
 * This file is intentionally non-mutating. It canonicalizes one maker command,
 * enforces a dedicated existing-role-model permission, and derives only an
 * opaque keyed command digest. Runtime persistence is isolated in the writer.
 */
if (!function_exists('hrm_work_authorization_validate_metadata'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_db.inc');
if (!function_exists('hrm_user_can_access_sensitive_field'))
    include_once($path_to_root.'/hrm/includes/hrm_security.inc');

function hrm_work_authorization_user_can_manage()
{
    return function_exists('user_check_access')
        && hrm_user_can_access_sensitive_field(
            HRM_FIELD_RESTRICTED_WORK_AUTHORIZATION,
            HRM_FIELD_ACTION_MANAGE
        );
}

function hrm_work_authorization_positive_id($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable)
        return null;
    if (is_int($value))
        return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

/** Return one exact, deterministic command shape or false. */
function hrm_work_authorization_canonical_command($input)
{
    $allowed = array('authorization_type','issuing_jurisdiction','authorization_status',
        'valid_from','valid_to','permit_identifier','sponsor','occupation_restriction',
        'location_restriction','employer_restriction','employee_document_id','predecessor_id');
    if (!is_array($input) || array_diff(array_keys($input), $allowed))
        return false;
    $required = array('authorization_type','issuing_jurisdiction','authorization_status','valid_from');
    foreach ($required as $field)
        if (!array_key_exists($field, $input) || !is_string($input[$field])) return false;
    if (array_key_exists('valid_to', $input) && $input['valid_to'] !== ''
        && $input['valid_to'] !== null && !is_string($input['valid_to']))
        return false;
    foreach (hrm_work_authorization_detail_fields() as $field)
        if (array_key_exists($field, $input) && !is_string($input[$field])) return false;

    $metadata = array(
        'authorization_type'=>(string)$input['authorization_type'],
        'issuing_jurisdiction'=>(string)$input['issuing_jurisdiction'],
        'authorization_status'=>(string)$input['authorization_status'],
        'verification_status'=>'unverified',
        'valid_from'=>(string)$input['valid_from'],
        'valid_to'=>array_key_exists('valid_to',$input) && $input['valid_to'] !== ''
            ? $input['valid_to'] : null,
    );
    if (!hrm_work_authorization_validate_metadata($metadata))
        return false;
    $details = array();
    foreach (hrm_work_authorization_detail_fields() as $field)
        $details[$field] = array_key_exists($field,$input) ? $input[$field] : '';
    $details = hrm_work_authorization_canonical_details($details);
    if ($details === false)
        return false;
    $document_id = hrm_work_authorization_positive_id(
        array_key_exists('employee_document_id',$input) ? $input['employee_document_id'] : null, true);
    $predecessor_id = hrm_work_authorization_positive_id(
        array_key_exists('predecessor_id',$input) ? $input['predecessor_id'] : null, true);
    if ($document_id === false || $predecessor_id === false)
        return false;
    return array(
        'authorization_type'=>$metadata['authorization_type'],
        'issuing_jurisdiction'=>$metadata['issuing_jurisdiction'],
        'authorization_status'=>$metadata['authorization_status'],
        'valid_from'=>$metadata['valid_from'],
        'valid_to'=>$metadata['valid_to'],
        'details'=>$details,
        'employee_document_id'=>$document_id,
        'predecessor_id'=>$predecessor_id,
        'verification_status'=>'unverified',
    );
}

/** Opaque retry identity; plaintext work-authorization fields are never audited. */
function hrm_work_authorization_command_digest($worker_id, $command, $master_key)
{
    $worker_id = hrm_work_authorization_positive_id($worker_id, false);
    if ($worker_id === false || !is_array($command) || !is_string($master_key)
        || strlen($master_key) !== 32)
        return false;
    $json = json_encode(array('worker_id'=>$worker_id,'command'=>$command));
    return $json === false ? false
        : hash_hmac('sha256', 'hrm-fnd-007-work-authorization-command-v1|'.$json, $master_key);
}
