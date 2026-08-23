<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 bounded Position compatibility foundation.
 *
 * The legacy positions row is accepted only as current Position identity and
 * active/inactive evidence. Job class, Department, reporting hierarchy,
 * compensation, headcount, manager and description semantics remain legacy
 * metadata and are deliberately not copied into the normalized Position.
 */

/** @return bool */
function hrm_position_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_positions')."'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_position_job_column_ready()
{
    if (!hrm_position_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_positions LIKE 'job_id'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_position_job_writer_authorized()
{
    return isset($_SESSION['wa_current_user'])
        && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'can_access')
        && $_SESSION['wa_current_user']->can_access('SA_POSITION');
}

/**
 * Normalize one legacy positions row into identity/status only.
 *
 * @param array $position
 * @param int $legal_entity_id
 * @return array|false
 */
function hrm_position_projection_from_legacy_position($position, $legal_entity_id)
{
    if (!is_array($position) || (int)$legal_entity_id <= 0)
        return false;
    $legacy_position_id = isset($position['position_id']) ? (int)$position['position_id'] : 0;
    $name = isset($position['position_name']) ? trim((string)$position['position_name']) : '';
    if ($legacy_position_id <= 0 || $name === '')
        return false;

    $legacy_code = isset($position['position_code']) ? trim((string)$position['position_code']) : '';
    if (strlen($legacy_code) > 20)
        return false;
    if ($legacy_code === '')
        $legacy_code = null;

    $inactive = isset($position['inactive']) ? (int)$position['inactive'] : 0;
    if ($inactive !== 0 && $inactive !== 1)
        return false;

    return array(
        'legal_entity_id'=>(int)$legal_entity_id,
        'legacy_position_id'=>$legacy_position_id,
        'position_code'=>'POSITION-'.$legacy_position_id,
        'legacy_position_code'=>$legacy_code,
        'position_name'=>$name,
        'position_status'=>$inactive ? 'inactive' : 'active',
        'reference_quality'=>'legacy_position_identity_only',
        'source'=>'legacy_position_seed',
    );
}

/**
 * Read one normalized Position by its immutable legacy compatibility key.
 *
 * @param int $legacy_position_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_position_by_legacy_position($legacy_position_id, $for_update = false)
{
    if (!hrm_position_table_ready() || (int)$legacy_position_id <= 0)
        return false;
    $job = hrm_position_job_column_ready() ? 'job_id' : 'NULL AS job_id';
    $sql = "SELECT position_id,legal_entity_id,".$job.",legacy_position_id,position_code,legacy_position_code,"
        ."position_name,position_status,reference_quality,source,created_at,created_by,updated_at,updated_by "
        ."FROM ".TB_PREF."hrm_positions WHERE legacy_position_id=".(int)$legacy_position_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Position compatibility row');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Verify an exact identity/status-only Position projection.
 *
 * @param array $row
 * @param array $projection
 * @return bool
 */
function hrm_position_row_matches_projection($row, $projection)
{
    if (!is_array($row) || !is_array($projection))
        return false;
    $left_code = ($row['legacy_position_code'] === null || $row['legacy_position_code'] === '')
        ? null : (string)$row['legacy_position_code'];
    $right_code = ($projection['legacy_position_code'] === null || $projection['legacy_position_code'] === '')
        ? null : (string)$projection['legacy_position_code'];
    return (int)$row['legal_entity_id'] === (int)$projection['legal_entity_id']
        && (int)$row['legacy_position_id'] === (int)$projection['legacy_position_id']
        && (string)$row['position_code'] === (string)$projection['position_code']
        && $left_code === $right_code
        && (string)$row['position_name'] === (string)$projection['position_name']
        && (string)$row['position_status'] === (string)$projection['position_status']
        && (string)$row['reference_quality'] === 'legacy_position_identity_only'
        && (string)$row['source'] === 'legacy_position_seed';
}

/**
 * Load one legacy Position row through the normalized Position lock path.
 * Mixed Job class, Department, hierarchy, compensation, headcount, manager
 * and description fields are deliberately not selected into this boundary.
 *
 * @param int $legacy_position_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_legacy_position_for_binding($legacy_position_id, $for_update = false)
{
    if ((int)$legacy_position_id <= 0)
        return false;
    $sql = "SELECT position_id,position_code,position_name,inactive FROM ".TB_PREF."positions "
        ."WHERE position_id=".(int)$legacy_position_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read legacy Position for normalized Position binding');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Resolve an exact legacy-Position-backed normalized Position under persisted
 * Employment Legal Entity scope. Zero and accepted negative irregular legacy
 * references remain explicit unknown/unbound state.
 *
 * @param mixed $legacy_position_id
 * @param int $legal_entity_id
 * @param bool $for_update
 * @param bool $allow_legacy_irregular
 * @return int|false|null
 */
function get_hrm_verified_position_binding($legacy_position_id, $legal_entity_id,
    $for_update = false, $allow_legacy_irregular = false)
{
    if (!is_numeric($legacy_position_id)
        || (string)(int)$legacy_position_id !== trim((string)$legacy_position_id)
        || (int)$legal_entity_id <= 0)
        return false;
    $legacy_position_id = (int)$legacy_position_id;
    if ($legacy_position_id === 0 || ($legacy_position_id < 0 && $allow_legacy_irregular))
        return null;
    if ($legacy_position_id < 0 || !hrm_position_table_ready())
        return false;
    $legacy = get_hrm_legacy_position_for_binding($legacy_position_id, $for_update);
    $position = get_hrm_position_by_legacy_position($legacy_position_id, $for_update);
    if (!is_array($legacy) || !is_array($position))
        return false;
    $projection = hrm_position_projection_from_legacy_position($legacy, $legal_entity_id);
    if (!$projection || !hrm_position_row_matches_projection($position, $projection))
        return false;
    return (int)$position['position_id'] > 0 ? (int)$position['position_id'] : false;
}

/**
 * Normalize one explicit Position -> Job relationship request. Empty/zero is
 * intentional unassignment; positive identifiers must be exact integers.
 *
 * @return int|null|false
 */
function normalize_hrm_position_job_target($job_id)
{
    if ($job_id === null || trim((string)$job_id) === '' || trim((string)$job_id) === '0')
        return null;
    if (!is_numeric($job_id)
        || (string)(int)$job_id !== trim((string)$job_id)
        || (int)$job_id <= 0)
        return false;
    return (int)$job_id;
}

/** Append restricted-value-free Position -> Job relationship evidence. */
function append_hrm_position_job_binding_audit($position_id, $legacy_position_id,
    $legal_entity_id, $before_job_id, $after_job_id)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!function_exists('append_domain_audit_event') || (int)$position_id <= 0
        || (int)$legacy_position_id <= 0 || (int)$legal_entity_id <= 0)
        return false;
    $scope = get_hrm_position_by_legacy_position((int)$legacy_position_id, true);
    if (!is_array($scope) || (int)$scope['position_id'] !== (int)$position_id
        || (int)$scope['legal_entity_id'] !== (int)$legal_entity_id
        || (string)$scope['reference_quality'] !== 'legacy_position_identity_only'
        || (string)$scope['source'] !== 'legacy_position_seed')
        return false;
    $before_job_id = ($before_job_id === null || $before_job_id === '') ? null : (int)$before_job_id;
    $after_job_id = ($after_job_id === null || $after_job_id === '') ? null : (int)$after_job_id;
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_position',
        'entity_key'=>array(
            'position_id'=>(int)$position_id,
            'legacy_position_id'=>(int)$legacy_position_id,
        ),
        'action'=>'hrm.position.job_binding_changed',
        'reason_code'=>'hrm_fnd_003_position_job_writer',
        'before'=>array('job_id'=>$before_job_id),
        'after'=>array('job_id'=>$after_job_id),
        'restricted_diff'=>array(
            'changed_fields'=>array('job_id'),
            'position_job_binding_changed'=>true,
            'assignment_job_binding_changed'=>false,
            'job_class_inference_used'=>false,
            'recruitment_title_inference_used'=>false,
            'reporting_hierarchy_changed'=>false,
            'manager_semantics_changed'=>false,
            'occupancy_headcount_changed'=>false,
            'salary_authority_changed'=>false,
            'work_location_binding_changed'=>false,
            'costing_changed'=>false,
            'concurrent_payroll_changed'=>false,
            'payroll_routes_changed'=>false,
            'accounting_rows_changed'=>false,
            'restricted_values_stored'=>false,
        ),
    ));
}

/**
 * Mutate only the normalized Position -> Job relationship under SA_POSITION.
 * Caller may already own a surrounding transaction; no transaction is opened
 * here so legacy Position identity sync plus relationship audit remain atomic.
 */
function set_hrm_position_job_binding($legacy_position_id, $job_id)
{
    global $path_to_root;
    if (!hrm_position_job_writer_authorized() || !hrm_position_job_column_ready()
        || (int)$legacy_position_id <= 0)
        return false;
    $target_job_id = normalize_hrm_position_job_target($job_id);
    if ($target_job_id === false)
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    if (!function_exists('get_hrm_verified_position_job_binding'))
        include_once($path_to_root.'/hrm/includes/db/job_db.inc');
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    if ($legal_entity_id === false)
        return false;
    $current = get_hrm_position_by_legacy_position((int)$legacy_position_id, true);
    if (!is_array($current) || (int)$current['position_id'] <= 0
        || (int)$current['legal_entity_id'] !== (int)$legal_entity_id
        || (string)$current['reference_quality'] !== 'legacy_position_identity_only'
        || (string)$current['source'] !== 'legacy_position_seed')
        return false;
    $current_job_id = isset($current['job_id']) && $current['job_id'] !== null && $current['job_id'] !== ''
        ? (int)$current['job_id'] : null;
    if ($target_job_id !== null) {
        $require_active = $target_job_id !== $current_job_id;
        if (!function_exists('get_hrm_verified_position_job_binding')
            || get_hrm_verified_position_job_binding(
                $target_job_id, (int)$legal_entity_id, true, $require_active
            ) === false)
            return false;
    }
    if ($target_job_id === $current_job_id)
        return true;
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $sql = "UPDATE ".TB_PREF."hrm_positions SET job_id="
        .($target_job_id === null ? 'NULL' : (int)$target_job_id)
        .",updated_at=NOW(),updated_by=".(int)$actor
        ." WHERE position_id=".(int)$current['position_id']
        ." AND legal_entity_id=".(int)$legal_entity_id;
    if (!db_query($sql, 'could not update normalized Position Job relationship'))
        return false;
    return append_hrm_position_job_binding_audit(
        (int)$current['position_id'], (int)$legacy_position_id, (int)$legal_entity_id,
        $current_job_id, $target_job_id
    ) !== false;
}

/**
 * Append restricted-value-free evidence for one legacy Position writer sync.
 * Position names/codes and all mixed compensation/classification values are
 * deliberately excluded from the audit payload.
 *
 * @param string $action
 * @param int $position_id
 * @param int $legacy_position_id
 * @param int $legal_entity_id
 * @param array $changed_fields
 * @param string $reason_code
 * @return array|false
 */
function append_hrm_position_writer_audit($action, $position_id, $legacy_position_id,
    $legal_entity_id, $changed_fields, $reason_code)
{
    if (!function_exists('append_domain_audit_event') || (int)$position_id <= 0
        || (int)$legacy_position_id <= 0 || (int)$legal_entity_id <= 0
        || !is_array($changed_fields) || trim((string)$action) === ''
        || trim((string)$reason_code) === '')
        return false;
    $scope = get_hrm_position_by_legacy_position($legacy_position_id, true);
    if (!is_array($scope) || (int)$scope['position_id'] !== (int)$position_id
        || (int)$scope['legal_entity_id'] !== (int)$legal_entity_id)
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$scope['legal_entity_id'],
        'entity_type'=>'hrm_position',
        'entity_key'=>array(
            'position_id'=>(int)$position_id,
            'legacy_position_id'=>(int)$legacy_position_id,
        ),
        'action'=>(string)$action,
        'reason_code'=>(string)$reason_code,
        'before'=>null,
        'after'=>array('legacy_position_projection_synchronized'=>true),
        'restricted_diff'=>array(
            'changed_fields'=>$changed_fields,
            'job_class_copied'=>false,
            'organization_semantics_copied'=>false,
            'reporting_hierarchy_copied'=>false,
            'compensation_copied'=>false,
            'headcount_copied'=>false,
            'manager_semantics_copied'=>false,
            'description_copied'=>false,
            'restricted_values_stored'=>false,
        ),
    ));
}

/**
 * Insert or update one exact legacy-Position-backed normalized identity/status
 * projection. Caller owns authorization and surrounding transaction.
 *
 * @param int $legacy_position_id
 * @param array $changed_fields
 * @param string $operation
 * @return bool
 */
function synchronize_hrm_position_from_legacy_position($legacy_position_id, $changed_fields, $operation)
{
    global $path_to_root;
    if ((int)$legacy_position_id <= 0 || !is_array($changed_fields)
        || trim((string)$operation) === '' || !hrm_position_table_ready())
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    if ($legal_entity_id === false)
        return false;
    $legacy = get_hrm_legacy_position_for_binding((int)$legacy_position_id, true);
    if (!is_array($legacy))
        return false;
    $projection = hrm_position_projection_from_legacy_position($legacy, (int)$legal_entity_id);
    if (!$projection)
        return false;
    $current = get_hrm_position_by_legacy_position((int)$legacy_position_id, true);
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    if ($current === null) {
        if ((string)$operation !== 'create')
            return false;
        $sql = "INSERT INTO ".TB_PREF."hrm_positions (legal_entity_id,legacy_position_id,position_code,"
            ."legacy_position_code,position_name,position_status,reference_quality,source,created_by) VALUES ("
            .(int)$projection['legal_entity_id'].",".(int)$projection['legacy_position_id'].","
            .db_escape($projection['position_code']).",".($projection['legacy_position_code'] === null
                ? 'NULL' : db_escape($projection['legacy_position_code'])).",".db_escape($projection['position_name']).","
            .db_escape($projection['position_status']).",'legacy_position_identity_only','legacy_position_seed',".(int)$actor.")";
        if (!db_query($sql, 'could not synchronize new normalized Position from legacy Position'))
            return false;
        $current = get_hrm_position_by_legacy_position((int)$legacy_position_id, true);
    } else {
        if (!is_array($current) || (int)$current['legal_entity_id'] !== (int)$legal_entity_id)
            return false;
        $sql = "UPDATE ".TB_PREF."hrm_positions SET legacy_position_code="
            .($projection['legacy_position_code'] === null ? 'NULL' : db_escape($projection['legacy_position_code']))
            .",position_name=".db_escape($projection['position_name'])
            .",position_status=".db_escape($projection['position_status'])
            .",updated_at=NOW(),updated_by=".(int)$actor
            ." WHERE position_id=".(int)$current['position_id'];
        if (!db_query($sql, 'could not synchronize normalized Position from legacy Position'))
            return false;
        $current = get_hrm_position_by_legacy_position((int)$legacy_position_id, true);
    }
    if (!is_array($current) || !hrm_position_row_matches_projection($current, $projection))
        return false;
    return append_hrm_position_writer_audit(
        'hrm.position.legacy_position_synchronized', (int)$current['position_id'],
        (int)$legacy_position_id, (int)$legal_entity_id, $changed_fields,
        'hrm_fnd_003_position_writer_'.$operation
    ) !== false;
}
