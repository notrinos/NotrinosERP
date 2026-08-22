<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 greenfield normalized Job foundation.
 *
 * No legacy table carries a trustworthy reusable Job identity. In particular,
 * job_classes remains pay-basis classification and positions remains Position
 * identity plus mixed legacy metadata. This helper therefore exposes only the
 * new Legal-Entity-scoped Job table and deliberately performs no legacy
 * projection, seed, writer synchronization, Position binding or Assignment
 * binding.
 */

/** @return bool */
function hrm_job_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_jobs')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Read one normalized Job by identity.
 *
 * @param int $job_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_job($job_id, $for_update = false)
{
    if (!hrm_job_table_ready() || (int)$job_id <= 0)
        return false;
    $sql = "SELECT job_id,legal_entity_id,job_code,job_name,job_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_jobs WHERE job_id=".(int)$job_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Job');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Read normalized Jobs for one Legal Entity. This is intentionally read-only;
 * no public Job writer or permission is introduced by the foundation slice.
 *
 * @param int $legal_entity_id
 * @param bool $active_only
 * @return resource|false
 */
function get_hrm_jobs_for_legal_entity($legal_entity_id, $active_only = true)
{
    if (!hrm_job_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $sql = "SELECT job_id,legal_entity_id,job_code,job_name,job_status,reference_quality,source "
        ."FROM ".TB_PREF."hrm_jobs WHERE legal_entity_id=".(int)$legal_entity_id;
    if ($active_only)
        $sql .= " AND job_status='active'";
    $sql .= ' ORDER BY job_code,job_id';
    return db_query($sql, 'could not list HRM Jobs');
}
