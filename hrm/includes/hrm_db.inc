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
 * Master DB aggregator — includes all HRM database layer files.
 */

// Organization
include_once($path_to_root.'/hrm/includes/db/department_db.inc');
include_once($path_to_root.'/hrm/includes/db/job_classes_db.inc');
include_once($path_to_root.'/hrm/includes/db/job_position_db.inc');
include_once($path_to_root.'/hrm/includes/db/pay_grade_db.inc');
include_once($path_to_root.'/hrm/includes/db/grade_db.inc');

// Employee
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/employee_history_db.inc');
include_once($path_to_root.'/hrm/includes/db/employee_document_db.inc');
include_once($path_to_root.'/hrm/includes/db/employee_dependent_db.inc');
include_once($path_to_root.'/hrm/includes/db/employee_salary_db.inc');
include_once($path_to_root.'/hrm/includes/db/doc_type_db.inc');

// Compensation
include_once($path_to_root.'/hrm/includes/db/pay_element_db.inc');
include_once($path_to_root.'/hrm/includes/db/salary_structure_db.inc');

// Attendance
include_once($path_to_root.'/hrm/includes/db/attendance_db.inc');
include_once($path_to_root.'/hrm/includes/db/attendance_deduction_db.inc');
include_once($path_to_root.'/hrm/includes/db/work_shift_db.inc');
include_once($path_to_root.'/hrm/includes/db/working_days_db.inc');
include_once($path_to_root.'/hrm/includes/db/holiday_db.inc');

// Leave
include_once($path_to_root.'/hrm/includes/db/leave_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_type_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_policy_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_request_db.inc');
include_once($path_to_root.'/hrm/includes/db/leave_balance_db.inc');

// Overtime
include_once($path_to_root.'/hrm/includes/db/overtime_db.inc');
include_once($path_to_root.'/hrm/includes/db/overtime_type_db.inc');
include_once($path_to_root.'/hrm/includes/db/overtime_request_db.inc');
include_once($path_to_root.'/hrm/includes/db/overtime_budget_db.inc');

// Loans
include_once($path_to_root.'/hrm/includes/db/loan_type_db.inc');
include_once($path_to_root.'/hrm/includes/db/loan_db.inc');

// Payroll
include_once($path_to_root.'/hrm/includes/db/payslip_db.inc');
include_once($path_to_root.'/hrm/includes/db/payroll_db.inc');

// Tax & Statutory
include_once($path_to_root.'/hrm/includes/db/tax_bracket_db.inc');
include_once($path_to_root.'/hrm/includes/db/statutory_db.inc');
include_once($path_to_root.'/hrm/includes/db/deduction_code_db.inc');

// EOS & Settings
include_once($path_to_root.'/hrm/includes/db/eos_db.inc');
include_once($path_to_root.'/hrm/includes/db/hr_settings_db.inc');

// Extended HRM (Phase 3 matrix features)
include_once($path_to_root.'/hrm/includes/db/recruitment_db.inc');
include_once($path_to_root.'/hrm/includes/db/training_db.inc');
include_once($path_to_root.'/hrm/includes/db/appraisal_db.inc');
include_once($path_to_root.'/hrm/includes/db/asset_allocation_db.inc');
