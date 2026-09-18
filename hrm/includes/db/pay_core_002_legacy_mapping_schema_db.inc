<?php
/** Frozen schema helper for PAY-CORE-002 legacy-period mapping-review custody. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');

function hrm_pay_core_002_legacy_mapping_review_expected_columns()
{
    return array('mapping_review_id','legacy_period_id','source_period_sha256','review_status','ambiguity_codes','calendar_resolution','group_resolution','legal_entity_resolution','currency_resolution','reviewed_by','reviewed_at','review_sha256','created_at');
}

function hrm_pay_core_002_legacy_mapping_review_trigger_name($op)
{
    $prefix=hrm_migration_active_table_prefix();
    if($prefix===false)return false;
    $suffix=strtoupper($op)==='UPDATE'?'u':'d';
    $name=$prefix.'pc2m_lpmr_'.$suffix;
    return strlen($name)<=64?$name:false;
}

function hrm_pay_core_002_legacy_mapping_review_trigger_exists($op)
{
    $physical=hrm_migration_physical_table_name('hrm_payroll_legacy_period_mapping_reviews');
    $name=hrm_pay_core_002_legacy_mapping_review_trigger_name($op);
    if($physical===false||$name===false)return false;
    $r=db_query('SELECT COUNT(*) n FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' AND EVENT_OBJECT_TABLE='.db_escape($physical).' AND EVENT_MANIPULATION='.db_escape(strtoupper($op)));
    $x=$r?db_fetch_assoc($r):false;
    return $x&&(int)$x['n']===1;
}

function hrm_pay_core_002_install_legacy_mapping_review_custody($patch)
{
    global $path_to_root;
    $table='hrm_payroll_legacy_period_mapping_reviews';
    if(!hrm_migration_table_exists($table)){
        $src=@file_get_contents($path_to_root.'/sql/upgrade_pay_core_002_legacy_mapping_custody.sql');
        if($src===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-CORE-002 legacy mapping custody SQL missing.');
        if(!db_query(str_replace('0_',TB_PREF,$src),'could not create PAY-CORE-002 legacy mapping review custody'))return false;
    }
    $physical=hrm_migration_physical_table_name($table);
    if($physical===false)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CORE-002 legacy mapping custody table unresolved.');
    $r=db_query('SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));
    $x=$r?db_fetch_assoc($r):false;
    if(!$x||strcasecmp((string)$x['ENGINE'],'InnoDB')!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CORE-002 legacy mapping custody engine mismatch.');
    $r=db_query('SHOW COLUMNS FROM '.TB_PREF.$table);$columns=array();while($r&&($x=db_fetch_assoc($r)))$columns[]=(string)$x['Field'];
    if($columns!==hrm_pay_core_002_legacy_mapping_review_expected_columns())return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CORE-002 legacy mapping custody column mismatch.');
    foreach(array('UPDATE','DELETE') as $op){
        if(hrm_pay_core_002_legacy_mapping_review_trigger_exists($op))continue;
        $name=hrm_pay_core_002_legacy_mapping_review_trigger_name($op);
        $sql='CREATE TRIGGER `'.$name.'` BEFORE '.$op.' ON `'.TB_PREF.$table.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape('PAY-CORE-002 legacy mapping review custody denies '.strtolower($op));
        if(!db_query($sql,'could not create PAY-CORE-002 legacy mapping immutable trigger'))return false;
    }
    foreach(array('UPDATE','DELETE') as $op)if(!hrm_pay_core_002_legacy_mapping_review_trigger_exists($op))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CORE-002 legacy mapping immutable trigger verification failed.');
    $r=db_query('SELECT COUNT(*) n FROM '.TB_PREF.$table);$x=$r?db_fetch_assoc($r):false;
    if(!$x||(int)$x['n']!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-CORE-002 legacy mapping custody must activate with zero rows.');
    return true;
}
