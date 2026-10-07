<?php
/** PAY-REC-001 immutable reconciliation/profile/checklist custody schema/readiness helpers. */
include_once(dirname(__FILE__).'/pay_rec_001_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_schema_db.inc');
include_once($path_to_root.'/includes/pay_rec_001_custody_contract.inc');

/** Return whether the custody contract is required for the supplied database version. */
function pay_rec_001_custody_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1049','>=');}
/** Return whether the custody schema is required for the supplied database version. */
function pay_rec_001_custody_schema_required_for_database_version($version){return version_compare((string)$version,'1.0.1050','>=');}
/** Return whether the custody guard is required for the supplied database version. */
function pay_rec_001_custody_guard_required_for_database_version($version){return version_compare((string)$version,'1.0.1051','>=');}

/** Return the exact PAY-REC custody table/column projection. */
function pay_rec_001_custody_tables()
{
    return array(
        'hrm_pay_rec_001_profiles'=>array(
            'reconciliation_profile_id','company_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id',
            'currency_code','policy_version','tolerance_sha256','rounding_sha256','filing_completion_policy_sha256','payroll_scope_sha256',
            'profile_sha256','prepared_by','prepared_at','created_at'
        ),
        'hrm_pay_rec_001_evidence_sets'=>array(
            'reconciliation_evidence_set_id','reconciliation_profile_id','evidence_kind','membership_sha256','evidence_sha256','item_count',
            'amount_minor','status_token','current_evidence','source_contract_sha256','recomputed_by','recomputed_at','created_at'
        ),
        'hrm_pay_rec_001_checklists'=>array(
            'close_checklist_id','reconciliation_profile_id','payroll_evidence_set_id','gl_evidence_set_id','payment_evidence_set_id',
            'filing_evidence_set_id','filing_completion_receipt_sha256','readiness_sha256','checklist_sha256','prepared_by','prepared_at','created_at'
        )
    );
}

/** Return the short immutable-trigger code for one PAY-REC custody table. */
function pay_rec_001_custody_trigger_code($table)
{
    $map=array('hrm_pay_rec_001_profiles'=>'pf','hrm_pay_rec_001_evidence_sets'=>'es','hrm_pay_rec_001_checklists'=>'cl');
    return isset($map[$table])?$map[$table]:false;
}

/** Return exact PAY-REC custody index specifications. */
function pay_rec_001_custody_index_specs()
{
    return array(
        'hrm_pay_rec_001_profiles'=>array(
            'PRIMARY'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('reconciliation_profile_id'))),
            'uq_pr1_pf_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('profile_sha256'))),
            'uq_pr1_pf_scope'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('company_id','legal_entity_id','payroll_period_id','pay_core_run_id','filing_obligation_id','policy_version','tolerance_sha256','rounding_sha256'))),
            'ix_pr1_pf_scope'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('company_id','legal_entity_id','payroll_period_id'))),
            'fk_pr1_pf_run'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('pay_core_run_id'))),
            'fk_pr1_pf_ob'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_obligation_id')))
        ),
        'hrm_pay_rec_001_evidence_sets'=>array(
            'PRIMARY'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('reconciliation_evidence_set_id'))),
            'uq_pr1_es_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('evidence_sha256'))),
            'uq_pr1_es_scope'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('reconciliation_profile_id','evidence_kind','membership_sha256'))),
            'ix_pr1_es_current'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('reconciliation_profile_id','evidence_kind','current_evidence'))),
            'fk_pr1_es_pf'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('reconciliation_profile_id')))
        ),
        'hrm_pay_rec_001_checklists'=>array(
            'PRIMARY'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('close_checklist_id'))),
            'uq_pr1_cl_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('checklist_sha256'))),
            'uq_pr1_cl_ready'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('reconciliation_profile_id','readiness_sha256'))),
            'fk_pr1_cl_pf'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('reconciliation_profile_id'))),
            'fk_pr1_cl_pay'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('payroll_evidence_set_id'))),
            'fk_pr1_cl_gl'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('gl_evidence_set_id'))),
            'fk_pr1_cl_pmt'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('payment_evidence_set_id'))),
            'fk_pr1_cl_fil'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_evidence_set_id')))
        )
    );
}

/** Return exact restrictive PAY-REC custody foreign-key specifications. */
function pay_rec_001_custody_foreign_keys()
{
    return array(
        'hrm_pay_rec_001_profiles'=>array(
            'fk_pr1_pf_run'=>array('referenced_table'=>'hrm_pay_core_009_runs','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'pay_core_run_id','referenced_column_name'=>'run_id'))),
            'fk_pr1_pf_ob'=>array('referenced_table'=>'hrm_pay_fil_001_obligations','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'filing_obligation_id','referenced_column_name'=>'filing_obligation_id')))
        ),
        'hrm_pay_rec_001_evidence_sets'=>array(
            'fk_pr1_es_pf'=>array('referenced_table'=>'hrm_pay_rec_001_profiles','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'reconciliation_profile_id','referenced_column_name'=>'reconciliation_profile_id')))
        ),
        'hrm_pay_rec_001_checklists'=>array(
            'fk_pr1_cl_pf'=>array('referenced_table'=>'hrm_pay_rec_001_profiles','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'reconciliation_profile_id','referenced_column_name'=>'reconciliation_profile_id'))),
            'fk_pr1_cl_pay'=>array('referenced_table'=>'hrm_pay_rec_001_evidence_sets','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'payroll_evidence_set_id','referenced_column_name'=>'reconciliation_evidence_set_id'))),
            'fk_pr1_cl_gl'=>array('referenced_table'=>'hrm_pay_rec_001_evidence_sets','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'gl_evidence_set_id','referenced_column_name'=>'reconciliation_evidence_set_id'))),
            'fk_pr1_cl_pmt'=>array('referenced_table'=>'hrm_pay_rec_001_evidence_sets','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'payment_evidence_set_id','referenced_column_name'=>'reconciliation_evidence_set_id'))),
            'fk_pr1_cl_fil'=>array('referenced_table'=>'hrm_pay_rec_001_evidence_sets','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'filing_evidence_set_id','referenced_column_name'=>'reconciliation_evidence_set_id')))
        )
    );
}

/** Return true only when the immutable denial trigger exactly matches the PAY-REC custody contract. */
function pay_rec_001_custody_trigger_valid($table,$operation)
{
    $prefix=hrm_migration_active_table_prefix();$code=pay_rec_001_custody_trigger_code($table);$physical=hrm_migration_physical_table_name($table);
    if($prefix===false||$code===false||$physical===false)return false;
    $name=$prefix.'pr1_'.$code.'_'.(strtoupper($operation)==='UPDATE'?'u':'d');
    $q=db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if(!$q||db_num_rows($q)!==1)return false;$row=db_fetch_assoc($q);
    $expected="SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable ".$table." row cannot be ".strtolower($operation)."d'";
    $normalise=function($value){$value=preg_replace('/\\s+/',' ',trim((string)$value));$value=preg_replace('/\\s*=\\s*/','=',$value);return strtolower($value);};
    return strtoupper((string)$row['EVENT_MANIPULATION'])===strtoupper($operation)&&(string)$row['EVENT_OBJECT_TABLE']===$physical&&strtoupper((string)$row['ACTION_TIMING'])==='BEFORE'&&$normalise($row['ACTION_STATEMENT'])===$normalise($expected);
}

/** Verify the source-only custody contract and least-privilege security-area registrations. */
function pay_rec_001_require_custody_contract_source($patch)
{
    global $path_to_root;
    if(!pay_rec_001_require_architecture_contract_source($patch))return false;
    $contract=pay_rec_001_custody_contract();
    if($contract['custody_status']!=='ZERO_SEED_IMMUTABLE_FOUNDATION_NO_RUNTIME_WRITER'||$contract['tables']!==array_keys(pay_rec_001_custody_tables())||$contract['runtime_writer_enabled']!==false||$contract['close_state_mutation_enabled']!==false||$contract['automatic_role_grant_allowed']!==false||$contract['production_close_allowed']!==false)
        return pay_rec_001_fail_patch($patch,'PAY-REC-001 custody contract authority invalid.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_rec_001_custody_contract.inc');
    foreach(array('exact_membership_hash_required','independent_recomputation_required','filing_completion_receipt_hash_required','historical_payroll_accounting_filing_mutation_allowed') as $token)
        if(!is_string($raw)||strpos($raw,$token)===false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 custody contract incomplete: '.$token.'.');
    foreach(array('db_query(','INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','file_put_contents(','openssl_sign(') as $token)
        if(stripos($raw,$token)!==false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 custody contract acquired forbidden authority: '.$token.'.');
    $access=@file_get_contents($path_to_root.'/includes/access_levels.inc');
    foreach(array('SA_HRM_PREPARE_PAY_REC','SA_HRM_REVIEW_PAY_REC','SA_HRM_APPROVE_PAY_REC','SA_HRM_CLOSE_PAY_REC','SA_HRM_RECOVER_PAY_REC','SA_HRM_VIEW_PAY_REC') as $token)
        if(!is_string($access)||strpos($access,"'".$token."'")===false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 security area missing: '.$token.'.');
    return true;
}

/** Verify the exact installed PAY-REC custody tables, indexes, FKs and immutable triggers. */
function pay_rec_001_require_custody_schema($patch)
{
    if(!pay_rec_001_require_custody_contract_source($patch))return false;
    $tables=pay_rec_001_custody_tables();$indexes=pay_rec_001_custody_index_specs();$fks=pay_rec_001_custody_foreign_keys();$prefix=hrm_migration_active_table_prefix();
    if($prefix===false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 table prefix unavailable.');
    foreach($tables as $table=>$columns){
        if(pay_fil_001_table_columns($table)!==$columns||!pay_fil_001_table_innodb($table))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-REC-001 custody table projection mismatch: '.$table);
        $actual_indexes=pay_fil_001_table_index_projection($table);$expected_indexes=$indexes[$table];ksort($expected_indexes,SORT_STRING);
        if(!is_array($actual_indexes)||$actual_indexes!==$expected_indexes)return hrm_migration_fail($patch,HRM_MIGRATION_INDEX_MISMATCH,'PAY-REC-001 custody index projection mismatch: '.$table);
        $actual_fks=pay_fil_001_table_foreign_key_projection($table);$expected_fks=$fks[$table];foreach($expected_fks as $name=>$spec)$expected_fks[$name]['referenced_table']=$prefix.$spec['referenced_table'];ksort($expected_fks,SORT_STRING);
        if(!is_array($actual_fks)||$actual_fks!==$expected_fks)return hrm_migration_fail($patch,HRM_MIGRATION_FK_MISMATCH,'PAY-REC-001 custody foreign-key projection mismatch: '.$table);
        foreach(array('UPDATE','DELETE') as $operation)if(!pay_rec_001_custody_trigger_valid($table,$operation))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-REC-001 immutable trigger mismatch: '.$table.'.'.$operation);
    }
    return true;
}

/** Require zero rows in all PAY-REC custody tables at foundation activation. */
function pay_rec_001_require_custody_zero_rows($patch)
{
    foreach(pay_rec_001_custody_tables() as $table=>$columns){$q=db_query('SELECT COUNT(*) AS c FROM '.TB_PREF.$table);$row=$q?db_fetch_assoc($q):false;if(!$row||(int)$row['c']!==0)return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-REC-001 custody must activate zero-seed: '.$table);}
    return true;
}

/** Install the additive zero-seed PAY-REC custody tables and exact denial triggers. */
function pay_rec_001_install_custody_schema($patch)
{
    global $path_to_root;
    if(!pay_rec_001_require_custody_contract_source($patch))return false;
    $raw=@file_get_contents($path_to_root.'/sql/upgrade_pay_rec_001_custody_foundation.sql');
    if(!is_string($raw)||strpos($raw,'-- PAY-REC-001 GROUP reconciliation_profile_checklist_custody')===false)return pay_rec_001_fail_patch($patch,'PAY-REC-001 custody schema source missing.');
    $prefix=hrm_migration_active_table_prefix();if($prefix===false)return false;
    $clean=preg_replace('/^--.*$/m','',$raw);$statements=preg_split('/;\s*(?:\r?\n|$)/',$clean);
    foreach($statements as $statement){$statement=trim($statement);if($statement===''||strpos($statement,'CREATE TRIGGER')===0)continue;$statement=str_replace('`0_','`'.$prefix,$statement);if(!db_query($statement))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-REC-001 custody schema statement failed.');}
    foreach(pay_rec_001_custody_tables() as $table=>$columns){foreach(array('UPDATE','DELETE') as $operation){if(pay_rec_001_custody_trigger_valid($table,$operation))continue;$code=pay_rec_001_custody_trigger_code($table);$name=$prefix.'pr1_'.$code.'_'.($operation==='UPDATE'?'u':'d');$physical=hrm_migration_physical_table_name($table);$message='PAY-REC-001 immutable '.$table.' row cannot be '.strtolower($operation).'d';if(!db_query('CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($message)))return false;}}
    return pay_rec_001_require_custody_schema($patch)&&pay_rec_001_require_custody_zero_rows($patch);
}
?>