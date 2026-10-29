<?php
/** Native Transfer construction and maintenance preserve complete source custody. */
require_once __DIR__.'/warehouse_transfer_line_db.inc';

/** Validate authored fields before SQL can truncate, coerce or normalize them. */
function transfer_header_values($from, $to, $date, $type, $reference, $memo, $ship, $receive)
{
    if (!is_string($type) || !in_array($type, array('standard','urgent','backorder'), true)) return false;
    foreach (array($from, $to) as $code)
        if (!is_string($code) || $code === '' || strpos($code, "\0") !== false) return false;
    foreach (array($date, $ship, $receive) as $index=>$value) {
        if ($index && ($value === null || $value === '')) continue;
        if (!is_string($value) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $value, $parts)
            || (int)$parts[1] < 1000 || !checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1])) return false;
    }
    foreach (array($reference, $memo) as $value)
        if ($value !== null && (!is_string($value) || strpos($value, "\0") !== false)) return false;
    $values = array('from_loc_code'=>$from, 'to_loc_code'=>$to, 'request_date'=>$date, 'transfer_type'=>$type,
        'reference'=>$reference === '' ? null : $reference, 'memo'=>$memo === '' ? null : $memo,
        'expected_ship_date'=>$ship === '' ? null : $ship, 'expected_recv_date'=>$receive === '' ? null : $receive);
    $select = array();
    foreach ($values as $field=>$value) $select[] = ($value === null ? 'NULL' : db_escape($value)).' AS '.$field;
    $rows = transfer_core_rows('SELECT '.implode(',', $select));
    if (!$rows) return false;
    $lengths = transfer_core_rows('SELECT CHAR_LENGTH('.db_escape($rows[0]['from_loc_code']).') AS from_length,'
        .'CHAR_LENGTH('.db_escape($rows[0]['to_loc_code']).') AS to_length,'
        .'CHAR_LENGTH('.db_escape($rows[0]['reference']).') AS reference_length,'
        .'OCTET_LENGTH('.db_escape($rows[0]['memo']).') AS memo_length');
    return $lengths && (int)$lengths[0]['from_length'] <= 5 && (int)$lengths[0]['to_length'] <= 5
        && (int)$lengths[0]['reference_length'] <= 60 && (int)$lengths[0]['memo_length'] <= 65535 ? $rows[0] : false;
}

/** Only the original maker's attributable rejected/cancelled reviews allow edits. */
function transfer_header_history_editable($history, $maker)
{
    if ($history === false) return false;
    foreach ($history['drafts'] as $draft) {
        $workflow = transfer_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
        if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
            || (int)$draft['submitted_by'] !== (int)$maker || !$workflow
            || (int)$workflow[0]['trans_type'] !== ST_WH_TRANSFER_ORDER) return false;
    }
    return true;
}

/** Lock complete current native numbering and raw family review/reservation state. */
function transfer_header_creation_state()
{
    $state = array();
    foreach (array(
        'SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_WH_TRANSFER_ORDER.' ORDER BY id FOR UPDATE',
        'SELECT a.* FROM '.TB_PREF.'approval_actions a INNER JOIN '.TB_PREF.'approval_drafts d ON d.id=a.draft_id WHERE d.trans_type='.ST_WH_TRANSFER_ORDER.' ORDER BY a.id FOR UPDATE',
        'SELECT * FROM '.TB_PREF.'wh_transfer_orders ORDER BY transfer_no FOR UPDATE',
        'SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_TRANSFER_ORDER.' ORDER BY id FOR UPDATE') as $query) {
        $rows = transfer_core_rows($query);
        if ($rows === false) return false;
        $state[] = $rows;
    }
    return $state;
}

/** Current dependencies include exact location spelling and active native maker. */
function transfer_header_dependencies($source, $actor)
{
    $dependencies = transfer_core_dependencies($source);
    if (!$dependencies) return false;
    foreach ($dependencies['locations'] as $code=>$rows)
        if ($rows[0]['loc_code'] !== (string)$code) return false;
    foreach ($source['lines'] as $line)
        if ($dependencies['stock'][$line['stock_id']][0]['stock_id'] !== $line['stock_id']) return false;
    $users = transfer_core_rows('SELECT id,role_id,inactive FROM '.TB_PREF.'users WHERE id='.$actor.' FOR UPDATE');
    if (!$users || $users[0]['inactive'] || (int)$users[0]['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
    $dependencies['maker'] = $users;
    return $dependencies;
}

/**
 * Own empty Draft creation or maker-owned standalone Draft maintenance. Reviews
 * precede source and dependency locks. Unique native numbers arbitrate concurrent
 * creation; SQL/deadlock failures require a fresh whole-command retry. No approval,
 * material conversion or financial authority is granted by constructing a draft.
 */
function write_native_transfer_header($identity, $from, $to, $date, $type, $reference, $memo, $ship, $receive)
{
    $context = transfer_core_context();
    $actor = isset($_SESSION['wa_current_user']->user) ? dimension_writer_reference_id($_SESSION['wa_current_user']->user) : false;
    $creating = $identity === null;
    if (!$context || !$actor || $actor > 32767 || (!$creating && !($identity = dimension_writer_reference_id($identity)))) return false;
    try {
        if (!begin_transaction()) return false;
        $creation = $creating ? transfer_header_creation_state() : null;
        $history = $creating ? null : transfer_core_history($identity);
        $source = $creating ? array('header'=>array(), 'lines'=>array()) : transfer_line_source($identity);
        $values = transfer_header_values($from, $to, $date, $type, $reference, $memo, $ship, $receive);
        if ($creation === false || !$source || !$values
            || (!$creating && !transfer_header_history_editable($history, $actor))) { cancel_transaction(); return false; }
        $expected = $source;
        $expected['header'] = array_merge($source['header'], $values);
        $dependencies = transfer_header_dependencies($expected, $actor);
        if (!$dependencies) { cancel_transaction(); return false; }
        $historical = array(); $number = 0;
        if ($creating) {
            foreach ($creation[2] as $row) {
                $current = dimension_writer_reference_id($row['transfer_no']);
                if (!$current || $current >= 2147483647) { cancel_transaction(); return false; }
                $number = max($number, $current);
            }
            foreach (array($creation[0], $creation[3]) as $index=>$rows) foreach ($rows as $row) {
                $current = dimension_writer_reference_id($row[$index ? 'id' : 'reserved_trans_no']);
                if (!$current || $current >= 2147483647) { cancel_transaction(); return false; }
                $number = max($number, $current);
                if (!$index) {
                    $data = get_approval_workflow_service()->decodeStoredDraftData($row['draft_data']);
                    $id = is_array($data) && isset($data['transfer_id']) ? dimension_writer_reference_id($data['transfer_id']) : false;
                    if (!$id) { cancel_transaction(); return false; }
                    $historical[$id] = true;
                }
            }
            $number++;
        }
        $result = dimension_ordinary_master_write('wh_transfer_orders', $creating ? null : $identity, array(),
            function($ignored) use ($creating, &$identity, &$expected, &$creation, $number, $actor, $historical, $context) {
                if (transfer_core_context() !== $context) return false;
                $now = transfer_core_rows('SELECT NOW() AS now_');
                if (!$now) return false;
                if ($creating) $expected['header'] += array('transfer_no'=>$number, 'status'=>'draft', 'transit_loc_id'=>null,
                    'actual_ship_date'=>null, 'actual_recv_date'=>null, 'requested_by'=>$actor, 'approved_by'=>null,
                    'shipped_by'=>null, 'received_by'=>null, 'approval_date'=>null, 'custom_data'=>null,
                    'created_at'=>$now[0]['now_'], 'updated_at'=>null);
                else $expected['header']['updated_at'] = $now[0]['now_'];
                $sets = array();
                foreach ($expected['header'] as $field=>$value)
                    if ($field !== 'transfer_id') $sets[] = $field.'='.($value === null ? 'NULL' : "'".db_escape_function($value)."'");
                if (!db_query(($creating ? 'INSERT INTO ' : 'UPDATE ').TB_PREF.'wh_transfer_orders SET '.implode(',', $sets)
                    .($creating ? '' : ' WHERE transfer_id='.$identity))) return false;
                if ($creating) {
                    $identity = dimension_writer_reference_id(db_insert_id());
                    if (!$identity || isset($historical[$identity])) return false;
                    $expected['header']['transfer_id'] = $identity;
                }
                // Canonical column order makes fingerprinting independent of the
                // authored field order while still comparing every stored field.
                $rows = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE');
                if (!$rows || array_diff_key($rows[0], $expected['header']) || array_diff_key($expected['header'], $rows[0])) return false;
                $expected['header'] = array_replace($rows[0], $expected['header']);
                if ($creating) { $creation[2][] = $rows[0]; usort($creation[2], function($a, $b) { return (int)$a['transfer_no'] - (int)$b['transfer_no']; }); }
                return $creating ? (db_preserve_insert_id($identity) ? $identity : false) : true;
            }, true);
        $current = $result ? transfer_line_source($identity) : false;
        if (!$current || transfer_core_source_fingerprint($current) !== transfer_core_source_fingerprint($expected)
            || ($creating ? transfer_header_creation_state() !== $creation : transfer_core_history($identity) !== $history)
            || transfer_header_dependencies($expected, $actor) !== $dependencies || transfer_core_context() !== $context
            || !commit_transaction()) { cancel_transaction(); return false; }
        if ($creating && !db_preserve_insert_id($identity)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
