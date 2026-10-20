<?php
/** Manufacturing-owned retained JSON presence, never unused/delete authority. */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_registry.inc';

/** Return fixed raw JSON owners exposed by work-order, issue-item and BOM readers. */
function manufacturing_dimension_reference_sources()
{
    return array('workorders','wo_issue_items','bom');
}

/** Recheck exact active-prefix retained storage without cached/caller authority. */
function manufacturing_dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized() || !is_string($table)
        || !in_array($table,manufacturing_dimension_reference_sources(),true)) return false;
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
        ." AND BINARY COLUMN_NAME=BINARY 'custom_data'");
    if (!$rows) return false;
    $row = db_fetch_row($rows);
    return $row && (int)$row[0] === 1;
}

/**
 * Observe positive native intent without reusing current-master/void joins that
 * hide retained work orders or issue lines. Include every lifecycle state and
 * orphan row; continue to positive siblings when another owner is unavailable.
 * Only strict ordinary JSON is recognized, matching the raw stored field. No
 * new source defaults, approval execution or WIP/GL attribution is established.
 * No document, stock, actor, quantity, cost or payload escapes this boundary.
 *
 * @param int|string $id Canonical positive active-company native identity
 * @return string used or unknown only
 */
function manufacturing_observe_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    foreach (manufacturing_dimension_reference_sources() as $table) {
        try {
            if (!manufacturing_dimension_reference_schema_available($table)) continue;
            $rows = db_query('SELECT custom_data AS payload FROM '.TB_PREF.$table);
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $payload = is_string($row['payload']) ? json_decode($row['payload'],true) : null;
                if (is_array($payload) && dimension_reference_payload_status($payload,(int)$id) === 'used') return 'used';
            }
        } catch (Exception $exception) {
            // An unavailable owner never attests absence; inspect fixed siblings.
        }
    }
    return 'unknown';
}
