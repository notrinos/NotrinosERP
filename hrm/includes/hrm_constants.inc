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

// HRM module constants and enums
// ============================================================

// ─── Gender ─────────────────────────────────────────────────
define('HRM_GENDER_MALE',   0);
define('HRM_GENDER_FEMALE', 1);

// ─── Marital Status ─────────────────────────────────────────
define('HRM_MARITAL_SINGLE',   0);
define('HRM_MARITAL_MARRIED',  1);
define('HRM_MARITAL_DIVORCED', 2);
define('HRM_MARITAL_WIDOWED',  3);

// ─── Employment Type ────────────────────────────────────────
define('HRM_EMP_PERMANENT',  0);
define('HRM_EMP_CONTRACT',   1);
define('HRM_EMP_PROBATION',  2);
define('HRM_EMP_INTERN',     3);
define('HRM_EMP_PARTTIME',   4);

// ─── Employment Status ──────────────────────────────────────
define('HRM_STATUS_ACTIVE',      'active');
define('HRM_STATUS_INACTIVE',    'inactive');
define('HRM_STATUS_TERMINATED',  'terminated');
define('HRM_STATUS_RESIGNED',    'resigned');
define('HRM_STATUS_RETIRED',     'retired');
define('HRM_STATUS_SUSPENDED',   'suspended');

// ─── Separation Reason ──────────────────────────────────────
define('HRM_SEP_RESIGN',        0);
define('HRM_SEP_TERMINATE',     1);
define('HRM_SEP_RETIRE',        2);
define('HRM_SEP_CONTRACT_END',  3);
define('HRM_SEP_DEATH',         4);

// ─── Payment Method ─────────────────────────────────────────
define('HRM_PAY_BANK_TRANSFER', 0);
define('HRM_PAY_CASH',          1);
define('HRM_PAY_CHEQUE',        2);

// ─── Pay Basis (Job Class) ──────────────────────────────────
define('HRM_PAY_MONTHLY', 0);
define('HRM_PAY_DAILY',   1);
define('HRM_PAY_HOURLY',  2);

// ─── Pay Element Category ───────────────────────────────────
define('HRM_ELEM_BASIC',               0);
define('HRM_ELEM_ALLOWANCE',           1);
define('HRM_ELEM_DEDUCTION',           2);
define('HRM_ELEM_EMPLOYER_CONTRIB',    3);
define('HRM_ELEM_STATUTORY_DEDUCTION', 4);
define('HRM_ELEM_BONUS',               5);
define('HRM_ELEM_REIMBURSEMENT',       6);

// ─── Pay Element Amount Type ────────────────────────────────
define('HRM_AMTTYPE_FIXED',          0);
define('HRM_AMTTYPE_PCT_BASIC',      1);
define('HRM_AMTTYPE_PCT_GROSS',      2);
define('HRM_AMTTYPE_FORMULA',        3);
define('HRM_AMTTYPE_ATTENDANCE',     4);

// ─── Approval / Workflow Status ─────────────────────────────
define('HRM_STATUS_PENDING',   'pending');
define('HRM_STATUS_APPROVED',  'approved');
define('HRM_STATUS_REJECTED',  'rejected');
define('HRM_STATUS_CANCELLED', 'cancelled');

// ─── Leave Types ────────────────────────────────────────────
define('HRM_LEAVE_PAID',   1);
define('HRM_LEAVE_UNPAID', 0);

// ─── Attendance Status ──────────────────────────────────────
define('HRM_ATT_PRESENT',    'present');
define('HRM_ATT_ABSENT',     'absent');
define('HRM_ATT_HALF_DAY',   'half_day');
define('HRM_ATT_ON_LEAVE',   'on_leave');
define('HRM_ATT_HOLIDAY',    'holiday');
define('HRM_ATT_WEEKEND',    'weekend');
define('HRM_ATT_REMOTE',     'remote');

// ─── Overtime Type ──────────────────────────────────────────
define('HRM_OT_REGULAR',  'regular');
define('HRM_OT_HOLIDAY',  'holiday');
define('HRM_OT_REST_DAY', 'rest_day');

// ─── Payslip Status ─────────────────────────────────────────
define('HRM_SLIP_DRAFT',    'draft');
define('HRM_SLIP_APPROVED', 'approved');
define('HRM_SLIP_POSTED',   'posted');
define('HRM_SLIP_VOIDED',   'voided');
define('HRM_SLIP_PAID',     'paid');

// ─── Payroll Period Status ──────────────────────────────────
define('HRM_PERIOD_DRAFT',    'draft');
define('HRM_PERIOD_OPEN',     'open');
define('HRM_PERIOD_LOCKED',   'locked');
define('HRM_PERIOD_POSTED',   'posted');
define('HRM_PERIOD_CLOSED',   'closed');

// ─── Payroll Period Type ────────────────────────────────────
define('HRM_PERIOD_MONTHLY',     0);
define('HRM_PERIOD_BIWEEKLY',    1);
define('HRM_PERIOD_WEEKLY',      2);
define('HRM_PERIOD_SEMIMONTHLY', 3);

// ─── Loan Status ────────────────────────────────────────────
define('HRM_LOAN_PENDING',   'pending');
define('HRM_LOAN_APPROVED',  'approved');
define('HRM_LOAN_ACTIVE',    'active');
define('HRM_LOAN_SETTLED',   'settled');
define('HRM_LOAN_CANCELLED', 'cancelled');

// ─── Employee History Change Types ──────────────────────────
define('HRM_HIST_HIRE',         'hire');
define('HRM_HIST_TRANSFER',     'transfer');
define('HRM_HIST_PROMOTION',    'promotion');
define('HRM_HIST_SALARY_CHANGE','salary_change');
define('HRM_HIST_GRADE_CHANGE', 'grade_change');
define('HRM_HIST_SEPARATION',   'separation');
define('HRM_HIST_SUSPENSION',   'suspension');
define('HRM_HIST_REINSTATE',    'reinstatement');

// ─── Attendance Deduction Rule Types ────────────────────────
define('HRM_DEDTYPE_ABSENCE',   0);  // deduct per absence day
define('HRM_DEDTYPE_LATENESS',  1);  // deduct per late minute
define('HRM_DEDTYPE_EARLY_EXIT',2);  // deduct per early-exit minute

// ─── Working Days (0=Sun … 6=Sat, PHP convention) ───────────
define('HRM_DAY_SUNDAY',    0);
define('HRM_DAY_MONDAY',    1);
define('HRM_DAY_TUESDAY',   2);
define('HRM_DAY_WEDNESDAY', 3);
define('HRM_DAY_THURSDAY',  4);
define('HRM_DAY_FRIDAY',    5);
define('HRM_DAY_SATURDAY',  6);

// ─── EOS (End of Service) Calculation Bases ─────────────────
define('HRM_EOS_BASIC',   0);   // EOS based on basic salary
define('HRM_EOS_GROSS',   1);   // EOS based on gross

// ─── Statutory Deduction Calculation Base ───────────────────
define('HRM_STAT_BASE_BASIC', 0);
define('HRM_STAT_BASE_GROSS', 1);

// ─── Report IDs (Class 8: Human Resource) ───────────────────
define('HRM_REP_PAYSLIP',           880);
define('HRM_REP_PAYROLL_SUMMARY',   881);
define('HRM_REP_SALARY_SHEET',      882);
define('HRM_REP_ATTENDANCE',        883);
define('HRM_REP_LEAVE_BALANCE',     884);
define('HRM_REP_LEAVE_COMPUTATION', 885);
define('HRM_REP_EMPLOYEE_LIST',     886);
define('HRM_REP_EMPLOYEE_PROFILE',  887);
define('HRM_REP_DOC_EXPIRY',        888);
define('HRM_REP_OVERTIME',          889);
define('HRM_REP_DEPT_COST',         890);
define('HRM_REP_LOAN_OUTSTANDING',  891);
define('HRM_REP_HEADCOUNT',         892);
define('HRM_REP_EMAIL_PAYSLIP',     893);

// ─── Lookup arrays for display ──────────────────────────────
function hrm_get_employment_types() {
    return array(
        HRM_EMP_PERMANENT => _('Permanent'),
        HRM_EMP_CONTRACT  => _('Contract'),
        HRM_EMP_PROBATION => _('Probation'),
        HRM_EMP_INTERN    => _('Intern'),
        HRM_EMP_PARTTIME  => _('Part-time'),
    );
}

function hrm_get_pay_basis_types() {
    return array(
        HRM_PAY_MONTHLY => _('Monthly'),
        HRM_PAY_DAILY   => _('Daily'),
        HRM_PAY_HOURLY  => _('Hourly'),
    );
}

function hrm_get_gender_types() {
    return array(
        HRM_GENDER_MALE   => _('Male'),
        HRM_GENDER_FEMALE => _('Female'),
    );
}

function hrm_get_marital_types() {
    return array(
        HRM_MARITAL_SINGLE   => _('Single'),
        HRM_MARITAL_MARRIED  => _('Married'),
        HRM_MARITAL_DIVORCED => _('Divorced'),
        HRM_MARITAL_WIDOWED  => _('Widowed'),
    );
}

function hrm_get_element_categories() {
    return array(
        HRM_ELEM_BASIC               => _('Basic Salary'),
        HRM_ELEM_ALLOWANCE           => _('Allowance'),
        HRM_ELEM_DEDUCTION           => _('Deduction'),
        HRM_ELEM_EMPLOYER_CONTRIB    => _('Employer Contribution'),
        HRM_ELEM_STATUTORY_DEDUCTION => _('Statutory Deduction'),
        HRM_ELEM_BONUS               => _('Bonus'),
        HRM_ELEM_REIMBURSEMENT       => _('Reimbursement'),
    );
}

function hrm_get_amount_types() {
    return array(
        HRM_AMTTYPE_FIXED      => _('Fixed Amount'),
        HRM_AMTTYPE_PCT_BASIC  => _('% of Basic'),
        HRM_AMTTYPE_PCT_GROSS  => _('% of Gross'),
        HRM_AMTTYPE_FORMULA    => _('Formula'),
        HRM_AMTTYPE_ATTENDANCE => _('Attendance Based'),
    );
}

function hrm_get_attendance_statuses() {
    return array(
        HRM_ATT_PRESENT  => _('Present'),
        HRM_ATT_ABSENT   => _('Absent'),
        HRM_ATT_HALF_DAY => _('Half Day'),
        HRM_ATT_ON_LEAVE => _('On Leave'),
        HRM_ATT_HOLIDAY  => _('Holiday'),
        HRM_ATT_WEEKEND  => _('Weekend'),
        HRM_ATT_REMOTE   => _('Remote / WFH'),
    );
}

function hrm_get_approval_statuses() {
    return array(
        HRM_STATUS_PENDING   => _('Pending'),
        HRM_STATUS_APPROVED  => _('Approved'),
        HRM_STATUS_REJECTED  => _('Rejected'),
        HRM_STATUS_CANCELLED => _('Cancelled'),
    );
}

function hrm_get_payslip_statuses() {
    return array(
        HRM_SLIP_DRAFT    => _('Draft'),
        HRM_SLIP_APPROVED => _('Approved'),
        HRM_SLIP_POSTED   => _('Posted'),
        HRM_SLIP_PAID     => _('Paid'),
        HRM_SLIP_VOIDED   => _('Voided'),
    );
}

function hrm_get_separation_reasons() {
    return array(
        HRM_SEP_RESIGN       => _('Resignation'),
        HRM_SEP_TERMINATE    => _('Termination'),
        HRM_SEP_RETIRE       => _('Retirement'),
        HRM_SEP_CONTRACT_END => _('Contract End'),
        HRM_SEP_DEATH        => _('Death'),
    );
}

function hrm_get_payment_methods() {
    return array(
        HRM_PAY_BANK_TRANSFER => _('Bank Transfer'),
        HRM_PAY_CASH          => _('Cash'),
        HRM_PAY_CHEQUE        => _('Cheque'),
    );
}

function hrm_get_period_types() {
    return array(
        HRM_PERIOD_MONTHLY     => _('Monthly'),
        HRM_PERIOD_BIWEEKLY    => _('Bi-weekly'),
        HRM_PERIOD_WEEKLY      => _('Weekly'),
        HRM_PERIOD_SEMIMONTHLY => _('Semi-monthly'),
    );
}

function hrm_get_weekday_names() {
    return array(
        HRM_DAY_SUNDAY    => _('Sunday'),
        HRM_DAY_MONDAY    => _('Monday'),
        HRM_DAY_TUESDAY   => _('Tuesday'),
        HRM_DAY_WEDNESDAY => _('Wednesday'),
        HRM_DAY_THURSDAY  => _('Thursday'),
        HRM_DAY_FRIDAY    => _('Friday'),
        HRM_DAY_SATURDAY  => _('Saturday'),
    );
}

// ─── Security area check helpers ────────────────────────────
function hrm_check_access($area) {
    return user_check_access($area);
}

function hrm_assert_access($area) {
    if (!user_check_access($area)) {
        display_error(_('You do not have permission to access this area.'));
        end_page();
        exit();
    }
}

