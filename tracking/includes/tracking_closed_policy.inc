<?php
/**
 * DIM-LFC-003: the simple delete policy, "a used Dimension is closed, never hard-deleted".
 *
 * One declaration says which stores written after the DIM-LFC-001 matrix hold a Dimension identity. The locked
 * B10 recheck reads each declared column with a shared locking read, so a Dimension named by any of them is
 * reported used (delete and axis reassignment are refused) and no writer of the store needs its own writer fence:
 * the delete holds the shared locks and a used Dimension is closed instead (close_dimension() only sets a flag, it
 * never removes the master or any reference).
 *
 * The rule for a new table: if it stores a Dimension identity (any column, however named), add it to
 * dimension_closed_policy_owners() (or, for a JSON payload, dimension_closed_policy_payload_owners()); if it stores
 * none, list it in dimension_closed_policy_exempt_tables() with the reason. The contract test fails for a
 * `dimension_*` table that is in neither list, so a new store cannot silently escape the delete recheck.
 * Do not add a new table to the per-family writer fence of dimension_ordinary_master_write().
 *
 * This file only declares and reads. It grants no access, writes nothing and never authorizes a deletion by
 * itself: the answer feeds dimension_reference_locked_recheck() (C01 owns the decision).
 */
require_once __DIR__.'/tracking_reference_registry.inc';

/**
 * Stores of the phase-5 families that hold a Dimension identity in scalar columns.
 * A zero is "no Dimension" (budget, commitment, forecast coordinates and hierarchy roots); an NULL never matches.
 *
 * @return array table => array(columns => exact column names, reason => why the store exists)
 */
function dimension_closed_policy_owners()
{
    return array(
        'dimension_allocation_driver_values'=>array('columns'=>array('target_id'),
            'reason'=>'DIM-ALC-002 measured driver units of one Dimension'),
        'dimension_allocation_driver_runs'=>array('columns'=>array('residual_target_id'),
            'reason'=>'DIM-ALC-002 guided run that routes the unmeasured residual to one Dimension'),
        'dimension_budget_version_lines'=>array('columns'=>array('coordinate_1', 'coordinate_2'),
            'reason'=>'DIM-BUD-002 immutable baseline and revision lines per Dimension coordinate'),
        'dimension_commitment_lines'=>array('columns'=>array('coordinate_1', 'coordinate_2'),
            'reason'=>'DIM-ADV-001 commitment register line per Dimension coordinate'),
        'dimension_commitment_reservations'=>array('columns'=>array('coordinate_1', 'coordinate_2'),
            'reason'=>'DIM-ADV-001 reserved funds per Dimension coordinate'),
        'dimension_forecast_lines'=>array('columns'=>array('coordinate_1', 'coordinate_2'),
            'reason'=>'DIM-ADV-001 estimate-to-complete line per Dimension coordinate'),
        'dimension_hierarchy_members'=>array('columns'=>array('member_id', 'parent_member_id'),
            'reason'=>'DIM-ADV-002 hierarchy member and its parent'));
}

/**
 * Stores that hold a Dimension identity only inside a JSON payload.
 *
 * @return array table => array(column => payload column, key => primary key, reason => why)
 */
function dimension_closed_policy_payload_owners()
{
    return array(
        'dimension_allocation_runs'=>array('column'=>'request_payload', 'key'=>'run_id',
            'reason'=>'DIM-ALC-001 retained request (source lines and target Dimensions) of a run that may never have posted'));
}

/**
 * Companion stores that hold no Dimension identity, with the reason. A `dimension_*` table that is in neither this
 * list nor an owner list fails the contract.
 *
 * @return array table => reason
 */
function dimension_closed_policy_exempt_tables()
{
    return array(
        'dimension_category_policies'=>'company category labels; the axis is a number 1 or 2, not a Dimension',
        'dimension_category_policy_rules'=>'transition rule between category labels; no Dimension identity',
        'dimension_posting_links'=>'links a source revision to a GL row; the Dimensions are on those two records',
        'dimension_allocation_sources'=>'links a run to the GL row it reallocates; the Dimensions are on that row',
        'dimension_allocation_drivers'=>'driver version header (key, axis, period, status); the measured Dimensions are in the values',
        'dimension_budget_versions'=>'budget version header; the Dimensions are in the version lines',
        'dimension_budget_activations'=>'append-only activation audit of a budget version; no Dimension identity',
        'dimension_commitment_events'=>'amount history of a commitment line; the Dimensions are on the line',
        'dimension_commitment_activations'=>'append-only activation audit of the reservation switch; no Dimension identity',
        'dimension_forecast_versions'=>'forecast version header; the Dimensions are in the forecast lines',
        'dimension_hierarchy_versions'=>'hierarchy version header (axis, effective date, status); the members are in the member rows');
}

/**
 * Stores whose Dimension identity another, earlier authority already observes in the locked recheck.
 *
 * @return array table => owner that observes it
 */
function dimension_closed_policy_other_authority_tables()
{
    return array('dimension_source_revisions'=>'source_revisions (DIM-FND-006)');
}

/**
 * Every table name this policy declares, for the live schema read.
 *
 * @return string[]
 */
function dimension_closed_policy_tables()
{
    return array_merge(array_keys(dimension_closed_policy_owners()), array_keys(dimension_closed_policy_payload_owners()));
}

/**
 * Observe the declared stores for one Dimension inside the caller's open transaction.
 *
 * Every read is a shared locking read of the latest committed rows, so a concurrent writer waits for the delete
 * and a committed reference is never read past. A table that is absent from the live schema cannot hold evidence
 * and is unused; a table with a declared column missing, an unreadable table, a payload that cannot be decoded
 * or any SQL failure is unknown. Only owner names and states are returned, never a row or payload.
 *
 * @param int|string $id Exact positive native Dimension identity
 * @param array $columns table => column => true from dimension_reference_recheck_columns()
 * @return array table => used|unused|unknown
 */
function dimension_closed_policy_observe($id, $columns)
{
    $result = array();
    foreach (dimension_closed_policy_tables() as $table) $result[$table] = 'unknown';
    if (!function_exists('dimension_lifecycle_valid_id') || !dimension_lifecycle_valid_id($id) || empty($GLOBALS['transaction_level'])
        || !is_array($columns)) return $result;
    $id = (int)$id;
    foreach (dimension_closed_policy_owners() as $table=>$owner) {
        if (!isset($columns[$table])) continue;
        if (!count($columns[$table])) { $result[$table] = 'unused'; continue; }
        $complete = true;
        $conditions = array();
        foreach ($owner['columns'] as $column) {
            if (!isset($columns[$table][$column])) $complete = false;
            $conditions[] = '`'.$column.'`='.$id;
        }
        if (!$complete) continue;
        $rows = db_query('SELECT 1 FROM '.TB_PREF.$table.' WHERE ('.implode(' OR ', $conditions).') LIMIT 1 LOCK IN SHARE MODE');
        if (!$rows) continue;
        $result[$table] = db_num_rows($rows) ? 'used' : 'unused';
    }
    foreach (dimension_closed_policy_payload_owners() as $table=>$owner) {
        if (!isset($columns[$table])) continue;
        if (!count($columns[$table])) { $result[$table] = 'unused'; continue; }
        if (!isset($columns[$table][$owner['column']]) || !isset($columns[$table][$owner['key']])) continue;
        $rows = db_query('SELECT `'.$owner['column'].'` FROM '.TB_PREF.$table.' LOCK IN SHARE MODE');
        if (!$rows) continue;
        $state = 'unused';
        while ($row = db_fetch_row($rows)) {
            $payload = json_decode((string)$row[0], true);
            $status = is_array($payload) ? dimension_reference_payload_status($payload, $id) : 'unknown';
            if ($status === 'used') { $state = 'used'; break; }
            if ($status === 'unknown') $state = 'unknown';
        }
        $result[$table] = $state;
    }
    return $result;
}
