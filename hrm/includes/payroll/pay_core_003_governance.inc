<?php
/**
 * Pure governance helpers for PAY-CORE-003 mapping, writers and approved inputs.
 * No database/session/request dependency and no production payroll authority.
 */
include_once(dirname(__FILE__).'/pay_core_003_contract.inc');

if (!defined('HRM_PAY_CORE_003_GOVERNANCE_VERSION'))
    define('HRM_PAY_CORE_003_GOVERNANCE_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_003_CONTROLLED_LEGACY_CONVERSION_REQUIRES_APPROVAL'))
    define('HRM_PAY_CORE_003_CONTROLLED_LEGACY_CONVERSION_REQUIRES_APPROVAL', true);
if (!defined('HRM_PAY_CORE_003_LEGACY_CALCULATOR_REMAINS_AUTHORITATIVE'))
    define('HRM_PAY_CORE_003_LEGACY_CALCULATOR_REMAINS_AUTHORITATIVE', true);

function hrm_pay_core_003_token($value, $max=128)
{
    if (!is_string($value)) return false;
    $value = trim($value);
    return $value !== '' && strlen($value) <= $max
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]*$/D', $value) === 1 ? $value : false;
}

function hrm_pay_core_003_canonicalize($value)
{
    if (!is_array($value)) return $value;
    $list = $value === array_values($value);
    if (!$list) ksort($value, SORT_STRING);
    foreach ($value as $key=>$item) $value[$key] = hrm_pay_core_003_canonicalize($item);
    return $value;
}

function hrm_pay_core_003_hash($value)
{
    $json = json_encode(hrm_pay_core_003_canonicalize($value),
        JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_PRESERVE_ZERO_FRACTION);
    return is_string($json) ? hash('sha256', $json) : false;
}

function hrm_pay_core_003_mapping_target_kinds()
{
    return array('element_version_bundle','eligibility_version','approved_input','documented_exception');
}

function hrm_pay_core_003_eligibility_kinds()
{
    return array('all','rule');
}

/** Validate one append-only element-version bundle independent of legacy conversion. */
function hrm_pay_core_003_validate_element_version_bundle($row)
{
    if (!is_array($row)) return false;
    foreach (array('element_code','version_no','effective_from','element_type','calculation_mode','input_definitions') as $key)
        if (!array_key_exists($key, $row)) return false;
    $probe = array(
        'element_code'=>$row['element_code'],
        'version_no'=>(int)$row['version_no'],
        'effective_from'=>$row['effective_from'],
        'effective_to'=>isset($row['effective_to']) ? $row['effective_to'] : null,
        'element_type'=>$row['element_type'],
        'calculation_mode'=>$row['calculation_mode'],
        'rule_artifact_sha256'=>isset($row['rule_artifact_sha256']) ? $row['rule_artifact_sha256'] : null,
        'version_sha256'=>str_repeat('a',64),
    );
    if (!hrm_pay_core_003_validate_element_version($probe)
        || !is_array($row['input_definitions']) || count($row['input_definitions']) > 16)
        return false;
    foreach ($row['input_definitions'] as $definition) {
        if (!is_array($definition)) return false;
        $definition['definition_sha256'] = str_repeat('b',64);
        if (!hrm_pay_core_003_validate_input_definition($definition)) return false;
    }
    return $row['calculation_mode'] !== 'input' || count($row['input_definitions']) > 0;
}

function hrm_pay_core_003_validate_eligibility_version($row)
{
    if (!is_array($row)) return false;
    foreach (array('element_version_id','version_no','effective_from','eligibility_kind') as $key)
        if (!array_key_exists($key,$row)) return false;
    if ((int)$row['element_version_id'] <= 0 || (int)$row['version_no'] < 1
        || !hrm_pay_core_003_valid_effective_range($row['effective_from'], isset($row['effective_to'])?$row['effective_to']:null))
        return false;
    if (!in_array($row['eligibility_kind'], hrm_pay_core_003_eligibility_kinds(), true)) return false;
    $artifact = isset($row['rule_artifact_sha256']) ? $row['rule_artifact_sha256'] : null;
    if ($row['eligibility_kind'] === 'rule') return hrm_pay_core_003_is_sha256($artifact);
    return $artifact === null || $artifact === '' || hrm_pay_core_003_is_sha256($artifact);
}

function hrm_pay_core_003_validate_typed_value($value_type, $value)
{
    if (!in_array($value_type, hrm_pay_core_003_input_value_types(), true)) return false;
    if ($value_type === 'decimal')
        return is_int($value) || is_float($value)
            || (is_string($value) && preg_match('/^-?[0-9]+(?:\.[0-9]+)?$/D',$value) === 1);
    if ($value_type === 'date') return hrm_pay_core_003_is_iso_date($value);
    if ($value_type === 'code') return is_string($value) && trim($value) !== '' && strlen($value) <= 128;
    return $value === true || $value === false || $value === 0 || $value === 1 || $value === '0' || $value === '1';
}

function hrm_pay_core_003_validate_approved_input_candidate($row)
{
    if (!is_array($row)) return false;
    foreach (array('payroll_relationship_id','element_version_id','input_definition_id','effective_date',
        'value_type','value','source_kind','source_event_id','source_sha256','approval_evidence_sha256') as $key)
        if (!array_key_exists($key,$row)) return false;
    if ((int)$row['payroll_relationship_id'] <= 0 || (int)$row['element_version_id'] <= 0
        || (int)$row['input_definition_id'] <= 0) return false;
    if (isset($row['assignment_id']) && $row['assignment_id'] !== null && (int)$row['assignment_id'] <= 0) return false;
    if (!hrm_pay_core_003_is_iso_date($row['effective_date'])
        || !hrm_pay_core_003_validate_typed_value($row['value_type'],$row['value'])) return false;
    if (hrm_pay_core_003_token($row['source_kind'],32) === false
        || hrm_pay_core_003_token($row['source_event_id'],128) === false) return false;
    if (!hrm_pay_core_003_is_sha256($row['source_sha256'])
        || !hrm_pay_core_003_is_sha256($row['approval_evidence_sha256'])) return false;
    if (isset($row['override_reason']) && $row['override_reason'] !== null
        && (trim((string)$row['override_reason']) === '' || strlen((string)$row['override_reason']) > 255)) return false;
    if (isset($row['supersedes_input_id']) && $row['supersedes_input_id'] !== null
        && (int)$row['supersedes_input_id'] <= 0) return false;
    return true;
}

/** Legacy mapping adds source-specific disposition fields to the governed writer contracts. */
function hrm_pay_core_003_validate_mapping_resolution($source_kind, $resolution)
{
    if (!in_array($source_kind,array('pay_element','salary_structure','employee_salary'),true)
        || !is_array($resolution) || !isset($resolution['target_kind'])
        || !in_array($resolution['target_kind'],hrm_pay_core_003_mapping_target_kinds(),true)) return false;
    $kind = $resolution['target_kind'];
    if ($kind === 'documented_exception')
        return hrm_pay_core_003_token(isset($resolution['reason_code'])?$resolution['reason_code']:'',96) !== false
            && isset($resolution['evidence_sha256']) && hrm_pay_core_003_is_sha256($resolution['evidence_sha256']);
    if ($kind === 'element_version_bundle') {
        return $source_kind === 'pay_element'
            && isset($resolution['legacy_default_amount_action'])
            && in_array($resolution['legacy_default_amount_action'],array('defer','approved_input_required','not_applicable'),true)
            && hrm_pay_core_003_validate_element_version_bundle($resolution);
    }
    if ($kind === 'eligibility_version') {
        return $source_kind === 'salary_structure'
            && isset($resolution['legacy_amount_action']) && $resolution['legacy_amount_action'] === 'defer'
            && hrm_pay_core_003_validate_eligibility_version($resolution);
    }
    if ($source_kind !== 'employee_salary' || !isset($resolution['value_source'])
        || $resolution['value_source'] !== 'legacy_amount') return false;
    $candidate = $resolution;
    $candidate['value'] = 0;
    return hrm_pay_core_003_validate_approved_input_candidate($candidate);
}

function hrm_pay_core_003_mapping_plan_hash($source_kind,$source_row_id,$source_sha256,$resolution,$supersedes_mapping_plan_id,$maker_id)
{
    if (!hrm_pay_core_003_is_sha256($source_sha256) || (int)$source_row_id <= 0 || (int)$maker_id <= 0
        || !hrm_pay_core_003_validate_mapping_resolution($source_kind,$resolution)) return false;
    return hrm_pay_core_003_hash(array(
        'source_kind'=>$source_kind,
        'source_row_id'=>(int)$source_row_id,
        'source_sha256'=>$source_sha256,
        'target_kind'=>$resolution['target_kind'],
        'target_payload_sha256'=>hrm_pay_core_003_hash($resolution),
        'supersedes_mapping_plan_id'=>$supersedes_mapping_plan_id===null?null:(int)$supersedes_mapping_plan_id,
        'maker_id'=>(int)$maker_id,
    ));
}

function hrm_pay_core_003_parity_projection_sha256($source_sha256,$target_set_sha256,$scope_token)
{
    if (!hrm_pay_core_003_is_sha256($source_sha256) || !hrm_pay_core_003_is_sha256($target_set_sha256)
        || hrm_pay_core_003_token($scope_token,96) === false) return false;
    return hrm_pay_core_003_hash(array(
        'source_sha256'=>$source_sha256,
        'target_set_sha256'=>$target_set_sha256,
        'scope_token'=>$scope_token,
        'authoritative'=>false,
    ));
}
