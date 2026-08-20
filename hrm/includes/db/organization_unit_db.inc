<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 bounded Organization Unit compatibility foundation.
 *
 * Legacy departments are the only currently verified management-hierarchy
 * source. Financial dimensions/cost centres, warehouses/stock locations and
 * customer branches are deliberately not reinterpreted as HR organization,
 * establishment or work-location identities.
 */

/** @return bool */
function hrm_organization_unit_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_organization_units')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Normalize one legacy department into the exact compatibility projection.
 * Cost-centre and manager values are retained only as unverified legacy
 * references; they are not promoted to normalized costing/manager semantics.
 *
 * @param array $department
 * @param int $legal_entity_id
 * @return array|false
 */
function hrm_organization_unit_projection_from_department($department, $legal_entity_id)
{
    if (!is_array($department) || (int)$legal_entity_id <= 0)
        return false;
    $department_id = isset($department['department_id']) ? (int)$department['department_id'] : 0;
    $name = isset($department['department_name']) ? trim((string)$department['department_name']) : '';
    if ($department_id <= 0 || $name === '')
        return false;

    $parent = isset($department['parent_department_id']) ? $department['parent_department_id'] : null;
    if ($parent === null || $parent === '' || (int)$parent === 0)
        $parent = null;
    elseif (!is_numeric($parent) || (string)(int)$parent !== trim((string)$parent) || (int)$parent <= 0)
        return false;
    else
        $parent = (int)$parent;
    if ($parent !== null && $parent === $department_id)
        return false;

    $code = isset($department['department_code']) ? trim((string)$department['department_code']) : '';
    if (strlen($code) > 20)
        return false;
    if ($code === '')
        $code = null;

    $manager = isset($department['manager_employee_id']) ? trim((string)$department['manager_employee_id']) : '';
    if (strlen($manager) > 20)
        return false;
    if ($manager === '')
        $manager = null;

    $cost_center = isset($department['cost_center_id']) ? $department['cost_center_id'] : 0;
    if (!is_numeric($cost_center) || (string)(int)$cost_center !== trim((string)$cost_center) || (int)$cost_center < 0)
        return false;

    $inactive = isset($department['inactive']) ? (int)$department['inactive'] : 0;
    if ($inactive !== 0 && $inactive !== 1)
        return false;

    return array(
        'legal_entity_id' => (int)$legal_entity_id,
        'legacy_department_id' => $department_id,
        'unit_code' => 'DEPT-'.$department_id,
        'legacy_department_code' => $code,
        'unit_name' => $name,
        'parent_legacy_department_id' => $parent,
        'legacy_manager_employee_id' => $manager,
        'legacy_cost_center_id' => (int)$cost_center,
        'unit_status' => $inactive ? 'inactive' : 'active',
        'reference_quality' => 'legacy_department_only',
        'source' => 'legacy_department_seed',
    );
}

/** @return array|false|null */
function get_hrm_organization_unit_by_legacy_department($department_id, $for_update = false)
{
    if (!hrm_organization_unit_table_ready() || (int)$department_id <= 0)
        return false;
    $sql = "SELECT organization_unit_id,legal_entity_id,legacy_department_id,unit_code,legacy_department_code,"
        ."unit_name,parent_organization_unit_id,legacy_manager_employee_id,legacy_cost_center_id,unit_status,"
        ."reference_quality,source,created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_organization_units "
        ."WHERE legacy_department_id=".(int)$department_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Organization Unit compatibility row');
    if (!$result)
        return false;
    if (db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/** @return int */
function hrm_organization_unit_actor_id()
{
    if (function_exists('user_id'))
        return (int)user_id();
    if (isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user))
        return (int)$_SESSION['wa_current_user']->user;
    return 0;
}

/**
 * Compare one persisted Organization Unit with the exact Department projection.
 *
 * @param array $row
 * @param array $projection
 * @param int|null $parent_organization_unit_id
 * @return bool
 */
function hrm_organization_unit_row_matches_projection($row, $projection, $parent_organization_unit_id)
{
    if (!is_array($row) || !is_array($projection))
        return false;
    $nullable = array('legacy_department_code', 'legacy_manager_employee_id');
    foreach ($nullable as $field) {
        $left = ($row[$field] === null || $row[$field] === '') ? null : (string)$row[$field];
        $right = ($projection[$field] === null || $projection[$field] === '') ? null : (string)$projection[$field];
        if ($left !== $right)
            return false;
    }
    $row_parent = ($row['parent_organization_unit_id'] === null || $row['parent_organization_unit_id'] === '')
        ? null : (int)$row['parent_organization_unit_id'];
    $expected_parent = ($parent_organization_unit_id === null || $parent_organization_unit_id === '')
        ? null : (int)$parent_organization_unit_id;
    return (int)$row['legal_entity_id'] === (int)$projection['legal_entity_id']
        && (int)$row['legacy_department_id'] === (int)$projection['legacy_department_id']
        && (string)$row['unit_code'] === (string)$projection['unit_code']
        && (string)$row['unit_name'] === (string)$projection['unit_name']
        && $row_parent === $expected_parent
        && (int)$row['legacy_cost_center_id'] === (int)$projection['legacy_cost_center_id']
        && (string)$row['unit_status'] === (string)$projection['unit_status']
        && (string)$row['reference_quality'] === 'legacy_department_only'
        && (string)$row['source'] === 'legacy_department_seed';
}

/**
 * Load one Department row using the normalized writer's deterministic lock path.
 *
 * @param int $department_id
 * @param bool $for_update
 * @return array|false|null
 */
function get_hrm_department_for_organization_unit($department_id, $for_update = false)
{
    if ((int)$department_id <= 0)
        return false;
    $sql = "SELECT department_id,department_code,department_name,parent_department_id,"
        ."manager_employee_id,cost_center_id,inactive FROM ".TB_PREF."departments "
        ."WHERE department_id=".(int)$department_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read legacy Department for Organization Unit binding');
    if (!$result)
        return false;
    if (db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Resolve an exact Department-backed Organization Unit under a persisted Legal Entity.
 * Zero and accepted negative legacy references remain explicit unknown/unbound state.
 *
 * @param mixed $department_id
 * @param int $legal_entity_id
 * @param bool $for_update
 * @param bool $allow_legacy_irregular
 * @return int|false|null
 */
function get_hrm_verified_organization_unit_binding($department_id, $legal_entity_id,
    $for_update = false, $allow_legacy_irregular = false)
{
    if (!is_numeric($department_id) || (string)(int)$department_id !== trim((string)$department_id)
        || (int)$legal_entity_id <= 0)
        return false;
    $department_id = (int)$department_id;
    if ($department_id === 0 || ($department_id < 0 && $allow_legacy_irregular))
        return null;
    if ($department_id < 0 || !hrm_organization_unit_table_ready())
        return false;
    $department = get_hrm_department_for_organization_unit($department_id, $for_update);
    $unit = get_hrm_organization_unit_by_legacy_department($department_id, $for_update);
    if (!is_array($department) || !is_array($unit))
        return false;
    $projection = hrm_organization_unit_projection_from_department($department, $legal_entity_id);
    if (!$projection)
        return false;
    $parent_id = null;
    if ($projection['parent_legacy_department_id'] !== null) {
        $parent = get_hrm_organization_unit_by_legacy_department(
            (int)$projection['parent_legacy_department_id'], $for_update
        );
        if (!is_array($parent) || (int)$parent['legal_entity_id'] !== (int)$legal_entity_id)
            return false;
        $parent_id = (int)$parent['organization_unit_id'];
    }
    if (!hrm_organization_unit_row_matches_projection($unit, $projection, $parent_id))
        return false;
    return (int)$unit['organization_unit_id'] > 0 ? (int)$unit['organization_unit_id'] : false;
}

/**
 * Append one restricted-value-free Department writer synchronization event.
 *
 * @param string $action
 * @param int $organization_unit_id
 * @param int $department_id
 * @param int $legal_entity_id
 * @param array $changed_fields
 * @param string $reason_code
 * @return array|false
 */
function append_hrm_organization_unit_writer_audit($action, $organization_unit_id,
    $department_id, $legal_entity_id, $changed_fields, $reason_code)
{
    if (!function_exists('append_domain_audit_event') || (int)$organization_unit_id <= 0
        || (int)$department_id <= 0 || (int)$legal_entity_id <= 0
        || !is_array($changed_fields) || trim((string)$action) === ''
        || trim((string)$reason_code) === '')
        return false;
    $scope = get_hrm_organization_unit_by_legacy_department($department_id, true);
    if (!is_array($scope) || (int)$scope['organization_unit_id'] !== (int)$organization_unit_id
        || (int)$scope['legal_entity_id'] !== (int)$legal_entity_id)
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$scope['legal_entity_id'],
        'entity_type'=>'hrm_organization_unit',
        'entity_key'=>array(
            'organization_unit_id'=>(int)$organization_unit_id,
            'legacy_department_id'=>(int)$department_id,
        ),
        'action'=>(string)$action,
        'reason_code'=>(string)$reason_code,
        'before'=>null,
        'after'=>array('department_projection_synchronized'=>true),
        'restricted_diff'=>array(
            'changed_fields'=>$changed_fields,
            'restricted_values_stored'=>false,
        ),
    ));
}

/**
 * Insert or update one exact Department-backed Organization Unit.
 * The caller owns the surrounding Department transaction and authorization.
 *
 * @param int $department_id
 * @param array $changed_fields
 * @param string $action create|update|inactive
 * @return bool
 */
function synchronize_hrm_organization_unit_from_department($department_id,
    $changed_fields, $action)
{
    if ((int)$department_id <= 0 || !is_array($changed_fields)
        || !in_array((string)$action, array('create','update','inactive'), true)
        || !function_exists('get_hrm_default_legal_entity_binding_id'))
        return false;
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
    if ($legal_entity_id === false)
        return false;
    $department = get_hrm_department_for_organization_unit($department_id, true);
    if (!is_array($department))
        return false;
    $projection = hrm_organization_unit_projection_from_department($department, $legal_entity_id);
    if (!$projection)
        return false;
    $parent_id = null;
    if ($projection['parent_legacy_department_id'] !== null) {
        $parent_id = get_hrm_verified_organization_unit_binding(
            (int)$projection['parent_legacy_department_id'], $legal_entity_id, true, false
        );
        if ($parent_id === false || $parent_id === null)
            return false;
    }
    $existing = get_hrm_organization_unit_by_legacy_department($department_id, true);
    $actor = hrm_organization_unit_actor_id();
    if ($action === 'create') {
        if ($existing !== null)
            return false;
        $sql = "INSERT INTO ".TB_PREF."hrm_organization_units (legal_entity_id,legacy_department_id,"
            ."unit_code,legacy_department_code,unit_name,parent_organization_unit_id,legacy_manager_employee_id,"
            ."legacy_cost_center_id,unit_status,reference_quality,source,created_by) VALUES ("
            .(int)$legal_entity_id.",".(int)$department_id.",".db_escape($projection['unit_code']).","
            .($projection['legacy_department_code'] === null ? 'NULL' : db_escape($projection['legacy_department_code'])).","
            .db_escape($projection['unit_name']).",".($parent_id === null ? 'NULL' : (int)$parent_id).","
            .($projection['legacy_manager_employee_id'] === null ? 'NULL' : db_escape($projection['legacy_manager_employee_id'])).","
            .(int)$projection['legacy_cost_center_id'].",".db_escape($projection['unit_status'])
            .",'legacy_department_only','legacy_department_seed',".(int)$actor.")";
        if (!db_query($sql, 'could not create synchronized HRM Organization Unit'))
            return false;
        $organization_unit_id = (int)db_insert_id();
        $audit_action = 'hrm.organization_unit.department_created';
        $reason_code = 'hrm_fnd_003_department_writer_created';
    } else {
        if (!is_array($existing) || (int)$existing['legal_entity_id'] !== (int)$legal_entity_id)
            return false;
        $organization_unit_id = (int)$existing['organization_unit_id'];
        $sql = "UPDATE ".TB_PREF."hrm_organization_units SET unit_code=".db_escape($projection['unit_code'])
            .",legacy_department_code=".($projection['legacy_department_code'] === null ? 'NULL' : db_escape($projection['legacy_department_code']))
            .",unit_name=".db_escape($projection['unit_name'])
            .",parent_organization_unit_id=".($parent_id === null ? 'NULL' : (int)$parent_id)
            .",legacy_manager_employee_id=".($projection['legacy_manager_employee_id'] === null ? 'NULL' : db_escape($projection['legacy_manager_employee_id']))
            .",legacy_cost_center_id=".(int)$projection['legacy_cost_center_id']
            .",unit_status=".db_escape($projection['unit_status'])
            .",updated_at=NOW(),updated_by=".(int)$actor
            ." WHERE organization_unit_id=".(int)$organization_unit_id
            ." AND legal_entity_id=".(int)$legal_entity_id;
        if (!db_query($sql, 'could not update synchronized HRM Organization Unit'))
            return false;
        $audit_action = $action === 'inactive'
            ? 'hrm.organization_unit.department_status_changed'
            : 'hrm.organization_unit.department_updated';
        $reason_code = $action === 'inactive'
            ? 'hrm_fnd_003_department_writer_status_changed'
            : 'hrm_fnd_003_department_writer_updated';
    }
    $updated = get_hrm_organization_unit_by_legacy_department($department_id, true);
    if (!is_array($updated)
        || !hrm_organization_unit_row_matches_projection($updated, $projection, $parent_id))
        return false;
    return append_hrm_organization_unit_writer_audit(
        $audit_action, $organization_unit_id, $department_id, $legal_entity_id,
        $changed_fields, $reason_code
    ) !== false;
}
