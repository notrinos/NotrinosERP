<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * HRM Hook System — per HRM_DEVELOPMENT_PLAN.md Section 8.3
 * Allows extensions to inject behavior at key points.
 *
 * Usage:
 *   hrm_register_hook('after_calculate_payslip', 'my_gosi_calc');
 *   function my_gosi_calc(&$payslip_doc, $employee) { ... }
 */

global $hrm_hooks;
$hrm_hooks = array();

/**
 * Register a callback for a hook event.
 * @param string   $event    Hook point name
 * @param callable $callback Function or method to call
 * @param int      $priority Lower = earlier execution (default 10)
 */
function hrm_register_hook($event, $callback, $priority=10) {
	global $hrm_hooks;
	$hrm_hooks[$event][$priority][] = $callback;
}

/**
 * Fire all registered callbacks for a hook.
 * Arguments are passed by reference.
 * @param string $event Hook point name
 * @param mixed  ...$args Arguments to pass to callbacks
 */
function hrm_fire_hook($event, &...$args) {
	global $hrm_hooks;
	if (isset($hrm_hooks[$event])) {
		ksort($hrm_hooks[$event]); // sort by priority
		foreach ($hrm_hooks[$event] as $priority => $callbacks) {
			foreach ($callbacks as $callback) {
				if (is_callable($callback))
					call_user_func_array($callback, $args);
			}
		}
	}
}

/**
 * Check if a hook has any registered callbacks.
 */
function hrm_has_hook($event) {
	global $hrm_hooks;
	return !empty($hrm_hooks[$event]);
}

/**
 * Remove all callbacks for a hook (or a specific callback).
 */
function hrm_remove_hook($event, $callback=null) {
	global $hrm_hooks;
	if ($callback === null) {
		unset($hrm_hooks[$event]);
	} else {
		if (isset($hrm_hooks[$event])) {
			foreach ($hrm_hooks[$event] as $priority => &$callbacks) {
				$callbacks = array_filter($callbacks, function($cb) use ($callback) {
					return $cb !== $callback;
				});
			}
		}
	}
}

// ══════════════════════════════════════════
// AVAILABLE HOOK POINTS:
// ══════════════════════════════════════════
//
// PAYROLL:
//   before_calculate_payslip(&$employee, $from_date, $to_date)
//   after_calculate_basic(&$payslip_doc, $employee)
//   after_calculate_earnings(&$payslip_doc, $employee)
//   before_calculate_deductions(&$payslip_doc, $employee)
//   after_calculate_payslip(&$payslip_doc, $employee)
//   before_post_payslip(&$payslip_doc)
//   after_post_payslip($payslip_id, $gl_trans_no)
//   on_void_payslip($payslip_id)
//
// LEAVE:
//   on_leave_request_submitted($request_id, $employee_id)
//   on_leave_status_change($request_id, $new_status)
//
// EMPLOYEE:
//   on_employee_created($employee_id)
//   on_employee_updated($employee_id, $old_data, $new_data)
//   on_employee_hired($employee_id)
//   on_employee_transferred($employee_id, $history_id)
//   on_employee_separated($employee_id, $reason)
//
// ATTENDANCE:
//   on_attendance_saved($employee_id, $date)
//   on_attendance_import($rows)
//
// DOCUMENTS:
//   on_document_uploaded($doc_id, $employee_id)
//   on_document_expiring($doc_id, $employee_id, $days_remaining)

