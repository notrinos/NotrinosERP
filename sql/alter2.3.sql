ALTER TABLE 0_comments ADD KEY `type_and_id` (`type`, `id`);
ALTER TABLE 0_quick_entries ADD COLUMN `bal_type` TINYINT(1) NOT NULL default '0'; 

# Key optimizations
ALTER TABLE 0_fiscal_year ADD UNIQUE KEY(`begin`), ADD UNIQUE KEY(`end`);
ALTER TABLE 0_useronline ADD KEY(`ip`);
ALTER TABLE 0_dimensions ADD KEY(`date_`), ADD KEY(`due_date`), ADD KEY(`type_`);
ALTER TABLE 0_gl_trans ADD KEY (`dimension_id`), ADD KEY (`dimension2_id`), ADD KEY (`tran_date`), ADD KEY `account_and_tran_date` (`account`, `tran_date`);
ALTER TABLE 0_chart_master DROP KEY `account_code`;
ALTER TABLE 0_chart_types ADD KEY(`class_id`);
ALTER TABLE 0_bank_accounts ADD KEY (`account_code`);
ALTER TABLE 0_bank_trans ADD KEY (`bank_act`,`reconciled`), ADD KEY (`bank_act`,`trans_date`);
ALTER TABLE 0_budget_trans ADD KEY `Account` (`account`, `tran_date`, `dimension_id`, `dimension2_id`);
ALTER TABLE 0_trans_tax_details ADD KEY `Type_and_Number` (`trans_type`,`trans_no`), ADD KEY (`tran_date`);
ALTER TABLE 0_audit_trail DROP KEY `fiscal_year`, ADD KEY `Seq` (`fiscal_year`, `gl_date`, `gl_seq`), ADD KEY `Type_and_Number` (`type`,`trans_no`);
ALTER TABLE 0_item_codes ADD KEY (`item_code`);
ALTER TABLE 0_stock_moves ADD KEY `Move` (`stock_id`,`loc_code`, `tran_date`);
ALTER TABLE 0_wo_issues ADD KEY (`workorder_id`);
ALTER TABLE 0_wo_manufacture ADD KEY (`workorder_id`);
ALTER TABLE 0_wo_requirements ADD KEY (`workorder_id`);
ALTER TABLE 0_bom DROP KEY `Parent_2`;
ALTER TABLE 0_refs ADD KEY `Type_and_Reference` (`type`,`reference`);
ALTER TABLE 0_grn_items ADD KEY (`grn_batch_id`);
ALTER TABLE 0_grn_batch ADD KEY (`delivery_date`), ADD KEY (`purch_order_no`);
ALTER TABLE 0_supp_invoice_items ADD KEY `Transaction` (`supp_trans_type`, `supp_trans_no`, `stock_id`);
ALTER TABLE 0_purch_order_details ADD KEY `order` (`order_no`, `po_detail_item`);
ALTER TABLE 0_purch_orders ADD KEY (`ord_date`);
ALTER TABLE 0_supp_trans ADD KEY (`tran_date`), DROP PRIMARY KEY, ADD PRIMARY KEY (`type`, `trans_no`);
ALTER TABLE 0_suppliers ADD KEY (`supp_ref`);
ALTER TABLE 0_supp_allocations ADD KEY `From` (`trans_type_from`, `trans_no_from`), ADD KEY `To` (`trans_type_to`, `trans_no_to`);
ALTER TABLE 0_cust_branch DROP KEY `br_name`, ADD KEY (`branch_ref`), ADD KEY (`group_no`);
ALTER TABLE 0_debtors_master ADD KEY (`debtor_ref`);
ALTER TABLE 0_debtor_trans DROP PRIMARY KEY, ADD PRIMARY KEY (`type`, `trans_no`), ADD KEY (`tran_date`);
ALTER TABLE 0_debtor_trans_details ADD KEY `Transaction` (`debtor_trans_type`, `debtor_trans_no`);
ALTER TABLE 0_cust_allocations ADD KEY `From` (`trans_type_from`, `trans_no_from`), ADD KEY `To` (`trans_type_to`, `trans_no_to`);
ALTER TABLE 0_sales_order_details ADD KEY `sorder` (`trans_type`, `order_no`);
ALTER TABLE 0_chart_master ADD KEY `accounts_by_type` (`account_type`, `account_code`);
# fix invalid constraint on databases generated from 2.2 version on en_US-new.sql
#ALTER TABLE `0_tax_types` DROP KEY `name`;

DROP TABLE IF EXISTS `0_sys_prefs`;

CREATE TABLE `0_sys_prefs` (
  `name` varchar(35) NOT NULL default '',
  `category` varchar(30),
  `type` varchar(20) NOT NULL default '',
  `length` smallint(6),
  `value` tinytext NULL,
  PRIMARY KEY  (`name`),
  KEY (`category`)
) TYPE=MyISAM;


INSERT INTO `0_sys_prefs` SELECT 'coy_name','setup.company', 'varchar','60', c.coy_name FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'gst_no','setup.company', 'varchar','25', c.gst_no FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'coy_no','setup.company', 'varchar','25', c.coy_no FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'tax_prd','setup.company', 'int','11', c.tax_prd FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'tax_last','setup.company', 'int','11', c.tax_last FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'postal_address','setup.company', 'tinytext','0', c.postal_address FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'phone','setup.company', 'varchar','30', c.phone FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'fax','setup.company', 'varchar','30',c.fax FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'email','setup.company', 'varchar','100', c.email FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'coy_logo','setup.company', 'varchar','100', c.coy_logo FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'domicile','setup.company', 'varchar','55', c.domicile FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'curr_default','setup.company', 'char','3', c.curr_default FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'use_dimension','setup.company', 'tinyint','1', c.use_dimension FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'f_year','setup.company', 'int','11', c.f_year FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'no_item_list','setup.company', 'tinyint','1', c.no_item_list FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'no_customer_list','setup.company', 'tinyint','1', c.no_customer_list FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'no_supplier_list','setup.company', 'tinyint','1', c.no_supplier_list FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'base_sales','setup.company', 'int','11', c.base_sales FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'time_zone','setup.company', 'tinyint','1', c.time_zone FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'add_pct','setup.company', 'int','5', c.add_pct FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'round_to','setup.company', 'int','5', c.round_to FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'login_tout','setup.company', 'smallint','6', c.login_tout FROM `0_company` c;
#INSERT INTO `0_sys_prefs` SELECT 'foreign_codes','setup.company', 'tinyint','1', c.foreign_codes FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'past_due_days','glsetup.general', 'int','11', c.past_due_days FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'profit_loss_year_act','glsetup.general', 'varchar','15', c.profit_loss_year_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'retained_earnings_act','glsetup.general', 'varchar','15', c.retained_earnings_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'bank_charge_act','glsetup.general', 'varchar','15',  c.bank_charge_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'exchange_diff_act','glsetup.general', 'varchar','15', c.exchange_diff_act FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'default_credit_limit','glsetup.customer', 'int','11', c.default_credit_limit FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'accumulate_shipping','glsetup.customer', 'tinyint','1', c.accumulate_shipping FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'legal_text','glsetup.customer', 'tinytext','0', c.legal_text FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'freight_act','glsetup.customer', 'varchar','15', c.freight_act FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'debtors_act','glsetup.sales', 'varchar','15', c.debtors_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_sales_act','glsetup.sales', 'varchar','15', c.default_sales_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_sales_discount_act','glsetup.sales', 'varchar','15', c.default_sales_discount_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_prompt_payment_act','glsetup.sales', 'varchar','15', c.default_prompt_payment_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_delivery_required','glsetup.sales', 'smallint','6', c.default_delivery_required FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'default_dim_required','glsetup.dims', 'int','11', c.default_dim_required FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'pyt_discount_act','glsetup.purchase', 'varchar','15', c.pyt_discount_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'creditors_act','glsetup.purchase', 'varchar','15', c.creditors_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'po_over_receive','glsetup.purchase', 'int','11', c.po_over_receive FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'po_over_charge','glsetup.purchase', 'int','11', c.po_over_charge FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'allow_negative_stock','glsetup.inventory', 'tinyint','1', c.allow_negative_stock FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'default_inventory_act','glsetup.items', 'varchar','15', c.default_inventory_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_cogs_act','glsetup.items', 'varchar','15', c.default_cogs_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_adj_act','glsetup.items', 'varchar','15', c.default_adj_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_inv_sales_act','glsetup.items', 'varchar','15', c.default_inv_sales_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'default_assembly_act','glsetup.items', 'varchar','15', c.default_assembly_act FROM `0_company` c;

INSERT INTO `0_sys_prefs` SELECT 'default_workorder_required','glsetup.manuf', 'int', '11', c.default_workorder_required FROM `0_company` c;

#INSERT INTO `0_sys_prefs` SELECT 'payroll_act','glsetup.payroll', 'varchar','15', c.payroll_act FROM `0_company` c;
INSERT INTO `0_sys_prefs` SELECT 'version_id', 'system', 'varchar', '11', c.version_id FROM `0_company` c;

ALTER TABLE `0_stock_master` ADD COLUMN `editable` TINYINT(1) NOT NULL default '0';
ALTER TABLE `0_debtor_trans` ADD COLUMN `payment_terms` int(11) default NULL;
ALTER TABLE `0_sales_orders` ADD COLUMN `payment_terms` int(11) default NULL;
ALTER TABLE `0_sales_orders` ADD COLUMN `total` double NOT NULL default '0';
ALTER TABLE `0_purch_orders` ADD COLUMN `total` double NOT NULL default '0';

# change account, groups and classes id's
ALTER TABLE `0_bank_accounts` CHANGE `account_code` `account_code` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_bank_trans` CHANGE `bank_act` `bank_act` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_budget_trans` CHANGE `account` `account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_chart_master` CHANGE `account_code` `account_code` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_chart_master` CHANGE `account_code2` `account_code2` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_cust_branch` CHANGE `sales_account` `sales_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_cust_branch` CHANGE `sales_discount_account` `sales_discount_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_cust_branch` CHANGE `receivables_account` `receivables_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_cust_branch` CHANGE `payment_discount_account` `payment_discount_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_gl_trans` CHANGE `account` `account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_quick_entry_lines` CHANGE `dest_id` `dest_id` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_category` CHANGE `dflt_sales_act` `dflt_sales_act` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_category` CHANGE `dflt_cogs_act` `dflt_cogs_act` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_category` CHANGE `dflt_inventory_act` `dflt_inventory_act` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_category` CHANGE `dflt_adjustment_act` `dflt_adjustment_act` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_category` CHANGE `dflt_assembly_act` `dflt_assembly_act` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_master` CHANGE `sales_account` `sales_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_master` CHANGE `cogs_account` `cogs_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_master` CHANGE `inventory_account` `inventory_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_master` CHANGE `adjustment_account` `adjustment_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_stock_master` CHANGE `assembly_account` `assembly_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_supp_invoice_items` CHANGE `gl_code` `gl_code` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_suppliers` CHANGE `purchase_account` `purchase_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_suppliers` CHANGE `payable_account` `payable_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_suppliers` CHANGE `payment_discount_account` `payment_discount_account` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_tax_types` CHANGE `sales_gl_code` `sales_gl_code` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_tax_types` CHANGE `purchasing_gl_code` `purchasing_gl_code` VARCHAR(15) NOT NULL DEFAULT '';
ALTER TABLE `0_tag_associations` CHANGE `record_id` `record_id` VARCHAR(15) NOT NULL;
ALTER TABLE `0_chart_class` CHANGE `cid` `cid` VARCHAR(3) NOT NULL;
ALTER TABLE `0_chart_master` CHANGE `account_type` `account_type` VARCHAR(10) NOT NULL DEFAULT '0';
ALTER TABLE `0_chart_types` CHANGE `id` `id` VARCHAR(10) NOT NULL;
ALTER TABLE `0_chart_types` CHANGE `parent` `parent` VARCHAR(10) NOT NULL DEFAULT '-1';
ALTER TABLE `0_chart_types` CHANGE `class_id` `class_id` VARCHAR(3) NOT NULL DEFAULT '';

UPDATE `0_chart_types` SET parent='' WHERE parent='0' OR parent='-1';

INSERT INTO `0_sys_prefs` (name, category, type, length, value) VALUES ('auto_curr_reval','setup.company', 'smallint','6', '1');

DROP TABLE IF EXISTS `0_crm_categories`;
CREATE TABLE `0_crm_categories` (
  `id` int(11) NOT NULL auto_increment COMMENT 'pure technical key',
  `type` varchar(20) NOT NULL COMMENT 'contact type e.g. customer' ,
  `action` varchar(20) NOT NULL COMMENT 'detailed usage e.g. department',
  `name` varchar(30) NOT NULL COMMENT 'for category selector',
  `description` tinytext NOT NULL COMMENT 'usage description',
  `system` tinyint(1) NOT NULL default '0' COMMENT 'nonzero for core system usage',
  `inactive` tinyint(1) NOT NULL default '0',
  PRIMARY KEY  (`id`),
  UNIQUE KEY(`type`, `action`),
  UNIQUE KEY(`type`, `name`)
) TYPE=InnoDB ;


INSERT INTO `0_crm_categories` VALUES (1, 'cust_branch', 'general', 'General', 'General contact data for customer branch (overrides company setting)', 1, 0);
INSERT INTO `0_crm_categories` VALUES (2, 'cust_branch', 'invoice', 'Invoices', 'Invoice posting (overrides company setting)', 1, 0);
INSERT INTO `0_crm_categories` VALUES (3, 'cust_branch', 'order', 'Orders', 'Order confirmation (overrides company setting)', 1, 0);
INSERT INTO `0_crm_categories` VALUES (4, 'cust_branch', 'delivery', 'Deliveries', 'Delivery coordination (overrides company setting)', 1, 0);
INSERT INTO `0_crm_categories` VALUES (5, 'customer', 'general', 'General', 'General contact data for customer', 1, 0);
INSERT INTO `0_crm_categories` VALUES (6, 'customer', 'order', 'Orders', 'Order confirmation', 1, 0);
INSERT INTO `0_crm_categories` VALUES (7, 'customer', 'delivery', 'Deliveries', 'Delivery coordination', 1, 0);
INSERT INTO `0_crm_categories` VALUES (8, 'customer', 'invoice', 'Invoices', 'Invoice posting', 1, 0);
INSERT INTO `0_crm_categories` VALUES (9, 'supplier', 'general', 'General', 'General contact data for supplier', 1, 0);
INSERT INTO `0_crm_categories` VALUES (10,'supplier', 'order', 'Orders', 'Order confirmation', 1, 0);
INSERT INTO `0_crm_categories` VALUES (11,'supplier', 'delivery', 'Deliveries', 'Delivery coordination', 1, 0);
INSERT INTO `0_crm_categories` VALUES (12,'supplier', 'invoice', 'Invoices', 'Invoice posting', 1, 0);

DROP TABLE IF EXISTS `0_crm_persons`;

CREATE TABLE `0_crm_persons` (
  `id` int(11) NOT NULL auto_increment,
  `ref` varchar(30) NOT NULL,
  `name` varchar(60) NOT NULL,
  `name2` varchar(60) default NULL,
  `address` tinytext  default NULL,
  `phone` varchar(30) default NULL,
  `phone2` varchar(30) default NULL,
  `fax` varchar(30) default NULL,
  `email` varchar(100)  default NULL,
  `lang` char(5) default NULL,
  `notes` tinytext NOT NULL,
  `tmp_id` varchar(11),
  `tmp_class` varchar(20),
  `inactive` tinyint(1) NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY (`ref`)
) TYPE=InnoDB  AUTO_INCREMENT=1 ;

DROP TABLE IF EXISTS `0_crm_contacts`;

CREATE TABLE `0_crm_contacts` (
  `id` int(11) NOT NULL auto_increment,
  `person_id` int(11) NOT NULL default '0' COMMENT 'foreign key to crm_contacts',
  `type` varchar(20) NOT NULL COMMENT 'foreign key to crm_categories',
  `action` varchar(20) NOT NULL COMMENT 'foreign key to crm_categories',
  `entity_id` varchar(11) NULL COMMENT 'entity id in related class table',
  PRIMARY KEY  (`id`),
  KEY(`type`, `action`)
) TYPE=InnoDB ;


#
# tmp_id, tmp_class fields are used temporarily during upgrade to makethe process easier
#
INSERT INTO `0_crm_persons` (`ref`, `email`, `lang`, `tmp_id`, `tmp_class`) 
	SELECT `debtor_ref`, `email`, if(`curr_code`=d.`lang`, NULL, 'en_GB'), `debtor_no`, 'customer'
		FROM `0_debtors_master`,
			(SELECT `value` as lang FROM `0_sys_prefs` WHERE name='curr_default') d;

INSERT INTO `0_crm_persons` (`ref`, `name`, `address`, `phone`, `phone2`,
		`fax`,`email`, `tmp_id`,`tmp_class`) 
	SELECT `branch_ref`, `contact_name`, `br_address`, `phone`, `phone2`,
		`fax`,`email`,`branch_code`, 'cust_branch' FROM `0_cust_branch`;

INSERT INTO `0_crm_persons` (`ref`, `name`, `address`, `phone`, `phone2`,
		`fax`,`email`,`lang`,`tmp_id`,`tmp_class`) 
	SELECT `supp_ref`, `contact`, `supp_address`, `phone`, `phone2`,
	`fax`,`email`,if(`curr_code`=d.`lang`, NULL, 'en_GB'),`supplier_id`,'supplier' 
		FROM `0_suppliers`,
			(SELECT `value` as lang FROM `0_sys_prefs` WHERE name='curr_default') d;


INSERT INTO `0_crm_contacts` (`person_id`, `type`, `action`, `entity_id`)
	SELECT `id`, `tmp_class`, 'general', `tmp_id`
	FROM `0_crm_persons`;

ALTER TABLE `0_debtor_trans_details` ADD COLUMN `src_id` int(11) default NULL;
ALTER TABLE `0_debtor_trans_details` ADD KEY (`src_id`);
ALTER TABLE `0_suppliers` ADD COLUMN `tax_included` tinyint(1) NOT NULL default '0' AFTER `payment_terms`;
ALTER TABLE `0_supp_trans` ADD COLUMN `tax_included` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_purch_orders` ADD COLUMN `tax_included` tinyint(1) NOT NULL default '0';
