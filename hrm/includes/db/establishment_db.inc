<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 normalized Establishment identity and explicit administrator writer.
 *
 * The accepted dependency review found no trustworthy reusable legacy
 * Establishment identity. Customer branches remain sales entities, inventory
 * locations remain warehouse/stock locations, financial dimensions remain
 * accounting allocations, and Departments remain Organization Units. This
 * writer therefore accepts only explicit HRM administrator input scoped to the
 * exact compatibility Legal Entity. Work Location binding, Assignment binding,
 * jurisdiction, address, filing, timezone/site and payroll-tax semantics remain
 * deliberately deferred.
 */

/** @return bool */
function hrm_establishment_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_establishments')."'");
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_establishment_writer_authorized()
{
    return isset($_SESSION['wa_current_user'])
        && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'can_access')
        && $_SESSION['wa_current_user']->can_access('SA_ESTABLISHMENT');
}

/** @param string $value @return int */
function hrm_establishment_text_length($value)
{
    return function_exists('mb_strlen') ? (int)mb_strlen((string)$value, 'UTF-8') : strlen((string)$value);
}

/**
 * Validate and normalize only structural Establishment identity fields.
 * Codes remain explicit administrator input; no legacy value is inferred.
 *
 * @return array|false
 */
function normalize_hrm_establishment_identity($code, $name, $status = 'active')
{
    $code = trim((string)$code);
    $name = trim((string)$name);
    $status = trim((string)$status);
    if ($code === '' || $name === '' || hrm_establishment_text_length($code) > 32
        || hrm_establishment_text_length($name) > 120
        || preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/D', $code) !== 1
        || !in_array($status, array('active','inactive'), true))
        return false;
    return array(
        'establishment_code'=>$code,
        'establishment_name'=>$name,
        'establishment_status'=>$status,
    );
}

/** Read one normalized Establishment identity. */
function get_hrm_establishment($establishment_id, $for_update = false)
{
    if (!hrm_establishment_table_ready() || (int)$establishment_id <= 0)
        return false;
    $sql = "SELECT establishment_id,legal_entity_id,establishment_code,establishment_name,establishment_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_establishments WHERE establishment_id=".(int)$establishment_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Establishment');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

function get_hrm_establishment_by_code($legal_entity_id, $code, $for_update = false)
{
    if (!hrm_establishment_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $identity = normalize_hrm_establishment_identity($code, 'placeholder', 'active');
    if ($identity === false)
        return false;
    $sql = "SELECT establishment_id,legal_entity_id,establishment_code,establishment_name,establishment_status,reference_quality,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_establishments WHERE legal_entity_id=".(int)$legal_entity_id
        ." AND establishment_code=".db_escape($identity['establishment_code']);
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Establishment by code');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

function get_hrm_establishments_for_legal_entity($legal_entity_id, $active_only = true)
{
    if (!hrm_establishment_table_ready() || (int)$legal_entity_id <= 0)
        return false;
    $sql = "SELECT establishment_id,legal_entity_id,establishment_code,establishment_name,establishment_status,reference_quality,source "
        ."FROM ".TB_PREF."hrm_establishments WHERE legal_entity_id=".(int)$legal_entity_id;
    if ($active_only)
        $sql .= " AND establishment_status='active'";
    $sql .= ' ORDER BY establishment_code,establishment_id';
    return db_query($sql, 'could not list HRM Establishments');
}

function hrm_establishment_row_owned_by_admin_writer($row, $legal_entity_id)
{
    return is_array($row)
        && (int)$row['establishment_id'] > 0
        && (int)$row['legal_entity_id'] === (int)$legal_entity_id
        && (string)$row['reference_quality'] === 'greenfield_explicit_admin_input'
        && (string)$row['source'] === 'hrm_establishment_admin';
}

/**
 * Resolve one explicit Work Location relationship target. Relationship writes
 * are authorized by the Work Location writer, not by SA_ESTABLISHMENT; this
 * helper validates only target identity/provenance/scope/state. New bindings
 * require active targets, while an existing binding may be preserved after
 * later Establishment inactivation.
 *
 * @return int|false
 */
function get_hrm_verified_work_location_establishment_binding($establishment_id, $legal_entity_id,
    $for_update = false, $require_active = true)
{
    if (!is_numeric($establishment_id)
        || (string)(int)$establishment_id !== trim((string)$establishment_id)
        || (int)$establishment_id <= 0 || (int)$legal_entity_id <= 0)
        return false;
    $row = get_hrm_establishment((int)$establishment_id, $for_update);
    if (!hrm_establishment_row_owned_by_admin_writer($row, (int)$legal_entity_id))
        return false;
    if ($require_active && (string)$row['establishment_status'] !== 'active')
        return false;
    return (int)$row['establishment_id'];
}

function append_hrm_establishment_writer_audit($action, $establishment_id, $legal_entity_id,
    $changed_fields, $reason_code)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!function_exists('append_domain_audit_event') || (int)$establishment_id <= 0
        || (int)$legal_entity_id <= 0 || !is_array($changed_fields)
        || trim((string)$action) === '' || trim((string)$reason_code) === '')
        return false;
    $scope = get_hrm_establishment($establishment_id, true);
    if (!hrm_establishment_row_owned_by_admin_writer($scope, $legal_entity_id))
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_establishment',
        'entity_key'=>array('establishment_id'=>(int)$establishment_id),
        'action'=>(string)$action,
        'reason_code'=>(string)$reason_code,
        'before'=>null,
        'after'=>array('explicit_admin_identity_written'=>true),
        'restricted_diff'=>array(
            'changed_fields'=>$changed_fields,
            'work_location_binding_changed'=>false,
            'assignment_binding_changed'=>false,
            'jurisdiction_semantics_changed'=>false,
            'address_semantics_changed'=>false,
            'filing_identifiers_changed'=>false,
            'timezone_site_semantics_changed'=>false,
            'payroll_tax_routing_changed'=>false,
            'restricted_values_stored'=>false,
        ),
    ));
}

function add_hrm_establishment($code, $name)
{
    global $path_to_root;
    if (!hrm_establishment_writer_authorized() || !hrm_establishment_table_ready())
        return false;
    $identity = normalize_hrm_establishment_identity($code, $name, 'active');
    if ($identity === false)
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    begin_transaction();
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    $duplicate = $legal_entity_id === false ? false
        : get_hrm_establishment_by_code((int)$legal_entity_id, $identity['establishment_code'], true);
    if ($legal_entity_id === false || $duplicate === false || is_array($duplicate)) {
        cancel_transaction();
        return false;
    }
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $sql = "INSERT INTO ".TB_PREF."hrm_establishments (legal_entity_id,establishment_code,establishment_name,establishment_status,"
        ."reference_quality,source,created_by) VALUES (".(int)$legal_entity_id.",".db_escape($identity['establishment_code']).",".db_escape($identity['establishment_name'])
        .",'active','greenfield_explicit_admin_input','hrm_establishment_admin',".$actor.")";
    $result = db_query($sql, 'could not create HRM Establishment');
    $establishment_id = $result ? (int)db_insert_id() : 0;
    if ($result && $establishment_id > 0)
        $result = append_hrm_establishment_writer_audit(
            'hrm.establishment.created', $establishment_id, (int)$legal_entity_id,
            array('establishment_code','establishment_name','establishment_status'),
            'hrm_fnd_003_establishment_writer_create'
        ) !== false;
    if ($result) {
        commit_transaction();
        return $establishment_id;
    }
    cancel_transaction();
    return false;
}

function update_hrm_establishment($establishment_id, $code, $name, $status)
{
    global $path_to_root;
    if (!hrm_establishment_writer_authorized() || !hrm_establishment_table_ready()
        || (int)$establishment_id <= 0)
        return false;
    $identity = normalize_hrm_establishment_identity($code, $name, $status);
    if ($identity === false)
        return false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
    begin_transaction();
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    $current = $legal_entity_id === false ? false : get_hrm_establishment((int)$establishment_id, true);
    if ($legal_entity_id === false || !hrm_establishment_row_owned_by_admin_writer($current, (int)$legal_entity_id)) {
        cancel_transaction();
        return false;
    }
    $duplicate = get_hrm_establishment_by_code((int)$legal_entity_id, $identity['establishment_code'], true);
    if ($duplicate === false || (is_array($duplicate) && (int)$duplicate['establishment_id'] !== (int)$establishment_id)) {
        cancel_transaction();
        return false;
    }
    $changed = array();
    foreach (array('establishment_code','establishment_name','establishment_status') as $field)
        if ((string)$current[$field] !== (string)$identity[$field])
            $changed[] = $field;
    if (empty($changed)) {
        commit_transaction();
        return true;
    }
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $sql = "UPDATE ".TB_PREF."hrm_establishments SET establishment_code=".db_escape($identity['establishment_code'])
        .",establishment_name=".db_escape($identity['establishment_name'])
        .",establishment_status=".db_escape($identity['establishment_status'])
        .",updated_at=NOW(),updated_by=".$actor." WHERE establishment_id=".(int)$establishment_id
        ." AND legal_entity_id=".(int)$legal_entity_id;
    $result = db_query($sql, 'could not update HRM Establishment');
    if ($result)
        $result = append_hrm_establishment_writer_audit(
            'hrm.establishment.updated', (int)$establishment_id, (int)$legal_entity_id, $changed,
            'hrm_fnd_003_establishment_writer_update'
        ) !== false;
    if ($result) {
        commit_transaction();
        return true;
    }
    cancel_transaction();
    return false;
}

function delete_hrm_establishment($establishment_id)
{
    return false;
}
