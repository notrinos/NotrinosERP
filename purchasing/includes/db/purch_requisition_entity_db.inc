<?php
/** Canonical generic requisition persistence delegates to native paired commands. */
require_once __DIR__.'/purch_requisition_line_db.inc';

/**
 * Merge partial Entity data only after current native owner locks. Derived and
 * workflow columns may be echoed unchanged, never assigned by generic CRUD.
 * Exact generated identities, paired Warehouse rows and aggregates remain owned
 * by the existing commands. This grants no approval or Dimension deletion authority.
 */
function write_purch_requisition_entity($owner, $operation, $key, $next, $validate)
{
    global $path_to_root;
    $context = purch_requisition_header_context();
    if ($context === false || !is_callable($validate) || !is_array($next)
        || !in_array($owner, array('purch_requisitions','purch_requisition_lines'), true)
        || !in_array($operation, array('insert','update','delete'), true)) return false;
    // Generic callers need the same native serializers and Warehouse helpers.
    if (!function_exists('encode_purch_requisition_custom_data')) require_once __DIR__.'/purch_requisition_db.inc';
    $header = $owner === 'purch_requisitions';
    // Existing Warehouse sources belong to the complete native bridge command.
    // Generic insert must not bypass its draft/current-policy admission.
    if ($header && $operation === 'insert' && array_key_exists('material_request_id', $next)
        && dimension_writer_reference_id($next['material_request_id']) !== 0) return false;
    $identity = $operation === 'insert' ? null : dimension_writer_reference_id($key);
    if ($identity === false || $identity === 0 || ($operation !== 'delete' && !$next)
        || ($identity === null && array_key_exists('id', $next))) return false;
    try {
        if (!begin_transaction()) return false;
        $parent = $header ? $identity : (isset($next['requisition_id']) ? dimension_writer_reference_id($next['requisition_id']) : false);
        if (!$header && $identity !== null) {
            $hint = purch_requisition_header_rows('SELECT requisition_id FROM '.TB_PREF.$owner.' WHERE id='.$identity);
            $parent = $hint ? dimension_writer_reference_id($hint[0]['requisition_id']) : false;
        }
        if (!$header && !$parent) { cancel_transaction(); return false; }
        // Native header uses owner-first; native lines use parent/master first.
        $execute = function($ignored = null) use ($header, $owner, $operation, $identity, $parent, $next, $validate, $context) {
            $old = array();
            if (!$header) {
                $parents = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$parent.' FOR UPDATE');
                if (!$parents) return false;
                $source = dimension_writer_reference_id($parents[0]['material_request_id']);
                if (!$source || !purch_requisition_header_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE')) return false;
            }
            if ($identity !== null) {
                $rows = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.$owner.' WHERE id='.$identity.' FOR UPDATE');
                if (!$rows || (!$header && (int)$rows[0]['requisition_id'] !== $parent)) return false;
                $old = $rows[0];
            }
            $defaults = $header
                ? array('requester_id'=>0,'department_id'=>0,'request_date'=>null,'required_date'=>null,'priority'=>'normal','purpose'=>'','notes'=>'','location'=>'','reference'=>'','dimension_id'=>0,'dimension2_id'=>0,'material_request_id'=>0,'custom_data'=>null)
                : array('requisition_id'=>$parent,'stock_id'=>'','description'=>'','quantity'=>0,'unit_of_measure'=>'','estimated_unit_price'=>0,'preferred_supplier_id'=>0,'notes'=>'','custom_data'=>null);
            $editable = array_keys($defaults);
            if ($identity !== null) $editable = array_diff($editable, $header ? array('requester_id','material_request_id') : array('requisition_id'));
            foreach ($next as $field=>$value) {
                if (in_array($field, $editable, true)) continue;
                if (!array_key_exists($field, $old) || ($value === null ? $old[$field] !== null : (string)$value !== (string)$old[$field])) return false;
            }
            if (!$validate() || purch_requisition_header_context() !== $context) return false;
            // A validator is extensible code, not authority to change a locked owner.
            if ($identity !== null && purch_requisition_header_rows('SELECT * FROM '.TB_PREF.$owner.' WHERE id='.$identity.' FOR UPDATE') !== array($old)) return false;
            $data = $next + $old + $defaults;
            $native = $operation === 'insert' ? 'add' : $operation;
            if ($header) return write_purch_requisition_header($native, $identity, $data);
            return write_purch_requisition_line($native, $identity === null ? $parent : $identity, $data);
        };
        $result = $header ? $execute() : dimension_ordinary_master_write('purch_requisitions', $parent, array(), $execute, true);
        if (!$result || purch_requisition_header_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        if ($identity === null && !db_preserve_insert_id($result)) return false;
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
