<?php
/**
 * HRM-FND-007 explicit retained legacy-key disposition policy.
 *
 * FND-007 never deletes or rewrites employees.custom_data. After a successful
 * conversion the governed Typed Attribute becomes the normalized authority,
 * while the legacy key remains a reconciliation/recovery copy until a future,
 * separately approved destructive lifecycle defines evidence and recovery.
 */
if (!function_exists('get_hrm_typed_attribute_legacy_conversion_reconciliation'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_reconciliation_db.inc');

/**
 * Classify one redacted reconciliation result into an explicit key disposition.
 *
 * @param array $reconciliation
 * @return array|false
 */
function hrm_typed_attribute_legacy_key_disposition($reconciliation)
{
    if (!is_array($reconciliation) || !isset($reconciliation['status'])
        || empty($reconciliation['read_only']) || empty($reconciliation['legacy_key_retained'])
        || !empty($reconciliation['legacy_custom_data_rewritten']) || !empty($reconciliation['plaintext_exposed']))
        return false;
    $status = (string)$reconciliation['status'];
    $blocked = strpos($status, 'blocked_') === 0;
    if ($status !== 'pending_conversion' && $status !== 'reconciled' && !$blocked)
        return false;
    if ($status === 'reconciled') {
        $authority = 'typed_attribute_authoritative_legacy_reconciliation_copy';
        $action = 'retain_for_reconciliation_and_recovery';
    } elseif ($status === 'pending_conversion') {
        $authority = 'legacy_retained_authoritative_until_conversion';
        $action = 'retain_until_governed_conversion';
    } else {
        $authority = 'reconciliation_blocked_no_authority_switch';
        $action = 'retain_and_require_manual_review';
    }
    return array(
        'reconciliation_status'=>$status,
        'authority'=>$authority,
        'disposition'=>$action,
        'legacy_key_retained'=>true,
        'legacy_key_delete_permitted'=>false,
        'legacy_custom_data_rewrite_permitted'=>false,
        'automatic_cleanup_permitted'=>false,
        'bulk_cleanup_permitted'=>false,
        'payroll_consumer_permitted'=>false,
        'work_authorization_delivery_permitted'=>false,
    );
}

/**
 * Resolve one conversion status and return its explicit retained-key policy.
 *
 * @param array $input employee_number + mapping_version_id only.
 * @return array|false
 */
function get_hrm_typed_attribute_legacy_key_disposition($input)
{
    $reconciliation = get_hrm_typed_attribute_legacy_conversion_reconciliation($input);
    return is_array($reconciliation) ? hrm_typed_attribute_legacy_key_disposition($reconciliation) : false;
}
