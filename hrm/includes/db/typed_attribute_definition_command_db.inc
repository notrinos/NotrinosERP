<?php
/**
 * HRM-FND-007 Typed Attribute definition/version maker-command custody.
 *
 * Definition metadata is governed, non-subject configuration. This helper is
 * intentionally non-mutating: it validates one draft command and derives an
 * opaque retry digest. Attribute-value writers remain out of scope.
 */
if (!function_exists('hrm_typed_attribute_validate_definition_metadata'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_db.inc');

function hrm_typed_attribute_definition_user_can_manage()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_MANAGE_TYPED_ATTRIBUTE_DEFINITION');
}

function hrm_typed_attribute_definition_positive_id($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable) return null;
    if (is_int($value)) return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1) return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

function hrm_typed_attribute_definition_date($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable) return null;
    if (!is_string($value) || preg_match('/^(\d{4})-(\d{2})-(\d{2})$/D', $value, $p) !== 1)
        return false;
    return checkdate((int)$p[2], (int)$p[3], (int)$p[1]) ? $value : false;
}

/**
 * Canonicalize the deliberately bounded validation grammar.
 * Regex/pattern execution is excluded from this release to avoid introducing
 * an unbounded evaluator at the configuration boundary.
 */
function hrm_typed_attribute_definition_validation_json($value_type, $raw)
{
    if (!is_string($raw)) return false;
    $raw = trim($raw);
    if ($raw === '' || strlen($raw) > 4000 || $raw[0] !== '{' || substr($raw, -1) !== '}') return false;
    $decoded = json_decode($raw, true);
    if (!is_array($decoded) || json_last_error() !== JSON_ERROR_NONE) return false;
    $allowed = array('minLength','maxLength','allowedValues','scale','unit');
    if (array_diff(array_keys($decoded), $allowed)) return false;
    $out = array();

    if (array_key_exists('minLength', $decoded) || array_key_exists('maxLength', $decoded)) {
        if (!in_array($value_type, array('text','code'), true)) return false;
        $ceiling = $value_type === 'text' ? 4000 : 128;
        foreach (array('minLength','maxLength') as $field) {
            if (!array_key_exists($field, $decoded)) continue;
            if (!is_int($decoded[$field]) || $decoded[$field] < 0 || $decoded[$field] > $ceiling) return false;
            $out[$field] = $decoded[$field];
        }
        if (isset($out['minLength']) && isset($out['maxLength']) && $out['minLength'] > $out['maxLength']) return false;
    }
    if (array_key_exists('scale', $decoded)) {
        if ($value_type !== 'decimal' || !is_int($decoded['scale']) || $decoded['scale'] < 0 || $decoded['scale'] > 12)
            return false;
        $out['scale'] = $decoded['scale'];
    }
    if (array_key_exists('unit', $decoded)) {
        if (!in_array($value_type, array('integer','decimal'), true) || !is_string($decoded['unit'])
            || preg_match('/^[A-Za-z][A-Za-z0-9._\/%-]{0,31}$/D', $decoded['unit']) !== 1)
            return false;
        $out['unit'] = $decoded['unit'];
    }
    if (array_key_exists('allowedValues', $decoded)) {
        if (!is_array($decoded['allowedValues']) || count($decoded['allowedValues']) < 1
            || count($decoded['allowedValues']) > 100) return false;
        $values = array();
        foreach ($decoded['allowedValues'] as $value) {
            $canonical = hrm_typed_attribute_canonical_value($value_type, $value);
            if ($canonical === false || isset($values[$canonical])) return false;
            $values[$canonical] = $canonical;
        }
        ksort($values, SORT_STRING);
        $out['allowedValues'] = array_values($values);
    }
    ksort($out, SORT_STRING);
    $json = json_encode($out, JSON_UNESCAPED_SLASHES);
    return $json === false || strlen($json) > 4000 ? false : $json;
}

/** Return one exact draft definition/version command or false. */
function hrm_typed_attribute_definition_canonical_command($input)
{
    $allowed = array('attribute_code','value_type','allowed_subject','sensitivity','owner_scope',
        'validation_json','effective_from','effective_to','predecessor_id');
    if (!is_array($input) || array_diff(array_keys($input), $allowed)) return false;
    foreach (array('attribute_code','value_type','allowed_subject','sensitivity','owner_scope','validation_json','effective_from') as $field)
        if (!array_key_exists($field, $input) || !is_string($input[$field])) return false;
    if (array_key_exists('effective_to', $input) && $input['effective_to'] !== null
        && $input['effective_to'] !== '' && !is_string($input['effective_to'])) return false;

    $metadata = array(
        'attribute_code'=>$input['attribute_code'],
        'value_type'=>$input['value_type'],
        'allowed_subject'=>$input['allowed_subject'],
        'sensitivity'=>$input['sensitivity'],
        'owner_scope'=>$input['owner_scope'],
        'validation_json'=>$input['validation_json'],
    );
    if (!hrm_typed_attribute_validate_definition_metadata($metadata)) return false;
    $validation = hrm_typed_attribute_definition_validation_json($input['value_type'], $input['validation_json']);
    $from = hrm_typed_attribute_definition_date($input['effective_from'], false);
    $to = hrm_typed_attribute_definition_date(
        array_key_exists('effective_to', $input) ? $input['effective_to'] : null, true);
    $predecessor = hrm_typed_attribute_definition_positive_id(
        array_key_exists('predecessor_id', $input) ? $input['predecessor_id'] : null, true);
    if ($validation === false || $from === false || $to === false || $predecessor === false
        || ($to !== null && strcmp($to, $from) < 0)) return false;

    return array(
        'attribute_code'=>$input['attribute_code'],
        'value_type'=>$input['value_type'],
        'allowed_subject'=>$input['allowed_subject'],
        'sensitivity'=>$input['sensitivity'],
        'owner_scope'=>$input['owner_scope'],
        'validation_json'=>$validation,
        'definition_status'=>'draft',
        'effective_from'=>$from,
        'effective_to'=>$to,
        'predecessor_id'=>$predecessor,
    );
}

function hrm_typed_attribute_definition_command_digest($command)
{
    if (!is_array($command)) return false;
    $json = json_encode($command, JSON_UNESCAPED_SLASHES);
    return $json === false ? false
        : hash('sha256', 'hrm-fnd-007-typed-attribute-definition-command-v1|'.$json);
}
