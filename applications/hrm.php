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

		// ═══════════════════════════════════════════════════════════
		// Module 0: Transactions
		// ═══════════════════════════════════════════════════════════
		$this->add_module(_('Transactions'));
		$this->add_lapp_function(0, _('Attendance S&heet'),         'hrm/transactions/attendance_sheet.php?',              'SA_ATTENDANCE',       MENU_TRANSACTION);
		$this->add_lapp_function(0, _('&Leave Request'),            'hrm/transactions/leave_request.php?',                 'SA_LEAVEREQUEST',     MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Leave A&pproval'),           'hrm/transactions/leave_approval.php?',                'SA_LEAVEAPPROVE',     MENU_TRANSACTION);
		$this->add_lapp_function(0, _('O&vertime Entry'),           'hrm/transactions/overtime_request.php?',              'SA_OVERTIMEREQUEST',  MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Overtime App&roval'),        'hrm/transactions/overtime_approval.php?',             'SA_OVERTIMEAPPROVE',  MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Employee &Transfer'),        'hrm/transactions/employee_transfer.php?',             'SA_EMPTRANSFER',      MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Sa&lary Revision'),          'hrm/transactions/salary_revision.php?',               'SA_SALARYREVISION',   MENU_TRANSACTION);

		$this->add_rapp_function(0, _('Pay&slip Entry'),            'hrm/transactions/payslip.php?NewPayslip=Yes',         'SA_PAYSLIP',          MENU_TRANSACTION);
		$this->add_rapp_function(0, _('Pa&yroll Processing'),       'hrm/transactions/payroll_process.php?',               'SA_PAYROLL',          MENU_TRANSACTION);
		$this->add_rapp_function(0, _('Payroll Appr&oval'),         'hrm/transactions/payroll_approval.php?',              'SA_PAYROLLAPPROVE',   MENU_TRANSACTION);
		$this->add_rapp_function(0, _('&Payment Advice'),           'hrm/transactions/employee_bank_entry.php?NewPayment=Yes', 'SA_EMPLOYEEPAYMENT', MENU_TRANSACTION);
		$this->add_rapp_function(0, _('Payment &Batch'),            'hrm/transactions/payment_batch.php?',                 'SA_PAYMENTBATCH',     MENU_TRANSACTION);
		$this->add_rapp_function(0, _('Loa&n Request'),             'hrm/transactions/loan_request.php?',                  'SA_LOAN',             MENU_TRANSACTION);
		$this->add_rapp_function(0, _('Loan Repa&yment'),           'hrm/transactions/loan_repayment.php?',                'SA_LOAN',             MENU_TRANSACTION);
		$this->add_rapp_function(0, _('Employee &Separation'),      'hrm/transactions/employee_separation.php?',           'SA_EMPSEPARATION',    MENU_TRANSACTION);

		// ═══════════════════════════════════════════════════════════
		// Module 1: Inquiries and Reports
		// ═══════════════════════════════════════════════════════════
		$this->add_module(_('Inquiries and Reports'));
		$this->add_lapp_function(1, _('Attendance &Report'),        'hrm/inquiry/attendance_inquiry.php?',                 'SA_ATTINQUIRY',       MENU_INQUIRY);
		$this->add_lapp_function(1, _('Leave &Balance'),            'hrm/inquiry/leave_balance_inquiry.php?',              'SA_LEAVEINQUIRY',     MENU_INQUIRY);
		$this->add_lapp_function(1, _('Pa&yslip History'),          'hrm/inquiry/payslip_inquiry.php?',                    'SA_PAYSLIPINQUIRY',   MENU_INQUIRY);
		$this->add_lapp_function(1, _('Payroll &Summary'),          'hrm/inquiry/payroll_summary.php?',                    'SA_PAYROLLSUMMARY',   MENU_INQUIRY);

		$this->add_rapp_function(1, _('Employee &Directory'),       'hrm/inquiry/employee_directory.php?',                 'SA_EMPLOYEEREP',      MENU_INQUIRY);
		$this->add_rapp_function(1, _('Employee &History'),         'hrm/inquiry/employee_history.php?',                   'SA_EMPHISTORY',       MENU_INQUIRY);
		$this->add_rapp_function(1, _('&Department Costs'),         'hrm/inquiry/department_cost.php?',                    'SA_DEPTCOST',         MENU_INQUIRY);
		$this->add_rapp_function(1, _('Loan &Outstanding'),         'hrm/inquiry/loan_report.php?',                        'SA_LOANREPORT',       MENU_INQUIRY);
		$this->add_rapp_function(1, _('Employee Trans&actions'),    'hrm/inquiry/employee_trans_inquiry.php?',             'SA_EMPLOYEETRANSVIEW', MENU_INQUIRY);

		// ═══════════════════════════════════════════════════════════
		// Module 2: Maintenance
		// ═══════════════════════════════════════════════════════════
		$this->add_module(_('Maintenance'));
		$this->add_lapp_function(2, _('Manage &Employees'),         'hrm/manage/employees.php?',                           'SA_EMPLOYEE',         MENU_ENTRY);
		$this->add_lapp_function(2, _('&Departments'),              'hrm/manage/departments.php?',                         'SA_DEPARTMENT',        MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Job &Classification'),       'hrm/manage/job_classes.php?',                         'SA_JOBCLASS',          MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Job &Positions'),            'hrm/manage/job_positions.php?',                       'SA_POSITION',          MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Pay &Grades'),               'hrm/manage/pay_grades.php?',                          'SA_PAYGRADE',          MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Document T&ypes'),           'hrm/manage/doc_types.php?',                           'SA_DOCTYPE',           MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('&Overtime Types'),           'hrm/manage/overtime_types.php?',                      'SA_OVERTIME',          MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Lea&ve Types'),              'hrm/manage/leave_types.php?',                         'SA_LEAVETYPE',         MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Leave P&olicies'),           'hrm/manage/leave_policies.php?',                      'SA_LEAVEPOLICY',       MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Deduction Co&des'),          'hrm/manage/deduction_codes.php?',                     'SA_DEDUCTIONCODE',     MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Attendance Deduction R&ules'),'hrm/manage/attendance_deduction_rules.php?',          'SA_ATTDEDUCTRULE',     MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('R&ecruitment'),              'hrm/manage/recruitment.php?',                         'SA_HRSETTINGS',        MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('&Training'),                 'hrm/manage/training.php?',                            'SA_HRSETTINGS',        MENU_MAINTENANCE);

		$this->add_rapp_function(2, _('&Holiday Calendar'),         'hrm/manage/holidays.php?',                            'SA_HOLIDAY',           MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Wor&k Shifts'),              'hrm/manage/work_shifts.php?',                         'SA_WORKSHIFT',         MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Working Da&ys'),             'hrm/manage/working_days.php?',                        'SA_WORKINGDAYS',       MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('End of Ser&vice Tiers'),     'hrm/manage/eos_calculation.php?',                     'SA_EOSCALC',           MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Pay Ele&ments'),             'hrm/manage/pay_elements.php?',                        'SA_PAYELEMENT',        MENU_SETTINGS);
		$this->add_rapp_function(2, _('Sa&lary Structure'),         'hrm/manage/salary_structure.php?',                    'SA_SALARYSTRUCTURE',   MENU_SETTINGS);
		$this->add_rapp_function(2, _('&Tax Brackets'),             'hrm/manage/tax_brackets.php?',                        'SA_TAXBRACKET',        MENU_SETTINGS);
		$this->add_rapp_function(2, _('Statutory &Deductions'),     'hrm/manage/statutory_deductions.php?',                'SA_STATUTORY',         MENU_SETTINGS);
		$this->add_rapp_function(2, _('L&oan Types'),               'hrm/manage/loan_types.php?',                          'SA_LOANTYPE',          MENU_SETTINGS);
		$this->add_rapp_function(2, _('Import/Export &Employees'),  'hrm/manage/import_employees.php?',                    'SA_IMPORTEMP',         MENU_SETTINGS);
		$this->add_rapp_function(2, _('Import/Export Atte&ndance'), 'hrm/manage/import_attendance.php?',                   'SA_IMPORTATT',         MENU_SETTINGS);
		$this->add_rapp_function(2, _('Migration &Validation'),     'hrm/manage/migration_validation.php?',                'SA_HRSETTINGS',        MENU_SETTINGS);
		$this->add_rapp_function(2, _('Appraisa&ls'),               'hrm/manage/appraisals.php?',                          'SA_EMPLOYEE',          MENU_SETTINGS);
		$this->add_rapp_function(2, _('Asset Allo&cation'),         'hrm/manage/asset_allocation.php?',                    'SA_EMPLOYEE',          MENU_SETTINGS);
		$this->add_rapp_function(2, _('HR Settin&gs'),              'hrm/manage/hr_settings.php?',                         'SA_HRSETTINGS',        MENU_SETTINGS);

		$this->add_extensions();
	}
}
