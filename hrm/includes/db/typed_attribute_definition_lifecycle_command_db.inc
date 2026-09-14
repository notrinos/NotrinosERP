<?php
/**
 * HRM-FND-007 Typed Attribute immutable definition lifecycle command custody.
 *
 * Lifecycle transitions never rewrite definition metadata or attribute values.
 * Deprecated definitions remain readable but are unavailable to new value
 * writes. Retired definitions are unavailable to current reads and writes.
 * A deprecated definition may be explicitly reactivated; retirement is final.
 */
if (!function_exists('hrm_typed_attribute_definition_positive_id'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_definition_command_db.inc');

function hrm_typed_attribute_definition_lifecycle_user_can_manage()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_MANAGE_TYPED_ATTRIBUTE_DEFINITION_LIFECYCLE');
}

function hrm_typed_attribute_definition_lifecycle_user_can_approve()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_APPROVE_TYPED_ATTRIBUTE_DEFINITION_LIFECYCLE');
}

function hrm_typed_attribute_definition_lifecycle_actions()
{
    return array('deprecate','retire','reactivate');
}

function hrm_typed_attribute_definition_lifecycle_pending_status($action)
{
    $map=array(
        'deprecate'=>'deprecating',
        'retire'=>'retiring',
        'reactivate'=>'reactivating',
    );
    return isset($map[$action]) ? $map[$action] : false;
}

function hrm_typed_attribute_definition_lifecycle_final_status($action)
{
    $map=array('deprecate'=>'deprecated','retire'=>'retired','reactivate'=>'active');
    return isset($map[$action]) ? $map[$action] : false;
}

function hrm_typed_attribute_definition_lifecycle_action_from_pending_status($status)
{
    $map=array(
        'deprecating'=>'deprecate',
        'retiring'=>'retire',
        'reactivating'=>'reactivate',
    );
    return isset($map[$status]) ? $map[$status] : false;
}

/** Canonicalize one lifecycle request. No free-text reason or metadata mutation is accepted. */
function hrm_typed_attribute_definition_lifecycle_canonical_command($input)
{
    $allowed=array('definition_version_id','action','effective_from');
    if (!is_array($input) || array_diff(array_keys($input),$allowed)
        || !array_key_exists('definition_version_id',$input)
        || !array_key_exists('action',$input) || !is_string($input['action'])
        || !array_key_exists('effective_from',$input) || !is_string($input['effective_from'])) return false;
    $id=hrm_typed_attribute_definition_positive_id($input['definition_version_id'],false);
    $date=hrm_typed_attribute_definition_date($input['effective_from'],false);
    if ($id===false || $date===false || !in_array($input['action'],hrm_typed_attribute_definition_lifecycle_actions(),true))
        return false;
    return array('definition_version_id'=>$id,'action'=>$input['action'],'effective_from'=>$date);
}

function hrm_typed_attribute_definition_lifecycle_command_digest($command)
{
    if (!is_array($command)) return false;
    $json=json_encode($command,JSON_UNESCAPED_SLASHES);
    return $json===false ? false : hash('sha256','hrm-fnd-007-typed-attribute-definition-lifecycle-command-v1|'.$json);
}
