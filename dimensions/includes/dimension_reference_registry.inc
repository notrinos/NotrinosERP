<?php
/**
 * Read-only Dimension reference-owner contract (DIM-FND-003).
 * No page, mutation, role grant or deletion integration. Observations are not a
 * concurrent deletion authorization; DIM-LFC-001 must lock/recheck all writers.
 */

/** Return source-verified scalar owners and opaque evidence requiring its own authority. */
function dimension_reference_owners() {
    $pair = array('dimension_id','dimension2_id');
    $tables = array(
        'attendance'=>$pair, 'bank_trans'=>$pair, 'budget_trans'=>$pair,
        'debtor_trans'=>$pair, 'debtors_master'=>$pair, 'departments'=>array('cost_center_id'),
        'employees'=>array('cost_center_id','dimension2_id'),
        'hrm_organization_units'=>array('legacy_cost_center_id'),
        'hrm_assignments'=>array('legacy_cost_center_id','legacy_dimension2_id'),
        'gl_trans'=>$pair, 'overtime'=>$pair, 'quick_entry_lines'=>$pair,
        'sales_agreements'=>$pair, 'stock_master'=>$pair, 'warranty_provision_log'=>$pair,
        'supp_invoice_items'=>$pair, 'suppliers'=>$pair, 'purch_requisitions'=>$pair,
        'purch_rfq'=>$pair, 'purch_agreements'=>$pair,
        'hrm_pay_gl_001_gl_posting_lines'=>array('core_dimension_id','core_dimension2_id'));
    $owners = array();
    foreach ($tables as $table=>$columns)
        $owners[$table] = array('kind'=>'scalar','table'=>$table,'columns'=>$columns);
    $owners['approval_drafts'] = array('kind'=>'payload','table'=>'approval_drafts','columns'=>array('draft_data'));
    $owners['approval_actions'] = array('kind'=>'payload','table'=>'approval_actions','columns'=>array('draft_data_snapshot'));
    $owners['tags'] = array('kind'=>'domain_adapter','reason'=>'TAG_DIMENSION association and ST_DIMENSION comments/references/attachments');
    $owners['payroll_custody'] = array('kind'=>'domain_adapter','reason'=>'Governed allocation axes, mapping/projection JSON, native-v1 and immutable payroll evidence');
    $owners['retained_payloads'] = array('kind'=>'domain_adapter','reason'=>'Domain audit, saved source/custom_data, serialized carts, session drafts and future source snapshots');
    $owners['extensions'] = array('kind'=>'domain_adapter','reason'=>'Installed and company hooks, inactive extension references, external deployment ownership');
    return $owners;
}

/** Build a SELECT-only scalar existence check from a fixed registered owner. */
function dimension_reference_owner_sql($owner, $id) {
    if (!is_int($id) || $id <= 0) return false;
    $owners = dimension_reference_owners();
    if (!isset($owners[$owner]) || $owners[$owner]['kind'] !== 'scalar') return false;
    $conditions = array();
    foreach ($owners[$owner]['columns'] as $column) $conditions[] = '`'.$column.'`='.$id;
    return 'SELECT 1 FROM '.TB_PREF.$owners[$owner]['table'].' WHERE ('.implode(' OR ',$conditions).') LIMIT 1';
}

/**
 * Inspect decoded ordinary draft intent without disclosing its content.
 * Invalid/opaque/nested serialized payloads remain unknown. Governed payroll
 * schemas require their owner's minimized adapter, never a generic JSON scan.
 */
function dimension_reference_payload_status($payload, $id, $depth = 0) {
    if (!is_int($id) || $id <= 0 || !is_array($payload) || $depth > 64) return 'unknown';
    $unknown = false;
    $keys = array('dimension_id','dimension2_id','dimension','dimension2','cost_center_id',
        'legacy_cost_center_id','legacy_dimension2_id','core_dimension_id','core_dimension2_id');
    foreach ($payload as $key=>$value) {
        if ($key === 'axis_value_key' || $key === 'core_slot_hint'
            || (is_string($value) && (strpos($value,'pay_glb_001_native_approval_schema') !== false
                || strpos($value,'native-result-v2') !== false))) $unknown = true;
        if (in_array($key,$keys,true)) {
            if (!is_int($value) && !(is_string($value) && preg_match('/^(?:0|[1-9][0-9]*)$/D',$value))) {
                $unknown = true;
            } elseif ((string)$value === (string)$id) return 'used';
        } elseif (is_array($value)) {
            $status = dimension_reference_payload_status($value,$id,$depth+1);
            if ($status === 'used') return 'used';
            if ($status === 'unknown') $unknown = true;
        } elseif (is_string($value) && preg_match('/^(?:\s*[\{\[]|[aOs]:[0-9]+:)/',$value)) {
            $unknown = true; // Never unserialize or guess opaque embedded state.
        }
    }
    return $unknown ? 'unknown' : 'unused';
}

/**
 * Aggregate trusted owner observations; failures and missing adapters fail closed.
 * Caller must authorize before supplying any reader. This performs no DB access
 * itself, returns owner names/status only and can never authorize a deletion.
 */
function dimension_reference_observe($id, $reader, $additional_owners = array()) {
    $result = array('state'=>'unknown','used_owners'=>array(),'unknown_owners'=>array(), 'deletion_authorized'=>false);
    if (!is_int($id) || $id <= 0 || !is_callable($reader)) return $result;
    foreach (dimension_reference_owners() as $name=>$owner) {
        $status = 'unknown';
        try { $status = call_user_func($reader,$name,$owner,$id); }
        catch (Exception $exception) { $status = 'unknown'; }
        if ($status === 'used') $result['used_owners'][] = $name;
        elseif ($status !== 'unused') $result['unknown_owners'][] = $name;
    }
    foreach ($additional_owners as $owner) $result['unknown_owners'][] = (string)$owner;
    $result['state'] = count($result['used_owners']) ? 'used' : (count($result['unknown_owners']) ? 'unknown' : 'unused');
    return $result;
}
