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
class HrmApp extends application {
	function __construct() {
		parent::__construct('hrm', _($this->help_context = '&Human Resources'));

		$this->add_module(_('Transactions'));

		$this->add_module(_('Inquiries and Reports'));

		$this->add_module(_('Maintenance'));
		$this->add_lapp_function(2, _('Manage &Employees'), 'hrm/manage/employees.php?', 'SA_EMPLOYEE', MENU_ENTRY);
		$this->add_lapp_function(2, _('&Departments'), '/hrm/manage/departments.php?', 'SA_DEPARTMENT', MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('&Overtime'), '/hrm/manage/overtime.php?', 'SA_OVERTIME', MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Lea&ve Types'), '/hrm/manage/leave_types.php?', 'SA_LEAVETYPE', MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Job Classification'), '/hrm/manage/job_classes.php?', 'SA_JOBCLASS', MENU_MAINTENANCE);

		$this->add_rapp_function(2, _('Job &Positions'), '/hrm/manage/job_positions.php?', 'SA_POSITION', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Manage &Grades'), '/hrm/manage/grades.php?', 'SA_PAYGRADE', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Pay Ele&ments'), '/hrm/manage/pay_elements.php?', 'SA_PAYELEMENT', MENU_SETTINGS);
		$this->add_rapp_function(2, _('Pay Elements Allo&cation'), '/hrm/manage/overtime.php?', 'SA_PAYELEMENTALLOC', MENU_SETTINGS);
		$this->add_rapp_function(2, _('Sa&lary Structure'), '/hrm/manage/salary_structure.php?', 'SA_SALARYSTRUCTURE', MENU_SETTINGS);

		$this->add_extensions();
	}
}
