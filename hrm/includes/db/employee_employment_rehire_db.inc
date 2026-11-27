<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * Employment rehire history guard.
 *
 * Rehire rows in hrm_employment_rehires link a predecessor and a successor
 * Employment episode. No runtime command creates them any more (the reviewed
 * duplicate-person link and rehire chain had no page and was removed), but rows
 * written before stay authoritative history: the chronology they recorded
 * must not be changed by later flat-row edits.
 */

/** @return bool */
function hrm_employment_rehire_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function')
        || !defined('TB_PREF'))
        return false;
    // Keep the runtime TB_PREF placeholder outside value escaping. In browser
    // sessions TB_PREF is '&TB_PREF&' and db_query() replaces that token with
    // the selected company prefix. Escaping the full prefixed name first would
    // encode the placeholder and make an installed foundation table appear absent.
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_employment_rehires')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Protect the chronology used by an executed rehire from later flat-row edits.
 * Successor end/status may continue to evolve, but the predecessor end and
 * successor start that established the rehire relationship are immutable.
 *
 * @param int $employment_id
 * @param array $projection
 * @return bool
 */
function hrm_employment_rehire_projection_change_allowed($employment_id, $projection)
{
    $employment_id = (int)$employment_id;
    if ($employment_id <= 0 || !is_array($projection) || !hrm_employment_rehire_table_ready())
        return true;
    $sql = "SELECT predecessor_employment_id,successor_employment_id,predecessor_end_date,"
        ."successor_start_date FROM ".TB_PREF."hrm_employment_rehires WHERE predecessor_employment_id="
        .$employment_id." OR successor_employment_id=".$employment_id." FOR UPDATE";
    $result = db_query($sql, 'could not verify immutable Employment rehire chronology');
    if (!$result)
        return false;
    while ($row = db_fetch_assoc($result)) {
        if ((int)$row['predecessor_employment_id'] === $employment_id) {
            if ((string)$projection['end_date'] !== (string)$row['predecessor_end_date']
                || (string)$projection['employment_status'] !== 'terminated')
                return false;
        }
        if ((int)$row['successor_employment_id'] === $employment_id
            && (string)$projection['start_date'] !== (string)$row['successor_start_date'])
            return false;
    }
    return true;
}
