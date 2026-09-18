<?php
/** Frozen helpers used only by the PAY-RULE-005 additive custody migrations. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');

function hrm_pay_rule_005_schema_columns($table)
{
    if(!hrm_migration_table_exists($table))return false;$r=db_query('SHOW COLUMNS FROM '.TB_PREF.$table);if(!$r)return false;$out=array();while($x=db_fetch_assoc($r))$out[]=(string)$x['Field'];return $out;
}
function hrm_pay_rule_005_schema_engine_innodb($table)
{
    $physical=hrm_migration_physical_table_name($table);if($physical===false)return false;$r=db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));$x=$r?db_fetch_assoc($r):false;return $x&&strcasecmp((string)$x['ENGINE'],'InnoDB')===0;
}
function hrm_pay_rule_005_trigger_name($code,$operation)
{
    $prefix=hrm_migration_active_table_prefix();$op=strtoupper((string)$operation)==='UPDATE'?'u':'d';if($prefix===false||!preg_match('/^[a-z0-9_]{1,12}$/D',$code))return false;$name=$prefix.'pr5_'.$code.'_'.$op;return strlen($name)<=64?$name:false;
}
function hrm_pay_rule_005_trigger_exists($table,$code,$operation)
{
    $physical=hrm_migration_physical_table_name($table);$name=hrm_pay_rule_005_trigger_name($code,$operation);if($physical===false||$name===false)return false;
    $r=db_query('SELECT COUNT(*) n FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' AND EVENT_OBJECT_TABLE='.db_escape($physical).' AND EVENT_MANIPULATION='.db_escape(strtoupper($operation)));$x=$r?db_fetch_assoc($r):false;return $x&&(int)$x['n']===1;
}
function hrm_pay_rule_005_ensure_triggers($patch,$tables)
{
    foreach($tables as $table=>$code)foreach(array('UPDATE','DELETE') as $op){if(hrm_pay_rule_005_trigger_exists($table,$code,$op))continue;$name=hrm_pay_rule_005_trigger_name($code,$op);if($name===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-RULE-005 immutable trigger name invalid for '.$table.'.');$msg='PAY-RULE-005 immutable custody denies '.strtolower($op).' on '.$table;$sql='CREATE TRIGGER `'.$name.'` BEFORE '.$op.' ON `'.TB_PREF.$table.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($msg);if(!db_query($sql,'could not create PAY-RULE-005 immutable trigger'))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-RULE-005 immutable trigger creation failed for '.$table.' '.$op.'.');}
    foreach($tables as $table=>$code)foreach(array('UPDATE','DELETE') as $op)if(!hrm_pay_rule_005_trigger_exists($table,$code,$op))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-RULE-005 immutable trigger verification failed for '.$table.' '.$op.'.');return true;
}
function hrm_pay_rule_005_install_tables($patch,$sql_file,$expected_columns,$trigger_codes)
{
    $source=@file_get_contents(dirname(dirname(dirname(dirname(__FILE__)))).'/sql/'.$sql_file);if($source===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-RULE-005 custody SQL source is missing: '.$sql_file.'.');
    preg_match_all('/CREATE TABLE `0_([^`]+)`.*?;(?=\s|$)/s',$source,$m,PREG_SET_ORDER);if(count($m)!==count($expected_columns))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-RULE-005 custody SQL table count mismatch for '.$sql_file.'.');
    foreach($m as $x)if(!isset($expected_columns[$x[1]]))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'Unexpected PAY-RULE-005 custody table '.$x[1].'.');
    foreach($m as $x)if(!hrm_migration_table_exists($x[1])&&!db_query(str_replace('0_',TB_PREF,$x[0]),'could not install PAY-RULE-005 custody table'))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-RULE-005 custody table creation failed for '.$x[1].'.');
    foreach($expected_columns as $table=>$columns){$actual=hrm_pay_rule_005_schema_columns($table);if($actual!==$columns||!hrm_pay_rule_005_schema_engine_innodb($table))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-RULE-005 custody schema mismatch for '.$table.'.');}
    return hrm_pay_rule_005_ensure_triggers($patch,$trigger_codes);
}
function hrm_pay_rule_005_require_zero_rows($patch,$tables,$label)
{
    foreach($tables as $table){$r=db_query('SELECT COUNT(*) n FROM '.TB_PREF.$table);$x=$r?db_fetch_assoc($r):false;if(!$x||(int)$x['n']!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,$label.' must activate with zero rows in '.$table.'.');}return true;
}
