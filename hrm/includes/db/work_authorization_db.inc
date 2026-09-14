<?php
/**
 * HRM-FND-007 Work Authorization foundation contract helpers.
 *
 * This foundation deliberately exposes no production writer. It defines the
 * validated/encrypted custody contract used by later command slices.
 */
if (!function_exists('hrm_person_sensitive_encrypt'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');

function hrm_work_authorization_types()
{
    return array('work_permit','residence_permit','visa','other');
}

function hrm_work_authorization_statuses()
{
    return array('pending','active','expired','revoked');
}

function hrm_work_authorization_verification_statuses()
{
    return array('unverified','pending','verified','rejected');
}

function hrm_work_authorization_validate_code($value, $allowed)
{
    return is_string($value) && in_array($value, $allowed, true);
}

function hrm_work_authorization_validate_jurisdiction($value)
{
    return is_string($value)
        && preg_match('/^[A-Z0-9][A-Z0-9._-]{1,63}$/D', $value) === 1;
}

function hrm_work_authorization_validate_date($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable)
        return true;
    if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2}$/D', $value) !== 1)
        return false;
    $parts = explode('-', $value);
    return checkdate((int)$parts[1], (int)$parts[2], (int)$parts[0]);
}

function hrm_work_authorization_validate_metadata($row)
{
    $allowed = array('authorization_type','issuing_jurisdiction','authorization_status',
        'verification_status','valid_from','valid_to');
    if (!is_array($row) || array_diff(array_keys($row), $allowed)
        || !hrm_work_authorization_validate_code(isset($row['authorization_type']) ? $row['authorization_type'] : null, hrm_work_authorization_types())
        || !hrm_work_authorization_validate_jurisdiction(isset($row['issuing_jurisdiction']) ? $row['issuing_jurisdiction'] : null)
        || !hrm_work_authorization_validate_code(isset($row['authorization_status']) ? $row['authorization_status'] : null, hrm_work_authorization_statuses())
        || !hrm_work_authorization_validate_code(isset($row['verification_status']) ? $row['verification_status'] : null, hrm_work_authorization_verification_statuses())
        || !hrm_work_authorization_validate_date(isset($row['valid_from']) ? $row['valid_from'] : null, false)
        || !hrm_work_authorization_validate_date(isset($row['valid_to']) ? $row['valid_to'] : null, true))
        return false;
    return empty($row['valid_to']) || $row['valid_to'] >= $row['valid_from'];
}

function hrm_work_authorization_detail_fields()
{
    return array('permit_identifier','sponsor','occupation_restriction',
        'location_restriction','employer_restriction');
}

function hrm_work_authorization_canonical_details($details)
{
    if (!is_array($details))
        return false;
    $unknown = array_diff(array_keys($details), hrm_work_authorization_detail_fields());
    if (!empty($unknown))
        return false;
    $out = array();
    foreach (hrm_work_authorization_detail_fields() as $field) {
        $value = isset($details[$field]) ? trim((string)$details[$field]) : '';
        if (strlen($value) > 500)
            return false;
        $out[$field] = $value;
    }
    return $out;
}

function hrm_work_authorization_mask_identifier($value)
{
    $value = preg_replace('/\s+/', '', trim((string)$value));
    if ($value === '')
        return '';
    if (strlen($value) <= 4)
        return '********';
    return '********'.substr($value, -4);
}

function hrm_work_authorization_crypto_context($worker_id, $version_no, $predecessor_id)
{
    $worker_id = (int)$worker_id;
    $version_no = (int)$version_no;
    $predecessor_id = $predecessor_id === null ? 0 : (int)$predecessor_id;
    if ($worker_id <= 0 || $version_no <= 0 || $predecessor_id < 0)
        return false;
    return 'hrm-fnd-007-work-auth-v1|worker='.$worker_id
        .'|version='.$version_no.'|predecessor='.$predecessor_id;
}

function hrm_work_authorization_seal_details($worker_id, $version_no, $predecessor_id, $details, $master_key)
{
    $canonical = hrm_work_authorization_canonical_details($details);
    $context = hrm_work_authorization_crypto_context($worker_id, $version_no, $predecessor_id);
    if ($canonical === false || $context === false || !is_string($master_key) || strlen($master_key) < 32)
        return false;
    $json = json_encode($canonical);
    if ($json === false)
        return false;
    return hrm_person_sensitive_encrypt($json, $context, $master_key);
}
