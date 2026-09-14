<?php
/**
 * HRM-FND-007 transaction-scoped Assignment readiness consumer boundary.
 *
 * This adapter does not mutate Assignment or Employee data and is not wired to
 * a production route yet. The jurisdiction must be supplied explicitly by a
 * future governed caller; nationality/passport/location are never inferred.
 */
if (!function_exists('get_hrm_work_authorization_readiness_as_of'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_readiness_db.inc');

function hrm_work_authorization_assignment_denied($decision, $employee_number, $assignment_id, $jurisdiction, $effective_date)
{
    return array(
        'assignment_ready'=>false,
        'decision'=>(string)$decision,
        'employee_number'=>(int)$employee_number,
        'assignment_id'=>(int)$assignment_id,
        'jurisdiction'=>(string)$jurisdiction,
        'effective_date'=>(string)$effective_date,
        'business_mutation'=>false,
    );
}

/**
 * Lock the exact current Employee/Worker/Employment/Assignment custody and then
 * evaluate Work Authorization readiness under the same caller transaction.
 */
function get_hrm_work_authorization_assignment_readiness_for_update($employee_number, $assignment_id, $jurisdiction, $effective_date)
{
    global $transaction_level;
    $employee_number = (int)$employee_number;
    $assignment_id = (int)$assignment_id;
    if (empty($transaction_level))
        return hrm_work_authorization_assignment_denied('deny_atomic_guard_transaction_required', $employee_number, $assignment_id, $jurisdiction, $effective_date);
    if (!function_exists('user_check_access') || !user_check_access('SA_EMPTRANSFER'))
        return hrm_work_authorization_assignment_denied('deny_assignment_authority_required', $employee_number, $assignment_id, $jurisdiction, $effective_date);
    if ($employee_number <= 0 || $assignment_id <= 0
        || !hrm_work_authorization_validate_jurisdiction($jurisdiction)
        || !hrm_work_authorization_validate_date($effective_date, false))
        return hrm_work_authorization_assignment_denied('deny_invalid_assignment_readiness_request', $employee_number, $assignment_id, $jurisdiction, $effective_date);

    $sql = 'SELECT e.employee_number,m.worker_id,em.employment_id,a.assignment_id '
        .'FROM '.TB_PREF.'employees e '
        .'INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.employee_number=e.employee_number AND m.retired_at IS NULL '
        .'INNER JOIN '.TB_PREF.'hrm_employments em ON em.worker_id=m.worker_id AND em.legacy_employee_number=e.employee_number AND em.legacy_current=1 '
        .'INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=em.employment_id AND a.legacy_employee_number=e.employee_number AND a.legacy_current=1 AND a.primary_flag=1 AND a.assignment_type_code=\'legacy_primary\' '
        .'WHERE e.employee_number='.$employee_number.' AND a.assignment_id='.$assignment_id
        .' ORDER BY m.mapping_id DESC LIMIT 2 FOR UPDATE';
    $r = db_query($sql, 'could not lock Work Authorization Assignment readiness custody');
    if (!$r || db_num_rows($r) !== 1)
        return hrm_work_authorization_assignment_denied('deny_assignment_custody_unavailable', $employee_number, $assignment_id, $jurisdiction, $effective_date);
    $row = db_fetch_assoc($r);
    if (!$row || (int)$row['employee_number'] !== $employee_number
        || (int)$row['assignment_id'] !== $assignment_id || (int)$row['worker_id'] <= 0
        || (int)$row['employment_id'] <= 0)
        return hrm_work_authorization_assignment_denied('deny_assignment_custody_unavailable', $employee_number, $assignment_id, $jurisdiction, $effective_date);

    $readiness = get_hrm_work_authorization_readiness_as_of((int)$row['worker_id'], $jurisdiction, $effective_date);
    if (!is_array($readiness) || empty($readiness['ready'])) {
        $decision = is_array($readiness) && isset($readiness['decision'])
            ? (string)$readiness['decision'] : 'deny_work_authorization_readiness_unavailable';
        return hrm_work_authorization_assignment_denied($decision, $employee_number, $assignment_id, $jurisdiction, $effective_date);
    }
    return array(
        'assignment_ready'=>true,
        'decision'=>'ready_under_approved_work_authorization_policy',
        'employee_number'=>$employee_number,
        'worker_id'=>(int)$row['worker_id'],
        'employment_id'=>(int)$row['employment_id'],
        'assignment_id'=>$assignment_id,
        'jurisdiction'=>(string)$jurisdiction,
        'effective_date'=>(string)$effective_date,
        'policy_version_id'=>(int)$readiness['policy_version_id'],
        'authorization_id'=>(int)$readiness['authorization_id'],
        'expiry_attention_required'=>!empty($readiness['expiry_attention_required']),
        'business_mutation'=>false,
    );
}
