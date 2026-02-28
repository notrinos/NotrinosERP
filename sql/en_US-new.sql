
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
	`attendance_date`     date            NOT NULL,
	`time_in`             time            DEFAULT NULL,
	`time_out`            time            DEFAULT NULL,
	`scheduled_in`        time            DEFAULT NULL,
	`scheduled_out`       time            DEFAULT NULL,
	`hours_worked`        decimal(5,2)    NOT NULL DEFAULT '0.00',
	`overtime_hours`      decimal(5,2)    NOT NULL DEFAULT '0.00',
	`late_minutes`        int(5)          NOT NULL DEFAULT '0',
	`early_leave_minutes` int(5)          NOT NULL DEFAULT '0',
	`attendance_status`   varchar(20)     NOT NULL DEFAULT 'present',
	`shift_id`            int(11)         DEFAULT NULL,
	`leave_type_id`       int(11)         DEFAULT NULL,
	`approval_status`     varchar(10)     NOT NULL DEFAULT 'approved',
	`approved_by`         varchar(20)     DEFAULT NULL,
	`notes`               text,
	`rate`                float(5)        NOT NULL DEFAULT '1',
	`payroll_processed`   tinyint(1)      NOT NULL DEFAULT '0',
	PRIMARY KEY (`attendance_id`),
	UNIQUE KEY `emp_date` (`employee_id`,`attendance_date`)
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
) ENGINE=InnoDB;

-- Data of table `0_audit_trail` --

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
) ENGINE=InnoDB;

-- Data of table `0_bank_trans` --

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
) ENGINE=InnoDB;

-- Data of table `0_bom` --

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
) ENGINE=InnoDB;

-- Data of table `0_crm_contacts` --

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
) ENGINE=InnoDB;

-- Data of table `0_crm_persons` --

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
) ENGINE=InnoDB;

-- Data of table `0_cust_allocations` --

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
) ENGINE=InnoDB;

-- Data of table `0_cust_branch` --

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

-- Structure of table `0_debtor_trans_details` --

DROP TABLE IF EXISTS `0_debtor_trans_details`;

CREATE TABLE `0_debtor_trans_details` (
	`id` int(11) NOT NULL AUTO_INCREMENT,
	`debtor_trans_no` int(11) DEFAULT NULL,
	`debtor_trans_type` int(11) DEFAULT NULL,
	`stock_id` varchar(20) NOT NULL DEFAULT '',
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
) ENGINE=InnoDB;

-- Data of table `0_debtor_trans_details` --

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
	PRIMARY KEY (`debtor_no`),
	UNIQUE KEY `debtor_ref` (`debtor_ref`),
	KEY `name` (`name`)
) ENGINE=InnoDB;

-- Data of table `0_debtors_master` --

-- Structure of table `0_departments` --

DROP TABLE IF EXISTS `0_departments`;
CREATE TABLE IF NOT EXISTS `0_departments` (
	`department_id`          int(11)      NOT NULL AUTO_INCREMENT,
	`department_code`        varchar(20)  NOT NULL DEFAULT '',
	`department_name`        varchar(100) NOT NULL DEFAULT '',
	`parent_department_id`   int(11)      DEFAULT NULL,
	`manager_employee_id`    varchar(20)  DEFAULT NULL,
	`cost_center`            varchar(30)  DEFAULT NULL,
	`location`               varchar(100) DEFAULT NULL,
	`description`            text,
	`is_active`              tinyint(1)   NOT NULL DEFAULT '1',
	PRIMARY KEY (`department_id`),
	UNIQUE KEY `dept_code` (`department_code`)
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
) ENGINE=InnoDB;

-- Data of table `0_dimensions` --

-- Structure of table `0_employees` --

DROP TABLE IF EXISTS `0_employees`;
CREATE TABLE IF NOT EXISTS `0_employees` (
	`employee_id`             varchar(20)     NOT NULL DEFAULT '',
	`person_id`               int(11)         DEFAULT NULL,
	`employee_code`           varchar(20)     NOT NULL DEFAULT '',
	`first_name`              varchar(50)     NOT NULL DEFAULT '',
	`middle_name`             varchar(50)     DEFAULT NULL,
	`last_name`               varchar(50)     NOT NULL DEFAULT '',
	`title`                   varchar(10)     DEFAULT NULL,
	`gender`                  char(1)         NOT NULL DEFAULT 'M',
	`date_of_birth`           date            DEFAULT NULL,
	`national_id`             varchar(30)     DEFAULT NULL,
	`passport_no`             varchar(30)     DEFAULT NULL,
	`nationality`             varchar(50)     DEFAULT NULL,
	`marital_status`          varchar(15)     NOT NULL DEFAULT 'single',
	`email`                   varchar(100)    DEFAULT NULL,
	`phone`                   varchar(30)     DEFAULT NULL,
	`mobile`                  varchar(30)     DEFAULT NULL,
	`address`                 varchar(200)    DEFAULT NULL,
	`city`                    varchar(50)     DEFAULT NULL,
	`state`                   varchar(50)     DEFAULT NULL,
	`country`                 varchar(50)     DEFAULT NULL,
	`postal_code`             varchar(15)     DEFAULT NULL,
	`hire_date`               date            NOT NULL,
	`termination_date`        date            DEFAULT NULL,
	`employment_type`         varchar(20)     NOT NULL DEFAULT 'full_time',
	`employment_status`       varchar(20)     NOT NULL DEFAULT 'active',
	`department_id`           int(11)         DEFAULT NULL,
	`position_id`             int(11)         DEFAULT NULL,
	`job_class_id`            int(11)         DEFAULT NULL,
	`pay_grade_id`            int(11)         DEFAULT NULL,
	`salary_structure_id`     int(11)         DEFAULT NULL,
	`manager_id`              varchar(20)     DEFAULT NULL,
	`work_location`           varchar(100)    DEFAULT NULL,
	`shift_id`                int(11)         DEFAULT NULL,
	`bank_name`               varchar(100)    DEFAULT NULL,
	`bank_account_no`         varchar(50)     DEFAULT NULL,
	`bank_account_name`       varchar(100)    DEFAULT NULL,
	`bank_branch`             varchar(100)    DEFAULT NULL,
	`payment_method`          varchar(20)     NOT NULL DEFAULT 'bank_transfer',
	`emergency_contact_name`  varchar(100)    DEFAULT NULL,
	`emergency_contact_phone` varchar(30)     DEFAULT NULL,
	`emergency_contact_rel`   varchar(30)     DEFAULT NULL,
	`tax_id`                  varchar(30)     DEFAULT NULL,
	`tax_exemption`           varchar(20)     DEFAULT NULL,
	`social_insurance_no`     varchar(30)     DEFAULT NULL,
	`photo`                   varchar(200)    DEFAULT NULL,
	`notes`                   text,
	`inactive`                tinyint(1)      NOT NULL DEFAULT '0',
	PRIMARY KEY (`employee_id`),
	UNIQUE KEY `emp_code` (`employee_code`)
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
) ENGINE=InnoDB;

-- Data of table `0_exchange_rates` --

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
) ENGINE=InnoDB AUTO_INCREMENT=2 ;

-- Data of table `0_fiscal_year` --

INSERT INTO `0_fiscal_year` VALUES
('1', '2025-01-01', '2025-12-31', '1');

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
) ENGINE=InnoDB;

-- Data of table `0_gl_trans` --

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
) ENGINE=InnoDB;

-- Data of table `0_grn_batch` --

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
	PRIMARY KEY (`id`),
	KEY `grn_batch_id` (`grn_batch_id`)
) ENGINE=InnoDB;

-- Data of table `0_grn_items` --

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
) ENGINE=InnoDB;

-- Data of table `0_item_codes` --

-- Structure of table `0_item_tax_type_exemptions` --

DROP TABLE IF EXISTS `0_item_tax_type_exemptions`;

CREATE TABLE `0_item_tax_type_exemptions` (
	`item_tax_type_id` int(11) NOT NULL DEFAULT '0',
	`tax_type_id` int(11) NOT NULL DEFAULT '0',
	PRIMARY KEY (`item_tax_type_id`,`tax_type_id`)
) ENGINE=InnoDB;

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
	`class_code`     varchar(20)  NOT NULL DEFAULT '',
	`class_name`     varchar(100) NOT NULL DEFAULT '',
	`pay_basis`      varchar(20)  NOT NULL DEFAULT 'monthly',
	`description`    text,
	`inactive`       tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`job_class_id`),
	UNIQUE KEY `class_code` (`class_code`)
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
	`leave_type_id`     int(11)      NOT NULL AUTO_INCREMENT,
	`leave_code`        varchar(10)  NOT NULL DEFAULT '',
	`leave_name`        varchar(60)  NOT NULL DEFAULT '',
	`allotted_days`     decimal(5,1) NOT NULL DEFAULT '0.0',
	`carry_forward`     tinyint(1)   NOT NULL DEFAULT '0',
	`max_carry_days`    decimal(5,1) NOT NULL DEFAULT '0.0',
	`encashable`        tinyint(1)   NOT NULL DEFAULT '0',
	`gender`            char(1)      NOT NULL DEFAULT 'A',
	`paid`              tinyint(1)   NOT NULL DEFAULT '1',
	`require_approval`  tinyint(1)   NOT NULL DEFAULT '1',
	`min_service_days`  int(11)      NOT NULL DEFAULT '0',
	`description`       text,
	`inactive`          tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`leave_type_id`),
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
	PRIMARY KEY (`loc_code`)
) ENGINE=InnoDB ;

-- Data of table `0_locations` --

INSERT INTO `0_locations` VALUES
('DEF', 'Default', 'N/A', '', '', '', '', '', '0', '0', '{}');

-- Structure of table `0_overtime` --

DROP TABLE IF EXISTS `0_overtime`;
CREATE TABLE IF NOT EXISTS `0_overtime` (
	`overtime_id`         int(11)      NOT NULL AUTO_INCREMENT,
	`employee_id`         varchar(20)  NOT NULL DEFAULT '',
	`overtime_date`       date         NOT NULL,
	`hours`               decimal(5,2) NOT NULL DEFAULT '0.00',
	`overtime_type`       varchar(20)  NOT NULL DEFAULT 'regular',
	`pay_rate_multiplier` decimal(4,2) NOT NULL DEFAULT '1.50',
	`account_code`        int(11)      DEFAULT NULL,
	`dimension_id`        int(11)      DEFAULT NULL,
	`dimension2_id`       int(11)      DEFAULT NULL,
	`requires_approval`   tinyint(1)   NOT NULL DEFAULT '1',
	`approval_status`     varchar(10)  NOT NULL DEFAULT 'pending',
		`approved_by`     varchar(20)  DEFAULT NULL,
	`notes`               text,
	PRIMARY KEY (`overtime_id`)
) ENGINE=InnoDB;

-- Data of table `0_overtime` --

-- Structure of table `0_pay_elements` --

DROP TABLE IF EXISTS `0_pay_elements`;
CREATE TABLE IF NOT EXISTS `0_pay_elements` (
	`element_id`       int(11)      NOT NULL AUTO_INCREMENT,
	`element_code`     varchar(20)  NOT NULL DEFAULT '',
	`element_name`     varchar(100) NOT NULL DEFAULT '',
	`element_type`     varchar(20)  NOT NULL DEFAULT 'earning',
	`calculation_type` varchar(20)  NOT NULL DEFAULT 'fixed',
	`formula`          text,
	`default_amount`   decimal(15,2) NOT NULL DEFAULT '0.00',
	`taxable`          tinyint(1)   NOT NULL DEFAULT '1',
	`pensionable`      tinyint(1)   NOT NULL DEFAULT '1',
	`account_code`     int(11)      DEFAULT NULL,
	`dimension_id`     int(11)      DEFAULT NULL,
	`dimension2_id`    int(11)      DEFAULT NULL,
	`recurring`        tinyint(1)   NOT NULL DEFAULT '1',
	`print_on_payslip` tinyint(1)   NOT NULL DEFAULT '1',
	`order_num`        int(11)      NOT NULL DEFAULT '0',
	`description`      text,
	`inactive`         tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`element_id`),
	UNIQUE KEY `element_code` (`element_code`)
) ENGINE=InnoDB;

-- Data of table `0_pay_elements` --

-- Structure of table `0_pay_grades` --

DROP TABLE IF EXISTS `0_pay_grades`;
CREATE TABLE IF NOT EXISTS `0_pay_grades` (
	`pay_grade_id`   int(11)       NOT NULL AUTO_INCREMENT,
	`grade_code`     varchar(20)   NOT NULL DEFAULT '',
	`grade_name`     varchar(100)  NOT NULL DEFAULT '',
	`grade_level`    int(5)        NOT NULL DEFAULT '0',
	`min_salary`     decimal(15,2) NOT NULL DEFAULT '0.00',
	`mid_salary`     decimal(15,2) NOT NULL DEFAULT '0.00',
	`max_salary`     decimal(15,2) NOT NULL DEFAULT '0.00',
	`currency`       char(3)       NOT NULL DEFAULT '',
	`inactive`       tinyint(1)    NOT NULL DEFAULT '0',
	PRIMARY KEY (`pay_grade_id`),
	UNIQUE KEY `grade_code` (`grade_code`)
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

DROP TABLE IF EXISTS `0_payslips`;
CREATE TABLE IF NOT EXISTS `0_payslips` (
	`payslip_id`         int(11)       NOT NULL AUTO_INCREMENT,
	`payslip_no`         varchar(30)   NOT NULL DEFAULT '',
	`employee_id`        varchar(20)   NOT NULL DEFAULT '',
	`period_id`          int(11)       NOT NULL DEFAULT '0',
	`period_start`       date          NOT NULL,
	`period_end`         date          NOT NULL,
	`payment_date`       date          DEFAULT NULL,
	`gross_salary`       decimal(15,2) NOT NULL DEFAULT '0.00',
	`total_earnings`     decimal(15,2) NOT NULL DEFAULT '0.00',
	`total_deductions`   decimal(15,2) NOT NULL DEFAULT '0.00',
	`total_tax`          decimal(15,2) NOT NULL DEFAULT '0.00',
	`net_salary`         decimal(15,2) NOT NULL DEFAULT '0.00',
	`employer_cost`      decimal(15,2) NOT NULL DEFAULT '0.00',
	`currency`           char(3)       NOT NULL DEFAULT '',
	`exchange_rate`      decimal(15,6) NOT NULL DEFAULT '1.000000',
	`payment_method`     varchar(20)   NOT NULL DEFAULT 'bank_transfer',
	`bank_account`       varchar(50)   DEFAULT NULL,
	`status`             varchar(20)   NOT NULL DEFAULT 'draft',
	`approved_by`        varchar(20)   DEFAULT NULL,
	`approved_date`      date          DEFAULT NULL,
	`posted`             tinyint(1)    NOT NULL DEFAULT '0',
	`journal_id`         int(11)       DEFAULT NULL,
	`notes`              text,
	PRIMARY KEY (`payslip_id`),
	UNIQUE KEY `payslip_no` (`payslip_no`)
) ENGINE=InnoDB;

-- Structure of table `0_positions` --

DROP TABLE IF EXISTS `0_positions`;
CREATE TABLE IF NOT EXISTS `0_positions` (
	`position_id`        int(11)      NOT NULL AUTO_INCREMENT,
	`position_code`      varchar(20)  NOT NULL DEFAULT '',
	`position_title`     varchar(100) NOT NULL DEFAULT '',
	`department_id`      int(11)      DEFAULT NULL,
	`job_class_id`       int(11)      DEFAULT NULL,
	`pay_grade_id`       int(11)      DEFAULT NULL,
	`reports_to`         int(11)      DEFAULT NULL,
	`min_experience`     int(5)       NOT NULL DEFAULT '0',
	`min_education`      varchar(50)  DEFAULT NULL,
	`headcount_budget`   int(11)      NOT NULL DEFAULT '0',
	`headcount_actual`   int(11)      NOT NULL DEFAULT '0',
	`description`        text,
	`inactive`           tinyint(1)   NOT NULL DEFAULT '0',
	PRIMARY KEY (`position_id`),
	UNIQUE KEY `position_code` (`position_code`)
) ENGINE=InnoDB;

-- Data of table `0_positions` --

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
) ENGINE=InnoDB;

-- Data of table `0_prices` --

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
) ENGINE=InnoDB;

-- Data of table `0_purch_data` --

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
	PRIMARY KEY (`po_detail_item`),
	KEY `order` (`order_no`,`po_detail_item`),
	KEY `itemcode` (`item_code`)
) ENGINE=InnoDB;

-- Data of table `0_purch_order_details` --

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
	PRIMARY KEY (`order_no`),
	KEY `ord_date` (`ord_date`)
) ENGINE=InnoDB;

-- Data of table `0_purch_orders` --

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
) ENGINE=InnoDB;

-- Data of table `0_recurrent_invoices` --

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
) ENGINE=InnoDB;

-- Data of table `0_refs` --

-- Structure of table `0_salary_structure` --

DROP TABLE IF EXISTS `0_salary_structure`;
CREATE TABLE IF NOT EXISTS `0_salary_structure` (
	`structure_id`      int(11)       NOT NULL AUTO_INCREMENT,
	`structure_code`    varchar(20)   NOT NULL DEFAULT '',
	`structure_name`    varchar(100)  NOT NULL DEFAULT '',
	`employee_id`       varchar(20)   DEFAULT NULL,
	`pay_grade_id`      int(11)       DEFAULT NULL,
	`effective_from`    date          NOT NULL,
	`effective_to`      date          DEFAULT NULL,
	`base_salary`       decimal(15,2) NOT NULL DEFAULT '0.00',
	`currency`          char(3)       NOT NULL DEFAULT '',
	`formula`           text,
	`is_default`        tinyint(1)    NOT NULL DEFAULT '0',
	`inactive`          tinyint(1)    NOT NULL DEFAULT '0',
	PRIMARY KEY (`structure_id`),
	UNIQUE KEY `structure_code` (`structure_code`)
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
	PRIMARY KEY (`id`),
	KEY `sorder` (`trans_type`,`order_no`),
	KEY `stkcode` (`stk_code`)
) ENGINE=InnoDB;

-- Data of table `0_sales_order_details` --

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
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`trans_type`,`order_no`)
) ENGINE=InnoDB;

-- Data of table `0_sales_orders` --

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
('2', 'System Administrator', 'System Administrator', '256;512;768;2816;3072;3328;5376;5632;5888;7936;8192;8448;9472;9728;10496;10752;11008;13056;13312;15616;15872;16128', '257;258;259;260;513;514;515;516;517;518;519;520;521;522;523;524;525;526;769;770;771;772;773;774;2817;2818;2819;2820;2821;2822;2823;3073;3074;3082;3075;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5634;5635;5636;5637;5641;5638;5639;5640;5889;5890;5891;7937;7938;7939;7940;8193;8194;8195;8196;8197;8449;8450;8451;9217;9218;9220;9473;9474;9475;9476;9729;10497;10753;10754;10755;10756;10757;11009;11010;11011;11012;13057;13313;13314;13315;15617;15618;15619;15620;15621;15622;15623;15624;15628;15625;15626;15627;15873;15874;15875;15876;15877;15878;15879;15880;15883;15881;15882;16129;16130;16131;16132;775', '0'),
('3', 'Salesman', 'Salesman', '768;3072;5632;8192;15872', '773;774;3073;3075;3081;5633;8194;15873;775', '0'),
('4', 'Stock Manager', 'Stock Manager', '768;2816;3072;3328;5632;5888;8192;8448;10752;11008;13312;15872;16128', '2818;2822;3073;3076;3077;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5633;5640;5889;5890;5891;8193;8194;8450;8451;10753;11009;11010;11012;13313;13315;15882;16129;16130;16131;16132;775', '0'),
('5', 'Production Manager', 'Production Manager', '512;768;2816;3072;3328;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '521;523;524;2818;2819;2820;2821;2822;2823;3073;3074;3076;3077;3078;3079;3080;3081;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5633;5640;5640;5889;5890;5891;8193;8194;8196;8197;8450;8451;10753;10755;11009;11010;11012;13313;13315;15617;15619;15620;15621;15624;15624;15876;15877;15880;15882;16129;16130;16131;16132;775', '0'),
('6', 'Purchase Officer', 'Purchase Officer', '512;768;2816;3072;3328;5376;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '521;523;524;2818;2819;2820;2821;2822;2823;3073;3074;3076;3077;3078;3079;3080;3081;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5377;5633;5635;5640;5640;5889;5890;5891;8193;8194;8196;8197;8449;8450;8451;10753;10755;11009;11010;11012;13313;13315;15617;15619;15620;15621;15624;15624;15876;15877;15880;15882;16129;16130;16131;16132;775', '0'),
('7', 'AR Officer', 'AR Officer', '512;768;2816;3072;3328;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '521;523;524;771;773;774;2818;2819;2820;2821;2822;2823;3073;3073;3074;3075;3076;3077;3078;3079;3080;3081;3081;3329;3330;3330;3330;3331;3331;3332;3333;3334;3335;5633;5633;5634;5637;5638;5639;5640;5640;5889;5890;5891;8193;8194;8194;8196;8197;8450;8451;10753;10755;11009;11010;11012;13313;13315;15617;15619;15620;15621;15624;15624;15873;15876;15877;15878;15880;15882;16129;16130;16131;16132;775', '0'),
('8', 'AP Officer', 'AP Officer', '512;768;2816;3072;3328;5376;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '257;258;259;260;521;523;524;769;770;771;772;773;774;2818;2819;2820;2821;2822;2823;3073;3074;3082;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5635;5640;5889;5890;5891;7937;7938;7939;7940;8193;8194;8196;8197;8449;8450;8451;10497;10753;10755;11009;11010;11012;13057;13313;13315;15617;15619;15620;15621;15624;15876;15877;15880;15882;16129;16130;16131;16132;775', '0'),
('9', 'Accountant', 'New Accountant', '512;768;2816;3072;3328;5376;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '257;258;259;260;521;523;524;771;772;773;774;2818;2819;2820;2821;2822;2823;3073;3074;3075;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5634;5635;5637;5638;5639;5640;5889;5890;5891;7937;7938;7939;7940;8193;8194;8196;8197;8449;8450;8451;10497;10753;10755;11009;11010;11012;13313;13315;15617;15618;15619;15620;15621;15624;15873;15876;15877;15878;15880;15882;16129;16130;16131;16132;775', '0'),
('10', 'Sub Admin', 'Sub Admin', '512;768;2816;3072;3328;5376;5632;5888;8192;8448;10752;11008;13312;15616;15872;16128', '257;258;259;260;521;523;524;771;772;773;774;2818;2819;2820;2821;2822;2823;3073;3074;3082;3075;3076;3077;3078;3079;3080;3081;3329;3330;3331;3332;3333;3334;3335;5377;5633;5634;5635;5637;5638;5639;5640;5889;5890;5891;7937;7938;7939;7940;8193;8194;8196;8197;8449;8450;8451;10497;10753;10755;11009;11010;11012;13057;13313;13315;15617;15619;15620;15621;15624;15873;15874;15876;15877;15878;15879;15880;15882;16129;16130;16131;16132;775', '0');

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
	`stock_id` varchar(20) NOT NULL DEFAULT '',
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
	PRIMARY KEY (`stock_id`)
) ENGINE=InnoDB;

-- Data of table `0_stock_master` --

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
	PRIMARY KEY (`trans_id`),
	KEY `type` (`type`,`trans_no`),
	KEY `Move` (`stock_id`,`loc_code`,`tran_date`)
) ENGINE=InnoDB;

-- Data of table `0_stock_moves` --

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
) ENGINE=InnoDB;

-- Data of table `0_supp_invoice_items` --

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
	PRIMARY KEY (`supplier_id`),
	UNIQUE KEY `supp_ref` (`supp_ref`)
) ENGINE=InnoDB;

-- Data of table `0_suppliers` --

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
('f_year', 'setup.company', 'int', 11, '1'),
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
('default_work_hours', 'setup.company', 'float', 5, '8'),
('weekend_day', 'setup.company', 'titnyint', 1, '7'),
('payroll_month_work_days', 'setup.company', 'float', '2', 26),
('payroll_payable_act', 'glsetup.hrm', 'varchar', 15, 2100),
('payroll_deductleave_act', 'glsetup.hrm', 'varchar', 15, ''),
('payroll_overtime_act', 'glsetup.hrm', 'varchar', 15, 5420);

-- Structure of table `0_tag_associations` --

DROP TABLE IF EXISTS `0_tag_associations`;

CREATE TABLE `0_tag_associations` (
	`record_id` varchar(15) NOT NULL,
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
	`inactive` tinyint(1) NOT NULL DEFAULT '0',
	`custom_data` JSON NOT NULL DEFAULT ('{}'),
	PRIMARY KEY (`id`),
	UNIQUE KEY `type` (`type`,`name`)
) ENGINE=InnoDB ;

-- Data of table `0_tags` --

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
) ENGINE=InnoDB;

-- Data of table `0_trans_tax_details` --

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
) ENGINE=InnoDB;

-- Data of table `0_wo_manufacture` --

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
) ENGINE=InnoDB;

-- Data of table `0_wo_requirements` --

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
) ENGINE=InnoDB;

-- Data of table `0_workorders` --

-- =============================================================
-- ATTENDANCE DEDUCTION RULES
-- =============================================================

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

-- 
-- LEAVE REQUESTS
-- 

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

-- =============================================================
-- END OF NEW HRM TABLES
-- =============================================================

