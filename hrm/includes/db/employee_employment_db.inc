<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-002 bounded Employment compatibility foundation.
 *
 * Legacy employees remain authoritative for current UI/payroll behavior. This
 * shadow stores only the current legal-employment interval required to begin
 * effective-dated Employment history. Contract and Assignment remain separate
 * bounded projections; HRM-FND-003 now binds legacy-compatible Employment to
 * the exact company-default Legal Entity without introducing organization,
 * location or payroll-relationship semantics.
 */

/** @return bool */
function hrm_employment_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function')
        || !defined('TB_PREF'))
        return false;
    // Keep the runtime TB_PREF placeholder outside value escaping. In browser
    // sessions TB_PREF is '&TB_PREF&' and db_query() replaces that token with
    // the selected company prefix. Escaping the full prefixed name first would
    // encode the placeholder and make an installed foundation table appear absent.
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_employments')."'");
    return $result && db_num_rows($result) === 1;
}

/** @return int */
function hrm_employment_actor_id()
{
    if (function_exists('user_id'))
        return (int)user_id();
    if (isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user))
        return (int)$_SESSION['wa_current_user']->user;
    return 0;
}

/**
 * Normalize a legacy SQL date. Zero/blank dates are explicit unknowns.
 *
 * @param mixed $value
 * @return string|null|false
 */
function hrm_employment_normalize_date($value)
{
    if ($value === null)
        return null;
    $value = trim((string)$value);
    if ($value === '' || $value === '0000-00-00')
        return null;
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', $value) !== 1)
        return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    if (!$date || $date->format('Y-m-d') !== $value)
        return false;
    return $value;
}


/**
 * Inclusive date-interval overlap predicate used by the bounded concurrency
 * policy. A NULL start is explicit unknown/past-unbounded history and a NULL
 * end is open-ended. Invalid dates fail closed as overlapping.
 *
 * @param mixed $left_start
 * @param mixed $left_end
 * @param mixed $right_start
 * @param mixed $right_end
 * @return bool
 */
function hrm_employment_intervals_overlap($left_start, $left_end, $right_start, $right_end)
{
    $left_start = hrm_employment_normalize_date($left_start);
    $left_end = hrm_employment_normalize_date($left_end);
    $right_start = hrm_employment_normalize_date($right_start);
    $right_end = hrm_employment_normalize_date($right_end);
    if ($left_start === false || $left_end === false || $right_start === false || $right_end === false)
        return true;
    return ($left_end === null || $right_start === null || strcmp($right_start, $left_end) <= 0)
        && ($right_end === null || $left_start === null || strcmp($left_start, $right_end) <= 0);
}

/**
 * Serialize Employment chronology on the shared Person row. This is the
 * common lock for multiple Worker/legacy employee bindings that may already
 * have been canonically linked to one Person.
 *
 * @param int $person_id
 * @return bool
 */
function hrm_employment_lock_person_for_interval_policy($person_id)
{
    $person_id = (int)$person_id;
    if ($person_id <= 0 || !function_exists('db_query'))
        return false;
    $result = db_query("SELECT person_id FROM ".TB_PREF."hrm_persons WHERE person_id=".$person_id." FOR UPDATE",
        'could not serialize HRM Employment chronology on Person');
    return $result && db_num_rows($result) === 1;
}

/**
 * Load one binding using Person -> Worker/map lock order. The initial unlocked
 * read resolves the common Person; after that Person row is locked, the
 * binding is re-read FOR UPDATE and must still point to the same Person/Worker.
 *
 * @param int $employee_number
 * @return array|false
 */
function hrm_employment_lock_binding_for_interval_policy($employee_number)
{
    if (!function_exists('get_hrm_person_worker_binding') || (int)$employee_number <= 0)
        return false;
    $observed = get_hrm_person_worker_binding((int)$employee_number, false);
    if (!$observed || $observed['retired_at'] !== null || (int)$observed['person_id'] <= 0
        || (int)$observed['worker_id'] <= 0
        || !hrm_employment_lock_person_for_interval_policy((int)$observed['person_id']))
        return false;
    $locked = get_hrm_person_worker_binding((int)$employee_number, true);
    if (!$locked || $locked['retired_at'] !== null
        || (int)$locked['person_id'] !== (int)$observed['person_id']
        || (int)$locked['worker_id'] !== (int)$observed['worker_id'])
        return false;
    return $locked;
}

/** @return bool */
function hrm_employment_interval_rows_are_non_overlapping($rows)
{
    if (!is_array($rows))
        return false;
    $count = count($rows);
    for ($i = 0; $i < $count; $i++) {
        if (!is_array($rows[$i]) || empty($rows[$i]['employment_id']))
            return false;
        $start = hrm_employment_normalize_date(isset($rows[$i]['start_date']) ? $rows[$i]['start_date'] : null);
        $end = hrm_employment_normalize_date(isset($rows[$i]['end_date']) ? $rows[$i]['end_date'] : null);
        if ($start === false || $end === false)
            return false;
        // Preserve one accepted legacy-invalid episode, but never let it
        // participate in a multi-Employment canonical chronology.
        if ($count > 1 && $start !== null && $end !== null && strcmp($end, $start) < 0)
            return false;
        for ($j = $i + 1; $j < $count; $j++) {
            if (!is_array($rows[$j]) || empty($rows[$j]['employment_id']))
                return false;
            if (hrm_employment_intervals_overlap(
                isset($rows[$i]['start_date']) ? $rows[$i]['start_date'] : null,
                isset($rows[$i]['end_date']) ? $rows[$i]['end_date'] : null,
                isset($rows[$j]['start_date']) ? $rows[$j]['start_date'] : null,
                isset($rows[$j]['end_date']) ? $rows[$j]['end_date'] : null
            ))
                return false;
        }
    }
    return true;
}

/**
 * Enforce the current compatibility policy: a canonical Person may retain
 * sequential Employment episodes (including accepted rehire history), but
 * overlapping/concurrent Employment episodes are denied until legal-employer
 * and payroll-relationship routing are explicitly introduced later.
 *
 * The caller must already hold the Person lock through
 * hrm_employment_lock_binding_for_interval_policy().
 *
 * @param array $binding
 * @param int $replacement_employment_id 0 for a new candidate row.
 * @param mixed $candidate_start
 * @param mixed $candidate_end
 * @return bool
 */
function hrm_employment_person_interval_policy_allows($binding, $replacement_employment_id,
    $candidate_start, $candidate_end)
{
    if (!is_array($binding) || (int)$binding['person_id'] <= 0 || (int)$binding['worker_id'] <= 0)
        return false;
    $candidate_start = hrm_employment_normalize_date($candidate_start);
    $candidate_end = hrm_employment_normalize_date($candidate_end);
    if ($candidate_start === false || $candidate_end === false)
        return false;

    $sql = "SELECT h.employment_id,h.worker_id,h.start_date,h.end_date FROM ".TB_PREF."hrm_employments h "
        ."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=h.worker_id "
        ."WHERE w.person_id=".(int)$binding['person_id']." ORDER BY h.employment_id FOR UPDATE";
    $result = db_query($sql, 'could not verify canonical Person Employment intervals');
    if (!$result)
        return false;
    $rows = array();
    $replaced = false;
    while ($row = db_fetch_assoc($result)) {
        if ((int)$replacement_employment_id > 0
            && (int)$row['employment_id'] === (int)$replacement_employment_id) {
            $row['worker_id'] = (int)$binding['worker_id'];
            $row['start_date'] = $candidate_start;
            $row['end_date'] = $candidate_end;
            $replaced = true;
        }
        $rows[] = $row;
    }
    if ((int)$replacement_employment_id > 0 && !$replaced)
        return false;
    if ((int)$replacement_employment_id === 0) {
        $rows[] = array(
            'employment_id' => -1,
            'worker_id' => (int)$binding['worker_id'],
            'start_date' => $candidate_start,
            'end_date' => $candidate_end,
        );
    }
    return hrm_employment_interval_rows_are_non_overlapping($rows);
}

/**
 * Validate the hypothetical Worker rebind performed by canonical Person-link
 * and reviewed rehire execution. Before HRM-FND-002 is installed this returns
 * true, preserving the predecessor deployment bridge. Once Employment exists,
 * every Employment owned by the canonical Person plus the alias Workers must
 * remain pairwise non-overlapping after the rebind.
 *
 * @param int $canonical_person_id
 * @param array $alias_workers
 * @return bool
 */
function hrm_employment_canonical_link_interval_policy_allows($canonical_person_id, $alias_workers)
{
    if (!hrm_employment_table_ready())
        return true;
    $canonical_person_id = (int)$canonical_person_id;
    if ($canonical_person_id <= 0 || !is_array($alias_workers) || empty($alias_workers)
        || !hrm_employment_lock_person_for_interval_policy($canonical_person_id))
        return false;
    $worker_ids = array();
    foreach ($alias_workers as $worker) {
        if (!is_array($worker) || (int)$worker['worker_id'] <= 0)
            return false;
        $worker_ids[(int)$worker['worker_id']] = (int)$worker['worker_id'];
    }
    if (empty($worker_ids))
        return false;
    $sql = "SELECT h.employment_id,h.worker_id,h.start_date,h.end_date FROM ".TB_PREF."hrm_employments h "
        ."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=h.worker_id WHERE w.person_id=".$canonical_person_id
        ." OR h.worker_id IN (".implode(',', $worker_ids).") ORDER BY h.employment_id FOR UPDATE";
    $result = db_query($sql, 'could not verify canonical Person-link Employment intervals');
    if (!$result)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($result))
        $rows[] = $row;
    return hrm_employment_interval_rows_are_non_overlapping($rows);
}

/**
 * Convert one legacy employee row into the bounded Employment projection.
 * Existing invalid historical intervals may be retained only during migration
 * or an exact no-op synchronization; new invalid intervals fail closed.
 *
 * @param array $employee
 * @param bool $allow_legacy_invalid
 * @return array|false
 */
function hrm_employment_projection_from_employee($employee, $allow_legacy_invalid = false)
{
    if (!is_array($employee) || empty($employee['employee_number']))
        return false;

    $start = hrm_employment_normalize_date(isset($employee['hire_date'])
        ? $employee['hire_date'] : null);
    $end = hrm_employment_normalize_date(isset($employee['released_date'])
        ? $employee['released_date'] : null);
    if ($start === false || $end === false)
        return false;

    $quality = $start === null ? 'unknown_start' : 'exact';
    if ($start !== null && $end !== null && strcmp($end, $start) < 0) {
        if (!$allow_legacy_invalid)
            return false;
        $quality = 'legacy_invalid_order';
    }

    if ($end !== null)
        $status = 'terminated';
    elseif (!empty($employee['inactive']))
        $status = 'inactive';
    else
        $status = 'active';

    $reason = null;
    if ($status === 'terminated' && isset($employee['separation_reason'])
        && $employee['separation_reason'] !== '' && $employee['separation_reason'] !== null) {
        if (!is_numeric($employee['separation_reason']))
            return false;
        $reason = 'legacy:'.(int)$employee['separation_reason'];
    }

    return array(
        'legacy_employee_number' => (int)$employee['employee_number'],
        'start_date' => $start,
        'end_date' => $end,
        'employment_status' => $status,
        'termination_reason_code' => $reason,
        'interval_quality' => $quality,
    );
}

/**
 * Load the legacy-current Employment linked through the retained employee map.
 * This is an internal compatibility helper, not a new UI/API authorization.
 *
 * @param int $employee_number
 * @param bool $for_update
 * @return array|false
 */
function get_hrm_legacy_current_employment($employee_number, $for_update = false)
{
    if (!hrm_employment_table_ready() || (int)$employee_number <= 0)
        return false;
    $sql = "SELECT h.employment_id,h.worker_id,h.employment_sequence,"
        ."h.legal_entity_id,h.legacy_employee_number,h.legacy_current,"
        ."h.start_date,h.end_date,h.employment_status,h.termination_reason_code,"
        ."h.interval_quality,h.source,h.created_by,h.updated_by "
        ."FROM ".TB_PREF."hrm_employments h "
        ."INNER JOIN ".TB_PREF."hrm_employee_worker_map m ON m.worker_id=h.worker_id "
        ."WHERE m.employee_number=".(int)$employee_number." AND m.retired_at IS NULL "
        ."AND h.legacy_employee_number=".(int)$employee_number." "
        ."AND h.legacy_current=1 ORDER BY h.employment_id DESC LIMIT 1";
    if ($for_update)
        $sql .= " FOR UPDATE";
    $result = db_query($sql, 'could not read HRM employment compatibility row');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param mixed $a @param mixed $b @return bool */
function hrm_employment_nullable_equal($a, $b)
{
    if ($a === null || $a === '')
        $a = null;
    if ($b === null || $b === '')
        $b = null;
    return $a === $b;
}

/**
 * Append one PII-free/restricted-value-free Employment shadow event.
 *
 * @param string $action
 * @param int $employment_id
 * @param int $worker_id
 * @param int $employee_number
 * @param array $changed_fields
 * @param string $reason_code
 * @return array|false
 */
function append_hrm_employment_domain_audit($action, $employment_id, $worker_id,
    $employee_number, $changed_fields, $reason_code)
{
    if (!function_exists('append_domain_audit_event') || (int)$employment_id <= 0
        || (int)$worker_id <= 0 || (int)$employee_number <= 0
        || !is_array($changed_fields) || trim((string)$action) === ''
        || trim((string)$reason_code) === '')
        return false;
    // Scope is derived from the persisted Employment row, never caller input.
    // This prevents an audit event from being attached to the wrong employer
    // chain if a future caller or future multi-entity path is introduced.
    $scope = db_query("SELECT legal_entity_id,worker_id,legacy_employee_number FROM ".TB_PREF."hrm_employments "
        ."WHERE employment_id=".(int)$employment_id." LIMIT 1",
        'could not resolve HRM Employment Legal Entity audit scope');
    $scope = $scope ? db_fetch_assoc($scope) : false;
    if (!$scope || (int)$scope['legal_entity_id'] <= 0
        || (int)$scope['worker_id'] !== (int)$worker_id
        || (int)$scope['legacy_employee_number'] !== (int)$employee_number)
        return false;
    $changed_fields = array_values(array_unique($changed_fields));
    sort($changed_fields);
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => (int)$scope['legal_entity_id'],
        'entity_type' => 'hrm_employment_shadow',
        'entity_key' => array(
            'employment_id' => (int)$employment_id,
            'worker_id' => (int)$worker_id,
            'employee_number' => (int)$employee_number,
        ),
        'action' => (string)$action,
        'reason_code' => (string)$reason_code,
        'before' => null,
        'after' => array('employment_projection_changed' => true),
        'restricted_diff' => array(
            'changed_fields' => $changed_fields,
            'restricted_values_stored' => false,
        ),
    ));
}

/**
 * Create the first legacy-current Employment for an existing Worker.
 *
 * @param array $employee
 * @param bool $audit
 * @param string $source
 * @param bool $allow_legacy_invalid
 * @return int|false
 */
function create_hrm_employment_from_employee($employee, $audit = true,
    $source = 'runtime_employee', $allow_legacy_invalid = false)
{
    if (!hrm_employment_table_ready() || !function_exists('get_hrm_person_worker_binding')
        || !function_exists('get_hrm_default_legal_entity_binding_id'))
        return false;
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(false);
    if ($legal_entity_id === false)
        return false;
    $projection = hrm_employment_projection_from_employee($employee, $allow_legacy_invalid);
    if (!$projection)
        return false;
    $employee_number = (int)$projection['legacy_employee_number'];
    $binding = hrm_employment_lock_binding_for_interval_policy($employee_number);
    if (!$binding || $binding['retired_at'] !== null)
        return false;
    if (get_hrm_legacy_current_employment($employee_number, true))
        return false;
    if (!hrm_employment_person_interval_policy_allows(
        $binding, 0, $projection['start_date'], $projection['end_date']
    ))
        return false;

    $actor = hrm_employment_actor_id();
    $sql = "INSERT INTO ".TB_PREF."hrm_employments (worker_id,employment_sequence,"
        ."legal_entity_id,legacy_employee_number,legacy_current,start_date,end_date,"
        ."employment_status,termination_reason_code,interval_quality,source,created_by) VALUES ("
        .(int)$binding['worker_id'].",1,".(int)$legal_entity_id.",".$employee_number.",1,"
        .($projection['start_date'] === null ? 'NULL' : db_escape($projection['start_date'])).","
        .($projection['end_date'] === null ? 'NULL' : db_escape($projection['end_date'])).","
        .db_escape($projection['employment_status']).","
        .($projection['termination_reason_code'] === null ? 'NULL'
            : db_escape($projection['termination_reason_code'])).","
        .db_escape($projection['interval_quality']).",".db_escape((string)$source).","
        .(int)$actor.")";
    if (!db_query($sql, 'could not create HRM employment shadow'))
        return false;
    $employment_id = (int)db_insert_id();
    if ($employment_id <= 0)
        return false;

    if ($audit && !append_hrm_employment_domain_audit(
        'hrm.employment.created', $employment_id, (int)$binding['worker_id'],
        $employee_number,
        array('employment_interval', 'employment_status', 'legacy_current_projection'),
        'hrm_fnd_002_employment_created'
    ))
        return false;
    return $employment_id;
}

/**
 * Synchronize only the legacy-current Employment projection.
 *
 * Existing migrated invalid intervals are tolerated only while byte-equivalent;
 * a new/changed invalid ordering is rejected. No rehire row is created here.
 *
 * @param array $employee
 * @return bool
 */
function synchronize_hrm_employment_from_employee($employee)
{
    if (!is_array($employee) || empty($employee['employee_number'])
        || !hrm_employment_table_ready())
        return false;
    $employee_number = (int)$employee['employee_number'];
    $binding = hrm_employment_lock_binding_for_interval_policy($employee_number);
    if (!$binding)
        return false;
    $current = get_hrm_legacy_current_employment($employee_number, true);
    if (!$current)
        return create_hrm_employment_from_employee($employee, true, 'runtime_employee') !== false;
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        return false;
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(false);
    if ($legal_entity_id === false || (int)$current['employment_sequence'] !== 1
        || (int)$current['legal_entity_id'] !== (int)$legal_entity_id
        || (int)$current['legacy_current'] !== 1
        || (int)$current['legacy_employee_number'] !== $employee_number)
        return false;

    $projection = hrm_employment_projection_from_employee($employee, false);
    if (!$projection) {
        $legacy_projection = hrm_employment_projection_from_employee($employee, true);
        if (!$legacy_projection || $legacy_projection['interval_quality'] !== 'legacy_invalid_order')
            return false;
        if (!hrm_employment_nullable_equal($current['start_date'], $legacy_projection['start_date'])
            || !hrm_employment_nullable_equal($current['end_date'], $legacy_projection['end_date'])
            || (string)$current['interval_quality'] !== 'legacy_invalid_order')
            return false;
        $projection = $legacy_projection;
    }

    $changed = array();
    foreach (array('start_date', 'end_date', 'termination_reason_code') as $field) {
        if (!hrm_employment_nullable_equal($current[$field], $projection[$field]))
            $changed[] = $field;
    }
    foreach (array('employment_status', 'interval_quality') as $field) {
        if ((string)$current[$field] !== (string)$projection[$field])
            $changed[] = $field;
    }
    if (empty($changed))
        return true;
    if (function_exists('hrm_employment_rehire_projection_change_allowed')
        && !hrm_employment_rehire_projection_change_allowed((int)$current['employment_id'], $projection))
        return false;
    if ((int)$current['worker_id'] !== (int)$binding['worker_id']
        || !hrm_employment_person_interval_policy_allows(
            $binding, (int)$current['employment_id'], $projection['start_date'], $projection['end_date']
        ))
        return false;

    $actor = hrm_employment_actor_id();
    $sql = "UPDATE ".TB_PREF."hrm_employments SET "
        ."start_date=".($projection['start_date'] === null ? 'NULL'
            : db_escape($projection['start_date'])).","
        ."end_date=".($projection['end_date'] === null ? 'NULL'
            : db_escape($projection['end_date'])).","
        ."employment_status=".db_escape($projection['employment_status']).","
        ."termination_reason_code=".($projection['termination_reason_code'] === null
            ? 'NULL' : db_escape($projection['termination_reason_code'])).","
        ."interval_quality=".db_escape($projection['interval_quality']).","
        ."updated_at=UTC_TIMESTAMP(),updated_by=".(int)$actor
        ." WHERE employment_id=".(int)$current['employment_id']." AND legacy_current=1";
    if (!db_query($sql, 'could not synchronize HRM employment shadow'))
        return false;
    return append_hrm_employment_domain_audit(
        'hrm.employment.synchronized', (int)$current['employment_id'],
        (int)$current['worker_id'], $employee_number, $changed,
        'hrm_fnd_002_legacy_projection_sync'
    ) !== false;
}

/**
 * Execute the bounded Employment side of one legacy Employee Separation.
 *
 * The legacy employee row has already been updated inside the caller's open
 * transaction. This function locks the accepted current Employment, requires
 * the same released date/status projection, and writes only the terminal
 * interval/status fields plus restricted-value-free PAY-AUD evidence.
 *
 * @param array $employee
 * @param string $effective_date SQL date (Y-m-d)
 * @return bool
 */
function execute_hrm_employment_termination($employee, $effective_date)
{
    if (!is_array($employee) || empty($employee['employee_number'])
        || !hrm_employment_table_ready())
        return false;
    $effective_date = hrm_employment_normalize_date($effective_date);
    if ($effective_date === false || $effective_date === null)
        return false;

    $employee_number = (int)$employee['employee_number'];
    $binding = hrm_employment_lock_binding_for_interval_policy($employee_number);
    if (!$binding)
        return false;
    $current = get_hrm_legacy_current_employment($employee_number, true);
    if (!function_exists('get_hrm_default_legal_entity_binding_id'))
        return false;
    $legal_entity_id = get_hrm_default_legal_entity_binding_id(false);
    if (!$current || $legal_entity_id === false || (int)$current['employment_sequence'] !== 1
        || (int)$current['legal_entity_id'] !== (int)$legal_entity_id
        || (int)$current['legacy_current'] !== 1
        || (int)$current['legacy_employee_number'] !== $employee_number)
        return false;

    $projection = hrm_employment_projection_from_employee($employee, false);
    if (!$projection || empty($employee['inactive'])
        || (string)$projection['employment_status'] !== 'terminated'
        || !hrm_employment_nullable_equal($projection['end_date'], $effective_date)
        || !hrm_employment_nullable_equal($projection['start_date'], $current['start_date']))
        return false;
    $start = hrm_employment_normalize_date($current['start_date']);
    if ($start === false || ($start !== null && strcmp($effective_date, $start) < 0))
        return false;

    if (function_exists('hrm_employment_rehire_projection_change_allowed')
        && !hrm_employment_rehire_projection_change_allowed((int)$current['employment_id'], $projection))
        return false;

    $same = hrm_employment_nullable_equal($current['start_date'], $projection['start_date'])
        && hrm_employment_nullable_equal($current['end_date'], $projection['end_date'])
        && hrm_employment_nullable_equal($current['termination_reason_code'], $projection['termination_reason_code'])
        && (string)$current['employment_status'] === (string)$projection['employment_status']
        && (string)$current['interval_quality'] === (string)$projection['interval_quality'];
    if ($same)
        return true;

    if ($current['end_date'] !== null && $current['end_date'] !== ''
        && !hrm_employment_nullable_equal($current['end_date'], $effective_date))
        return false;
    if ((int)$current['worker_id'] !== (int)$binding['worker_id']
        || !hrm_employment_person_interval_policy_allows(
            $binding, (int)$current['employment_id'], $projection['start_date'], $projection['end_date']
        ))
        return false;

    $actor = hrm_employment_actor_id();
    $sql = "UPDATE ".TB_PREF."hrm_employments SET "
        ."end_date=".db_escape($projection['end_date']).","
        ."employment_status='terminated',"
        ."termination_reason_code=".($projection['termination_reason_code'] === null
            ? 'NULL' : db_escape($projection['termination_reason_code'])).","
        ."interval_quality=".db_escape($projection['interval_quality']).","
        ."source='runtime_separation',updated_at=UTC_TIMESTAMP(),updated_by=".(int)$actor
        ." WHERE employment_id=".(int)$current['employment_id']." AND legacy_current=1";
    if (!db_query($sql, 'could not terminate HRM employment shadow'))
        return false;
    return append_hrm_employment_domain_audit(
        'hrm.employment.terminated', (int)$current['employment_id'],
        (int)$current['worker_id'], $employee_number,
        array('end_date', 'employment_status', 'termination_reason_code'),
        'hrm_fnd_002_employee_separation'
    ) !== false;
}

/**
 * Compatibility deployment wrapper. Before the additive migration creates the
 * table, legacy writes remain usable and the migration will deterministically
 * seed them. Once the table exists, synchronization becomes mandatory.
 *
 * @param array $employee
 * @param bool $create
 * @param array|null $effective_change
 * @return bool
 */
function synchronize_hrm_employment_from_employee_if_ready($employee, $create = false, $effective_change = null)
{
    if (!hrm_employment_table_ready())
        return true;
    if ($create)
        return create_hrm_employment_from_employee($employee, true, 'runtime_employee') !== false;
    if (is_array($effective_change)
        && isset($effective_change['change_type'])
        && (string)$effective_change['change_type'] === 'separation') {
        $effective_date = isset($effective_change['effective_date'])
            ? $effective_change['effective_date'] : null;
        return execute_hrm_employment_termination($employee, $effective_date);
    }
    return synchronize_hrm_employment_from_employee($employee);
}
