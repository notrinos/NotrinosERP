<?php
/** Atomic native requisition header commands; no approval or deletion certificate. */
require_once __DIR__.'/../../../dimensions/includes/dimension_reference_writer_fence.inc';

/** Bind an operation to its actual actor, selected connection and DB handle. */
function purch_requisition_header_context()
{
    global $db_connections, $db;
    $actor = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
    if (!is_object($actor) || empty($actor->user) || !isset($actor->cur_con)
        || !isset($db_connections[$actor->cur_con]) || !method_exists($actor, 'can_access')
        || !$actor->can_access('SA_PURCHREQUISITION')) return false;
    return hash('sha256', serialize(array($actor->user, $actor->cur_con,
        $db_connections[$actor->cur_con], is_object($db) ? spl_object_hash($db) : $db)));
}

/** Read current native rows without joined snapshots or fatal optional diagnostics. */
function purch_requisition_header_rows($sql)
{
    $result = db_query($sql);
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch($result)) $rows[] = $row;
    return $rows;
}

/** Preserve serialized bytes using the same raw SQL literal pattern as RFQs. */
function sql_purch_requisition_header_payload($value)
{
    return $value === null ? 'NULL' : "'".db_escape_function($value)."'";
}

/** Admit header dates and identifiers before any linked source is constructed. */
function purch_requisition_header_values_valid($data)
{
    foreach (array('department_id','dimension_id','dimension2_id') as $field)
        if (!array_key_exists($field, $data) || dimension_writer_reference_id($data[$field]) === false) return false;
    if (!isset($data['priority']) || !in_array($data['priority'], array('low','normal','high','urgent'), true)) return false;
    foreach (array('request_date','required_date') as $field) {
        $value = isset($data[$field]) ? $data[$field] : null;
        if ($field === 'required_date' && ($value === null || $value === '')) continue;
        if (!is_string($value) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $value, $match)
            || !checkdate((int)$match[2], (int)$match[3], (int)$match[1])) return false;
    }
    return true;
}

/**
 * Own native header, reference and Warehouse effects in one transaction. An
 * omitted update payload keeps current bytes. Existing scalar references may
 * remain closed/orphaned; retained paths cannot be removed or reassigned.
 * Native approval/status, arbitrary Entity and other Warehouse writers remain
 * separate B02/B03/B10 gates. This command never approves or reopens a source.
 */
function write_purch_requisition_header($operation, $key, $data)
{
    $context = purch_requisition_header_context();
    if ($context === false || !in_array($operation, array('add','update','delete'), true)) return false;
    $identity = $operation === 'add' ? null : dimension_writer_reference_id($key);
    if ($identity === false || $identity === 0) return false;
    try {
        if (!begin_transaction()) return false;
        $old = null;
        if ($identity !== null) {
            $rows = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
            if (!$rows || !in_array($rows[0]['status'], $operation === 'delete' ? array('draft') : array('draft','rejected'), true)) {
                cancel_transaction(); return false;
            }
            $old = $rows[0];
        }
        if ($operation !== 'delete') {
            if ($operation === 'add') {
                if (dimension_writer_reference_id($data['requester_id']) === false
                    || dimension_writer_reference_id($data['material_request_id']) === false) { cancel_transaction(); return false; }
                $data['requester_id'] = $data['requester_id'] ? $data['requester_id'] : (int)$_SESSION['wa_current_user']->user;
                $data['request_date'] = $data['request_date'] ? $data['request_date'] : date2sql(Today());
            } else {
                $data['id'] = $identity;
                $data['request_date'] = $data['request_date'] ? $data['request_date'] : $old['request_date'];
                $data['reference'] = $data['reference'] !== '' ? $data['reference'] : $old['reference'];
                if ($data['custom_data'] === null) $data['custom_data'] = $old['custom_data'];
                // A disabled/omitted page selector is not an explicit clear.
                // Direct callers retain the native explicit zero-clear contract.
                foreach (array('dimension_id','dimension2_id') as $field)
                    if ($data[$field] === null) $data[$field] = $old[$field];
            }
            hook_invoke_all('purch_requisition_before_save', $data);
            if (purch_requisition_header_context() !== $context || !purch_requisition_header_values_valid($data)
                || ($identity !== null && (!isset($data['id']) || dimension_writer_reference_id($data['id']) !== $identity))) {
                cancel_transaction(); return false;
            }
            if ($old) {
                $current = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
                if (!$current || $current[0] !== $old) { cancel_transaction(); return false; }
            }
            if (!isset($data['required_date']) || $data['required_date'] === '') $data['required_date'] = null;
            $data['department_id'] = dimension_writer_reference_id($data['department_id']);
            $encoded = encode_purch_requisition_custom_data($data['custom_data']);
            if ($encoded === null && !in_array($data['custom_data'], array(null,'',array()), true)) { cancel_transaction(); return false; }
            $data['custom_data'] = $encoded;
        }
        $next = $operation === 'delete' ? array('dimension_id'=>0,'dimension2_id'=>0)
            : array('dimension_id'=>$data['dimension_id'],'dimension2_id'=>$data['dimension2_id'],'custom_data'=>$data['custom_data']);
        $result = dimension_ordinary_master_write('purch_requisitions', $identity, $next,
            function($admitted) use ($operation, $identity, $data, $old, $context) {
                if (purch_requisition_header_context() !== $context) return false;
                if ($operation !== 'delete') {
                    $data['dimension_id'] = $admitted['dimension_id'];
                    $data['dimension2_id'] = $admitted['dimension2_id'];
                }
                $source = $old ? dimension_writer_reference_id($old['material_request_id']) : dimension_writer_reference_id($data['material_request_id']);
                if ($source === false) return false;
                if (!$old && $source && !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST')) return false;
                if ($source) {
                    $rows = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE');
                    if (!$rows || $rows[0]['request_type'] !== 'purchase'
                        || !in_array($rows[0]['status'], $old ? array('draft','rejected') : array('draft','submitted','approved','ordered','fulfilled'), true)
                        || ($old && (!empty($rows[0]['linked_doc_type']) || !empty($rows[0]['linked_doc_no'])))) return false;
                    if ($operation === 'delete' && dimension_master_payload_transition($rows[0]['custom_data'], null, true) === false) return false;
                    $links = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$source.' ORDER BY id FOR UPDATE');
                    if ($links === false || count($links) !== ($old ? 1 : 0)
                        || ($old && (int)$links[0]['id'] !== $identity)) return false;
                } elseif ($old) return false; // Never invent a replacement for a broken historical bridge.
                if ($operation === 'delete') {
                    $children = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
                    $rfqs = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_rfq WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
                    $material = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$source.' ORDER BY line_id FOR UPDATE');
                    if ($children === false || $rfqs === false || $rfqs || $material === false) return false;
                    $expected = array();
                    foreach ($children as $child) {
                        if ((float)$child['qty_ordered'] != 0 || (int)$child['po_number'] != 0
                            || !in_array($child['status'], array('pending','rejected'), true)
                            || dimension_master_payload_transition($child['custom_data'], null, true) === false) return false;
                        $line = dimension_writer_reference_id($child['material_request_line_id']);
                        if (!$line || isset($expected[$line])) return false;
                        $expected[$line] = $child;
                    }
                    if (count($expected) !== count($material)) return false;
                    foreach ($material as $line) {
                        if (!isset($expected[(int)$line['line_id']]) || (float)$line['qty_fulfilled'] != 0
                            || dimension_master_payload_transition($line['custom_data'], null, true) === false) return false;
                        $links = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id='.(int)$line['line_id'].' ORDER BY id FOR UPDATE');
                        if (!$links || count($links) !== 1 || (int)$links[0]['id'] !== (int)$expected[(int)$line['line_id']]['id']) return false;
                    }
                    foreach (array('purch_requisition_lines'=>'requisition_id','wh_material_request_lines'=>'request_id',
                        'purch_requisitions'=>'id','wh_material_requests'=>'request_id') as $table=>$column)
                        if (!db_query('DELETE FROM '.TB_PREF.$table.' WHERE '.$column.'='.($table === 'wh_material_request_lines' || $table === 'wh_material_requests' ? $source : $identity))) return false;
                    return true;
                }
                $location = purch_requisition_header_rows('SELECT loc_code FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($data['location']).' AND inactive=0 FOR UPDATE');
                if (!$location) return false;
                if ((int)$data['department_id'] && !purch_requisition_header_rows('SELECT department_id FROM '.TB_PREF.'departments WHERE department_id='.(int)$data['department_id'].' FOR UPDATE')) return false;
                if (!$old) {
                    if (dimension_writer_reference_id($data['requester_id']) === false || !$data['requester_id']
                        || !purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'users WHERE id='.(int)$data['requester_id'].' AND inactive=0 FOR UPDATE')) return false;
                    $construction = null;
                    if (!$source) {
                        // Paired construction retains requisition authority and the owning transaction.
                        require_once __DIR__.'/../../../inventory/warehouse/includes/db/warehouse_material_create_db.inc';
                        $source = create_material_request_header('purchase', $data['location'], $data['request_date'], $data['required_date'], $data['reference'], $data['notes'], true);
                        if (!$source || db_error_no()) return false;
                        $created = purch_requisition_header_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE request_id='.(int)$source.' FOR UPDATE');
                        if (!$created) return false;
                        $construction = array();
                        foreach (array('wh_material_requests'=>'request_id='.(int)$source,
                            'approval_drafts'=>'trans_type='.ST_WH_MATERIAL_REQUEST,
                            'approval_actions'=>'draft_id IN (SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_WH_MATERIAL_REQUEST.')',
                            'voided'=>'type='.ST_WH_MATERIAL_REQUEST) as $table=>$where) {
                            $rows = material_core_rows('SELECT * FROM '.TB_PREF.$table.' WHERE '.$where.' ORDER BY 1 FOR UPDATE');
                            if ($rows === false) return false;
                            $construction[$table] = array('where'=>$where,'rows'=>$rows);
                        }
                    }
                    $data['material_request_id'] = $source;
                    $data['status'] = 'draft';
                    $data['total_estimated'] = 0;
                    $data['dimension_id'] = $admitted['dimension_id'];
                    $data['dimension2_id'] = $admitted['dimension2_id'];
                    $columns = array('reference','material_request_id','requester_id','department_id','request_date','required_date','status','priority','purpose','notes','total_estimated','location','dimension_id','dimension2_id','custom_data');
                    $values = array();
                    foreach ($columns as $field) $values[] = $field === 'custom_data' ? sql_purch_requisition_header_payload($data[$field]) : ($data[$field] === null ? 'NULL' : db_escape($data[$field]));
                    if (!db_query('INSERT INTO '.TB_PREF.'purch_requisitions ('.implode(',', $columns).') VALUES ('.implode(',', $values).')')) return false;
                    $id = db_insert_id();
                    $reference = $data['reference'] !== '' ? $data['reference'] : format_purch_requisition_reference($id);
                    if (!db_query('UPDATE '.TB_PREF.'purch_requisitions SET reference='.db_escape($reference).' WHERE id='.(int)$id)) return false;
                    if ($construction !== null) {
                        foreach ($construction as $table=>$saved)
                            if (material_core_rows('SELECT * FROM '.TB_PREF.$table.' WHERE '.$saved['where'].' ORDER BY 1 FOR UPDATE') !== $saved['rows']) return false;
                        $pairs = material_core_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.(int)$source.' ORDER BY id FOR UPDATE');
                        $children = material_core_rows('SELECT line_id FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.(int)$source.' ORDER BY line_id FOR UPDATE');
                        $native_children = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.(int)$id.' ORDER BY id FOR UPDATE');
                        if (!$pairs || count($pairs) !== 1 || (int)$pairs[0]['id'] !== (int)$id || $children === false || $children
                            || $native_children === false || $native_children) return false;
                        $expected_columns = array();
                        foreach ($columns as $index=>$field)
                            $expected_columns[] = ($field === 'reference' ? db_escape($reference) : $values[$index]).' AS '.$field;
                        $expected_parent = material_core_rows('SELECT '.implode(',', $expected_columns));
                        if (!$expected_parent) return false;
                        $expected_parent[0] += array('approved_by'=>0,'approved_date'=>null,'rejected_by'=>0,'rejected_date'=>null,'rejection_reason'=>null);
                        foreach ($expected_parent[0] as $field=>$value)
                            if (($value === null) !== ($pairs[0][$field] === null) || (string)$value !== (string)$pairs[0][$field]) return false;
                    }
                    if (!db_preserve_insert_id($id)) return false;
                    return $id;
                }
                $sets = array();
                foreach (array('department_id','request_date','required_date','priority','purpose','notes','location','reference','dimension_id','dimension2_id','custom_data') as $field)
                    $sets[] = $field.'='.($field === 'custom_data' ? sql_purch_requisition_header_payload($data[$field]) : ($data[$field] === null ? 'NULL' : db_escape($data[$field])));
                if (!db_query('UPDATE '.TB_PREF.'purch_requisitions SET '.implode(',', $sets).' WHERE id='.$identity)) return false;
                if (!db_query('UPDATE '.TB_PREF.'wh_material_requests SET warehouse_loc_code='.db_escape($data['location'])
                    .', request_date='.db_escape($data['request_date']).', required_date='.($data['required_date'] ? db_escape($data['required_date']) : 'NULL')
                    .', reference='.db_escape($data['reference']).', memo='.db_escape($data['notes']).', updated_at=NOW() WHERE request_id='.$source)) return false;
                return db_query('UPDATE '.TB_PREF.'wh_material_request_lines SET required_date='.($data['required_date'] ? db_escape($data['required_date']) : 'NULL').' WHERE request_id='.$source);
            }, false, $operation === 'delete');
        if (!$result || purch_requisition_header_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
