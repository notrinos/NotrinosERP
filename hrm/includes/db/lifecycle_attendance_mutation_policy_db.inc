<?php
/**
 * HRM-FND-005 temporary Suspension attendance mutation policy evaluator.
 *
 * This file is deliberately side-effect-free. It does not query the database,
 * call the service-state resolver, or mutate attendance/leave/payroll state.
 */

/** Return one canonical SQL date or false. */
function hrm_lifecycle_attendance_policy_date($date)
{
    $date = trim((string)$date);
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', $date) !== 1)
        return false;
    $parsed = DateTime::createFromFormat('!Y-m-d', $date);
    return $parsed && $parsed->format('Y-m-d') === $date ? $date : false;
}

/**
 * Decide policy for a future attendance/scheduling-adjacent mutation.
 *
 * @param array|false $service_state Result from get_hrm_lifecycle_employee_service_state_as_of().
 * @param string $mutation_date Exact SQL date.
 * @param string $mutation_kind Finite supported mutation kind.
 * @param bool $has_existing_evidence Whether attendance/leave evidence already exists for the day.
 * @param bool $payroll_locked Whether existing payslip evidence locks the date.
 * @return array<string,mixed>
 */
function hrm_lifecycle_attendance_mutation_policy_decide($service_state, $mutation_date, $mutation_kind,
    $has_existing_evidence, $payroll_locked)
{
    $date = hrm_lifecycle_attendance_policy_date($mutation_date);
    $kind = trim((string)$mutation_kind);
    $supported = array(
        'attendance_write', 'attendance_meta_write', 'attendance_delete',
        'attendance_leave_write', 'attendance_bulk_fill'
    );

    $base = array(
        'can_mutate' => false,
        'decision' => 'deny_invalid_or_ambiguous_service_state',
        'mutation_date' => $date === false ? '' : $date,
        'mutation_kind' => $kind,
        'existing_evidence' => (bool)$has_existing_evidence,
        'existing_evidence_disposition' => (bool)$has_existing_evidence
            ? 'preserve_visible_no_mutation' : 'none',
        'correction_policy' => 'separate_review_required',
        'scope' => 'temporary_suspension_attendance_policy_only'
    );

    if ($date === false || !in_array($kind, $supported, true) || !is_array($service_state))
        return $base;
    if (!isset($service_state['scope'], $service_state['service_state'], $service_state['as_of_date'])
        || $service_state['scope'] !== 'temporary_suspension_overlay_only'
        || hrm_lifecycle_attendance_policy_date($service_state['as_of_date']) !== $date
        || !in_array($service_state['service_state'], array('active', 'suspended'), true))
        return $base;

    if ((bool)$payroll_locked) {
        $base['decision'] = 'deny_payroll_locked';
        return $base;
    }

    if ($service_state['service_state'] === 'suspended') {
        $base['decision'] = 'deny_suspended_day_mutation';
        return $base;
    }

    $base['can_mutate'] = true;
    $base['decision'] = 'legacy_rules_continue';
    $base['existing_evidence_disposition'] = (bool)$has_existing_evidence
        ? 'legacy_rules_continue' : 'none';
    $base['correction_policy'] = 'legacy_rules_continue';
    return $base;
}
