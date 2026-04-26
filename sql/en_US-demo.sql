
SET NAMES utf8;

-- Structure of table `0_areas` --

DROP TABLE IF EXISTS `0_areas`;

CREATE TABLE `0_areas` (
	`area_code` int(11) NOT NULL AUTO_INCREMENT,
	`description` varchar(60) NOT NULL DEFAULT '',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`area_code`),
	UNIQUE KEY `description` (`description`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_areas` --

INSERT INTO `0_areas` VALUES
('1', 'Global', '0', '{}');

-- Structure of table `0_attachments` --

DROP TABLE IF EXISTS `0_attachments`;

CREATE TABLE `0_attachments` (
	`id` int(11) unsigned NOT NULL AUTO_INCREMENT,
	`description` varchar(60) NOT NULL DEFAULT '',
	`type_no` int(11) NOT NULL DEFAULT '0',
	`trans_no` int(11) NOT NULL DEFAULT '0',
	`unique_name` varchar(60) NOT NULL DEFAULT '',
	`tran_date` date NOT NULL DEFAULT '0000-00-00',
	`filename` varchar(60) NOT NULL DEFAULT '',
	`filesize` int(11) NOT NULL DEFAULT '0',
	`filetype` varchar(60) NOT NULL DEFAULT '',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `type_no` (`type_no`,`trans_no`)
) ENGINE=InnoDB;

-- Data of table `0_attachments` --

-- Structure of table `0_attendance` --

DROP TABLE IF EXISTS `0_attendance`;
CREATE TABLE IF NOT EXISTS `0_attendance` (
	`attendance_id`       int(11)         NOT NULL AUTO_INCREMENT,
	`employee_id`         varchar(20)     NOT NULL DEFAULT '',
	`date`                date            NOT NULL,
	`shift_id`            int(11)         DEFAULT NULL,
	`clock_in`            time            DEFAULT NULL,
	`clock_out`           time            DEFAULT NULL,
	`regular_hours`       double          NOT NULL DEFAULT '0',
	`overtime_hours`      double          NOT NULL DEFAULT '0',
	`overtime_type_id`    int(11)         DEFAULT NULL,
	`late_minutes`        int(11)         NOT NULL DEFAULT '0',
	`early_leave_min`     int(11)         NOT NULL DEFAULT '0',
	`status`              tinyint(1)      NOT NULL DEFAULT '0' COMMENT '0=present, 1=absent, 2=half_day, 3=on_leave, 4=holiday, 5=weekend',
	`source`              tinyint(1)      NOT NULL DEFAULT '0' COMMENT '0=manual, 1=biometric, 2=import',
	`rate`                double          NOT NULL DEFAULT '1',
	`dimension_id`        int(11)         NOT NULL DEFAULT '0',
	`dimension2_id`       int(11)         NOT NULL DEFAULT '0',
	`notes`               varchar(200)    DEFAULT NULL,
	`approved`            tinyint(1)      NOT NULL DEFAULT '0',
	`approved_by`         varchar(20)     DEFAULT NULL,
	PRIMARY KEY (`attendance_id`),
	UNIQUE KEY `emp_date` (`employee_id`,`date`),
	KEY `date` (`date`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- Data of table `0_attendance` --

-- Structure of table `0_audit_trail` --

DROP TABLE IF EXISTS `0_audit_trail`;

CREATE TABLE `0_audit_trail` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`type` smallint(6) unsigned NOT NULL DEFAULT '0',
	`trans_no` int(11) unsigned NOT NULL DEFAULT '0',
	`user` smallint(6) unsigned NOT NULL DEFAULT '0',
	`stamp` timestamp NOT NULL,
	`description` varchar(60) DEFAULT NULL,
	`fiscal_year` int(11) NOT NULL DEFAULT '0',
	`gl_date` date NOT NULL DEFAULT '0000-00-00',
	`gl_seq` int(11) unsigned DEFAULT NULL,
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `Seq` (`fiscal_year`,`gl_date`,`gl_seq`),
	KEY `Type_and_Number` (`type`,`trans_no`)
) ENGINE=InnoDB AUTO_INCREMENT=37 ;

-- Data of table `0_audit_trail` --

INSERT INTO `0_audit_trail` VALUES
('1', '18', '1', '1', '2025-05-05 14:08:02', NULL, '1', '2025-05-05', '0', '{}'),
('2', '25', '1', '1', '2025-05-05 14:08:14', NULL, '1', '2025-05-05', '1', '{}'),
('3', '30', '1', '1', '2025-05-05 14:09:54', NULL, '1', '2025-05-10', '0', '{}'),
('4', '13', '1', '1', '2025-05-05 14:09:55', NULL, '1', '2025-05-10', '13', '{}'),
('5', '10', '1', '1', '2025-05-05 14:09:55', NULL, '1', '2025-05-10', '14', '{}'),
('6', '12', '1', '1', '2025-05-05 14:09:55', NULL, '1', '2025-05-10', '15', '{}'),
('7', '29', '1', '1', '2025-05-05 14:18:49', 'Quick production.', '1', '2025-05-05', '2', '{}'),
('8', '18', '2', '1', '2025-05-05 14:22:32', NULL, '1', '2025-05-05', '0', '{}'),
('9', '25', '2', '1', '2025-05-05 14:22:32', NULL, '1', '2025-05-05', '3', '{}'),
('10', '20', '1', '1', '2025-05-05 14:22:32', NULL, '1', '2025-05-05', '4', '{}'),
('11', '30', '2', '1', '2025-05-07 07:55:15', NULL, '1', '2025-05-07', '0', '{}'),
('12', '13', '2', '1', '2025-05-07 07:55:16', NULL, '1', '2025-05-07', '7', '{}'),
('13', '10', '2', '1', '2025-05-07 07:55:16', NULL, '1', '2025-05-07', '8', '{}'),
('14', '12', '2', '1', '2025-05-07 07:55:16', NULL, '1', '2025-05-07', '9', '{}'),
('15', '30', '3', '1', '2025-05-07 08:08:24', NULL, '1', '2025-05-07', '0', '{}'),
('16', '30', '4', '1', '2025-05-07 09:18:44', NULL, '1', '2025-05-07', '0', '{}'),
('17', '30', '5', '1', '2025-05-07 11:42:41', NULL, '1', '2025-05-07', '0', '{}'),
('18', '13', '3', '1', '2025-05-07 11:42:41', NULL, '1', '2025-05-07', '10', '{}'),
('19', '10', '3', '1', '2025-05-07 11:42:41', NULL, '1', '2025-05-07', '11', '{}'),
('20', '30', '6', '1', '2025-05-07 14:02:35', NULL, '1', '2025-05-07', '0', '{}'),
('21', '30', '7', '1', '2025-05-07 14:05:38', NULL, '1', '2025-05-07', '0', '{}'),
('22', '13', '4', '1', '2025-05-07 14:05:38', NULL, '1', '2025-05-07', '0', '{}'),
('23', '10', '4', '1', '2025-05-07 14:05:38', NULL, '1', '2025-05-07', '0', '{}'),
('24', '12', '3', '1', '2025-05-07 14:05:38', NULL, '1', '2025-05-07', '0', '{}'),
('25', '26', '1', '1', '2025-05-07 15:59:34', NULL, '1', '2025-05-07', NULL, '{}'),
('26', '29', '1', '1', '2025-05-07 15:59:01', 'Production.', '1', '2025-05-07', '5', '{}'),
('27', '26', '1', '1', '2025-05-07 15:59:34', 'Released.', '1', '2025-05-07', '6', '{}'),
('28', '1', '1', '1', '2025-05-07 16:01:00', NULL, '1', '2025-05-07', '12', '{}'),
('29', '30', '8', '1', '2026-01-21 11:13:06', NULL, '2', '2026-01-21', '0', '{}'),
('30', '13', '5', '1', '2026-01-21 11:13:06', NULL, '2', '2026-01-21', '0', '{}'),
('31', '10', '5', '1', '2026-01-21 11:13:06', NULL, '2', '2026-01-21', '0', '{}'),
('32', '12', '4', '1', '2026-01-21 11:13:06', NULL, '2', '2026-01-21', '0', '{}'),
('33', '18', '3', '1', '2026-01-21 11:14:14', NULL, '2', '2026-01-21', '0', '{}'),
('34', '25', '3', '1', '2026-01-21 11:14:14', NULL, '2', '2026-01-21', '0', '{}'),
('35', '20', '2', '1', '2026-01-21 11:14:14', NULL, '2', '2026-01-21', '0', '{}'),
('36', '0', '1', '1', '2026-01-21 11:15:35', NULL, '1', '2025-12-31', '16', '{}');

-- Structure of table `0_bank_accounts` --

DROP TABLE IF EXISTS `0_bank_accounts`;

CREATE TABLE `0_bank_accounts` (
	`account_code` varchar(15) NOT NULL DEFAULT '',
	`account_type` smallint(6) NOT NULL DEFAULT '0',
	`bank_account_name` varchar(60) NOT NULL DEFAULT '',
	`bank_account_number` varchar(100) NOT NULL DEFAULT '',
	`bank_name` varchar(60) NOT NULL DEFAULT '',
	`bank_address` tinytext,
	`bank_curr_code` char(3) NOT NULL DEFAULT '',
	`dflt_curr_act` tinyint(1) NOT NULL DEFAULT '0',
	`id` smallint(6) NOT NULL AUTO_INCREMENT,
	`bank_charge_act` varchar(15) NOT NULL DEFAULT '',
	`last_reconciled_date` timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
	`ending_reconcile_balance` double NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `bank_account_name` (`bank_account_name`),
	KEY `bank_account_number` (`bank_account_number`),
	KEY `account_code` (`account_code`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_bank_accounts` --

INSERT INTO `0_bank_accounts` VALUES
('1060', '0', 'Current account', 'N/A', 'N/A', NULL, 'USD', '1', '1', '5690', '0000-00-00 00:00:00', '0', '0', '{}'),
('1065', '3', 'Petty Cash account', 'N/A', 'N/A', NULL, 'USD', '0', '2', '5690', '0000-00-00 00:00:00', '0', '0', '{}');

-- Structure of table `0_bank_trans` --

DROP TABLE IF EXISTS `0_bank_trans`;

CREATE TABLE `0_bank_trans` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`type` smallint(6) DEFAULT NULL,
	`trans_no` int(11) DEFAULT NULL,
	`bank_act` varchar(15) NOT NULL DEFAULT '',
	`ref` varchar(40) DEFAULT NULL,
	`trans_date` date NOT NULL DEFAULT '0000-00-00',
	`amount` double DEFAULT NULL,
	`dimension_id` int(11) NOT NULL DEFAULT '0',
	`dimension2_id` int(11) NOT NULL DEFAULT '0',
	`person_type_id` int(11) NOT NULL DEFAULT '0',
	`person_id` tinyblob,
	`reconciled` date DEFAULT NULL,
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `bank_act` (`bank_act`,`ref`),
	KEY `type` (`type`,`trans_no`),
	KEY `bank_act_2` (`bank_act`,`reconciled`),
	KEY `bank_act_3` (`bank_act`,`trans_date`)
) ENGINE=InnoDB AUTO_INCREMENT=6 ;

-- Data of table `0_bank_trans` --

INSERT INTO `0_bank_trans` VALUES
('1', '12', '1', '2', '001/2025', '2025-05-10', '6240', '0', '0', '2', '1', NULL, '{}'),
('2', '12', '2', '2', '002/2025', '2025-05-07', '300', '0', '0', '2', '1', NULL, '{}'),
('3', '12', '3', '2', '003/2025', '2025-05-07', '0', '0', '0', '2', '1', NULL, '{}'),
('4', '1', '1', '1', '001/2025', '2025-05-07', '-5', '0', '0', '0', 'Goods received', NULL, '{}'),
('5', '12', '4', '2', '001/2026', '2026-01-21', '1250', '0', '0', '2', '1', NULL, '{}');

-- Structure of table `0_bom` --

DROP TABLE IF EXISTS `0_bom`;

CREATE TABLE `0_bom` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`parent` char(20) NOT NULL DEFAULT '',
	`component` char(20) NOT NULL DEFAULT '',
	`workcentre_added` int(11) NOT NULL DEFAULT '0',
	`loc_code` char(5) NOT NULL DEFAULT '',
	`quantity` double NOT NULL DEFAULT '1',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`parent`,`component`,`workcentre_added`,`loc_code`),
	KEY `component` (`component`),
	KEY `id` (`id`),
	KEY `loc_code` (`loc_code`),
	KEY `parent` (`parent`,`loc_code`),
	KEY `workcentre_added` (`workcentre_added`)
) ENGINE=InnoDB AUTO_INCREMENT=5 ;

-- Data of table `0_bom` --

INSERT INTO `0_bom` VALUES
('1', '201', '101', '1', 'DEF', '1', '{}'),
('2', '201', '102', '1', 'DEF', '1', '{}'),
('3', '201', '103', '1', 'DEF', '1', '{}'),
('4', '201', '301', '1', 'DEF', '1', '{}');

-- Structure of table `0_budget_trans` --

DROP TABLE IF EXISTS `0_budget_trans`;

CREATE TABLE `0_budget_trans` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`tran_date` date NOT NULL DEFAULT '0000-00-00',
	`account` varchar(15) NOT NULL DEFAULT '',
	`memo_` tinytext NOT NULL,
	`amount` double NOT NULL DEFAULT '0',
	`dimension_id` int(11) DEFAULT '0',
	`dimension2_id` int(11) DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `Account` (`account`,`tran_date`,`dimension_id`,`dimension2_id`)
) ENGINE=InnoDB ;

-- Data of table `0_budget_trans` --

-- Structure of table `0_chart_class` --

DROP TABLE IF EXISTS `0_chart_class`;

CREATE TABLE `0_chart_class` (
	`cid` varchar(3) NOT NULL,
	`class_name` varchar(60) NOT NULL DEFAULT '',
	`ctype` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`cid`)
) ENGINE=InnoDB ;

-- Data of table `0_chart_class` --

INSERT INTO `0_chart_class` VALUES
('1', 'Assets', '1', '0', '{}'),
('2', 'Liabilities', '2', '0', '{}'),
('3', 'Income', '4', '0', '{}'),
('4', 'Costs', '6', '0', '{}');

-- Structure of table `0_chart_master` --

DROP TABLE IF EXISTS `0_chart_master`;

CREATE TABLE `0_chart_master` (
	`account_code` varchar(15) NOT NULL DEFAULT '',
	`account_code2` varchar(15) NOT NULL DEFAULT '',
	`account_name` varchar(60) NOT NULL DEFAULT '',
	`account_type` varchar(10) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`account_code`),
	KEY `account_name` (`account_name`),
	KEY `accounts_by_type` (`account_type`,`account_code`)
) ENGINE=InnoDB ;

-- Data of table `0_chart_master` --

INSERT INTO `0_chart_master` VALUES
('1060', '', 'Checking Account', '1', '0', '{}'),
('1065', '', 'Petty Cash', '1', '0', '{}'),
('1200', '', 'Accounts Receivables', '1', '0', '{}'),
('1205', '', 'Allowance for doubtful accounts', '1', '0', '{}'),
('1510', '', 'Inventory', '2', '0', '{}'),
('1520', '', 'Stocks of Raw Materials', '2', '0', '{}'),
('1530', '', 'Stocks of Work In Progress', '2', '0', '{}'),
('1540', '', 'Stocks of Finished Goods', '2', '0', '{}'),
('1550', '', 'Goods Received Clearing account', '2', '0', '{}'),
('1820', '', 'Office Furniture &amp; Equipment', '3', '0', '{}'),
('1825', '', 'Accum. Amort. -Furn. &amp; Equip.', '3', '0', '{}'),
('1840', '', 'Vehicle', '3', '0', '{}'),
('1845', '', 'Accum. Amort. -Vehicle', '3', '0', '{}'),
('2100', '', 'Accounts Payable', '4', '0', '{}'),
('2105', '', 'Deferred Income', '4', '0', '{}'),
('2110', '', 'Accrued Income Tax - Federal', '4', '0', '{}'),
('2120', '', 'Accrued Income Tax - State', '4', '0', '{}'),
('2130', '', 'Accrued Franchise Tax', '4', '0', '{}'),
('2140', '', 'Accrued Real &amp; Personal Prop Tax', '4', '0', '{}'),
('2150', '', 'Sales Tax', '4', '0', '{}'),
('2160', '', 'Accrued Use Tax Payable', '4', '0', '{}'),
('2210', '', 'Accrued Wages', '4', '0', '{}'),
('2220', '', 'Accrued Comp Time', '4', '0', '{}'),
('2230', '', 'Accrued Holiday Pay', '4', '0', '{}'),
('2240', '', 'Accrued Vacation Pay', '4', '0', '{}'),
('2310', '', 'Accr. Benefits - 401K', '4', '0', '{}'),
('2320', '', 'Accr. Benefits - Stock Purchase', '4', '0', '{}'),
('2330', '', 'Accr. Benefits - Med, Den', '4', '0', '{}'),
('2340', '', 'Accr. Benefits - Payroll Taxes', '4', '0', '{}'),
('2350', '', 'Accr. Benefits - Credit Union', '4', '0', '{}'),
('2360', '', 'Accr. Benefits - Savings Bond', '4', '0', '{}'),
('2370', '', 'Accr. Benefits - Garnish', '4', '0', '{}'),
('2380', '', 'Accr. Benefits - Charity Cont.', '4', '0', '{}'),
('2620', '', 'Bank Loans', '5', '0', '{}'),
('2680', '', 'Loans from Shareholders', '5', '0', '{}'),
('3350', '', 'Common Shares', '6', '0', '{}'),
('3590', '', 'Retained Earnings - prior years', '7', '0', '{}'),
('4010', '', 'Sales', '8', '0', '{}'),
('4430', '', 'Shipping &amp; Handling', '9', '0', '{}'),
('4440', '', 'Interest', '9', '0', '{}'),
('4450', '', 'Foreign Exchange Gain', '9', '0', '{}'),
('4500', '', 'Prompt Payment Discounts', '9', '0', '{}'),
('4510', '', 'Discounts Given', '9', '0', '{}'),
('5010', '', 'Cost of Goods Sold - Retail', '10', '0', '{}'),
('5020', '', 'Material Usage Varaiance', '10', '0', '{}'),
('5030', '', 'Consumable Materials', '10', '0', '{}'),
('5040', '', 'Purchase price Variance', '10', '0', '{}'),
('5050', '', 'Purchases of materials', '10', '0', '{}'),
('5060', '', 'Discounts Received', '10', '0', '{}'),
('5100', '', 'Freight', '10', '0', '{}'),
('5410', '', 'Wages &amp; Salaries', '11', '0', '{}'),
('5420', '', 'Wages - Overtime', '11', '0', '{}'),
('5430', '', 'Benefits - Comp Time', '11', '0', '{}'),
('5440', '', 'Benefits - Payroll Taxes', '11', '0', '{}'),
('5450', '', 'Benefits - Workers Comp', '11', '0', '{}'),
('5460', '', 'Benefits - Pension', '11', '0', '{}'),
('5470', '', 'Benefits - General Benefits', '11', '0', '{}'),
('5510', '', 'Inc Tax Exp - Federal', '11', '0', '{}'),
('5520', '', 'Inc Tax Exp - State', '11', '0', '{}'),
('5530', '', 'Taxes - Real Estate', '11', '0', '{}'),
('5540', '', 'Taxes - Personal Property', '11', '0', '{}'),
('5550', '', 'Taxes - Franchise', '11', '0', '{}'),
('5560', '', 'Taxes - Foreign Withholding', '11', '0', '{}'),
('5610', '', 'Accounting &amp; Legal', '12', '0', '{}'),
('5615', '', 'Advertising &amp; Promotions', '12', '0', '{}'),
('5620', '', 'Bad Debts', '12', '0', '{}'),
('5660', '', 'Amortization Expense', '12', '0', '{}'),
('5685', '', 'Insurance', '12', '0', '{}'),
('5690', '', 'Interest &amp; Bank Charges', '12', '0', '{}'),
('5700', '', 'Office Supplies', '12', '0', '{}'),
('5760', '', 'Rent', '12', '0', '{}'),
('5765', '', 'Repair &amp; Maintenance', '12', '0', '{}'),
('5780', '', 'Telephone', '12', '0', '{}'),
('5785', '', 'Travel &amp; Entertainment', '12', '0', '{}'),
('5790', '', 'Utilities', '12', '0', '{}'),
('5795', '', 'Registrations', '12', '0', '{}'),
('5800', '', 'Licenses', '12', '0', '{}'),
('5810', '', 'Foreign Exchange Loss', '12', '0', '{}'),
('9990', '', 'Year Profit/Loss', '12', '0', '{}');

-- Structure of table `0_chart_types` --

DROP TABLE IF EXISTS `0_chart_types`;

CREATE TABLE `0_chart_types` (
	`id` varchar(10) NOT NULL,
	`name` varchar(60) NOT NULL DEFAULT '',
	`class_id` varchar(3) NOT NULL DEFAULT '',
	`parent` varchar(10) NOT NULL DEFAULT '-1',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `name` (`name`),
	KEY `class_id` (`class_id`)
) ENGINE=InnoDB ;

-- Data of table `0_chart_types` --

INSERT INTO `0_chart_types` VALUES
('1', 'Current Assets', '1', '-1', '0', '{}'),
('2', 'Inventory Assets', '1', '-1', '0', '{}'),
('3', 'Capital Assets', '1', '-1', '0', '{}'),
('4', 'Current Liabilities', '2', '-1', '0', '{}'),
('5', 'Long Term Liabilities', '2', '-1', '0', '{}'),
('6', 'Share Capital', '2', '-1', '0', '{}'),
('7', 'Retained Earnings', '2', '-1', '0', '{}'),
('8', 'Sales Revenue', '3', '-1', '0', '{}'),
('9', 'Other Revenue', '3', '-1', '0', '{}'),
('10', 'Cost of Goods Sold', '4', '-1', '0', '{}'),
('11', 'Payroll Expenses', '4', '-1', '0', '{}'),
('12', 'General &amp; Administrative expenses', '4', '-1', '0', '{}');

-- Structure of table `0_comments` --

DROP TABLE IF EXISTS `0_comments`;

CREATE TABLE `0_comments` (
	`type` int(11) NOT NULL DEFAULT '0',
	`id` int(11) NOT NULL DEFAULT '0',
	`date_` date DEFAULT '0000-00-00',
	`memo_` tinytext,
	UNIQUE KEY `type_id_date` (`type`,`id`,`date_`),
	KEY `type_and_id` (`type`,`id`)
) ENGINE=InnoDB ;

-- Data of table `0_comments` --

INSERT INTO `0_comments` VALUES
('12', '1', '2025-05-10', 'Cash invoice 1'),
('12', '2', '2025-05-07', 'Cash invoice 2'),
('13', '4', '2025-05-07', 'Recurrent Invoice covers period 04/01/2025 - 04/07/2025.'),
('10', '4', '2025-05-07', 'Recurrent Invoice covers period 04/01/2025 - 04/07/2025.'),
('12', '3', '2025-05-07', 'Cash invoice 4'),
('12', '4', '2026-01-21', 'Default #5'),
('0', '1', '2025-12-31', 'Closing Year');

-- Structure of table `0_credit_status` --

DROP TABLE IF EXISTS `0_credit_status`;

CREATE TABLE `0_credit_status` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`reason_description` char(100) NOT NULL DEFAULT '',
	`dissallow_invoices` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `reason_description` (`reason_description`)
) ENGINE=InnoDB AUTO_INCREMENT=5 ;

-- Data of table `0_credit_status` --

INSERT INTO `0_credit_status` VALUES
('1', 'Good History', '0', '0'),
('3', 'No more work until payment received', '1', '0'),
('4', 'In liquidation', '1', '0');

-- Structure of table `0_crm_categories` --

DROP TABLE IF EXISTS `0_crm_categories`;

CREATE TABLE `0_crm_categories` (
	`id` int(11) NOT NULL AUTO_INCREMENT COMMENT 'pure technical key',
	`type` varchar(20) NOT NULL COMMENT 'contact type e.g. customer',
	`action` varchar(20) NOT NULL COMMENT 'detailed usage e.g. department',
	`name` varchar(30) NOT NULL COMMENT 'for category selector',
	`description` tinytext NOT NULL COMMENT 'usage description',
	`system` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'nonzero for core system usage',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `type` (`type`,`action`),
	UNIQUE KEY `type_2` (`type`,`name`)
) ENGINE=InnoDB AUTO_INCREMENT=13 ;

-- Data of table `0_crm_categories` --

INSERT INTO `0_crm_categories` VALUES
('1', 'cust_branch', 'general', 'General', 'General contact data for customer branch (overrides company setting)', '1', '0'),
('2', 'cust_branch', 'invoice', 'Invoices', 'Invoice posting (overrides company setting)', '1', '0'),
('3', 'cust_branch', 'order', 'Orders', 'Order confirmation (overrides company setting)', '1', '0'),
('4', 'cust_branch', 'delivery', 'Deliveries', 'Delivery coordination (overrides company setting)', '1', '0'),
('5', 'customer', 'general', 'General', 'General contact data for customer', '1', '0'),
('6', 'customer', 'order', 'Orders', 'Order confirmation', '1', '0'),
('7', 'customer', 'delivery', 'Deliveries', 'Delivery coordination', '1', '0'),
('8', 'customer', 'invoice', 'Invoices', 'Invoice posting', '1', '0'),
('9', 'supplier', 'general', 'General', 'General contact data for supplier', '1', '0'),
('10', 'supplier', 'order', 'Orders', 'Order confirmation', '1', '0'),
('11', 'supplier', 'delivery', 'Deliveries', 'Delivery coordination', '1', '0'),
('12', 'supplier', 'invoice', 'Invoices', 'Invoice posting', '1', '0');

-- Structure of table `0_crm_contacts` --

DROP TABLE IF EXISTS `0_crm_contacts`;

CREATE TABLE `0_crm_contacts` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`person_id` int(11) NOT NULL DEFAULT '0' COMMENT 'foreign key to crm_persons',
	`type` varchar(20) NOT NULL COMMENT 'foreign key to crm_categories',
	`action` varchar(20) NOT NULL COMMENT 'foreign key to crm_categories',
	`entity_id` varchar(11) DEFAULT NULL COMMENT 'entity id in related class table',
	PRIMARY KEY (`id`),
	KEY `type` (`type`,`action`)
) ENGINE=InnoDB AUTO_INCREMENT=11 ;

-- Data of table `0_crm_contacts` --

INSERT INTO `0_crm_contacts` VALUES
('4', '2', 'supplier', 'general', '2'),
('5', '3', 'cust_branch', 'general', '1'),
('7', '3', 'customer', 'general', '1'),
('8', '1', 'supplier', 'general', '1'),
('9', '4', 'cust_branch', 'general', '2'),
('10', '4', 'customer', 'general', '2');

-- Structure of table `0_crm_persons` --

DROP TABLE IF EXISTS `0_crm_persons`;

CREATE TABLE `0_crm_persons` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`ref` varchar(30) NOT NULL,
	`name` varchar(60) NOT NULL,
	`name2` varchar(60) DEFAULT NULL,
	`address` tinytext,
	`phone` varchar(30) DEFAULT NULL,
	`phone2` varchar(30) DEFAULT NULL,
	`fax` varchar(30) DEFAULT NULL,
	`email` varchar(100) DEFAULT NULL,
	`lang` char(5) DEFAULT NULL,
	`notes` tinytext NOT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	KEY `ref` (`ref`)
) ENGINE=InnoDB AUTO_INCREMENT=5 ;

-- Data of table `0_crm_persons` --

INSERT INTO `0_crm_persons` VALUES
('1', 'Dino Saurius', 'John Doe', NULL, 'N/A', NULL, NULL, NULL, NULL, NULL, '', '0'),
('2', 'Beefeater', 'Joe Oversea', NULL, 'N/A', NULL, NULL, NULL, NULL, NULL, '', '0'),
('3', 'Donald Easter', 'Donald Easter LLC', NULL, 'N/A', NULL, NULL, NULL, NULL, NULL, '', '0'),
('4', 'MoneyMaker', 'MoneyMaker Ltd.', NULL, NULL, NULL, NULL, NULL, NULL, NULL, '', '0');

-- Structure of table `0_currencies` --

DROP TABLE IF EXISTS `0_currencies`;

CREATE TABLE `0_currencies` (
	`currency` varchar(60) NOT NULL DEFAULT '',
	`curr_abrev` char(3) NOT NULL DEFAULT '',
	`curr_symbol` varchar(10) NOT NULL DEFAULT '',
	`country` varchar(100) NOT NULL DEFAULT '',
	`hundreds_name` varchar(15) NOT NULL DEFAULT '',
	`auto_update` tinyint(1) NOT NULL DEFAULT '1',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`curr_abrev`)
) ENGINE=InnoDB ;

-- Data of table `0_currencies` --

INSERT INTO `0_currencies` VALUES
('CA Dollars', 'CAD', '$', 'Canada', 'Cents', '1', '0'),
('Euro', 'EUR', '€', 'Europe', 'Cents', '1', '0'),
('Pounds', 'GBP', '£', 'England', 'Pence', '1', '0'),
('US Dollars', 'USD', '$', 'United States', 'Cents', '1', '0');

-- Structure of table `0_cust_allocations` --

DROP TABLE IF EXISTS `0_cust_allocations`;

CREATE TABLE `0_cust_allocations` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`person_id` int(11) DEFAULT NULL,
	`amt` double unsigned DEFAULT NULL,
	`date_alloc` date NOT NULL DEFAULT '0000-00-00',
	`trans_no_from` int(11) DEFAULT NULL,
	`trans_type_from` int(11) DEFAULT NULL,
	`trans_no_to` int(11) DEFAULT NULL,
	`trans_type_to` int(11) DEFAULT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `trans_type_from` (`person_id`,`trans_type_from`,`trans_no_from`,`trans_type_to`,`trans_no_to`),
	KEY `From` (`trans_type_from`,`trans_no_from`),
	KEY `To` (`trans_type_to`,`trans_no_to`)
) ENGINE=InnoDB AUTO_INCREMENT=5 ;

-- Data of table `0_cust_allocations` --

INSERT INTO `0_cust_allocations` VALUES
('1', '1', '6240', '2025-05-10', '1', '12', '1', '10'),
('2', '1', '300', '2025-05-07', '2', '12', '2', '10'),
('3', '1', '0', '2025-05-07', '3', '12', '4', '10'),
('4', '1', '1250', '2026-01-21', '4', '12', '5', '10');

-- Structure of table `0_cust_branch` --

DROP TABLE IF EXISTS `0_cust_branch`;

CREATE TABLE `0_cust_branch` (
	`branch_code` int(11) NOT NULL AUTO_INCREMENT,
	`debtor_no` int(11) NOT NULL DEFAULT '0',
	`br_name` varchar(60) NOT NULL DEFAULT '',
	`branch_ref` varchar(30) NOT NULL DEFAULT '',
	`br_address` tinytext NOT NULL,
	`area` int(11) DEFAULT NULL,
	`salesman` int(11) NOT NULL DEFAULT '0',
	`default_location` varchar(5) NOT NULL DEFAULT '',
	`tax_group_id` int(11) DEFAULT NULL,
	`sales_account` varchar(15) NOT NULL DEFAULT '',
	`sales_discount_account` varchar(15) NOT NULL DEFAULT '',
	`receivables_account` varchar(15) NOT NULL DEFAULT '',
	`payment_discount_account` varchar(15) NOT NULL DEFAULT '',
	`default_ship_via` int(11) NOT NULL DEFAULT '1',
	`br_post_address` tinytext NOT NULL,
	`group_no` int(11) NOT NULL DEFAULT '0',
	`notes` tinytext NOT NULL,
	`bank_account` varchar(60) DEFAULT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`branch_code`,`debtor_no`),
	KEY `branch_ref` (`branch_ref`),
	KEY `group_no` (`group_no`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_cust_branch` --

INSERT INTO `0_cust_branch` VALUES
('1', '1', 'Donald Easter LLC', 'Donald Easter', 'N/A', '1', '1', 'DEF', '1', '', '4510', '1200', '4500', '1', 'N/A', '0', '', NULL, '0'),
('2', '2', 'MoneyMaker Ltd.', 'MoneyMaker', '', '1', '1', 'DEF', '2', '', '4510', '1200', '4500', '1', '', '0', '', NULL, '0');

-- Structure of table `0_debtor_trans` --

DROP TABLE IF EXISTS `0_debtor_trans`;

CREATE TABLE `0_debtor_trans` (
	`trans_no` int(11) unsigned NOT NULL DEFAULT '0',
	`type` smallint(6) unsigned NOT NULL DEFAULT '0',
	`version` tinyint(1) unsigned NOT NULL DEFAULT '0',
	`debtor_no` int(11) unsigned NOT NULL,
	`branch_code` int(11) NOT NULL DEFAULT '-1',
	`tran_date` date NOT NULL DEFAULT '0000-00-00',
	`due_date` date NOT NULL DEFAULT '0000-00-00',
	`reference` varchar(60) NOT NULL DEFAULT '',
	`tpe` int(11) NOT NULL DEFAULT '0',
	`order_` int(11) NOT NULL DEFAULT '0',
	`ov_amount` double NOT NULL DEFAULT '0',
	`ov_gst` double NOT NULL DEFAULT '0',
	`ov_freight` double NOT NULL DEFAULT '0',
	`ov_freight_tax` double NOT NULL DEFAULT '0',
	`ov_discount` double NOT NULL DEFAULT '0',
	`alloc` double NOT NULL DEFAULT '0',
	`prep_amount` double NOT NULL DEFAULT '0',
	`rate` double NOT NULL DEFAULT '1',
	`ship_via` int(11) DEFAULT NULL,
	`dimension_id` int(11) NOT NULL DEFAULT '0',
	`dimension2_id` int(11) NOT NULL DEFAULT '0',
	`payment_terms` int(11) DEFAULT NULL,
	`tax_included` tinyint(1) unsigned NOT NULL DEFAULT '0',
	`pos_id` smallint(6) unsigned NOT NULL,
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`type`,`trans_no`,`debtor_no`),
	KEY `debtor_no` (`debtor_no`,`branch_code`),
	KEY `tran_date` (`tran_date`),
	KEY `order_` (`order_`)
) ENGINE=InnoDB ;

-- Data of table `0_debtor_trans` --

INSERT INTO `0_debtor_trans` VALUES
('1', '10', '0', '1', '1', '2025-05-10', '2025-05-05', '001/2025', '1', '1', '6240', '0', '0', '0', '0', '6240', '0', '1', '1', '0', '0', '4', '1', '1', '{}'),
('2', '10', '0', '1', '1', '2025-05-07', '2025-05-07', '002/2025', '1', '2', '300', '0', '0', '0', '0', '300', '0', '1', '1', '0', '0', '4', '1', '1', '{}'),
('3', '10', '0', '2', '2', '2025-05-07', '2025-06-17', '003/2025', '1', '5', '267.14', '0', '0', '0', '0', '0', '0', '1.123', '1', '1', '0', '1', '1', '1', '{}'),
('4', '10', '0', '1', '1', '2025-05-07', '2025-05-07', '004/2025', '1', '7', '0', '0', '0', '0', '0', '0', '0', '1', '1', '0', '0', '4', '1', '1', '{}'),
('5', '10', '0', '1', '1', '2026-01-21', '2026-01-21', '001/2026', '1', '8', '1250', '0', '0', '0', '0', '1250', '0', '1', '1', '0', '0', '4', '1', '1', '{}'),
('1', '12', '0', '1', '1', '2025-05-10', '0000-00-00', '001/2025', '0', '0', '6240', '0', '0', '0', '0', '6240', '0', '1', '0', '0', '0', NULL, '0', '1', '{}'),
('2', '12', '0', '1', '1', '2025-05-07', '0000-00-00', '002/2025', '0', '0', '300', '0', '0', '0', '0', '300', '0', '1', '0', '0', '0', NULL, '0', '1', '{}'),
('3', '12', '0', '1', '1', '2025-05-07', '0000-00-00', '003/2025', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', NULL, '0', '1', '{}'),
('4', '12', '0', '1', '1', '2026-01-21', '0000-00-00', '001/2026', '0', '0', '1250', '0', '0', '0', '0', '1250', '0', '1', '0', '0', '0', NULL, '0', '1', '{}'),
('1', '13', '1', '1', '1', '2025-05-10', '2025-05-05', 'auto', '1', '1', '6240', '0', '0', '0', '0', '0', '0', '1', '1', '0', '0', '4', '1', '1', '{}'),
('2', '13', '1', '1', '1', '2025-05-07', '2025-05-07', 'auto', '1', '2', '300', '0', '0', '0', '0', '0', '0', '1', '1', '0', '0', '4', '1', '1', '{}'),
('3', '13', '1', '2', '2', '2025-05-07', '2025-06-17', 'auto', '1', '5', '267.14', '0', '0', '0', '0', '0', '0', '1.123', '1', '1', '0', '1', '1', '1', '{}'),
('4', '13', '1', '1', '1', '2025-05-07', '2025-05-07', 'auto', '1', '7', '0', '0', '0', '0', '0', '0', '0', '1', '1', '0', '0', '4', '1', '1', '{}'),
('5', '13', '1', '1', '1', '2026-01-21', '2026-01-21', 'auto', '1', '8', '1250', '0', '0', '0', '0', '0', '0', '1', '1', '0', '0', '4', '1', '1', '{}');

-- Structure of table `0_debtor_trans_details` --

DROP TABLE IF EXISTS `0_debtor_trans_details`;

CREATE TABLE `0_debtor_trans_details` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`debtor_trans_no` int(11) DEFAULT NULL,
	`debtor_trans_type` int(11) DEFAULT NULL,
	`stock_id` varchar(20) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '',
	`description` tinytext,
	`unit_price` double NOT NULL DEFAULT '0',
	`unit_tax` double NOT NULL DEFAULT '0',
	`quantity` double NOT NULL DEFAULT '0',
	`discount_percent` double NOT NULL DEFAULT '0',
	`standard_cost` double NOT NULL DEFAULT '0',
	`qty_done` double NOT NULL DEFAULT '0',
	`src_id` int(11) NOT NULL,
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `Transaction` (`debtor_trans_type`,`debtor_trans_no`),
	KEY `src_id` (`src_id`)
) ENGINE=InnoDB AUTO_INCREMENT=15 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- Data of table `0_debtor_trans_details` --

INSERT INTO `0_debtor_trans_details` VALUES
('1', '1', '13', '101', 'Vsmart Aris 6GB-64GB', '300', '14.29', '20', '0', '200', '20', '1', '{}'),
('2', '1', '13', '301', 'Support', '80', '3.81', '3', '0', '0', '3', '2', '{}'),
('3', '1', '10', '101', 'Vsmart Aris 6GB-64GB', '300', '14.2855', '20', '0', '200', '0', '1', '{}'),
('4', '1', '10', '301', 'Support', '80', '3.81', '3', '0', '0', '0', '2', '{}'),
('5', '2', '13', '101', 'Vsmart Aris 6GB-64GB', '300', '14.29', '1', '0', '200', '1', '3', '{}'),
('6', '2', '10', '101', 'Vsmart Aris 6GB-64GB', '300', '14.29', '1', '0', '200', '0', '5', '{}'),
('7', '3', '13', '102', 'Vsmart Live 4 (6GB/64GB)', '222.62', '0', '1', '0', '150', '1', '7', '{}'),
('8', '3', '13', '103', 'Vsmart Live 4 Cover Case', '44.52', '0', '1', '0', '10', '1', '8', '{}'),
('9', '3', '10', '102', 'Vsmart Live 4 (6GB/64GB)', '222.62', '0', '1', '0', '150', '0', '7', '{}'),
('10', '3', '10', '103', 'Vsmart Live 4 Cover Case', '44.52', '0', '1', '0', '10', '0', '8', '{}'),
('11', '4', '13', '202', 'Maintenance', '0', '0', '5', '0', '0', '5', '10', '{}'),
('12', '4', '10', '202', 'Maintenance', '0', '0', '5', '0', '0', '0', '11', '{}'),
('13', '5', '13', '102', 'Vsmart Live 4 (6GB/64GB)', '250', '11.904', '5', '0', '150', '5', '11', '{}'),
('14', '5', '10', '102', 'Vsmart Live 4 (6GB/64GB)', '250', '11.904', '5', '0', '150', '0', '13', '{}');

-- Structure of table `0_debtors_master` --

DROP TABLE IF EXISTS `0_debtors_master`;

CREATE TABLE `0_debtors_master` (
	`debtor_no` int(11) NOT NULL AUTO_INCREMENT,
	`name` varchar(100) NOT NULL DEFAULT '',
	`debtor_ref` varchar(30) NOT NULL,
	`address` tinytext,
	`tax_id` varchar(55) NOT NULL DEFAULT '',
	`curr_code` char(3) NOT NULL DEFAULT '',
	`sales_type` int(11) NOT NULL DEFAULT '1',
	`dimension_id` int(11) NOT NULL DEFAULT '0',
	`dimension2_id` int(11) NOT NULL DEFAULT '0',
	`credit_status` int(11) NOT NULL DEFAULT '0',
	`payment_terms` int(11) DEFAULT NULL,
	`discount` double NOT NULL DEFAULT '0',
	`pymt_discount` double NOT NULL DEFAULT '0',
	`credit_limit` float NOT NULL DEFAULT '1000',
	`notes` tinytext NOT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`credit_insurance_limit` double NOT NULL DEFAULT '0',
	`credit_review_date` date DEFAULT NULL,
	`credit_risk_score` enum('low','medium','high','critical') NOT NULL DEFAULT 'medium',
	`payment_behavior_score` double NOT NULL DEFAULT '0',
	`customer_tier` enum('standard','silver','gold','platinum','vip') NOT NULL DEFAULT 'standard',
	`preferred_communication` enum('email','phone','whatsapp','portal') NOT NULL DEFAULT 'email',
	`industry` varchar(50) NOT NULL DEFAULT '',
	`company_size` enum('small','medium','large','enterprise') NOT NULL DEFAULT 'medium',
	PRIMARY KEY (`debtor_no`),
	UNIQUE KEY `debtor_ref` (`debtor_ref`),
	KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_debtors_master` --

INSERT INTO `0_debtors_master` VALUES
('1', 'Donald Easter LLC', 'Donald Easter', 'N/A', '123456789', 'USD', '1', '0', '0', '1', '4', '0', '0', '1000', '', '0', '{}', '0', NULL, 'medium', '0', 'standard', 'email', '', 'medium'),
('2', 'MoneyMaker Ltd.', 'MoneyMaker', 'N/A', '54354333', 'EUR', '1', '1', '0', '1', '1', '0', '0', '1000', '', '0', '{}', '0', NULL, 'medium', '0', 'standard', 'email', '', 'medium');

-- Structure of table `0_departments` --

DROP TABLE IF EXISTS `0_departments`;
CREATE TABLE IF NOT EXISTS `0_departments` (
	`department_id`              int(11)      NOT NULL AUTO_INCREMENT,
	`department_code`            varchar(20)  DEFAULT NULL,
	`department_name`            tinytext     NOT NULL,
	`parent_department_id`       int(11)      DEFAULT NULL,
	`manager_employee_id`        varchar(20)  DEFAULT NULL,
	`cost_center_id`             int(11)      NOT NULL DEFAULT '0',
	`payroll_expense_account`    varchar(15)  DEFAULT NULL,
	`payroll_liability_account`  varchar(15)  DEFAULT NULL,
	`description`                text,
	`inactive`                   tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`department_id`),
	KEY `parent_department_id` (`parent_department_id`)
) ENGINE=InnoDB;

-- Data of table `0_departments` --

-- Structure of table `0_dimensions` --

DROP TABLE IF EXISTS `0_dimensions`;

CREATE TABLE `0_dimensions` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`reference` varchar(60) NOT NULL DEFAULT '',
	`name` varchar(60) NOT NULL DEFAULT '',
	`type_` tinyint(1) NOT NULL DEFAULT '1',
	`closed` tinyint(1) NOT NULL DEFAULT '0',
	`date_` date NOT NULL DEFAULT '0000-00-00',
	`due_date` date NOT NULL DEFAULT '0000-00-00',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `reference` (`reference`),
	KEY `date_` (`date_`),
	KEY `due_date` (`due_date`),
	KEY `type_` (`type_`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_dimensions` --

INSERT INTO `0_dimensions` VALUES
('1', '001/2025', 'Cost Centre', '1', '0', '2025-05-05', '2025-05-25', '{}');

-- Structure of table `0_employees` --

DROP TABLE IF EXISTS `0_employees`;
CREATE TABLE IF NOT EXISTS `0_employees` (
	`employee_number`     int(11)         NOT NULL AUTO_INCREMENT,
	`employee_id`         varchar(20)     NOT NULL,
	`first_name`          varchar(100)    NOT NULL,
	`last_name`           varchar(100)    NOT NULL,
	`middle_name`         varchar(100)    DEFAULT NULL,
	`gender`              tinyint(1)      NOT NULL DEFAULT '0',
	`birth_date`          date            DEFAULT NULL,
	`nationality`         varchar(60)     DEFAULT NULL,
	`marital_status`      tinyint(1)      NOT NULL DEFAULT '0',
	`dependents_no`       int(10)         NOT NULL DEFAULT '0',
	`address`             tinytext,
	`city`                varchar(60)     DEFAULT NULL,
	`state`               varchar(60)     DEFAULT NULL,
	`country`             varchar(60)     DEFAULT NULL,
	`phone`               varchar(30)     DEFAULT NULL,
	`mobile`              varchar(30)     DEFAULT NULL,
	`email`               varchar(100)    DEFAULT NULL,
	`personal_email`      varchar(100)    DEFAULT NULL,
	`national_id`         varchar(100)    DEFAULT NULL,
	`passport`            varchar(100)    DEFAULT NULL,
	`passport_expiry`     date            DEFAULT NULL,
	`tax_number`          varchar(100)    DEFAULT NULL,
	`social_security_no`  varchar(100)    DEFAULT NULL,
	`bank_name`           varchar(100)    DEFAULT NULL,
	`bank_branch`         varchar(100)    DEFAULT NULL,
	`bank_account`        varchar(100)    DEFAULT NULL,
	`bank_routing`        varchar(60)     DEFAULT NULL,
	`payment_method`      tinyint(1)      NOT NULL DEFAULT '0' COMMENT '0=bank_transfer, 1=cash, 2=check',
	`hire_date`           date            DEFAULT NULL,
	`confirmation_date`   date            DEFAULT NULL,
	`probation_end_date`  date            DEFAULT NULL,
	`released_date`       date            DEFAULT NULL,
	`separation_reason`   tinyint(1)      DEFAULT NULL COMMENT '0=resign, 1=terminate, 2=retire, 3=contract_end',
	`employment_type`     tinyint(1)      NOT NULL DEFAULT '0' COMMENT '0=permanent, 1=contract, 2=probation, 3=intern, 4=parttime',
	`department_id`       int(11)         NOT NULL DEFAULT '0',
	`position_id`         int(11)         NOT NULL DEFAULT '0',
	`grade_id`            int(11)         NOT NULL DEFAULT '0',
	`shift_id`            int(11)         DEFAULT NULL,
	`reporting_to`        varchar(20)     DEFAULT NULL,
	`personal_salary`     tinyint(1)      NOT NULL DEFAULT '0' COMMENT '1=use individual salary from employee_salary table',
	`cost_center_id`      int(11)         DEFAULT NULL,
	`dimension2_id`       int(11)         NOT NULL DEFAULT '0',
	`emergency_name`      varchar(100)    DEFAULT NULL,
	`emergency_relation`  varchar(60)     DEFAULT NULL,
	`emergency_phone`     varchar(30)     DEFAULT NULL,
	`login_id`            varchar(60)     NOT NULL DEFAULT '',
	`notes`               text,
	`inactive`            tinyint(1)      NOT NULL DEFAULT '0',
	`custom_data`         json            DEFAULT NULL,
	PRIMARY KEY (`employee_number`),
	UNIQUE KEY `employee_id` (`employee_id`),
	KEY `department_id` (`department_id`),
	KEY `position_id` (`position_id`),
	KEY `grade_id` (`grade_id`),
	KEY `reporting_to` (`reporting_to`)
) ENGINE=InnoDB;

-- Data of table `0_employees` --

-- Structure of table `0_exchange_rates` --

DROP TABLE IF EXISTS `0_exchange_rates`;

CREATE TABLE `0_exchange_rates` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`curr_code` char(3) NOT NULL DEFAULT '',
	`rate_buy` double NOT NULL DEFAULT '0',
	`rate_sell` double NOT NULL DEFAULT '0',
	`date_` date NOT NULL DEFAULT '0000-00-00',
	PRIMARY KEY (`id`),
	UNIQUE KEY `curr_code` (`curr_code`,`date_`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_exchange_rates` --

INSERT INTO `0_exchange_rates` VALUES
('1', 'EUR', '1.123', '1.123', '2025-05-07');

-- Structure of table `0_fiscal_year` --

DROP TABLE IF EXISTS `0_fiscal_year`;

CREATE TABLE `0_fiscal_year` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`begin` date DEFAULT '0000-00-00',
	`end` date DEFAULT '0000-00-00',
	`closed` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `begin` (`begin`),
	UNIQUE KEY `end` (`end`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_fiscal_year` --

INSERT INTO `0_fiscal_year` VALUES
('1', '2025-01-01', '2025-12-31', '1'),
('2', '2026-01-01', '2026-12-31', '0');

-- Structure of table `0_gl_trans` --

DROP TABLE IF EXISTS `0_gl_trans`;

CREATE TABLE `0_gl_trans` (
	`counter` int(11) NOT NULL AUTO_INCREMENT,
	`type` smallint(6) NOT NULL DEFAULT '0',
	`type_no` int(11) NOT NULL DEFAULT '0',
	`tran_date` date NOT NULL DEFAULT '0000-00-00',
	`account` varchar(15) NOT NULL DEFAULT '',
	`memo_` tinytext NOT NULL,
	`amount` double NOT NULL DEFAULT '0',
	`dimension_id` int(11) NOT NULL DEFAULT '0',
	`dimension2_id` int(11) NOT NULL DEFAULT '0',
	`person_type_id` int(11) DEFAULT NULL,
	`person_id` tinyblob,
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`counter`),
	KEY `Type_and_Number` (`type`,`type_no`),
	KEY `dimension_id` (`dimension_id`),
	KEY `dimension2_id` (`dimension2_id`),
	KEY `tran_date` (`tran_date`),
	KEY `account_and_tran_date` (`account`,`tran_date`)
) ENGINE=InnoDB AUTO_INCREMENT=55 ;

-- Data of table `0_gl_trans` --

INSERT INTO `0_gl_trans` VALUES
('1', '25', '1', '2025-05-05', '1510', '101', '20000', '0', '0', NULL, NULL, '{}'),
('2', '25', '1', '2025-05-05', '1510', '102', '15000', '0', '0', NULL, NULL, '{}'),
('3', '25', '1', '2025-05-05', '1510', '103', '1000', '0', '0', NULL, NULL, '{}'),
('4', '25', '1', '2025-05-05', '1550', '', '-36000', '0', '0', NULL, NULL, '{}'),
('5', '13', '1', '2025-05-10', '5010', '', '4000', '0', '0', NULL, NULL, '{}'),
('6', '13', '1', '2025-05-10', '1510', '', '-4000', '0', '0', NULL, NULL, '{}'),
('7', '10', '1', '2025-05-10', '4010', '', '-5714.29', '0', '0', NULL, NULL, '{}'),
('8', '10', '1', '2025-05-10', '4010', '', '-228.57', '0', '0', NULL, NULL, '{}'),
('9', '10', '1', '2025-05-10', '1200', '', '6240', '0', '0', '2', '1', '{}'),
('10', '10', '1', '2025-05-10', '2150', '', '-297.14', '0', '0', NULL, NULL, '{}'),
('11', '12', '1', '2025-05-10', '1065', '', '6240', '0', '0', NULL, NULL, '{}'),
('12', '12', '1', '2025-05-10', '1200', '', '-6240', '0', '0', '2', '1', '{}'),
('13', '29', '1', '2025-05-05', '1510', '1 * Vsmart Aris 6GB-64GB', '-400', '0', '0', NULL, NULL, '{}'),
('14', '29', '1', '2025-05-05', '1510', '1 * Vsmart Live 4 (6GB/64GB)', '-300', '0', '0', NULL, NULL, '{}'),
('15', '29', '1', '2025-05-05', '1510', '1 * Vsmart Live 4 Cover Case', '-20', '0', '0', NULL, NULL, '{}'),
('16', '29', '1', '2025-05-05', '1530', '1 * Support', '720', '0', '0', NULL, NULL, '{}'),
('17', '26', '1', '2025-05-05', '1530', '', '-720', '0', '0', NULL, NULL, '{}'),
('18', '26', '1', '2025-05-05', '1510', '', '720', '0', '0', NULL, NULL, '{}'),
('19', '25', '2', '2025-05-05', '1510', '101', '3000', '0', '0', NULL, NULL, '{}'),
('20', '25', '2', '2025-05-05', '1550', '', '-3000', '0', '0', NULL, NULL, '{}'),
('21', '20', '1', '2025-05-05', '2150', '', '150', '0', '0', NULL, NULL, '{}'),
('22', '20', '1', '2025-05-05', '2100', '', '-3150', '0', '0', '3', '1', '{}'),
('23', '20', '1', '2025-05-05', '1550', '', '3000', '0', '0', NULL, NULL, '{}'),
('24', '13', '2', '2025-05-07', '5010', '', '200', '0', '0', NULL, NULL, '{}'),
('25', '13', '2', '2025-05-07', '1510', '', '-200', '0', '0', NULL, NULL, '{}'),
('26', '10', '2', '2025-05-07', '4010', '', '-285.71', '0', '0', NULL, NULL, '{}'),
('27', '10', '2', '2025-05-07', '1200', '', '300', '0', '0', '2', '1', '{}'),
('28', '10', '2', '2025-05-07', '2150', '', '-14.29', '0', '0', NULL, NULL, '{}'),
('29', '12', '2', '2025-05-07', '1065', '', '300', '0', '0', NULL, NULL, '{}'),
('30', '12', '2', '2025-05-07', '1200', '', '-300', '0', '0', '2', '1', '{}'),
('31', '13', '3', '2025-05-07', '5010', '', '150', '1', '0', NULL, NULL, '{}'),
('32', '13', '3', '2025-05-07', '1510', '', '-150', '0', '0', NULL, NULL, '{}'),
('33', '13', '3', '2025-05-07', '5010', '', '10', '1', '0', NULL, NULL, '{}'),
('34', '13', '3', '2025-05-07', '1510', '', '-10', '0', '0', NULL, NULL, '{}'),
('35', '10', '3', '2025-05-07', '4010', '', '-250', '1', '0', NULL, NULL, '{}'),
('36', '10', '3', '2025-05-07', '4010', '', '-50', '1', '0', NULL, NULL, '{}'),
('37', '10', '3', '2025-05-07', '1200', '', '300', '0', '0', '2', '2', '{}'),
('38', '12', '3', '2025-05-07', '1065', '', '0', '0', '0', NULL, NULL, '{}'),
('39', '1', '1', '2025-05-07', '5010', '', '5', '1', '0', NULL, NULL, '{}'),
('40', '1', '1', '2025-05-07', '1060', '', '-5', '0', '0', NULL, NULL, '{}'),
('41', '13', '5', '2026-01-21', '5010', '', '750', '0', '0', NULL, NULL, '{}'),
('42', '13', '5', '2026-01-21', '1510', '', '-750', '0', '0', NULL, NULL, '{}'),
('43', '10', '5', '2026-01-21', '4010', '', '-1190.48', '0', '0', NULL, NULL, '{}'),
('44', '10', '5', '2026-01-21', '1200', '', '1250', '0', '0', '2', '1', '{}'),
('45', '10', '5', '2026-01-21', '2150', '', '-59.52', '0', '0', NULL, NULL, '{}'),
('46', '12', '4', '2026-01-21', '1065', '', '1250', '0', '0', NULL, NULL, '{}'),
('47', '12', '4', '2026-01-21', '1200', '', '-1250', '0', '0', '2', '1', '{}'),
('48', '25', '3', '2026-01-21', '1510', '102', '900', '0', '0', NULL, NULL, '{}'),
('49', '25', '3', '2026-01-21', '1550', '', '-900', '0', '0', NULL, NULL, '{}'),
('50', '20', '2', '2026-01-21', '2150', '', '45', '0', '0', NULL, NULL, '{}'),
('51', '20', '2', '2026-01-21', '2100', '', '-945', '0', '0', '3', '1', '{}'),
('52', '20', '2', '2026-01-21', '1550', '', '900', '0', '0', NULL, NULL, '{}'),
('53', '0', '1', '2025-12-31', '3590', 'Closing Year', '-2163.57', '0', '0', NULL, NULL, '{}'),
('54', '0', '1', '2025-12-31', '9990', 'Closing Year', '2163.57', '0', '0', NULL, NULL, '{}');

-- Structure of table `0_grn_batch` --

DROP TABLE IF EXISTS `0_grn_batch`;

CREATE TABLE `0_grn_batch` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`supplier_id` int(11) NOT NULL DEFAULT '0',
	`purch_order_no` int(11) DEFAULT NULL,
	`reference` varchar(60) NOT NULL DEFAULT '',
	`delivery_date` date NOT NULL DEFAULT '0000-00-00',
	`loc_code` varchar(5) DEFAULT NULL,
	`rate` double DEFAULT '1',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `delivery_date` (`delivery_date`),
	KEY `purch_order_no` (`purch_order_no`)
) ENGINE=InnoDB AUTO_INCREMENT=4 ;

-- Data of table `0_grn_batch` --

INSERT INTO `0_grn_batch` VALUES
('1', '1', '1', '001/2025', '2025-05-05', 'DEF', '1', '{}'),
('2', '1', '2', 'auto', '2025-05-05', 'DEF', '1', '{}'),
('3', '1', '3', 'auto', '2026-01-21', 'DEF', '1', '{}');

-- Structure of table `0_grn_items` --

DROP TABLE IF EXISTS `0_grn_items`;

CREATE TABLE `0_grn_items` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`grn_batch_id` int(11) DEFAULT NULL,
	`po_detail_item` int(11) NOT NULL DEFAULT '0',
	`item_code` varchar(20) NOT NULL DEFAULT '',
	`description` tinytext,
	`qty_recd` double NOT NULL DEFAULT '0',
	`quantity_inv` double NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_numbers_json` text DEFAULT NULL COMMENT 'JSON array of serial numbers received for this line',
	`expiry_date` date DEFAULT NULL COMMENT 'Expiry date of received goods',
	`manufacturing_date` date DEFAULT NULL COMMENT 'Manufacturing date of received goods',
	`inspection_status` varchar(20) DEFAULT 'none' COMMENT 'none|pending|pass|fail',
	PRIMARY KEY (`id`),
	KEY `grn_batch_id` (`grn_batch_id`)
) ENGINE=InnoDB AUTO_INCREMENT=6 ;

-- Data of table `0_grn_items` --

INSERT INTO `0_grn_items` VALUES
('1', '1', '1', '101', 'Vsmart Aris 6GB-64GB', '100', '0', '{}'),
('2', '1', '2', '102', 'Vsmart Live 4 (6GB/64GB)', '100', '0', '{}'),
('3', '1', '3', '103', 'Vsmart Live 4 Cover Case', '100', '0', '{}'),
('4', '2', '4', '101', 'Vsmart Aris 6GB-64GB', '15', '15', '{}'),
('5', '3', '5', '102', 'Vsmart Live 4 (6GB/64GB)', '6', '6', '{}');

-- Structure of table `0_groups` --

DROP TABLE IF EXISTS `0_groups`;

CREATE TABLE `0_groups` (
	`id` smallint(6) unsigned NOT NULL AUTO_INCREMENT,
	`description` varchar(60) NOT NULL DEFAULT '',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `description` (`description`)
) ENGINE=InnoDB AUTO_INCREMENT=4 ;

-- Data of table `0_groups` --

INSERT INTO `0_groups` VALUES
('1', 'Small', '0'),
('2', 'Medium', '0'),
('3', 'Large', '0');

-- Structure of table `0_item_codes` --

DROP TABLE IF EXISTS `0_item_codes`;

CREATE TABLE `0_item_codes` (
	`id` int(11) unsigned NOT NULL AUTO_INCREMENT,
	`item_code` varchar(20) NOT NULL,
	`stock_id` varchar(20) NOT NULL,
	`description` varchar(200) NOT NULL DEFAULT '',
	`category_id` smallint(6) unsigned NOT NULL,
	`quantity` double NOT NULL DEFAULT '1',
	`is_foreign` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `stock_id` (`stock_id`,`item_code`),
	KEY `item_code` (`item_code`)
) ENGINE=InnoDB AUTO_INCREMENT=9 ;

-- Data of table `0_item_codes` --

INSERT INTO `0_item_codes` VALUES
('1', '101', '101', 'Vsmart Aris 6GB-64GB', '1', '1', '0', '0', '{}'),
('2', '102', '102', 'Vsmart Live 4 (6GB/64GB)', '1', '1', '0', '0', '{}'),
('3', '103', '103', 'Vsmart Live 4 Cover Case', '1', '1', '0', '0', '{}'),
('4', '201', '201', 'AP Surf Set', '3', '1', '0', '0', '{}'),
('5', '301', '301', 'Support', '4', '1', '0', '0', '{}'),
('6', '501', '102', 'Live 4 Pack', '1', '1', '0', '0', '{}'),
('7', '501', '103', 'Live 4 Pack', '1', '1', '0', '0', '{}'),
('8', '202', '202', 'Maintenance', '4', '1', '0', '0', '{}');

-- Structure of table `0_item_tax_type_exemptions` --

DROP TABLE IF EXISTS `0_item_tax_type_exemptions`;

CREATE TABLE `0_item_tax_type_exemptions` (
	`item_tax_type_id` int(11) NOT NULL DEFAULT '0',
	`tax_type_id` int(11) NOT NULL DEFAULT '0',
	PRIMARY KEY (`item_tax_type_id`,`tax_type_id`)
) ENGINE=InnoDB ;

-- Data of table `0_item_tax_type_exemptions` --

-- Structure of table `0_item_tax_types` --

DROP TABLE IF EXISTS `0_item_tax_types`;

CREATE TABLE `0_item_tax_types` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`name` varchar(60) NOT NULL DEFAULT '',
	`exempt` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_item_tax_types` --

INSERT INTO `0_item_tax_types` VALUES
('1', 'Regular', '0', '0');

-- Structure of table `0_item_units` --

DROP TABLE IF EXISTS `0_item_units`;

CREATE TABLE `0_item_units` (
	`abbr` varchar(20) NOT NULL,
	`name` varchar(40) NOT NULL,
	`decimals` tinyint(2) NOT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`abbr`),
	UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB ;

-- Data of table `0_item_units` --

INSERT INTO `0_item_units` VALUES
('each', 'Each', '0', '0'),
('hr', 'Hours', '0', '0');

-- Structure of table `0_job_classes` --

DROP TABLE IF EXISTS `0_job_classes`;
CREATE TABLE IF NOT EXISTS `0_job_classes` (
	`job_class_id`   int(11)      NOT NULL AUTO_INCREMENT,
	`class_code`     varchar(20)  DEFAULT NULL,
	`class_name`     varchar(100) NOT NULL,
	`pay_basis`      tinyint(1)   NOT NULL DEFAULT '0' COMMENT '0=monthly, 1=daily, 2=hourly',
	`description`    text,
	`inactive`       tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`job_class_id`)
) ENGINE=InnoDB;

-- Data of table `0_job_classes` --

-- Structure of table `0_journal` --

DROP TABLE IF EXISTS `0_journal`;

CREATE TABLE `0_journal` (
	`type` smallint(6) NOT NULL DEFAULT '0',
	`trans_no` int(11) NOT NULL DEFAULT '0',
	`tran_date` date DEFAULT '0000-00-00',
	`reference` varchar(60) NOT NULL DEFAULT '',
	`source_ref` varchar(60) NOT NULL DEFAULT '',
	`event_date` date DEFAULT '0000-00-00',
	`doc_date` date NOT NULL DEFAULT '0000-00-00',
	`currency` char(3) NOT NULL DEFAULT '',
	`amount` double NOT NULL DEFAULT '0',
	`rate` double NOT NULL DEFAULT '1',
	PRIMARY KEY (`type`,`trans_no`),
	KEY `tran_date` (`tran_date`)
) ENGINE=InnoDB ;

-- Data of table `0_journal` --

INSERT INTO `0_journal` VALUES
('0', '1', '2025-12-31', '001/2012', '', '2025-12-31', '2025-12-31', 'USD', '2163.57', '1');

-- Structure of table `0_leave_details` --

DROP TABLE IF EXISTS `0_leave_details`;
CREATE TABLE IF NOT EXISTS `0_leave_details` (
	`id`                int(11)      NOT NULL AUTO_INCREMENT,
	`employee_id`       varchar(20)  NOT NULL DEFAULT '',
	`leave_type_id`     int(11)      NOT NULL DEFAULT '0',
	`from_date`         date         NOT NULL,
	`to_date`           date         NOT NULL,
	`days`              decimal(5,1) NOT NULL DEFAULT '0.0',
	`approval_status`   varchar(10)  NOT NULL DEFAULT 'pending',
	`approved_by`       varchar(20)  DEFAULT NULL,
	`approved_date`     date         DEFAULT NULL,
	`reason`            text,
	PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- Data of table `0_leave_details` --

-- Structure of table `0_leave_types` --

DROP TABLE IF EXISTS `0_leave_types`;
CREATE TABLE IF NOT EXISTS `0_leave_types` (
	`leave_id`              int(11)      NOT NULL AUTO_INCREMENT,
	`leave_code`            varchar(10)  NOT NULL,
	`leave_name`            varchar(100) NOT NULL,
	`pay_rate`              double       NOT NULL DEFAULT '100' COMMENT '100=full pay, 50=half pay, 0=unpaid',
	`is_paid`               tinyint(1)   NOT NULL DEFAULT '1',
	`is_carry_forward`      tinyint(1)   NOT NULL DEFAULT '0',
	`max_carry_forward`     double       NOT NULL DEFAULT '0',
	`is_encashable`         tinyint(1)   NOT NULL DEFAULT '0',
	`requires_document`     tinyint(1)   NOT NULL DEFAULT '0',
	`max_consecutive_days`  int(11)      DEFAULT NULL,
	`applicable_gender`     tinyint(1)   DEFAULT NULL COMMENT 'NULL=all, 0=male, 1=female',
	`deductable`            tinyint(1)   NOT NULL DEFAULT '0',
	`color_code`            varchar(10)  DEFAULT '#3498db',
	`description`           text,
	`inactive`              tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`leave_id`),
	UNIQUE KEY `leave_code` (`leave_code`)
) ENGINE=InnoDB;

-- Data of table `0_leave_types` --

-- Structure of table `0_loc_stock` --

DROP TABLE IF EXISTS `0_loc_stock`;

CREATE TABLE `0_loc_stock` (
	`loc_code` char(5) NOT NULL DEFAULT '',
	`stock_id` char(20) NOT NULL DEFAULT '',
	`reorder_level` double NOT NULL DEFAULT '0',
	PRIMARY KEY (`loc_code`,`stock_id`),
	KEY `stock_id` (`stock_id`)
) ENGINE=InnoDB ;

-- Data of table `0_loc_stock` --

INSERT INTO `0_loc_stock` VALUES
('DEF', '101', '0'),
('DEF', '102', '0'),
('DEF', '103', '0'),
('DEF', '201', '0'),
('DEF', '202', '0'),
('DEF', '301', '0');

-- Structure of table `0_locations` --

DROP TABLE IF EXISTS `0_locations`;

CREATE TABLE `0_locations` (
	`loc_code` varchar(5) NOT NULL DEFAULT '',
	`location_name` varchar(60) NOT NULL DEFAULT '',
	`delivery_address` tinytext NOT NULL,
	`phone` varchar(30) NOT NULL DEFAULT '',
	`phone2` varchar(30) NOT NULL DEFAULT '',
	`fax` varchar(30) NOT NULL DEFAULT '',
	`email` varchar(100) NOT NULL DEFAULT '',
	`contact` varchar(30) NOT NULL DEFAULT '',
	`fixed_asset` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`wh_enabled` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Enable WMS features for this location',
	`inbound_steps` tinyint(1) NOT NULL DEFAULT '1' COMMENT '1=direct, 2=receive+putaway, 3=receive+QC+putaway',
	`outbound_steps` tinyint(1) NOT NULL DEFAULT '1' COMMENT '1=ship, 2=pick+ship, 3=pick+pack+ship',
	`default_route_in` int(11) DEFAULT NULL COMMENT 'FK to wh_routes',
	`default_route_out` int(11) DEFAULT NULL COMMENT 'FK to wh_routes',
	`removal_strategy` varchar(20) DEFAULT 'fifo' COMMENT 'fifo|fefo|lifo|closest',
	`picking_method` varchar(20) DEFAULT 'single' COMMENT 'single|batch|wave|cluster',
	`barcode_prefix` varchar(10) DEFAULT NULL COMMENT 'Prefix for auto-generated location barcodes',
	`use_packages` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Enable packing/package tracking',
	`use_cycle_counts` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Enable cycle counting',
	`cross_dock_enabled` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Enable cross-docking at this warehouse',
	PRIMARY KEY (`loc_code`)
) ENGINE=InnoDB ;

-- Data of table `0_locations` --

INSERT INTO `0_locations` VALUES
('DEF', 'Default', 'N/A', '', '', '', '', '', '0', '0', '{}');

-- Structure of table `0_overtime` --

DROP TABLE IF EXISTS `0_overtime`;
CREATE TABLE IF NOT EXISTS `0_overtime` (
	`overtime_id`        int(11)      NOT NULL AUTO_INCREMENT,
	`overtime_code`      varchar(20)  DEFAULT NULL,
	`overtime_name`      varchar(100) NOT NULL,
	`pay_rate`           double       NOT NULL DEFAULT '1.5' COMMENT 'multiplier e.g. 1.5 = 150%',
	`max_hours_day`      double       DEFAULT NULL,
	`max_hours_month`    double       DEFAULT NULL,
	`account_code`       varchar(15)  DEFAULT NULL,
	`dimension_id`       int(11)      NOT NULL DEFAULT '0',
	`dimension2_id`      int(11)      NOT NULL DEFAULT '0',
	`requires_approval`  tinyint(1)   NOT NULL DEFAULT '1',
	`inactive`           tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`overtime_id`)
) ENGINE=InnoDB;

-- Data of table `0_overtime` --

-- Structure of table `0_pay_elements` --

DROP TABLE IF EXISTS `0_pay_elements`;
CREATE TABLE IF NOT EXISTS `0_pay_elements` (
	`element_id`        int(11)      NOT NULL AUTO_INCREMENT,
	`element_code`      varchar(20)  NOT NULL,
	`element_name`      varchar(100) NOT NULL,
	`element_category`  tinyint(1)   NOT NULL DEFAULT '0' COMMENT '0=basic, 1=allowance, 2=deduction, 3=employer_contribution, 4=statutory_deduction, 5=bonus, 6=reimbursement',
	`is_deduction`      tinyint(1)   NOT NULL DEFAULT '0',
	`amount_type`       tinyint(1)   NOT NULL DEFAULT '0' COMMENT '0=fixed, 1=pct_of_basic, 2=pct_of_gross, 3=formula, 4=attendance_based',
	`default_amount`    double       NOT NULL DEFAULT '0',
	`formula`           text         DEFAULT NULL COMMENT 'Used when amount_type=3. Variables: BASIC, GROSS, DAYS_WORKED, etc.',
	`account_code`      varchar(15)  NOT NULL DEFAULT '',
	`employer_account`  varchar(15)  DEFAULT NULL,
	`is_taxable`        tinyint(1)   NOT NULL DEFAULT '1',
	`affects_gross`     tinyint(1)   NOT NULL DEFAULT '1',
	`is_statutory`      tinyint(1)   NOT NULL DEFAULT '0',
	`max_amount`        double       DEFAULT NULL,
	`min_amount`        double       DEFAULT NULL,
	`display_order`     int(11)      NOT NULL DEFAULT '0',
	`description`       text,
	`inactive`          tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`element_id`),
	UNIQUE KEY `element_code` (`element_code`)
) ENGINE=InnoDB;

-- Data of table `0_pay_elements` --

-- Structure of table `0_pay_grades` --

DROP TABLE IF EXISTS `0_pay_grades`;
CREATE TABLE IF NOT EXISTS `0_pay_grades` (
	`grade_id`       int(11)      NOT NULL AUTO_INCREMENT,
	`grade_code`     varchar(20)  DEFAULT NULL,
	`grade_name`     varchar(60)  NOT NULL,
	`grade_level`    int(11)      NOT NULL DEFAULT '0',
	`min_salary`     double       NOT NULL DEFAULT '0',
	`mid_salary`     double       NOT NULL DEFAULT '0',
	`max_salary`     double       NOT NULL DEFAULT '0',
	`description`    text,
	`inactive`       tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`grade_id`)
) ENGINE=InnoDB;

-- Data of table `0_pay_grades` --

-- Structure of table `0_payment_terms` --

DROP TABLE IF EXISTS `0_payment_terms`;

CREATE TABLE `0_payment_terms` (
	`terms_indicator` int(11) NOT NULL AUTO_INCREMENT,
	`terms` char(80) NOT NULL DEFAULT '',
	`days_before_due` smallint(6) NOT NULL DEFAULT '0',
	`day_in_following_month` smallint(6) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`terms_indicator`),
	UNIQUE KEY `terms` (`terms`)
) ENGINE=InnoDB AUTO_INCREMENT=6 ;

-- Data of table `0_payment_terms` --

INSERT INTO `0_payment_terms` VALUES
('1', 'Due 15th Of the Following Month', '0', '17', '0'),
('2', 'Due By End Of The Following Month', '0', '30', '0'),
('3', 'Payment due within 10 days', '10', '0', '0'),
('4', 'Cash Only', '0', '0', '0'),
('5', 'Prepaid', '-1', '0', '0');

-- Structure of table `0_positions` --

DROP TABLE IF EXISTS `0_positions`;
CREATE TABLE IF NOT EXISTS `0_positions` (
	`position_id`              int(11)      NOT NULL AUTO_INCREMENT,
	`position_code`            varchar(20)  DEFAULT NULL,
	`position_name`            text         NOT NULL,
	`job_class_id`             int(11)      NOT NULL DEFAULT '0',
	`department_id`            int(11)      DEFAULT NULL,
	`reports_to_position_id`   int(11)      DEFAULT NULL,
	`basic_amount`             double       NOT NULL DEFAULT '0',
	`min_salary`               double       DEFAULT NULL,
	`max_salary`               double       DEFAULT NULL,
	`budgeted_headcount`       int(11)      NOT NULL DEFAULT '1',
	`is_manager`               tinyint(1)   NOT NULL DEFAULT '0',
	`description`              text,
	`inactive`                 tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`position_id`),
	KEY `job_class_id` (`job_class_id`),
	KEY `department_id` (`department_id`)
) ENGINE=InnoDB;

-- Data of table `0_positions` --

-- Structure of table `0_payslips` --

DROP TABLE IF EXISTS `0_payslips`;
CREATE TABLE IF NOT EXISTS `0_payslips` (
	`payslip_id`              int(11)      NOT NULL AUTO_INCREMENT,
	`payroll_period_id`       int(11)      DEFAULT NULL,
	`trans_no`                int(11)      NOT NULL DEFAULT '0',
	`reference`               varchar(60)  NOT NULL DEFAULT '',
	`employee_id`             varchar(20)  NOT NULL DEFAULT '',
	`tran_date`               date         NOT NULL,
	`from_date`               date         NOT NULL,
	`to_date`                 date         NOT NULL,
	`department_id`           int(11)      DEFAULT NULL,
	`position_id`             int(11)      DEFAULT NULL,
	`grade_id`                int(11)      DEFAULT NULL,
	`basic_salary`            double       NOT NULL DEFAULT '0',
	`total_earnings`          double       NOT NULL DEFAULT '0',
	`gross_salary`            double       NOT NULL DEFAULT '0',
	`total_deductions`        double       NOT NULL DEFAULT '0',
	`net_salary`              double       NOT NULL DEFAULT '0',
	`employer_contributions`  double       NOT NULL DEFAULT '0',
	`total_cost`              double       NOT NULL DEFAULT '0',
	`working_days`            double       NOT NULL DEFAULT '0',
	`worked_days`             double       NOT NULL DEFAULT '0',
	`overtime_hours`          double       NOT NULL DEFAULT '0',
	`leave_days`              double       NOT NULL DEFAULT '0',
	`absent_days`             double       NOT NULL DEFAULT '0',
	`leaves`                  int(11)      NOT NULL DEFAULT '0',
	`deductable_leaves`       int(11)      NOT NULL DEFAULT '0',
	`loan_deduction`          double       NOT NULL DEFAULT '0',
	`payable_amount`          double       NOT NULL DEFAULT '0',
	`salary_amount`           double       NOT NULL DEFAULT '0',
	`status`                  tinyint(1)   NOT NULL DEFAULT '0' COMMENT '0=draft, 1=confirmed, 2=posted, 3=paid, 4=voided',
	`payment_trans_no`        int(11)      DEFAULT NULL,
	`notes`                   text,
	`custom_data`             json         DEFAULT NULL,
	PRIMARY KEY (`payslip_id`),
	UNIQUE KEY `period_employee` (`payroll_period_id`, `employee_id`),
	KEY `employee_id` (`employee_id`),
	KEY `tran_date` (`tran_date`)
) ENGINE=InnoDB;
-- Data of table `0_payslips` --

-- Structure of table `0_prices` --

DROP TABLE IF EXISTS `0_prices`;

CREATE TABLE `0_prices` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`stock_id` varchar(20) NOT NULL DEFAULT '',
	`sales_type_id` int(11) NOT NULL DEFAULT '0',
	`curr_abrev` char(3) NOT NULL DEFAULT '',
	`price` double NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `price` (`stock_id`,`sales_type_id`,`curr_abrev`)
) ENGINE=InnoDB AUTO_INCREMENT=4 ;

-- Data of table `0_prices` --

INSERT INTO `0_prices` VALUES
('1', '101', '1', 'USD', '300', '{}'),
('2', '102', '1', 'USD', '250', '{}'),
('3', '103', '1', 'USD', '50', '{}');

-- Structure of table `0_print_profiles` --

DROP TABLE IF EXISTS `0_print_profiles`;

CREATE TABLE `0_print_profiles` (
	`id` smallint(6) unsigned NOT NULL AUTO_INCREMENT,
	`profile` varchar(30) NOT NULL,
	`report` varchar(5) DEFAULT NULL,
	`printer` tinyint(3) unsigned DEFAULT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `profile` (`profile`,`report`)
) ENGINE=InnoDB AUTO_INCREMENT=10 ;

-- Data of table `0_print_profiles` --

INSERT INTO `0_print_profiles` VALUES
('1', 'Out of office', NULL, '0'),
('2', 'Sales Department', NULL, '0'),
('3', 'Central', NULL, '2'),
('4', 'Sales Department', '104', '2'),
('5', 'Sales Department', '105', '2'),
('6', 'Sales Department', '107', '2'),
('7', 'Sales Department', '109', '2'),
('8', 'Sales Department', '110', '2'),
('9', 'Sales Department', '201', '2');

-- Structure of table `0_printers` --

DROP TABLE IF EXISTS `0_printers`;

CREATE TABLE `0_printers` (
	`id` tinyint(3) unsigned NOT NULL AUTO_INCREMENT,
	`name` varchar(20) NOT NULL,
	`description` varchar(60) NOT NULL,
	`queue` varchar(20) NOT NULL,
	`host` varchar(40) NOT NULL,
	`port` smallint(11) unsigned NOT NULL,
	`timeout` tinyint(3) unsigned NOT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=4 ;

-- Data of table `0_printers` --

INSERT INTO `0_printers` VALUES
('1', 'QL500', 'Label printer', 'QL500', 'server', '127', '20'),
('2', 'Samsung', 'Main network printer', 'scx4521F', 'server', '515', '5'),
('3', 'Local', 'Local print server at user IP', 'lp', '', '515', '10');

-- Structure of table `0_purch_data` --

DROP TABLE IF EXISTS `0_purch_data`;

CREATE TABLE `0_purch_data` (
	`supplier_id` int(11) NOT NULL DEFAULT '0',
	`stock_id` char(20) NOT NULL DEFAULT '',
	`price` double NOT NULL DEFAULT '0',
	`suppliers_uom` char(50) NOT NULL DEFAULT '',
	`conversion_factor` double NOT NULL DEFAULT '1',
	`supplier_description` char(50) NOT NULL DEFAULT '',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`supplier_id`,`stock_id`)
) ENGINE=InnoDB ;

-- Data of table `0_purch_data` --

INSERT INTO `0_purch_data` VALUES
('1', '101', '200', '', '1', 'Vsmart Aris 6GB-64GB', '{}'),
('1', '102', '150', '', '1', 'Vsmart Live 4 (6GB/64GB)', '{}'),
('1', '103', '10', '', '1', 'Vsmart Live 4 Cover Case', '{}');

-- Structure of table `0_purch_order_details` --

DROP TABLE IF EXISTS `0_purch_order_details`;

CREATE TABLE `0_purch_order_details` (
	`po_detail_item` int(11) NOT NULL AUTO_INCREMENT,
	`order_no` int(11) NOT NULL DEFAULT '0',
	`item_code` varchar(20) NOT NULL DEFAULT '',
	`description` tinytext,
	`delivery_date` date NOT NULL DEFAULT '0000-00-00',
	`qty_invoiced` double NOT NULL DEFAULT '0',
	`unit_price` double NOT NULL DEFAULT '0',
	`act_price` double NOT NULL DEFAULT '0',
	`std_cost_unit` double NOT NULL DEFAULT '0',
	`quantity_ordered` double NOT NULL DEFAULT '0',
	`quantity_received` double NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`cross_dock_so_no` int(11) DEFAULT NULL COMMENT 'Linked SO for cross-dock',
	`cross_dock_so_line` int(11) DEFAULT NULL COMMENT 'Linked SO line ID for cross-dock',
	PRIMARY KEY (`po_detail_item`),
	KEY `order` (`order_no`,`po_detail_item`),
	KEY `itemcode` (`item_code`)
) ENGINE=InnoDB AUTO_INCREMENT=6 ;

-- Data of table `0_purch_order_details` --

INSERT INTO `0_purch_order_details` VALUES
('1', '1', '101', 'Vsmart Aris 6GB-64GB', '2025-05-15', '0', '200', '200', '200', '100', '100', '{}'),
('2', '1', '102', 'Vsmart Live 4 (6GB/64GB)', '2025-05-15', '0', '150', '150', '150', '100', '100', '{}'),
('3', '1', '103', 'Vsmart Live 4 Cover Case', '2025-05-15', '0', '10', '10', '10', '100', '100', '{}'),
('4', '2', '101', 'Vsmart Aris 6GB-64GB', '2025-05-05', '15', '200', '200', '200', '15', '15', '{}'),
('5', '3', '102', 'Vsmart Live 4 (6GB/64GB)', '2026-01-21', '6', '150', '150', '150', '6', '6', '{}');

-- Structure of table `0_purch_orders` --

DROP TABLE IF EXISTS `0_purch_orders`;

CREATE TABLE `0_purch_orders` (
	`order_no` int(11) NOT NULL AUTO_INCREMENT,
	`supplier_id` int(11) NOT NULL DEFAULT '0',
	`comments` tinytext,
	`ord_date` date NOT NULL DEFAULT '0000-00-00',
	`reference` tinytext NOT NULL,
	`supp_reference` tinytext,
	`into_stock_location` varchar(5) NOT NULL DEFAULT '',
	`delivery_address` tinytext NOT NULL,
	`total` double NOT NULL DEFAULT '0',
	`prep_amount` double NOT NULL DEFAULT '0',
	`alloc` double NOT NULL DEFAULT '0',
	`tax_included` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`drop_ship` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'This is a drop-ship PO',
	`drop_ship_so_no` int(11) DEFAULT NULL COMMENT 'Linked SO for drop-ship',
	`agreement_id` INT NOT NULL DEFAULT 0 COMMENT 'FK to purch_agreements.id (blanket order linking)',
	`requisition_id` INT NOT NULL DEFAULT 0 COMMENT 'FK to purch_requisitions.id (originated from requisition)',
	`rfq_id` INT NOT NULL DEFAULT 0 COMMENT 'FK to purch_rfq.id (response to this RFQ)',
	PRIMARY KEY (`order_no`),
	KEY `ord_date` (`ord_date`)
) ENGINE=InnoDB AUTO_INCREMENT=4 ;

-- Data of table `0_purch_orders` --

INSERT INTO `0_purch_orders` VALUES
('1', '1', NULL, '2025-05-05', '001/2025', NULL, 'DEF', 'N/A', '37800', '0', '0', '0', '{}'),
('2', '1', NULL, '2025-05-05', 'auto', 'rr4', 'DEF', 'N/A', '3150', '0', '0', '0', '{}'),
('3', '1', NULL, '2026-01-21', 'auto', 'asd5', 'DEF', 'N/A', '945', '0', '0', '0', '{}');

-- Structure of table `0_quick_entries` --

DROP TABLE IF EXISTS `0_quick_entries`;

CREATE TABLE `0_quick_entries` (
	`id` smallint(6) unsigned NOT NULL AUTO_INCREMENT,
	`type` tinyint(1) NOT NULL DEFAULT '0',
	`description` varchar(60) NOT NULL,
	`usage` varchar(120) DEFAULT NULL,
	`base_amount` double NOT NULL DEFAULT '0',
	`base_desc` varchar(60) DEFAULT NULL,
	`bal_type` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	KEY `description` (`description`)
) ENGINE=InnoDB AUTO_INCREMENT=4 ;

-- Data of table `0_quick_entries` --

INSERT INTO `0_quick_entries` VALUES
('1', '1', 'Maintenance', NULL, '0', 'Amount', '0'),
('2', '4', 'Phone', NULL, '0', 'Amount', '0'),
('3', '2', 'Cash Sales', 'Retail sales without invoice', '0', 'Amount', '0');

-- Structure of table `0_quick_entry_lines` --

DROP TABLE IF EXISTS `0_quick_entry_lines`;

CREATE TABLE `0_quick_entry_lines` (
	`id` smallint(6) unsigned NOT NULL AUTO_INCREMENT,
	`qid` smallint(6) unsigned NOT NULL,
	`amount` double DEFAULT '0',
	`memo` tinytext NOT NULL,
	`action` varchar(2) NOT NULL,
	`dest_id` varchar(15) NOT NULL DEFAULT '',
	`dimension_id` smallint(6) unsigned DEFAULT NULL,
	`dimension2_id` smallint(6) unsigned DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `qid` (`qid`)
) ENGINE=InnoDB AUTO_INCREMENT=7 ;

-- Data of table `0_quick_entry_lines` --

INSERT INTO `0_quick_entry_lines` VALUES
('1', '1', '0', '', 't-', '1', '0', '0'),
('2', '2', '0', '', 't-', '1', '0', '0'),
('3', '3', '0', '', 't-', '1', '0', '0'),
('4', '3', '0', '', '=', '4010', '0', '0'),
('5', '1', '0', '', '=', '5765', '0', '0'),
('6', '2', '0', '', '=', '5780', '0', '0');

-- Structure of table `0_recurrent_invoices` --

DROP TABLE IF EXISTS `0_recurrent_invoices`;

CREATE TABLE `0_recurrent_invoices` (
	`id` smallint(6) unsigned NOT NULL AUTO_INCREMENT,
	`description` varchar(60) NOT NULL DEFAULT '',
	`order_no` int(11) unsigned NOT NULL,
	`debtor_no` int(11) unsigned DEFAULT NULL,
	`group_no` smallint(6) unsigned DEFAULT NULL,
	`days` int(11) NOT NULL DEFAULT '0',
	`monthly` int(11) NOT NULL DEFAULT '0',
	`begin` date NOT NULL DEFAULT '0000-00-00',
	`end` date NOT NULL DEFAULT '0000-00-00',
	`last_sent` date NOT NULL DEFAULT '0000-00-00',
	PRIMARY KEY (`id`),
	UNIQUE KEY `description` (`description`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_recurrent_invoices` --

INSERT INTO `0_recurrent_invoices` VALUES
('1', 'Weekly Maintenance', '6', '1', '1', '7', '0', '2025-04-01', '2026-05-07', '2025-04-08');

-- Structure of table `0_reflines` --

DROP TABLE IF EXISTS `0_reflines`;

CREATE TABLE `0_reflines` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`trans_type` int(11) NOT NULL,
	`prefix` char(5) NOT NULL DEFAULT '',
	`pattern` varchar(35) NOT NULL DEFAULT '1',
	`description` varchar(60) NOT NULL DEFAULT '',
	`default` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `prefix` (`trans_type`,`prefix`)
) ENGINE=InnoDB AUTO_INCREMENT=23 ;

-- Data of table `0_reflines` --

INSERT INTO `0_reflines` VALUES
('1', '0', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('2', '1', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('3', '2', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('4', '4', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('5', '10', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('6', '11', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('7', '12', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('8', '13', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('9', '16', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('10', '17', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('11', '18', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('12', '20', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('13', '21', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('14', '22', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('15', '25', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('16', '26', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('17', '28', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('18', '29', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('19', '30', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('20', '32', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('21', '35', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('22', '40', '', '{001}/{YYYY}', '', '1', '0', '{}'),
('23', '80', '', '{001}/{YYYY}', '', '1', '0', '{}');

-- Structure of table `0_refs` --

DROP TABLE IF EXISTS `0_refs`;

CREATE TABLE `0_refs` (
	`id` int(11) NOT NULL DEFAULT '0',
	`type` int(11) NOT NULL DEFAULT '0',
	`reference` varchar(100) NOT NULL DEFAULT '',
	PRIMARY KEY (`id`,`type`),
	KEY `Type_and_Reference` (`type`,`reference`)
) ENGINE=InnoDB ;

-- Data of table `0_refs` --

INSERT INTO `0_refs` VALUES
('1', '1', '001/2025'),
('1', '10', '001/2025'),
('5', '10', '001/2026'),
('2', '10', '002/2025'),
('3', '10', '003/2025'),
('4', '10', '004/2025'),
('1', '12', '001/2025'),
('4', '12', '001/2026'),
('2', '12', '002/2025'),
('3', '12', '003/2025'),
('1', '18', '001/2025'),
('1', '20', '001/2025'),
('2', '20', '001/2026'),
('1', '25', '001/2025'),
('1', '26', '001/2025'),
('2', '26', '002/2025'),
('3', '26', '003/2025'),
('3', '30', '001/2025'),
('4', '30', '002/2025'),
('6', '30', '003/2025'),
('1', '40', '001/2025');

-- Structure of table `0_salary_structure` --

DROP TABLE IF EXISTS `0_salary_structure`;
CREATE TABLE IF NOT EXISTS `0_salary_structure` (
	`structure_id`   int(11)      NOT NULL AUTO_INCREMENT,
	`position_id`    int(11)      NOT NULL,
	`grade_id`       int(11)      NOT NULL DEFAULT '0',
	`element_id`     int(11)      NOT NULL,
	`pay_amount`     double       NOT NULL DEFAULT '0',
	`formula`        text         DEFAULT NULL COMMENT 'Overrides element formula for this structure entry',
	`effective_from` date         NOT NULL,
	`effective_to`   date         DEFAULT NULL,
	`is_active`      tinyint(1)   NOT NULL DEFAULT '1',
	PRIMARY KEY (`structure_id`),
	UNIQUE KEY `unique_structure` (`position_id`, `grade_id`, `element_id`, `effective_from`),
	KEY `element_id` (`element_id`)
) ENGINE=InnoDB;

-- Data of table `0_salary_structure` --

-- Structure of table `0_sales_order_details` --

DROP TABLE IF EXISTS `0_sales_order_details`;

CREATE TABLE `0_sales_order_details` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`order_no` int(11) NOT NULL DEFAULT '0',
	`trans_type` smallint(6) NOT NULL DEFAULT '30',
	`stk_code` varchar(20) NOT NULL DEFAULT '',
	`description` tinytext,
	`qty_sent` double NOT NULL DEFAULT '0',
	`unit_price` double NOT NULL DEFAULT '0',
	`quantity` double NOT NULL DEFAULT '0',
	`invoiced` double NOT NULL DEFAULT '0',
	`discount_percent` double NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`drop_ship` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Line is drop-shipped from supplier',
	`drop_ship_supplier_id` int(11) DEFAULT NULL COMMENT 'Supplier for drop-ship',
	`drop_ship_po_no` int(11) DEFAULT NULL COMMENT 'Linked PO number for drop-ship',
	PRIMARY KEY (`id`),
	KEY `sorder` (`trans_type`,`order_no`),
	KEY `stkcode` (`stk_code`)
) ENGINE=InnoDB AUTO_INCREMENT=12 ;

-- Data of table `0_sales_order_details` --

INSERT INTO `0_sales_order_details` VALUES
('1', '1', '30', '101', 'Vsmart Aris 6GB-64GB', '20', '300', '20', '0', '0', '{}'),
('2', '1', '30', '301', 'Support', '3', '80', '3', '0', '0', '{}'),
('3', '2', '30', '101', 'Vsmart Aris 6GB-64GB', '1', '300', '1', '0', '0', '{}'),
('4', '3', '30', '102', 'Vsmart Live 4 (6GB/64GB)', '0', '250', '1', '0', '0', '{}'),
('5', '3', '30', '103', 'Vsmart Live 4 Cover Case', '0', '50', '1', '0', '0', '{}'),
('6', '4', '30', '101', 'Vsmart Aris 6GB-64GB', '0', '267.14', '1', '0', '0', '{}'),
('7', '5', '30', '102', 'Vsmart Live 4 (6GB/64GB)', '1', '222.62', '1', '0', '0', '{}'),
('8', '5', '30', '103', 'Vsmart Live 4 Cover Case', '1', '44.52', '1', '0', '0', '{}'),
('9', '6', '30', '202', 'Maintenance', '0', '90', '5', '0', '0', '{}'),
('10', '7', '30', '202', 'Maintenance', '5', '0', '5', '0', '0', '{}'),
('11', '8', '30', '102', 'Vsmart Live 4 (6GB/64GB)', '5', '250', '5', '0', '0', '{}');

-- Structure of table `0_sales_orders` --

DROP TABLE IF EXISTS `0_sales_orders`;

CREATE TABLE `0_sales_orders` (
	`order_no` int(11) NOT NULL,
	`trans_type` smallint(6) NOT NULL DEFAULT '30',
	`version` tinyint(1) unsigned NOT NULL DEFAULT '0',
	`type` tinyint(1) NOT NULL DEFAULT '0',
	`debtor_no` int(11) NOT NULL DEFAULT '0',
	`branch_code` int(11) NOT NULL DEFAULT '0',
	`salesman_code` int(11) NOT NULL DEFAULT '0',
	`reference` varchar(100) NOT NULL DEFAULT '',
	`customer_ref` tinytext NOT NULL,
	`comments` text  NOT NULL DEFAULT '',
	`ord_date` date NOT NULL DEFAULT '0000-00-00',
	`order_type` int(11) NOT NULL DEFAULT '0',
	`ship_via` int(11) NOT NULL DEFAULT '0',
	`delivery_address` tinytext NOT NULL,
	`contact_phone` varchar(30) DEFAULT NULL,
	`contact_email` varchar(100) DEFAULT NULL,
	`deliver_to` tinytext NOT NULL,
	`freight_cost` double NOT NULL DEFAULT '0',
	`from_stk_loc` varchar(5) NOT NULL DEFAULT '',
	`delivery_date` date NOT NULL DEFAULT '0000-00-00',
	`payment_terms` int(11) DEFAULT NULL,
	`total` double NOT NULL DEFAULT '0',
	`prep_amount` double NOT NULL DEFAULT '0',
	`alloc` double NOT NULL DEFAULT '0',
	`template_id` int(11) NOT NULL DEFAULT '0',
	`validity_date` date DEFAULT NULL,
	`opportunity_id` int(11) NOT NULL DEFAULT '0',
	`terms_and_conditions` text DEFAULT NULL,
	`margin_total` double NOT NULL DEFAULT '0',
	`cost_total` double NOT NULL DEFAULT '0',
	`agreement_id` int(11) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`trans_type`,`order_no`)
) ENGINE=InnoDB;

-- Data of table `0_sales_orders` --

INSERT INTO `0_sales_orders` VALUES
('1', '30', '1', '0', '1', '1', '1', 'auto', '', NULL, '2025-05-10', '1', '1', 'N/A', NULL, NULL, 'Donald Easter LLC', '0', 'DEF', '2025-05-05', '4', '6240', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}'),
('2', '30', '1', '0', '1', '1', '1', 'auto', '', NULL, '2025-05-07', '1', '1', 'N/A', NULL, NULL, 'Donald Easter LLC', '0', 'DEF', '2025-05-07', '4', '300', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}'),
('3', '30', '0', '0', '1', '1', '1', '001/2025', '', NULL, '2025-05-07', '1', '1', 'N/A', NULL, NULL, 'Donald Easter LLC', '0', 'DEF', '2025-05-08', '4', '300', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}'),
('4', '30', '0', '0', '2', '2', '1', '002/2025', '', NULL, '2025-05-07', '1', '1', 'N/A', NULL, NULL, 'MoneyMaker Ltd.', '0', 'DEF', '2025-05-08', '1', '267.14', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}'),
('5', '30', '1', '0', '2', '2', '1', 'auto', '', NULL, '2025-05-07', '1', '1', 'N/A', NULL, NULL, 'MoneyMaker Ltd.', '0', 'DEF', '2025-06-17', '1', '267.14', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}'),
('6', '30', '0', '1', '1', '1', '1', '003/2025', '', NULL, '2025-05-07', '1', '1', 'N/A', NULL, NULL, 'Donald Easter LLC', '0', 'DEF', '2025-05-08', '4', '450', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}'),
('7', '30', '1', '0', '1', '1', '1', 'auto', '', 'Recurrent Invoice covers period 04/01/2025 - 04/07/2025.', '2025-05-07', '1', '1', 'N/A', NULL, NULL, 'Donald Easter LLC', '0', 'DEF', '2025-05-07', '4', '0', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}'),
('8', '30', '1', '0', '1', '1', '1', 'auto', '', NULL, '2026-01-21', '1', '1', 'N/A', NULL, NULL, 'Donald Easter LLC', '0', 'DEF', '2026-01-21', '4', '1250', '0', '0', 0, NULL, 0, NULL, 0, 0, 0, '{}');


-- Structure of table `0_sales_pricelists` -- (Phase 1)
CREATE TABLE IF NOT EXISTS `0_sales_pricelists` (
	`id` INT AUTO_INCREMENT PRIMARY KEY,
	`name` VARCHAR(100) NOT NULL,
	`description` TINYTEXT,
	`currency` CHAR(3) DEFAULT '',
	`is_default` TINYINT(1) DEFAULT 0,
	`date_start` DATE DEFAULT NULL,
	`date_end` DATE DEFAULT NULL,
	`priority` INT DEFAULT 10,
	`min_order_amount` DOUBLE DEFAULT 0,
	`applicable_to` ENUM('all','sales_type','customer','customer_group','branch') DEFAULT 'all',
	`applicable_id` INT DEFAULT 0,
	`inactive` TINYINT(1) DEFAULT 0,
	`custom_data` JSON,
	KEY `idx_dates` (`date_start`, `date_end`),
	KEY `idx_applicable` (`applicable_to`, `applicable_id`),
	KEY `idx_priority` (`priority`)
) ENGINE=InnoDB;

-- Data of table `0_sales_pricelists` --

-- Structure of table `0_sales_pricelist_rules` -- (Phase 1)
CREATE TABLE IF NOT EXISTS `0_sales_pricelist_rules` (
	`id` INT AUTO_INCREMENT PRIMARY KEY,
	`pricelist_id` INT NOT NULL,
	`stock_id` VARCHAR(20) DEFAULT '',
	`stock_category_id` INT DEFAULT 0,
	`min_quantity` DOUBLE DEFAULT 0,
	`computation_type` ENUM('fixed','percentage','formula') DEFAULT 'fixed',
	`fixed_price` DOUBLE DEFAULT 0,
	`percentage` DOUBLE DEFAULT 0,
	`base_price_type` ENUM('list_price','cost','other_pricelist') DEFAULT 'list_price',
	`base_pricelist_id` INT DEFAULT 0,
	`surcharge` DOUBLE DEFAULT 0,
	`rounding` DOUBLE DEFAULT 0.01,
	`priority` INT DEFAULT 10,
	`date_start` DATE DEFAULT NULL,
	`date_end` DATE DEFAULT NULL,
	`inactive` TINYINT(1) DEFAULT 0,
	`custom_data` JSON,
	KEY `idx_pricelist` (`pricelist_id`),
	KEY `idx_stock` (`stock_id`),
	KEY `idx_category` (`stock_category_id`),
	KEY `idx_priority` (`priority`),
	KEY `idx_dates` (`date_start`, `date_end`)
) ENGINE=InnoDB;

-- Data of table `0_sales_pricelist_rules` --

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
	VALUES ('use_advanced_pricelists', 'sys', 'tinyint', 1, '0');

-- Structure of table `0_sales_quotation_templates` --

DROP TABLE IF EXISTS `0_sales_quotation_templates`;

CREATE TABLE `0_sales_quotation_templates` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`name` varchar(100) NOT NULL,
	`description` tinytext,
	`validity_days` int(11) NOT NULL DEFAULT '30',
	`terms_and_conditions` text,
	`notes` text,
	`sales_type` int(11) NOT NULL DEFAULT '0',
	`default_payment_terms` int(11) NOT NULL DEFAULT '0',
	`default_ship_via` int(11) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` longtext,
	PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_quotation_templates` --

-- Structure of table `0_sales_quotation_template_lines` --

DROP TABLE IF EXISTS `0_sales_quotation_template_lines`;

CREATE TABLE `0_sales_quotation_template_lines` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`template_id` int(11) NOT NULL,
	`line_type` enum('product','section','note','optional') NOT NULL DEFAULT 'product',
	`stock_id` varchar(20) NOT NULL DEFAULT '',
	`description` tinytext,
	`quantity` double NOT NULL DEFAULT '1',
	`discount_percent` double NOT NULL DEFAULT '0',
	`is_optional` tinyint(1) NOT NULL DEFAULT '0',
	`sort_order` int(11) NOT NULL DEFAULT '0',
	`custom_data` longtext,
	PRIMARY KEY (`id`),
	KEY `idx_template` (`template_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_quotation_template_lines` --

-- Structure of table `0_sales_agreements` --

DROP TABLE IF EXISTS `0_sales_agreements`;

CREATE TABLE `0_sales_agreements` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`reference` varchar(30) NOT NULL DEFAULT '',
	`agreement_type` enum('blanket_order','framework_agreement','contract') NOT NULL DEFAULT 'blanket_order',
	`debtor_no` int(11) NOT NULL,
	`branch_code` int(11) NOT NULL DEFAULT '0',
	`salesman_id` int(11) NOT NULL DEFAULT '0',
	`status` enum('draft','confirmed','active','expired','cancelled') NOT NULL DEFAULT 'draft',
	`date_start` date NOT NULL,
	`date_end` date NOT NULL,
	`currency` char(3) NOT NULL DEFAULT '',
	`payment_terms` int(11) NOT NULL DEFAULT '0',
	`total_committed` double NOT NULL DEFAULT '0',
	`total_ordered` double NOT NULL DEFAULT '0',
	`total_delivered` double NOT NULL DEFAULT '0',
	`total_invoiced` double NOT NULL DEFAULT '0',
	`auto_renew` tinyint(1) NOT NULL DEFAULT '0',
	`renewal_period_months` int(11) NOT NULL DEFAULT '12',
	`terms_and_conditions` text DEFAULT NULL,
	`notes` text DEFAULT NULL,
	`dimension_id` int(11) NOT NULL DEFAULT '0',
	`dimension2_id` int(11) NOT NULL DEFAULT '0',
	`created_by` int(11) NOT NULL DEFAULT '0',
	`created_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_debtor` (`debtor_no`),
	KEY `idx_status` (`status`),
	KEY `idx_dates` (`date_start`,`date_end`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- Data of table `0_sales_agreements` --

-- Structure of table `0_sales_agreement_lines` --

DROP TABLE IF EXISTS `0_sales_agreement_lines`;

CREATE TABLE `0_sales_agreement_lines` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`agreement_id` int(11) NOT NULL,
	`stock_id` varchar(20) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '',
	`description` tinytext DEFAULT NULL,
	`committed_qty` double NOT NULL DEFAULT '0',
	`ordered_qty` double NOT NULL DEFAULT '0',
	`delivered_qty` double NOT NULL DEFAULT '0',
	`invoiced_qty` double NOT NULL DEFAULT '0',
	`unit_price` double NOT NULL DEFAULT '0',
	`discount_percent` double NOT NULL DEFAULT '0',
	`price_valid_until` date DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_agreement_id` (`agreement_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- Data of table `0_sales_agreement_lines` --

-- Structure of table `0_sales_discount_programs` -- (Phase 4)

CREATE TABLE IF NOT EXISTS `0_sales_discount_programs` (
	`id` INT NOT NULL AUTO_INCREMENT,
	`name` VARCHAR(100) NOT NULL,
	`program_type` ENUM('coupon','loyalty','automatic','volume') NOT NULL DEFAULT 'automatic',
	`status` ENUM('draft','active','expired','cancelled') NOT NULL DEFAULT 'draft',
	`date_start` DATE DEFAULT NULL,
	`date_end` DATE DEFAULT NULL,
	`min_order_amount` DOUBLE NOT NULL DEFAULT '0',
	`min_quantity` DOUBLE NOT NULL DEFAULT '0',
	`applicable_items` VARCHAR(500) NOT NULL DEFAULT '',
	`applicable_categories` VARCHAR(200) NOT NULL DEFAULT '',
	`applicable_customers` VARCHAR(200) NOT NULL DEFAULT '',
	`applicable_sales_types` VARCHAR(100) NOT NULL DEFAULT '',
	`reward_type` ENUM('percentage_discount','fixed_discount','free_product','free_shipping') NOT NULL DEFAULT 'percentage_discount',
	`reward_value` DOUBLE NOT NULL DEFAULT '0',
	`reward_product_id` VARCHAR(20) NOT NULL DEFAULT '',
	`reward_max_amount` DOUBLE NOT NULL DEFAULT '0',
	`usage_limit` INT NOT NULL DEFAULT '0',
	`usage_count` INT NOT NULL DEFAULT '0',
	`per_customer_limit` INT NOT NULL DEFAULT '0',
	`stackable` TINYINT(1) NOT NULL DEFAULT '0',
	`priority` INT NOT NULL DEFAULT '10',
	`inactive` TINYINT(1) NOT NULL DEFAULT '0',
	`custom_data` LONGTEXT DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_status` (`status`),
	KEY `idx_type` (`program_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_discount_programs` --

-- Structure of table `0_sales_coupons` -- (Phase 4)

CREATE TABLE IF NOT EXISTS `0_sales_coupons` (
	`id` INT NOT NULL AUTO_INCREMENT,
	`code` VARCHAR(30) NOT NULL,
	`program_id` INT NOT NULL,
	`debtor_no` INT NOT NULL DEFAULT '0',
	`valid_from` DATE DEFAULT NULL,
	`valid_until` DATE DEFAULT NULL,
	`usage_limit` INT NOT NULL DEFAULT '1',
	`usage_count` INT NOT NULL DEFAULT '0',
	`is_active` TINYINT(1) NOT NULL DEFAULT '1',
	`custom_data` LONGTEXT DEFAULT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `idx_code` (`code`),
	KEY `idx_program` (`program_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_coupons` --

-- Structure of table `0_sales_discount_usage` -- (Phase 4)

CREATE TABLE IF NOT EXISTS `0_sales_discount_usage` (
	`id` INT NOT NULL AUTO_INCREMENT,
	`program_id` INT NOT NULL,
	`coupon_id` INT NOT NULL DEFAULT '0',
	`debtor_no` INT NOT NULL,
	`trans_type` SMALLINT NOT NULL,
	`trans_no` INT NOT NULL,
	`discount_amount` DOUBLE NOT NULL DEFAULT '0',
	`applied_date` DATETIME DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_program` (`program_id`),
	KEY `idx_customer` (`debtor_no`),
	KEY `idx_trans` (`trans_type`,`trans_no`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_discount_usage` --

-- Structure of table `0_sales_rma_reasons` -- (Phase 5)

CREATE TABLE IF NOT EXISTS `0_sales_rma_reasons` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `description` VARCHAR(100) NOT NULL,
  `requires_inspection` TINYINT(1) NOT NULL DEFAULT 0,
  `default_disposition` VARCHAR(20) NOT NULL DEFAULT 'restock',
  `inactive` TINYINT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_rma_reasons` --

INSERT IGNORE INTO `0_sales_rma_reasons` (`description`, `requires_inspection`, `default_disposition`) VALUES
('Defective / Faulty Product', 1, 'repair'),
('Wrong Item Delivered', 0, 'restock'),
('Damaged in Transit', 1, 'scrap'),
('Customer Changed Mind', 0, 'restock'),
('Product Not as Described', 0, 'restock'),
('Quality Below Standard', 1, 'quarantine'),
('Excess Quantity', 0, 'restock'),
('Other', 0, 'restock');

-- Structure of table `0_sales_rma` -- (Phase 5)

CREATE TABLE IF NOT EXISTS `0_sales_rma` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `reference` VARCHAR(60) NOT NULL DEFAULT '',
  `debtor_no` INT NOT NULL,
  `branch_code` INT NOT NULL DEFAULT 0,
  `request_date` DATE NOT NULL,
  `status` VARCHAR(20) NOT NULL DEFAULT 'pending',
  `source_type` SMALLINT NOT NULL DEFAULT 0,
  `source_no` INT NOT NULL DEFAULT 0,
  `return_reason_id` INT NOT NULL DEFAULT 0,
  `return_method` VARCHAR(20) NOT NULL DEFAULT 'credit_note',
  `customer_notes` TEXT DEFAULT NULL,
  `internal_notes` TEXT DEFAULT NULL,
  `authorized_by` INT NOT NULL DEFAULT 0,
  `authorized_date` DATETIME DEFAULT NULL,
  `rejected_by` INT NOT NULL DEFAULT 0,
  `rejected_date` DATETIME DEFAULT NULL,
  `rejection_reason` VARCHAR(255) DEFAULT NULL,
  `total_amount` DOUBLE NOT NULL DEFAULT 0,
  `refund_amount` DOUBLE NOT NULL DEFAULT 0,
  `restocking_fee_percent` DOUBLE NOT NULL DEFAULT 0,
  `restocking_fee_amount` DOUBLE NOT NULL DEFAULT 0,
  `wh_return_order_id` INT NOT NULL DEFAULT 0,
  `credit_note_no` INT NOT NULL DEFAULT 0,
  `replacement_order_no` INT NOT NULL DEFAULT 0,
  `created_by` INT NOT NULL DEFAULT 0,
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `custom_data` TEXT DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_debtor` (`debtor_no`),
  KEY `idx_status` (`status`),
  KEY `idx_source` (`source_type`, `source_no`),
  KEY `idx_wh_return` (`wh_return_order_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_rma` --

-- Structure of table `0_sales_rma_lines` -- (Phase 5)

CREATE TABLE IF NOT EXISTS `0_sales_rma_lines` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `rma_id` INT NOT NULL,
  `stock_id` VARCHAR(20) NOT NULL,
  `description` VARCHAR(200) DEFAULT NULL,
  `quantity_requested` DOUBLE NOT NULL DEFAULT 0,
  `quantity_authorized` DOUBLE NOT NULL DEFAULT 0,
  `unit_price` DOUBLE NOT NULL DEFAULT 0,
  `return_condition` VARCHAR(20) NOT NULL DEFAULT 'good',
  `serial_number` VARCHAR(50) NOT NULL DEFAULT '',
  `batch_number` VARCHAR(50) NOT NULL DEFAULT '',
  `notes` VARCHAR(255) DEFAULT NULL,
  `custom_data` TEXT DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_rma` (`rma_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Data of table `0_sales_rma_lines` --

-- Structure of table `0_sales_commission_plans` -- (Phase 6)
CREATE TABLE IF NOT EXISTS `0_sales_commission_plans` (
	`id` INT AUTO_INCREMENT PRIMARY KEY,
	`name` VARCHAR(100) NOT NULL,
	`plan_type` ENUM('percentage','tiered','target_based','achievement') DEFAULT 'percentage',
	`calculation_base` ENUM('revenue','margin','quantity') DEFAULT 'revenue',
	`period_type` ENUM('per_transaction','monthly','quarterly','yearly') DEFAULT 'per_transaction',
	`status` ENUM('draft','active','expired') DEFAULT 'draft',
	`date_start` DATE DEFAULT NULL,
	`date_end` DATE DEFAULT NULL,
	`inactive` TINYINT(1) DEFAULT 0,
	`custom_data` LONGTEXT DEFAULT NULL COMMENT 'JSON',
	INDEX `idx_status` (`status`),
	INDEX `idx_inactive` (`inactive`)
) ENGINE=InnoDB;

-- Data of table `0_sales_commission_plans` --

-- Structure of table `0_sales_commission_tiers` -- (Phase 6)
CREATE TABLE IF NOT EXISTS `0_sales_commission_tiers` (
	`id` INT AUTO_INCREMENT PRIMARY KEY,
	`plan_id` INT NOT NULL,
	`threshold_from` DOUBLE DEFAULT 0,
	`threshold_to` DOUBLE DEFAULT 0,
	`commission_rate` DOUBLE DEFAULT 0,
	`fixed_bonus` DOUBLE DEFAULT 0,
	`sort_order` INT DEFAULT 0,
	KEY `idx_plan` (`plan_id`)
) ENGINE=InnoDB;

-- Data of table `0_sales_commission_tiers` --

-- Structure of table `0_sales_commission_assignments` -- (Phase 6)
CREATE TABLE IF NOT EXISTS `0_sales_commission_assignments` (
	`id` INT AUTO_INCREMENT PRIMARY KEY,
	`plan_id` INT NOT NULL,
	`salesman_id` INT NOT NULL,
	`date_start` DATE DEFAULT NULL,
	`date_end` DATE DEFAULT NULL,
	`target_amount` DOUBLE DEFAULT 0,
	KEY `idx_salesman` (`salesman_id`),
	KEY `idx_plan` (`plan_id`)
) ENGINE=InnoDB;

-- Data of table `0_sales_commission_assignments` --

-- Structure of table `0_sales_commission_entries` -- (Phase 6)
CREATE TABLE IF NOT EXISTS `0_sales_commission_entries` (
	`id` INT AUTO_INCREMENT PRIMARY KEY,
	`salesman_id` INT NOT NULL,
	`plan_id` INT DEFAULT 0,
	`trans_type` SMALLINT NOT NULL,
	`trans_no` INT NOT NULL,
	`debtor_no` INT NOT NULL,
	`trans_date` DATE NOT NULL,
	`base_amount` DOUBLE DEFAULT 0,
	`commission_rate` DOUBLE DEFAULT 0,
	`commission_amount` DOUBLE DEFAULT 0,
	`status` ENUM('calculated','approved','paid') DEFAULT 'calculated',
	`payment_date` DATE DEFAULT NULL,
	`payment_reference` VARCHAR(60) DEFAULT '',
	`custom_data` LONGTEXT DEFAULT NULL COMMENT 'JSON',
	KEY `idx_salesman` (`salesman_id`),
	KEY `idx_date` (`trans_date`),
	KEY `idx_status` (`status`),
	UNIQUE KEY `idx_trans` (`trans_type`, `trans_no`, `salesman_id`)
) ENGINE=InnoDB;

-- Data of table `0_sales_commission_entries` --

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
	VALUES ('use_advanced_commissions', 'setup.company', 'tinyint', 1, '0');

-- Structure of table `0_sales_pos` --

DROP TABLE IF EXISTS `0_sales_pos`;

CREATE TABLE `0_sales_pos` (
	`id` smallint(6) unsigned NOT NULL AUTO_INCREMENT,
	`pos_name` varchar(30) NOT NULL,
	`cash_sale` tinyint(1) NOT NULL,
	`credit_sale` tinyint(1) NOT NULL,
	`pos_location` varchar(5) NOT NULL,
	`pos_account` smallint(6) unsigned NOT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `pos_name` (`pos_name`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_sales_pos` --

INSERT INTO `0_sales_pos` VALUES
('1', 'Default', '1', '1', 'DEF', '2', '0', '{}');

-- Structure of table `0_sales_types` --

DROP TABLE IF EXISTS `0_sales_types`;

CREATE TABLE `0_sales_types` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`sales_type` char(50) NOT NULL DEFAULT '',
	`tax_included` int(1) NOT NULL DEFAULT '0',
	`factor` double NOT NULL DEFAULT '1',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `sales_type` (`sales_type`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_sales_types` --

INSERT INTO `0_sales_types` VALUES
('1', 'Retail', '1', '1', '0', '{}'),
('2', 'Wholesale', '0', '0.7', '0', '{}');

-- Structure of table `0_salesman` --

DROP TABLE IF EXISTS `0_salesman`;

CREATE TABLE `0_salesman` (
	`salesman_code` int(11) NOT NULL AUTO_INCREMENT,
	`salesman_name` char(60) NOT NULL DEFAULT '',
	`salesman_phone` char(30) NOT NULL DEFAULT '',
	`salesman_fax` char(30) NOT NULL DEFAULT '',
	`salesman_email` varchar(100) NOT NULL DEFAULT '',
	`provision` double NOT NULL DEFAULT '0',
	`break_pt` double NOT NULL DEFAULT '0',
	`provision2` double NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`salesman_code`),
	UNIQUE KEY `salesman_name` (`salesman_name`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_salesman` --

INSERT INTO `0_salesman` VALUES
('1', 'Sales Person', '', '', '', '5', '1000', '4', '0', '{}');

-- Structure of table `0_security_roles` --

DROP TABLE IF EXISTS `0_security_roles`;

CREATE TABLE `0_security_roles` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`role` varchar(30) NOT NULL,
	`description` varchar(50) DEFAULT NULL,
	`sections` text,
	`areas` text,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `role` (`role`)
) ENGINE=InnoDB AUTO_INCREMENT=11 ;

-- Data of table `0_security_roles` --

INSERT INTO `0_security_roles` VALUES
('1', 'Inquiries', 'Inquiries', '768;2816;3072;3328;5632;5888;8192;8448;10752;11008;13312;15872;16128', '257;258;259;260;513;514;515;516;517;518;519;520;521;522;523;524;525;773;774;2822;3073;3075;3076;3077;3329;3330;3331;3332;3333;3334;3335;5377;5633;5640;5889;5890;5891;7937;7938;7939;7940;8193;8194;8450;8451;10497;10753;11009;11010;11012;13313;13315;15617;15618;15619;15620;15621;15622;15623;15624;15625;15626;15873;15882;16129;16130;16131;16132;775', '0'),
('2', 'System Administrator', 'System Administrator', '256;512;768;2816;3072;3328;5376;5632;5888;7936;8192;8448;9472;9728;10496;10752;11008;13056;13312;15616;15872;16128', '257;258;259;260;513;514;515;516;517;518;519;520;521;522;523;524;525;526;769;770;771;772;773;774;2817;2818;2819;2820;2821;2822;2823;3073;3074;3082;3075;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5634;5635;5636;5637;5641;5638;5639;5640;5889;5890;5891;7937;7938;7939;7940;7941;7942;7943;7944;7945;7946;7947;7948;7949;7950;7951;7952;7953;7954;7955;7956;7957;7958;7959;7960;7961;7962;7963;7964;7965;7966;7967;7968;7969;8193;8194;8195;8196;8197;8449;8450;8451;8452;8453;8454;8455;8456;8457;8458;9217;9218;9220;9473;9474;9475;9476;9729;10497;10753;10754;10755;10756;10757;11009;11010;11011;11012;13057;13313;13314;13315;15617;15618;15619;15620;15621;15622;15623;15624;15628;15625;15626;15627;15873;15874;15875;15876;15877;15878;15879;15880;15883;15881;15882;16129;16130;16131;16132;775', '0'),
('3', 'Salesman', 'Salesman', '768;3072;5632;8192;15872', '773;774;3073;3075;3081;5633;8194;15873;775', '0'),
('4', 'Stock Manager', 'Stock Manager', '768;2816;3072;3328;5632;5888;7936;8192;8448;10752;11008;13312;15872;16128', '2818;2822;3073;3076;3077;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5633;5640;5889;5890;5891;7941;7943;7944;7945;7946;7947;7948;7949;7950;7951;7952;7953;7954;7955;7956;7957;7958;7959;7960;7961;7962;7963;7964;7965;7966;7967;7968;7969;8193;8194;8450;8451;8452;8453;8454;8455;8456;8457;8458;10753;11009;11010;11012;13313;13315;15882;16129;16130;16131;16132;775', '0'),
('5', 'Production Manager', 'Production Manager', '512;768;2816;3072;3328;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '521;523;524;2818;2819;2820;2821;2822;2823;3073;3074;3076;3077;3078;3079;3080;3081;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5633;5640;5640;5889;5890;5891;8193;8194;8196;8197;8450;8451;10753;10755;11009;11010;11012;13313;13315;15617;15619;15620;15621;15624;15624;15876;15877;15880;15882;16129;16130;16131;16132;775', '0'),
('6', 'Purchase Officer', 'Purchase Officer', '512;768;2816;3072;3328;5376;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '521;523;524;2818;2819;2820;2821;2822;2823;3073;3074;3076;3077;3078;3079;3080;3081;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5377;5633;5635;5640;5640;5889;5890;5891;8193;8194;8196;8197;8449;8450;8451;10753;10755;11009;11010;11012;13313;13315;15617;15619;15620;15621;15624;15624;15876;15877;15880;15882;16129;16130;16131;16132;775', '0'),
('7', 'AR Officer', 'AR Officer', '512;768;2816;3072;3328;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '521;523;524;771;773;774;2818;2819;2820;2821;2822;2823;3073;3073;3074;3075;3076;3077;3078;3079;3080;3081;3081;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5633;5633;5634;5637;5638;5639;5640;5640;5889;5890;5891;8193;8194;8194;8196;8197;8450;8451;10753;10755;11009;11010;11012;13313;13315;15617;15619;15620;15621;15624;15624;15873;15876;15877;15878;15880;15882;16129;16130;16131;16132;775', '0'),
('8', 'AP Officer', 'AP Officer', '512;768;2816;3072;3328;5376;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '257;258;259;260;521;523;524;769;770;771;772;773;774;2818;2819;2820;2821;2822;2823;3073;3074;3082;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5635;5640;5889;5890;5891;7937;7938;7939;7940;8193;8194;8196;8197;8449;8450;8451;10497;10753;10755;11009;11010;11012;13057;13313;13315;15617;15619;15620;15621;15624;15876;15877;15880;15882;16129;16130;16131;16132;775', '0'),
('9', 'Accountant', 'New Accountant', '512;768;2816;3072;3328;5376;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '257;258;259;260;521;523;524;771;772;773;774;2818;2819;2820;2821;2822;2823;3073;3074;3075;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5634;5635;5637;5638;5639;5640;5889;5890;5891;7937;7938;7939;7940;8193;8194;8196;8197;8449;8450;8451;10497;10753;10755;11009;11010;11012;13313;13315;15617;15618;15619;15620;15621;15624;15873;15876;15877;15878;15880;15882;16129;16130;16131;16132;775', '0'),
('10', 'Sub Admin', 'Sub Admin', '512;768;2816;3072;3328;5376;5632;5888;7936;8192;8448;10752;11008;13312;15616;15872;16128', '257;258;259;260;521;523;524;771;772;773;774;2818;2819;2820;2821;2822;2823;3073;3074;3082;3075;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5634;5635;5637;5638;5639;5640;5889;5890;5891;7937;7938;7939;7940;7941;7942;7943;7944;7945;7946;7947;7948;7949;7950;7951;7952;7953;7954;7955;7956;7957;7958;7959;7960;7961;7962;7963;7964;7965;7966;7967;7968;7969;8193;8194;8196;8197;8449;8450;8451;8452;8453;8454;8455;8456;8457;8458;10497;10753;10755;11009;11010;11012;13057;13313;13315;15617;15619;15620;15621;15624;15873;15874;15876;15877;15878;15879;15880;15882;16129;16130;16131;16132;775', '0');

-- Structure of table `0_shippers` --

DROP TABLE IF EXISTS `0_shippers`;

CREATE TABLE `0_shippers` (
	`shipper_id` int(11) NOT NULL AUTO_INCREMENT,
	`shipper_name` varchar(60) NOT NULL DEFAULT '',
	`phone` varchar(30) NOT NULL DEFAULT '',
	`phone2` varchar(30) NOT NULL DEFAULT '',
	`contact` tinytext NOT NULL,
	`address` tinytext NOT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`shipper_id`),
	UNIQUE KEY `name` (`shipper_name`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_shippers` --

INSERT INTO `0_shippers` VALUES
('1', 'Default', '', '', '', '', '0');

-- Structure of table `0_sql_trail` --

DROP TABLE IF EXISTS `0_sql_trail`;

CREATE TABLE `0_sql_trail` (
	`id` int(11) unsigned NOT NULL AUTO_INCREMENT,
	`sql` text NOT NULL,
	`result` tinyint(1) NOT NULL,
	`msg` varchar(255) NOT NULL,
	PRIMARY KEY (`id`)
) ENGINE=InnoDB ;

-- Data of table `0_sql_trail` --


-- Structure of table `0_stock_category` --

DROP TABLE IF EXISTS `0_stock_category`;

CREATE TABLE `0_stock_category` (
	`category_id` int(11) NOT NULL AUTO_INCREMENT,
	`description` varchar(60) NOT NULL DEFAULT '',
	`dflt_tax_type` int(11) NOT NULL DEFAULT '1',
	`dflt_units` varchar(20) NOT NULL DEFAULT 'each',
	`dflt_mb_flag` char(1) NOT NULL DEFAULT 'B',
	`dflt_sales_act` varchar(15) NOT NULL DEFAULT '',
	`dflt_cogs_act` varchar(15) NOT NULL DEFAULT '',
	`dflt_inventory_act` varchar(15) NOT NULL DEFAULT '',
	`dflt_adjustment_act` varchar(15) NOT NULL DEFAULT '',
	`dflt_wip_act` varchar(15) NOT NULL DEFAULT '',
	`dflt_dim1` int(11) DEFAULT NULL,
	`dflt_dim2` int(11) DEFAULT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`dflt_no_sale` tinyint(1) NOT NULL DEFAULT '0',
	`dflt_no_purchase` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`category_id`),
	UNIQUE KEY `description` (`description`)
) ENGINE=InnoDB AUTO_INCREMENT=5 ;

-- Data of table `0_stock_category` --

INSERT INTO `0_stock_category` VALUES
('1', 'Components', '1', 'each', 'B', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '0', '0', '{}'),
('2', 'Charges', '1', 'each', 'D', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '0', '0', '{}'),
('3', 'Systems', '1', 'each', 'M', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '0', '0', '{}'),
('4', 'Services', '1', 'hr', 'D', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '0', '0', '{}');

-- Structure of table `0_stock_fa_class` --

DROP TABLE IF EXISTS `0_stock_fa_class`;

CREATE TABLE `0_stock_fa_class` (
	`fa_class_id` varchar(20) NOT NULL DEFAULT '',
	`parent_id` varchar(20) NOT NULL DEFAULT '',
	`description` varchar(200) NOT NULL DEFAULT '',
	`long_description` tinytext NOT NULL,
	`depreciation_rate` double NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`fa_class_id`)
) ENGINE=InnoDB ;

-- Data of table `0_stock_fa_class` --

-- Structure of table `0_stock_master` --

DROP TABLE IF EXISTS `0_stock_master`;

CREATE TABLE `0_stock_master` (
	`stock_id` varchar(20) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '',
	`category_id` int(11) NOT NULL DEFAULT '0',
	`tax_type_id` int(11) NOT NULL DEFAULT '0',
	`description` varchar(200) NOT NULL DEFAULT '',
	`long_description` tinytext NOT NULL,
	`units` varchar(20) NOT NULL DEFAULT 'each',
	`mb_flag` char(1) NOT NULL DEFAULT 'B',
	`sales_account` varchar(15) NOT NULL DEFAULT '',
	`cogs_account` varchar(15) NOT NULL DEFAULT '',
	`inventory_account` varchar(15) NOT NULL DEFAULT '',
	`adjustment_account` varchar(15) NOT NULL DEFAULT '',
	`wip_account` varchar(15) NOT NULL DEFAULT '',
	`dimension_id` int(11) DEFAULT NULL,
	`dimension2_id` int(11) DEFAULT NULL,
	`purchase_cost` double NOT NULL DEFAULT '0',
	`material_cost` double NOT NULL DEFAULT '0',
	`labour_cost` double NOT NULL DEFAULT '0',
	`overhead_cost` double NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`no_sale` tinyint(1) NOT NULL DEFAULT '0',
	`no_purchase` tinyint(1) NOT NULL DEFAULT '0',
	`editable` tinyint(1) NOT NULL DEFAULT '0',
	`depreciation_method` char(1) NOT NULL DEFAULT 'S',
	`depreciation_rate` double NOT NULL DEFAULT '0',
	`depreciation_factor` double NOT NULL DEFAULT '1',
	`depreciation_start` date NOT NULL DEFAULT '0000-00-00',
	`depreciation_date` date NOT NULL DEFAULT '0000-00-00',
	`fa_class_id` varchar(20) NOT NULL DEFAULT '',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`track_by` varchar(15) NOT NULL DEFAULT 'none' COMMENT 'none|serial|batch|serial_batch',
	`has_serial_no` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Enable serial number tracking',
	`has_batch_no` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Enable batch/lot tracking',
	`has_expiry_date` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Enable expiry date tracking',
	`serial_no_prefix` varchar(20) DEFAULT NULL COMMENT 'Auto-generate serial prefix',
	`batch_no_prefix` varchar(20) DEFAULT NULL COMMENT 'Auto-generate batch prefix',
	`warranty_days` int(11) DEFAULT NULL COMMENT 'Default warranty period in days',
	`quality_inspection_required` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Require quality inspection on receiving',
	`min_order_qty` double DEFAULT NULL COMMENT 'Minimum order quantity',
	`lead_time_days` int(11) DEFAULT NULL COMMENT 'Supplier lead time in days',
	`shelf_life_days` int(11) DEFAULT NULL COMMENT 'Default shelf life in days',
	`item_weight` decimal(15,4) DEFAULT NULL COMMENT 'Unit weight in kg',
	`item_volume` decimal(15,4) DEFAULT NULL COMMENT 'Unit volume in m3',
	`putaway_rule_id` int(11) DEFAULT NULL COMMENT 'FK to wh_putaway_rules',
	`removal_strategy` varchar(20) DEFAULT NULL COMMENT 'Item-specific removal override: fifo|fefo|lifo|closest|least_packages',
	`default_bin_id` int(11) DEFAULT NULL COMMENT 'FK to wh_locations.loc_id',
	`storage_category_id` int(11) DEFAULT NULL COMMENT 'FK to wh_storage_categories.id',
	`abc_class` char(1) DEFAULT NULL COMMENT 'A/B/C velocity class',
	`drop_ship_eligible` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Item can be drop-shipped from supplier to customer',
	`cross_dock_eligible` tinyint(1) NOT NULL DEFAULT '0' COMMENT 'Item eligible for cross-docking',
	PRIMARY KEY (`stock_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- Data of table `0_stock_master` --

INSERT INTO `0_stock_master` VALUES
('101', '1', '1', 'Vsmart Aris 6GB-64GB', '', 'each', 'B', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '200', '0', '0', '0', '0', '0', '0', 'S', '0', '1', '0000-00-00', '0000-00-00', '', '{}'),
('102', '1', '1', 'Vsmart Live 4 (6GB/64GB)', '', 'each', 'B', '4010', '5010', '1510', '5040', '1530', '0', '0', '150', '150', '0', '0', '0', '0', '0', '0', 'S', '0', '1', '0000-00-00', '0000-00-00', '', '{}'),
('103', '1', '1', 'Vsmart Live 4 Cover Case', '', 'each', 'B', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '10', '0', '0', '0', '0', '0', '0', 'S', '0', '1', '0000-00-00', '0000-00-00', '', '{}'),
('201', '3', '1', 'AP Surf Set', '', 'each', 'M', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '360', '0', '0', '0', '0', '0', '0', 'S', '0', '1', '0000-00-00', '0000-00-00', '', '{}'),
('202', '4', '1', 'Maintenance', '', 'hr', 'D', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1', 'S', '0', '1', '0000-00-00', '0000-00-00', '', '{}'),
('301', '4', '1', 'Support', '', 'hr', 'D', '4010', '5010', '1510', '5040', '1530', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', 'S', '0', '1', '0000-00-00', '0000-00-00', '', '{}');

-- Structure of table `0_stock_moves` --

DROP TABLE IF EXISTS `0_stock_moves`;

CREATE TABLE `0_stock_moves` (
	`trans_id` int(11) NOT NULL AUTO_INCREMENT,
	`trans_no` int(11) NOT NULL DEFAULT '0',
	`stock_id` char(20) NOT NULL DEFAULT '',
	`type` smallint(6) NOT NULL DEFAULT '0',
	`loc_code` char(5) NOT NULL DEFAULT '',
	`tran_date` date NOT NULL DEFAULT '0000-00-00',
	`price` double NOT NULL DEFAULT '0',
	`reference` char(40) NOT NULL DEFAULT '',
	`qty` double NOT NULL DEFAULT '1',
	`standard_cost` double NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`expiry_date` date DEFAULT NULL COMMENT 'Expiry date at time of movement',
	`quality_status` varchar(10) DEFAULT 'none' COMMENT 'none|pending|pass|fail',
	`from_bin_id` int(11) DEFAULT NULL COMMENT 'FK to wh_locations.loc_id (source bin)',
	`to_bin_id` int(11) DEFAULT NULL COMMENT 'FK to wh_locations.loc_id (dest bin)',
	`wh_operation_id` int(11) DEFAULT NULL COMMENT 'FK to wh_operations.op_id',
	`package_id` int(11) DEFAULT NULL COMMENT 'FK to wh_packages.package_id',
	`stock_status` varchar(20) DEFAULT 'available' COMMENT 'available|reserved|on_hold|quarantine|in_transit',
	PRIMARY KEY (`trans_id`),
	KEY `type` (`type`,`trans_no`),
	KEY `Move` (`stock_id`,`loc_code`,`tran_date`),
	KEY `idx_sm_serial_id` (`serial_id`),
	KEY `idx_sm_batch_id` (`batch_id`),
	KEY `idx_sm_from_bin_id` (`from_bin_id`),
	KEY `idx_sm_to_bin_id` (`to_bin_id`)
) ENGINE=InnoDB AUTO_INCREMENT=18 ;

-- Data of table `0_stock_moves` --

INSERT INTO `0_stock_moves` VALUES
('1', '1', '101', '25', 'DEF', '2025-05-05', '200', '', '100', '200', '{}'),
('2', '1', '102', '25', 'DEF', '2025-05-05', '150', '', '100', '150', '{}'),
('3', '1', '103', '25', 'DEF', '2025-05-05', '10', '', '100', '10', '{}'),
('4', '1', '101', '13', 'DEF', '2025-05-10', '300', 'auto', '-20', '200', '{}'),
('5', '1', '301', '13', 'DEF', '2025-05-10', '80', 'auto', '-3', '0', '{}'),
('6', '1', '101', '29', 'DEF', '2025-05-05', '200', '001/2025', '-2', '200', '{}'),
('7', '1', '102', '29', 'DEF', '2025-05-05', '150', '001/2025', '-2', '150', '{}'),
('8', '1', '103', '29', 'DEF', '2025-05-05', '10', '001/2025', '-2', '10', '{}'),
('9', '1', '301', '29', 'DEF', '2025-05-05', '0', '001/2025', '-2', '0', '{}'),
('10', '1', '201', '26', 'DEF', '2025-05-05', '0', '001/2025', '2', '360', '{}'),
('11', '2', '101', '25', 'DEF', '2025-05-05', '200', '', '15', '200', '{}'),
('12', '2', '101', '13', 'DEF', '2025-05-07', '300', 'auto', '-1', '200', '{}'),
('13', '3', '102', '13', 'DEF', '2025-05-07', '222.62', 'auto', '-1', '150', '{}'),
('14', '3', '103', '13', 'DEF', '2025-05-07', '44.52', 'auto', '-1', '10', '{}'),
('15', '4', '202', '13', 'DEF', '2025-05-07', '0', 'auto', '-5', '0', '{}'),
('16', '5', '102', '13', 'DEF', '2026-01-21', '250', 'auto', '-5', '150', '{}'),
('17', '3', '102', '25', 'DEF', '2026-01-21', '150', '', '6', '150', '{}');

-- =====================================================================================
-- Advanced Inventory / Warehouse Management Tables
-- =====================================================================================

-- Structure of table `0_wh_location_types` --

DROP TABLE IF EXISTS `0_wh_location_types`;

CREATE TABLE `0_wh_location_types` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`type_code` varchar(20) NOT NULL,
	`type_name` varchar(60) NOT NULL,
	`is_physical` tinyint(1) NOT NULL DEFAULT '1',
	`can_store` tinyint(1) NOT NULL DEFAULT '1',
	`sort_order` int(11) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `type_code` (`type_code`)
) ENGINE=InnoDB;

-- Data of table `0_wh_location_types` --

INSERT INTO `0_wh_location_types` (`type_code`, `type_name`, `is_physical`, `can_store`, `sort_order`) VALUES
('warehouse', 'Warehouse', '1', '0', '10'),
('zone', 'Zone', '1', '0', '20'),
('aisle', 'Aisle', '1', '0', '30'),
('rack', 'Rack', '1', '0', '40'),
('shelf', 'Shelf', '1', '0', '50'),
('bin', 'Bin', '1', '1', '60'),
('staging', 'Staging Area', '1', '1', '70'),
('dock', 'Dock Door', '1', '1', '80'),
('quality', 'Quality Check', '1', '1', '90'),
('transit', 'In Transit', '0', '0', '100'),
('virtual', 'Virtual/View', '0', '0', '110'),
('scrap', 'Scrap Location', '1', '1', '120'),
('production', 'Production Floor', '1', '1', '130'),
('packing', 'Packing Station', '1', '1', '140'),
('returns', 'Returns Area', '1', '1', '150');

-- Structure of table `0_wh_storage_categories` --

DROP TABLE IF EXISTS `0_wh_storage_categories`;

CREATE TABLE `0_wh_storage_categories` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`category_name` varchar(60) NOT NULL,
	`max_weight` decimal(15,4) DEFAULT NULL,
	`max_volume` decimal(15,4) DEFAULT NULL,
	`max_units` int(11) DEFAULT NULL,
	`allow_mixed_items` tinyint(1) NOT NULL DEFAULT '1',
	`allow_mixed_lots` tinyint(1) NOT NULL DEFAULT '1',
	`temperature_min` decimal(5,2) DEFAULT NULL,
	`temperature_max` decimal(5,2) DEFAULT NULL,
	`humidity_max` decimal(5,2) DEFAULT NULL,
	`is_hazmat` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_storage_categories` --

-- Structure of table `0_wh_locations` --

DROP TABLE IF EXISTS `0_wh_locations`;

CREATE TABLE `0_wh_locations` (
	`loc_id` int(11) NOT NULL AUTO_INCREMENT,
	`loc_code` varchar(30) NOT NULL,
	`loc_name` varchar(100) NOT NULL,
	`parent_loc_id` int(11) DEFAULT NULL,
	`warehouse_loc_code` varchar(5) NOT NULL COMMENT 'FK to 0_locations.loc_code (root warehouse)',
	`location_type_id` int(11) NOT NULL COMMENT 'FK to 0_wh_location_types',
	`location_path` varchar(500) DEFAULT NULL COMMENT 'Materialized path: /1/5/12/45',
	`full_name` varchar(500) DEFAULT NULL COMMENT 'Warehouse A > Zone 1 > Aisle A > Bin 03',
	`max_weight` decimal(15,4) DEFAULT NULL COMMENT 'Max weight capacity (kg)',
	`max_volume` decimal(15,4) DEFAULT NULL COMMENT 'Max volume capacity (m3)',
	`max_units` int(11) DEFAULT NULL COMMENT 'Max number of units/pallets',
	`current_weight` decimal(15,4) DEFAULT '0.0000',
	`current_volume` decimal(15,4) DEFAULT '0.0000',
	`current_units` int(11) DEFAULT '0',
	`storage_category_id` int(11) DEFAULT NULL COMMENT 'FK to 0_wh_storage_categories',
	`zone_type` varchar(20) DEFAULT NULL COMMENT 'cold|dry|hazmat|outdoor|climate_controlled',
	`abc_class` char(1) DEFAULT NULL COMMENT 'A/B/C classification for slotting',
	`pick_sequence` int(11) DEFAULT '0' COMMENT 'Order for picking path optimization',
	`barcode` varchar(50) DEFAULT NULL COMMENT 'Location barcode for scanning',
	`is_active` tinyint(1) NOT NULL DEFAULT '1',
	`is_default_receipt` tinyint(1) NOT NULL DEFAULT '0',
	`is_default_ship` tinyint(1) NOT NULL DEFAULT '0',
	`is_default_scrap` tinyint(1) NOT NULL DEFAULT '0',
	`is_default_production` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`loc_id`),
	UNIQUE KEY `loc_code` (`loc_code`),
	KEY `parent_loc_id` (`parent_loc_id`),
	KEY `warehouse_loc_code` (`warehouse_loc_code`),
	KEY `location_type_id` (`location_type_id`),
	KEY `storage_category_id` (`storage_category_id`),
	KEY `barcode` (`barcode`),
	KEY `location_path` (`location_path`(191))
) ENGINE=InnoDB;

-- Data of table `0_wh_locations` --

-- Structure of table `0_serial_numbers` --

DROP TABLE IF EXISTS `0_serial_numbers`;

CREATE TABLE `0_serial_numbers` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`serial_no` varchar(100) NOT NULL COMMENT 'The serial number string (IMEI, VIN, etc.)',
	`stock_id` varchar(20) NOT NULL COMMENT 'FK to stock_master.stock_id',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id (if serial belongs to a batch)',
	`status` varchar(20) NOT NULL DEFAULT 'available' COMMENT 'available|reserved|delivered|in_transit|in_production|in_repair|quarantine|recalled|scrapped|returned|expired',
	`loc_code` varchar(5) DEFAULT NULL COMMENT 'Current warehouse location',
	`wh_loc_id` int(11) DEFAULT NULL COMMENT 'FK to wh_locations.loc_id (current bin)',
	`supplier_id` int(11) DEFAULT NULL COMMENT 'Supplier who provided the serial',
	`customer_id` int(11) DEFAULT NULL COMMENT 'Customer who received the serial',
	`purchase_date` date DEFAULT NULL,
	`delivery_date` date DEFAULT NULL,
	`warranty_start` date DEFAULT NULL,
	`warranty_end` date DEFAULT NULL,
	`warranty_type` varchar(50) DEFAULT NULL COMMENT 'standard|extended|limited',
	`manufacturing_date` date DEFAULT NULL,
	`expiry_date` date DEFAULT NULL,
	`grn_id` int(11) DEFAULT NULL COMMENT 'GRN that received this serial',
	`delivery_id` int(11) DEFAULT NULL COMMENT 'Delivery note that shipped this serial',
	`work_order_id` int(11) DEFAULT NULL COMMENT 'Work Order that produced this serial',
	`purchase_cost` double DEFAULT '0' COMMENT 'Actual cost of this specific unit',
	`actual_cost` double DEFAULT NULL COMMENT 'Actual landed cost for this specific serial (overrides standard cost for COGS)',
	`notes` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL COMMENT 'Extensible attributes JSON',
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `uk_serial_no_stock` (`serial_no`, `stock_id`),
	KEY `idx_stock_id` (`stock_id`),
	KEY `idx_batch_id` (`batch_id`),
	KEY `idx_status` (`status`),
	KEY `idx_loc_code` (`loc_code`),
	KEY `idx_wh_loc_id` (`wh_loc_id`),
	KEY `idx_customer_id` (`customer_id`),
	KEY `idx_supplier_id` (`supplier_id`),
	KEY `idx_warranty_end` (`warranty_end`),
	KEY `idx_expiry_date` (`expiry_date`)
) ENGINE=InnoDB;

-- Data of table `0_serial_numbers` --

-- Structure of table `0_stock_batches` --

DROP TABLE IF EXISTS `0_stock_batches`;

CREATE TABLE `0_stock_batches` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`batch_no` varchar(100) NOT NULL COMMENT 'Batch/Lot number string',
	`stock_id` varchar(20) NOT NULL COMMENT 'FK to stock_master.stock_id',
	`status` varchar(20) NOT NULL DEFAULT 'active' COMMENT 'active|quarantine|recalled|expired|consumed|scrapped',
	`manufacturing_date` date DEFAULT NULL,
	`expiry_date` date DEFAULT NULL,
	`best_before_date` date DEFAULT NULL COMMENT 'Best-before date (food)',
	`retest_date` date DEFAULT NULL COMMENT 'Retest date (pharma/chemicals)',
	`shelf_life_days` int(11) DEFAULT NULL,
	`supplier_id` int(11) DEFAULT NULL,
	`supplier_batch_no` varchar(100) DEFAULT NULL COMMENT 'Supplier own batch number',
	`grn_id` int(11) DEFAULT NULL COMMENT 'GRN that first received this batch',
	`work_order_id` int(11) DEFAULT NULL COMMENT 'Work Order that produced this batch',
	`initial_qty` double DEFAULT '0' COMMENT 'Original quantity received/produced',
	`batch_cost` double DEFAULT NULL COMMENT 'Batch-specific cost per unit (overrides standard cost for COGS)',
	`country_of_origin` varchar(60) DEFAULT NULL,
	`certification` varchar(200) DEFAULT NULL COMMENT 'Quality certifications (ISO, GMP)',
	`notes` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL COMMENT 'Extensible attributes JSON',
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `uk_batch_no_stock` (`batch_no`, `stock_id`),
	KEY `idx_stock_id` (`stock_id`),
	KEY `idx_status` (`status`),
	KEY `idx_expiry_date` (`expiry_date`),
	KEY `idx_manufacturing_date` (`manufacturing_date`),
	KEY `idx_supplier_id` (`supplier_id`)
) ENGINE=InnoDB;

-- Data of table `0_stock_batches` --

-- Structure of table `0_serial_movements` --

DROP TABLE IF EXISTS `0_serial_movements`;

CREATE TABLE `0_serial_movements` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`serial_id` int(11) NOT NULL COMMENT 'FK to serial_numbers.id',
	`stock_move_id` int(11) DEFAULT NULL COMMENT 'FK to stock_moves.trans_id',
	`trans_type` smallint(6) NOT NULL COMMENT 'Transaction type (ST_SUPPRECEIVE, etc.)',
	`trans_no` int(11) NOT NULL,
	`from_loc` varchar(5) DEFAULT NULL,
	`to_loc` varchar(5) DEFAULT NULL,
	`from_status` varchar(20) DEFAULT NULL,
	`to_status` varchar(20) DEFAULT NULL,
	`tran_date` date NOT NULL,
	`reference` varchar(60) DEFAULT NULL,
	`notes` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_serial_id` (`serial_id`),
	KEY `idx_trans` (`trans_type`, `trans_no`),
	KEY `idx_stock_move` (`stock_move_id`),
	KEY `idx_tran_date` (`tran_date`)
) ENGINE=InnoDB;

-- Data of table `0_serial_movements` --

-- Structure of table `0_batch_movements` --

DROP TABLE IF EXISTS `0_batch_movements`;

CREATE TABLE `0_batch_movements` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`batch_id` int(11) NOT NULL COMMENT 'FK to stock_batches.id',
	`stock_move_id` int(11) DEFAULT NULL COMMENT 'FK to stock_moves.trans_id',
	`trans_type` smallint(6) NOT NULL,
	`trans_no` int(11) NOT NULL,
	`loc_code` varchar(5) NOT NULL,
	`wh_loc_id` int(11) DEFAULT NULL COMMENT 'FK to wh_locations.loc_id (bin)',
	`quantity` double NOT NULL COMMENT 'Qty moved (+inbound, -outbound)',
	`tran_date` date NOT NULL,
	`reference` varchar(60) DEFAULT NULL,
	`notes` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_batch_id` (`batch_id`),
	KEY `idx_trans` (`trans_type`, `trans_no`),
	KEY `idx_stock_move` (`stock_move_id`),
	KEY `idx_tran_date` (`tran_date`)
) ENGINE=InnoDB;

-- Data of table `0_batch_movements` --

-- Structure of table `0_quality_parameters` --

DROP TABLE IF EXISTS `0_quality_parameters`;

CREATE TABLE `0_quality_parameters` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`name` varchar(100) NOT NULL COMMENT 'Parameter name (Temperature, Weight, etc.)',
	`description` varchar(200) DEFAULT NULL,
	`parameter_type` varchar(20) NOT NULL DEFAULT 'numeric' COMMENT 'numeric|text|boolean|list',
	`unit` varchar(20) DEFAULT NULL COMMENT 'Measurement unit',
	`min_value` double DEFAULT NULL,
	`max_value` double DEFAULT NULL,
	`acceptable_values` text DEFAULT NULL COMMENT 'JSON array for list type',
	`stock_id` varchar(20) DEFAULT NULL COMMENT 'NULL = global, set = item-specific',
	`category_id` int(11) DEFAULT NULL COMMENT 'NULL = all categories, set = category-specific',
	`mandatory` tinyint(1) NOT NULL DEFAULT '0',
	`sort_order` int(11) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	KEY `idx_stock_id` (`stock_id`),
	KEY `idx_category_id` (`category_id`)
) ENGINE=InnoDB;

-- Data of table `0_quality_parameters` --

-- Structure of table `0_quality_inspections` --

DROP TABLE IF EXISTS `0_quality_inspections`;

CREATE TABLE `0_quality_inspections` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`reference` varchar(60) NOT NULL COMMENT 'Inspection document number',
	`stock_id` varchar(20) NOT NULL,
	`inspection_type` varchar(20) NOT NULL COMMENT 'incoming|outgoing|in_process|periodic',
	`trans_type` smallint(6) DEFAULT NULL,
	`trans_no` int(11) DEFAULT NULL,
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`loc_code` varchar(5) DEFAULT NULL,
	`inspected_qty` double DEFAULT '0',
	`accepted_qty` double DEFAULT '0',
	`rejected_qty` double DEFAULT '0',
	`result` varchar(20) NOT NULL DEFAULT 'pending' COMMENT 'pending|pass|fail|partial',
	`inspector_id` int(11) DEFAULT NULL COMMENT 'FK to users',
	`inspection_date` date NOT NULL,
	`completion_date` date DEFAULT NULL,
	`notes` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL COMMENT 'Flexible inspection parameters JSON',
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_stock_id` (`stock_id`),
	KEY `idx_trans` (`trans_type`, `trans_no`),
	KEY `idx_batch_id` (`batch_id`),
	KEY `idx_serial_id` (`serial_id`),
	KEY `idx_result` (`result`)
) ENGINE=InnoDB;

-- Data of table `0_quality_inspections` --

-- Structure of table `0_quality_inspection_readings` --

DROP TABLE IF EXISTS `0_quality_inspection_readings`;

CREATE TABLE `0_quality_inspection_readings` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`inspection_id` int(11) NOT NULL COMMENT 'FK to quality_inspections.id',
	`parameter_id` int(11) NOT NULL COMMENT 'FK to quality_parameters.id',
	`reading_value` varchar(200) DEFAULT NULL COMMENT 'Actual reading',
	`result` varchar(10) DEFAULT NULL COMMENT 'pass|fail',
	`notes` text DEFAULT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `uk_insp_param` (`inspection_id`, `parameter_id`),
	KEY `idx_parameter_id` (`parameter_id`)
) ENGINE=InnoDB;

-- Data of table `0_quality_inspection_readings` --

-- Structure of table `0_warranty_claims` --

DROP TABLE IF EXISTS `0_warranty_claims`;

CREATE TABLE `0_warranty_claims` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`reference` varchar(60) NOT NULL COMMENT 'Claim document number',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`stock_id` varchar(20) NOT NULL,
	`customer_id` int(11) NOT NULL COMMENT 'FK to debtors_master',
	`claim_date` date NOT NULL,
	`warranty_valid` tinyint(1) DEFAULT '1' COMMENT 'Is within warranty period?',
	`status` varchar(20) NOT NULL DEFAULT 'open' COMMENT 'open|acknowledged|in_repair|replaced|resolved|rejected|closed',
	`issue_type` varchar(20) DEFAULT 'defective' COMMENT 'defective|damaged|malfunction|missing_parts|other',
	`issue_description` text NOT NULL,
	`resolution_description` text DEFAULT NULL,
	`resolution_date` date DEFAULT NULL,
	`replacement_serial_id` int(11) DEFAULT NULL COMMENT 'New serial if replaced',
	`repair_cost` double DEFAULT '0',
	`is_chargeable` tinyint(1) DEFAULT '0' COMMENT 'Bill customer?',
	`assigned_to` int(11) DEFAULT NULL COMMENT 'Assigned technician/user',
	`crm_lead_id` int(11) DEFAULT NULL COMMENT 'Link to CRM (support ticket)',
	`notes` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_serial_id` (`serial_id`),
	KEY `idx_customer_id` (`customer_id`),
	KEY `idx_stock_id` (`stock_id`),
	KEY `idx_status` (`status`),
	KEY `idx_claim_date` (`claim_date`)
) ENGINE=InnoDB;

-- Data of table `0_warranty_claims` --

-- Structure of table `0_warranty_claim_parts` --

DROP TABLE IF EXISTS `0_warranty_claim_parts`;

CREATE TABLE `0_warranty_claim_parts` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`claim_id` int(11) NOT NULL COMMENT 'FK to warranty_claims.id',
	`stock_id` varchar(20) NOT NULL COMMENT 'Part item code',
	`serial_id` int(11) DEFAULT NULL COMMENT 'Part serial (if serialized)',
	`quantity` double NOT NULL DEFAULT '1',
	`unit_cost` double DEFAULT '0',
	PRIMARY KEY (`id`),
	KEY `idx_claim_id` (`claim_id`),
	KEY `idx_stock_id` (`stock_id`)
) ENGINE=InnoDB;

-- Data of table `0_warranty_claim_parts` --

-- Structure of table `0_recall_campaigns` --

DROP TABLE IF EXISTS `0_recall_campaigns`;

CREATE TABLE `0_recall_campaigns` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`reference` varchar(60) NOT NULL COMMENT 'Recall campaign reference',
	`title` varchar(200) NOT NULL,
	`description` text NOT NULL,
	`stock_id` varchar(20) NOT NULL COMMENT 'Affected item',
	`recall_type` varchar(30) NOT NULL COMMENT 'voluntary|mandatory|market_withdrawal',
	`severity` varchar(10) NOT NULL DEFAULT 'medium' COMMENT 'critical|high|medium|low',
	`status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft|active|in_progress|completed|cancelled',
	`start_date` date NOT NULL,
	`end_date` date DEFAULT NULL,
	`affected_batch_ids` text DEFAULT NULL COMMENT 'Comma-separated batch IDs',
	`affected_serial_from` varchar(100) DEFAULT NULL COMMENT 'Serial range start',
	`affected_serial_to` varchar(100) DEFAULT NULL COMMENT 'Serial range end',
	`affected_date_from` date DEFAULT NULL COMMENT 'Manufacturing date range start',
	`affected_date_to` date DEFAULT NULL,
	`regulatory_reference` varchar(200) DEFAULT NULL COMMENT 'FDA/MHRA etc. reference',
	`total_affected_units` int(11) DEFAULT '0',
	`total_recovered_units` int(11) DEFAULT '0',
	`resolution` varchar(200) DEFAULT NULL COMMENT 'Repair/replace/refund',
	`notes` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	UNIQUE KEY `uk_reference` (`reference`),
	KEY `idx_stock_id` (`stock_id`),
	KEY `idx_status` (`status`)
) ENGINE=InnoDB;

-- Data of table `0_recall_campaigns` --

-- Structure of table `0_recall_items` --

DROP TABLE IF EXISTS `0_recall_items`;

CREATE TABLE `0_recall_items` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`recall_id` int(11) NOT NULL COMMENT 'FK to recall_campaigns.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`customer_id` int(11) DEFAULT NULL COMMENT 'Customer who has the item',
	`status` varchar(20) NOT NULL DEFAULT 'identified' COMMENT 'identified|notified|returned|repaired|replaced|refunded|unreachable',
	`notification_date` date DEFAULT NULL,
	`return_date` date DEFAULT NULL,
	`resolution_date` date DEFAULT NULL,
	`notes` text DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_recall_id` (`recall_id`),
	KEY `idx_serial_id` (`serial_id`),
	KEY `idx_batch_id` (`batch_id`),
	KEY `idx_customer_id` (`customer_id`)
) ENGINE=InnoDB;

-- Data of table `0_recall_items` --

-- Structure of table `0_production_traceability` --

DROP TABLE IF EXISTS `0_production_traceability`;

CREATE TABLE `0_production_traceability` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`work_order_id` int(11) NOT NULL,
	`finished_serial_id` int(11) DEFAULT NULL COMMENT 'Produced serial',
	`finished_batch_id` int(11) DEFAULT NULL COMMENT 'Produced batch',
	`component_stock_id` varchar(20) NOT NULL COMMENT 'Component item',
	`component_serial_id` int(11) DEFAULT NULL COMMENT 'Component serial consumed',
	`component_batch_id` int(11) DEFAULT NULL COMMENT 'Component batch consumed',
	`component_qty` double NOT NULL COMMENT 'Qty consumed',
	`created_at` datetime DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_wo` (`work_order_id`),
	KEY `idx_finished_serial` (`finished_serial_id`),
	KEY `idx_finished_batch` (`finished_batch_id`),
	KEY `idx_component_serial` (`component_serial_id`),
	KEY `idx_component_batch` (`component_batch_id`)
) ENGINE=InnoDB;

-- Data of table `0_production_traceability` --

-- Structure of table `0_wh_operations` --

DROP TABLE IF EXISTS `0_wh_operations`;

CREATE TABLE `0_wh_operations` (
	`op_id` int(11) NOT NULL AUTO_INCREMENT,
	`op_type` varchar(20) NOT NULL COMMENT 'receipt|quality_check|putaway|pick|pack|ship|transfer|adjustment|scrap|return',
	`op_status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft|ready|in_progress|done|cancelled',
	`source_doc_type` int(11) DEFAULT NULL COMMENT 'ST_SUPPRECEIVE, ST_CUSTDELIVERY, etc.',
	`source_doc_no` int(11) DEFAULT NULL,
	`parent_op_id` int(11) DEFAULT NULL COMMENT 'Previous step in multi-step flow',
	`next_op_id` int(11) DEFAULT NULL COMMENT 'Next step (filled when created)',
	`route_id` int(11) DEFAULT NULL COMMENT 'FK to wh_routes',
	`from_loc_id` int(11) DEFAULT NULL COMMENT 'Source bin/location',
	`to_loc_id` int(11) DEFAULT NULL COMMENT 'Destination bin/location',
	`scheduled_date` datetime DEFAULT NULL,
	`started_at` datetime DEFAULT NULL,
	`completed_at` datetime DEFAULT NULL,
	`assigned_to` smallint(6) DEFAULT NULL COMMENT 'FK to 0_users (worker)',
	`priority` tinyint(3) NOT NULL DEFAULT '5' COMMENT '1=highest, 9=lowest',
	`wave_id` int(11) DEFAULT NULL COMMENT 'FK to wh_picking_waves',
	`memo` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`op_id`),
	KEY `op_type` (`op_type`),
	KEY `op_status` (`op_status`),
	KEY `source_doc` (`source_doc_type`, `source_doc_no`),
	KEY `parent_op_id` (`parent_op_id`),
	KEY `wave_id` (`wave_id`),
	KEY `assigned_to` (`assigned_to`),
	KEY `scheduled_date` (`scheduled_date`)
) ENGINE=InnoDB;

-- Data of table `0_wh_operations` --

-- Structure of table `0_wh_operation_lines` --

DROP TABLE IF EXISTS `0_wh_operation_lines`;

CREATE TABLE `0_wh_operation_lines` (
	`line_id` int(11) NOT NULL AUTO_INCREMENT,
	`op_id` int(11) NOT NULL COMMENT 'FK to wh_operations',
	`stock_id` varchar(20) NOT NULL,
	`qty_planned` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`qty_done` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`from_loc_id` int(11) DEFAULT NULL COMMENT 'Source bin (pick from)',
	`to_loc_id` int(11) DEFAULT NULL COMMENT 'Destination bin (put to)',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`package_id` int(11) DEFAULT NULL COMMENT 'FK to wh_packages',
	`unit_cost` decimal(15,4) DEFAULT '0.0000',
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`line_id`),
	KEY `op_id` (`op_id`),
	KEY `stock_id` (`stock_id`),
	KEY `batch_id` (`batch_id`),
	KEY `serial_id` (`serial_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_operation_lines` --

-- Structure of table `0_wh_routes` --

DROP TABLE IF EXISTS `0_wh_routes`;

CREATE TABLE `0_wh_routes` (
	`route_id` int(11) NOT NULL AUTO_INCREMENT,
	`route_name` varchar(100) NOT NULL,
	`route_type` varchar(20) NOT NULL COMMENT 'inbound|outbound|internal|return|cross_dock|drop_ship',
	`warehouse_loc_code` varchar(5) DEFAULT NULL COMMENT 'Apply to specific warehouse, NULL = all',
	`is_default` tinyint(1) NOT NULL DEFAULT '0',
	`sequence` int(11) NOT NULL DEFAULT '10',
	`active` tinyint(1) NOT NULL DEFAULT '1',
	`description` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`route_id`),
	KEY `route_type` (`route_type`)
) ENGINE=InnoDB;

-- Data of table `0_wh_routes` --

-- Structure of table `0_wh_route_rules` --

DROP TABLE IF EXISTS `0_wh_route_rules`;

CREATE TABLE `0_wh_route_rules` (
	`rule_id` int(11) NOT NULL AUTO_INCREMENT,
	`route_id` int(11) NOT NULL COMMENT 'FK to wh_routes',
	`rule_type` varchar(10) NOT NULL COMMENT 'push|pull',
	`sequence` int(11) NOT NULL DEFAULT '10',
	`from_loc_id` int(11) DEFAULT NULL,
	`to_loc_id` int(11) DEFAULT NULL,
	`operation_type` varchar(20) NOT NULL COMMENT 'receipt|quality|putaway|pick|pack|ship|transfer',
	`trigger_method` varchar(20) NOT NULL DEFAULT 'manual' COMMENT 'manual|auto_on_confirm|auto_on_complete',
	`delay_days` int(11) DEFAULT '0',
	`active` tinyint(1) NOT NULL DEFAULT '1',
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`rule_id`),
	KEY `route_id` (`route_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_route_rules` --

-- Structure of table `0_wh_putaway_rules` --

DROP TABLE IF EXISTS `0_wh_putaway_rules`;

CREATE TABLE `0_wh_putaway_rules` (
	`rule_id` int(11) NOT NULL AUTO_INCREMENT,
	`rule_name` varchar(100) NOT NULL,
	`warehouse_loc_code` varchar(5) DEFAULT NULL,
	`sequence` int(11) NOT NULL DEFAULT '10' COMMENT 'Lower = higher priority',
	`stock_id` varchar(20) DEFAULT NULL COMMENT 'Specific item',
	`category_id` smallint(6) DEFAULT NULL COMMENT 'Item category',
	`storage_category_id` int(11) DEFAULT NULL COMMENT 'Required storage category',
	`target_loc_id` int(11) DEFAULT NULL COMMENT 'Fixed target bin',
	`target_zone_id` int(11) DEFAULT NULL COMMENT 'Target zone (find available bin within)',
	`strategy` varchar(30) NOT NULL DEFAULT 'first_available' COMMENT 'first_available|nearest_available|add_to_existing|least_packages|abc_class',
	`active` tinyint(1) NOT NULL DEFAULT '1',
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`rule_id`),
	KEY `sequence` (`sequence`),
	KEY `warehouse_loc_code` (`warehouse_loc_code`)
) ENGINE=InnoDB;

-- Data of table `0_wh_putaway_rules` --

-- Structure of table `0_wh_removal_strategies` --

DROP TABLE IF EXISTS `0_wh_removal_strategies`;

CREATE TABLE `0_wh_removal_strategies` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`warehouse_loc_code` varchar(5) DEFAULT NULL COMMENT 'NULL = default for all',
	`stock_id` varchar(20) DEFAULT NULL COMMENT 'NULL = all items',
	`category_id` smallint(6) DEFAULT NULL COMMENT 'NULL = all categories',
	`strategy` varchar(20) NOT NULL DEFAULT 'fifo' COMMENT 'fifo|fefo|lifo|closest|least_packages',
	`sequence` int(11) NOT NULL DEFAULT '10',
	`active` tinyint(1) NOT NULL DEFAULT '1',
	PRIMARY KEY (`id`),
	KEY `sequence` (`sequence`)
) ENGINE=InnoDB;

-- Data of table `0_wh_removal_strategies` --

-- Structure of table `0_wh_transfer_orders` --

DROP TABLE IF EXISTS `0_wh_transfer_orders`;

CREATE TABLE `0_wh_transfer_orders` (
	`transfer_id` int(11) NOT NULL AUTO_INCREMENT,
	`transfer_no` int(11) NOT NULL COMMENT 'Sequential transfer number',
	`transfer_type` varchar(20) NOT NULL DEFAULT 'standard' COMMENT 'standard|urgent|backorder',
	`status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft|approved|shipped|in_transit|received|cancelled',
	`from_loc_code` varchar(5) NOT NULL COMMENT 'Source warehouse (FK 0_locations)',
	`to_loc_code` varchar(5) NOT NULL COMMENT 'Dest warehouse (FK 0_locations)',
	`transit_loc_id` int(11) DEFAULT NULL COMMENT 'Transit location (FK wh_locations)',
	`request_date` date NOT NULL,
	`expected_ship_date` date DEFAULT NULL,
	`actual_ship_date` date DEFAULT NULL,
	`expected_recv_date` date DEFAULT NULL,
	`actual_recv_date` date DEFAULT NULL,
	`requested_by` smallint(6) DEFAULT NULL,
	`approved_by` smallint(6) DEFAULT NULL,
	`shipped_by` smallint(6) DEFAULT NULL,
	`received_by` smallint(6) DEFAULT NULL,
	`approval_date` datetime DEFAULT NULL,
	`reference` varchar(60) DEFAULT NULL,
	`memo` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`transfer_id`),
	UNIQUE KEY `transfer_no` (`transfer_no`),
	KEY `status` (`status`),
	KEY `from_loc_code` (`from_loc_code`),
	KEY `to_loc_code` (`to_loc_code`),
	KEY `request_date` (`request_date`)
) ENGINE=InnoDB;

-- Data of table `0_wh_transfer_orders` --

-- Structure of table `0_wh_transfer_order_lines` --

DROP TABLE IF EXISTS `0_wh_transfer_order_lines`;

CREATE TABLE `0_wh_transfer_order_lines` (
	`line_id` int(11) NOT NULL AUTO_INCREMENT,
	`transfer_id` int(11) NOT NULL,
	`stock_id` varchar(20) NOT NULL,
	`qty_requested` decimal(15,4) NOT NULL,
	`qty_shipped` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`qty_received` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`from_bin_id` int(11) DEFAULT NULL COMMENT 'Specific source bin',
	`to_bin_id` int(11) DEFAULT NULL COMMENT 'Specific dest bin',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`unit_cost` decimal(15,4) DEFAULT '0.0000',
	`memo` varchar(255) DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`line_id`),
	KEY `transfer_id` (`transfer_id`),
	KEY `batch_id` (`batch_id`),
	KEY `serial_id` (`serial_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_transfer_order_lines` --

-- Structure of table `0_wh_material_requests` --

DROP TABLE IF EXISTS `0_wh_material_requests`;

CREATE TABLE `0_wh_material_requests` (
	`request_id` int(11) NOT NULL AUTO_INCREMENT,
	`request_no` int(11) NOT NULL,
	`request_type` varchar(20) NOT NULL COMMENT 'purchase|transfer|manufacturing|issue',
	`status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft|submitted|approved|ordered|fulfilled|cancelled',
	`warehouse_loc_code` varchar(5) NOT NULL COMMENT 'Requesting warehouse',
	`requested_by` smallint(6) DEFAULT NULL,
	`approved_by` smallint(6) DEFAULT NULL,
	`request_date` date NOT NULL,
	`required_date` date DEFAULT NULL,
	`reference` varchar(60) DEFAULT NULL,
	`memo` text DEFAULT NULL,
	`linked_doc_type` int(11) DEFAULT NULL COMMENT 'PO, Transfer Order, Work Order',
	`linked_doc_no` int(11) DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`request_id`),
	UNIQUE KEY `request_no` (`request_no`),
	KEY `status` (`status`),
	KEY `request_type` (`request_type`)
) ENGINE=InnoDB;

-- Data of table `0_wh_material_requests` --

-- Structure of table `0_wh_material_request_lines` --

DROP TABLE IF EXISTS `0_wh_material_request_lines`;

CREATE TABLE `0_wh_material_request_lines` (
	`line_id` int(11) NOT NULL AUTO_INCREMENT,
	`request_id` int(11) NOT NULL,
	`stock_id` varchar(20) NOT NULL,
	`qty_requested` decimal(15,4) NOT NULL,
	`qty_fulfilled` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`required_date` date DEFAULT NULL,
	`memo` varchar(255) DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`line_id`),
	KEY `request_id` (`request_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_material_request_lines` --

-- Structure of table `0_wh_picking_waves` --

DROP TABLE IF EXISTS `0_wh_picking_waves`;

CREATE TABLE `0_wh_picking_waves` (
	`wave_id` int(11) NOT NULL AUTO_INCREMENT,
	`wave_name` varchar(60) NOT NULL,
	`wave_type` varchar(20) NOT NULL DEFAULT 'standard' COMMENT 'standard|rush|backorder',
	`status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft|released|in_progress|done|cancelled',
	`warehouse_loc_code` varchar(5) NOT NULL,
	`picking_method` varchar(20) NOT NULL DEFAULT 'single' COMMENT 'single|batch|cluster|wave',
	`created_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`released_date` datetime DEFAULT NULL,
	`completed_date` datetime DEFAULT NULL,
	`assigned_to` smallint(6) DEFAULT NULL,
	`total_orders` int(11) DEFAULT '0',
	`total_lines` int(11) DEFAULT '0',
	`memo` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`wave_id`),
	KEY `status` (`status`),
	KEY `warehouse_loc_code` (`warehouse_loc_code`)
) ENGINE=InnoDB;

-- Data of table `0_wh_picking_waves` --

-- Structure of table `0_wh_packages` --

DROP TABLE IF EXISTS `0_wh_packages`;

CREATE TABLE `0_wh_packages` (
	`package_id` int(11) NOT NULL AUTO_INCREMENT,
	`package_code` varchar(40) NOT NULL COMMENT 'Barcode/identifier',
	`package_type` varchar(20) NOT NULL DEFAULT 'box' COMMENT 'box|pallet|crate|envelope|other',
	`status` varchar(20) NOT NULL DEFAULT 'open' COMMENT 'open|sealed|shipped|received|returned',
	`current_loc_id` int(11) DEFAULT NULL COMMENT 'Current warehouse location',
	`weight` decimal(15,4) DEFAULT NULL,
	`length` decimal(10,2) DEFAULT NULL,
	`width` decimal(10,2) DEFAULT NULL,
	`height` decimal(10,2) DEFAULT NULL,
	`shipping_label` varchar(100) DEFAULT NULL,
	`tracking_number` varchar(100) DEFAULT NULL,
	`carrier` varchar(60) DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`package_id`),
	UNIQUE KEY `package_code` (`package_code`),
	KEY `current_loc_id` (`current_loc_id`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- Data of table `0_wh_packages` --

-- Structure of table `0_wh_package_contents` --

DROP TABLE IF EXISTS `0_wh_package_contents`;

CREATE TABLE `0_wh_package_contents` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`package_id` int(11) NOT NULL,
	`stock_id` varchar(20) NOT NULL,
	`qty` decimal(15,4) NOT NULL,
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	PRIMARY KEY (`id`),
	KEY `package_id` (`package_id`),
	KEY `batch_id` (`batch_id`),
	KEY `serial_id` (`serial_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_package_contents` --

-- Structure of table `0_wh_cycle_count_plans` --

DROP TABLE IF EXISTS `0_wh_cycle_count_plans`;

CREATE TABLE `0_wh_cycle_count_plans` (
	`plan_id` int(11) NOT NULL AUTO_INCREMENT,
	`plan_name` varchar(100) NOT NULL,
	`count_method` varchar(20) NOT NULL COMMENT 'location_based|item_based|abc_based|full',
	`warehouse_loc_code` varchar(5) DEFAULT NULL,
	`frequency_days` int(11) NOT NULL DEFAULT '30',
	`abc_class` char(1) DEFAULT NULL COMMENT 'For ABC-based: A=weekly, B=monthly, C=quarterly',
	`last_count_date` date DEFAULT NULL,
	`next_count_date` date DEFAULT NULL,
	`active` tinyint(1) NOT NULL DEFAULT '1',
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`plan_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_cycle_count_plans` --

-- Structure of table `0_wh_cycle_counts` --

DROP TABLE IF EXISTS `0_wh_cycle_counts`;

CREATE TABLE `0_wh_cycle_counts` (
	`count_id` int(11) NOT NULL AUTO_INCREMENT,
	`plan_id` int(11) DEFAULT NULL,
	`count_date` date NOT NULL,
	`status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft|in_progress|review|approved|posted',
	`warehouse_loc_code` varchar(5) NOT NULL,
	`counted_by` smallint(6) DEFAULT NULL,
	`approved_by` smallint(6) DEFAULT NULL,
	`posted_date` datetime DEFAULT NULL,
	`memo` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`count_id`),
	KEY `plan_id` (`plan_id`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- Data of table `0_wh_cycle_counts` --

-- Structure of table `0_wh_cycle_count_lines` --

DROP TABLE IF EXISTS `0_wh_cycle_count_lines`;

CREATE TABLE `0_wh_cycle_count_lines` (
	`line_id` int(11) NOT NULL AUTO_INCREMENT,
	`count_id` int(11) NOT NULL,
	`wh_loc_id` int(11) NOT NULL COMMENT 'Bin being counted',
	`stock_id` varchar(20) NOT NULL,
	`system_qty` decimal(15,4) NOT NULL COMMENT 'Expected QoH',
	`counted_qty` decimal(15,4) DEFAULT NULL COMMENT 'Physically counted',
	`variance_qty` decimal(15,4) DEFAULT NULL COMMENT 'Maintained by app code: counted_qty - system_qty',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`adjustment_posted` tinyint(1) NOT NULL DEFAULT '0',
	`memo` varchar(255) DEFAULT NULL,
	PRIMARY KEY (`line_id`),
	KEY `count_id` (`count_id`),
	KEY `batch_id` (`batch_id`),
	KEY `serial_id` (`serial_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_cycle_count_lines` --

-- Structure of table `0_wh_scrap` --

DROP TABLE IF EXISTS `0_wh_scrap`;

CREATE TABLE `0_wh_scrap` (
	`scrap_id` int(11) NOT NULL AUTO_INCREMENT,
	`scrap_date` date NOT NULL,
	`stock_id` varchar(20) NOT NULL,
	`qty` decimal(15,4) NOT NULL,
	`from_loc_id` int(11) NOT NULL COMMENT 'Source bin',
	`scrap_loc_id` int(11) DEFAULT NULL COMMENT 'Scrap destination',
	`reason_code` varchar(20) NOT NULL COMMENT 'damaged|expired|quality_fail|obsolete|other',
	`reason_detail` varchar(255) DEFAULT NULL,
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`unit_cost` decimal(15,4) DEFAULT '0.0000',
	`total_cost` decimal(15,4) DEFAULT '0.0000',
	`stock_move_id` int(11) DEFAULT NULL COMMENT 'Link to stock_moves entry',
	`approved_by` smallint(6) DEFAULT NULL,
	`memo` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`scrap_id`),
	KEY `stock_id` (`stock_id`),
	KEY `reason_code` (`reason_code`),
	KEY `serial_id` (`serial_id`),
	KEY `batch_id` (`batch_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_scrap` --

-- Structure of table `0_wh_return_orders` --

DROP TABLE IF EXISTS `0_wh_return_orders`;

CREATE TABLE `0_wh_return_orders` (
	`return_id` int(11) NOT NULL AUTO_INCREMENT,
	`return_no` int(11) NOT NULL,
	`return_type` varchar(20) NOT NULL COMMENT 'customer_return|supplier_return|rma',
	`status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft|approved|received|inspected|processed|closed',
	`source_doc_type` int(11) DEFAULT NULL COMMENT 'Original delivery/receipt',
	`source_doc_no` int(11) DEFAULT NULL,
	`customer_id` int(11) DEFAULT NULL,
	`supplier_id` int(11) DEFAULT NULL,
	`warehouse_loc_code` varchar(5) NOT NULL,
	`return_date` date NOT NULL,
	`received_date` date DEFAULT NULL,
	`disposition_code` varchar(20) DEFAULT NULL COMMENT 'restock|refurbish|scrap|return_to_vendor',
	`reference` varchar(60) DEFAULT NULL,
	`memo` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`return_id`),
	UNIQUE KEY `return_no` (`return_no`),
	KEY `status` (`status`)
) ENGINE=InnoDB;

-- Data of table `0_wh_return_orders` --

-- Structure of table `0_wh_return_order_lines` --

DROP TABLE IF EXISTS `0_wh_return_order_lines`;

CREATE TABLE `0_wh_return_order_lines` (
	`line_id` int(11) NOT NULL AUTO_INCREMENT,
	`return_id` int(11) NOT NULL,
	`stock_id` varchar(20) NOT NULL,
	`qty_expected` decimal(15,4) NOT NULL,
	`qty_received` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`qty_good` decimal(15,4) NOT NULL DEFAULT '0.0000' COMMENT 'Restockable qty',
	`qty_damaged` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`qty_scrap` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`disposition_code` varchar(20) DEFAULT NULL,
	`to_bin_id` int(11) DEFAULT NULL,
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`reason_code` varchar(20) DEFAULT NULL COMMENT 'defective|wrong_item|damaged_in_transit|not_needed',
	`memo` varchar(255) DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`line_id`),
	KEY `return_id` (`return_id`),
	KEY `batch_id` (`batch_id`),
	KEY `serial_id` (`serial_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_return_order_lines` --

-- Structure of table `0_wh_consignment_stock` --

DROP TABLE IF EXISTS `0_wh_consignment_stock`;

CREATE TABLE `0_wh_consignment_stock` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`stock_id` varchar(20) NOT NULL,
	`supplier_id` int(11) NOT NULL,
	`wh_loc_id` int(11) NOT NULL COMMENT 'Bin where stored',
	`qty_on_hand` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`qty_consumed` decimal(15,4) NOT NULL DEFAULT '0.0000' COMMENT 'Used but not yet invoiced',
	`unit_cost` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`receipt_date` date NOT NULL,
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`status` varchar(20) NOT NULL DEFAULT 'on_hand' COMMENT 'on_hand|consumed|returned',
	`memo` text DEFAULT NULL COMMENT 'Notes / reference',
	`loc_code` varchar(5) DEFAULT NULL COMMENT 'Inventory location code',
	`custom_data` text DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `stock_id` (`stock_id`),
	KEY `supplier_id` (`supplier_id`),
	KEY `batch_id` (`batch_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_consignment_stock` --

-- Structure of table `0_wh_consignment_movements` --

DROP TABLE IF EXISTS `0_wh_consignment_movements`;

CREATE TABLE `0_wh_consignment_movements` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`consignment_id` int(11) NOT NULL COMMENT 'FK to wh_consignment_stock.id',
	`movement_type` varchar(20) NOT NULL COMMENT 'receive|consume|return|adjust',
	`qty` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`movement_date` date NOT NULL,
	`description` text DEFAULT NULL,
	`reference_no` int(11) DEFAULT NULL COMMENT 'Related transaction number',
	`memo` text DEFAULT NULL,
	`invoiced` tinyint(1) NOT NULL DEFAULT '0' COMMENT '1 if covered by AP invoice',
	`invoice_trans_no` int(11) DEFAULT NULL COMMENT 'Linked supplier invoice trans_no',
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `consignment_id` (`consignment_id`),
	KEY `movement_type` (`movement_type`),
	KEY `invoiced` (`invoiced`)
) ENGINE=InnoDB;

-- Data of table `0_wh_consignment_movements` --

-- Structure of table `0_wh_vmi_levels` --

DROP TABLE IF EXISTS `0_wh_vmi_levels`;

CREATE TABLE `0_wh_vmi_levels` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`stock_id` varchar(20) NOT NULL,
	`supplier_id` int(11) NOT NULL,
	`min_level` decimal(15,4) NOT NULL DEFAULT '0.0000' COMMENT 'Minimum stock level (reorder point)',
	`max_level` decimal(15,4) NOT NULL DEFAULT '0.0000' COMMENT 'Maximum stock level',
	`reorder_qty` decimal(15,4) NOT NULL DEFAULT '0.0000' COMMENT 'Suggested reorder quantity',
	`loc_code` varchar(5) DEFAULT NULL COMMENT 'Location code for level tracking',
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	UNIQUE KEY `uk_item_supplier` (`stock_id`, `supplier_id`),
	KEY `supplier_id` (`supplier_id`)
) ENGINE=InnoDB;

-- Data of table `0_wh_vmi_levels` --

-- Structure of table `0_wh_replenishment_rules` --

DROP TABLE IF EXISTS `0_wh_replenishment_rules`;

CREATE TABLE `0_wh_replenishment_rules` (
	`rule_id` int(11) NOT NULL AUTO_INCREMENT,
	`rule_name` varchar(100) NOT NULL,
	`rule_type` varchar(20) NOT NULL COMMENT 'min_max|mto|reorder_point|inter_warehouse|pick_face|forecast',
	`stock_id` varchar(20) DEFAULT NULL,
	`category_id` smallint(6) DEFAULT NULL,
	`warehouse_loc_code` varchar(5) DEFAULT NULL,
	`bin_loc_id` int(11) DEFAULT NULL COMMENT 'For pick-face replenishment',
	`min_qty` decimal(15,4) DEFAULT NULL,
	`max_qty` decimal(15,4) DEFAULT NULL,
	`reorder_qty` decimal(15,4) DEFAULT NULL,
	`safety_stock` decimal(15,4) DEFAULT NULL,
	`lead_time_days` int(11) DEFAULT NULL,
	`supply_warehouse` varchar(5) DEFAULT NULL COMMENT 'For inter-warehouse',
	`supply_method` varchar(20) DEFAULT 'purchase' COMMENT 'purchase|transfer|manufacture',
	`preferred_supplier` int(11) DEFAULT NULL,
	`active` tinyint(1) NOT NULL DEFAULT '1',
	`auto_execute` tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=suggest only, 1=auto-create orders',
	`custom_data` text DEFAULT NULL,
	`preferred_supplier_id` INT NOT NULL DEFAULT 0 COMMENT 'For purchasing-focused reorder',
	`auto_create_rfq` TINYINT(1) NOT NULL DEFAULT 0 COMMENT '1=auto-create RFQ for multi-sourcing',
	`auto_create_po` TINYINT(1) NOT NULL DEFAULT 0 COMMENT '1=auto-create PO from approved suppliers',
	PRIMARY KEY (`rule_id`),
	KEY `rule_type` (`rule_type`)
) ENGINE=InnoDB;

-- Data of table `0_wh_replenishment_rules` --

-- Structure of table `0_wh_bin_stock` --

DROP TABLE IF EXISTS `0_wh_bin_stock`;

CREATE TABLE `0_wh_bin_stock` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`wh_loc_id` int(11) NOT NULL COMMENT 'FK to wh_locations.loc_id (bin)',
	`stock_id` varchar(20) NOT NULL COMMENT 'FK to stock_master.stock_id',
	`qty_on_hand` decimal(15,4) NOT NULL DEFAULT '0.0000',
	`qty_reserved` decimal(15,4) NOT NULL DEFAULT '0.0000' COMMENT 'Reserved for orders',
	`qty_available` decimal(15,4) NOT NULL DEFAULT '0.0000' COMMENT 'Maintained by app code: on_hand - reserved',
	`reorder_level` decimal(15,4) DEFAULT NULL COMMENT 'Bin-specific reorder',
	`max_level` decimal(15,4) DEFAULT NULL COMMENT 'Max for this item in this bin',
	`batch_id` int(11) DEFAULT NULL COMMENT 'FK to stock_batches.id',
	`serial_id` int(11) DEFAULT NULL COMMENT 'FK to serial_numbers.id',
	`expiry_date` date DEFAULT NULL,
	`stock_status` varchar(20) NOT NULL DEFAULT 'available' COMMENT 'available|reserved|on_hold|quarantine|damaged|in_transit',
	`last_count_date` date DEFAULT NULL,
	`custom_data` text DEFAULT NULL,
	`updated_at` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	UNIQUE KEY `bin_item_batch_serial` (`wh_loc_id`, `stock_id`, `batch_id`, `serial_id`),
	KEY `wh_loc_id` (`wh_loc_id`),
	KEY `stock_id` (`stock_id`),
	KEY `batch_id` (`batch_id`),
	KEY `serial_id` (`serial_id`),
	KEY `stock_status` (`stock_status`),
	KEY `expiry_date` (`expiry_date`)
) ENGINE=InnoDB;

-- Data of table `0_wh_bin_stock` --

-- Structure of table `0_warranty_provision_log` --

DROP TABLE IF EXISTS `0_warranty_provision_log`;

CREATE TABLE `0_warranty_provision_log` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`trans_type` int(11) NOT NULL COMMENT 'Source trans type (e.g., ST_CUSTDELIVERY)',
	`trans_no` int(11) NOT NULL COMMENT 'Source trans number',
	`stock_id` varchar(20) NOT NULL,
	`serial_id` int(11) DEFAULT NULL,
	`batch_id` int(11) DEFAULT NULL,
	`customer_id` int(11) NOT NULL,
	`provision_type` varchar(10) NOT NULL COMMENT 'accrual or release',
	`amount` double NOT NULL DEFAULT '0',
	`warranty_end` date DEFAULT NULL,
	`tran_date` date NOT NULL,
	`gl_trans_id` int(11) DEFAULT NULL COMMENT 'Reference to gl_trans counter',
	`notes` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_wp_trans` (`trans_type`, `trans_no`),
	KEY `idx_wp_stock` (`stock_id`),
	KEY `idx_wp_customer` (`customer_id`),
	KEY `idx_wp_type` (`provision_type`)
) ENGINE=InnoDB;

-- Data of table `0_warranty_provision_log` --

-- Structure of table `0_regulatory_profiles` --

DROP TABLE IF EXISTS `0_regulatory_profiles`;

CREATE TABLE `0_regulatory_profiles` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`stock_id` varchar(20) DEFAULT NULL COMMENT 'Specific item (NULL = category-level)',
	`category_id` int(11) DEFAULT NULL COMMENT 'Item category (NULL = item-level)',
	`framework` varchar(20) NOT NULL COMMENT 'dscsa|fmd|fsma204|udi',
	`enabled` tinyint(1) NOT NULL DEFAULT '1',
	`settings` text DEFAULT NULL COMMENT 'JSON framework-specific settings',
	`notes` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_date` datetime DEFAULT CURRENT_TIMESTAMP,
	`updated_date` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY (`id`),
	KEY `idx_reg_stock` (`stock_id`),
	KEY `idx_reg_category` (`category_id`),
	KEY `idx_reg_framework` (`framework`),
	UNIQUE KEY `uk_reg_profile` (`stock_id`, `category_id`, `framework`)
) ENGINE=InnoDB;

-- Data of table `0_regulatory_profiles` --

-- Structure of table `0_dscsa_transactions` --

DROP TABLE IF EXISTS `0_dscsa_transactions`;

CREATE TABLE `0_dscsa_transactions` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`transaction_type` varchar(20) NOT NULL COMMENT 'sale|return|suspect|illegitimate',
	`delivery_id` int(11) DEFAULT NULL COMMENT 'FK to trans_no of ST_CUSTDELIVERY',
	`grn_id` int(11) DEFAULT NULL COMMENT 'FK to grn_batch_id for inbound',
	`stock_id` varchar(20) NOT NULL,
	`serial_id` int(11) DEFAULT NULL,
	`batch_id` int(11) DEFAULT NULL,
	`ndc` varchar(20) DEFAULT NULL COMMENT 'National Drug Code',
	`gtin` varchar(14) DEFAULT NULL,
	`serial_no` varchar(50) DEFAULT NULL,
	`lot_number` varchar(50) DEFAULT NULL,
	`expiry_date` date DEFAULT NULL,
	`sender_id` int(11) DEFAULT NULL COMMENT 'Supplier or our company',
	`sender_name` varchar(100) DEFAULT NULL,
	`sender_license` varchar(50) DEFAULT NULL COMMENT 'State license number',
	`receiver_id` int(11) DEFAULT NULL COMMENT 'Customer',
	`receiver_name` varchar(100) DEFAULT NULL,
	`receiver_license` varchar(50) DEFAULT NULL,
	`transaction_date` date NOT NULL,
	`verification_status` varchar(20) DEFAULT 'pending' COMMENT 'pending|verified|failed|suspect',
	`verification_date` datetime DEFAULT NULL,
	`verified_by` int(11) DEFAULT NULL,
	`gs1_barcode` text DEFAULT NULL COMMENT 'Full GS1 DataMatrix content',
	`transaction_history` text DEFAULT NULL COMMENT 'JSON chain of prior TI/TH/TS',
	`suspect_reason` text DEFAULT NULL,
	`suspect_reported_date` datetime DEFAULT NULL,
	`quarantine_action` varchar(20) DEFAULT NULL COMMENT 'quarantine|destroy|return',
	`notes` text DEFAULT NULL,
	`custom_data` text DEFAULT NULL COMMENT 'JSON extensible',
	PRIMARY KEY (`id`),
	KEY `idx_dscsa_delivery` (`delivery_id`),
	KEY `idx_dscsa_grn` (`grn_id`),
	KEY `idx_dscsa_stock` (`stock_id`),
	KEY `idx_dscsa_serial` (`serial_id`),
	KEY `idx_dscsa_batch` (`batch_id`),
	KEY `idx_dscsa_type` (`transaction_type`),
	KEY `idx_dscsa_status` (`verification_status`),
	KEY `idx_dscsa_date` (`transaction_date`)
) ENGINE=InnoDB;

-- Data of table `0_dscsa_transactions` --

-- Structure of table `0_fsma_tracking_events` --

DROP TABLE IF EXISTS `0_fsma_tracking_events`;

CREATE TABLE `0_fsma_tracking_events` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`event_type` varchar(30) NOT NULL COMMENT 'growing|receiving|transforming|creating|shipping',
	`stock_id` varchar(20) NOT NULL,
	`batch_id` int(11) DEFAULT NULL,
	`lot_code` varchar(50) DEFAULT NULL COMMENT 'Traceability lot code (TLC)',
	`location_description` text DEFAULT NULL COMMENT 'Where the event occurred',
	`loc_code` varchar(5) DEFAULT NULL COMMENT 'FK to locations',
	`event_date` datetime NOT NULL,
	`quantity` double DEFAULT NULL,
	`unit_of_measure` varchar(20) DEFAULT NULL,
	`reference_type` varchar(20) DEFAULT NULL COMMENT 'grn|delivery|transfer|production',
	`reference_id` int(11) DEFAULT NULL COMMENT 'FK to the source transaction',
	`trading_partner_type` varchar(20) DEFAULT NULL COMMENT 'supplier|customer|transporter',
	`trading_partner_id` int(11) DEFAULT NULL,
	`trading_partner_name` varchar(100) DEFAULT NULL,
	`country_of_origin` varchar(50) DEFAULT NULL,
	`harvest_date` date DEFAULT NULL,
	`cooling_date` date DEFAULT NULL,
	`pack_date` date DEFAULT NULL,
	`ship_date` date DEFAULT NULL,
	`temperature_data` text DEFAULT NULL COMMENT 'JSON temperature log',
	`notes` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_date` datetime DEFAULT CURRENT_TIMESTAMP,
	`custom_data` text DEFAULT NULL COMMENT 'JSON extensible for KDEs',
	PRIMARY KEY (`id`),
	KEY `idx_fsma_stock` (`stock_id`),
	KEY `idx_fsma_batch` (`batch_id`),
	KEY `idx_fsma_event` (`event_type`),
	KEY `idx_fsma_date` (`event_date`),
	KEY `idx_fsma_lot` (`lot_code`),
	KEY `idx_fsma_ref` (`reference_type`, `reference_id`)
) ENGINE=InnoDB;

-- Data of table `0_fsma_tracking_events` --

-- Structure of table `0_udi_registrations` --

DROP TABLE IF EXISTS `0_udi_registrations`;

CREATE TABLE `0_udi_registrations` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`stock_id` varchar(20) NOT NULL,
	`udi_di` varchar(50) NOT NULL COMMENT 'Device Identifier (fixed portion)',
	`udi_pi` varchar(100) DEFAULT NULL COMMENT 'Production Identifier (variable: serial+lot+expiry+date)',
	`issuing_agency` varchar(10) NOT NULL DEFAULT 'GS1' COMMENT 'GS1|HIBCC|ICCBBA',
	`device_description` text DEFAULT NULL,
	`brand_name` varchar(100) DEFAULT NULL,
	`version_model` varchar(100) DEFAULT NULL,
	`company_name` varchar(100) DEFAULT NULL,
	`mri_safety` varchar(20) DEFAULT NULL COMMENT 'MR Safe|MR Conditional|MR Unsafe',
	`device_sterile` tinyint(1) DEFAULT '0',
	`single_use` tinyint(1) DEFAULT '0',
	`implantable` tinyint(1) DEFAULT '0',
	`rx_only` tinyint(1) DEFAULT '0',
	`otc` tinyint(1) DEFAULT '0',
	`fda_listing_number` varchar(20) DEFAULT NULL,
	`fda_premarket_number` varchar(20) DEFAULT NULL COMMENT '510(k) or PMA number',
	`gtin` varchar(14) DEFAULT NULL,
	`serial_id` int(11) DEFAULT NULL,
	`batch_id` int(11) DEFAULT NULL,
	`manufacturing_date` date DEFAULT NULL,
	`expiry_date` date DEFAULT NULL,
	`gudid_submitted` tinyint(1) DEFAULT '0' COMMENT 'Submitted to FDA GUDID',
	`gudid_submission_date` datetime DEFAULT NULL,
	`notes` text DEFAULT NULL,
	`created_by` int(11) DEFAULT NULL,
	`created_date` datetime DEFAULT CURRENT_TIMESTAMP,
	`updated_date` datetime DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP,
	`custom_data` text DEFAULT NULL COMMENT 'JSON extensible',
	PRIMARY KEY (`id`),
	KEY `idx_udi_stock` (`stock_id`),
	KEY `idx_udi_di` (`udi_di`),
	KEY `idx_udi_serial` (`serial_id`),
	KEY `idx_udi_batch` (`batch_id`),
	KEY `idx_udi_gtin` (`gtin`),
	KEY `idx_udi_agency` (`issuing_agency`)
) ENGINE=InnoDB;

-- Data of table `0_udi_registrations` --

-- =====================================================================================
-- End of Advanced Inventory / Warehouse Management Tables

-- Structure of table `0_supp_allocations` --

DROP TABLE IF EXISTS `0_supp_allocations`;

CREATE TABLE `0_supp_allocations` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`person_id` int(11) DEFAULT NULL,
	`amt` double unsigned DEFAULT NULL,
	`date_alloc` date NOT NULL DEFAULT '0000-00-00',
	`trans_no_from` int(11) DEFAULT NULL,
	`trans_type_from` int(11) DEFAULT NULL,
	`trans_no_to` int(11) DEFAULT NULL,
	`trans_type_to` int(11) DEFAULT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `trans_type_from` (`person_id`,`trans_type_from`,`trans_no_from`,`trans_type_to`,`trans_no_to`),
	KEY `From` (`trans_type_from`,`trans_no_from`),
	KEY `To` (`trans_type_to`,`trans_no_to`)
) ENGINE=InnoDB ;

-- Data of table `0_supp_allocations` --

-- Structure of table `0_supp_invoice_items` --

DROP TABLE IF EXISTS `0_supp_invoice_items`;

CREATE TABLE `0_supp_invoice_items` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`supp_trans_no` int(11) DEFAULT NULL,
	`supp_trans_type` int(11) DEFAULT NULL,
	`gl_code` varchar(15) NOT NULL DEFAULT '',
	`grn_item_id` int(11) DEFAULT NULL,
	`po_detail_item_id` int(11) DEFAULT NULL,
	`stock_id` varchar(20) NOT NULL DEFAULT '',
	`description` tinytext,
	`quantity` double NOT NULL DEFAULT '0',
	`unit_price` double NOT NULL DEFAULT '0',
	`unit_tax` double NOT NULL DEFAULT '0',
	`memo_` tinytext,
	`dimension_id` int(11) NOT NULL DEFAULT '0',
	`dimension2_id` int(11) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	KEY `Transaction` (`supp_trans_type`,`supp_trans_no`,`stock_id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_supp_invoice_items` --

INSERT INTO `0_supp_invoice_items` VALUES
('1', '1', '20', '0', '4', '4', '101', 'Vsmart Aris 6GB-64GB', '15', '200', '10', NULL, '0', '0', '{}'),
('2', '2', '20', '0', '5', '5', '102', 'Vsmart Live 4 (6GB/64GB)', '6', '150', '7.5', NULL, '0', '0', '{}');

-- Structure of table `0_supp_trans` --

DROP TABLE IF EXISTS `0_supp_trans`;

CREATE TABLE `0_supp_trans` (
	`trans_no` int(11) unsigned NOT NULL DEFAULT '0',
	`type` smallint(6) unsigned NOT NULL DEFAULT '0',
	`supplier_id` int(11) unsigned NOT NULL,
	`reference` tinytext NOT NULL,
	`supp_reference` varchar(60) NOT NULL DEFAULT '',
	`tran_date` date NOT NULL DEFAULT '0000-00-00',
	`due_date` date NOT NULL DEFAULT '0000-00-00',
	`ov_amount` double NOT NULL DEFAULT '0',
	`ov_discount` double NOT NULL DEFAULT '0',
	`ov_gst` double NOT NULL DEFAULT '0',
	`rate` double NOT NULL DEFAULT '1',
	`alloc` double NOT NULL DEFAULT '0',
	`tax_included` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`type`,`trans_no`,`supplier_id`),
	KEY `supplier_id` (`supplier_id`),
	KEY `tran_date` (`tran_date`)
) ENGINE=InnoDB ;

-- Data of table `0_supp_trans` --

INSERT INTO `0_supp_trans` VALUES
('1', '20', '1', '001/2025', 'rr4', '2025-05-05', '2025-05-15', '3000', '0', '150', '1', '0', '0', '{}'),
('2', '20', '1', '001/2026', 'asd5', '2026-01-21', '2026-01-31', '900', '0', '45', '1', '0', '0', '{}');

-- Structure of table `0_suppliers` --

DROP TABLE IF EXISTS `0_suppliers`;

CREATE TABLE `0_suppliers` (
	`supplier_id` int(11) NOT NULL AUTO_INCREMENT,
	`supp_name` varchar(60) NOT NULL DEFAULT '',
	`supp_ref` varchar(30) NOT NULL DEFAULT '',
	`address` tinytext NOT NULL,
	`supp_address` tinytext NOT NULL,
	`gst_no` varchar(25) NOT NULL DEFAULT '',
	`contact` varchar(60) NOT NULL DEFAULT '',
	`supp_account_no` varchar(40) NOT NULL DEFAULT '',
	`website` varchar(100) NOT NULL DEFAULT '',
	`bank_account` varchar(60) NOT NULL DEFAULT '',
	`curr_code` char(3) DEFAULT NULL,
	`payment_terms` int(11) DEFAULT NULL,
	`tax_included` tinyint(1) NOT NULL DEFAULT '0',
	`dimension_id` int(11) DEFAULT '0',
	`dimension2_id` int(11) DEFAULT '0',
	`tax_group_id` int(11) DEFAULT NULL,
	`credit_limit` double NOT NULL DEFAULT '0',
	`purchase_account` varchar(15) NOT NULL DEFAULT '',
	`payable_account` varchar(15) NOT NULL DEFAULT '',
	`payment_discount_account` varchar(15) NOT NULL DEFAULT '',
	`notes` tinytext NOT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	`vendor_tier` ENUM('standard','preferred','strategic','critical') NOT NULL DEFAULT 'standard' COMMENT 'Supplier classification tier',
	`vendor_category` VARCHAR(50) NOT NULL DEFAULT '' COMMENT 'Product/service categories supplied',
	`overall_score` DOUBLE NOT NULL DEFAULT 0 COMMENT 'Composite vendor scorecard (0-100)',
	`quality_score` DOUBLE NOT NULL DEFAULT 0 COMMENT 'Quality metrics: on-spec delivery rate',
	`delivery_score` DOUBLE NOT NULL DEFAULT 0 COMMENT 'On-time delivery performance percentage',
	`price_score` DOUBLE NOT NULL DEFAULT 0 COMMENT 'Cost competitiveness vs market benchmarks',
	`service_score` DOUBLE NOT NULL DEFAULT 0 COMMENT 'Customer service responsiveness rating',
	`last_evaluation_date` DATE DEFAULT NULL COMMENT 'Most recent vendor scorecard evaluation',
	`next_evaluation_date` DATE DEFAULT NULL COMMENT 'Scheduled next evaluation date',
	`evaluation_frequency_months` INT NOT NULL DEFAULT 6 COMMENT 'Months between periodic evaluations',
	`certifications` TEXT COMMENT 'ISO/quality certifications held (JSON list or text)',
	`approved_categories` VARCHAR(500) NOT NULL DEFAULT '' COMMENT 'Approved categories for this supplier',
	`lead_time_average` INT NOT NULL DEFAULT 0 COMMENT 'Average lead time in days',
	`on_time_delivery_pct` DOUBLE NOT NULL DEFAULT 0 COMMENT 'Percentage of on-time deliveries',
	`defect_rate_pct` DOUBLE NOT NULL DEFAULT 0 COMMENT 'Quality defect rate percentage',
	`payment_reliability_score` DOUBLE NOT NULL DEFAULT 0 COMMENT 'Payment behavior and reliability score',
	PRIMARY KEY (`supplier_id`),
	UNIQUE KEY `supp_ref` (`supp_ref`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_suppliers` --

INSERT INTO `0_suppliers` VALUES
('1', 'Dino Saurius Inc.', 'Dino Saurius', 'N/A', '', '987654321', '', '', '', '', 'USD', '3', '0', '0', '0', '1', '0', '', '2100', '5060', '', '0', '{}'),
('2', 'Beefeater Ltd.', 'Beefeater', 'N/A', '', '67565590', '', '', '', '', 'GBP', '4', '0', '0', '0', '1', '0', '', '2100', '5060', '', '0', '{}');

-- Structure of table `0_sys_prefs` --

DROP TABLE IF EXISTS `0_sys_prefs`;

CREATE TABLE `0_sys_prefs` (
	`name` varchar(35) NOT NULL DEFAULT '',
	`category` varchar(30) DEFAULT NULL,
	`type` varchar(20) NOT NULL DEFAULT '',
	`length` smallint(6) DEFAULT NULL,
	`value` text NOT NULL,
	PRIMARY KEY (`name`),
	KEY `category` (`category`)
) ENGINE=InnoDB ;

-- Data of table `0_sys_prefs` --

INSERT INTO `0_sys_prefs` VALUES
('coy_name', 'setup.company', 'varchar', 60, 'Company name'),
('gst_no', 'setup.company', 'varchar', 25, ''),
('coy_no', 'setup.company', 'varchar', 25, ''),
('tax_prd', 'setup.company', 'int', 11, '1'),
('tax_last', 'setup.company', 'int', 11, '1'),
('postal_address', 'setup.company', 'tinytext', 0, 'N/A'),
('phone', 'setup.company', 'varchar', 30, ''),
('fax', 'setup.company', 'varchar', 30, ''),
('email', 'setup.company', 'varchar', 100, ''),
('coy_logo', 'setup.company', 'varchar', 100, ''),
('domicile', 'setup.company', 'varchar', 55, ''),
('curr_default', 'setup.company', 'char', 3, 'USD'),
('use_dimension', 'setup.company', 'tinyint', 1, '1'),
('f_year', 'setup.company', 'int', 11, '2'),
('shortname_name_in_list','setup.company', 'tinyint', 1, '0'),
('no_customer_list', 'setup.company', 'tinyint', 1, '0'),
('no_supplier_list', 'setup.company', 'tinyint', 1, '0'),
('base_sales', 'setup.company', 'int', 11, '1'),
('time_zone', 'setup.company', 'tinyint', 1, '0'),
('add_pct', 'setup.company', 'int', 5, '-1'),
('round_to', 'setup.company', 'int', 5, '1'),
('login_tout', 'setup.company', 'smallint', 6, '600'),
('past_due_days', 'glsetup.general', 'int', 11, '30'),
('profit_loss_year_act', 'glsetup.general', 'varchar', 15, '9990'),
('retained_earnings_act', 'glsetup.general', 'varchar', 15, '3590'),
('bank_charge_act', 'glsetup.general', 'varchar', 15, '5690'),
('exchange_diff_act', 'glsetup.general', 'varchar', 15, '4450'),
('tax_algorithm', 'glsetup.customer', 'tinyint', 1, '1'),
('default_credit_limit', 'glsetup.customer', 'int', 11, '1000'),
('accumulate_shipping', 'glsetup.customer', 'tinyint', 1, '0'),
('legal_text', 'glsetup.customer', 'tinytext', 0, ''),
('freight_act', 'glsetup.customer', 'varchar', 15, '4430'),
('debtors_act', 'glsetup.sales', 'varchar', 15, '1200'),
('default_sales_act', 'glsetup.sales', 'varchar', 15, '4010'),
('default_sales_discount_act', 'glsetup.sales', 'varchar', 15, '4510'),
('default_prompt_payment_act', 'glsetup.sales', 'varchar', 15, '4500'),
('default_delivery_required', 'glsetup.sales', 'smallint', 6, '1'),
('default_receival_required', 'glsetup.purchase', 'smallint', 6, '10'),
('default_quote_valid_days', 'glsetup.sales', 'smallint', 6, '30'),
('default_dim_required', 'glsetup.dims', 'int', 11, '20'),
('pyt_discount_act', 'glsetup.purchase', 'varchar', 15, '5060'),
('creditors_act', 'glsetup.purchase', 'varchar', 15, '2100'),
('po_over_receive', 'glsetup.purchase', 'int', 11, '10'),
('po_over_charge', 'glsetup.purchase', 'int', 11, '10'),
('allow_negative_stock', 'glsetup.inventory', 'tinyint', 1, '0'),
('default_inventory_act', 'glsetup.items', 'varchar', 15, '1510'),
('default_cogs_act', 'glsetup.items', 'varchar', 15, '5010'),
('default_adj_act', 'glsetup.items', 'varchar', 15, '5040'),
('default_inv_sales_act', 'glsetup.items', 'varchar', 15, '4010'),
('default_wip_act', 'glsetup.items', 'varchar', 15, '1530'),
('default_workorder_required', 'glsetup.manuf', 'int', 11, '20'),
('version_id', 'system', 'varchar', 11, '1.0'),
('auto_curr_reval', 'setup.company', 'smallint', 6, '1'),
('grn_clearing_act', 'glsetup.purchase', 'varchar', 15, '1550'),
('bcc_email', 'setup.company', 'varchar', 100, ''),
('deferred_income_act', 'glsetup.sales', 'varchar', '15', '2105'),
('gl_closing_date','setup.closing_date', 'date', 8, ''),
('alternative_tax_include_on_docs','setup.company', 'tinyint', 1, '0'),
('no_zero_lines_amount','glsetup.sales', 'tinyint', 1, '1'),
('show_po_item_codes','glsetup.purchase', 'tinyint', 1, '0'),
('accounts_alpha','glsetup.general', 'tinyint', 1, '0'),
('loc_notification','glsetup.inventory', 'tinyint', 1, '0'),
('print_invoice_no','glsetup.sales', 'tinyint', 1, '0'),
('allow_negative_prices','glsetup.inventory', 'tinyint', 1, '1'),
('print_item_images_on_quote','glsetup.inventory', 'tinyint', 1, '0'),
('suppress_tax_rates','setup.company', 'tinyint', 1, '0'),
('company_logo_report','setup.company', 'tinyint', 1, '0'),
('barcodes_on_stock','setup.company', 'tinyint', 1, '0'),
('print_dialog_direct','setup.company', 'tinyint', 1, '0'),
('ref_no_auto_increase','setup.company', 'tinyint', 1, '0'),
('default_loss_on_asset_disposal_act', 'glsetup.items', 'varchar', '15', '5660'),
('depreciation_period', 'glsetup.company', 'tinyint', '1', '1'),
('use_manufacturing','setup.company', 'tinyint', 1, '1'),
('dim_on_recurrent_invoice','setup.company', 'tinyint', 1, '0'),
('long_description_invoice','setup.company', 'tinyint', 1, '0'),
('max_days_in_docs','setup.company', 'smallint', 5, '180'),
('use_fixed_assets','setup.company', 'tinyint', 1, '1'),
('use_hrm','setup.company', 'tinyint', 1, '1'),
('weekend_day', 'setup.company', 'titnyint', 1, '7'),
('payroll_month_work_days', 'setup.company', 'float', '2', 26),
('payroll_payable_act', 'glsetup.hrm', 'varchar', 15, 2100),
('payroll_deductleave_act', 'glsetup.hrm', 'varchar', 15, ''),
('payroll_overtime_act', 'glsetup.hrm', 'varchar', 15, 5420),
('warranty_provision_account', 'glsetup.inventory', 'varchar', 15, ''),
('warranty_expense_account', 'glsetup.inventory', 'varchar', 15, ''),
('warranty_provision_rate', 'glsetup.inventory', 'float', 8, '0'),
('warranty_provision_enabled', 'glsetup.inventory', 'tinyint', 1, '0'),
('regulatory_compliance_enabled', 'setup.company', 'tinyint', 1, '0'),
('dscsa_enabled', 'setup.company', 'tinyint', 1, '0'),
('fsma204_enabled', 'setup.company', 'tinyint', 1, '0'),
('udi_enabled', 'setup.company', 'tinyint', 1, '0'),
('dscsa_company_license', 'setup.company', 'varchar', 50, ''),
('dscsa_company_dea', 'setup.company', 'varchar', 50, ''),
('fsma204_firm_name', 'setup.company', 'varchar', 100, ''),
('fsma204_fda_registration', 'setup.company', 'varchar', 50, ''),
('udi_company_name', 'setup.company', 'varchar', 100, ''),
('udi_issuing_agency', 'setup.company', 'varchar', 10, 'GS1');

-- Structure of table `0_tag_associations` --

DROP TABLE IF EXISTS `0_tag_associations`;

CREATE TABLE `0_tag_associations` (
	`record_id` varchar(30) NOT NULL,
	`tag_id` int(11) NOT NULL,
	PRIMARY KEY (`record_id`,`tag_id`)
) ENGINE=InnoDB ;

-- Data of table `0_tag_associations` --

-- Structure of table `0_tags` --

DROP TABLE IF EXISTS `0_tags`;

CREATE TABLE `0_tags` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`type` smallint(6) NOT NULL,
	`name` varchar(30) NOT NULL,
	`description` varchar(60) DEFAULT NULL,
	`color` varchar(20) DEFAULT NULL,
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `type` (`type`,`name`)
) ENGINE=InnoDB ;

-- Data of table `0_tags` --

INSERT INTO `0_tags` (`type`, `name`, `description`, `color`) VALUES
(3, 'Hot', 'Hot lead/opportunity', '#f44336'),
(3, 'Warm', 'Warm lead/opportunity', '#ff9800'),
(3, 'Cold', 'Cold lead/opportunity', '#2196F3'),
(3, 'VIP', 'VIP client', '#9c27b0'),
(3, 'Enterprise', 'Enterprise segment', '#4caf50'),
(3, 'SMB', 'Small/medium business', '#00bcd4'),
(3, 'Government', 'Government sector', '#795548'),
(3, 'Reseller', 'Reseller partner', '#607d8b');

-- Structure of table `0_tax_group_items` --

DROP TABLE IF EXISTS `0_tax_group_items`;

CREATE TABLE `0_tax_group_items` (
	`tax_group_id` int(11) NOT NULL DEFAULT '0',
	`tax_type_id` int(11) NOT NULL DEFAULT '0',
	`tax_shipping` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`tax_group_id`,`tax_type_id`)
) ENGINE=InnoDB ;

-- Data of table `0_tax_group_items` --

INSERT INTO `0_tax_group_items` VALUES
('1', '1', '1');

-- Structure of table `0_tax_groups` --

DROP TABLE IF EXISTS `0_tax_groups`;

CREATE TABLE `0_tax_groups` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`name` varchar(60) NOT NULL DEFAULT '',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=3 ;

-- Data of table `0_tax_groups` --

INSERT INTO `0_tax_groups` VALUES
('1', 'Tax', '0'),
('2', 'Tax Exempt', '0');

-- Structure of table `0_tax_types` --

DROP TABLE IF EXISTS `0_tax_types`;

CREATE TABLE `0_tax_types` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`rate` double NOT NULL DEFAULT '0',
	`sales_gl_code` varchar(15) NOT NULL DEFAULT '',
	`purchasing_gl_code` varchar(15) NOT NULL DEFAULT '',
	`name` varchar(60) NOT NULL DEFAULT '',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_tax_types` --

INSERT INTO `0_tax_types` VALUES
('1', '5', '2150', '2150', 'Tax', '0', '{}');

-- Structure of table `0_trans_tax_details` --

DROP TABLE IF EXISTS `0_trans_tax_details`;

CREATE TABLE `0_trans_tax_details` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`trans_type` smallint(6) DEFAULT NULL,
	`trans_no` int(11) DEFAULT NULL,
	`tran_date` date NOT NULL,
	`tax_type_id` int(11) NOT NULL DEFAULT '0',
	`rate` double NOT NULL DEFAULT '0',
	`ex_rate` double NOT NULL DEFAULT '1',
	`included_in_price` tinyint(1) NOT NULL DEFAULT '0',
	`net_amount` double NOT NULL DEFAULT '0',
	`amount` double NOT NULL DEFAULT '0',
	`memo` tinytext,
	`reg_type` tinyint(1) DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `Type_and_Number` (`trans_type`,`trans_no`),
	KEY `tran_date` (`tran_date`)
) ENGINE=InnoDB AUTO_INCREMENT=11 ;

-- Data of table `0_trans_tax_details` --

INSERT INTO `0_trans_tax_details` VALUES
('1', '13', '1', '2025-05-10', '1', '5', '1', '1', '5942.86', '297.14', 'auto', NULL),
('2', '10', '1', '2025-05-10', '1', '5', '1', '1', '5942.86', '297.14', '001/2025', '0'),
('3', '20', '1', '2025-05-05', '1', '5', '1', '0', '3000', '150', 'rr4', '1'),
('4', '13', '2', '2025-05-07', '1', '5', '1', '1', '285.71', '14.29', 'auto', NULL),
('5', '10', '2', '2025-05-07', '1', '5', '1', '1', '285.71', '14.29', '002/2025', '0'),
('6', '13', '3', '2025-05-07', '0', '0', '1.123', '1', '267.14', '0', 'auto', NULL),
('7', '10', '3', '2025-05-07', '0', '0', '1.123', '1', '267.14', '0', '003/2025', '0'),
('8', '13', '5', '2026-01-21', '1', '5', '1', '1', '1190.48', '59.52', 'auto', NULL),
('9', '10', '5', '2026-01-21', '1', '5', '1', '1', '1190.48', '59.52', '001/2026', '0'),
('10', '20', '2', '2026-01-21', '1', '5', '1', '0', '900', '45', 'asd5', '1');

-- Structure of table `0_useronline` --

DROP TABLE IF EXISTS `0_useronline`;

CREATE TABLE `0_useronline` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`timestamp` int(15) NOT NULL DEFAULT '0',
	`ip` varchar(40) NOT NULL DEFAULT '',
	`file` varchar(100) NOT NULL DEFAULT '',
	PRIMARY KEY (`id`),
	KEY `timestamp` (`timestamp`),
	KEY `ip` (`ip`)
) ENGINE=InnoDB ;

-- Data of table `0_useronline` --


-- Structure of table `0_users` --

DROP TABLE IF EXISTS `0_users`;

CREATE TABLE `0_users` (
	`id` smallint(6) NOT NULL AUTO_INCREMENT,
	`login_id` varchar(60) NOT NULL DEFAULT '',
	`password` varchar(100) NOT NULL DEFAULT '',
	`real_name` varchar(100) NOT NULL DEFAULT '',
	`role_id` int(11) NOT NULL DEFAULT '1',
	`phone` varchar(30) NOT NULL DEFAULT '',
	`email` varchar(100) DEFAULT NULL,
	`language` varchar(20) DEFAULT NULL,
	`date_format` tinyint(1) NOT NULL DEFAULT '0',
	`date_sep` tinyint(1) NOT NULL DEFAULT '0',
	`tho_sep` tinyint(1) NOT NULL DEFAULT '0',
	`dec_sep` tinyint(1) NOT NULL DEFAULT '0',
	`theme` varchar(20) NOT NULL DEFAULT 'default',
	`page_size` varchar(20) NOT NULL DEFAULT 'A4',
	`prices_dec` smallint(6) NOT NULL DEFAULT '2',
	`qty_dec` smallint(6) NOT NULL DEFAULT '2',
	`rates_dec` smallint(6) NOT NULL DEFAULT '4',
	`percent_dec` smallint(6) NOT NULL DEFAULT '1',
	`show_gl` tinyint(1) NOT NULL DEFAULT '1',
	`show_codes` tinyint(1) NOT NULL DEFAULT '0',
	`show_hints` tinyint(1) NOT NULL DEFAULT '0',
	`last_visit_date` datetime DEFAULT NULL,
	`query_size` tinyint(1) unsigned NOT NULL DEFAULT '10',
	`graphic_links` tinyint(1) DEFAULT '1',
	`pos` smallint(6) DEFAULT '1',
	`print_profile` varchar(30) NOT NULL DEFAULT '',
	`rep_popup` tinyint(1) DEFAULT '1',
	`sticky_doc_date` tinyint(1) DEFAULT '0',
	`startup_tab` varchar(20) NOT NULL DEFAULT '',
	`transaction_days` smallint(6) NOT NULL DEFAULT '30',
	`save_report_selections` smallint(6) NOT NULL DEFAULT '0',
	`use_date_picker` tinyint(1) NOT NULL DEFAULT '1',
	`def_print_destination` tinyint(1) NOT NULL DEFAULT '0',
	`def_print_orientation` tinyint(1) NOT NULL DEFAULT '0',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `login_id` (`login_id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_users` --

INSERT INTO `0_users` VALUES
('1', 'admin', '5f4dcc3b5aa765d61d8327deb882cf99', 'Administrator', '2', '', 'adm@example.com', 'C', '0', '0', '0', '0', 'default', 'Letter', '2', '2', '4', '1', '1', '0', '0', '2025-05-07 13:58:33', '10', '1', '1', '1', '1', '0', 'orders', '30', '0', '1', '0', '0', '0');

-- Structure of table `0_voided` --

DROP TABLE IF EXISTS `0_voided`;

CREATE TABLE `0_voided` (
	`type` int(11) NOT NULL DEFAULT '0',
	`id` int(11) NOT NULL DEFAULT '0',
	`date_` date NOT NULL DEFAULT '0000-00-00',
	`memo_` tinytext NOT NULL,
	UNIQUE KEY `id` (`type`,`id`)
) ENGINE=InnoDB ;

-- Data of table `0_voided` --


-- Structure of table `0_wo_costing` --

DROP TABLE IF EXISTS `0_wo_costing`;

CREATE TABLE `0_wo_costing` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`workorder_id` int(11) NOT NULL DEFAULT '0',
	`cost_type` tinyint(1) NOT NULL DEFAULT '0',
	`trans_type` int(11) NOT NULL DEFAULT '0',
	`trans_no` int(11) NOT NULL DEFAULT '0',
	`factor` double NOT NULL DEFAULT '1',
	PRIMARY KEY (`id`)
) ENGINE=InnoDB ;

-- Data of table `0_wo_costing` --

-- Structure of table `0_wo_issue_items` --

DROP TABLE IF EXISTS `0_wo_issue_items`;

CREATE TABLE `0_wo_issue_items` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`stock_id` varchar(40) DEFAULT NULL,
	`issue_id` int(11) DEFAULT NULL,
	`qty_issued` double DEFAULT NULL,
	`unit_cost` double NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`)
) ENGINE=InnoDB ;

-- Data of table `0_wo_issue_items` --


-- Structure of table `0_wo_issues` --

DROP TABLE IF EXISTS `0_wo_issues`;

CREATE TABLE `0_wo_issues` (
	`issue_no` int(11) NOT NULL AUTO_INCREMENT,
	`workorder_id` int(11) NOT NULL DEFAULT '0',
	`reference` varchar(100) DEFAULT NULL,
	`issue_date` date DEFAULT NULL,
	`loc_code` varchar(5) DEFAULT NULL,
	`workcentre_id` int(11) DEFAULT NULL,
	PRIMARY KEY (`issue_no`),
	KEY `workorder_id` (`workorder_id`)
) ENGINE=InnoDB ;

-- Data of table `0_wo_issues` --

-- Structure of table `0_wo_manufacture` --

DROP TABLE IF EXISTS `0_wo_manufacture`;

CREATE TABLE `0_wo_manufacture` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`reference` varchar(100) DEFAULT NULL,
	`workorder_id` int(11) NOT NULL DEFAULT '0',
	`quantity` double NOT NULL DEFAULT '0',
	`date_` date NOT NULL DEFAULT '0000-00-00',
	PRIMARY KEY (`id`),
	KEY `workorder_id` (`workorder_id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_wo_manufacture` --

INSERT INTO `0_wo_manufacture` VALUES
('1', '001/2025', '1', '2', '2025-05-05');

-- Structure of table `0_wo_requirements` --

DROP TABLE IF EXISTS `0_wo_requirements`;

CREATE TABLE `0_wo_requirements` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`workorder_id` int(11) NOT NULL DEFAULT '0',
	`stock_id` char(20) NOT NULL DEFAULT '',
	`workcentre` int(11) NOT NULL DEFAULT '0',
	`units_req` double NOT NULL DEFAULT '1',
	`unit_cost` double NOT NULL DEFAULT '0',
	`loc_code` char(5) NOT NULL DEFAULT '',
	`units_issued` double NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	KEY `workorder_id` (`workorder_id`)
) ENGINE=InnoDB AUTO_INCREMENT=9 ;

-- Data of table `0_wo_requirements` --

INSERT INTO `0_wo_requirements` VALUES
('1', '1', '101', '1', '1', '200', 'DEF', '2'),
('2', '1', '102', '1', '1', '150', 'DEF', '2'),
('3', '1', '103', '1', '1', '10', 'DEF', '2'),
('4', '1', '301', '1', '1', '0', 'DEF', '2'),
('5', '2', '101', '1', '1', '200', 'DEF', '0'),
('6', '2', '102', '1', '1', '150', 'DEF', '0'),
('7', '2', '103', '1', '1', '10', 'DEF', '0'),
('8', '2', '301', '1', '1', '0', 'DEF', '0');

-- Structure of table `0_workcentres` --

DROP TABLE IF EXISTS `0_workcentres`;

CREATE TABLE `0_workcentres` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`name` char(40) NOT NULL DEFAULT '',
	`description` char(50) NOT NULL DEFAULT '',
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	PRIMARY KEY (`id`),
	UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_workcentres` --

INSERT INTO `0_workcentres` VALUES
('1', 'Work Centre', '', '0');

-- Structure of table `0_workorders` --

DROP TABLE IF EXISTS `0_workorders`;

CREATE TABLE `0_workorders` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`wo_ref` varchar(60) NOT NULL DEFAULT '',
	`loc_code` varchar(5) NOT NULL DEFAULT '',
	`units_reqd` double NOT NULL DEFAULT '1',
	`stock_id` varchar(20) NOT NULL DEFAULT '',
	`date_` date NOT NULL DEFAULT '0000-00-00',
	`type` tinyint(4) NOT NULL DEFAULT '0',
	`required_by` date NOT NULL DEFAULT '0000-00-00',
	`released_date` date NOT NULL DEFAULT '0000-00-00',
	`units_issued` double NOT NULL DEFAULT '0',
	`closed` tinyint(1) NOT NULL DEFAULT '0',
	`released` tinyint(1) NOT NULL DEFAULT '0',
	`additional_costs` double NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `wo_ref` (`wo_ref`)
) ENGINE=InnoDB AUTO_INCREMENT=4 ;

-- Data of table `0_workorders` --

INSERT INTO `0_workorders` VALUES
('1', '001/2025', 'DEF', '2', '201', '2025-05-05', '0', '2025-05-05', '2025-05-05', '2', '1', '1', '0', '{}'),
('2', '002/2025', 'DEF', '5', '201', '2025-05-07', '2', '2025-05-27', '2025-05-07', '0', '0', '1', '0', '{}'),
('3', '003/2025', 'DEF', '5', '201', '2025-05-07', '2', '2025-05-27', '0000-00-00', '0', '0', '0', '0', '{}');

-- ============================================================
-- ATTENDANCE DEDUCTION RULES
-- ============================================================

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

-- ============================================================
-- DEDUCTION CODES
-- ============================================================

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

-- ============================================================
-- DOCUMENT TYPES
-- ============================================================

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

-- ============================================================
-- EMPLOYEE DEPENDENTS
-- ============================================================

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

-- ============================================================
-- EMPLOYEE DOCUMENTS
-- ============================================================

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

-- ============================================================
-- EMPLOYEE HISTORY
-- ============================================================

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

-- ============================================================
-- EMPLOYEE LOANS
-- ============================================================

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

-- ============================================================
-- EMPLOYEE SALARY (personal overrides)
-- ============================================================

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

-- ============================================================
-- EOS CALCULATION (End of Service tiers)
-- ============================================================

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

-- ============================================================
-- HOLIDAYS
-- ============================================================

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

-- ============================================================
-- HR SETTINGS
-- ============================================================

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

-- ============================================================
-- LEAVE BALANCES
-- ============================================================

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

-- ============================================================
-- LEAVE POLICIES
-- ============================================================

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

-- ============================================================
-- LEAVE REQUESTS
-- ============================================================

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

-- ============================================================
-- LOAN REPAYMENTS
-- ============================================================

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

-- ============================================================
-- LOAN TYPES
-- ============================================================

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

-- ============================================================
-- OVERTIME BUDGET
-- ============================================================

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

-- ============================================================
-- OVERTIME REQUESTS
-- ============================================================

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

-- ============================================================
-- PAYROLL PERIODS
-- ============================================================

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

-- ============================================================
-- PAYSLIP DETAILS (line items per payslip)
-- ============================================================

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

-- ============================================================
-- STATUTORY DEDUCTIONS (social insurance, pension, etc.)
-- ============================================================

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

-- ============================================================
-- TAX BRACKETS (configurable income tax slabs)
-- ============================================================

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

-- ============================================================
-- WORK SHIFTS
-- ============================================================

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

-- ============================================================
-- WORKING DAYS (weekly configuration)
-- ============================================================

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

-- ============================================================
-- RECRUITMENT OPENINGS
-- ============================================================

-- Structure of table `0_recruitment_openings` --

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

-- Data of table `0_recruitment_openings` --

-- ============================================================
-- RECRUITMENT APPLICANTS
-- ============================================================

-- Structure of table `0_recruitment_applicants` --

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

-- Data of table `0_recruitment_applicants` --

-- ============================================================
-- TRAINING COURSES
-- ============================================================

-- Structure of table `0_training_courses` --

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

-- Data of table `0_training_courses` --

-- ============================================================
-- EMPLOYEE TRAINING RECORDS
-- ============================================================

-- Structure of table `0_employee_training` --

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

-- Data of table `0_employee_training` --

-- ============================================================
-- EMPLOYEE APPRAISALS
-- ============================================================

-- Structure of table `0_employee_appraisals` --

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

-- Data of table `0_employee_appraisals` --

-- ============================================================
-- EMPLOYEE ASSET ALLOCATION
-- ============================================================

-- Structure of table `0_employee_asset_allocation` --

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

-- Data of table `0_employee_asset_allocation` --

-- =============================================================
-- END OF NEW HRM TABLES
-- =============================================================

-- =====================================================
-- Table: approval_workflows
-- Defines which transaction types have approval enabled
-- =====================================================
DROP TABLE IF EXISTS `0_approval_workflows`;

CREATE TABLE IF NOT EXISTS `0_approval_workflows` (
	`id`              INT(11)      NOT NULL AUTO_INCREMENT,
	`trans_type`      INT(11)      NOT NULL,
	`name`            VARCHAR(100) NOT NULL DEFAULT '',
	`description`     VARCHAR(255) NOT NULL DEFAULT '',
	`is_active`       TINYINT(1)   NOT NULL DEFAULT 1,
	`require_comments_on_reject` TINYINT(1) NOT NULL DEFAULT 1,
	`require_comments_on_approve` TINYINT(1) NOT NULL DEFAULT 0,
	`allow_edit_on_approve` TINYINT(1) NOT NULL DEFAULT 0,
	`allow_self_approve` TINYINT(1) NOT NULL DEFAULT 0,
	`created_by`      SMALLINT(6)  NOT NULL,
	`created_date`    DATETIME     NOT NULL,
	`modified_by`     SMALLINT(6)  DEFAULT NULL,
	`modified_date`   DATETIME     DEFAULT NULL,
	`version`         INT(11)      NOT NULL DEFAULT 1,
	PRIMARY KEY (`id`),
	UNIQUE KEY `trans_type` (`trans_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- =====================================================
-- Table: approval_levels
-- Multi-level approval configuration per workflow
-- =====================================================
DROP TABLE IF EXISTS `0_approval_levels`;

CREATE TABLE IF NOT EXISTS `0_approval_levels` (
	`id`              INT(11)      NOT NULL AUTO_INCREMENT,
	`workflow_id`     INT(11)      NOT NULL,
	`level`           SMALLINT(6)  NOT NULL,
	`role_id`         INT(11)      NOT NULL,
	`min_approvers`   SMALLINT(6)  NOT NULL DEFAULT 1,
	`amount_threshold` DOUBLE      NOT NULL DEFAULT 0 COMMENT 'Auto-approve if trans amount <= this value',
	`amount_upper`    DOUBLE       NOT NULL DEFAULT 0 COMMENT 'This level required only if amount > threshold of previous level',
	`escalation_days` SMALLINT(6)  NOT NULL DEFAULT 0 COMMENT '0 = no escalation',
	`escalation_to_level` SMALLINT(6) DEFAULT NULL COMMENT 'Level to escalate to after timeout',
	`loc_code`        VARCHAR(20)  DEFAULT NULL COMMENT 'Location restriction (NULL = all locations)',
	`conditions` 	  TEXT         DEFAULT NULL COMMENT 'JSON conditions for level applicability',
	`is_active`       TINYINT(1)   NOT NULL DEFAULT 1,
	PRIMARY KEY (`id`),
	UNIQUE KEY `workflow_role_level` (`workflow_id`, `level`, `role_id`),
	KEY `workflow_level` (`workflow_id`, `level`),
	KEY `role_id` (`role_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- =====================================================
-- Table: approval_drafts
-- Stores pending/processed draft transactions
-- =====================================================
DROP TABLE IF EXISTS `0_approval_drafts`;

CREATE TABLE IF NOT EXISTS `0_approval_drafts` (
	`id`              INT(11)      NOT NULL AUTO_INCREMENT,
	`workflow_id`     INT(11)      NOT NULL,
	`trans_type`      INT(11)      NOT NULL,
	`reserved_trans_no` INT(11)    NOT NULL COMMENT 'Reserved number from transaction sequence',
	`draft_data`      MEDIUMTEXT   NOT NULL COMMENT 'JSON-encoded transaction data',
	`amount`          DOUBLE       NOT NULL DEFAULT 0,
	`currency`        VARCHAR(3)   DEFAULT NULL,
	`person_type_id`  INT(11)      DEFAULT NULL,
	`person_id`       VARCHAR(60)  DEFAULT NULL,
	`current_level`   SMALLINT(6)  NOT NULL DEFAULT 0,
	`status`          TINYINT(2)   NOT NULL DEFAULT 0 COMMENT '0=pending,1=approved,2=rejected,3=cancelled,4=expired,5=delegated',
	`submitted_by`    SMALLINT(6)  NOT NULL,
	`submitted_date`  DATETIME     NOT NULL,
	`approved_trans_no` INT(11)    DEFAULT NULL COMMENT 'Actual trans_no after approval execution',
	`completed_date`  DATETIME     DEFAULT NULL,
	`workflow_version` INT(11)     NOT NULL DEFAULT 1 COMMENT 'Snapshot of workflow version at submission',
	`reference`       VARCHAR(100) DEFAULT NULL COMMENT 'Generated reference string',
	`summary`         VARCHAR(255) DEFAULT NULL COMMENT 'Human-readable summary of draft',
	`loc_code`        VARCHAR(20)  DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_status` (`status`),
	KEY `idx_trans_type` (`trans_type`),
	KEY `idx_submitted_by` (`submitted_by`),
	KEY `idx_reserved_no` (`trans_type`, `reserved_trans_no`),
	KEY `idx_workflow` (`workflow_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- =====================================================
-- Table: approval_actions
-- Complete audit trail of all approval workflow actions
-- =====================================================
DROP TABLE IF EXISTS `0_approval_actions`;

CREATE TABLE IF NOT EXISTS `0_approval_actions` (
	`id`              INT(11)      NOT NULL AUTO_INCREMENT,
	`draft_id`        INT(11)      NOT NULL,
	`action_type`     VARCHAR(20)  NOT NULL COMMENT 'submit,approve,reject,delegate,escalate,cancel,edit,resubmit',
	`user_id`         SMALLINT(6)  NOT NULL,
	`level`           SMALLINT(6)  NOT NULL DEFAULT 0,
	`comments`        TEXT         DEFAULT NULL,
	`draft_data_snapshot` MEDIUMTEXT DEFAULT NULL COMMENT 'JSON snapshot if edited',
	`action_date`     DATETIME     NOT NULL,
	`ip_address`      VARCHAR(45)  DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_draft` (`draft_id`),
	KEY `idx_user` (`user_id`),
	KEY `idx_date` (`action_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- =====================================================
-- Table: approval_delegations
-- Active delegation assignments
-- =====================================================
DROP TABLE IF EXISTS `0_approval_delegations`;

CREATE TABLE IF NOT EXISTS `0_approval_delegations` (
	`id`              INT(11)      NOT NULL AUTO_INCREMENT,
	`from_user_id`    SMALLINT(6)  NOT NULL,
	`to_user_id`      SMALLINT(6)  NOT NULL,
	`trans_type`      INT(11)      DEFAULT NULL COMMENT 'NULL = all types',
	`from_date`       DATE         NOT NULL,
	`to_date`         DATE         DEFAULT NULL,
	`reason`          VARCHAR(255) DEFAULT NULL,
	`is_active`       TINYINT(1)   NOT NULL DEFAULT 1,
	`created_date`    DATETIME     NOT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_active` (`is_active`, `from_date`, `to_date`),
	KEY `idx_from_user` (`from_user_id`),
	KEY `idx_to_user` (`to_user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- =====================================================
-- Table: approval_notifications
-- Notification queue for approval events
-- =====================================================
DROP TABLE IF EXISTS `0_approval_notifications`;

CREATE TABLE IF NOT EXISTS `0_approval_notifications` (
	`id`              INT(11)      NOT NULL AUTO_INCREMENT,
	`draft_id`        INT(11)      NOT NULL,
	`user_id`         SMALLINT(6)  NOT NULL,
	`notification_type` VARCHAR(30) NOT NULL COMMENT 'pending,approved,rejected,escalated,delegated',
	`is_read`         TINYINT(1)   NOT NULL DEFAULT 0,
	`is_sent`         TINYINT(1)   NOT NULL DEFAULT 0 COMMENT 'Email sent flag',
	`created_date`    DATETIME     NOT NULL,
	`read_date`       DATETIME     DEFAULT NULL,
	PRIMARY KEY (`id`),
	KEY `idx_user_unread` (`user_id`, `is_read`),
	KEY `idx_unsent` (`is_sent`),
	KEY `idx_draft` (`draft_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ================================================================
-- NotrinosERP CRM Module -- Database Schema
-- Version: 1.0
-- ================================================================

-- Add use_crm preference to sys_prefs if not present
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_crm', 'setup.company', 'tinyint', 1, '0');

-- ================================================================
-- CONFIGURATION TABLES
-- ================================================================

-- Lead Sources (where leads come from)
DROP TABLE IF EXISTS `0_crm_lead_sources`;
CREATE TABLE `0_crm_lead_sources` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(100) NOT NULL,
  `description` varchar(255) DEFAULT NULL,
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `0_crm_lead_sources` (`name`, `description`, `active`) VALUES
('Website', 'Company website contact form', 1),
('Cold Call', 'Outbound cold calling', 1),
('Email', 'Inbound email inquiry', 1),
('Referral', 'Customer or partner referral', 1),
('Advertisement', 'Paid advertising', 1),
('Trade Show', 'Trade shows and exhibitions', 1),
('Social Media', 'Social media channels', 1),
('Employee Referral', 'Internal employee referral', 1),
('Walk-in', 'Walk-in customer', 1),
('Other', 'Other sources', 1);

-- Pipeline Stages (configurable opportunity stages)
DROP TABLE IF EXISTS `0_crm_sales_stages`;
CREATE TABLE `0_crm_sales_stages` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(100) NOT NULL,
  `sequence` int(11) NOT NULL DEFAULT 0,
  `probability` int(3) NOT NULL DEFAULT 0,
  `description` varchar(255) DEFAULT NULL,
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `0_crm_sales_stages` (`name`, `sequence`, `probability`, `description`, `active`) VALUES
('New', 1, 10, 'Newly identified opportunity', 1),
('Qualification', 2, 20, 'Evaluating fit and interest', 1),
('Needs Analysis', 3, 40, 'Understanding customer requirements', 1),
('Value Proposition', 4, 60, 'Presenting solution and value', 1),
('Decision Makers', 5, 70, 'Engaging decision makers', 1),
('Proposal', 6, 80, 'Formal proposal submitted', 1),
('Negotiation', 7, 90, 'Negotiating terms and pricing', 1),
('Won', 8, 100, 'Deal closed successfully', 1),
('Lost', 9, 0, 'Deal lost', 1);

-- Lost Reasons
DROP TABLE IF EXISTS `0_crm_lost_reasons`;
CREATE TABLE `0_crm_lost_reasons` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `description` varchar(255) NOT NULL,
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `0_crm_lost_reasons` (`description`, `active`) VALUES
('Too expensive', 1),
('Chose competitor', 1),
('No budget', 1),
('No response', 1),
('Bad timing', 1),
('Requirements not met', 1),
('Contact lost', 1),
('Other', 1);

-- Activity Types
DROP TABLE IF EXISTS `0_crm_activity_types`;
CREATE TABLE `0_crm_activity_types` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(100) NOT NULL,
  `category` varchar(20) NOT NULL DEFAULT 'todo' COMMENT 'call, email, meeting, todo, upload',
  `icon` varchar(50) DEFAULT NULL,
  `default_user_id` int(11) DEFAULT NULL,
  `default_summary` varchar(255) DEFAULT NULL,
  `chaining_type` varchar(10) NOT NULL DEFAULT 'none' COMMENT 'none, suggest, trigger',
  `chained_activity_type_id` int(11) DEFAULT NULL,
  `schedule_days` int(11) DEFAULT 0,
  `schedule_type` varchar(20) DEFAULT 'completion' COMMENT 'completion, deadline',
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `0_crm_activity_types` (`name`, `category`, `icon`, `chaining_type`, `active`) VALUES
('Email', 'email', 'fa-envelope', 'none', 1),
('Phone Call', 'call', 'fa-phone', 'suggest', 1),
('Meeting', 'meeting', 'fa-calendar', 'none', 1),
('To-Do', 'todo', 'fa-check', 'none', 1),
('Upload Document', 'upload', 'fa-upload', 'none', 1),
('Follow-up', 'todo', 'fa-redo', 'none', 1),
('Demo/Presentation', 'meeting', 'fa-desktop', 'none', 1),
('Site Visit', 'meeting', 'fa-building', 'none', 1);

-- Update Phone Call to suggest Follow-up
UPDATE `0_crm_activity_types` SET
  `chained_activity_type_id` = (SELECT `id` FROM (SELECT `id` FROM `0_crm_activity_types` WHERE `name` = 'Follow-up') t),
  `schedule_days` = 3
WHERE `name` = 'Phone Call';

-- Appointment Types
DROP TABLE IF EXISTS `0_crm_appointment_types`;
CREATE TABLE `0_crm_appointment_types` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(100) NOT NULL,
  `default_duration` int(11) NOT NULL DEFAULT 60 COMMENT 'minutes',
  `location` varchar(255) DEFAULT NULL,
  `video_link` varchar(255) DEFAULT NULL,
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `0_crm_appointment_types` (`name`, `default_duration`, `active`) VALUES
('General Meeting', 60, 1),
('Product Demo', 45, 1),
('Discovery Call', 30, 1),
('Follow-up Meeting', 30, 1),
('Contract Review', 60, 1);

-- CRM tags use the shared 0_tags table (type=3, TAG_CRM).
-- Seed data is inserted above in the 0_tags section.

-- ================================================================
-- SALES TEAM TABLES
-- ================================================================

DROP TABLE IF EXISTS `0_crm_sales_teams`;
CREATE TABLE `0_crm_sales_teams` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(100) NOT NULL,
  `team_leader_id` int(11) DEFAULT NULL COMMENT 'FK to 0_users',
  `email_alias` varchar(100) DEFAULT NULL,
  `invoicing_target` decimal(14,2) DEFAULT 0.00,
  `use_leads` tinyint(1) NOT NULL DEFAULT 1,
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

DROP TABLE IF EXISTS `0_crm_sales_team_members`;
CREATE TABLE `0_crm_sales_team_members` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `team_id` int(11) NOT NULL,
  `user_id` int(11) NOT NULL,
  `max_leads_30days` int(11) DEFAULT 30,
  `skip_auto_assign` tinyint(1) NOT NULL DEFAULT 0,
  `domain_filter` text DEFAULT NULL COMMENT 'JSON assignment rule filter',
  PRIMARY KEY (`id`),
  UNIQUE KEY `team_member` (`team_id`, `user_id`),
  KEY `team_id` (`team_id`),
  KEY `user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ================================================================
-- CORE ENTITY TABLES
-- ================================================================

-- Unified Lead/Opportunity table (Odoo pattern: is_opportunity flag)
DROP TABLE IF EXISTS `0_crm_leads`;
CREATE TABLE `0_crm_leads` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `lead_ref` varchar(30) DEFAULT NULL,
  `title` varchar(255) NOT NULL,
  `company_name` varchar(200) DEFAULT NULL,
  `contact_name` varchar(200) DEFAULT NULL,
  `email` varchar(200) DEFAULT NULL,
  `phone` varchar(50) DEFAULT NULL,
  `mobile` varchar(50) DEFAULT NULL,
  `website` varchar(255) DEFAULT NULL,
  `address` text DEFAULT NULL,
  `city` varchar(100) DEFAULT NULL,
  `state` varchar(100) DEFAULT NULL,
  `country` varchar(100) DEFAULT NULL,
  `postal_code` varchar(20) DEFAULT NULL,
  `lead_source_id` int(11) DEFAULT NULL,
  `lead_status` varchar(20) NOT NULL DEFAULT 'new' COMMENT 'new, contacted, qualified, converted, lost',
  `priority` tinyint(1) NOT NULL DEFAULT 0 COMMENT '0=Normal, 1=Low, 2=Medium, 3=High',
  `is_opportunity` tinyint(1) NOT NULL DEFAULT 0,
  `stage_id` int(11) DEFAULT NULL COMMENT 'FK to crm_sales_stages',
  `assigned_to` int(11) DEFAULT NULL COMMENT 'FK to 0_users',
  `sales_team_id` int(11) DEFAULT NULL,
  `expected_revenue` decimal(14,2) DEFAULT 0.00,
  `probability` int(3) DEFAULT 10 COMMENT '0-100 percent',
  `expected_close_date` date DEFAULT NULL,
  `date_created` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `date_qualified` datetime DEFAULT NULL,
  `date_converted` datetime DEFAULT NULL,
  `date_won` datetime DEFAULT NULL,
  `date_lost` datetime DEFAULT NULL,
  `lost_reason_id` int(11) DEFAULT NULL,
  `lost_notes` text DEFAULT NULL,
  `linked_customer_id` int(11) DEFAULT NULL COMMENT 'FK to 0_debtors_master',
  `linked_person_id` int(11) DEFAULT NULL COMMENT 'FK to 0_crm_persons',
  `created_by` int(11) DEFAULT NULL,
  `notes` text DEFAULT NULL,
  `inactive` tinyint(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `lead_ref` (`lead_ref`),
  KEY `lead_status` (`lead_status`),
  KEY `is_opportunity` (`is_opportunity`),
  KEY `stage_id` (`stage_id`),
  KEY `assigned_to` (`assigned_to`),
  KEY `sales_team_id` (`sales_team_id`),
  KEY `lead_source_id` (`lead_source_id`),
  KEY `expected_close_date` (`expected_close_date`),
  KEY `linked_customer_id` (`linked_customer_id`),
  KEY `date_created` (`date_created`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- CRM entity-tag associations use the shared 0_tag_associations table.
-- record_id format: "entity_type:entity_id" (e.g. "lead:42").

-- ================================================================
-- ACTIVITY TABLES
-- ================================================================

DROP TABLE IF EXISTS `0_crm_activities`;
CREATE TABLE `0_crm_activities` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `entity_type` varchar(20) NOT NULL DEFAULT 'lead' COMMENT 'lead, contact, customer',
  `entity_id` int(11) NOT NULL,
  `activity_type_id` int(11) NOT NULL,
  `summary` varchar(255) DEFAULT NULL,
  `description` text DEFAULT NULL,
  `date_scheduled` datetime NOT NULL,
  `date_completed` datetime DEFAULT NULL,
  `assigned_to` int(11) DEFAULT NULL COMMENT 'FK to 0_users',
  `created_by` int(11) DEFAULT NULL,
  `created_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `status` varchar(20) NOT NULL DEFAULT 'planned' COMMENT 'planned, done, cancelled, overdue',
  `outcome` varchar(100) DEFAULT NULL COMMENT 'positive, neutral, negative',
  `duration_minutes` int(11) DEFAULT NULL,
  `next_activity_id` int(11) DEFAULT NULL COMMENT 'FK to self, chained activity',
  PRIMARY KEY (`id`),
  KEY `entity` (`entity_type`, `entity_id`),
  KEY `assigned_to` (`assigned_to`),
  KEY `status` (`status`),
  KEY `date_scheduled` (`date_scheduled`),
  KEY `activity_type_id` (`activity_type_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Activity Plans (pre-configured sequences)
DROP TABLE IF EXISTS `0_crm_activity_plans`;
CREATE TABLE `0_crm_activity_plans` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `plan_name` varchar(100) NOT NULL,
  `description` varchar(255) DEFAULT NULL,
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

DROP TABLE IF EXISTS `0_crm_activity_plan_lines`;
CREATE TABLE `0_crm_activity_plan_lines` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `plan_id` int(11) NOT NULL,
  `activity_type_id` int(11) NOT NULL,
  `summary` varchar(255) DEFAULT NULL,
  `assignment` varchar(20) NOT NULL DEFAULT 'ask_at_launch' COMMENT 'ask_at_launch, default_user',
  `assigned_to` int(11) DEFAULT NULL,
  `interval_value` int(11) NOT NULL DEFAULT 0,
  `interval_unit` varchar(10) NOT NULL DEFAULT 'days' COMMENT 'days, weeks, months',
  `trigger_timing` varchar(10) NOT NULL DEFAULT 'after' COMMENT 'before, after',
  `sequence` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `plan_id` (`plan_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ================================================================
-- COMMUNICATION LOG
-- ================================================================

DROP TABLE IF EXISTS `0_crm_communication_log`;
CREATE TABLE `0_crm_communication_log` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `entity_type` varchar(20) NOT NULL DEFAULT 'lead' COMMENT 'lead, contact, customer',
  `entity_id` int(11) NOT NULL,
  `comm_type` varchar(20) NOT NULL COMMENT 'email, call, meeting, note, sms',
  `subject` varchar(255) DEFAULT NULL,
  `body` text DEFAULT NULL,
  `direction` varchar(10) DEFAULT NULL COMMENT 'inbound, outbound',
  `from_address` varchar(200) DEFAULT NULL,
  `to_address` varchar(200) DEFAULT NULL,
  `date_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` int(11) DEFAULT NULL,
  `duration_minutes` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `entity` (`entity_type`, `entity_id`),
  KEY `date_time` (`date_time`),
  KEY `comm_type` (`comm_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ================================================================
-- CAMPAIGN TABLES
-- ================================================================

DROP TABLE IF EXISTS `0_crm_campaigns`;
CREATE TABLE `0_crm_campaigns` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(200) NOT NULL,
  `campaign_type` varchar(50) DEFAULT NULL COMMENT 'email, event, social, referral, other',
  `start_date` date DEFAULT NULL,
  `end_date` date DEFAULT NULL,
  `status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft, active, completed, cancelled',
  `budget` decimal(14,2) DEFAULT 0.00,
  `assigned_to` int(11) DEFAULT NULL,
  `lead_source_id` int(11) DEFAULT NULL COMMENT 'auto-tag leads from this source',
  `description` text DEFAULT NULL,
  `created_by` int(11) DEFAULT NULL,
  `created_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `status` (`status`),
  KEY `start_date` (`start_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Email Templates
DROP TABLE IF EXISTS `0_crm_email_templates`;
CREATE TABLE `0_crm_email_templates` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(100) NOT NULL,
  `subject` varchar(255) NOT NULL,
  `body_html` text NOT NULL,
  `category` varchar(50) DEFAULT NULL,
  `variables_json` text DEFAULT NULL COMMENT 'JSON list of available variables',
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Campaign Email Schedule
DROP TABLE IF EXISTS `0_crm_campaign_emails`;
CREATE TABLE `0_crm_campaign_emails` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `campaign_id` int(11) NOT NULL,
  `email_template_id` int(11) DEFAULT NULL,
  `day_offset` int(11) NOT NULL DEFAULT 0 COMMENT 'days from campaign start',
  `subject` varchar(255) DEFAULT NULL,
  `body` text DEFAULT NULL,
  `sequence` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `campaign_id` (`campaign_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- Campaign-Lead tracking (which leads are enrolled in which campaigns)
DROP TABLE IF EXISTS `0_crm_campaign_leads`;
CREATE TABLE `0_crm_campaign_leads` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `campaign_id` int(11) NOT NULL,
  `lead_id` int(11) NOT NULL,
  `enrolled_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `status` varchar(20) NOT NULL DEFAULT 'active' COMMENT 'active, unsubscribed, completed',
  `last_email_sent` int(11) DEFAULT NULL COMMENT 'sequence number of last email sent',
  PRIMARY KEY (`id`),
  UNIQUE KEY `campaign_lead` (`campaign_id`, `lead_id`),
  KEY `lead_id` (`lead_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ================================================================
-- CONTRACT TABLE
-- ================================================================

DROP TABLE IF EXISTS `0_crm_contracts`;
CREATE TABLE `0_crm_contracts` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `contract_ref` varchar(30) DEFAULT NULL,
  `customer_id` int(11) DEFAULT NULL COMMENT 'FK to 0_debtors_master',
  `lead_id` int(11) DEFAULT NULL COMMENT 'FK to 0_crm_leads',
  `title` varchar(255) NOT NULL,
  `description` text DEFAULT NULL,
  `contract_value` decimal(14,2) DEFAULT 0.00,
  `start_date` date DEFAULT NULL,
  `end_date` date DEFAULT NULL,
  `renewal_alert_days` int(11) DEFAULT 30,
  `status` varchar(20) NOT NULL DEFAULT 'draft' COMMENT 'draft, active, expired, cancelled, renewed',
  `signed_by` varchar(200) DEFAULT NULL,
  `signed_date` date DEFAULT NULL,
  `created_by` int(11) DEFAULT NULL,
  `created_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `contract_ref` (`contract_ref`),
  KEY `customer_id` (`customer_id`),
  KEY `status` (`status`),
  KEY `end_date` (`end_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ================================================================
-- APPOINTMENT TABLE
-- ================================================================

DROP TABLE IF EXISTS `0_crm_appointments`;
CREATE TABLE `0_crm_appointments` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `title` varchar(255) NOT NULL,
  `appointment_type_id` int(11) DEFAULT NULL,
  `lead_id` int(11) DEFAULT NULL,
  `customer_id` int(11) DEFAULT NULL,
  `contact_person_id` int(11) DEFAULT NULL COMMENT 'FK to 0_crm_persons',
  `date_time` datetime NOT NULL,
  `duration_minutes` int(11) NOT NULL DEFAULT 60,
  `location` varchar(255) DEFAULT NULL,
  `video_link` varchar(255) DEFAULT NULL,
  `attendees_json` text DEFAULT NULL COMMENT 'JSON list of internal user IDs',
  `notes` text DEFAULT NULL,
  `status` varchar(20) NOT NULL DEFAULT 'scheduled' COMMENT 'scheduled, confirmed, completed, cancelled, no_show',
  `reminder_sent` tinyint(1) NOT NULL DEFAULT 0,
  `created_by` int(11) DEFAULT NULL,
  `created_date` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `lead_id` (`lead_id`),
  KEY `customer_id` (`customer_id`),
  KEY `date_time` (`date_time`),
  KEY `status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ================================================================
-- LEAD SCORING CONFIG
-- ================================================================

DROP TABLE IF EXISTS `0_crm_scoring_config`;
CREATE TABLE `0_crm_scoring_config` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `variable_name` varchar(50) NOT NULL COMMENT 'source, country, email_quality, phone_quality, tags',
  `weight` decimal(5,2) NOT NULL DEFAULT 1.00,
  `active` tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  UNIQUE KEY `variable_name` (`variable_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `0_crm_scoring_config` (`variable_name`, `weight`, `active`) VALUES
('source', 1.00, 1),
('country', 0.50, 1),
('email_quality', 1.50, 1),
('phone_quality', 1.00, 1),
('tags', 0.75, 0);

-- ================================================================
-- CRM MODULE SETTINGS
-- ================================================================

DROP TABLE IF EXISTS `0_crm_module_settings`;
CREATE TABLE `0_crm_module_settings` (
  `setting_key` varchar(50) NOT NULL,
  `setting_value` text DEFAULT NULL,
  `description` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`setting_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT INTO `0_crm_module_settings` (`setting_key`, `setting_value`, `description`) VALUES
('auto_create_contact', '1', 'Automatically create contact record for each new lead'),
('carry_forward_communication', '1', 'Copy communication history when lead converts to customer'),
('auto_close_days', '90', 'Auto-close stale opportunities after N days (0=disabled)'),
('lead_ref_prefix', 'LD', 'Prefix for auto-generated lead reference numbers'),
('opportunity_ref_prefix', 'OP', 'Prefix for auto-generated opportunity reference numbers'),
('contract_ref_prefix', 'CT', 'Prefix for auto-generated contract reference numbers'),
('default_probability', '10', 'Default probability for new leads (percent)'),
('enable_lead_scoring', '0', 'Enable predictive lead scoring'),
('scoring_start_date', '', 'Consider leads created after this date for scoring'),
('assignment_mode', 'manual', 'Lead assignment mode: manual or auto');

SET @old_foreign_key_checks = @@FOREIGN_KEY_CHECKS;
SET FOREIGN_KEY_CHECKS = 0;


-- Indexes for new stock_moves columns (use CREATE INDEX IF NOT EXISTS alternative)
-- MariaDB 10.5+ supports this syntax
CREATE INDEX IF NOT EXISTS `idx_sm_serial_id`   ON `0_stock_moves` (`serial_id`);
CREATE INDEX IF NOT EXISTS `idx_sm_batch_id`    ON `0_stock_moves` (`batch_id`);
CREATE INDEX IF NOT EXISTS `idx_sm_from_bin_id` ON `0_stock_moves` (`from_bin_id`);
CREATE INDEX IF NOT EXISTS `idx_sm_to_bin_id`   ON `0_stock_moves` (`to_bin_id`);


-- -------------------------------------------------------------------------------------
-- Restore foreign key checks
-- -------------------------------------------------------------------------------------

SET FOREIGN_KEY_CHECKS = @old_foreign_key_checks;

-- ---------------------------------------------------------------------------
-- 1. Company preference settings for warranty provision GL accounts
-- ---------------------------------------------------------------------------

-- Warranty provision accrual account (Balance Sheet - Current Liability)
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('warranty_provision_account', 'tracking', 'VARCHAR', 15, '');

-- Warranty expense account (P&L - Warranty Expense)
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('warranty_expense_account', 'tracking', 'VARCHAR', 15, '');

-- Default warranty provision rate (% of item cost)
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('warranty_provision_rate', 'tracking', 'REAL', 8, '5.0');

-- Enable/disable automatic warranty provision posting
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('warranty_provision_enabled', 'tracking', 'TINYINT', 1, '0');

-- ---------------------------------------------------------------------------
-- 5. System preference: regulatory compliance master switch
-- ---------------------------------------------------------------------------
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('regulatory_compliance_enabled', 'tracking', 'TINYINT', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('dscsa_enabled', 'tracking', 'TINYINT', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('fsma204_enabled', 'tracking', 'TINYINT', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('udi_enabled', 'tracking', 'TINYINT', 1, '0');

-- Company license info for DSCSA transaction statements
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('dscsa_company_license', 'tracking', 'VARCHAR', 50, '');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('dscsa_company_dea', 'tracking', 'VARCHAR', 20, '');

-- FSMA 204 settings
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('fsma204_firm_name', 'tracking', 'VARCHAR', 100, '');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('fsma204_fda_registration', 'tracking', 'VARCHAR', 30, '');

-- UDI settings
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('udi_company_name', 'tracking', 'VARCHAR', 100, '');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('udi_issuing_agency', 'tracking', 'VARCHAR', 10, 'GS1');

-- Phase 4: Advanced Discount & Promotion Engine
INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_discount_programs', 'sales', 'tinyint', 1, '0');

-- ================================================================
-- Purchase Requisition module. Confusing fields: material_request_id links to WMS request; status/priority enums drive workflow states; custom_data stores future-safe metadata.
-- ================================================================

CREATE TABLE IF NOT EXISTS `0_purch_requisitions` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `reference` VARCHAR(60) NOT NULL,
  `material_request_id` INT NOT NULL DEFAULT 0,
  `requester_id` SMALLINT NOT NULL,
  `department_id` INT NOT NULL DEFAULT 0,
  `request_date` DATE NOT NULL,
  `required_date` DATE DEFAULT NULL,
  `status` ENUM('draft','submitted','approved','partially_ordered','ordered','rejected','cancelled') NOT NULL DEFAULT 'draft',
  `priority` ENUM('low','normal','high','urgent') NOT NULL DEFAULT 'normal',
  `purpose` TINYTEXT,
  `notes` TEXT,
  `total_estimated` DOUBLE NOT NULL DEFAULT 0,
  `approved_by` SMALLINT NOT NULL DEFAULT 0,
  `approved_date` DATETIME DEFAULT NULL,
  `rejected_by` SMALLINT NOT NULL DEFAULT 0,
  `rejected_date` DATETIME DEFAULT NULL,
  `rejection_reason` TINYTEXT,
  `location` VARCHAR(5) NOT NULL DEFAULT '',
  `dimension_id` INT NOT NULL DEFAULT 0,
  `dimension2_id` INT NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_requester` (`requester_id`),
  KEY `idx_status` (`status`),
  KEY `idx_date` (`request_date`),
  KEY `idx_material_request` (`material_request_id`),
  KEY `idx_department` (`department_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_requisition_lines` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `requisition_id` INT NOT NULL,
  `material_request_line_id` INT NOT NULL DEFAULT 0,
  `stock_id` VARCHAR(20) NOT NULL,
  `description` TINYTEXT,
  `quantity` DOUBLE NOT NULL,
  `unit_of_measure` VARCHAR(20) NOT NULL DEFAULT '',
  `estimated_unit_price` DOUBLE NOT NULL DEFAULT 0,
  `preferred_supplier_id` INT NOT NULL DEFAULT 0,
  `notes` TINYTEXT,
  `qty_ordered` DOUBLE NOT NULL DEFAULT 0,
  `po_number` INT NOT NULL DEFAULT 0,
  `status` ENUM('pending','approved','ordered','rejected') NOT NULL DEFAULT 'pending',
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_requisition` (`requisition_id`),
  KEY `idx_material_request_line` (`material_request_line_id`),
  KEY `idx_supplier` (`preferred_supplier_id`),
  KEY `idx_status` (`status`),
  CONSTRAINT `fk_purch_req_line_header`
    FOREIGN KEY (`requisition_id`) REFERENCES `0_purch_requisitions` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_purchase_requisitions', 'purchase', 'smallint', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('requisition_auto_approval_limit', 'purchase', 'int', 11, '0');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5378'
  ELSE CONCAT(`areas`, ';5378')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5378;%';

-- ================================================================
-- Request For Quotation module. Confusing structure: header in purch_rfq, invited vendors in purch_rfq_vendors, per-item quotes in purch_rfq_vendor_lines.
-- ================================================================

CREATE TABLE IF NOT EXISTS `0_purch_rfq` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `reference` VARCHAR(60) NOT NULL,
  `rfq_type` ENUM('standard','call_for_tender') NOT NULL DEFAULT 'standard',
  `status` ENUM('draft','sent','received','evaluated','awarded','expired','cancelled') NOT NULL DEFAULT 'draft',
  `created_by` SMALLINT NOT NULL,
  `created_date` DATE NOT NULL,
  `deadline_date` DATE DEFAULT NULL,
  `validity_date` DATE DEFAULT NULL,
  `requisition_id` INT NOT NULL DEFAULT 0,
  `description` TINYTEXT,
  `notes` TEXT,
  `terms_and_conditions` TEXT,
  `delivery_location` VARCHAR(5) NOT NULL DEFAULT '',
  `required_delivery_date` DATE DEFAULT NULL,
  `evaluation_criteria` TEXT,
  `dimension_id` INT NOT NULL DEFAULT 0,
  `dimension2_id` INT NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_status` (`status`),
  KEY `idx_date` (`created_date`),
  KEY `idx_requisition` (`requisition_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_rfq_items` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `rfq_id` INT NOT NULL,
  `stock_id` VARCHAR(20) NOT NULL,
  `description` TINYTEXT,
  `quantity` DOUBLE NOT NULL,
  `unit_of_measure` VARCHAR(20) NOT NULL DEFAULT '',
  `target_price` DOUBLE NOT NULL DEFAULT 0,
  `specifications` TEXT,
  `sort_order` INT NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_rfq` (`rfq_id`),
  KEY `idx_stock` (`stock_id`),
  CONSTRAINT `fk_purch_rfq_item_header`
    FOREIGN KEY (`rfq_id`) REFERENCES `0_purch_rfq` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_rfq_vendors` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `rfq_id` INT NOT NULL,
  `supplier_id` INT NOT NULL,
  `status` ENUM('invited','responded','declined','awarded','rejected') NOT NULL DEFAULT 'invited',
  `sent_date` DATETIME DEFAULT NULL,
  `response_date` DATETIME DEFAULT NULL,
  `total_quoted` DOUBLE NOT NULL DEFAULT 0,
  `delivery_lead_days` INT NOT NULL DEFAULT 0,
  `payment_terms` TINYTEXT,
  `vendor_notes` TEXT,
  `evaluator_score` DOUBLE NOT NULL DEFAULT 0,
  `evaluator_notes` TEXT,
  `is_winner` TINYINT(1) NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_rfq` (`rfq_id`),
  KEY `idx_supplier` (`supplier_id`),
  UNIQUE KEY `idx_rfq_supplier` (`rfq_id`, `supplier_id`),
  CONSTRAINT `fk_purch_rfq_vendor_header`
    FOREIGN KEY (`rfq_id`) REFERENCES `0_purch_rfq` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_rfq_vendor_lines` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `rfq_vendor_id` INT NOT NULL,
  `rfq_item_id` INT NOT NULL,
  `quoted_price` DOUBLE NOT NULL DEFAULT 0,
  `quoted_quantity` DOUBLE NOT NULL DEFAULT 0,
  `delivery_lead_days` INT NOT NULL DEFAULT 0,
  `notes` TINYTEXT,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_vendor` (`rfq_vendor_id`),
  KEY `idx_item` (`rfq_item_id`),
  UNIQUE KEY `idx_vendor_item` (`rfq_vendor_id`, `rfq_item_id`),
  CONSTRAINT `fk_purch_rfq_vendor_line_vendor`
    FOREIGN KEY (`rfq_vendor_id`) REFERENCES `0_purch_rfq_vendors` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_purch_rfq_vendor_line_item`
    FOREIGN KEY (`rfq_item_id`) REFERENCES `0_purch_rfq_items` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_purchase_rfq', 'purchase', 'smallint', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('rfq_default_deadline_days', 'purchase', 'smallint', 6, '14');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5379'
  ELSE CONCAT(`areas`, ';5379')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5379;%';

-- ================================================================
-- Purchase Agreements / Blanket Orders. Confusing fields: agreement_id on purchase orders links drawdown against agreement lines and committed quantities.
-- ================================================================

CREATE TABLE IF NOT EXISTS `0_purch_agreements` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `reference` VARCHAR(60) NOT NULL,
  `agreement_type` ENUM('blanket_order','framework_agreement','contract') NOT NULL DEFAULT 'blanket_order',
  `supplier_id` INT NOT NULL,
  `buyer_id` INT NOT NULL DEFAULT 0,
  `status` ENUM('draft','confirmed','active','expired','cancelled') NOT NULL DEFAULT 'draft',
  `date_start` DATE NOT NULL,
  `date_end` DATE DEFAULT NULL,
  `currency` CHAR(3) NOT NULL DEFAULT '',
  `payment_terms` INT NOT NULL DEFAULT 0,
  `total_committed` DOUBLE NOT NULL DEFAULT 0,
  `total_ordered` DOUBLE NOT NULL DEFAULT 0,
  `total_received` DOUBLE NOT NULL DEFAULT 0,
  `total_invoiced` DOUBLE NOT NULL DEFAULT 0,
  `delivery_location` VARCHAR(5) NOT NULL DEFAULT '',
  `auto_renew` TINYINT(1) NOT NULL DEFAULT 0,
  `renewal_period_months` INT NOT NULL DEFAULT 12,
  `terms_and_conditions` TEXT,
  `notes` TINYTEXT,
  `rfq_id` INT NOT NULL DEFAULT 0,
  `dimension_id` INT NOT NULL DEFAULT 0,
  `dimension2_id` INT NOT NULL DEFAULT 0,
  `created_by` INT NOT NULL DEFAULT 0,
  `created_date` DATETIME DEFAULT NULL,
  `inactive` TINYINT(1) NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_supplier` (`supplier_id`),
  KEY `idx_status` (`status`),
  KEY `idx_dates` (`date_start`, `date_end`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_agreement_lines` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `agreement_id` INT NOT NULL,
  `stock_id` VARCHAR(20) NOT NULL,
  `description` TINYTEXT,
  `committed_qty` DOUBLE NOT NULL DEFAULT 0,
  `min_qty_per_order` DOUBLE NOT NULL DEFAULT 0,
  `ordered_qty` DOUBLE NOT NULL DEFAULT 0,
  `received_qty` DOUBLE NOT NULL DEFAULT 0,
  `invoiced_qty` DOUBLE NOT NULL DEFAULT 0,
  `unit_price` DOUBLE NOT NULL DEFAULT 0,
  `discount_percent` DOUBLE NOT NULL DEFAULT 0,
  `price_valid_until` DATE DEFAULT NULL,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_agreement` (`agreement_id`),
  KEY `idx_stock` (`stock_id`),
  CONSTRAINT `fk_purch_agreement_lines_header`
    FOREIGN KEY (`agreement_id`) REFERENCES `0_purch_agreements` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 0_purch_orders columns agreement_id, requisition_id, rfq_id already defined in CREATE TABLE

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_purchase_agreements', 'purchase', 'smallint', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('purchase_agreement_expiry_alert_days', 'purchase', 'smallint', 6, '30');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5380'
  ELSE CONCAT(`areas`, ';5380')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5380;%';

-- ================================================================
-- Vendor scorecard and evaluation criteria. Confusing model: criteria weights + per-evaluation scores + performance log events combine into overall supplier score.
-- All vendor evaluation columns already defined in CREATE TABLE 0_suppliers
-- ================================================================

CREATE TABLE IF NOT EXISTS `0_vendor_evaluation_criteria` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(100) NOT NULL,
  `category` ENUM('quality','delivery','price','service','compliance') NOT NULL DEFAULT 'quality',
  `weight` DOUBLE NOT NULL DEFAULT 1.0,
  `description` TINYTEXT,
  `scoring_method` ENUM('manual','calculated') NOT NULL DEFAULT 'manual',
  `calculation_formula` TEXT,
  `inactive` TINYINT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_name_category` (`name`, `category`),
  KEY `idx_category` (`category`),
  KEY `idx_inactive` (`inactive`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_vendor_evaluations` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `supplier_id` INT NOT NULL,
  `evaluation_date` DATE NOT NULL,
  `evaluator_id` SMALLINT NOT NULL,
  `period_from` DATE NOT NULL,
  `period_to` DATE NOT NULL,
  `status` ENUM('draft','submitted','approved') NOT NULL DEFAULT 'draft',
  `overall_score` DOUBLE NOT NULL DEFAULT 0,
  `quality_score` DOUBLE NOT NULL DEFAULT 0,
  `delivery_score` DOUBLE NOT NULL DEFAULT 0,
  `price_score` DOUBLE NOT NULL DEFAULT 0,
  `service_score` DOUBLE NOT NULL DEFAULT 0,
  `recommendation` ENUM('maintain','upgrade','downgrade','remove','probation') NOT NULL DEFAULT 'maintain',
  `action_plan` TEXT,
  `notes` TEXT,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_supplier` (`supplier_id`),
  KEY `idx_date` (`evaluation_date`),
  KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_vendor_evaluation_scores` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `evaluation_id` INT NOT NULL,
  `criteria_id` INT NOT NULL,
  `score` DOUBLE NOT NULL DEFAULT 0,
  `evidence` TINYTEXT,
  `notes` TINYTEXT,
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_evaluation_criteria` (`evaluation_id`, `criteria_id`),
  KEY `idx_evaluation` (`evaluation_id`),
  KEY `idx_criteria` (`criteria_id`),
  CONSTRAINT `fk_vendor_eval_scores_eval`
    FOREIGN KEY (`evaluation_id`) REFERENCES `0_vendor_evaluations` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_vendor_eval_scores_criteria`
    FOREIGN KEY (`criteria_id`) REFERENCES `0_vendor_evaluation_criteria` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_vendor_performance_log` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `supplier_id` INT NOT NULL,
  `event_type` ENUM('delivery_received','quality_issue','price_change','late_delivery','early_delivery','complaint','resolution') NOT NULL,
  `event_date` DATE NOT NULL,
  `reference_type` SMALLINT NOT NULL DEFAULT 0,
  `reference_no` INT NOT NULL DEFAULT 0,
  `details` TINYTEXT,
  `impact_score` DOUBLE NOT NULL DEFAULT 0,
  `recorded_by` SMALLINT NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_supplier` (`supplier_id`),
  KEY `idx_date` (`event_date`),
  KEY `idx_type` (`event_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_vendor_evaluation_criteria` (`name`, `category`, `weight`, `description`, `scoring_method`, `calculation_formula`, `inactive`)
VALUES
  ('Inspection Pass Rate', 'quality', 1.50, 'Measures how often received items pass quality inspection on first review.', 'calculated', 'inspection_pass_rate_pct', 0),
  ('Defect Rate', 'quality', 1.25, 'Measures defects or failed inspections recorded during the review period.', 'calculated', '100 - defect_rate_pct', 0),
  ('On-Time Delivery', 'delivery', 1.50, 'Measures how consistently deliveries arrive on or before the requested date.', 'calculated', 'on_time_delivery_pct', 0),
  ('Lead Time Accuracy', 'delivery', 1.00, 'Measures whether actual delivery lead time stays within the expected range.', 'manual', '', 0),
  ('Price Competitiveness', 'price', 1.25, 'Compares vendor pricing against the average market price for the same items.', 'calculated', 'price_competitiveness_score', 0),
  ('Responsiveness', 'service', 1.00, 'Measures communication quality, issue handling, and turnaround time.', 'manual', '', 0),
  ('Compliance', 'compliance', 1.00, 'Measures contractual, regulatory, and documentation compliance.', 'manual', '', 0);

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_vendor_evaluation', 'purchase', 'smallint', 1, '0');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5381'
  ELSE CONCAT(`areas`, ';5381')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5381;%';

-- ================================================================
-- Vendor pricelists and purchase templates. Note: pricing precedence can be agreement > vendor_pricelist > purch_data fallback depending on runtime logic.
-- ================================================================

CREATE TABLE IF NOT EXISTS `0_vendor_pricelists` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `supplier_id` INT NOT NULL,
  `stock_id` VARCHAR(20) NOT NULL,
  `vendor_product_code` VARCHAR(50) NOT NULL DEFAULT '',
  `vendor_product_name` VARCHAR(100) NOT NULL DEFAULT '',
  `vendor_uom` VARCHAR(20) NOT NULL DEFAULT '',
  `conversion_factor` DOUBLE NOT NULL DEFAULT 1,
  `currency` CHAR(3) NOT NULL DEFAULT '',
  `min_order_qty` DOUBLE NOT NULL DEFAULT 0,
  `price_break_qty_1` DOUBLE NOT NULL DEFAULT 0,
  `price_1` DOUBLE NOT NULL DEFAULT 0,
  `price_break_qty_2` DOUBLE NOT NULL DEFAULT 0,
  `price_2` DOUBLE NOT NULL DEFAULT 0,
  `price_break_qty_3` DOUBLE NOT NULL DEFAULT 0,
  `price_3` DOUBLE NOT NULL DEFAULT 0,
  `price_break_qty_4` DOUBLE NOT NULL DEFAULT 0,
  `price_4` DOUBLE NOT NULL DEFAULT 0,
  `lead_time_days` INT NOT NULL DEFAULT 0,
  `valid_from` DATE DEFAULT NULL,
  `valid_until` DATE DEFAULT NULL,
  `discount_percent` DOUBLE NOT NULL DEFAULT 0,
  `last_purchase_date` DATE DEFAULT NULL,
  `last_purchase_price` DOUBLE NOT NULL DEFAULT 0,
  `notes` TINYTEXT,
  `is_preferred` TINYINT(1) NOT NULL DEFAULT 0,
  `inactive` TINYINT(1) NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_supplier` (`supplier_id`),
  KEY `idx_stock` (`stock_id`),
  KEY `idx_preferred` (`stock_id`, `is_preferred`),
  UNIQUE KEY `idx_supplier_stock` (`supplier_id`, `stock_id`, `currency`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_order_templates` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(100) NOT NULL,
  `description` TINYTEXT,
  `supplier_id` INT NOT NULL DEFAULT 0,
  `delivery_location` VARCHAR(5) NOT NULL DEFAULT '',
  `default_payment_terms` INT NOT NULL DEFAULT 0,
  `notes` TEXT,
  `inactive` TINYINT(1) NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_supplier` (`supplier_id`),
  KEY `idx_inactive` (`inactive`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_order_template_lines` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `template_id` INT NOT NULL,
  `stock_id` VARCHAR(20) NOT NULL,
  `description` TINYTEXT,
  `default_quantity` DOUBLE NOT NULL DEFAULT 1,
  `sort_order` INT NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_template` (`template_id`),
  KEY `idx_stock` (`stock_id`),
  CONSTRAINT `fk_purch_template_lines_header`
    FOREIGN KEY (`template_id`) REFERENCES `0_purch_order_templates` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_vendor_pricelists`
  (`supplier_id`, `stock_id`, `vendor_product_code`, `vendor_product_name`, `vendor_uom`,
   `conversion_factor`, `currency`, `min_order_qty`,
   `price_break_qty_1`, `price_1`, `price_break_qty_2`, `price_2`,
   `price_break_qty_3`, `price_3`, `price_break_qty_4`, `price_4`,
   `lead_time_days`, `valid_from`, `valid_until`, `discount_percent`,
   `last_purchase_date`, `last_purchase_price`, `notes`, `is_preferred`, `inactive`, `custom_data`)
SELECT purch_data.`supplier_id`,
       purch_data.`stock_id`,
       '',
       purch_data.`supplier_description`,
       purch_data.`suppliers_uom`,
       IF(purch_data.`conversion_factor` <= 0, 1, purch_data.`conversion_factor`),
       supplier.`curr_code`,
       0,
       0,
       purch_data.`price`,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       NULL,
       NULL,
       0,
       NULL,
       0,
       '',
       0,
       0,
       NULL
FROM `0_purch_data` purch_data
INNER JOIN `0_suppliers` supplier ON supplier.`supplier_id` = purch_data.`supplier_id`;

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_vendor_pricelists', 'purchase', 'smallint', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_purchase_templates', 'purchase', 'smallint', 1, '0');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5382'
  ELSE CONCAT(`areas`, ';5382')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5382;%';

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5383'
  ELSE CONCAT(`areas`, ';5383')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5383;%';

-- ================================================================
-- 3-way matching and bill-control policy. Confusing fields: tolerance/action_on_exceed determine block/warn/approval behavior; exceptions keep mismatch evidence.
-- ================================================================

CREATE TABLE IF NOT EXISTS `0_purch_matching_config` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `match_type` ENUM('price_variance','quantity_variance','total_variance') NOT NULL,
  `supplier_id` INT NOT NULL DEFAULT 0,
  `tolerance_type` ENUM('percentage','fixed_amount') NOT NULL DEFAULT 'percentage',
  `tolerance_value` DOUBLE NOT NULL DEFAULT 0,
  `action_on_exceed` ENUM('warn','block','require_approval') NOT NULL DEFAULT 'warn',
  `custom_data` JSON DEFAULT NULL,
  `inactive` TINYINT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `idx_match_type` (`match_type`),
  KEY `idx_supplier_match` (`supplier_id`, `match_type`),
  KEY `idx_inactive` (`inactive`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_matching_exceptions` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `exception_type` ENUM('price_variance','quantity_variance','total_variance','missing_grn','missing_po') NOT NULL,
  `trans_type` SMALLINT NOT NULL,
  `trans_no` INT NOT NULL,
  `supplier_id` INT NOT NULL,
  `po_number` INT NOT NULL DEFAULT 0,
  `grn_batch_id` INT NOT NULL DEFAULT 0,
  `stock_id` VARCHAR(20) NOT NULL DEFAULT '',
  `expected_value` DOUBLE NOT NULL DEFAULT 0,
  `actual_value` DOUBLE NOT NULL DEFAULT 0,
  `variance_amount` DOUBLE NOT NULL DEFAULT 0,
  `variance_percent` DOUBLE NOT NULL DEFAULT 0,
  `status` ENUM('open','approved','rejected','resolved') NOT NULL DEFAULT 'open',
  `resolution_notes` TINYTEXT,
  `resolved_by` SMALLINT NOT NULL DEFAULT 0,
  `resolved_date` DATETIME DEFAULT NULL,
  `created_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_status` (`status`),
  KEY `idx_supplier` (`supplier_id`),
  KEY `idx_trans` (`trans_type`, `trans_no`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_purch_bill_control_policy` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `name` VARCHAR(100) NOT NULL,
  `bill_basis` ENUM('on_ordered','on_received') NOT NULL DEFAULT 'on_received',
  `require_grn_before_invoice` TINYINT(1) NOT NULL DEFAULT 1,
  `require_po_for_invoice` TINYINT(1) NOT NULL DEFAULT 0,
  `allow_over_invoice` TINYINT(1) NOT NULL DEFAULT 0,
  `auto_match_on_grn` TINYINT(1) NOT NULL DEFAULT 1,
  `is_default` TINYINT(1) NOT NULL DEFAULT 0,
  `inactive` TINYINT(1) NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_default` (`is_default`),
  KEY `idx_inactive` (`inactive`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_purch_matching_config`
  (`match_type`, `supplier_id`, `tolerance_type`, `tolerance_value`, `action_on_exceed`, `custom_data`, `inactive`)
VALUES
  ('price_variance', 0, 'percentage', 5, 'warn', NULL, 0),
  ('quantity_variance', 0, 'percentage', 5, 'warn', NULL, 0),
  ('total_variance', 0, 'percentage', 5, 'warn', NULL, 0);

INSERT IGNORE INTO `0_purch_bill_control_policy`
  (`name`, `bill_basis`, `require_grn_before_invoice`, `require_po_for_invoice`, `allow_over_invoice`, `auto_match_on_grn`, `is_default`, `inactive`, `custom_data`)
VALUES
  ('Default 3-Way Matching Policy', 'on_received', 1, 0, 0, 1, 1, 0, NULL);

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_3way_matching', 'purchase', 'smallint', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('default_matching_tolerance_pct', 'purchase', 'smallint', 6, '5');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5384'
  ELSE CONCAT(`areas`, ';5384')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5384;%';

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5642'
  ELSE CONCAT(`areas`, ';5642')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5642;%';

-- ================================================================
-- Automated reorder and procurement planning. Note: preferred_supplier_id extends replenishment rules; plan lines are operational staging before PO generation.
-- All replenishment rule columns already defined in CREATE TABLE 0_wh_replenishment_rules
-- ================================================================

UPDATE `0_wh_replenishment_rules`
SET `preferred_supplier_id` = IFNULL(`preferred_supplier`, 0)
WHERE IFNULL(`preferred_supplier_id`, 0) = 0
  AND IFNULL(`preferred_supplier`, 0) > 0;

CREATE TABLE IF NOT EXISTS `0_procurement_plan` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `reference` VARCHAR(60) NOT NULL,
  `plan_date` DATE NOT NULL,
  `plan_type` ENUM('auto_reorder','demand_based','manual') NOT NULL DEFAULT 'auto_reorder',
  `status` ENUM('draft','confirmed','in_progress','completed','cancelled') NOT NULL DEFAULT 'draft',
  `created_by` SMALLINT NOT NULL DEFAULT 0,
  `notes` TINYTEXT,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_date` (`plan_date`),
  KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `0_procurement_plan_lines` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `plan_id` INT NOT NULL,
  `stock_id` VARCHAR(20) NOT NULL,
  `location` VARCHAR(5) NOT NULL DEFAULT '',
  `replenishment_rule_id` INT NOT NULL DEFAULT 0,
  `current_stock` DOUBLE NOT NULL DEFAULT 0,
  `required_qty` DOUBLE NOT NULL DEFAULT 0,
  `suggested_order_qty` DOUBLE NOT NULL DEFAULT 0,
  `supplier_id` INT NOT NULL DEFAULT 0,
  `estimated_price` DOUBLE NOT NULL DEFAULT 0,
  `estimated_lead_time` INT NOT NULL DEFAULT 0,
  `priority` ENUM('low','normal','high','urgent') NOT NULL DEFAULT 'normal',
  `status` ENUM('pending','approved','ordered','skipped') NOT NULL DEFAULT 'pending',
  `po_number` INT NOT NULL DEFAULT 0,
  `custom_data` JSON DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_plan` (`plan_id`),
  KEY `idx_stock` (`stock_id`),
  KEY `idx_supplier` (`supplier_id`),
  KEY `idx_status` (`status`),
  CONSTRAINT `fk_procurement_plan_lines_header`
    FOREIGN KEY (`plan_id`) REFERENCES `0_procurement_plan` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_procurement_planning', 'purchase', 'smallint', 1, '0');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5385'
  ELSE CONCAT(`areas`, ';5385')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5385;%';

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5643'
  ELSE CONCAT(`areas`, ';5643')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5643;%';

-- ================================================================
-- Purchase analytics and KPI cache. Confusing parts: cache table schema may evolve by metric type; reports should tolerate sparse KPI rows.
-- ================================================================

CREATE TABLE IF NOT EXISTS `0_purch_kpi_cache` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `kpi_type` ENUM('monthly_spend','vendor_spend','category_spend','lead_time_avg',
                  'on_time_delivery','price_variance','savings','open_po_value',
                  'pending_grn','pending_invoice','matching_exceptions') NOT NULL,
  `period_key` VARCHAR(20) NOT NULL,
  `dimension_key` VARCHAR(50) NOT NULL DEFAULT '',
  `value_1` DOUBLE NOT NULL DEFAULT 0,
  `value_2` DOUBLE NOT NULL DEFAULT 0,
  `value_3` DOUBLE NOT NULL DEFAULT 0,
  `detail_json` JSON DEFAULT NULL,
  `updated_at` DATETIME DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_kpi` (`kpi_type`, `period_key`, `dimension_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
VALUES ('use_purchase_dashboard', 'purchase', 'smallint', 1, '1');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5892'
  ELSE CONCAT(`areas`, ';5892')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5892;%';

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '5893'
  ELSE CONCAT(`areas`, ';5893')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;5893;%';

-- ================================================================
-- Phase 8: Advanced Credit Control & Customer Enhancements
-- ================================================================

-- Credit review history
CREATE TABLE IF NOT EXISTS `0_sales_credit_reviews` (
  `id`                INT NOT NULL AUTO_INCREMENT,
  `debtor_no`         INT NOT NULL,
  `review_date`       DATE NOT NULL,
  `reviewer_id`       INT NOT NULL DEFAULT 0,
  `old_credit_limit`  DOUBLE DEFAULT 0,
  `new_credit_limit`  DOUBLE DEFAULT 0,
  `old_credit_status` INT DEFAULT 0,
  `new_credit_status` INT DEFAULT 0,
  `risk_score`        ENUM('low','medium','high','critical') DEFAULT 'medium',
  `notes`             TEXT,
  PRIMARY KEY (`id`),
  KEY `idx_debtor` (`debtor_no`),
  KEY `idx_date`   (`review_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Credit holds
CREATE TABLE IF NOT EXISTS `0_sales_credit_holds` (
  `id`                  INT NOT NULL AUTO_INCREMENT,
  `debtor_no`           INT NOT NULL,
  `hold_type`           ENUM('manual','over_limit','overdue','risk') DEFAULT 'manual',
  `hold_date`           DATETIME NOT NULL,
  `release_date`        DATETIME DEFAULT NULL,
  `held_by`             INT DEFAULT 0,
  `released_by`         INT DEFAULT 0,
  `reason`              TINYTEXT,
  `affects_orders`      TINYINT(1) DEFAULT 1,
  `affects_deliveries`  TINYINT(1) DEFAULT 1,
  `affects_invoices`    TINYINT(1) DEFAULT 0,
  PRIMARY KEY (`id`),
  KEY `idx_debtor` (`debtor_no`),
  KEY `idx_active` (`debtor_no`, `release_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
  VALUES ('use_advanced_credit_control', 'setup.company', 'tinyint', 1, '0');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
  VALUES ('credit_check_on_order', 'setup.company', 'tinyint', 1, '1');

INSERT IGNORE INTO `0_sys_prefs` (`name`, `category`, `type`, `length`, `value`)
  VALUES ('credit_check_on_delivery', 'setup.company', 'tinyint', 1, '1');

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '3084'
  ELSE CONCAT(`areas`, ';3084')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;3084;%';

UPDATE `0_security_roles`
SET `areas` = CASE
  WHEN IFNULL(`areas`, '') = '' THEN '3085'
  ELSE CONCAT(`areas`, ';3085')
END
WHERE `id` IN (2, 10)
  AND CONCAT(';', IFNULL(`areas`, ''), ';') NOT LIKE '%;3085;%';

-- END CONSOLIDATED MIGRATIONS
