<?php
/** Native standalone draft header maintenance retains source and core custody. */
require_once __DIR__.'/warehouse_material_line_db.inc';

/** Validate native header inputs and compare their actual escaped DB values. */
function material_header_values($type, $location, $date, $required, $reference, $memo)
{
    if (!is_string($type) || !array_key_exists($type, get_material_request_types())
        || !is_string($location) || $location === '' || strpos($location, "\0") !== false) return false;
    foreach (array($date, $required) as $index=>$value) {
        if ($index === 1 && ($value === null || $value === '')) continue;
        if (!is_string($value) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $value, $parts)
            || (int)$parts[1] < 1000 || !checkdate((int)$parts[2], (int)$parts[3], (int)$parts[1])) return false;
    }
    foreach (array($reference, $memo) as $value)
        if ($value !== null && (!is_string($value) || strpos($value, "\0") !== false)) return false;
    $values = array('request_type'=>$type, 'warehouse_loc_code'=>$location, 'request_date'=>$date,
        'required_date'=>$required === '' ? null : $required,
        'reference'=>$reference === '' ? null : $reference, 'memo'=>$memo === '' ? null : $memo);
    $select = array();
    foreach ($values as $field=>$value) $select[] = ($value === null ? 'NULL' : db_escape($value)).' AS '.$field;
    $rows = material_core_rows('SELECT '.implode(',', $select));
    if (!$rows) return false;
    $lengths = material_core_rows('SELECT CHAR_LENGTH('.db_escape($rows[0]['warehouse_loc_code']).') AS location_length,'
        .'CHAR_LENGTH('.db_escape($rows[0]['reference']).') AS reference_length,'
        .'OCTET_LENGTH('.db_escape($rows[0]['memo']).') AS memo_length');
    return $lengths && (int)$lengths[0]['location_length'] <= 5 && (int)$lengths[0]['reference_length'] <= 60
        && (int)$lengths[0]['memo_length'] <= 65535 ? $rows[0] : false;
}

/**
 * Maintain only an original maker's unconsumed standalone Draft. Lock retained
 * reviews before current source, then join the ordinary classification fence.
 * Paired requisitions continue through their existing authoritative command.
 */
function write_standalone_material_header($request_id, $type, $location, $date, $required, $reference, $memo)
{
    $context = material_core_context(); $identity = dimension_writer_reference_id($request_id);
    if (!$context || !$identity) return false;
    try {
        if (!begin_transaction()) return false;
        $history = material_core_history($identity);
        $source = material_line_source($identity);
        $values = material_header_values($type, $location, $date, $required, $reference, $memo);
        if ($history === false || !$source || !$values) { cancel_transaction(); return false; }
        foreach ($history['drafts'] as $draft)
            if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)
                || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']) { cancel_transaction(); return false; }
        $locations = material_core_rows('SELECT loc_code,inactive FROM '.TB_PREF.'locations WHERE loc_code='
            .db_escape($values['warehouse_loc_code']).' FOR UPDATE');
        if (!$locations || $locations[0]['loc_code'] !== $values['warehouse_loc_code']
            || ($locations[0]['inactive'] && $source['header']['warehouse_loc_code'] !== $values['warehouse_loc_code'])) {
            cancel_transaction(); return false;
        }
        $result = dimension_ordinary_master_write('wh_material_requests', $identity, array(),
            function($ignored) use ($identity, $values, $source, $history, $context) {
                if (material_core_context() !== $context) return false;
                $now = material_core_rows('SELECT NOW() AS updated_at');
                if (!$now) return false;
                $expected = $source;
                $expected['header'] = array_merge($source['header'], $values, $now[0]);
                $sets = array();
                foreach (array_merge($values, $now[0]) as $field=>$value)
                    $sets[] = $field.'='.($value === null ? 'NULL' : db_escape($value));
                if (!db_query('UPDATE '.TB_PREF.'wh_material_requests SET '.implode(',', $sets).' WHERE request_id='.$identity)) return false;
                $current = material_line_source($identity);
                return $current && material_core_source_fingerprint($current) === material_core_source_fingerprint($expected)
                    && material_core_history($identity) === $history && material_core_context() === $context;
            }, true);
        if (!$result || material_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
