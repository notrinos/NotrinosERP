ALTER TABLE `0_supp_trans` ADD COLUMN `tax_included` tinyint(1) NOT NULL default '0';
ALTER TABLE `0_purch_orders` ADD COLUMN `tax_included` tinyint(1) NOT NULL default '0';
UPDATE `0_crm_persons` SET `lang`='C' WHERE `lang`='en_GB';
UPDATE `0_users` SET `language`='C' WHERE `language`='en_GB';
UPDATE `0_suppliers` SET `purchase_account`='';
