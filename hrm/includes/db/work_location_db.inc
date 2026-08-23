<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 normalized Work Location identity and explicit administrator writer.
 *
 * The accepted dependency review found no trustworthy legacy workforce-location
 * mapping. This writer therefore accepts only explicit HRM administrator input
 * scoped to the exact compatibility Legal Entity. It deliberately introduces no
 * Establishment identity is now available only as an explicit administrator-selected
 * relationship target. Address, timezone, jurisdiction, remote/site, inventory-location,
 * branch, dimension, Department, Job, Position, Assignment or payroll-tax semantics remain
 * deliberately uninferred.
 */

/** @return bool */
function hrm_work_location_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_work_locations')."'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_work_location_establishment_column_ready()
{
    if (!hrm_work_location_table_ready())
        return false;
    $result = db_query("SHOW COLUMNS FROM ".TB_PREF."hrm_work_locations LIKE 'establishment_id'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_work_location_writer_authorized()
{
    return isset($_SESSION['wa_current_user'])
        && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'can_access')
        && $_SESSION['wa_current_user']->can_access('SA_WORKLOCATION');
}

/** @param string $value @return int */
function hrm_work_location_text_length($value)
{
    return function_exists('mb_strlen') ? (int)mb_strlen((string)$value, 'UTF-8') : strlen((string)$value);
}

/**
 * Validate and normalize only structural Work Location identity fields.
 * Codes remain explicit administrator input; no legacy value is inferred.
 *
 * @param mixed $code
 * @param mixed $name
 * @param mixed $status
 * @return array|false
 */
function normalize_hrm_work_location_identity($code, $name, $status = 'active')
{
    $code = trim((string)$code);
    $name = trim((string)$name);
    $status = trim((string)$status);
    if ($code === '' || $name === '' || hrm_work_location_text_length($code) > 32
        || hrm_work_location_text_length($name) > 120
        || preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/D', $code) !== 1
        || !in_array($status, array('active','inactive'), true))
        return false;
    return array(
        'work_location_code'=>$code,
        'work_location_name'=>$name,
        'work_location_status'=>$status,
    );
}

/** Read one normalized Work Location identity. */
function get_hrm_work_location($work_location_id, $for_update = false)
{
    if (!hrm_work_location_table_ready() || (int)$work_location_id <= 0)
        return false;
    $establishment = hrm_work_location_establishment_column_ready() ? 'establishment_id' : 'NULL AS establishment_id';
    $sql = "SELECT work_location_id,legal_entity_id,".$establishment.",work_location_code,work_location_name,work_location_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_work_locations WHERE work_location_id=".(int)$work_location_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Work Location');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

function get_hrm_work_location_by_code($legal_entity_id, $code, $for_update = false)
{
    if (!hrm_work_location_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $identity = normalize_hrm_work_location_identity($code, 'placeholder', 'active');
    if ($identity === false)
        return false;
    $establishment = hrm_work_location_establishment_column_ready() ? 'establishment_id' : 'NULL AS establishment_id';
    $sql = "SELECT work_location_id,legal_entity_id,".$establishment.",work_location_code,work_location_name,work_location_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_work_locations WHERE legal_entity_id=".(int)$legal_entity_id
        ." AND work_location_code=".db_escape($identity['work_location_code']);
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Work Location by code');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

function get_hrm_work_locations_for_legal_entity($legal_entity_id, $active_only = true)
{
    if (!hrm_work_location_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $establishment = hrm_work_location_establishment_column_ready() ? 'establishment_id' : 'NULL AS establishment_id';
    $sql = "SELECT work_location_id,legal_entity_id,".$establishment.",work_location_code,work_location_name,work_location_status,reference_quality,source "
        ."FROM ".TB_PREF."hrm_work_locations WHERE legal_entity_id=".(int)$legal_entity_id;
    if ($active_only)
        $sql .= " AND work_location_status='active'";
    $sql .= ' ORDER BY work_location_code,work_location_id';
    return db_query($sql, 'could not list HRM Work Locations');
}

function hrm_work_location_row_owned_by_admin_writer($row, $legal_entity_id)
{
    return is_array($row)
        && (int)$row['work_location_id'] > 0
        && (int)$row['legal_entity_id'] === (int)$legal_entity_id
        && (string)$row['reference_quality'] === 'greenfield_explicit_admin_input'
        && (string)$row['source'] === 'hrm_work_location_admin';
}

/**
 * Resolve one Assignment writer target without inferring location semantics.
 * Only explicit administrator-owned Work Locations in the Employment Legal
 * Entity are eligible. New bindings require an active target; preservation of
 * an existing historical binding may opt out of the active-state requirement.
 *
 * @return int|false
 */
function get_hrm_verified_assignment_work_location_binding($work_location_id, $legal_entity_id,
    $for_update = false, $require_active = true)
{
    if (!is_numeric($work_location_id)
        || (string)(int)$work_location_id !== trim((string)$work_location_id)
        || (int)$work_location_id <= 0 || (int)$legal_entity_id <= 0)
        return false;
    $row = get_hrm_work_location((int)$work_location_id, $for_update);
    if (!hrm_work_location_row_owned_by_admin_writer($row, (int)$legal_entity_id))
        return false;
    if ($require_active && (string)$row['work_location_status'] !== 'active')
        return false;
    return (int)$row['work_location_id'];
}

/**
 * Normalize an explicit Work Location -> Establishment relationship request.
 * Empty/zero means an intentional unassignment; positive identifiers must be
 * exact decimal integers.
 *
 * @return int|null|false
 */
function normalize_hrm_work_location_establishment_target($establishment_id)
{
    if ($establishment_id === null || trim((string)$establishment_id) === '' || trim((string)$establishment_id) === '0')
        return null;
    if (!is_numeric($establishment_id)
        || (string)(int)$establishment_id !== trim((string)$establishment_id)
        || (int)$establishment_id <= 0)
        return false;
    return (int)$establishment_id;
}

function append_hrm_work_location_establishment_binding_audit($work_location_id, $legal_entity_id,
    $before_establishment_id, $after_establishment_id)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!function_exists('append_domain_audit_event') || (int)$work_location_id <= 0 || (int)$legal_entity_id <= 0)
        return false;
    $scope = get_hrm_work_location($work_location_id, true);
    if (!hrm_work_location_row_owned_by_admin_writer($scope, $legal_entity_id))
        return false;
    $before_establishment_id = ($before_establishment_id === null || $before_establishment_id === '') ? null : (int)$before_establishment_id;
    $after_establishment_id = ($after_establishment_id === null || $after_establishment_id === '') ? null : (int)$after_establishment_id;
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_work_location',
        'entity_key'=>array('work_location_id'=>(int)$work_location_id),
        'action'=>'hrm.work_location.establishment_binding_changed',
        'reason_code'=>'hrm_fnd_003_work_location_establishment_writer',
        'before'=>array('establishment_id'=>$before_establishment_id),
        'after'=>array('establishment_id'=>$after_establishment_id),
        'restricted_diff'=>array(
            'changed_fields'=>array('establishment_id'),
            'establishment_binding_changed'=>true,
            'address_semantics_changed'=>false,
            'timezone_semantics_changed'=>false,
            'jurisdiction_semantics_changed'=>false,
            'remote_site_semantics_changed'=>false,
            'assignment_binding_changed'=>false,
            'payroll_tax_routing_changed'=>false,
            'payroll_routes_changed'=>false,
            'accounting_rows_changed'=>false,
            'restricted_values_stored'=>false,
        ),
    ));
}

function append_hrm_work_location_writer_audit($action, $work_location_id, $legal_entity_id,
    $changed_fields, $reason_code)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!function_exists('append_domain_audit_event') || (int)$work_location_id <= 0
        || (int)$legal_entity_id <= 0 || !is_array($changed_fields)
        || trim((string)$action) === '' || trim((string)$reason_code) === '')
        return false;
    $scope = get_hrm_work_location($work_location_id, true);
    if (!hrm_work_location_row_owned_by_admin_writer($scope, $legal_entity_id))
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_work_location',
        'entity_key'=>array('work_location_id'=>(int)$work_location_id),
        'action'=>(string)$action,
        'reason_code'=>(string)$reason_code,
        'before'=>null,
        'after'=>array('explicit_admin_identity_written'=>true),
        'restricted_diff'=>array(
            'changed_fields'=>$changed_fields,
            'establishment_binding_changed'=>false,
            'address_semantics_changed'=>false,
            'timezone_semantics_changed'=>false,
            'jurisdiction_semantics_changed'=>false,
            'remote_site_semantics_changed'=>false,
            'assignment_binding_changed'=>false,
            'payroll_tax_routing_changed'=>false,
            'restricted_values_stored'=>false,
        ),
    ));
}

function add_hrm_work_location($code, $name, $establishment_id = null, $establishment_explicit = false)
{
    global $path_to_root;
    if (!hrm_work_location_writer_authorized() || !hrm_work_location_table_ready())
        return false;
    $identity = normalize_hrm_work_location_identity($code, $name, 'active');
    if ($identity === false)
        return false;
    if ($establishment_explicit && !hrm_work_location_establishment_column_ready())
        return false;
    $target_establishment_id = $establishment_explicit
        ? normalize_hrm_work_location_establishment_target($establishment_id) : null;
    if ($establishment_explicit && $target_establishment_id === false)
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    if ($establishment_explicit && $target_establishment_id !== null
        && !function_exists('get_hrm_verified_work_location_establishment_binding'))
        include_once($path_to_root.'/hrm/includes/db/establishment_db.inc');
    begin_transaction();
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    $duplicate = $legal_entity_id === false ? false
        : get_hrm_work_location_by_code((int)$legal_entity_id, $identity['work_location_code'], true);
    if ($legal_entity_id === false || $duplicate === false || is_array($duplicate)) {
        cancel_transaction();
        return false;
    }
    if ($establishment_explicit && $target_establishment_id !== null) {
        if (!function_exists('get_hrm_verified_work_location_establishment_binding')
            || get_hrm_verified_work_location_establishment_binding(
                $target_establishment_id, (int)$legal_entity_id, true, true
            ) === false) {
            cancel_transaction();
            return false;
        }
    }
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $establishment_column = $establishment_explicit ? ',establishment_id' : '';
    $establishment_value = $establishment_explicit
        ? ','.($target_establishment_id === null ? 'NULL' : (int)$target_establishment_id) : '';
    $sql = "INSERT INTO ".TB_PREF."hrm_work_locations (legal_entity_id".$establishment_column.",work_location_code,work_location_name,work_location_status,"
        ."reference_quality,source,created_by) VALUES (".(int)$legal_entity_id.$establishment_value.",".db_escape($identity['work_location_code']).",".db_escape($identity['work_location_name'])
        .",'active','greenfield_explicit_admin_input','hrm_work_location_admin',".$actor.")";
    $result = db_query($sql, 'could not create HRM Work Location');
    $work_location_id = $result ? (int)db_insert_id() : 0;
    if ($result && $work_location_id > 0)
        $result = append_hrm_work_location_writer_audit(
            'hrm.work_location.created', $work_location_id, (int)$legal_entity_id,
            array('work_location_code','work_location_name','work_location_status'),
            'hrm_fnd_003_work_location_writer_create'
        ) !== false;
    if ($result && $target_establishment_id !== null)
        $result = append_hrm_work_location_establishment_binding_audit(
            $work_location_id, (int)$legal_entity_id, null, $target_establishment_id
        ) !== false;
    if ($result) {
        commit_transaction();
        return $work_location_id;
    }
    cancel_transaction();
    return false;
}

function update_hrm_work_location($work_location_id, $code, $name, $status,
    $establishment_id = null, $establishment_explicit = false)
{
    global $path_to_root;
    if (!hrm_work_location_writer_authorized() || !hrm_work_location_table_ready()
        || (int)$work_location_id <= 0)
        return false;
    $identity = normalize_hrm_work_location_identity($code, $name, $status);
    if ($identity === false)
        return false;
    if ($establishment_explicit && !hrm_work_location_establishment_column_ready())
        return false;
    $requested_establishment_id = $establishment_explicit
        ? normalize_hrm_work_location_establishment_target($establishment_id) : null;
    if ($establishment_explicit && $requested_establishment_id === false)
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    if ($establishment_explicit && !function_exists('get_hrm_verified_work_location_establishment_binding'))
        include_once($path_to_root.'/hrm/includes/db/establishment_db.inc');
    begin_transaction();
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    $current = $legal_entity_id === false ? false : get_hrm_work_location((int)$work_location_id, true);
    if ($legal_entity_id === false || !hrm_work_location_row_owned_by_admin_writer($current, (int)$legal_entity_id)) {
        cancel_transaction();
        return false;
    }
    $current_establishment_id = isset($current['establishment_id']) && $current['establishment_id'] !== null
        && $current['establishment_id'] !== '' ? (int)$current['establishment_id'] : null;
    $target_establishment_id = $establishment_explicit ? $requested_establishment_id : $current_establishment_id;
    if ($target_establishment_id !== null && !function_exists('get_hrm_verified_work_location_establishment_binding'))
        include_once($path_to_root.'/hrm/includes/db/establishment_db.inc');
    if ($target_establishment_id !== null) {
        $require_active = $target_establishment_id !== $current_establishment_id;
        if (!function_exists('get_hrm_verified_work_location_establishment_binding')
            || get_hrm_verified_work_location_establishment_binding(
                $target_establishment_id, (int)$legal_entity_id, true, $require_active
            ) === false) {
            cancel_transaction();
            return false;
        }
    }
    $duplicate = get_hrm_work_location_by_code((int)$legal_entity_id, $identity['work_location_code'], true);
    if ($duplicate === false || (is_array($duplicate) && (int)$duplicate['work_location_id'] !== (int)$work_location_id)) {
        cancel_transaction();
        return false;
    }
    $identity_changed = array();
    foreach (array('work_location_code','work_location_name','work_location_status') as $field)
        if ((string)$current[$field] !== (string)$identity[$field])
            $identity_changed[] = $field;
    $relationship_changed = $target_establishment_id !== $current_establishment_id;
    if (empty($identity_changed) && !$relationship_changed) {
        commit_transaction();
        return true;
    }
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $establishment_update = $establishment_explicit
        ? ",establishment_id=".($target_establishment_id === null ? 'NULL' : (int)$target_establishment_id) : '';
    $sql = "UPDATE ".TB_PREF."hrm_work_locations SET work_location_code=".db_escape($identity['work_location_code'])
        .",work_location_name=".db_escape($identity['work_location_name'])
        .",work_location_status=".db_escape($identity['work_location_status'])
        .$establishment_update
        .",updated_at=NOW(),updated_by=".$actor." WHERE work_location_id=".(int)$work_location_id
        ." AND legal_entity_id=".(int)$legal_entity_id;
    $result = db_query($sql, 'could not update HRM Work Location');
    if ($result && !empty($identity_changed))
        $result = append_hrm_work_location_writer_audit(
            'hrm.work_location.updated', (int)$work_location_id, (int)$legal_entity_id, $identity_changed,
            'hrm_fnd_003_work_location_writer_update'
        ) !== false;
    if ($result && $relationship_changed)
        $result = append_hrm_work_location_establishment_binding_audit(
            (int)$work_location_id, (int)$legal_entity_id, $current_establishment_id, $target_establishment_id
        ) !== false;
    if ($result) {
        commit_transaction();
        return true;
    }
    cancel_transaction();
    return false;
}

function delete_hrm_work_location($work_location_id)
{
    return false;
}
