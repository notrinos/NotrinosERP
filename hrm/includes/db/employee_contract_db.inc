<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-002 bounded Contract compatibility foundation.
 *
 * Legacy employees remain authoritative for current UI/payroll behavior. This
 * shadow binds the legacy employment_type to the accepted current Employment
 * effective interval. It does not infer signed-document terms, contractual
 * expiry, legal entity, Assignment, compensation, hours/FTE or rehire.
 */

/** @return bool */
function hrm_contract_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function')
        || !defined('TB_PREF'))
        return false;
    // Keep the runtime TB_PREF placeholder outside value escaping. In browser
    // sessions TB_PREF is '&TB_PREF&' and db_query() replaces that token with
    // the selected company prefix. Escaping the full prefixed name first would
    // encode the placeholder and make an installed foundation table appear absent.
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_contracts')."'");
    return $result && db_num_rows($result) === 1;
}

/** @return int */
function hrm_contract_actor_id()
{
    if (function_exists('user_id'))
        return (int)user_id();
    if (isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user))
        return (int)$_SESSION['wa_current_user']->user;
    return 0;
}

/**
 * Map only the finite legacy employment-type enum. Unknown historical numeric
 * values can be retained explicitly during migration/no-op reconciliation but
 * are not accepted as new runtime semantics.
 *
 * @param mixed $value
 * @param bool $allow_legacy_unknown
 * @return array|false
 */
function hrm_contract_type_projection($value, $allow_legacy_unknown = false)
{
    if (!is_numeric($value) || (string)(int)$value !== trim((string)$value))
        return false;
    $legacy_type = (int)$value;
    $types = array(
        0 => 'legacy_permanent',
        1 => 'legacy_contract',
        2 => 'legacy_probation',
        3 => 'legacy_intern',
        4 => 'legacy_parttime',
    );
    if (isset($types[$legacy_type])) {
        return array(
            'legacy_employment_type' => $legacy_type,
            'contract_type_code' => $types[$legacy_type],
            'type_quality' => 'exact',
        );
    }
    if (!$allow_legacy_unknown)
        return false;
    return array(
        'legacy_employment_type' => $legacy_type,
        'contract_type_code' => 'legacy_unknown',
        'type_quality' => 'legacy_unknown',
    );
}

/**
 * Build one Contract version from an employee and its accepted Employment.
 * Effective dates are compatibility/version dates inherited from Employment;
 * they are not asserted to be signed contract start/expiry dates.
 *
 * @param array $employee
 * @param array $employment
 * @param bool $allow_legacy_unknown
 * @return array|false
 */
function hrm_contract_projection_from_employee($employee, $employment, $allow_legacy_unknown = false)
{
    if (!is_array($employee) || !is_array($employment)
        || empty($employee['employee_number']) || empty($employment['employment_id']))
        return false;
    $employee_number = (int)$employee['employee_number'];
    if ((int)$employment['legacy_employee_number'] !== $employee_number
        || (int)$employment['legacy_current'] !== 1
        || (int)$employment['employment_sequence'] !== 1)
        return false;
    $type = hrm_contract_type_projection(
        isset($employee['employment_type']) ? $employee['employment_type'] : null,
        $allow_legacy_unknown
    );
    if (!$type)
        return false;
    return array(
        'employment_id' => (int)$employment['employment_id'],
        'legacy_employee_number' => $employee_number,
        'legacy_employment_type' => $type['legacy_employment_type'],
        'contract_type_code' => $type['contract_type_code'],
        'type_quality' => $type['type_quality'],
        'effective_from' => ($employment['start_date'] === null || $employment['start_date'] === '')
            ? null : (string)$employment['start_date'],
        'effective_to' => ($employment['end_date'] === null || $employment['end_date'] === '')
            ? null : (string)$employment['end_date'],
        'interval_quality' => (string)$employment['interval_quality'],
    );
}

/**
 * Internal current-Contract compatibility lookup. Not a new UI/API authority.
 *
 * @param int $employee_number
 * @param bool $for_update
 * @return array|false
 */
function get_hrm_legacy_current_contract($employee_number, $for_update = false)
{
    if (!hrm_contract_table_ready() || (int)$employee_number <= 0)
        return false;
    $sql = "SELECT c.contract_id,c.employment_id,c.contract_sequence,c.legacy_employee_number,"
        ."c.legacy_current,c.legacy_employment_type,c.contract_type_code,c.type_quality,"
        ."c.effective_from,c.effective_to,c.interval_quality,c.source,c.created_by,c.updated_by "
        ."FROM ".TB_PREF."hrm_contracts c INNER JOIN ".TB_PREF."hrm_employments h "
        ."ON h.employment_id=c.employment_id WHERE c.legacy_employee_number=".(int)$employee_number." "
        ."AND c.legacy_current=1 AND h.legacy_employee_number=".(int)$employee_number." "
        ."AND h.legacy_current=1 ORDER BY c.contract_id DESC LIMIT 1";
    if ($for_update)
        $sql .= " FOR UPDATE";
    $result = db_query($sql, 'could not read HRM contract compatibility row');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param mixed $a @param mixed $b @return bool */
function hrm_contract_nullable_equal($a, $b)
{
    if ($a === null || $a === '')
        $a = null;
    if ($b === null || $b === '')
        $b = null;
    return $a === $b;
}

/**
 * Append restricted-value-free Contract shadow evidence.
 *
 * @return array|false
 */
function append_hrm_contract_domain_audit($action, $contract_id, $employment_id,
    $employee_number, $changed_fields, $reason_code)
{
    if (!function_exists('append_domain_audit_event') || (int)$contract_id <= 0
        || (int)$employment_id <= 0 || (int)$employee_number <= 0
        || !is_array($changed_fields) || trim((string)$action) === ''
        || trim((string)$reason_code) === '')
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => 0,
        'entity_type' => 'hrm_contract_shadow',
        'entity_key' => array(
            'contract_id' => (int)$contract_id,
            'employment_id' => (int)$employment_id,
            'employee_number' => (int)$employee_number,
        ),
        'action' => (string)$action,
        'reason_code' => (string)$reason_code,
        'before' => null,
        'after' => array('contract_projection_changed' => true),
        'restricted_diff' => array(
            'changed_fields' => $changed_fields,
            'restricted_values_stored' => false,
        ),
    ));
}

/** @return int|false */
function create_hrm_contract_from_employee($employee, $audit = true,
    $source = 'runtime_employee', $allow_legacy_unknown = false)
{
    if (!hrm_contract_table_ready() || !function_exists('get_hrm_legacy_current_employment'))
        return false;
    $employee_number = isset($employee['employee_number']) ? (int)$employee['employee_number'] : 0;
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    if (!$employment)
        return false;
    $projection = hrm_contract_projection_from_employee($employee, $employment, $allow_legacy_unknown);
    if (!$projection || get_hrm_legacy_current_contract($employee_number, true))
        return false;

    $actor = hrm_contract_actor_id();
    $sql = "INSERT INTO ".TB_PREF."hrm_contracts (employment_id,contract_sequence,"
        ."legacy_employee_number,legacy_current,legacy_employment_type,contract_type_code,type_quality,"
        ."effective_from,effective_to,interval_quality,source,created_by) VALUES ("
        .(int)$projection['employment_id'].",1,".$employee_number.",1,"
        .(int)$projection['legacy_employment_type'].",".db_escape($projection['contract_type_code']).","
        .db_escape($projection['type_quality']).","
        .($projection['effective_from'] === null ? 'NULL' : db_escape($projection['effective_from'])).","
        .($projection['effective_to'] === null ? 'NULL' : db_escape($projection['effective_to'])).","
        .db_escape($projection['interval_quality']).",".db_escape((string)$source).",".(int)$actor.")";
    if (!db_query($sql, 'could not create HRM contract shadow'))
        return false;
    $contract_id = (int)db_insert_id();
    if ($contract_id <= 0)
        return false;
    if ($audit && !append_hrm_contract_domain_audit(
        'hrm.contract.created', $contract_id, (int)$projection['employment_id'], $employee_number,
        array('contract_type', 'effective_interval', 'legacy_current_projection'),
        'hrm_fnd_002_contract_created'
    ))
        return false;
    return $contract_id;
}

/** @return bool */
function synchronize_hrm_contract_from_employee($employee)
{
    if (!is_array($employee) || empty($employee['employee_number']) || !hrm_contract_table_ready())
        return false;
    $employee_number = (int)$employee['employee_number'];
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    if (!$employment)
        return false;
    $current = get_hrm_legacy_current_contract($employee_number, true);
    if (!$current)
        return create_hrm_contract_from_employee($employee, true, 'runtime_employee') !== false;
    if ((int)$current['employment_id'] !== (int)$employment['employment_id']
        || (int)$current['contract_sequence'] !== 1
        || (int)$current['legacy_current'] !== 1
        || (int)$current['legacy_employee_number'] !== $employee_number)
        return false;

    $projection = hrm_contract_projection_from_employee($employee, $employment, false);
    if (!$projection) {
        $legacy_projection = hrm_contract_projection_from_employee($employee, $employment, true);
        if (!$legacy_projection || $legacy_projection['type_quality'] !== 'legacy_unknown')
            return false;
        if ((int)$current['legacy_employment_type'] !== (int)$legacy_projection['legacy_employment_type']
            || (string)$current['contract_type_code'] !== 'legacy_unknown'
            || (string)$current['type_quality'] !== 'legacy_unknown')
            return false;
        $projection = $legacy_projection;
    }

    $changed = array();
    foreach (array('effective_from', 'effective_to') as $field) {
        if (!hrm_contract_nullable_equal($current[$field], $projection[$field]))
            $changed[] = $field;
    }
    foreach (array('legacy_employment_type', 'contract_type_code', 'type_quality', 'interval_quality') as $field) {
        if ((string)$current[$field] !== (string)$projection[$field])
            $changed[] = $field;
    }
    if (empty($changed))
        return true;

    $actor = hrm_contract_actor_id();
    $sql = "UPDATE ".TB_PREF."hrm_contracts SET "
        ."legacy_employment_type=".(int)$projection['legacy_employment_type'].","
        ."contract_type_code=".db_escape($projection['contract_type_code']).","
        ."type_quality=".db_escape($projection['type_quality']).","
        ."effective_from=".($projection['effective_from'] === null ? 'NULL' : db_escape($projection['effective_from'])).","
        ."effective_to=".($projection['effective_to'] === null ? 'NULL' : db_escape($projection['effective_to'])).","
        ."interval_quality=".db_escape($projection['interval_quality']).","
        ."updated_at=UTC_TIMESTAMP(),updated_by=".(int)$actor
        ." WHERE contract_id=".(int)$current['contract_id']." AND legacy_current=1";
    if (!db_query($sql, 'could not synchronize HRM contract shadow'))
        return false;
    return append_hrm_contract_domain_audit(
        'hrm.contract.synchronized', (int)$current['contract_id'], (int)$employment['employment_id'],
        $employee_number, $changed, 'hrm_fnd_002_legacy_contract_projection_sync'
    ) !== false;
}

/**
 * Staged-deployment bridge: before the additive Contract table exists, legacy
 * writes continue and migration seeds deterministically. Once it exists,
 * Contract synchronization is mandatory in the existing employee transaction.
 *
 * @return bool
 */
function synchronize_hrm_contract_from_employee_if_ready($employee, $create = false)
{
    if (!hrm_contract_table_ready())
        return true;
    if ($create)
        return create_hrm_contract_from_employee($employee, true, 'runtime_employee') !== false;
    return synchronize_hrm_contract_from_employee($employee);
}
