<?php
/** HRM-FND-007 explicit maker/checker legacy custom_data mapping command custody. */
if (!function_exists('hrm_typed_attribute_legacy_mapping_key'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_mapping_db.inc');

function hrm_typed_attribute_legacy_mapping_user_can_manage()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_MANAGE_TYPED_ATTRIBUTE_LEGACY_MAPPING');
}

function hrm_typed_attribute_legacy_mapping_user_can_approve()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_APPROVE_TYPED_ATTRIBUTE_LEGACY_MAPPING');
}

function hrm_typed_attribute_legacy_mapping_positive_id($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable) return null;
    if (is_int($value)) return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1) return false;
    $number=(int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

function hrm_typed_attribute_legacy_mapping_review_ref($value)
{
    if (!is_string($value) || strlen($value) < 1 || strlen($value) > 128) return false;
    return preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:\/-]{0,127}$/D', $value) === 1 ? $value : false;
}

/** Canonical maker request. Raw legacy values are intentionally absent from the grammar. */
function hrm_typed_attribute_legacy_mapping_canonical_command($input)
{
    $allowed=array('legacy_key','attribute_code','effective_from','effective_to','qualified_review_ref','predecessor_id');
    if (!is_array($input) || array_diff(array_keys($input),$allowed)) return false;
    foreach(array('legacy_key','attribute_code','effective_from','qualified_review_ref') as $field)
        if(!array_key_exists($field,$input) || !is_string($input[$field])) return false;
    $key=hrm_typed_attribute_legacy_mapping_key($input['legacy_key']);
    $code=preg_match('/^[a-z][a-z0-9_.-]{0,63}$/D',$input['attribute_code'])===1 ? $input['attribute_code'] : false;
    $from=hrm_typed_attribute_legacy_mapping_date($input['effective_from'],false);
    $to=hrm_typed_attribute_legacy_mapping_date(array_key_exists('effective_to',$input)?$input['effective_to']:null,true);
    $review=hrm_typed_attribute_legacy_mapping_review_ref($input['qualified_review_ref']);
    $predecessor=hrm_typed_attribute_legacy_mapping_positive_id(array_key_exists('predecessor_id',$input)?$input['predecessor_id']:null,true);
    if($key===false||$code===false||$from===false||$to===false||$review===false||$predecessor===false
        ||($to!==null&&strcmp($to,$from)<0)) return false;
    return array('legacy_key'=>$key,'attribute_code'=>$code,'subject_type'=>'worker','mapping_status'=>'draft',
        'effective_from'=>$from,'effective_to'=>$to,'qualified_review_ref'=>$review,'predecessor_id'=>$predecessor);
}

function hrm_typed_attribute_legacy_mapping_command_digest($command)
{
    if(!is_array($command))return false;
    $json=json_encode($command,JSON_UNESCAPED_SLASHES);
    return $json===false?false:hash('sha256','hrm-fnd-007-legacy-mapping-command-v1|'.$json);
}
