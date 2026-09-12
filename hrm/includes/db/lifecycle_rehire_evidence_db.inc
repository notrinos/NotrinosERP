<?php
/**
 * Exact completed same-code Rehire evidence for normalized compatibility.
 * No caller may create a successor merely by changing a sequence or source tag.
 */

/**
 * Resolve the immutable creation receipt for a successor Employment.
 * This internal read exposes numeric custody and dates only.
 * @return array|false
 */
function get_hrm_lifecycle_rehire_employment_evidence($employment, $for_update = false)
{
    if (!is_array($employment) || empty($employment['employment_id'])
        || (int)$employment['employment_sequence'] < 2 || !function_exists('db_query'))
        return false;
    $sql = "SELECT r.*,c.worker_id,c.employment_id AS command_employment_id,c.assignment_id AS command_assignment_id,"
        ."c.command_type,c.command_status,c.row_version,c.request_hash,c.requested_by,c.completed_by,"
        ."c.approval_draft_id,c.effective_at,d.trans_type AS draft_type,d.status AS draft_status,"
        ."d.submitted_by AS draft_maker,d.draft_data,h.effective_date,h.change_type,h.reason,"
        ."p.worker_id AS predecessor_worker_id,p.employment_sequence AS predecessor_sequence,"
        ."p.legacy_employee_number AS predecessor_employee_number,p.end_date AS predecessor_end_date,"
        ."p.legacy_current AS predecessor_current,s.start_date AS successor_start_date,"
        ."s.worker_id AS successor_worker_id,s.legacy_employee_number AS successor_employee_number,"
        ."s.employment_sequence AS successor_sequence,sc.employment_id AS contract_employment_id,"
        ."sa.employment_id AS assignment_employment_id,sc.contract_sequence,sa.assignment_sequence,"
        ."sc.effective_from AS contract_start,sa.effective_from AS assignment_start,"
        ."h.employee_id AS history_employee_id,m.employee_id AS mapped_employee_id "
        ."FROM ".TB_PREF."hrm_lifecycle_rehire_executions r "
        ."INNER JOIN ".TB_PREF."hrm_lifecycle_commands c ON c.lifecycle_command_id=r.lifecycle_command_id "
        ."INNER JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."INNER JOIN ".TB_PREF."employee_history h ON h.history_id=r.history_id "
        ."INNER JOIN ".TB_PREF."hrm_employments p ON p.employment_id=r.predecessor_employment_id "
        ."INNER JOIN ".TB_PREF."hrm_employments s ON s.employment_id=r.successor_employment_id "
        ."INNER JOIN ".TB_PREF."hrm_contracts sc ON sc.contract_id=r.successor_contract_id "
        ."INNER JOIN ".TB_PREF."hrm_assignments sa ON sa.assignment_id=r.successor_assignment_id "
        ."INNER JOIN ".TB_PREF."hrm_employee_worker_map m ON m.worker_id=c.worker_id "
        ."WHERE r.successor_employment_id=".(int)$employment['employment_id']." LIMIT 2";
    if ($for_update) $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not verify completed Rehire Employment evidence');
    if (!$result || !($row = db_fetch_assoc($result)) || db_fetch_assoc($result))
        return false;
    if ($row['command_type'] !== 'employee_rehire' || $row['command_status'] !== 'completed'
        || (int)$row['row_version'] !== 2 || (int)$row['requested_by'] <= 0
        || (int)$row['completed_by'] <= 0 || (int)$row['requested_by'] === (int)$row['completed_by']
        || (int)$row['approval_draft_id'] <= 0
        || (int)$row['draft_type'] !== 102 || (int)$row['draft_status'] !== 1
        || (int)$row['draft_maker'] !== (int)$row['requested_by']
        || $row['effective_at'] !== $employment['start_date'].' 00:00:00'
        || (int)$row['command_employment_id'] !== (int)$row['predecessor_employment_id']
        || (int)$row['command_assignment_id'] !== (int)$row['predecessor_assignment_id']
        || (int)$row['worker_id'] !== (int)$employment['worker_id']
        || (int)$row['predecessor_worker_id'] !== (int)$employment['worker_id']
        || (int)$row['successor_worker_id'] !== (int)$employment['worker_id']
        || (int)$row['successor_employee_number'] !== (int)$employment['legacy_employee_number']
        || (int)$row['predecessor_employee_number'] !== (int)$employment['legacy_employee_number']
        || (int)$row['successor_sequence'] !== (int)$employment['employment_sequence']
        || (int)$row['successor_sequence'] !== (int)$row['predecessor_sequence'] + 1
        || $row['predecessor_current'] !== null
        || $row['successor_start_date'] !== $employment['start_date']
        || $row['effective_date'] !== $employment['start_date']
        || $row['history_employee_id'] !== $row['mapped_employee_id']
        || (int)$row['contract_employment_id'] !== (int)$employment['employment_id']
        || (int)$row['assignment_employment_id'] !== (int)$employment['employment_id']
        || (int)$row['contract_sequence'] !== 1 || (int)$row['assignment_sequence'] !== 1
        || $row['contract_start'] !== $employment['start_date'] || $row['assignment_start'] !== $employment['start_date']
        || $row['change_type'] !== 'rehire'
        || $row['reason'] !== 'lifecycle_command:'.(int)$row['lifecycle_command_id']
        || empty($row['predecessor_end_date'])
        || strcmp($row['predecessor_end_date'], $row['successor_start_date']) >= 0)
        return false;
    $draft = json_decode(html_entity_decode($row['draft_data'], ENT_QUOTES, 'UTF-8'), true);
    if (!is_array($draft) || count($draft) !== 6
        || !isset($draft['lifecycle_command_id'], $draft['command_row_version'], $draft['worker_id'],
            $draft['employee_number'], $draft['effective_date'], $draft['request_hash'])
        || (int)$draft['lifecycle_command_id'] !== (int)$row['lifecycle_command_id']
        || (int)$draft['command_row_version'] !== 1
        || (int)$draft['worker_id'] !== (int)$row['worker_id']
        || (int)$draft['employee_number'] !== (int)$employment['legacy_employee_number']
        || $draft['effective_date'] !== $employment['start_date']
        || $draft['request_hash'] !== $row['request_hash']) return false;
    unset($row['draft_data']);
    return $row;
}

/**
 * Admit sequence one unchanged; later episodes require a completed receipt and
 * an unchanged original Employee hire date. No new interval is inferred.
 * @return bool
 */
function hrm_lifecycle_rehire_compatibility_allows($employee, $employment)
{
    if (!is_array($employee) || !is_array($employment)
        || (int)$employment['employment_sequence'] < 1)
        return false;
    if ((int)$employment['employment_sequence'] === 1)
        return true;
    $receipt = get_hrm_lifecycle_rehire_employment_evidence($employment, true);
    return $receipt && isset($employee['hire_date'])
        && (string)$employee['hire_date'] === (string)$receipt['first_hire_date'];
}

/**
 * Build the existing compatibility projection with the evidenced episode date.
 * The caller's original Employee array and stored first hire date stay intact.
 * @return array|false
 */
function hrm_lifecycle_rehire_employee_episode_projection($employee, $employment)
{
    if (!hrm_lifecycle_rehire_compatibility_allows($employee, $employment))
        return false;
    if ((int)$employment['employment_sequence'] > 1)
        $employee['hire_date'] = $employment['start_date'];
    return hrm_employment_projection_from_employee($employee, false);
}
