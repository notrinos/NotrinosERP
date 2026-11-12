<?php
/**
 * DIM-LFC-001 B10: locked current-read recheck of every registered ordinary
 * reference owner. This is the "unused" authority the serialized protocol
 * needs; it is still NOT a deletion certificate. Protected (B04/B05), shared
 * (B06), approval (B03), session (B08) and extension (B09) owners keep their
 * own authority. With B09 every owner family now has an observer, so a fully reviewed,
 * unreferenced deployment aggregates to `unused`; the answer is still never a deletion
 * certificate (`deletion_authorized` stays false until C01 owns the delete lifecycle).
 *
 * B09 replaces the deferred extension owner with dimensions/includes/dimension_extension_authority.inc:
 * installed, inactive and unregistered extension code, the live schema and declared external writers
 * must all be accounted for (reviewed source digests) or the owner is unknown. The registry write path
 * takes the exclusive fence row the recheck holds shared (sys_prefs version_id), and a signature of the
 * deployment is compared before the first lock and after the master lock.
 *
 * B08 replaces the deferred session-draft owner with the durable session draft
 * registration in dimensions/includes/dimension_session_draft_authority.inc: every
 * live registered session needs a complete seal of this company, the live carts of
 * the actor's own request are classified from memory, and a live reference reports
 * used. A missing seal, unknown seal, company mismatch or unreadable table is unknown.
 *
 * B06 assigns the shared stores (attachments, audit trail, comments, references,
 * reference lines, tags and tag associations) to their domain authorities in
 * dimensions/includes/dimension_shared_record_authority.inc: ordinary
 * transaction/master families decode through the canonical projection, protected
 * HRM and payroll families deny a coordinate-like key, and every unassigned,
 * ambiguous or dangling key stays unknown.
 *
 * B04 adds the six HRM scalar owners (attendance, overtime, departments,
 * employees, organization units, assignments) with the same locked current
 * reads. Only numeric coordinates are read; no person or payroll payload is
 * selected or returned. B04 retained HRM evidence (staged encrypted Hire
 * payloads and encrypted Worker profile versions) is classified by the HRM-owned
 * minimized decoder in hrm/includes/db/hrm_dimension_retained_evidence_db.inc;
 * HRM approval families are classified inside the B03 approval observer.
 *
 * B05 replaces the deferred payroll owners with the payroll-owned minimized
 * decoder in hrm/includes/db/pay_dimension_reference_authority_db.inc: PAY-GL
 * posting-line projections, governed allocation axes (independent axes never
 * collapse onto a core Dimension), payslip custom_data, payroll reversal
 * evidence and the PAY-GLB native receipts. ST_PAYROLL_PERIOD approval drafts
 * are classified inside the approval observer by the same authority.
 */
require_once __DIR__.'/dimension_lifecycle_db.inc';
require_once __DIR__.'/dimension_reference_registry.inc';
require_once __DIR__.'/dimension_reference_writer_fence.inc';
require_once __DIR__.'/dimension_master_payload_guard.inc';
require_once __DIR__.'/dimension_shared_record_authority.inc';
require_once __DIR__.'/dimension_session_draft_authority.inc';
require_once __DIR__.'/dimension_extension_authority.inc';

/**
 * Return the ordinary owners the B02 matrix registers for reconciliation:
 * table => exact native scalar Dimension columns. Every owner except the two
 * scalar-only ones also retains ordinary custom_data JSON.
 *
 * @return array
 */
function dimension_reference_recheck_owners()
{
    return array(
        'attendance'=>array('dimension_id','dimension2_id'), 'departments'=>array('cost_center_id'),
        'employees'=>array('cost_center_id','dimension2_id'), 'hrm_assignments'=>array('legacy_cost_center_id','legacy_dimension2_id'),
        'hrm_organization_units'=>array('legacy_cost_center_id'), 'overtime'=>array('dimension_id','dimension2_id'),
        'areas'=>array(), 'bank_accounts'=>array(), 'bank_trans'=>array('dimension_id','dimension2_id'), 'bom'=>array(),
        'budget_trans'=>array('dimension_id','dimension2_id'), 'chart_class'=>array(), 'chart_master'=>array(), 'chart_types'=>array(),
        'debtor_trans'=>array('dimension_id','dimension2_id'), 'debtor_trans_details'=>array(), 'debtors_master'=>array('dimension_id','dimension2_id'),
        'dscsa_transactions'=>array(), 'fsma_tracking_events'=>array(), 'gl_trans'=>array('dimension_id','dimension2_id'), 'grn_batch'=>array(),
        'grn_items'=>array(), 'item_codes'=>array(), 'locations'=>array(), 'prices'=>array(), 'procurement_plan'=>array(), 'procurement_plan_lines'=>array(),
        'purch_agreement_lines'=>array(), 'purch_agreements'=>array('dimension_id','dimension2_id'), 'purch_bill_control_policy'=>array(),
        'purch_data'=>array(), 'purch_matching_config'=>array(), 'purch_matching_exceptions'=>array(), 'purch_order_details'=>array(),
        'purch_order_template_lines'=>array(), 'purch_order_templates'=>array(), 'purch_orders'=>array(), 'purch_requisition_lines'=>array(),
        'purch_requisitions'=>array('dimension_id','dimension2_id'), 'purch_rfq'=>array('dimension_id','dimension2_id'), 'purch_rfq_items'=>array(),
        'purch_rfq_vendor_lines'=>array(), 'purch_rfq_vendors'=>array(), 'quality_inspections'=>array(),
        'quick_entry_lines'=>array('dimension_id','dimension2_id'), 'recall_campaigns'=>array(), 'sales_agreement_lines'=>array(),
        'sales_agreements'=>array('dimension_id','dimension2_id'), 'sales_commission_entries'=>array(), 'sales_commission_plans'=>array(),
        'sales_coupons'=>array(), 'sales_discount_programs'=>array(), 'sales_order_details'=>array(), 'sales_orders'=>array(), 'sales_pos'=>array(),
        'sales_pricelist_rules'=>array(), 'sales_pricelists'=>array(), 'sales_quotation_template_lines'=>array(), 'sales_quotation_templates'=>array(),
        'sales_rma'=>array(), 'sales_rma_lines'=>array(), 'sales_types'=>array(), 'salesman'=>array(), 'serial_numbers'=>array(), 'stock_batches'=>array(),
        'stock_category'=>array('dflt_dim1','dflt_dim2'), 'stock_master'=>array('dimension_id','dimension2_id'), 'stock_moves'=>array(),
        'supp_invoice_items'=>array('dimension_id','dimension2_id'), 'supp_trans'=>array(), 'suppliers'=>array('dimension_id','dimension2_id'),
        'tax_types'=>array(), 'udi_registrations'=>array(), 'vendor_evaluations'=>array(), 'vendor_performance_log'=>array(), 'vendor_pricelists'=>array(),
        'warranty_claims'=>array(), 'warranty_provision_log'=>array('dimension_id','dimension2_id'), 'wh_bin_stock'=>array(),
        'wh_consignment_stock'=>array(), 'wh_cycle_count_plans'=>array(), 'wh_cycle_counts'=>array(), 'wh_locations'=>array(),
        'wh_material_request_lines'=>array(), 'wh_material_requests'=>array(), 'wh_operation_lines'=>array(), 'wh_operations'=>array(),
        'wh_packages'=>array(), 'wh_picking_waves'=>array(), 'wh_putaway_rules'=>array(), 'wh_replenishment_rules'=>array(),
        'wh_return_order_lines'=>array(), 'wh_return_orders'=>array(), 'wh_route_rules'=>array(), 'wh_routes'=>array(), 'wh_scrap'=>array(),
        'wh_storage_categories'=>array(), 'wh_transfer_order_lines'=>array(), 'wh_transfer_orders'=>array(), 'wo_issue_items'=>array(),
        'workorders'=>array());
}

/** Owners whose canonical schema has no retained custom_data column. */
function dimension_reference_recheck_scalar_only()
{
    return array('quick_entry_lines', 'warranty_provision_log', 'attendance', 'departments', 'hrm_assignments',
        'hrm_organization_units', 'overtime');
}

/** Owners that remain unknown here because another requirement owns their authority (none after B09). */
function dimension_reference_recheck_deferred_owners()
{
    return array();
}

/**
 * Read the live active-company columns of all given owners in one query.
 *
 * @param array $tables Unprefixed registered owner tables
 * @return array|false table => column => true; false when the read fails
 */
function dimension_reference_recheck_columns($tables)
{
    $names = array();
    foreach ($tables as $table) $names[] = "'".db_escape_function(TB_PREF.$table)."'";
    $rows = db_query('SELECT TABLE_NAME, COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        .' AND TABLE_NAME IN ('.implode(',', $names).')');
    if (!$rows) return false;
    $columns = array();
    while ($row = db_fetch_row($rows)) $columns[$row[0]][$row[1]] = true;
    // Resolve the active prefix exactly as db_query() substitutes TB_PREF.
    $company = isset($_SESSION['wa_current_user']->cur_con) ? $_SESSION['wa_current_user']->cur_con : 0;
    $prefix = isset($GLOBALS['db_connections'][$company]['tbpref']) ? (string)$GLOBALS['db_connections'][$company]['tbpref'] : '';
    $result = array();
    foreach ($tables as $table) {
        $prefixed = $prefix.$table;
        $result[$table] = isset($columns[$prefixed]) ? $columns[$prefixed] : array();
    }
    return $result;
}

/**
 * A missing registered column, or an unregistered Dimension-like column,
 * makes the owner unknown.
 *
 * @return bool True only when the live schema matches the registration
 */
function dimension_reference_recheck_schema($columns, $scalar, $retained)
{
    foreach ($scalar as $column) if (!isset($columns[$column])) return false;
    if ($retained && !isset($columns['custom_data'])) return false;
    foreach (array_keys($columns) as $column)
        if (preg_match('/(^|_)(dimension[0-9]*|dim[0-9]+)(_id)?$|cost_cent/', $column) && !in_array($column, $scalar, true))
            return false;
    return true;
}

/**
 * Classify one owner with locking reads, which return the latest committed
 * rows (no stale repeatable-read snapshot) and hold shared row and gap locks
 * until the caller's transaction ends, so no writer can add a reference after
 * an "unused" answer. Retained JSON uses the writers' own canonical projection.
 *
 * @return string used, unused or unknown
 */
function dimension_reference_recheck_owner($table, $columns, $scalar, $retained, $id)
{
    if (!dimension_reference_recheck_schema($columns, $scalar, $retained)) return 'unknown';
    if (count($scalar)) {
        $conditions = array();
        foreach ($scalar as $column) $conditions[] = '`'.$column.'`='.$id;
        $rows = db_query('SELECT 1 FROM '.TB_PREF.$table.' WHERE ('.implode(' OR ', $conditions).') LIMIT 1 LOCK IN SHARE MODE');
        if (!$rows) return 'unknown';
        if (db_num_rows($rows)) return 'used';
    }
    if (!$retained) return 'unused';
    $where = $table === 'dimensions' ? ' WHERE id<>'.$id : '';
    $rows = db_query('SELECT custom_data FROM '.TB_PREF.$table.$where.' LOCK IN SHARE MODE');
    if (!$rows) return 'unknown';
    $state = 'unused';
    while ($row = db_fetch_row($rows)) {
        $references = dimension_master_payload_references($row[0]);
        if ($references === false) { $state = 'unknown'; continue; }
        foreach ($references as $reference) if ((int)$reference[0] === $id) return 'used';
    }
    return $state;
}

/**
 * Recheck every ordinary owner for one Dimension inside the caller's open
 * transaction. Locks follow the writer order: owner rows first (tables in
 * fixed sorted order), then the Dimension master FOR UPDATE. Any SQL failure,
 * lock timeout or deadlock stops the scan and leaves the remaining owners
 * unknown; the caller must then roll back. Requires repeatable-read or
 * serializable isolation so the shared gap locks block new references.
 *
 * @param int|string $id Exact positive native Dimension identity
 * @param string[] $tolerate_own Shared owners whose rows bound to the Dimension's own identity the caller retains
 *        (C01 delete keeps refs and audit_trail; reassignment keeps all own evidence). Empty by default.
 * @return array state (used|unknown|unused), owners name=>state; deletion_authorized is always false
 */
function dimension_reference_locked_recheck($id, $tolerate_own = array())
{
    $owners = dimension_reference_recheck_owners();
    $owners['dimensions'] = array();
    ksort($owners);
    $result = array('state'=>'unknown', 'owners'=>array(), 'deletion_authorized'=>false);
    foreach (array_keys($owners) as $name) $result['owners'][$name] = 'unknown';
    foreach (dimension_reference_recheck_deferred_owners() as $name=>$requirement) $result['owners'][$name] = 'unknown';
    $result['owners']['session_drafts'] = 'unknown';
    $result['owners']['extensions'] = 'unknown';
    $hrm_retained = array('hrm_lifecycle_hire_commands', 'hrm_lifecycle_hire_payloads', 'hrm_worker_profile_versions');
    $result['owners']['hrm_lifecycle_hire_payloads'] = 'unknown';
    $result['owners']['hrm_worker_profile_versions'] = 'unknown';
    require_once dirname(dirname(__DIR__)).'/hrm/includes/db/pay_dimension_reference_authority_db.inc';
    foreach (pay_dimension_reference_owner_names() as $name) $result['owners'][$name] = 'unknown';
    foreach (dimension_shared_record_owner_names() as $name) $result['owners'][$name] = 'unknown';
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id) || empty($GLOBALS['transaction_level'])) return $result;
    $id = (int)$id;
    $extension_signature = dimension_extension_signature();
    if ($extension_signature === '') return $result;
    try {
        $level = db_query('SELECT @@SESSION.tx_isolation');
        $level = $level ? db_fetch_row($level) : false;
        if (!$level || !in_array(strtoupper($level[0]), array('REPEATABLE-READ', 'SERIALIZABLE'), true)) return $result;
        $scalar_only = dimension_reference_recheck_scalar_only();
        $columns = dimension_reference_recheck_columns(array_merge(array_keys($owners), $hrm_retained, pay_dimension_reference_tables(),
            dimension_shared_record_tables()));
        if ($columns === false) return $result;
        foreach ($owners as $name=>$scalar) {
            $state = dimension_reference_recheck_owner($name, $columns[$name], $scalar, !in_array($name, $scalar_only, true), $id);
            $result['owners'][$name] = $state;
            if ($state === 'unknown' && db_error_no()) return $result;
        }
        // B03: approval drafts and action snapshots, current locked reads. Protected or
        // unsupported families inside them keep the owner unknown (B04/B05 authority).
        require_once dirname(dirname(__DIR__)).'/includes/approval/approval_workflow.inc';
        $approval = new ApprovalWorkflow((int)$_SESSION['wa_current_user']->access, (int)$_SESSION['wa_current_user']->user);
        foreach ($approval->observeDimensionReferences($id, true) as $name=>$state) {
            $result['owners'][$name] = $state;
            if ($state === 'unknown' && db_error_no()) return $result;
        }
        // B04: HRM-owned minimized decoder for staged Hire payloads and Worker profile versions.
        require_once dirname(dirname(__DIR__)).'/hrm/includes/db/hrm_dimension_retained_evidence_db.inc';
        foreach (hrm_dimension_retained_observe($id, $columns) as $name=>$state) {
            $result['owners'][$name] = $state;
            if ($state === 'unknown' && db_error_no()) return $result;
        }
        // B05: payroll-owned minimized decoder for PAY-GL, payslip, reversal and PAY-GLB evidence.
        foreach (pay_dimension_reference_observe($id, $columns) as $name=>$state) {
            $result['owners'][$name] = $state;
            if ($state === 'unknown' && db_error_no()) return $result;
        }
        // B06: shared attachments, audit, comments, references, reference lines and tags.
        foreach (dimension_shared_record_observe($id, $columns, $tolerate_own) as $name=>$state) {
            $result['owners'][$name] = $state;
            if ($state === 'unknown' && db_error_no()) return $result;
        }
        // B08: durable session draft registration of every live session, and the actor's own live carts.
        foreach (dimension_session_draft_observe($id) as $name=>$state) {
            $result['owners'][$name] = $state;
            if ($state === 'unknown' && db_error_no()) return $result;
        }
        // B09: installed/inactive/unregistered extension code, schema drift and declared external writers.
        foreach (dimension_extension_observe($id) as $name=>$state) {
            $result['owners'][$name] = $state;
            if ($state === 'unknown' && db_error_no()) return $result;
        }
        $master = db_query('SELECT id FROM '.TB_PREF.'dimensions WHERE id='.$id.' FOR UPDATE');
        if (!$master || !db_num_rows($master)) return $result;
        // The deployment must be the one the observers read: a registry write or code drop after the first read is unknown.
        if (dimension_extension_signature() !== $extension_signature) $result['owners']['extensions'] = 'unknown';
    } catch (Exception $exception) {
        return $result;
    }
    $states = array_values($result['owners']);
    $result['state'] = in_array('used', $states, true) ? 'used' : (in_array('unknown', $states, true) ? 'unknown' : 'unused');
    return $result;
}
