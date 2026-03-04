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
 * Master UI aggregator — includes all HRM UI helper files.
 * Per HRM_DEVELOPMENT_PLAN.md Section 4.2 (Separation of Concerns).
 */

include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/ui/hrm_selectors.inc');
include_once($path_to_root.'/hrm/includes/ui/employee_ui.inc');
include_once($path_to_root.'/hrm/includes/ui/payslip_ui.inc');
include_once($path_to_root.'/hrm/includes/ui/attendance_ui.inc');
include_once($path_to_root.'/hrm/includes/ui/leave_ui.inc');
include_once($path_to_root.'/hrm/includes/ui/overtime_ui.inc');
include_once($path_to_root.'/hrm/includes/ui/department_ui.inc');
include_once($path_to_root.'/hrm/includes/ui/payroll_ui.inc');
include_once($path_to_root.'/hrm/includes/ui/salary_structure_ui.inc');