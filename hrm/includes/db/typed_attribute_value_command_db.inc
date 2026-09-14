<?php
/**
 * HRM-FND-007 Typed Attribute value command/policy custody.
 *
 * This layer resolves one published effective definition, validates subject
 * existence and canonicalizes one value command. It does not mutate values.
 */
if (!function_exists('hrm_typed_attribute_definition_validation_json'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_definition_command_db.inc');

function hrm_typed_attribute_value_user_can_manage($sensitivity)
{
    if (!function_exists('user_check_access')
        || !in_array($sensitivity, hrm_typed_attribute_sensitivities(), true)) return false;
    if (!user_check_access('SA_EMPLOYEE') || !user_check_access('SA_HRM_MANAGE_TYPED_ATTRIBUTE_VALUE'))
        return false;
    return $sensitivity === 'standard'
        || user_check_access('SA_HRM_ACCESS_SENSITIVE_TYPED_ATTRIBUTE_VALUE');
}

function hrm_typed_attribute_value_user_can_view($sensitivity)
{
    if (!function_exists('user_check_access')
        || !in_array($sensitivity, hrm_typed_attribute_sensitivities(), true)) return false;
    if (!user_check_access('SA_EMPLOYEE') || !user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_VALUE'))
        return false;
    return $sensitivity === 'standard'
        || user_check_access('SA_HRM_ACCESS_SENSITIVE_TYPED_ATTRIBUTE_VALUE');
}

function hrm_typed_attribute_value_object_scope_security_area($owner_scope)
{
    $map = array(
        'hr'=>'SA_HRM_ACCESS_TYPED_ATTRIBUTE_SCOPE_HR',
        'payroll'=>'SA_HRM_ACCESS_TYPED_ATTRIBUTE_SCOPE_PAYROLL',
        'security'=>'SA_HRM_ACCESS_TYPED_ATTRIBUTE_SCOPE_SECURITY',
        'localization'=>'SA_HRM_ACCESS_TYPED_ATTRIBUTE_SCOPE_LOCALIZATION',
    );
    return isset($map[$owner_scope]) ? $map[$owner_scope] : false;
}

function hrm_typed_attribute_value_user_can_access_object_scope($owner_scope)
{
    $area = hrm_typed_attribute_value_object_scope_security_area($owner_scope);
    return $area !== false && function_exists('user_check_access') && user_check_access($area);
}

function hrm_typed_attribute_value_positive_id($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable) return null;
    if (is_int($value)) return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1) return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

function hrm_typed_attribute_value_datetime($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable) return null;
    if (!is_string($value)
        || preg_match('/^(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})$/D', $value, $p) !== 1)
        return false;
    return checkdate((int)$p[2], (int)$p[3], (int)$p[1])
        && (int)$p[4] <= 23 && (int)$p[5] <= 59 && (int)$p[6] <= 59 ? $value : false;
}

function hrm_typed_attribute_value_attribute_code($value)
{
    return is_string($value) && preg_match('/^[a-z][a-z0-9_.-]{2,63}$/D', $value) === 1 ? $value : false;
}

function hrm_typed_attribute_value_subject_table($subject_type)
{
    $map = array(
        'person'=>array('table'=>'hrm_persons','column'=>'person_id'),
        'worker'=>array('table'=>'hrm_workers','column'=>'worker_id'),
        'employment'=>array('table'=>'hrm_employments','column'=>'employment_id'),
        'assignment'=>array('table'=>'hrm_assignments','column'=>'assignment_id'),
    );
    return isset($map[$subject_type]) ? $map[$subject_type] : false;
}

function hrm_typed_attribute_value_subject_exists($subject_type, $subject_id, $for_update = false)
{
    $binding = hrm_typed_attribute_value_subject_table($subject_type);
    $subject_id = hrm_typed_attribute_value_positive_id($subject_id, false);
    if ($binding === false || $subject_id === false) return false;
    $sql = 'SELECT '.$binding['column'].' FROM '.TB_PREF.$binding['table']
        .' WHERE '.$binding['column'].'='.(int)$subject_id.' LIMIT 2'.($for_update ? ' FOR UPDATE' : '');
    $r = db_query($sql, 'could not validate Typed Attribute subject custody');
    return $r && db_num_rows($r) === 1;
}

/**
 * Resolve the latest finalized definition lifecycle state as of one command date.
 * Pending maker/checker rows are deliberately ignored until checker application.
 */
function hrm_typed_attribute_definition_resolve_state($attribute_code, $effective_at, $for_update = false)
{
    $attribute_code = hrm_typed_attribute_value_attribute_code($attribute_code);
    $effective_at = hrm_typed_attribute_value_datetime($effective_at, false);
    if ($attribute_code === false || $effective_at === false || !is_bool($for_update)) return false;
    $date = substr($effective_at, 0, 10);
    $sql = 'SELECT d.definition_id,d.attribute_code,v.definition_version_id,v.version_no,v.value_type,'
        .'v.allowed_subject,v.sensitivity,v.owner_scope,v.validation_json,v.definition_status,'
        .'v.effective_from,v.effective_to FROM '.TB_PREF.'hrm_attribute_definitions d '
        .'INNER JOIN '.TB_PREF.'hrm_attribute_definition_versions v ON v.definition_id=d.definition_id '
        .'WHERE d.attribute_code='.db_escape($attribute_code)
        ." AND v.definition_status IN ('active','deprecated','retired')"
        .' AND v.effective_from<='.db_escape($date)
        .' ORDER BY v.effective_from DESC,v.version_no DESC,v.definition_version_id DESC LIMIT 2'
        .($for_update ? ' FOR UPDATE' : '');
    $r = db_query($sql, 'could not resolve Typed Attribute definition lifecycle state');
    if (!$r || db_num_rows($r) < 1) return false;
    $row = db_fetch_assoc($r);
    $metadata = array(
        'attribute_code'=>(string)$row['attribute_code'],
        'value_type'=>(string)$row['value_type'],
        'allowed_subject'=>(string)$row['allowed_subject'],
        'sensitivity'=>(string)$row['sensitivity'],
        'owner_scope'=>(string)$row['owner_scope'],
        'validation_json'=>(string)$row['validation_json'],
    );
    $validation = hrm_typed_attribute_definition_validation_json(
        (string)$row['value_type'], (string)$row['validation_json']);
    if (!hrm_typed_attribute_validate_definition_metadata($metadata)
        || $validation === false || $validation !== (string)$row['validation_json']) return false;
    return $row;
}

/**
 * Resolve a definition for one usage. New writes require active state; current
 * reads may use active or deprecated state. Retired state always fails closed.
 */
function hrm_typed_attribute_value_resolve_definition($attribute_code, $effective_at, $for_update = false, $usage = 'write')
{
    if (!in_array($usage,array('write','read'),true)) return false;
    $row=hrm_typed_attribute_definition_resolve_state($attribute_code,$effective_at,$for_update);
    if (!is_array($row)) return false;
    $status=(string)$row['definition_status'];
    if ($usage==='write' && $status!=='active') return false;
    if ($usage==='read' && !in_array($status,array('active','deprecated'),true)) return false;
    if ($row['effective_to']!==null && substr((string)$effective_at,0,10)>(string)$row['effective_to']) return false;
    return $row;
}

function hrm_typed_attribute_value_apply_validation($definition, $raw_value)
{
    if (!is_array($definition) || !isset($definition['value_type'],$definition['validation_json'])) return false;
    $canonical = hrm_typed_attribute_canonical_value((string)$definition['value_type'], $raw_value);
    // The accepted sensitive-value crypto primitive intentionally accepts 1..1024 bytes.
    // Fail here rather than accepting a command that the encrypted writer cannot persist.
    if ($canonical === false || $canonical === '' || strlen($canonical) > 1024) return false;
    $validation = json_decode((string)$definition['validation_json'], true);
    if (!is_array($validation) || json_last_error() !== JSON_ERROR_NONE) return false;
    if (isset($validation['minLength']) && strlen($canonical) < (int)$validation['minLength']) return false;
    if (isset($validation['maxLength']) && strlen($canonical) > (int)$validation['maxLength']) return false;
    if (isset($validation['allowedValues'])) {
        if (!is_array($validation['allowedValues']) || !in_array($canonical, $validation['allowedValues'], true))
            return false;
    }
    if (isset($validation['scale'])) {
        $dot = strpos($canonical, '.');
        $scale = $dot === false ? 0 : strlen($canonical) - $dot - 1;
        if ($scale > (int)$validation['scale']) return false;
    }
    return $canonical;
}

/** Canonicalize one value command against one already-resolved active definition. */
function hrm_typed_attribute_value_canonical_command($input, $definition)
{
    $allowed = array('attribute_code','subject_type','subject_id','value','effective_from','effective_to','predecessor_id','source');
    if (!is_array($input) || array_diff(array_keys($input), $allowed) || !is_array($definition)) return false;
    foreach (array('attribute_code','subject_type','effective_from') as $field)
        if (!array_key_exists($field, $input) || !is_string($input[$field])) return false;
    if (!array_key_exists('subject_id',$input) || !array_key_exists('value',$input)) return false;
    $code = hrm_typed_attribute_value_attribute_code($input['attribute_code']);
    $subject_type = in_array($input['subject_type'], hrm_typed_attribute_subject_types(), true) ? $input['subject_type'] : false;
    $subject_id = hrm_typed_attribute_value_positive_id($input['subject_id'], false);
    $from = hrm_typed_attribute_value_datetime($input['effective_from'], false);
    $to = hrm_typed_attribute_value_datetime(array_key_exists('effective_to',$input)?$input['effective_to']:null, true);
    $predecessor = hrm_typed_attribute_value_positive_id(array_key_exists('predecessor_id',$input)?$input['predecessor_id']:null, true);
    $source = array_key_exists('source',$input) ? $input['source'] : 'manual';
    if ($code === false || $subject_type === false || $subject_id === false || $from === false || $to === false
        || $predecessor === false || !is_string($source) || $source !== 'manual'
        || ($to !== null && strcmp($to,$from) < 0)
        || array_diff(array('definition_id','attribute_code','definition_version_id','definition_status','allowed_subject',
            'sensitivity','effective_from','effective_to','value_type','validation_json'), array_keys($definition))
        || $definition['attribute_code'] !== $code || $definition['definition_status'] !== 'active'
        || $definition['allowed_subject'] !== $subject_type) return false;
    $date = substr($from,0,10);
    if ($date < (string)$definition['effective_from']
        || ($definition['effective_to'] !== null && $date > (string)$definition['effective_to'])) return false;
    $canonical_value = hrm_typed_attribute_value_apply_validation($definition, $input['value']);
    if ($canonical_value === false
        || !hrm_typed_attribute_value_user_can_manage((string)$definition['sensitivity'])
        || !hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope'])) return false;
    return array(
        'attribute_code'=>$code,
        'definition_id'=>(int)$definition['definition_id'],
        'definition_version_id'=>(int)$definition['definition_version_id'],
        'subject_type'=>$subject_type,
        'subject_id'=>$subject_id,
        'value'=>$canonical_value,
        'sensitivity'=>(string)$definition['sensitivity'],
        'effective_from'=>$from,
        'effective_to'=>$to,
        'predecessor_id'=>$predecessor,
        'source'=>'manual',
    );
}
