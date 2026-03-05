-- =============================================================
-- HRM Module: New Tables (Phase 1-3)
-- Appended to en_US-new.sql and en_US-demo.sql
-- All tables prefixed with 0_ (TB_PREF)
-- =============================================================

-- ─────────────────────────────────────────────────────────────
-- ATTENDANCE DEDUCTION RULES
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_attendance_deduction_rules` --

DROP TABLE IF EXISTS `0_attendance_deduction_rules`;

CREATE TABLE IF NOT EXISTS `0_attendance_deduction_rules` (
	`rule_id`        int(11) NOT NULL AUTO_INCREMENT,
	`rule_type`      tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=absence_count_based, 1=late_minutes_based',
	`from_value`     double NOT NULL DEFAULT '0',
	`to_value`       double NOT NULL DEFAULT '0',
	`deduction_rate` double NOT NULL DEFAULT '0' COMMENT 'days of salary to deduct',
	`day_of_week`    tinyint(1) DEFAULT NULL COMMENT 'NULL=all days',
	`work_hours`     double DEFAULT '8',
	`inactive`       tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`rule_id`)
) ENGINE=InnoDB;

-- Data of table `0_attendance_deduction_rules` --

-- ─────────────────────────────────────────────────────────────
-- DEDUCTION CODES
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_deduction_codes` --

DROP TABLE IF EXISTS `0_deduction_codes`;

CREATE TABLE IF NOT EXISTS `0_deduction_codes` (
	`deduction_id`   int(11) NOT NULL AUTO_INCREMENT,
	`deduction_name` varchar(100) NOT NULL,
	`account_code`   varchar(15) DEFAULT NULL,
	`inactive`       tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`deduction_id`)
) ENGINE=InnoDB;

-- Data of table `0_deduction_codes` --

-- ─────────────────────────────────────────────────────────────
-- DOCUMENT TYPES
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_document_types` --

DROP TABLE IF EXISTS `0_document_types`;

CREATE TABLE IF NOT EXISTS `0_document_types` (
	`doc_type_id`   int(11) NOT NULL AUTO_INCREMENT,
	`type_name`     varchar(100) NOT NULL,
	`notify_before` int(11) DEFAULT '30' COMMENT 'days before expiry to alert',
	`is_required`   tinyint(1) DEFAULT '0',
	`inactive`      tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`doc_type_id`)
) ENGINE=InnoDB;

-- Data of table `0_document_types` --

INSERT INTO `0_document_types` (`type_name`, `notify_before`, `is_required`) VALUES
('Passport', 60, 1),
('National ID', 90, 1),
('Residence Permit', 60, 0),
('Work Permit', 60, 0),
('Driving License', 30, 0),
('Medical Certificate', 30, 0);

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE DEPENDENTS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_employee_dependents` --

DROP TABLE IF EXISTS `0_employee_dependents`;

CREATE TABLE IF NOT EXISTS `0_employee_dependents` (
	`dependent_id`   int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`    varchar(20) NOT NULL,
	`name`           varchar(100) NOT NULL,
	`relationship`   varchar(30) NOT NULL COMMENT 'spouse, child, parent, sibling',
	`birth_date`     date DEFAULT NULL,
	`gender`         tinyint(1) DEFAULT '0',
	`national_id`    varchar(100) DEFAULT NULL,
	`is_beneficiary` tinyint(1) DEFAULT '0',
	PRIMARY KEY (`dependent_id`),
	KEY `employee_id` (`employee_id`)
) ENGINE=InnoDB;

-- Data of table `0_employee_dependents` --

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE DOCUMENTS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_employee_documents` --

DROP TABLE IF EXISTS `0_employee_documents`;

CREATE TABLE IF NOT EXISTS `0_employee_documents` (
	`doc_id`        int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`   varchar(20) NOT NULL,
	`doc_type_id`   int(11) NOT NULL,
	`doc_name`      varchar(200) NOT NULL,
	`file_path`     varchar(500) DEFAULT NULL,
	`issue_date`    date DEFAULT NULL,
	`expiry_date`   date DEFAULT NULL,
	`notes`         text,
	`uploaded_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`uploaded_by`   smallint(6) DEFAULT NULL,
	PRIMARY KEY (`doc_id`),
	KEY `employee_id` (`employee_id`),
	KEY `expiry_date` (`expiry_date`)
) ENGINE=InnoDB;

-- Data of table `0_employee_documents` --

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE HISTORY
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_employee_history` --

DROP TABLE IF EXISTS `0_employee_history`;

CREATE TABLE IF NOT EXISTS `0_employee_history` (
	`history_id`        int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`       varchar(20) NOT NULL,
	`change_type`       varchar(30) NOT NULL COMMENT 'hire, transfer, promotion, salary_change, grade_change, separation',
	`effective_date`    date NOT NULL,
	`old_department_id` int(11) DEFAULT NULL,
	`new_department_id` int(11) DEFAULT NULL,
	`old_position_id`   int(11) DEFAULT NULL,
	`new_position_id`   int(11) DEFAULT NULL,
	`old_grade_id`      int(11) DEFAULT NULL,
	`new_grade_id`      int(11) DEFAULT NULL,
	`old_salary`        double DEFAULT NULL,
	`new_salary`        double DEFAULT NULL,
	`reason`            text,
	`approved_by`       varchar(20) DEFAULT NULL,
	`created_date`      timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by`        smallint(6) DEFAULT NULL,
	PRIMARY KEY (`history_id`),
	KEY `employee_id` (`employee_id`),
	KEY `effective_date` (`effective_date`)
) ENGINE=InnoDB;

-- Data of table `0_employee_history` --

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE LOANS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_employee_loans` --

DROP TABLE IF EXISTS `0_employee_loans`;

CREATE TABLE IF NOT EXISTS `0_employee_loans` (
	`loan_id`            int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`        varchar(20) NOT NULL,
	`loan_type_id`       int(11) NOT NULL,
	`loan_amount`        double NOT NULL DEFAULT '0',
	`interest_rate`      double DEFAULT '0',
	`installments`       int(11) NOT NULL DEFAULT '1',
	`installment_amount` double NOT NULL DEFAULT '0',
	`outstanding_amount` double NOT NULL DEFAULT '0',
	`loan_date`          date NOT NULL,
	`first_repayment`    date NOT NULL,
	`status`             tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=pending, 1=active, 2=completed, 3=cancelled',
	`approved_by`        varchar(20) DEFAULT NULL,
	`approval_date`      date DEFAULT NULL,
	`gl_trans_no`        int(11) DEFAULT NULL,
	`notes`              text,
	PRIMARY KEY (`loan_id`),
	KEY `employee_id` (`employee_id`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- Data of table `0_employee_loans` --

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE SALARY (personal overrides)
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_employee_salary` --

DROP TABLE IF EXISTS `0_employee_salary`;

CREATE TABLE IF NOT EXISTS `0_employee_salary` (
	`salary_id`      int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`    varchar(20) NOT NULL,
	`element_id`     int(11) NOT NULL,
	`amount`         double NOT NULL DEFAULT '0',
	`formula`        text DEFAULT NULL,
	`effective_from` date NOT NULL,
	`effective_to`   date DEFAULT NULL,
	`is_active`      tinyint(1) NOT NULL DEFAULT '1',
	`reference`      varchar(60) DEFAULT NULL,
	PRIMARY KEY (`salary_id`),
	UNIQUE KEY `unique_emp_element` (`employee_id`, `element_id`, `effective_from`),
	KEY `employee_id` (`employee_id`)
) ENGINE=InnoDB;

-- Data of table `0_employee_salary` --

-- ─────────────────────────────────────────────────────────────
-- EOS CALCULATION (End of Service tiers)
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_eos_calculation` --

DROP TABLE IF EXISTS `0_eos_calculation`;

CREATE TABLE IF NOT EXISTS `0_eos_calculation` (
	`eos_id`           int(11) NOT NULL AUTO_INCREMENT,
	`from_years`       double NOT NULL DEFAULT '0',
	`to_years`         double DEFAULT NULL,
	`termination_rate` double NOT NULL DEFAULT '0' COMMENT '% of monthly salary per year',
	`resignation_rate` double NOT NULL DEFAULT '0',
	`description`      text,
	PRIMARY KEY (`eos_id`)
) ENGINE=InnoDB;

-- Data of table `0_eos_calculation` --

-- ─────────────────────────────────────────────────────────────
-- HOLIDAYS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_holidays` --

DROP TABLE IF EXISTS `0_holidays`;

CREATE TABLE IF NOT EXISTS `0_holidays` (
	`holiday_id`   int(11) NOT NULL AUTO_INCREMENT,
	`holiday_name` varchar(100) NOT NULL,
	`holiday_date` date NOT NULL,
	`to_date`      date DEFAULT NULL COMMENT 'for multi-day holidays',
	`recurring`    tinyint(1) NOT NULL DEFAULT '0' COMMENT '1=repeats yearly',
	`is_paid`      tinyint(1) NOT NULL DEFAULT '1',
	`description`  text,
	PRIMARY KEY (`holiday_id`),
	KEY `holiday_date` (`holiday_date`)
) ENGINE=InnoDB;

-- Data of table `0_holidays` --

-- ─────────────────────────────────────────────────────────────
-- HR SETTINGS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_hr_settings` --

DROP TABLE IF EXISTS `0_hr_settings`;

CREATE TABLE IF NOT EXISTS `0_hr_settings` (
	`setting_id`    int(11) NOT NULL AUTO_INCREMENT,
	`setting_key`   varchar(60) NOT NULL,
	`setting_value` text,
	`description`   varchar(200) DEFAULT NULL,
	PRIMARY KEY (`setting_id`),
	UNIQUE KEY `setting_key` (`setting_key`)
) ENGINE=InnoDB;

-- Data of table `0_hr_settings` --

INSERT INTO `0_hr_settings` (`setting_key`, `setting_value`, `description`) VALUES
('payroll_cycle',               'monthly',           'Payroll frequency: monthly, biweekly, weekly'),
('probation_months',            '3',                 'Default probation period in months'),
('fiscal_year_start_month',     '1',                 'Month number for fiscal year start'),
('auto_leave_accrual',          '1',                 '1=auto-accrue leave monthly'),
('leave_year_reset_month',      '1',                 'Month to reset/carryforward leave balances'),
('overtime_calculation_base',   'basic',             'basic or gross for OT hourly rate'),
('tax_calculation_method',      'annual_projected',  'annual_projected or month_standalone'),
('payroll_approval_required',   '1',                 'Require approval before payment'),
('default_payment_method',      '0',                 '0=bank_transfer, 1=cash, 2=check'),
('attendance_deduction_type',   '0',                 '0=absence_based, 1=time_based'),
('employer_expense_account',    '',                  'Default employer contribution expense GL account'),
('tax_payable_account',         '',                  'Tax payable liability GL account'),
('loan_receivable_account',     '',                  'Default loan receivable GL account'),
('eos_calculation_factor',      '1',                 'Salary multiplication factor for EOS calculation'),
('eos_gratuity_account',        '',                  'Gratuity/EOS expense GL account');

-- ─────────────────────────────────────────────────────────────
-- LEAVE BALANCES
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_leave_balances` --

DROP TABLE IF EXISTS `0_leave_balances`;

CREATE TABLE IF NOT EXISTS `0_leave_balances` (
	`balance_id`      int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`     varchar(20) NOT NULL,
	`leave_id`        int(11) NOT NULL,
	`fiscal_year`     int(4) NOT NULL,
	`entitled`        double NOT NULL DEFAULT '0',
	`carried_forward` double NOT NULL DEFAULT '0',
	`taken`           double NOT NULL DEFAULT '0',
	`pending`         double NOT NULL DEFAULT '0' COMMENT 'pending approval',
	`adjusted`        double NOT NULL DEFAULT '0' COMMENT 'manual adjustment',
	PRIMARY KEY (`balance_id`),
	UNIQUE KEY `emp_leave_year` (`employee_id`, `leave_id`, `fiscal_year`)
) ENGINE=InnoDB;

-- Data of table `0_leave_balances` --

-- ─────────────────────────────────────────────────────────────
-- LEAVE POLICIES
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_leave_policies` --

DROP TABLE IF EXISTS `0_leave_policies`;

CREATE TABLE IF NOT EXISTS `0_leave_policies` (
	`policy_id`            int(11) NOT NULL AUTO_INCREMENT,
	`policy_name`          varchar(100) NOT NULL,
	`leave_id`             int(11) NOT NULL,
	`grade_id`             int(11) DEFAULT NULL COMMENT 'NULL=all grades',
	`employment_type`      tinyint(1) DEFAULT NULL COMMENT 'NULL=all types',
	`annual_entitlement`   double NOT NULL DEFAULT '0',
	`accrual_method`       tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=annual_grant, 1=monthly_accrual, 2=quarterly',
	`probation_applicable` tinyint(1) NOT NULL DEFAULT '0',
	`min_service_months`   int(11) NOT NULL DEFAULT '0',
	`effective_from`       date NOT NULL,
	`effective_to`         date DEFAULT NULL,
	`inactive`             tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`policy_id`),
	KEY `leave_id` (`leave_id`)
) ENGINE=InnoDB;

-- Data of table `0_leave_policies` --

-- ─────────────────────────────────────────────────────────────
-- LEAVE REQUESTS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_leave_requests` --

DROP TABLE IF EXISTS `0_leave_requests`;

CREATE TABLE IF NOT EXISTS `0_leave_requests` (
	`request_id`       int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`      varchar(20) NOT NULL,
	`leave_id`         int(11) NOT NULL,
	`from_date`        date NOT NULL,
	`to_date`          date NOT NULL,
	`days`             double NOT NULL DEFAULT '0',
	`half_day`         tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=full, 1=first_half, 2=second_half',
	`reason`           text,
	`status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=pending, 1=approved, 2=rejected, 3=cancelled',
	`approved_by`      varchar(20) DEFAULT NULL,
	`approval_date`    datetime DEFAULT NULL,
	`approval_remarks` text,
	`doc_attachment`   varchar(500) DEFAULT NULL,
	`request_date`     timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`request_id`),
	KEY `employee_id` (`employee_id`),
	KEY `status` (`status`),
	KEY `from_date` (`from_date`)
) ENGINE=InnoDB;

-- Data of table `0_leave_requests` --

-- ─────────────────────────────────────────────────────────────
-- LOAN REPAYMENTS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_loan_repayments` --

DROP TABLE IF EXISTS `0_loan_repayments`;

CREATE TABLE IF NOT EXISTS `0_loan_repayments` (
	`repayment_id`     int(11) NOT NULL AUTO_INCREMENT,
	`loan_id`          int(11) NOT NULL,
	`installment_no`   int(11) NOT NULL,
	`due_date`         date NOT NULL,
	`principal_amount` double NOT NULL DEFAULT '0',
	`interest_amount`  double NOT NULL DEFAULT '0',
	`total_amount`     double NOT NULL DEFAULT '0',
	`paid_amount`      double NOT NULL DEFAULT '0',
	`paid_date`        date DEFAULT NULL,
	`payslip_id`       int(11) DEFAULT NULL COMMENT 'linked payslip if auto-deducted',
	`status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=scheduled, 1=paid, 2=overdue',
	PRIMARY KEY (`repayment_id`),
	KEY `loan_id` (`loan_id`),
	KEY `due_date` (`due_date`)
) ENGINE=InnoDB;

-- Data of table `0_loan_repayments` --

-- ─────────────────────────────────────────────────────────────
-- LOAN TYPES
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_loan_types` --

DROP TABLE IF EXISTS `0_loan_types`;

CREATE TABLE IF NOT EXISTS `0_loan_types` (
	`loan_type_id`     int(11) NOT NULL AUTO_INCREMENT,
	`loan_type_name`   varchar(100) NOT NULL,
	`loan_type_code`   varchar(20) NOT NULL,
	`interest_rate`    double NOT NULL DEFAULT '0',
	`max_amount`       double DEFAULT NULL,
	`max_installments` int(11) DEFAULT NULL,
	`max_active_loans` int(11) NOT NULL DEFAULT '1',
	`account_code`     varchar(15) NOT NULL DEFAULT '' COMMENT 'loan receivable account',
	`interest_account` varchar(15) DEFAULT NULL,
	`inactive`         tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`loan_type_id`),
	UNIQUE KEY `loan_type_code` (`loan_type_code`)
) ENGINE=InnoDB;

-- Data of table `0_loan_types` --

-- ─────────────────────────────────────────────────────────────
-- OVERTIME BUDGET
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_overtime_budget` --

DROP TABLE IF EXISTS `0_overtime_budget`;

CREATE TABLE IF NOT EXISTS `0_overtime_budget` (
	`budget_id`     int(11) NOT NULL AUTO_INCREMENT,
	`fiscal_year`   int(4) NOT NULL,
	`month`         tinyint(2) NOT NULL,
	`budget_hours`  double NOT NULL DEFAULT '0',
	`budget_amount` double NOT NULL DEFAULT '0',
	PRIMARY KEY (`budget_id`),
	UNIQUE KEY `year_month` (`fiscal_year`, `month`)
) ENGINE=InnoDB;

-- Data of table `0_overtime_budget` --

-- ─────────────────────────────────────────────────────────────
-- OVERTIME REQUESTS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_overtime_requests` --

DROP TABLE IF EXISTS `0_overtime_requests`;

CREATE TABLE IF NOT EXISTS `0_overtime_requests` (
	`request_id`    int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`   varchar(20) NOT NULL,
	`overtime_id`   int(11) NOT NULL,
	`date`          date NOT NULL,
	`hours`         double NOT NULL DEFAULT '0',
	`reason`        text,
	`status`        tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=pending, 1=approved, 2=rejected',
	`approved_by`   varchar(20) DEFAULT NULL,
	`approval_date` datetime DEFAULT NULL,
	`request_date`  timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`request_id`),
	KEY `employee_id` (`employee_id`),
	KEY `date` (`date`)
) ENGINE=InnoDB;

-- Data of table `0_overtime_requests` --

-- ─────────────────────────────────────────────────────────────
-- PAYROLL PERIODS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_payroll_periods` --

DROP TABLE IF EXISTS `0_payroll_periods`;

CREATE TABLE IF NOT EXISTS `0_payroll_periods` (
	`period_id`           int(11) NOT NULL AUTO_INCREMENT,
	`period_name`         varchar(60) NOT NULL,
	`from_date`           date NOT NULL,
	`to_date`             date NOT NULL,
	`pay_date`            date DEFAULT NULL,
	`department_id`       int(11) DEFAULT NULL COMMENT 'NULL=all departments',
	`status`              tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=draft, 1=calculated, 2=approved, 3=posted, 4=paid, 5=closed, 6=voided',
	`total_gross`         double NOT NULL DEFAULT '0',
	`total_deductions`    double NOT NULL DEFAULT '0',
	`total_net`           double NOT NULL DEFAULT '0',
	`total_employer_cost` double NOT NULL DEFAULT '0',
	`approved_by`         varchar(20) DEFAULT NULL,
	`approval_date`       datetime DEFAULT NULL,
	`gl_trans_no`         int(11) DEFAULT NULL,
	`created_by`          smallint(6) DEFAULT NULL,
	`created_date`        timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`notes`               text,
	PRIMARY KEY (`period_id`),
	KEY `from_date` (`from_date`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- Data of table `0_payroll_periods` --

-- ─────────────────────────────────────────────────────────────
-- PAYSLIP DETAILS (line items per payslip)
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_payslip_details` --

DROP TABLE IF EXISTS `0_payslip_details`;

CREATE TABLE IF NOT EXISTS `0_payslip_details` (
	`detail_id`         int(11) NOT NULL AUTO_INCREMENT,
	`payslip_id`        int(11) NOT NULL,
	`element_id`        int(11) NOT NULL,
	`element_name`      varchar(100) NOT NULL COMMENT 'snapshot at payroll time',
	`element_category`  tinyint(1) NOT NULL DEFAULT '0',
	`is_deduction`      tinyint(1) NOT NULL DEFAULT '0',
	`amount_type`       tinyint(1) NOT NULL DEFAULT '0',
	`base_amount`       double NOT NULL DEFAULT '0' COMMENT 'calculation base',
	`rate`              double NOT NULL DEFAULT '0' COMMENT 'percentage rate if applicable',
	`calculated_amount` double NOT NULL DEFAULT '0',
	`adjusted_amount`   double DEFAULT NULL COMMENT 'manual override',
	`final_amount`      double NOT NULL DEFAULT '0',
	`account_code`      varchar(15) NOT NULL DEFAULT '',
	`formula_used`      text DEFAULT NULL COMMENT 'audit: formula that produced this',
	`is_taxable`        tinyint(1) NOT NULL DEFAULT '1',
	`display_order`     int(11) NOT NULL DEFAULT '0',
	`memo`              varchar(255) DEFAULT NULL,
	PRIMARY KEY (`detail_id`),
	KEY `payslip_id` (`payslip_id`),
	KEY `element_id` (`element_id`)
) ENGINE=InnoDB;

-- Data of table `0_payslip_details` --

-- ─────────────────────────────────────────────────────────────
-- STATUTORY DEDUCTIONS (social insurance, pension, etc.)
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_statutory_deductions` --

DROP TABLE IF EXISTS `0_statutory_deductions`;

CREATE TABLE IF NOT EXISTS `0_statutory_deductions` (
	`statutory_id`     int(11) NOT NULL AUTO_INCREMENT,
	`statutory_name`   varchar(100) NOT NULL,
	`statutory_code`   varchar(20) NOT NULL,
	`employee_rate`    double NOT NULL DEFAULT '0' COMMENT '% of applicable base',
	`employer_rate`    double NOT NULL DEFAULT '0',
	`employee_fixed`   double NOT NULL DEFAULT '0',
	`employer_fixed`   double NOT NULL DEFAULT '0',
	`ceiling_amount`   double DEFAULT NULL COMMENT 'max salary subject to this deduction',
	`floor_amount`     double DEFAULT NULL,
	`calculation_base` tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=basic, 1=gross',
	`employee_account` varchar(15) DEFAULT NULL,
	`employer_account` varchar(15) DEFAULT NULL,
	`effective_from`   date NOT NULL,
	`effective_to`     date DEFAULT NULL,
	`inactive`         tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`statutory_id`),
	UNIQUE KEY `statutory_code` (`statutory_code`)
) ENGINE=InnoDB;

-- Data of table `0_statutory_deductions` --

-- ─────────────────────────────────────────────────────────────
-- TAX BRACKETS (configurable income tax slabs)
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_tax_brackets` --

DROP TABLE IF EXISTS `0_tax_brackets`;

CREATE TABLE IF NOT EXISTS `0_tax_brackets` (
	`bracket_id`     int(11) NOT NULL AUTO_INCREMENT,
	`bracket_name`   varchar(60) NOT NULL,
	`from_amount`    double NOT NULL DEFAULT '0',
	`to_amount`      double DEFAULT NULL,
	`rate`           double NOT NULL DEFAULT '0' COMMENT 'tax percentage',
	`fixed_amount`   double NOT NULL DEFAULT '0' COMMENT 'fixed tax on lower portion',
	`effective_from` date NOT NULL,
	`effective_to`   date DEFAULT NULL,
	PRIMARY KEY (`bracket_id`),
	KEY `effective_from` (`effective_from`)
) ENGINE=InnoDB;

-- Data of table `0_tax_brackets` --

-- ─────────────────────────────────────────────────────────────
-- WORK SHIFTS
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_work_shifts` --

DROP TABLE IF EXISTS `0_work_shifts`;

CREATE TABLE IF NOT EXISTS `0_work_shifts` (
	`shift_id`        int(11) NOT NULL AUTO_INCREMENT,
	`shift_name`      varchar(60) NOT NULL,
	`start_time`      time NOT NULL,
	`end_time`        time NOT NULL,
	`break_duration`  int(11) NOT NULL DEFAULT '60' COMMENT 'minutes',
	`work_hours`      double NOT NULL DEFAULT '8.00',
	`is_night_shift`  tinyint(1) NOT NULL DEFAULT '0',
	`inactive`        tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`shift_id`)
) ENGINE=InnoDB;

-- Data of table `0_work_shifts` --

INSERT INTO `0_work_shifts` (`shift_name`, `start_time`, `end_time`, `break_duration`, `work_hours`) VALUES
('Regular Shift', '08:00:00', '17:00:00', 60, 8);

-- ─────────────────────────────────────────────────────────────
-- WORKING DAYS (weekly configuration)
-- ─────────────────────────────────────────────────────────────

-- Structure of table `0_working_days` --

DROP TABLE IF EXISTS `0_working_days`;

CREATE TABLE IF NOT EXISTS `0_working_days` (
	`id`          int(11) NOT NULL AUTO_INCREMENT,
	`day_of_week` tinyint(1) NOT NULL COMMENT '0=Sunday, 1=Monday ... 6=Saturday',
	`is_working`  tinyint(1) NOT NULL DEFAULT '1',
	`work_hours`  double NOT NULL DEFAULT '8',
	PRIMARY KEY (`id`),
	UNIQUE KEY `day_of_week` (`day_of_week`)
) ENGINE=InnoDB;

-- Data of table `0_working_days` -- (Mon-Fri working, Sat-Sun off)

INSERT INTO `0_working_days` (`day_of_week`, `is_working`, `work_hours`) VALUES
(0, 0, 0),
(1, 1, 8),
(2, 1, 8),
(3, 1, 8),
(4, 1, 8),
(5, 1, 8),
(6, 0, 0);

-- ─────────────────────────────────────────────────────────────
-- RECRUITMENT OPENINGS
-- ─────────────────────────────────────────────────────────────

DROP TABLE IF EXISTS `0_recruitment_openings`;

CREATE TABLE IF NOT EXISTS `0_recruitment_openings` (
	`opening_id`       int(11) NOT NULL AUTO_INCREMENT,
	`job_title`        varchar(120) NOT NULL,
	`department_id`    int(11) DEFAULT NULL,
	`position_id`      int(11) DEFAULT NULL,
	`headcount`        int(11) NOT NULL DEFAULT '1',
	`opening_date`     date NOT NULL,
	`closing_date`     date DEFAULT NULL,
	`status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=open,1=on_hold,2=closed,3=cancelled',
	`description`      text,
	`created_by`       smallint(6) DEFAULT NULL,
	`created_date`     timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`opening_id`),
	KEY `department_id` (`department_id`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- ─────────────────────────────────────────────────────────────
-- RECRUITMENT APPLICANTS
-- ─────────────────────────────────────────────────────────────

DROP TABLE IF EXISTS `0_recruitment_applicants`;

CREATE TABLE IF NOT EXISTS `0_recruitment_applicants` (
	`applicant_id`     int(11) NOT NULL AUTO_INCREMENT,
	`opening_id`       int(11) DEFAULT NULL,
	`full_name`        varchar(140) NOT NULL,
	`email`            varchar(120) DEFAULT NULL,
	`mobile`           varchar(40) DEFAULT NULL,
	`source`           varchar(80) DEFAULT NULL,
	`applied_date`     date NOT NULL,
	`status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=new,1=screened,2=interviewed,3=offered,4=hired,5=rejected',
	`expected_salary`  double DEFAULT NULL,
	`remarks`          text,
	PRIMARY KEY (`applicant_id`),
	KEY `opening_id` (`opening_id`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- ─────────────────────────────────────────────────────────────
-- TRAINING COURSES
-- ─────────────────────────────────────────────────────────────

DROP TABLE IF EXISTS `0_training_courses`;

CREATE TABLE IF NOT EXISTS `0_training_courses` (
	`course_id`        int(11) NOT NULL AUTO_INCREMENT,
	`course_code`      varchar(30) DEFAULT NULL,
	`course_name`      varchar(140) NOT NULL,
	`provider`         varchar(140) DEFAULT NULL,
	`default_hours`    double NOT NULL DEFAULT '0',
	`default_cost`     double NOT NULL DEFAULT '0',
	`inactive`         tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`course_id`),
	KEY `course_code` (`course_code`)
) ENGINE=InnoDB;

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE TRAINING RECORDS
-- ─────────────────────────────────────────────────────────────

DROP TABLE IF EXISTS `0_employee_training`;

CREATE TABLE IF NOT EXISTS `0_employee_training` (
	`training_id`      int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`      varchar(20) NOT NULL,
	`course_id`        int(11) NOT NULL,
	`training_date`    date NOT NULL,
	`completion_date`  date DEFAULT NULL,
	`status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=planned,1=in_progress,2=completed,3=cancelled',
	`score`            double DEFAULT NULL,
	`cost_amount`      double NOT NULL DEFAULT '0',
	`remarks`          text,
	PRIMARY KEY (`training_id`),
	KEY `employee_id` (`employee_id`),
	KEY `course_id` (`course_id`),
	KEY `training_date` (`training_date`)
) ENGINE=InnoDB;

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE APPRAISALS
-- ─────────────────────────────────────────────────────────────

DROP TABLE IF EXISTS `0_employee_appraisals`;

CREATE TABLE IF NOT EXISTS `0_employee_appraisals` (
	`appraisal_id`     int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`      varchar(20) NOT NULL,
	`reviewer_id`      varchar(20) DEFAULT NULL,
	`period_from`      date NOT NULL,
	`period_to`        date NOT NULL,
	`appraisal_date`   date NOT NULL,
	`overall_score`    double NOT NULL DEFAULT '0',
	`rating_scale`     tinyint(1) NOT NULL DEFAULT '5',
	`status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=draft,1=submitted,2=approved',
	`strengths`        text,
	`improvements`     text,
	`recommendation`   text,
	PRIMARY KEY (`appraisal_id`),
	KEY `employee_id` (`employee_id`),
	KEY `appraisal_date` (`appraisal_date`)
) ENGINE=InnoDB;

-- ─────────────────────────────────────────────────────────────
-- EMPLOYEE ASSET ALLOCATION
-- ─────────────────────────────────────────────────────────────

DROP TABLE IF EXISTS `0_employee_asset_allocation`;

CREATE TABLE IF NOT EXISTS `0_employee_asset_allocation` (
	`allocation_id`    int(11) NOT NULL AUTO_INCREMENT,
	`employee_id`      varchar(20) NOT NULL,
	`asset_name`       varchar(140) NOT NULL,
	`asset_code`       varchar(60) DEFAULT NULL,
	`serial_no`        varchar(80) DEFAULT NULL,
	`allocation_date`  date NOT NULL,
	`expected_return`  date DEFAULT NULL,
	`return_date`      date DEFAULT NULL,
	`status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=allocated,1=returned,2=lost,3=damaged',
	`notes`            text,
	PRIMARY KEY (`allocation_id`),
	KEY `employee_id` (`employee_id`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- =============================================================
-- END OF NEW HRM TABLES
-- =============================================================
