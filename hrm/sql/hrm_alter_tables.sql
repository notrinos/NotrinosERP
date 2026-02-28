-- =============================================================
-- HRM Module: ALTER existing tables to add new columns
-- Run after the base table definitions
-- =============================================================

-- ─────────────────────────────────────────────────────────────
-- 0_departments: add hierarchy, manager, cost-center, code
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_departments`
	ADD COLUMN IF NOT EXISTS `department_code`           varchar(20) DEFAULT NULL   AFTER `department_id`,
	ADD COLUMN IF NOT EXISTS `parent_department_id`      int(11) DEFAULT NULL       AFTER `department_name`,
	ADD COLUMN IF NOT EXISTS `manager_employee_id`       varchar(20) DEFAULT NULL   AFTER `parent_department_id`,
	ADD COLUMN IF NOT EXISTS `cost_center_id`            int(11) NOT NULL DEFAULT 0 AFTER `manager_employee_id`,
	ADD COLUMN IF NOT EXISTS `payroll_liability_account` varchar(15) DEFAULT NULL   AFTER `payroll_account`,
	ADD COLUMN IF NOT EXISTS `description`               text                       AFTER `payroll_liability_account`;

-- ─────────────────────────────────────────────────────────────
-- 0_job_classes: add code and description
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_job_classes`
	ADD COLUMN IF NOT EXISTS `class_code`  varchar(20) DEFAULT NULL AFTER `class_id`,
	ADD COLUMN IF NOT EXISTS `description` text                     AFTER `pay_type`;

-- ─────────────────────────────────────────────────────────────
-- 0_positions: add dept, reporting line, salary range, headcount
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_positions`
	ADD COLUMN IF NOT EXISTS `position_code`           varchar(20) DEFAULT NULL  AFTER `position_id`,
	ADD COLUMN IF NOT EXISTS `department_id`           int(11) DEFAULT NULL      AFTER `position_name`,
	ADD COLUMN IF NOT EXISTS `reports_to_position_id`  int(11) DEFAULT NULL      AFTER `job_class_id`,
	ADD COLUMN IF NOT EXISTS `min_salary`              double DEFAULT NULL        AFTER `basic_amount`,
	ADD COLUMN IF NOT EXISTS `max_salary`              double DEFAULT NULL        AFTER `min_salary`,
	ADD COLUMN IF NOT EXISTS `budgeted_headcount`      int(11) NOT NULL DEFAULT 1 AFTER `max_salary`,
	ADD COLUMN IF NOT EXISTS `is_manager`              tinyint(1) NOT NULL DEFAULT 0 AFTER `budgeted_headcount`,
	ADD COLUMN IF NOT EXISTS `description`             text                       AFTER `is_manager`;

-- ─────────────────────────────────────────────────────────────
-- 0_pay_grades: decouple from position, add salary range
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_pay_grades`
	ADD COLUMN IF NOT EXISTS `grade_code`  varchar(20) DEFAULT NULL AFTER `grade_id`,
	ADD COLUMN IF NOT EXISTS `grade_level` int(11) NOT NULL DEFAULT 0 AFTER `grade_name`,
	ADD COLUMN IF NOT EXISTS `min_salary`  double NOT NULL DEFAULT 0 AFTER `grade_level`,
	ADD COLUMN IF NOT EXISTS `mid_salary`  double NOT NULL DEFAULT 0 AFTER `min_salary`,
	ADD COLUMN IF NOT EXISTS `max_salary`  double NOT NULL DEFAULT 0 AFTER `mid_salary`,
	ADD COLUMN IF NOT EXISTS `description` text                     AFTER `max_salary`;

-- ─────────────────────────────────────────────────────────────
-- 0_pay_elements: add code, category, formula, employer GL
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_pay_elements`
	ADD COLUMN IF NOT EXISTS `element_code`      varchar(20) DEFAULT NULL      AFTER `element_id`,
	ADD COLUMN IF NOT EXISTS `element_category`  tinyint(1) NOT NULL DEFAULT 0 AFTER `element_name`,
	ADD COLUMN IF NOT EXISTS `formula`           text DEFAULT NULL              AFTER `amount`,
	ADD COLUMN IF NOT EXISTS `employer_account`  varchar(15) DEFAULT NULL       AFTER `account_code`,
	ADD COLUMN IF NOT EXISTS `is_taxable`        tinyint(1) NOT NULL DEFAULT 1  AFTER `employer_account`,
	ADD COLUMN IF NOT EXISTS `affects_gross`     tinyint(1) NOT NULL DEFAULT 1  AFTER `is_taxable`,
	ADD COLUMN IF NOT EXISTS `is_statutory`      tinyint(1) NOT NULL DEFAULT 0  AFTER `affects_gross`,
	ADD COLUMN IF NOT EXISTS `max_amount`        double DEFAULT NULL             AFTER `is_statutory`,
	ADD COLUMN IF NOT EXISTS `min_amount`        double DEFAULT NULL             AFTER `max_amount`,
	ADD COLUMN IF NOT EXISTS `display_order`     int(11) NOT NULL DEFAULT 0     AFTER `min_amount`,
	ADD COLUMN IF NOT EXISTS `description`       text                           AFTER `display_order`;

-- ─────────────────────────────────────────────────────────────
-- 0_salary_structure: add effective dates and structure_id PK
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_salary_structure`
	ADD COLUMN IF NOT EXISTS `effective_from` date NOT NULL DEFAULT '2000-01-01' AFTER `pay_amount`,
	ADD COLUMN IF NOT EXISTS `effective_to`   date DEFAULT NULL                  AFTER `effective_from`,
	ADD COLUMN IF NOT EXISTS `is_active`      tinyint(1) NOT NULL DEFAULT 1      AFTER `effective_to`,
	ADD COLUMN IF NOT EXISTS `formula`        text DEFAULT NULL                   AFTER `is_active`;

-- ─────────────────────────────────────────────────────────────
-- 0_attendance: add PK, shift, clock times, status, source
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_attendance`
	ADD COLUMN IF NOT EXISTS `attendance_id`   int(11) NOT NULL AUTO_INCREMENT FIRST,
	ADD COLUMN IF NOT EXISTS `shift_id`        int(11) DEFAULT NULL            AFTER `date`,
	ADD COLUMN IF NOT EXISTS `clock_in`        time DEFAULT NULL               AFTER `shift_id`,
	ADD COLUMN IF NOT EXISTS `clock_out`       time DEFAULT NULL               AFTER `clock_in`,
	ADD COLUMN IF NOT EXISTS `regular_hours`   double NOT NULL DEFAULT 0       AFTER `hours`,
	ADD COLUMN IF NOT EXISTS `overtime_hours`  double NOT NULL DEFAULT 0       AFTER `regular_hours`,
	ADD COLUMN IF NOT EXISTS `late_minutes`    int(11) NOT NULL DEFAULT 0      AFTER `overtime_hours`,
	ADD COLUMN IF NOT EXISTS `early_leave_min` int(11) NOT NULL DEFAULT 0      AFTER `late_minutes`,
	ADD COLUMN IF NOT EXISTS `status`          tinyint(1) NOT NULL DEFAULT 0   AFTER `early_leave_min`,
	ADD COLUMN IF NOT EXISTS `source`          tinyint(1) NOT NULL DEFAULT 0   AFTER `status`,
	ADD COLUMN IF NOT EXISTS `dimension_id`    int(11) NOT NULL DEFAULT 0      AFTER `rate`,
	ADD COLUMN IF NOT EXISTS `dimension2_id`   int(11) NOT NULL DEFAULT 0      AFTER `dimension_id`,
	ADD COLUMN IF NOT EXISTS `approved`        tinyint(1) NOT NULL DEFAULT 0   AFTER `dimension2_id`,
	ADD COLUMN IF NOT EXISTS `approved_by`     varchar(20) DEFAULT NULL        AFTER `approved`,
	ADD PRIMARY KEY IF NOT EXISTS (`attendance_id`);

-- ─────────────────────────────────────────────────────────────
-- 0_leave_types: add workflow, carry-forward, encashment cols
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_leave_types`
	ADD COLUMN IF NOT EXISTS `is_paid`              tinyint(1) NOT NULL DEFAULT 1   AFTER `pay_rate`,
	ADD COLUMN IF NOT EXISTS `is_carry_forward`     tinyint(1) NOT NULL DEFAULT 0   AFTER `is_paid`,
	ADD COLUMN IF NOT EXISTS `max_carry_forward`    double NOT NULL DEFAULT 0        AFTER `is_carry_forward`,
	ADD COLUMN IF NOT EXISTS `is_encashable`        tinyint(1) NOT NULL DEFAULT 0   AFTER `max_carry_forward`,
	ADD COLUMN IF NOT EXISTS `requires_document`    tinyint(1) NOT NULL DEFAULT 0   AFTER `is_encashable`,
	ADD COLUMN IF NOT EXISTS `max_consecutive_days` int(11) DEFAULT NULL             AFTER `requires_document`,
	ADD COLUMN IF NOT EXISTS `applicable_gender`    tinyint(1) DEFAULT NULL         AFTER `max_consecutive_days`,
	ADD COLUMN IF NOT EXISTS `color_code`           varchar(10) NOT NULL DEFAULT '#3498db' AFTER `applicable_gender`,
	ADD COLUMN IF NOT EXISTS `description`          text                            AFTER `color_code`;

-- ─────────────────────────────────────────────────────────────
-- 0_overtime: add code, limits, dimensions, approval flag
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_overtime`
	ADD COLUMN IF NOT EXISTS `overtime_code`      varchar(20) DEFAULT NULL        AFTER `overtime_id`,
	ADD COLUMN IF NOT EXISTS `max_hours_day`      double DEFAULT NULL              AFTER `pay_rate`,
	ADD COLUMN IF NOT EXISTS `max_hours_month`    double DEFAULT NULL              AFTER `max_hours_day`,
	ADD COLUMN IF NOT EXISTS `account_code`       varchar(15) DEFAULT NULL        AFTER `max_hours_month`,
	ADD COLUMN IF NOT EXISTS `dimension_id`       int(11) NOT NULL DEFAULT 0      AFTER `account_code`,
	ADD COLUMN IF NOT EXISTS `dimension2_id`      int(11) NOT NULL DEFAULT 0      AFTER `dimension_id`,
	ADD COLUMN IF NOT EXISTS `requires_approval`  tinyint(1) NOT NULL DEFAULT 1   AFTER `dimension2_id`;

-- ─────────────────────────────────────────────────────────────
-- 0_payslips: add period link, position snapshots, summary cols, status
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_payslips`
	ADD COLUMN IF NOT EXISTS `payroll_period_id`      int(11) DEFAULT NULL          AFTER `payslip_id`,
	ADD COLUMN IF NOT EXISTS `from_date`              date DEFAULT NULL              AFTER `tran_date`,
	ADD COLUMN IF NOT EXISTS `to_date`                date DEFAULT NULL              AFTER `from_date`,
	ADD COLUMN IF NOT EXISTS `department_id`          int(11) DEFAULT NULL          AFTER `to_date`,
	ADD COLUMN IF NOT EXISTS `position_id`            int(11) DEFAULT NULL          AFTER `department_id`,
	ADD COLUMN IF NOT EXISTS `grade_id`               int(11) DEFAULT NULL          AFTER `position_id`,
	ADD COLUMN IF NOT EXISTS `basic_salary`           double NOT NULL DEFAULT 0     AFTER `grade_id`,
	ADD COLUMN IF NOT EXISTS `total_earnings`         double NOT NULL DEFAULT 0     AFTER `basic_salary`,
	ADD COLUMN IF NOT EXISTS `gross_salary`           double NOT NULL DEFAULT 0     AFTER `total_earnings`,
	ADD COLUMN IF NOT EXISTS `total_deductions`       double NOT NULL DEFAULT 0     AFTER `gross_salary`,
	ADD COLUMN IF NOT EXISTS `net_salary`             double NOT NULL DEFAULT 0     AFTER `total_deductions`,
	ADD COLUMN IF NOT EXISTS `employer_contributions` double NOT NULL DEFAULT 0     AFTER `net_salary`,
	ADD COLUMN IF NOT EXISTS `total_cost`             double NOT NULL DEFAULT 0     AFTER `employer_contributions`,
	ADD COLUMN IF NOT EXISTS `working_days`           double NOT NULL DEFAULT 0     AFTER `total_cost`,
	ADD COLUMN IF NOT EXISTS `worked_days`            double NOT NULL DEFAULT 0     AFTER `working_days`,
	ADD COLUMN IF NOT EXISTS `overtime_hours`         double NOT NULL DEFAULT 0     AFTER `worked_days`,
	ADD COLUMN IF NOT EXISTS `leave_days`             double NOT NULL DEFAULT 0     AFTER `overtime_hours`,
	ADD COLUMN IF NOT EXISTS `absent_days`            double NOT NULL DEFAULT 0     AFTER `leave_days`,
	ADD COLUMN IF NOT EXISTS `loan_deduction`         double NOT NULL DEFAULT 0     AFTER `absent_days`,
	ADD COLUMN IF NOT EXISTS `status`                 tinyint(1) NOT NULL DEFAULT 0 AFTER `loan_deduction`,
	ADD COLUMN IF NOT EXISTS `payment_trans_no`       int(11) DEFAULT NULL          AFTER `status`,
	ADD COLUMN IF NOT EXISTS `custom_data`            JSON NOT NULL DEFAULT ('{}')  AFTER `payment_trans_no`;

-- ─────────────────────────────────────────────────────────────
-- 0_employees: add extended fields
-- ─────────────────────────────────────────────────────────────
ALTER TABLE `0_employees`
	ADD COLUMN IF NOT EXISTS `middle_name`         varchar(100) DEFAULT NULL   AFTER `last_name`,
	ADD COLUMN IF NOT EXISTS `nationality`         varchar(60) DEFAULT NULL    AFTER `gender`,
	ADD COLUMN IF NOT EXISTS `city`                varchar(60) DEFAULT NULL    AFTER `address`,
	ADD COLUMN IF NOT EXISTS `state`               varchar(60) DEFAULT NULL    AFTER `city`,
	ADD COLUMN IF NOT EXISTS `country`             varchar(60) DEFAULT NULL    AFTER `state`,
	ADD COLUMN IF NOT EXISTS `phone`               varchar(30) DEFAULT NULL    AFTER `country`,
	ADD COLUMN IF NOT EXISTS `personal_email`      varchar(100) DEFAULT NULL   AFTER `email`,
	ADD COLUMN IF NOT EXISTS `passport_expiry`     date DEFAULT NULL           AFTER `passport`,
	ADD COLUMN IF NOT EXISTS `social_security_no`  varchar(100) DEFAULT NULL   AFTER `tax_number`,
	ADD COLUMN IF NOT EXISTS `bank_name`           varchar(100) DEFAULT NULL   AFTER `bank_account`,
	ADD COLUMN IF NOT EXISTS `bank_branch`         varchar(100) DEFAULT NULL   AFTER `bank_name`,
	ADD COLUMN IF NOT EXISTS `bank_routing`        varchar(60) DEFAULT NULL    AFTER `bank_branch`,
	ADD COLUMN IF NOT EXISTS `payment_method`      tinyint(1) NOT NULL DEFAULT 0 AFTER `bank_routing`,
	ADD COLUMN IF NOT EXISTS `confirmation_date`   date DEFAULT NULL           AFTER `hire_date`,
	ADD COLUMN IF NOT EXISTS `probation_end_date`  date DEFAULT NULL           AFTER `confirmation_date`,
	ADD COLUMN IF NOT EXISTS `separation_reason`   tinyint(1) DEFAULT NULL     AFTER `released_date`,
	ADD COLUMN IF NOT EXISTS `employment_type`     tinyint(1) NOT NULL DEFAULT 0 AFTER `separation_reason`,
	ADD COLUMN IF NOT EXISTS `shift_id`            int(11) DEFAULT NULL        AFTER `grade_id`,
	ADD COLUMN IF NOT EXISTS `reporting_to`        varchar(20) DEFAULT NULL    AFTER `shift_id`,
	ADD COLUMN IF NOT EXISTS `cost_center_id`      int(11) DEFAULT NULL        AFTER `reporting_to`,
	ADD COLUMN IF NOT EXISTS `dimension2_id`       int(11) NOT NULL DEFAULT 0  AFTER `cost_center_id`,
	ADD COLUMN IF NOT EXISTS `emergency_name`      varchar(100) DEFAULT NULL   AFTER `dimension2_id`,
	ADD COLUMN IF NOT EXISTS `emergency_relation`  varchar(60) DEFAULT NULL    AFTER `emergency_name`,
	ADD COLUMN IF NOT EXISTS `emergency_phone`     varchar(30) DEFAULT NULL    AFTER `emergency_relation`,
	ADD KEY IF NOT EXISTS `grade_id` (`grade_id`),
	ADD KEY IF NOT EXISTS `reporting_to` (`reporting_to`);

-- =============================================================
-- END OF ALTER STATEMENTS
-- =============================================================
