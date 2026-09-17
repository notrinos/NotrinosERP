<?php
/**
 * PAY-RULE-003 pure typed decision-table contract foundation.
 *
 * This file validates in-memory artifacts only. It performs no database,
 * filesystem, network, session/global or payroll mutation work.
 */
if (!defined('HRM_PAY_RULE_003_DECISION_CONTRACT_VERSION'))
    define('HRM_PAY_RULE_003_DECISION_CONTRACT_VERSION', '1.0.473');

function hrm_payroll_decision_table_is_list($value)
{
    if (!is_array($value)) return false;
    $i = 0;
    foreach ($value as $k => $unused) {
        if ($k !== $i++) return false;
    }
    return true;
}

function hrm_payroll_decision_table_fail(&$error, $code)
{
    $error = $code;
    return false;
}

function hrm_payroll_decision_table_exact_keys($value, $required, $optional, &$error, $prefix)
{
    if (!is_array($value)) return hrm_payroll_decision_table_fail($error, $prefix.'_not_object');
    foreach ($required as $key)
        if (!array_key_exists($key, $value)) return hrm_payroll_decision_table_fail($error, $prefix.'_missing_'.$key);
    $allowed = array_flip(array_merge($required, $optional));
    foreach ($value as $key => $unused)
        if (!isset($allowed[$key])) return hrm_payroll_decision_table_fail($error, $prefix.'_unknown_'.$key);
    return true;
}

function hrm_payroll_decision_table_valid_name($value)
{
    return is_string($value) && preg_match('/^[A-Z][A-Z0-9_]{0,63}$/D', $value) === 1;
}

function hrm_payroll_decision_table_valid_id($value)
{
    return is_string($value) && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D', $value) === 1;
}

function hrm_payroll_decision_table_valid_semver($value)
{
    return is_string($value) && preg_match('/^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[A-Za-z0-9.-]+)?$/D', $value) === 1;
}

function hrm_payroll_decision_table_valid_date($value)
{
    if (!is_string($value) || preg_match('/^(\d{4})-(\d{2})-(\d{2})$/D', $value, $m) !== 1) return false;
    return checkdate((int)$m[2], (int)$m[3], (int)$m[1]);
}

function hrm_payroll_decision_table_scalar_key($value)
{
    if (is_bool($value)) return $value ? 'b:1' : 'b:0';
    if (is_int($value)) return 'i:'.$value;
    if (is_string($value)) return 's:'.$value;
    return 'x:'.gettype($value);
}

function hrm_payroll_decision_table_validate_scalar($type, $value)
{
    if ($type === 'boolean') return is_bool($value);
    if ($type === 'integer') return is_int($value) || (is_string($value) && preg_match('/^-?(0|[1-9][0-9]*)$/D', $value) === 1);
    if ($type === 'decimal') return is_string($value) && preg_match('/^-?(0|[1-9][0-9]*)(?:\.[0-9]+)?$/D', $value) === 1;
    if ($type === 'date') return hrm_payroll_decision_table_valid_date($value);
    if ($type === 'string') return is_string($value) && strlen($value) <= 256;
    return false;
}

function hrm_payroll_decision_table_decimal_compare($left, $right)
{
    $parts=array();
    foreach(array((string)$left,(string)$right) as $raw){
        $negative=isset($raw[0])&&$raw[0]==='-';
        if($negative)$raw=substr($raw,1);
        $pair=explode('.',$raw,2);
        $whole=ltrim($pair[0],'0');if($whole==='')$whole='0';
        $fraction=isset($pair[1])?rtrim($pair[1],'0'):'';
        if($whole==='0'&&$fraction==='')$negative=false;
        $parts[]=array($negative,$whole,$fraction);
    }
    if($parts[0][0]!==$parts[1][0])return $parts[0][0]?-1:1;
    $sign=$parts[0][0]?-1:1;
    if(strlen($parts[0][1])!==strlen($parts[1][1]))return strlen($parts[0][1])<strlen($parts[1][1])?-$sign:$sign;
    $cmp=strcmp($parts[0][1],$parts[1][1]);if($cmp!==0)return $cmp<0?-$sign:$sign;
    $len=max(strlen($parts[0][2]),strlen($parts[1][2]));
    $a=str_pad($parts[0][2],$len,'0');$b=str_pad($parts[1][2],$len,'0');$cmp=strcmp($a,$b);
    if($cmp===0)return 0;return $cmp<0?-$sign:$sign;
}

function hrm_payroll_decision_table_compare_scalar($type, $left, $right)
{
    if ($type === 'decimal' || $type === 'integer') return hrm_payroll_decision_table_decimal_compare((string)$left,(string)$right);
    if ($type === 'date' || $type === 'string') return strcmp((string)$left, (string)$right);
    return 0;
}

function hrm_payroll_decision_table_validate_column($column, $kind, &$error)
{
    $required = array('name','type','unit');
    $optional = $kind === 'input' ? array('null_policy') : array('required','default');
    if (!hrm_payroll_decision_table_exact_keys($column, $required, $optional, $error, 'decision_'.$kind)) return false;
    if (!hrm_payroll_decision_table_valid_name($column['name'])) return hrm_payroll_decision_table_fail($error, 'decision_'.$kind.'_name_invalid');
    $types = array('boolean','integer','decimal','date','string');
    $units = array('boolean','count','currency','date','day','hour','quantity','rate','scalar','code');
    if (!in_array($column['type'], $types, true)) return hrm_payroll_decision_table_fail($error, 'decision_'.$kind.'_type_invalid');
    if (!in_array($column['unit'], $units, true)) return hrm_payroll_decision_table_fail($error, 'decision_'.$kind.'_unit_invalid');
    if ($column['type'] === 'boolean' && $column['unit'] !== 'boolean') return hrm_payroll_decision_table_fail($error, 'decision_'.$kind.'_boolean_unit_invalid');
    if ($column['type'] === 'date' && $column['unit'] !== 'date') return hrm_payroll_decision_table_fail($error, 'decision_'.$kind.'_date_unit_invalid');
    if ($kind === 'input') {
        if (!isset($column['null_policy']) || !in_array($column['null_policy'], array('reject','no_match','allow'), true))
            return hrm_payroll_decision_table_fail($error, 'decision_input_null_policy_invalid');
    } else {
        if (!array_key_exists('required', $column) || !is_bool($column['required']))
            return hrm_payroll_decision_table_fail($error, 'decision_output_required_invalid');
        if (!$column['required'] && !array_key_exists('default', $column))
            return hrm_payroll_decision_table_fail($error, 'decision_output_default_required');
        if (array_key_exists('default', $column) && !hrm_payroll_decision_table_validate_scalar($column['type'], $column['default']))
            return hrm_payroll_decision_table_fail($error, 'decision_output_default_type_invalid');
    }
    return true;
}

function hrm_payroll_decision_table_validate_predicate($predicate, $inputs, &$error)
{
    if (!is_array($predicate)) return hrm_payroll_decision_table_fail($error, 'decision_predicate_not_object');
    if (!isset($predicate['input']) || !isset($predicate['op'])) return hrm_payroll_decision_table_fail($error, 'decision_predicate_shape_invalid');
    $op = $predicate['op'];
    $has_value = array_key_exists('value', $predicate);
    $required = $op === 'exists' ? array('input','op') : array('input','op','value');
    if (!hrm_payroll_decision_table_exact_keys($predicate, $required, array(), $error, 'decision_predicate')) return false;
    if (!isset($inputs[$predicate['input']])) return hrm_payroll_decision_table_fail($error, 'decision_predicate_input_unknown');
    $type = $inputs[$predicate['input']]['type'];
    $allowed = array(
        'boolean'=>array('eq','neq','exists'),
        'integer'=>array('eq','neq','lt','lte','gt','gte','between','in','exists'),
        'decimal'=>array('eq','neq','lt','lte','gt','gte','between','in','exists'),
        'date'=>array('eq','neq','lt','lte','gt','gte','between','in','exists'),
        'string'=>array('eq','neq','in','exists')
    );
    if (!in_array($op, $allowed[$type], true)) return hrm_payroll_decision_table_fail($error, 'decision_predicate_operator_invalid');
    if ($op === 'exists') return !$has_value ? true : hrm_payroll_decision_table_fail($error, 'decision_predicate_exists_has_value');
    $value = $predicate['value'];
    if ($op === 'between') {
        if (!hrm_payroll_decision_table_is_list($value) || count($value) !== 2) return hrm_payroll_decision_table_fail($error, 'decision_predicate_between_shape_invalid');
        if (!hrm_payroll_decision_table_validate_scalar($type, $value[0]) || !hrm_payroll_decision_table_validate_scalar($type, $value[1]))
            return hrm_payroll_decision_table_fail($error, 'decision_predicate_between_type_invalid');
        if (hrm_payroll_decision_table_compare_scalar($type, $value[0], $value[1]) > 0)
            return hrm_payroll_decision_table_fail($error, 'decision_predicate_between_bounds_invalid');
        return true;
    }
    if ($op === 'in') {
        if (!hrm_payroll_decision_table_is_list($value) || count($value) < 1 || count($value) > 100)
            return hrm_payroll_decision_table_fail($error, 'decision_predicate_in_shape_invalid');
        $seen = array();
        foreach ($value as $member) {
            if (!hrm_payroll_decision_table_validate_scalar($type, $member)) return hrm_payroll_decision_table_fail($error, 'decision_predicate_in_type_invalid');
            $key=hrm_payroll_decision_table_scalar_key($member);
            if (isset($seen[$key])) return hrm_payroll_decision_table_fail($error, 'decision_predicate_in_duplicate');
            $seen[$key]=true;
        }
        return true;
    }
    if (!hrm_payroll_decision_table_validate_scalar($type, $value)) return hrm_payroll_decision_table_fail($error, 'decision_predicate_value_type_invalid');
    return true;
}

function hrm_payroll_decision_table_canonicalize($value)
{
    if (!is_array($value)) return $value;
    if (hrm_payroll_decision_table_is_list($value)) {
        $out=array(); foreach($value as $v) $out[]=hrm_payroll_decision_table_canonicalize($v); return $out;
    }
    ksort($value, SORT_STRING);
    foreach ($value as $k=>$v) $value[$k]=hrm_payroll_decision_table_canonicalize($v);
    return $value;
}

function hrm_payroll_decision_table_validate($artifact, &$error)
{
    $error = null;
    $required=array('schema_version','artifact_type','table_id','version','hit_policy','inputs','outputs','rows','overlap_policy','gap_policy');
    if (!hrm_payroll_decision_table_exact_keys($artifact,$required,array('description'),$error,'decision_artifact')) return false;
    if ($artifact['schema_version'] !== 1 || $artifact['artifact_type'] !== 'decision_table') return hrm_payroll_decision_table_fail($error,'decision_artifact_identity_invalid');
    if (!hrm_payroll_decision_table_valid_id($artifact['table_id']) || !hrm_payroll_decision_table_valid_semver($artifact['version'])) return hrm_payroll_decision_table_fail($error,'decision_artifact_version_invalid');
    if (!in_array($artifact['hit_policy'],array('unique','first_by_priority','collect','sum'),true)) return hrm_payroll_decision_table_fail($error,'decision_hit_policy_invalid');
    if (!in_array($artifact['overlap_policy'],array('reject','allow'),true) || !in_array($artifact['gap_policy'],array('reject','allow'),true)) return hrm_payroll_decision_table_fail($error,'decision_interval_policy_invalid');
    if (!hrm_payroll_decision_table_is_list($artifact['inputs']) || count($artifact['inputs'])<1 || count($artifact['inputs'])>32) return hrm_payroll_decision_table_fail($error,'decision_inputs_invalid');
    if (!hrm_payroll_decision_table_is_list($artifact['outputs']) || count($artifact['outputs'])<1 || count($artifact['outputs'])>16) return hrm_payroll_decision_table_fail($error,'decision_outputs_invalid');
    $inputs=array();
    foreach($artifact['inputs'] as $column){ if(!hrm_payroll_decision_table_validate_column($column,'input',$error)) return false; if(isset($inputs[$column['name']])) return hrm_payroll_decision_table_fail($error,'decision_input_duplicate'); $inputs[$column['name']]=$column; }
    $outputs=array();
    foreach($artifact['outputs'] as $column){ if(!hrm_payroll_decision_table_validate_column($column,'output',$error)) return false; if(isset($outputs[$column['name']])) return hrm_payroll_decision_table_fail($error,'decision_output_duplicate'); $outputs[$column['name']]=$column; }
    if ($artifact['hit_policy']==='sum') foreach($outputs as $column) if(!in_array($column['type'],array('integer','decimal'),true)) return hrm_payroll_decision_table_fail($error,'decision_sum_output_non_numeric');
    if (!hrm_payroll_decision_table_is_list($artifact['rows']) || count($artifact['rows'])<1 || count($artifact['rows'])>1000) return hrm_payroll_decision_table_fail($error,'decision_rows_invalid');
    $row_ids=array();$priorities=array();$signatures=array();
    foreach($artifact['rows'] as $row){
        $optional=$artifact['hit_policy']==='first_by_priority'?array('priority'):array();
        if(!hrm_payroll_decision_table_exact_keys($row,array('row_id','predicates','outputs','effective_from','effective_to','source_ids','explanation_label'),$optional,$error,'decision_row')) return false;
        if(!hrm_payroll_decision_table_valid_id($row['row_id'])) return hrm_payroll_decision_table_fail($error,'decision_row_id_invalid');
        if(isset($row_ids[$row['row_id']])) return hrm_payroll_decision_table_fail($error,'decision_row_id_duplicate'); $row_ids[$row['row_id']]=true;
        if($artifact['hit_policy']==='first_by_priority'){
            if(!array_key_exists('priority',$row)||!is_int($row['priority'])) return hrm_payroll_decision_table_fail($error,'decision_priority_required');
            if(isset($priorities[$row['priority']])) return hrm_payroll_decision_table_fail($error,'decision_priority_duplicate');$priorities[$row['priority']]=true;
        }
        if(!hrm_payroll_decision_table_is_list($row['predicates'])||count($row['predicates'])<1||count($row['predicates'])>32) return hrm_payroll_decision_table_fail($error,'decision_row_predicates_invalid');
        foreach($row['predicates'] as $predicate) if(!hrm_payroll_decision_table_validate_predicate($predicate,$inputs,$error)) return false;
        if(!is_array($row['outputs'])) return hrm_payroll_decision_table_fail($error,'decision_row_outputs_invalid');
        foreach($row['outputs'] as $name=>$value){ if(!isset($outputs[$name])) return hrm_payroll_decision_table_fail($error,'decision_row_output_unknown'); if(!hrm_payroll_decision_table_validate_scalar($outputs[$name]['type'],$value)) return hrm_payroll_decision_table_fail($error,'decision_row_output_type_invalid'); }
        foreach($outputs as $name=>$column) if($column['required']&&!array_key_exists($name,$row['outputs'])) return hrm_payroll_decision_table_fail($error,'decision_row_output_required_missing');
        if(!hrm_payroll_decision_table_valid_date($row['effective_from'])) return hrm_payroll_decision_table_fail($error,'decision_row_effective_from_invalid');
        if($row['effective_to']!==null&&!hrm_payroll_decision_table_valid_date($row['effective_to'])) return hrm_payroll_decision_table_fail($error,'decision_row_effective_to_invalid');
        if($row['effective_to']!==null&&strcmp($row['effective_from'],$row['effective_to'])>0) return hrm_payroll_decision_table_fail($error,'decision_row_effective_interval_invalid');
        if(!hrm_payroll_decision_table_is_list($row['source_ids'])||count($row['source_ids'])<1||count($row['source_ids'])>16) return hrm_payroll_decision_table_fail($error,'decision_row_source_ids_invalid');
        $sources=array();foreach($row['source_ids'] as $source){if(!hrm_payroll_decision_table_valid_id($source))return hrm_payroll_decision_table_fail($error,'decision_row_source_id_invalid');if(isset($sources[$source]))return hrm_payroll_decision_table_fail($error,'decision_row_source_id_duplicate');$sources[$source]=true;}
        if(!is_string($row['explanation_label'])||trim($row['explanation_label'])===''||strlen($row['explanation_label'])>200) return hrm_payroll_decision_table_fail($error,'decision_row_explanation_invalid');
        $signature=hash('sha256',json_encode(hrm_payroll_decision_table_canonicalize(array('predicates'=>$row['predicates'],'effective_from'=>$row['effective_from'],'effective_to'=>$row['effective_to']))));
        if(isset($signatures[$signature])) return hrm_payroll_decision_table_fail($error,'decision_row_duplicate_match_surface');$signatures[$signature]=true;
    }
    return true;
}

function hrm_payroll_decision_table_hash($artifact, &$error)
{
    if (!hrm_payroll_decision_table_validate($artifact, $error)) return false;
    return hash('sha256', json_encode(hrm_payroll_decision_table_canonicalize($artifact), JSON_UNESCAPED_SLASHES));
}
