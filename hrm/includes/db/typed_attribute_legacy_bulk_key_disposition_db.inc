<?php
/** HRM-FND-007 explicit non-destructive retained-key disposition for bulk legacy conversion. */
if (!function_exists('get_hrm_typed_attribute_legacy_bulk_reconciliation'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_reconciliation_db.inc');

function hrm_typed_attribute_legacy_bulk_key_disposition($reconciliation)
{
    if (!is_array($reconciliation) || !isset($reconciliation['status']) || empty($reconciliation['read_only'])
        || empty($reconciliation['legacy_keys_retained']) || !empty($reconciliation['legacy_custom_data_rewritten'])
        || !empty($reconciliation['plaintext_exposed']))
        return false;
    $status = (string)$reconciliation['status'];
    $blocked = strpos($status, 'blocked_') === 0;
    if ($status === 'reconciled_batch') {
        $authority = 'typed_attributes_authoritative_legacy_batch_reconciliation_copy';
        $disposition = 'retain_batch_legacy_keys_for_reconciliation_and_recovery';
    } elseif ($status === 'pending_batch_completion') {
        $authority = 'legacy_retained_authoritative_until_batch_completion';
        $disposition = 'retain_until_governed_batch_completion';
    } elseif ($blocked) {
        $authority = 'bulk_reconciliation_blocked_no_authority_switch';
        $disposition = 'retain_and_require_manual_review';
    } else {
        return false;
    }
    return array(
        'reconciliation_status'=>$status,
        'authority'=>$authority,
        'disposition'=>$disposition,
        'legacy_keys_retained'=>true,
        'legacy_key_delete_permitted'=>false,
        'legacy_custom_data_rewrite_permitted'=>false,
        'automatic_cleanup_permitted'=>false,
        'bulk_cleanup_permitted'=>false,
        'destructive_lifecycle_activated'=>false,
        'payroll_consumer_permitted'=>false,
        'work_authorization_delivery_permitted'=>false,
    );
}

function get_hrm_typed_attribute_legacy_bulk_key_disposition($batch_id)
{
    $reconciliation = get_hrm_typed_attribute_legacy_bulk_reconciliation($batch_id);
    return is_array($reconciliation) ? hrm_typed_attribute_legacy_bulk_key_disposition($reconciliation) : false;
}
