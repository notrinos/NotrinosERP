<?php
/**
 * HRM-FND-005 temporary service-state as-of read model.
 *
 * This helper deliberately does not decide overall employment, payroll, leave,
 * attendance or scheduling eligibility. It resolves only the temporary
 * Suspension/Reinstatement overlay from lifecycle-owned history.
 */

if (!defined('HRM_HIST_SUSPENSION') || !defined('HRM_HIST_REINSTATE'))
    include_once($path_to_root.'/hrm/includes/hrm_constants.inc');

/** Normalize one canonical SQL date without accepting SQL fragments. */
function hrm_lifecycle_service_state_as_of_date($date)
{
    $date = trim((string)$date);
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', $date) !== 1)
        return false;
    $parsed = DateTime::createFromFormat('!Y-m-d', $date);
    return $parsed && $parsed->format('Y-m-d') === $date ? $date : false;
}

/** Validate and replay already-projected lifecycle service-state rows. */
function hrm_lifecycle_service_state_as_of_resolve_rows($employee_id, $as_of_date, $worker_id, $rows)
{
    $employee_id = trim((string)$employee_id);
    $as_of_date = hrm_lifecycle_service_state_as_of_date($as_of_date);
    $worker_id = (int)$worker_id;
    if ($employee_id === '' || strlen($employee_id) > 20 || $as_of_date === false || $worker_id <= 0 || !is_array($rows))
        return false;

    $state = 'active';
    $previous_type = '';
    $previous_date = '';
    $previous_history_id = 0;
    $previous_employment_id = 0;
    $previous_assignment_id = 0;
    $last_history_id = 0;
    $last_command_id = 0;
    $last_employment_id = 0;
    $last_assignment_id = 0;

    foreach ($rows as $row) {
        if (!is_array($row))
            return false;

        $history_id = isset($row['history_id']) ? (int)$row['history_id'] : 0;
        $change_type = isset($row['change_type']) ? (string)$row['change_type'] : '';
        $effective_date = isset($row['effective_date']) ? hrm_lifecycle_service_state_as_of_date((string)$row['effective_date']) : false;
        $reason = isset($row['reason']) ? (string)$row['reason'] : '';
        $command_id = isset($row['lifecycle_command_id']) ? (int)$row['lifecycle_command_id'] : 0;
        $command_worker_id = isset($row['worker_id']) ? (int)$row['worker_id'] : 0;
        $employment_id = isset($row['employment_id']) ? (int)$row['employment_id'] : 0;
        $assignment_id = isset($row['assignment_id']) ? (int)$row['assignment_id'] : 0;
        $command_type = isset($row['command_type']) ? (string)$row['command_type'] : '';
        $command_status = isset($row['command_status']) ? (string)$row['command_status'] : '';
        $command_effective_at = isset($row['effective_at']) ? (string)$row['effective_at'] : '';
        $row_version = isset($row['row_version']) ? (int)$row['row_version'] : 0;

        if ($history_id <= 0 || $effective_date === false || strcmp($effective_date, $as_of_date) > 0
            || $command_id <= 0 || $command_worker_id !== $worker_id || $employment_id <= 0 || $assignment_id <= 0
            || $command_status !== 'completed' || $row_version !== 2
            || preg_match('/^lifecycle_command:([1-9][0-9]*)$/D', $reason, $match) !== 1
            || (int)$match[1] !== $command_id
            || $command_effective_at !== $effective_date.' 00:00:00')
            return false;

        if ($change_type === 'rehire') {
            // A completed Rehire begins a new episode; the old Suspension
            // overlay remains historical and cannot suspend the successor.
            if ($command_type !== 'employee_rehire' || empty($row['rehire_receipt_verified'])
                || empty($row['successor_employment_id']) || empty($row['successor_assignment_id']))
                return false;
            $state = 'active';
            $previous_type = '';
            $previous_date = '';
            $previous_employment_id = (int)$row['successor_employment_id'];
            $previous_assignment_id = (int)$row['successor_assignment_id'];
            $last_history_id = $history_id; $last_command_id = $command_id;
            $last_employment_id = $previous_employment_id; $last_assignment_id = $previous_assignment_id;
            continue;
        } elseif ($change_type === HRM_HIST_SUSPENSION) {
            if ($command_type !== 'employee_suspension' || $previous_type === HRM_HIST_SUSPENSION)
                return false;
            if ($previous_type !== '' && $previous_type !== HRM_HIST_REINSTATE)
                return false;
            $state = 'suspended';
        } elseif ($change_type === HRM_HIST_REINSTATE) {
            if ($command_type !== 'employee_reinstatement' || $previous_type !== HRM_HIST_SUSPENSION
                || $employment_id !== $previous_employment_id || $assignment_id !== $previous_assignment_id)
                return false;
            $state = 'active';
        } else {
            return false;
        }

        if ($previous_date !== '') {
            $date_cmp = strcmp($effective_date, $previous_date);
            if ($date_cmp < 0 || ($date_cmp === 0 && $history_id <= $previous_history_id))
                return false;
            if ($employment_id !== $previous_employment_id)
                return false;
        }

        $previous_type = $change_type;
        $previous_date = $effective_date;
        $previous_history_id = $history_id;
        $previous_employment_id = $employment_id;
        $previous_assignment_id = $assignment_id;
        $last_history_id = $history_id;
        $last_command_id = $command_id;
        $last_employment_id = $employment_id;
        $last_assignment_id = $assignment_id;
    }

    return array(
        'employee_id' => $employee_id,
        'worker_id' => $worker_id,
        'as_of_date' => $as_of_date,
        'service_state' => $state,
        'scope' => 'temporary_suspension_overlay_only',
        'history_id' => $last_history_id,
        'lifecycle_command_id' => $last_command_id,
        'employment_id' => $last_employment_id,
        'assignment_id' => $last_assignment_id
    );
}

/**
 * Read one Employee's temporary Suspension/Reinstatement state as of a date.
 * Locking mode requires the caller's transaction and Employee projection lock;
 * current reads avoid an older outer approval transaction's read snapshot.
 */
function get_hrm_lifecycle_employee_service_state_as_of($employee_id, $as_of_date, $for_update=false)
{
    $employee_id = trim((string)$employee_id);
    $as_of_date = hrm_lifecycle_service_state_as_of_date($as_of_date);
    if ($employee_id === '' || strlen($employee_id) > 20 || $as_of_date === false)
        return false;

    $mapping_sql = "SELECT employee_id,worker_id,retired_at FROM ".TB_PREF."hrm_employee_worker_map "
        ."WHERE employee_id=".db_escape($employee_id)." LIMIT 2";
    if ($for_update === true) $mapping_sql .= ' FOR UPDATE';
    $mapping_result = db_query($mapping_sql, 'could not resolve HRM lifecycle service-state Employee/Worker custody');
    if (!$mapping_result || (int)db_num_rows($mapping_result) !== 1)
        return false;
    $mapping = db_fetch_assoc($mapping_result);
    if (!$mapping || (int)$mapping['worker_id'] <= 0
        || ($mapping['retired_at'] !== null && trim((string)$mapping['retired_at']) !== ''))
        return false;

    // With multiple same-code episodes, the flat Employee hire/release fields
    // cannot express an intervening gap. Deny consumer eligibility in that gap
    // rather than treating an empty Suspension overlay as active service.
    $episodes = db_query("SELECT employment_id,worker_id,employment_sequence,legacy_employee_number,"
        ."start_date,end_date FROM ".TB_PREF."hrm_employments WHERE worker_id=".(int)$mapping['worker_id']
        ." ORDER BY employment_sequence".($for_update ? ' FOR UPDATE' : ''),
        'could not verify Rehire service intervals');
    if (!$episodes) return false;
    $episode_rows = array(); $has_rehire = false;
    while ($episode = db_fetch_assoc($episodes)) {
        $episode_rows[] = $episode;
        if ((int)$episode['employment_sequence'] > 1) $has_rehire = true;
    }
    if ($has_rehire) {
        include_once(dirname(__FILE__).'/lifecycle_rehire_evidence_db.inc');
        $matches = 0;
        foreach ($episode_rows as $episode) {
            if ((int)$episode['employment_sequence'] > 1
                && !get_hrm_lifecycle_rehire_employment_evidence($episode, $for_update)) return false;
            if (!empty($episode['start_date']) && $episode['start_date'] <= $as_of_date
                && (empty($episode['end_date']) || $episode['end_date'] >= $as_of_date)) $matches++;
        }
        if ($matches !== 1) return false;
    }

    $history_sql = "SELECT h.history_id,h.change_type,h.effective_date,h.reason,"
        ."c.lifecycle_command_id,c.worker_id,c.employment_id,c.assignment_id,c.command_type,"
        ."c.command_status,c.effective_at,c.row_version "
        ."FROM ".TB_PREF."employee_history h "
        ."LEFT JOIN ".TB_PREF."hrm_lifecycle_commands c "
        ."ON c.lifecycle_command_id=CAST(SUBSTRING(h.reason,19) AS UNSIGNED) "
        ."WHERE h.employee_id=".db_escape($employee_id)." AND h.change_type IN ("
        .db_escape(HRM_HIST_SUSPENSION).",".db_escape(HRM_HIST_REINSTATE).",'rehire') "
        ."AND h.effective_date<=".db_escape($as_of_date)." "
        ."ORDER BY h.effective_date ASC,h.history_id ASC";
    if ($for_update === true) $history_sql .= ' FOR UPDATE';
    $history_result = db_query($history_sql, 'could not resolve HRM lifecycle service-state history custody');
    if (!$history_result)
        return false;

    $rows = array();
    while ($row = db_fetch_assoc($history_result)) {
        if ($row['change_type'] === 'rehire') {
            include_once(dirname(__FILE__).'/lifecycle_rehire_evidence_db.inc');
            $sql = "SELECT e.* FROM ".TB_PREF."hrm_employments e INNER JOIN "
                .TB_PREF."hrm_lifecycle_rehire_executions r ON r.successor_employment_id=e.employment_id "
                ."WHERE r.lifecycle_command_id=".(int)$row['lifecycle_command_id'];
            if ($for_update) $sql .= ' FOR UPDATE';
            $result = db_query($sql, 'could not resolve Rehire service-state episode');
            $employment = $result ? db_fetch_assoc($result) : false;
            $receipt = $employment ? get_hrm_lifecycle_rehire_employment_evidence($employment, $for_update) : false;
            if (!$receipt || (int)$receipt['history_id'] !== (int)$row['history_id']) return false;
            $row['rehire_receipt_verified'] = true;
            $row['successor_employment_id'] = (int)$receipt['successor_employment_id'];
            $row['successor_assignment_id'] = (int)$receipt['successor_assignment_id'];
        }
        $rows[] = $row;
    }

    return hrm_lifecycle_service_state_as_of_resolve_rows(
        $employee_id, $as_of_date, (int)$mapping['worker_id'], $rows
    );
}
