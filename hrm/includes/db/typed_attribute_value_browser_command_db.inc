<?php
/** HRM-FND-007 browser-safe single-subject Typed Attribute value command adapter. */
if (!function_exists('hrm_typed_attribute_value_current_leaf_id'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_projection_db.inc');

/**
 * Append one immutable value using server-owned predecessor custody.
 * Browser callers cannot provide predecessor_id, source, effective_to or definition metadata.
 */
function hrm_typed_attribute_value_append_current_version($input)
{
    $allowed = array('attribute_code','subject_type','subject_id','value','effective_from');
    if (!is_array($input) || array_diff(array_keys($input), $allowed)
        || array_diff($allowed, array_keys($input))) return false;
    $subject_type = isset($input['subject_type']) && is_string($input['subject_type'])
        && in_array($input['subject_type'], hrm_typed_attribute_subject_types(), true)
        ? $input['subject_type'] : false;
    $subject_id = hrm_typed_attribute_value_positive_id($input['subject_id'], false);
    if ($subject_type === false || $subject_id === false
        || !hrm_typed_attribute_value_subject_exists($subject_type, $subject_id, false)) return false;
    $definition = hrm_typed_attribute_value_resolve_definition($input['attribute_code'], $input['effective_from'], false);
    if (!is_array($definition) || (string)$definition['allowed_subject'] !== $subject_type
        || !hrm_typed_attribute_value_user_can_manage((string)$definition['sensitivity'])
        || !hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope'])) return false;
    $predecessor_id = hrm_typed_attribute_value_current_leaf_id(
        (int)$definition['definition_id'], $subject_type, $subject_id);
    if ($predecessor_id === false) return false;
    return hrm_typed_attribute_value_append_version(array(
        'attribute_code'=>(string)$input['attribute_code'],
        'subject_type'=>$subject_type,
        'subject_id'=>$subject_id,
        'value'=>$input['value'],
        'effective_from'=>(string)$input['effective_from'],
        'effective_to'=>null,
        'predecessor_id'=>$predecessor_id,
        'source'=>'manual',
    ));
}
