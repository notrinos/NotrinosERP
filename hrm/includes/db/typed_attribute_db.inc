<?php
/**
 * HRM-FND-007 governed typed-attribute foundation contract helpers.
 *
 * No legacy JSON parser or production writer is intentionally exposed here.
 */
function hrm_typed_attribute_value_types()
{
    return array('text','integer','decimal','boolean','date','datetime','code');
}

function hrm_typed_attribute_subject_types()
{
    return array('person','worker','employment','assignment');
}

function hrm_typed_attribute_sensitivities()
{
    return array('standard','confidential','restricted');
}

function hrm_typed_attribute_owner_scopes()
{
    return array('hr','payroll','security','localization');
}

function hrm_typed_attribute_validate_definition_metadata($row)
{
    $allowed = array('attribute_code','value_type','allowed_subject','sensitivity','owner_scope','validation_json');
    if (!is_array($row) || array_diff(array_keys($row), $allowed))
        return false;
    $code = isset($row['attribute_code']) ? (string)$row['attribute_code'] : '';
    if (preg_match('/^[a-z][a-z0-9_.-]{2,63}$/D', $code) !== 1
        || !in_array(isset($row['value_type']) ? $row['value_type'] : null, hrm_typed_attribute_value_types(), true)
        || !in_array(isset($row['allowed_subject']) ? $row['allowed_subject'] : null, hrm_typed_attribute_subject_types(), true)
        || !in_array(isset($row['sensitivity']) ? $row['sensitivity'] : null, hrm_typed_attribute_sensitivities(), true)
        || !in_array(isset($row['owner_scope']) ? $row['owner_scope'] : null, hrm_typed_attribute_owner_scopes(), true))
        return false;
    $validation = isset($row['validation_json']) ? trim((string)$row['validation_json']) : '{}';
    if (strlen($validation) > 4000 || $validation === '' || $validation[0] !== '{'
        || substr($validation, -1) !== '}')
        return false;
    $decoded = json_decode($validation, true);
    return is_array($decoded) && json_last_error() === JSON_ERROR_NONE;
}

function hrm_typed_attribute_canonical_value($value_type, $value)
{
    switch ($value_type) {
        case 'text':
            return is_string($value) && strlen($value) <= 4000 ? $value : false;
        case 'integer':
            if (is_int($value)) return (string)$value;
            return is_string($value)
                && strlen($value) <= 40
                && preg_match('/^-?(?:0|[1-9][0-9]*)$/D', $value) === 1 ? $value : false;
        case 'decimal':
            return is_string($value)
                && strlen($value) <= 64
                && preg_match('/^-?(?:0|[1-9][0-9]*)(?:\.[0-9]{1,12})?$/D', $value) === 1 ? $value : false;
        case 'boolean':
            if ($value === true || $value === 1 || $value === '1') return 'true';
            if ($value === false || $value === 0 || $value === '0') return 'false';
            return false;
        case 'date':
            if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2}$/D', $value) !== 1) return false;
            $p = explode('-', $value); return checkdate((int)$p[1], (int)$p[2], (int)$p[0]) ? $value : false;
        case 'datetime':
            if (!is_string($value)
                || preg_match('/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})Z$/D', $value, $parts) !== 1)
                return false;
            return checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1])
                && (int)$parts[4] <= 23 && (int)$parts[5] <= 59 && (int)$parts[6] <= 59 ? $value : false;
        case 'code':
            return is_string($value) && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D', $value) === 1 ? $value : false;
    }
    return false;
}

function hrm_typed_attribute_crypto_context($definition_version_id, $subject_type, $subject_id, $value_version_no, $predecessor_id)
{
    if ((int)$definition_version_id <= 0
        || !in_array($subject_type, hrm_typed_attribute_subject_types(), true)
        || (int)$subject_id <= 0 || (int)$value_version_no <= 0
        || ($predecessor_id !== null && (int)$predecessor_id <= 0))
        return false;
    return 'hrm-fnd-007-attribute-v1|definition_version='.(int)$definition_version_id
        .'|subject='.$subject_type.':'.(int)$subject_id
        .'|version='.(int)$value_version_no
        .'|predecessor='.($predecessor_id === null ? 0 : (int)$predecessor_id);
}
