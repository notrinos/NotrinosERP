<?php
/** PAY-FIL-001 TT80 semantic source-of-truth custody schema/readiness helpers. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_completion_schema_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_semantic_source_custody_contract.inc');

function pay_fil_001_tt80_semantic_source_custody_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1031','>=');}
function pay_fil_001_tt80_semantic_source_custody_schema_required_for_database_version($version){return version_compare((string)$version,'1.0.1032','>=');}
function pay_fil_001_tt80_semantic_source_custody_guard_required_for_database_version($version){return version_compare((string)$version,'1.0.1033','>=');}

function pay_fil_001_tt80_semantic_source_custody_tables()
{
    return array(
        'hrm_pay_fil_001_tt80_taxpayer_profiles'=>array(
            'tt80_taxpayer_profile_id','filing_obligation_id','taxpayer_tax_code','taxpayer_name','taxpayer_address',
            'taxpayer_ward_code','taxpayer_ward_name','taxpayer_province_code','taxpayer_province_name',
            'tax_office_code','tax_office_name','source_evidence_sha256','profile_sha256','captured_by','captured_at','created_at'
        ),
        'hrm_pay_fil_001_tt80_person_facts'=>array(
            'tt80_person_fact_id','filing_source_set_id','subject_key','paid_worker','withheld_person','tax_residency',
            'employment_contract_term_class','filing_taxable_income_minor','withholding_subject_taxable_income_minor',
            'personal_income_tax_withheld_minor','foreign_insurer_nonmandatory_insurance_taxable_minor',
            'foreign_insurer_nonmandatory_insurance_withheld_minor','petroleum_contract_exempt_taxable_minor',
            'resolution_exempt_income_minor','currency_code','source_evidence_sha256','fact_sha256','captured_by','captured_at','created_at'
        ),
        'hrm_pay_fil_001_tt80_person_fact_links'=>array(
            'tt80_person_fact_link_id','tt80_person_fact_id','filing_source_link_id','source_link_sha256',
            'binding_role','binding_sha256','created_at'
        )
    );
}
function pay_fil_001_tt80_semantic_source_custody_trigger_code($table)
{
    $m=array(
        'hrm_pay_fil_001_tt80_taxpayer_profiles'=>'tp',
        'hrm_pay_fil_001_tt80_person_facts'=>'pf',
        'hrm_pay_fil_001_tt80_person_fact_links'=>'pl'
    );
    return isset($m[$table])?$m[$table]:false;
}
function pay_fil_001_tt80_semantic_source_custody_index_specs()
{
    return array(
        'hrm_pay_fil_001_tt80_taxpayer_profiles'=>array(
            'PRIMARY'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('tt80_taxpayer_profile_id'))),
            'uq_pfl1_tp_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('profile_sha256'))),
            'uq_pfl1_tp_ob_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_obligation_id','profile_sha256'))),
            'fk_pfl1_tp_ob'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_obligation_id')))
        ),
        'hrm_pay_fil_001_tt80_person_facts'=>array(
            'PRIMARY'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('tt80_person_fact_id'))),
            'uq_pfl1_pf_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('fact_sha256'))),
            'uq_pfl1_pf_set_sub_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_source_set_id','subject_key','fact_sha256'))),
            'ix_pfl1_pf_set_sub'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_source_set_id','subject_key'))),
            'fk_pfl1_pf_set'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_source_set_id')))
        ),
        'hrm_pay_fil_001_tt80_person_fact_links'=>array(
            'PRIMARY'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('tt80_person_fact_link_id'))),
            'uq_pfl1_pl_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('binding_sha256'))),
            'uq_pfl1_pl_fact_link_role'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('tt80_person_fact_id','filing_source_link_id','binding_role'))),
            'fk_pfl1_pl_fact'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('tt80_person_fact_id'))),
            'fk_pfl1_pl_link'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_source_link_id')))
        )
    );
}
function pay_fil_001_tt80_semantic_source_custody_foreign_keys()
{
    return array(
        'hrm_pay_fil_001_tt80_taxpayer_profiles'=>array(
            'fk_pfl1_tp_ob'=>array('referenced_table'=>'hrm_pay_fil_001_obligations','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'filing_obligation_id','referenced_column_name'=>'filing_obligation_id')))
        ),
        'hrm_pay_fil_001_tt80_person_facts'=>array(
            'fk_pfl1_pf_set'=>array('referenced_table'=>'hrm_pay_fil_001_source_sets','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'filing_source_set_id','referenced_column_name'=>'filing_source_set_id')))
        ),
        'hrm_pay_fil_001_tt80_person_fact_links'=>array(
            'fk_pfl1_pl_fact'=>array('referenced_table'=>'hrm_pay_fil_001_tt80_person_facts','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'tt80_person_fact_id','referenced_column_name'=>'tt80_person_fact_id'))),
            'fk_pfl1_pl_link'=>array('referenced_table'=>'hrm_pay_fil_001_source_links','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'filing_source_link_id','referenced_column_name'=>'filing_source_link_id')))
        )
    );
}
function pay_fil_001_tt80_semantic_source_custody_trigger_valid($table,$operation)
{
    $prefix=hrm_migration_active_table_prefix();$code=pay_fil_001_tt80_semantic_source_custody_trigger_code($table);
    $physical=hrm_migration_physical_table_name($table);
    if($prefix===false||$code===false||$physical===false)return false;
    $name=$prefix.'pfl1_'.$code.'_'.(strtoupper($operation)==='UPDATE'?'u':'d');
    $q=db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if(!$q||db_num_rows($q)!==1)return false;$r=db_fetch_assoc($q);
    return strtoupper((string)$r['EVENT_MANIPULATION'])===strtoupper($operation)
        &&(string)$r['EVENT_OBJECT_TABLE']===$physical
        &&strtoupper((string)$r['ACTION_TIMING'])==='BEFORE'
        &&strpos((string)$r['ACTION_STATEMENT'],'PAY-FIL-001 immutable')!==false;
}
function pay_fil_001_tt80_semantic_source_custody_require_contract_source($patch)
{
    global $path_to_root;
    if(!pay_fil_001_require_tt80_semantic_architecture_guard_source($patch))return false;
    $c=pay_fil_001_tt80_semantic_source_custody_contract();
    if(($c['custody_status']??'')!=='CUSTODY_DESIGN_ACCEPTED_ADMISSION_REVIEW_NOT_ENABLED'
        ||($c['semantic_mapping_publication_allowed']??true)!==false
        ||($c['automatic_inference_allowed']??true)!==false
        ||($c['historical_mutation_allowed']??true)!==false)
        return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source custody authority invalid.');
    if(($c['taxpayer_fields']??array())!==pay_fil_001_tt80_semantic_taxpayer_facts())
        return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 taxpayer custody field set drift.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_fil_001_tt80_semantic_source_custody_contract.inc');
    foreach(array('explicit_source_evidence_sha256','one_or_more_existing_pay_fil_source_links','automatic_inference_allowed','semantic_mapping_publication_allowed') as $token)
        if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source custody contract incomplete: '.$token.'.');
    foreach(array('db_query(','INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','file_put_contents(','openssl_sign(') as $token)
        if(stripos($raw,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source custody contract acquired forbidden authority: '.$token.'.');
    return true;
}
function pay_fil_001_tt80_semantic_source_custody_require_schema($patch)
{
    if(!pay_fil_001_require_schema($patch)||!pay_fil_001_completion_require_groups($patch,array('internal_render_artifact','validation_evidence','official_schema_authority','authorization_intent')))return false;
    $tables=pay_fil_001_tt80_semantic_source_custody_tables();$indexes=pay_fil_001_tt80_semantic_source_custody_index_specs();$fks=pay_fil_001_tt80_semantic_source_custody_foreign_keys();
    $prefix=hrm_migration_active_table_prefix();if($prefix===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-FIL-001 TT80 custody table prefix unavailable.');
    foreach($tables as $table=>$columns){
        if(pay_fil_001_table_columns($table)!==$columns||!pay_fil_001_table_innodb($table))
            return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-FIL-001 TT80 semantic custody table projection mismatch: '.$table);
        $actual=pay_fil_001_table_index_projection($table);$want=$indexes[$table];ksort($want,SORT_STRING);
        if(!is_array($actual)||$actual!==$want)return hrm_migration_fail($patch,HRM_MIGRATION_INDEX_MISMATCH,'PAY-FIL-001 TT80 semantic custody index projection mismatch: '.$table);
        $actual_fk=pay_fil_001_table_foreign_key_projection($table);$want_fk=$fks[$table];
        foreach($want_fk as $name=>$spec)$want_fk[$name]['referenced_table']=$prefix.$spec['referenced_table'];
        ksort($want_fk,SORT_STRING);
        if(!is_array($actual_fk)||$actual_fk!==$want_fk)return hrm_migration_fail($patch,HRM_MIGRATION_FK_MISMATCH,'PAY-FIL-001 TT80 semantic custody foreign-key projection mismatch: '.$table);
        foreach(array('UPDATE','DELETE') as $operation)
            if(!pay_fil_001_tt80_semantic_source_custody_trigger_valid($table,$operation))
                return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-FIL-001 TT80 semantic custody immutable trigger mismatch: '.$table.'.'.$operation);
    }
    return true;
}
function pay_fil_001_tt80_semantic_source_custody_require_zero_rows($patch)
{
    foreach(pay_fil_001_tt80_semantic_source_custody_tables() as $table=>$columns){
        $q=db_query('SELECT COUNT(*) AS c FROM '.TB_PREF.$table);$r=$q?db_fetch_assoc($q):false;
        if(!$r||(int)$r['c']!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-FIL-001 TT80 semantic custody must activate zero-seed: '.$table);
    }
    return true;
}
function pay_fil_001_tt80_semantic_source_custody_install_schema($patch)
{
    global $path_to_root;
    if(!pay_fil_001_tt80_semantic_source_custody_require_contract_source($patch))return false;
    $raw=@file_get_contents($path_to_root.'/sql/upgrade_pay_fil_001_tt80_semantic_source_custody.sql');
    if(!is_string($raw)||strpos($raw,'-- PAY-FIL-001 GROUP tt80_semantic_source_custody')===false)
        return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-FIL-001 TT80 semantic custody schema source missing.');
    $prefix=hrm_migration_active_table_prefix();if($prefix===false)return false;
    $clean=preg_replace('/^--.*$/m','',$raw);$statements=preg_split('/;\s*(?:\r?\n|$)/',$clean);
    foreach($statements as $statement){
        $statement=trim($statement);if($statement===''||strpos($statement,'CREATE TRIGGER')===0)continue;
        $statement=str_replace('`0_','`'.$prefix,$statement);
        if(!db_query($statement))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-FIL-001 TT80 semantic custody schema statement failed.');
    }
    foreach(pay_fil_001_tt80_semantic_source_custody_tables() as $table=>$columns){
        foreach(array('UPDATE','DELETE') as $operation){
            if(pay_fil_001_tt80_semantic_source_custody_trigger_valid($table,$operation))continue;
            $code=pay_fil_001_tt80_semantic_source_custody_trigger_code($table);
            $name=$prefix.'pfl1_'.$code.'_'.($operation==='UPDATE'?'u':'d');
            $physical=hrm_migration_physical_table_name($table);
            $message='PAY-FIL-001 immutable '.$table.' row cannot be '.strtolower($operation).'d';
            if(!db_query('CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($message)))return false;
        }
    }
    return pay_fil_001_tt80_semantic_source_custody_require_schema($patch)&&pay_fil_001_tt80_semantic_source_custody_require_zero_rows($patch);
}
function pay_fil_001_tt80_semantic_source_custody_require_runtime_guard_source($patch)
{
    global $path_to_root;
    if(!pay_fil_001_tt80_semantic_source_custody_require_contract_source($patch))return false;
    $raw=@file_get_contents($path_to_root.'/hrm/includes/db/pay_fil_001_tt80_renderer_runtime_db.inc');
    foreach(array('semantic_source_incomplete','CUSTODY_DESIGN_ACCEPTED_ADMISSION_REVIEW_NOT_ENABLED','semantic_source_custody_contract_sha256','semantic_source_admission_unreviewed') as $token)
        if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic custody runtime guard incomplete: '.$token.'.');
    foreach(array('INSERT INTO','UPDATE ','DELETE FROM','begin_transaction(','commit_transaction(','file_put_contents(','curl_','fsockopen(','stream_socket_client(','openssl_sign(') as $token)
        if(stripos($raw,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic custody runtime guard is not read-only: '.$token.'.');
    return true;
}
?>