-- phpMyAdmin SQL Dump
-- version 2.9.0.2
-- http://www.phpmyadmin.net
-- 
-- Host: localhost
-- Generation Time: Mar 20, 2007 at 11:10 AM
-- Server version: 4.1.21
-- PHP Version: 4.4.2
-- 
-- Database: `frontacc_frontacc`
-- 

-- --------------------------------------------------------

-- 
-- ALTER TABLE
-- 

DROP TABLE IF EXISTS `0_item_units`; 
CREATE TABLE IF NOT EXISTS `0_item_units` (
  `abbr` varchar(20) NOT NULL, 
  `name` varchar(40) NOT NULL, 
  `decimals` tinyint(2) NOT NULL,
  PRIMARY KEY (`abbr`),
  UNIQUE KEY `name` (`name`)
) TYPE = MyISAM;

INSERT INTO `0_item_units` (`abbr`, `name`, `decimals`) SELECT DISTINCT `units`, CONCAT(UPPER(SUBSTRING(`units`, 1, 1)), LOWER(SUBSTRING(`units`, 2))), 0 FROM `0_stock_master` ;
UPDATE `0_debtor_trans` SET `ov_amount`=-`ov_amount`, `ov_gst`=-`ov_gst`, `ov_freight`=-`ov_freight`, `ov_discount`=-`ov_discount` WHERE `ov_amount` < 0 AND `type` <> 10 AND `type` <> 13 ;

DROP TABLE IF EXISTS `0_form_items`; 

ALTER TABLE `0_tax_types` DROP INDEX `name`, ADD UNIQUE `name` ( `name` , `rate` );

ALTER TABLE `0_tax_group_items` DROP `included_in_price`;
ALTER TABLE `0_debtor_trans` ADD `ov_freight_tax` DOUBLE DEFAULT '0' NOT NULL AFTER `ov_freight` ;
ALTER TABLE `0_sales_types` ADD `tax_included` INT( 1 ) DEFAULT '0' NOT NULL AFTER `sales_type` ;
ALTER TABLE `0_sales_types` ADD `factor` DOUBLE DEFAULT '1' NOT NULL AFTER `tax_included` ;

ALTER TABLE `0_bom` CHANGE `workcentre_added` `workcentre_added` INT( 11 ) NOT NULL DEFAULT '0';
ALTER TABLE `0_wo_requirements` CHANGE `workcentre` `workcentre` INT( 11 ) NOT NULL DEFAULT '0';

ALTER TABLE `0_debtor_trans` ADD `version` TINYINT(1) UNSIGNED DEFAULT '0' NOT NULL AFTER `type`;
ALTER TABLE `0_sales_orders` ADD `version` TINYINT(1) UNSIGNED DEFAULT '0' NOT NULL AFTER `order_no`;
ALTER TABLE `0_sales_orders` ADD `type` TINYINT(1) NOT NULL DEFAULT '0' AFTER `version`;

ALTER TABLE `0_tax_types` DROP `out`;
ALTER TABLE `0_debtor_trans_details` ADD COLUMN `qty_done` double NOT NULL default '0';

ALTER TABLE `0_debtor_trans` ADD COLUMN `trans_link` int(11) NOT NULL default '0';
INSERT INTO `0_sys_types` VALUES ('13', 'Delivery', '1', '1');
ALTER TABLE `0_sales_order_details` CHANGE `qty_invoiced` `qty_sent` DOUBLE NOT NULL default '0';

ALTER TABLE `0_supp_invoice_items` CHANGE `gl_code` `gl_code` VARCHAR(11) NOT NULL DEFAULT '0';
ALTER TABLE `0_sales_order_details` DROP PRIMARY KEY;
ALTER TABLE `0_sales_order_details` ADD `id` INTEGER(11) NOT NULL AUTO_INCREMENT FIRST, ADD PRIMARY KEY (`id`);

ALTER TABLE `0_company` ADD `no_item_list` TINYINT(1) NOT NULL DEFAULT '0' AFTER `f_year`;
ALTER TABLE `0_company` ADD `no_customer_list` TINYINT(1) NOT NULL DEFAULT '0' AFTER `no_item_list`;
ALTER TABLE `0_company` ADD `no_supplier_list` TINYINT(1) NOT NULL DEFAULT '0' AFTER `no_customer_list`;
ALTER TABLE `0_company` ADD `base_sales` INT( 11 ) DEFAULT '-1' NOT NULL AFTER `no_supplier_list` ;

ALTER TABLE `0_salesman` ADD `provision` DOUBLE NOT NULL DEFAULT '0' AFTER `salesman_email`;
ALTER TABLE `0_salesman` ADD `break_pt` DOUBLE NOT NULL DEFAULT '0' AFTER `provision`;
ALTER TABLE `0_salesman` ADD `provision2` DOUBLE NOT NULL DEFAULT '0' AFTER `break_pt`;

