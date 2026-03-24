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
 * Employee change history CRUD
 * Provides audit trail for all employee changes (transfers, promotions, salary changes).
 */

/**
 * Add one employee history record.
 *
 * @param string $employee_id
 * @param string $change_type
 * @param string $effective_date
 * @param int|null $old_department_id
 * @param int|null $new_department_id
 * @param int|null $old_position_id
 * @param int|null $new_position_id
 * @param int|null $old_grade_id
 * @param int|null $new_grade_id
 * @param float|null $old_salary
 * @param float|null $new_salary
 * @param string $reason
 * @param string $approved_by
 * @return int
 */
function add_employee_history($employee_id, $change_type, $effective_date, $old_department_id=null, $new_department_id=null, $old_position_id=null, $new_position_id=null, $old_grade_id=null, $new_grade_id=null, $old_salary=null, $new_salary=null, $reason='', $approved_by='') {
	$effective_date_sql = is_date($effective_date) ? date2sql($effective_date) : trim((string)$effective_date);
	if ($effective_date_sql === '')
		$effective_date_sql = date2sql(Today());

	$created_by = isset($_SESSION['wa_current_user']) ? (int)$_SESSION['wa_current_user']->user : 0;

	$sql = "INSERT INTO ".TB_PREF."employee_history
		(employee_id, change_type, effective_date,
		 old_department_id, new_department_id, old_position_id, new_position_id,
		 old_grade_id, new_grade_id, old_salary, new_salary, reason, approved_by, created_by)
		VALUES (
			".db_escape($employee_id).",
			".db_escape($change_type).",
			".db_escape($effective_date_sql).",
			".($old_department_id ? db_escape((int)$old_department_id) : 'NULL').",
			".($new_department_id ? db_escape((int)$new_department_id) : 'NULL').",
			".($old_position_id ? db_escape((int)$old_position_id) : 'NULL').",
			".($new_position_id ? db_escape((int)$new_position_id) : 'NULL').",
			".($old_grade_id ? db_escape((int)$old_grade_id) : 'NULL').",
			".($new_grade_id ? db_escape((int)$new_grade_id) : 'NULL').",
			".($old_salary !== null && $old_salary !== '' ? db_escape((float)$old_salary) : 'NULL').",
			".($new_salary !== null && $new_salary !== '' ? db_escape((float)$new_salary) : 'NULL').",
			".db_escape($reason).",
			".(trim((string)$approved_by) === '' ? 'NULL' : db_escape($approved_by)).",
			".db_escape($created_by)."
		)";

	db_query($sql, 'could not add employee history');
	return (int)db_insert_id();
}

function get_employee_history($employee_id, $change_type='') {
	$sql = "SELECT h.*,
				d_old.department_name as old_department_name,
				d_new.department_name as new_department_name,
				p_old.position_name as old_position_name,
				p_new.position_name as new_position_name,
				g_old.grade_name as old_grade_name,
				g_new.grade_name as new_grade_name
			FROM ".TB_PREF."employee_history h
			LEFT JOIN ".TB_PREF."departments d_old ON h.old_department_id = d_old.department_id
			LEFT JOIN ".TB_PREF."departments d_new ON h.new_department_id = d_new.department_id
			LEFT JOIN ".TB_PREF."positions p_old ON h.old_position_id = p_old.position_id
			LEFT JOIN ".TB_PREF."positions p_new ON h.new_position_id = p_new.position_id
			LEFT JOIN ".TB_PREF."pay_grades g_old ON h.old_grade_id = g_old.grade_id
			LEFT JOIN ".TB_PREF."pay_grades g_new ON h.new_grade_id = g_new.grade_id
			WHERE h.employee_id = ".db_escape($employee_id);

	if (!empty($change_type))
		$sql .= " AND h.change_type = ".db_escape($change_type);

	$sql .= " ORDER BY h.effective_date DESC, h.history_id DESC";

	return db_query($sql, 'could not get employee history');
}

function get_history_record($history_id) {
	$sql = "SELECT * FROM ".TB_PREF."employee_history WHERE history_id = ".db_escape($history_id);
	$result = db_query($sql, 'could not get history record');
	return db_fetch_assoc($result);
}

function delete_history_record($history_id) {
	$sql = "DELETE FROM ".TB_PREF."employee_history WHERE history_id = ".db_escape($history_id);
	db_query($sql, 'could not delete history record');
}

/**
 * Get the formatted change type label.
 */
function get_change_type_label($change_type) {
	$types = array(
		HRM_HIST_HIRE          => _('Hired'),
		HRM_HIST_TRANSFER      => _('Transfer'),
		HRM_HIST_PROMOTION     => _('Promotion'),
		HRM_HIST_SALARY_CHANGE => _('Salary Change'),
		HRM_HIST_GRADE_CHANGE  => _('Grade Change'),
		HRM_HIST_SEPARATION    => _('Separation'),
		HRM_HIST_SUSPENSION    => _('Suspension'),
		HRM_HIST_REINSTATE     => _('Reinstatement'),
	);
	return isset($types[$change_type]) ? $types[$change_type] : $change_type;
}

