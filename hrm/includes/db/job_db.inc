<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 normalized Job identity and explicit administrator writer.
 *
 * No legacy source carries a trustworthy reusable Job identity. job_classes
 * remains pay-basis classification and recruitment job_title remains
 * opening-specific free text. This writer therefore accepts only explicit HRM
 * administrator input scoped to the exact compatibility Legal Entity. Position
 * binding, Assignment binding, hierarchy, headcount/occupancy, salary authority,
 * costing and concurrent-payroll semantics remain deliberately deferred.
 */

/** @return bool */
function hrm_job_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_jobs')."'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_job_writer_authorized()
{
    return isset($_SESSION['wa_current_user'])
        && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'can_access')
        && $_SESSION['wa_current_user']->can_access('SA_JOB');
}

/** @param string $value @return int */
function hrm_job_text_length($value)
{
    return function_exists('mb_strlen') ? (int)mb_strlen((string)$value, 'UTF-8') : strlen((string)$value);
}

/** @return array|false */
function normalize_hrm_job_identity($code, $name, $status = 'active')
{
    $code = trim((string)$code);
    $name = trim((string)$name);
    $status = trim((string)$status);
    if ($code === '' || $name === '' || hrm_job_text_length($code) > 32
        || hrm_job_text_length($name) > 120
        || preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/D', $code) !== 1
        || !in_array($status, array('active','inactive'), true))
        return false;
    return array('job_code'=>$code, 'job_name'=>$name, 'job_status'=>$status);
}

/** @return array|false|null */
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

function get_hrm_job_by_code($legal_entity_id, $code, $for_update = false)
{
    if (!hrm_job_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $identity = normalize_hrm_job_identity($code, 'placeholder', 'active');
    if ($identity === false)
        return false;
    $sql = "SELECT job_id,legal_entity_id,job_code,job_name,job_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_jobs WHERE legal_entity_id=".(int)$legal_entity_id
        ." AND job_code=".db_escape($identity['job_code']);
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Job by code');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/** @return resource|false */
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

function hrm_job_row_owned_by_admin_writer($row, $legal_entity_id)
{
    return is_array($row)
        && (int)$row['job_id'] > 0
        && (int)$row['legal_entity_id'] === (int)$legal_entity_id
        && (string)$row['reference_quality'] === 'greenfield_explicit_admin_input'
        && (string)$row['source'] === 'hrm_job_admin';
}

/**
 * Verify one explicit Position relationship target. New bindings require an
 * active same-Legal-Entity Job owned by the normalized Job administrator
 * writer; an exact existing binding may opt out of the active-state check.
 *
 * @return int|false
 */
function get_hrm_verified_position_job_binding($job_id, $legal_entity_id,
    $for_update = false, $require_active = true)
{
    if (!is_numeric($job_id)
        || (string)(int)$job_id !== trim((string)$job_id)
        || (int)$job_id <= 0 || (int)$legal_entity_id <= 0)
        return false;
    $row = get_hrm_job((int)$job_id, $for_update);
    if (!hrm_job_row_owned_by_admin_writer($row, (int)$legal_entity_id))
        return false;
    if ($require_active && (string)$row['job_status'] !== 'active')
        return false;
    return (int)$row['job_id'];
}

function append_hrm_job_writer_audit($action, $job_id, $legal_entity_id, $changed_fields, $reason_code)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!function_exists('append_domain_audit_event') || (int)$job_id <= 0
        || (int)$legal_entity_id <= 0 || !is_array($changed_fields)
        || trim((string)$action) === '' || trim((string)$reason_code) === '')
        return false;
    $scope = get_hrm_job($job_id, true);
    if (!hrm_job_row_owned_by_admin_writer($scope, $legal_entity_id))
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_job',
        'entity_key'=>array('job_id'=>(int)$job_id),
        'action'=>(string)$action,
        'reason_code'=>(string)$reason_code,
        'before'=>null,
        'after'=>array('explicit_admin_identity_written'=>true),
        'restricted_diff'=>array(
            'changed_fields'=>$changed_fields,
            'job_class_inference_used'=>false,
            'recruitment_title_inference_used'=>false,
            'position_binding_changed'=>false,
            'assignment_binding_changed'=>false,
            'salary_authority_changed'=>false,
            'hierarchy_changed'=>false,
            'occupancy_headcount_changed'=>false,
            'work_location_binding_changed'=>false,
            'costing_changed'=>false,
            'concurrent_payroll_changed'=>false,
            'restricted_values_stored'=>false,
        ),
    ));
}

function add_hrm_job($code, $name)
{
    global $path_to_root;
    if (!hrm_job_writer_authorized() || !hrm_job_table_ready())
        return false;
    $identity = normalize_hrm_job_identity($code, $name, 'active');
    if ($identity === false)
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    begin_transaction();
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    $duplicate = $legal_entity_id === false ? false
        : get_hrm_job_by_code((int)$legal_entity_id, $identity['job_code'], true);
    if ($legal_entity_id === false || $duplicate === false || is_array($duplicate)) {
        cancel_transaction();
        return false;
    }
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $sql = "INSERT INTO ".TB_PREF."hrm_jobs (legal_entity_id,job_code,job_name,job_status,reference_quality,source,created_by) VALUES ("
        .(int)$legal_entity_id.",".db_escape($identity['job_code']).",".db_escape($identity['job_name'])
        .",'active','greenfield_explicit_admin_input','hrm_job_admin',".$actor.")";
    $result = db_query($sql, 'could not create HRM Job');
    $job_id = $result ? (int)db_insert_id() : 0;
    if ($result && $job_id > 0)
        $result = append_hrm_job_writer_audit('hrm.job.created', $job_id, (int)$legal_entity_id,
            array('job_code','job_name','job_status'), 'hrm_fnd_003_job_writer_create') !== false;
    if ($result) {
        commit_transaction();
        return $job_id;
    }
    cancel_transaction();
    return false;
}

function update_hrm_job($job_id, $code, $name, $status)
{
    global $path_to_root;
    if (!hrm_job_writer_authorized() || !hrm_job_table_ready() || (int)$job_id <= 0)
        return false;
    $identity = normalize_hrm_job_identity($code, $name, $status);
    if ($identity === false)
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    begin_transaction();
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    $current = $legal_entity_id === false ? false : get_hrm_job((int)$job_id, true);
    if ($legal_entity_id === false || !hrm_job_row_owned_by_admin_writer($current, (int)$legal_entity_id)) {
        cancel_transaction();
        return false;
    }
    $duplicate = get_hrm_job_by_code((int)$legal_entity_id, $identity['job_code'], true);
    if ($duplicate === false || (is_array($duplicate) && (int)$duplicate['job_id'] !== (int)$job_id)) {
        cancel_transaction();
        return false;
    }
    $changed = array();
    foreach (array('job_code','job_name','job_status') as $field)
        if ((string)$current[$field] !== (string)$identity[$field])
            $changed[] = $field;
    if (empty($changed)) {
        commit_transaction();
        return true;
    }
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $sql = "UPDATE ".TB_PREF."hrm_jobs SET job_code=".db_escape($identity['job_code'])
        .",job_name=".db_escape($identity['job_name']).",job_status=".db_escape($identity['job_status'])
        .",updated_at=NOW(),updated_by=".$actor." WHERE job_id=".(int)$job_id." AND legal_entity_id=".(int)$legal_entity_id;
    $result = db_query($sql, 'could not update HRM Job');
    if ($result)
        $result = append_hrm_job_writer_audit('hrm.job.updated', (int)$job_id, (int)$legal_entity_id,
            $changed, 'hrm_fnd_003_job_writer_update') !== false;
    if ($result) {
        commit_transaction();
        return true;
    }
    cancel_transaction();
    return false;
}

function delete_hrm_job($job_id)
{
    return false;
}
