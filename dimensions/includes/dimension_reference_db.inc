<?php
/** Authorized observations through fixed source-owned readers (DIM-LFC-001B3). */
require_once __DIR__.'/dimension_reference_registry.inc';
require_once __DIR__.'/dimension_reference_metadata.inc';
require_once __DIR__.'/dimension_reference_schema.inc';
require_once dirname(dirname(__DIR__)).'/hrm/includes/db/pay_gl_001_dimension_references_db.inc';
require_once dirname(dirname(__DIR__)).'/sales/includes/db/sales_dimension_references_db.inc';
require_once dirname(dirname(__DIR__)).'/purchasing/includes/db/purch_dimension_references_db.inc';
require_once dirname(dirname(__DIR__)).'/inventory/includes/db/inventory_dimension_references_db.inc';

/**
 * Observe scalar, metadata, approval and bounded retained owners in the active company.
 * Deny before any reader/query; never accept caller attestations or cache across
 * requests/companies. Payroll allocations and saved Sales/Purchasing/Inventory intent add
 * positive presence only. Absence across protected/retained/session/extension
 * domains stays unknown until complete owner authority and writer fences exist.
 * This is an advisory snapshot, never an unused certificate or delete authority.
 *
 * @param int|string $id Exact positive native identity
 * @return array Owner names/states only; deletion_authorized is always false
 */
function dimension_reference_db_observe($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id))
        return dimension_reference_observe(0, null);
    $approval = array('approval_drafts'=>'unknown', 'approval_actions'=>'unknown');
    if (function_exists('get_approval_workflow_service')) {
        $service = get_approval_workflow_service();
        if (is_object($service) && method_exists($service, 'observeDimensionReferences'))
            $approval = $service->observeDimensionReferences($id);
    }
    $metadata = dimension_reference_metadata_observe($id);
    $payroll = pay_gl_001_observe_dimension_references($id);
    $sales = sales_observe_dimension_references($id);
    $purchasing = purchasing_observe_dimension_references($id);
    $inventory = inventory_observe_dimension_references($id);
    return dimension_reference_observe((int)$id, function($name, $owner, $identity) use ($approval, $metadata, $payroll, $sales, $purchasing, $inventory) {
        if ($owner['kind'] === 'scalar') {
            if (!dimension_reference_schema_available($owner['table'])) return 'unknown';
            $sql = dimension_reference_owner_sql($name, $identity);
            if ($sql === false) return 'unknown';
            $rows = db_query($sql);
            return $rows ? (db_num_rows($rows) ? 'used' : 'unused') : 'unknown';
        }
        if (isset($approval[$name])) return $approval[$name];
        if ($name === 'tags') return $metadata['state'];
        if ($name === 'payroll_custody') return $payroll;
        if ($name === 'retained_payloads') return $sales === 'used' || $purchasing === 'used' || $inventory === 'used' ? 'used' : 'unknown';
        return 'unknown';
    });
}
