ALTER TABLE `0_company` DROP COLUMN `custom1_name`;
ALTER TABLE `0_company` DROP COLUMN `custom2_name`;
ALTER TABLE `0_company` DROP COLUMN `custom3_name`;
ALTER TABLE `0_company` DROP COLUMN `custom1_value`;
ALTER TABLE `0_company` DROP COLUMN `custom2_value`;
ALTER TABLE `0_company` DROP COLUMN `custom3_value`;

ALTER TABLE `0_company` ADD COLUMN `default_delivery_required` SMALLINT(6) NULL DEFAULT '1';
ALTER TABLE `0_company` ADD COLUMN `version_id` VARCHAR(11) NOT NULL DEFAULT '';
ALTER TABLE `0_company` DROP COLUMN `purch_exchange_diff_act`;
ALTER TABLE `0_company` ADD COLUMN`profit_loss_year_act` VARCHAR(11) NOT NULL DEFAULT '' AFTER `exchange_diff_act`;
ALTER TABLE `0_company` ADD COLUMN `time_zone` TINYINT(1) NOT NULL DEFAULT '0';
ALTER TABLE `0_company` ADD COLUMN `add_pct` INT(5) NOT NULL DEFAULT '-1';
ALTER TABLE `0_company` ADD COLUMN `round_to` INT(5) NOT NULL DEFAULT '1';
ALTER TABLE `0_company` CHANGE `grn_act` `bank_charge_act` VARCHAR(11) NOT NULL DEFAULT '';
#INSERT INTO `0_chart_master` VALUES ('9990', '', 'Profit and Loss this year', '52', '0');
UPDATE `0_company` SET `profit_loss_year_act`='9990', `version_id`='2.2' WHERE `coy_code`=1; 

ALTER TABLE `0_stock_category` DROP COLUMN `stock_act`;
ALTER TABLE `0_stock_category` DROP COLUMN `cogs_act`;
ALTER TABLE `0_stock_category` DROP COLUMN `adj_gl_act`;
ALTER TABLE `0_stock_category` DROP COLUMN `purch_price_var_act`;

ALTER TABLE `0_stock_category` ADD COLUMN `dflt_tax_type` int(11) NOT NULL default '1';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_units` varchar(20) NOT NULL default 'each';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_mb_flag` char(1) NOT NULL default 'B';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_sales_act` varchar(11) NOT NULL default '';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_cogs_act` varchar(11) NOT NULL default '';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_inventory_act` varchar(11) NOT NULL default '';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_adjustment_act` varchar(11) NOT NULL default '';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_assembly_act` varchar(11) NOT NULL default '';
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_dim1` int(11) default NULL;
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_dim2` int(11) default NULL;
ALTER TABLE `0_stock_category` ADD COLUMN `dflt_no_sale` tinyint(1) NOT NULL default '0';

ALTER TABLE `0_users` ADD COLUMN `sticky_doc_date` TINYINT(1) DEFAULT '0';
ALTER TABLE `0_users` ADD COLUMN `startup_tab` VARCHAR(20) NOT NULL default 'orders' AFTER `sticky_doc_date`;

ALTER TABLE `0_debtors_master` MODIFY COLUMN `name` varchar(100) NOT NULL default '';

ALTER TABLE `0_cust_branch` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';

ALTER TABLE `0_sys_types` DROP COLUMN `type_name`;

ALTER TABLE `0_chart_class` CHANGE `balance_sheet` `ctype` TINYINT(1) NOT NULL DEFAULT '0';

ALTER TABLE `0_chart_class` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_chart_types` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_movement_types` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_item_tax_types` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_tax_types` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_tax_groups` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';

ALTER TABLE `0_users` DROP PRIMARY KEY;
ALTER TABLE `0_users` ADD `id` SMALLINT(6) AUTO_INCREMENT PRIMARY KEY FIRST;
ALTER TABLE `0_users` ADD UNIQUE KEY (`user_id`);
ALTER TABLE `0_users` ADD COLUMN `inactive` tinyint(1) NOT NULL default '0';

DROP TABLE IF EXISTS `0_audit_trail`;
# fiscal_year, gl_date, gl_seq - journal sequence data
CREATE TABLE `0_audit_trail` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `type` smallint(6) unsigned NOT NULL default '0',
  `trans_no` int(11) unsigned NOT NULL default '0',
  `user` smallint(6) unsigned NOT NULL default '0',
  `stamp` timestamp NOT NULL default CURRENT_TIMESTAMP,
  `description` varchar(60) default NULL,
  `fiscal_year` int(11) NOT NULL,
  `gl_date` date NOT NULL default '0000-00-00',
  `gl_seq` int(11) unsigned default NULL,
   PRIMARY KEY (`id`),
  KEY (`fiscal_year`, `gl_seq`)
) TYPE=InnoDB  ;

ALTER TABLE `0_stock_master` ADD COLUMN `no_sale` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_currencies` ADD COLUMN `auto_update` tinyint(1) NOT NULL default '1';

ALTER TABLE `0_debtors_master` ADD COLUMN `debtor_ref` varchar(30) NOT NULL;
UPDATE `0_debtors_master` SET `debtor_ref`=`name` WHERE 1; 
ALTER TABLE `0_suppliers` ADD COLUMN `supp_ref` varchar(30) NOT NULL;
UPDATE `0_suppliers` SET `supp_ref`=`supp_name` WHERE 1; 
ALTER TABLE `0_cust_branch` ADD COLUMN `branch_ref`	varchar(30) NOT NULL;
UPDATE `0_cust_branch` SET `branch_ref`=`br_name` WHERE 1; 

DROP TABLE IF EXISTS `0_security_roles`;

CREATE TABLE `0_security_roles` (
  `id` int(11) NOT NULL auto_increment,
  `role` varchar(30) NOT NULL,
  `description` varchar(50) default NULL,
  `sections` text,
  `areas` text,
  `inactive` tinyint(1) NOT NULL default '0',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `role` (`role`)
) TYPE=MyISAM AUTO_INCREMENT=1;

ALTER TABLE `0_company` ADD COLUMN `login_tout` SMALLINT(6) NOT NULL DEFAULT '600';
ALTER TABLE `0_users` CHANGE COLUMN `full_access` `role_id` int(11) NOT NULL default '1';

ALTER TABLE `0_sales_order_details` ADD COLUMN `trans_type` SMALLINT(6) NOT NULL DEFAULT '30' AFTER `order_no`;
ALTER TABLE `0_sales_orders` CHANGE COLUMN `order_no` `order_no` int(11) NOT NULL;
ALTER TABLE `0_sales_orders` ADD COLUMN `trans_type` SMALLINT(6) NOT NULL DEFAULT '30' AFTER `order_no`;
ALTER TABLE `0_sales_orders` ADD COLUMN `reference` varchar(100) NOT NULL DEFAULT '' AFTER `branch_code`;
ALTER TABLE `0_sales_orders` DROP PRIMARY KEY;
ALTER TABLE `0_sales_orders` ADD PRIMARY KEY ( `trans_type` , `order_no` ); 
UPDATE `0_sales_orders`	SET `reference`=`order_no` WHERE 1;
INSERT INTO `0_sys_types` (`type_id`, `type_no`, `next_reference`) VALUES (32, 0, '1');

ALTER TABLE `0_bank_accounts` ADD COLUMN `dflt_curr_act` TINYINT(1) NOT NULL default '0' AFTER `bank_curr_code`;

DROP TABLE IF EXISTS `0_tags`;

CREATE TABLE `0_tags` (
  `id` int(11) NOT NULL auto_increment,
  `type` smallint(6) NOT NULL,
  `name` varchar(30) NOT NULL,
  `description` varchar(60) default NULL,
  `inactive` tinyint(1) NOT NULL default '0',
  PRIMARY KEY  (`id`),
  UNIQUE KEY(`type`,`name`)
) TYPE=MyISAM AUTO_INCREMENT=1;

DROP TABLE IF EXISTS `0_tag_associations`;

CREATE TABLE `0_tag_associations` (
  `record_id` varchar(11) NOT NULL,
  `tag_id` int(11) NOT NULL,
  UNIQUE KEY(`record_id`,`tag_id`)
) TYPE=MyISAM;

DROP TABLE IF EXISTS `0_useronline` ;

CREATE TABLE `0_useronline` (
	`id` int(11) NOT NULL AUTO_INCREMENT ,
	`timestamp` int(15) NOT NULL default '0',
	`ip` varchar(40) NOT NULL default '',
	`file` varchar(100) NOT NULL default '',
	PRIMARY KEY `id` (`id`) ,
	KEY (`timestamp`) 
) TYPE=MYISAM AUTO_INCREMENT=1;

ALTER TABLE `0_suppliers` ADD COLUMN `phone2` varchar(30) NOT NULL default '' AFTER `phone`;
ALTER TABLE `0_cust_branch` ADD COLUMN `phone2` varchar(30) NOT NULL default '' AFTER `phone`;
ALTER TABLE `0_shippers` ADD COLUMN `phone2` varchar(30) NOT NULL default '' AFTER `phone`;
ALTER TABLE `0_locations` ADD COLUMN `phone2` varchar(30) NOT NULL default '' AFTER `phone`;
ALTER TABLE `0_debtors_master` ADD COLUMN `notes` tinytext NULL default '' AFTER `credit_limit`;
ALTER TABLE `0_cust_branch` ADD COLUMN `notes` tinytext NULL default '' AFTER `group_no`;
