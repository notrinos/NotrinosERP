<?php
/** Native demand construction retains number, source and review custody atomically. */
require_once __DIR__.'/warehouse_material_header_db.inc';

/** Keep standalone and paired requisition authority explicit at construction. */
function material_create_context($paired)
{
    if (!$paired) return material_core_context();
    return function_exists('purch_requisition_header_context') ? purch_requisition_header_context() : false;
}

/**
 * Create only an empty, unapproved native draft. The paired entry is called by
 * the existing requisition command and retains that command's outer transaction.
 * Lock current review/number rows rather than trusting MAX over an MVCC snapshot.
 * Unique native numbers remain the final database arbiter for concurrent empty
 * ranges; an engine deadlock/SQL failure rolls back the whole command for retry.
 */
function create_material_request_header($type, $location, $date, $required, $reference, $memo, $paired = false)
{
    $context = material_create_context($paired);
    $actor = isset($_SESSION['wa_current_user']->user) ? dimension_writer_reference_id($_SESSION['wa_current_user']->user) : false;
    if (!$context || !$actor || $actor > 32767 || !defined('ST_WH_MATERIAL_REQUEST')
        || ($paired && empty($GLOBALS['transaction_level']))) return false;
    try {
        if (!begin_transaction()) return false;
        $values = material_header_values($type, $location, $date, $required, $reference, $memo);
        if (!$values || ($paired && $type !== 'purchase')) { cancel_transaction(); return false; }
        $reviews = material_core_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='
            .ST_WH_MATERIAL_REQUEST.' ORDER BY id FOR UPDATE');
        $actions = material_core_rows('SELECT a.* FROM '.TB_PREF.'approval_actions a INNER JOIN '.TB_PREF
            .'approval_drafts d ON d.id=a.draft_id WHERE d.trans_type='.ST_WH_MATERIAL_REQUEST.' ORDER BY a.id FOR UPDATE');
        $numbers = material_core_rows('SELECT request_no FROM '.TB_PREF.'wh_material_requests ORDER BY request_no FOR UPDATE');
        $reserved = material_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_MATERIAL_REQUEST.' ORDER BY id FOR UPDATE');
        $locations = material_core_rows('SELECT loc_code,inactive FROM '.TB_PREF.'locations WHERE loc_code='
            .db_escape($values['warehouse_loc_code']).' FOR UPDATE');
        $users = material_core_rows('SELECT id,inactive FROM '.TB_PREF.'users WHERE id='.$actor.' FOR UPDATE');
        if ($reviews === false || $actions === false || $numbers === false || $reserved === false || !$locations || !$users || $users[0]['inactive']
            || $locations[0]['loc_code'] !== $values['warehouse_loc_code'] || $locations[0]['inactive']) { cancel_transaction(); return false; }
        $maximum = 0; $historical_ids = array();
        $approval = $reviews ? get_approval_workflow_service() : null;
        foreach ($numbers as $row) {
            $number = dimension_writer_reference_id($row['request_no']);
            if (!$number || $number >= 2147483647) { cancel_transaction(); return false; }
            $maximum = max($maximum, $number);
        }
        foreach ($reserved as $row) {
            $number = dimension_writer_reference_id($row['id']);
            if (!$number || $number >= 2147483647) { cancel_transaction(); return false; }
            $maximum = max($maximum, $number);
        }
        foreach ($reviews as $row) {
            $number = dimension_writer_reference_id($row['reserved_trans_no']);
            $data = $approval->decodeStoredDraftData($row['draft_data']);
            $id = is_array($data) && isset($data['request_id']) ? dimension_writer_reference_id($data['request_id']) : false;
            if (!$number || $number >= 2147483647 || !$id) { cancel_transaction(); return false; }
            $maximum = max($maximum, $number); $historical_ids[$id] = true;
        }
        $request_no = $maximum + 1;
        $result = dimension_ordinary_master_write('wh_material_requests', null, array(),
            function($ignored) use ($values, $context, $paired, $request_no, $actor, $reviews, $actions, $reserved, $historical_ids) {
                if (material_create_context($paired) !== $context) return false;
                $now = material_core_rows('SELECT NOW() AS created_at');
                if (!$now) return false;
                $expected = array_merge($values, array('request_no'=>(string)$request_no, 'status'=>'draft',
                    'requested_by'=>(string)$actor, 'approved_by'=>null, 'linked_doc_type'=>null, 'linked_doc_no'=>null,
                    'custom_data'=>null, 'created_at'=>$now[0]['created_at'], 'updated_at'=>null));
                $sets = array();
                foreach ($expected as $field=>$value) $sets[] = $field.'='.($value === null ? 'NULL' : db_escape($value));
                if (!db_query('INSERT INTO '.TB_PREF.'wh_material_requests SET '.implode(',', $sets))) return false;
                $id = dimension_writer_reference_id(db_insert_id());
                if (!$id || isset($historical_ids[$id])) return false;
                // Read every authored field, including nullable defaults, after SQL.
                $current = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$id.' FOR UPDATE');
                $expected = array_merge(array('request_id'=>(string)$id), $expected);
                if (!$current || array_diff_key($current[0], $expected) || array_diff_key($expected, $current[0])
                    || material_core_source_fingerprint(array('header'=>$current[0], 'lines'=>array()))
                    !== material_core_source_fingerprint(array('header'=>array_replace($current[0], $expected), 'lines'=>array()))) return false;
                $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$id.' ORDER BY line_id FOR UPDATE');
                $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$id.' ORDER BY id FOR UPDATE');
                if ($lines === false || $lines || $pairs === false || $pairs
                    || material_core_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_WH_MATERIAL_REQUEST.' ORDER BY id FOR UPDATE') !== $reviews
                    || material_core_rows('SELECT a.* FROM '.TB_PREF.'approval_actions a INNER JOIN '.TB_PREF.'approval_drafts d ON d.id=a.draft_id WHERE d.trans_type='.ST_WH_MATERIAL_REQUEST.' ORDER BY a.id FOR UPDATE') !== $actions
                    || material_core_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_WH_MATERIAL_REQUEST.' ORDER BY id FOR UPDATE') !== $reserved
                    || material_create_context($paired) !== $context) return false;
                return db_preserve_insert_id($id) ? $id : false;
            });
        if (!$result || material_create_context($paired) !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        if (!db_preserve_insert_id($result)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
