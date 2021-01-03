# Patch for upgrade from 2.2beta to 2.2RC/final

ALTER TABLE `0_tag_associations` DROP COLUMN `id`;
ALTER TABLE `0_tag_associations` ADD  UNIQUE KEY(`record_id`,`tag_id`);

DROP TABLE IF EXISTS `0_useronline` ;

CREATE TABLE `0_useronline` (
	`id` int(11) NOT NULL AUTO_INCREMENT ,
	`timestamp` int(15) NOT NULL default '0',
	`ip` varchar(40) NOT NULL default '',
	`file` varchar(100) NOT NULL default '',
	PRIMARY KEY `id` (`id`) ,
	KEY (`timestamp`) 
) TYPE=MYISAM AUTO_INCREMENT=1;
