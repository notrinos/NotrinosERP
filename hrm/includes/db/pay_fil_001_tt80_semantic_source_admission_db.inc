<?php
/** PAY-FIL-001 TT80 governed semantic source admission/review and completeness custody. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_source_custody_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_semantic_source_admission_contract.inc');

function pay_fil_001_tt80_semantic_source_admission_authority_required_for_database_version($version){return version_compare((string)$version,'1.0.1034','>=');}
function pay_fil_001_tt80_semantic_source_review_custody_required_for_database_version($version){return version_compare((string)$version,'1.0.1035','>=');}
function pay_fil_001_tt80_semantic_source_admission_writer_required_for_database_version($version){return version_compare((string)$version,'1.0.1036','>=');}

function pay_fil_001_tt80_semantic_source_review_table()
{
    return array('hrm_pay_fil_001_tt80_semantic_reviews'=>array(
        'tt80_semantic_review_id','filing_obligation_id','filing_source_set_id','tt80_taxpayer_profile_id','taxpayer_profile_sha256',
        'expected_subject_count','expected_subject_set_sha256','subject_roster_evidence_sha256','person_fact_count','person_fact_set_sha256',
        'person_fact_link_count','person_fact_link_set_sha256','source_evidence_set_sha256','maker_set_sha256','completeness_status',
        'findings_json','findings_sha256','review_sha256','reviewed_by','reviewed_at','created_at'
    ));
}
function pay_fil_001_tt80_semantic_source_review_index_specs()
{
    return array('hrm_pay_fil_001_tt80_semantic_reviews'=>array(
        'PRIMARY'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('tt80_semantic_review_id'))),
        'uq_pfl1_sr_sha'=>array('non_unique'=>0,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('review_sha256'))),
        'ix_pfl1_sr_set_status'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_source_set_id','completeness_status'))),
        'fk_pfl1_sr_ob'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_obligation_id'))),
        'fk_pfl1_sr_set'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('filing_source_set_id'))),
        'fk_pfl1_sr_tp'=>array('non_unique'=>1,'index_type'=>'BTREE','columns'=>pay_fil_001_schema_index_columns(array('tt80_taxpayer_profile_id')))
    ));
}
function pay_fil_001_tt80_semantic_source_review_foreign_keys()
{
    return array('hrm_pay_fil_001_tt80_semantic_reviews'=>array(
        'fk_pfl1_sr_ob'=>array('referenced_table'=>'hrm_pay_fil_001_obligations','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'filing_obligation_id','referenced_column_name'=>'filing_obligation_id'))),
        'fk_pfl1_sr_set'=>array('referenced_table'=>'hrm_pay_fil_001_source_sets','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'filing_source_set_id','referenced_column_name'=>'filing_source_set_id'))),
        'fk_pfl1_sr_tp'=>array('referenced_table'=>'hrm_pay_fil_001_tt80_taxpayer_profiles','update_rule'=>'RESTRICT','delete_rule'=>'RESTRICT','columns'=>array(array('sequence'=>1,'column_name'=>'tt80_taxpayer_profile_id','referenced_column_name'=>'tt80_taxpayer_profile_id')))
    ));
}
function pay_fil_001_tt80_semantic_source_review_trigger_valid($operation)
{
    $prefix=hrm_migration_active_table_prefix();$physical=hrm_migration_physical_table_name('hrm_pay_fil_001_tt80_semantic_reviews');
    if($prefix===false||$physical===false)return false;
    $name=$prefix.'pfl1_sr_'.(strtoupper($operation)==='UPDATE'?'u':'d');
    $q=db_query('SELECT EVENT_MANIPULATION,EVENT_OBJECT_TABLE,ACTION_TIMING,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND TRIGGER_NAME='.db_escape($name).' LIMIT 2');
    if(!$q||db_num_rows($q)!==1)return false;$r=db_fetch_assoc($q);
    return strtoupper((string)$r['EVENT_MANIPULATION'])===strtoupper($operation)
        &&(string)$r['EVENT_OBJECT_TABLE']===$physical
        &&strtoupper((string)$r['ACTION_TIMING'])==='BEFORE'
        &&strpos((string)$r['ACTION_STATEMENT'],'PAY-FIL-001 immutable')!==false;
}

function pay_fil_001_tt80_semantic_source_admission_require_authority_source($patch)
{
    global $path_to_root;
    if(!pay_fil_001_tt80_semantic_source_custody_require_contract_source($patch))return false;
    $c=pay_fil_001_tt80_semantic_source_admission_contract();
    if($c['prepare_permission']!==PAY_FIL_001_PREPARE_PERMISSION||$c['review_permission']!==PAY_FIL_001_REVIEW_PERMISSION
        ||$c['maker_checker_required']!==true||$c['admission_status']!=='GOVERNED_ADMISSION_REVIEW_ENABLED_MAPPING_UNPUBLISHED'
        ||$c['semantic_mapping_publication_allowed']!==false||$c['runtime_generation_allowed']!==false
        ||$c['production_transmission_allowed']!==false||$c['automatic_inference_allowed']!==false)
        return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source admission authority invalid.');
    $raw=@file_get_contents($path_to_root.'/includes/pay_fil_001_tt80_semantic_source_admission_contract.inc');
    foreach(array('PAY_FIL_001_PREPARE_PERMISSION','PAY_FIL_001_REVIEW_PERMISSION','maker_checker_required','subject_roster_evidence_sha256','semantic_mapping_publication_allowed','automatic_admission_allowed') as $token)
        if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source admission contract incomplete: '.$token.'.');
    foreach(array('db_query(','INSERT INTO','UPDATE ','DELETE FROM','curl_','fsockopen(','stream_socket_client(','file_put_contents(','openssl_sign(') as $token)
        if(stripos($raw,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic source admission contract acquired forbidden authority: '.$token.'.');
    return true;
}
function pay_fil_001_tt80_semantic_source_review_require_schema($patch)
{
    if(!pay_fil_001_tt80_semantic_source_custody_require_schema($patch))return false;
    $tables=pay_fil_001_tt80_semantic_source_review_table();$indexes=pay_fil_001_tt80_semantic_source_review_index_specs();$fks=pay_fil_001_tt80_semantic_source_review_foreign_keys();
    $prefix=hrm_migration_active_table_prefix();if($prefix===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-FIL-001 TT80 semantic review table prefix unavailable.');
    foreach($tables as $table=>$columns){
        if(pay_fil_001_table_columns($table)!==$columns||!pay_fil_001_table_innodb($table))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-FIL-001 TT80 semantic review table projection mismatch: '.$table);
        $actual=pay_fil_001_table_index_projection($table);$want=$indexes[$table];ksort($want,SORT_STRING);if(!is_array($actual)||$actual!==$want)return hrm_migration_fail($patch,HRM_MIGRATION_INDEX_MISMATCH,'PAY-FIL-001 TT80 semantic review index projection mismatch: '.$table);
        $actual_fk=pay_fil_001_table_foreign_key_projection($table);$want_fk=$fks[$table];foreach($want_fk as $name=>$spec)$want_fk[$name]['referenced_table']=$prefix.$spec['referenced_table'];ksort($want_fk,SORT_STRING);if(!is_array($actual_fk)||$actual_fk!==$want_fk)return hrm_migration_fail($patch,HRM_MIGRATION_FK_MISMATCH,'PAY-FIL-001 TT80 semantic review foreign-key projection mismatch: '.$table);
        foreach(array('UPDATE','DELETE') as $operation)if(!pay_fil_001_tt80_semantic_source_review_trigger_valid($operation))return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-FIL-001 TT80 semantic review immutable trigger mismatch: '.$operation);
    }
    return true;
}
function pay_fil_001_tt80_semantic_source_review_require_zero_rows($patch)
{
    $q=db_query('SELECT COUNT(*) AS c FROM '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_reviews');$r=$q?db_fetch_assoc($q):false;
    return $r&&(int)$r['c']===0?true:hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-FIL-001 TT80 semantic review custody must activate zero-seed.');
}
function pay_fil_001_tt80_semantic_source_review_install_schema($patch)
{
    global $path_to_root;
    if(!pay_fil_001_tt80_semantic_source_admission_require_authority_source($patch))return false;
    $raw=@file_get_contents($path_to_root.'/sql/upgrade_pay_fil_001_tt80_semantic_source_review.sql');
    if(!is_string($raw)||strpos($raw,'-- PAY-FIL-001 GROUP tt80_semantic_source_review')===false)return hrm_migration_fail($patch,HRM_MIGRATION_TAMPER_SUSPECT,'PAY-FIL-001 TT80 semantic review schema source missing.');
    $prefix=hrm_migration_active_table_prefix();if($prefix===false)return false;$clean=preg_replace('/^--.*$/m','',$raw);$statements=preg_split('/;\s*(?:\r?\n|$)/',$clean);
    foreach($statements as $statement){$statement=trim($statement);if($statement===''||strpos($statement,'CREATE TRIGGER')===0)continue;$statement=str_replace('`0_','`'.$prefix,$statement);if(!db_query($statement))return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'PAY-FIL-001 TT80 semantic review schema statement failed.');}
    foreach(array('UPDATE','DELETE') as $operation){if(pay_fil_001_tt80_semantic_source_review_trigger_valid($operation))continue;$name=$prefix.'pfl1_sr_'.($operation==='UPDATE'?'u':'d');$physical=hrm_migration_physical_table_name('hrm_pay_fil_001_tt80_semantic_reviews');$message='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_semantic_reviews row cannot be '.strtolower($operation).'d';if(!db_query('CREATE TRIGGER `'.$name.'` BEFORE '.$operation.' ON `'.$physical.'` FOR EACH ROW SIGNAL SQLSTATE \'45000\' SET MESSAGE_TEXT='.db_escape($message)))return false;}
    return pay_fil_001_tt80_semantic_source_review_require_schema($patch)&&pay_fil_001_tt80_semantic_source_review_require_zero_rows($patch);
}

function pay_fil_001_tt80_semantic_source_closed_input(array $input,array $fields)
{
    $a=array_keys($input);$b=$fields;sort($a,SORT_STRING);sort($b,SORT_STRING);return $a===$b;
}
function pay_fil_001_tt80_semantic_source_text($value,$max)
{
    $value=trim((string)$value);return $value!==''&&strlen($value)<=$max?$value:false;
}
function pay_fil_001_tt80_semantic_source_input_fail(&$error,$code)
{
    $error=(string)$code;return false;
}
function pay_fil_001_tt80_semantic_source_validate_taxpayer_profile(array $input,$filing_obligation_id,&$error)
{
    $fields=pay_fil_001_tt80_semantic_custody_taxpayer_fields();$all=$fields;$all[]='source_evidence_sha256';if(!pay_fil_001_tt80_semantic_source_closed_input($input,$all))return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_taxpayer_profile_fields_invalid');
    $out=array('filing_obligation_id'=>(int)$filing_obligation_id);
    foreach($fields as $field){$max=$field==='taxpayer_address'?512:($field==='taxpayer_name'||substr($field,-5)==='_name'?255:32);$v=pay_fil_001_tt80_semantic_source_text($input[$field],$max);if($v===false)return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_taxpayer_profile_value_invalid');if($max===32&&pay_fil_001_token($v,$max)===false)return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_taxpayer_profile_code_invalid');$out[$field]=$v;}
    if(!pay_fil_001_sha($input['source_evidence_sha256']))return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_taxpayer_profile_evidence_invalid');$out['source_evidence_sha256']=$input['source_evidence_sha256'];$out['profile_sha256']=pay_fil_001_hash($out);return $out;
}
function pay_fil_001_tt80_semantic_source_validate_person_fact(array $input,$filing_source_set_id,&$error)
{
    $fields=pay_fil_001_tt80_semantic_custody_person_fields();$all=$fields;$all[]='source_evidence_sha256';if(!pay_fil_001_tt80_semantic_source_closed_input($input,$all))return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_person_fact_fields_invalid');
    $out=array('filing_source_set_id'=>(int)$filing_source_set_id);$subject=pay_fil_001_token($input['subject_key'],128);if($subject===false)return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_subject_key_invalid');$out['subject_key']=$subject;
    foreach(array('paid_worker','withheld_person') as $field){$v=(string)$input[$field];if($v!=='0'&&$v!=='1')return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_person_boolean_invalid');$out[$field]=(int)$v;}
    $res=(string)$input['tax_residency'];if(!in_array($res,array('resident','nonresident','unknown'),true))return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_tax_residency_invalid');$out['tax_residency']=$res;
    $term=(string)$input['employment_contract_term_class'];if(!in_array($term,array('at_least_3_months','under_3_months','not_applicable','unknown'),true))return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_contract_term_invalid');$out['employment_contract_term_class']=$term;
    foreach(array('filing_taxable_income_minor','withholding_subject_taxable_income_minor','personal_income_tax_withheld_minor','foreign_insurer_nonmandatory_insurance_taxable_minor','foreign_insurer_nonmandatory_insurance_withheld_minor','petroleum_contract_exempt_taxable_minor','resolution_exempt_income_minor') as $field){$v=pay_fil_001_uint($input[$field]);if($v===false)return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_person_amount_invalid');$out[$field]=$v;}
    if((string)$input['currency_code']!=='VND')return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_person_currency_invalid');$out['currency_code']='VND';if(!pay_fil_001_sha($input['source_evidence_sha256']))return pay_fil_001_tt80_semantic_source_input_fail($error,'tt80_person_evidence_invalid');$out['source_evidence_sha256']=$input['source_evidence_sha256'];$out['fact_sha256']=pay_fil_001_hash($out);return $out;
}
function pay_fil_001_tt80_semantic_source_profile_row_matches(array $row,array $profile)
{
    if((int)$row['filing_obligation_id']!==(int)$profile['filing_obligation_id'])return false;foreach(pay_fil_001_tt80_semantic_custody_taxpayer_fields() as $field)if((string)$row[$field]!== (string)$profile[$field])return false;return (string)$row['source_evidence_sha256']===(string)$profile['source_evidence_sha256']&&hash_equals((string)$row['profile_sha256'],(string)$profile['profile_sha256']);
}
function pay_fil_001_tt80_semantic_source_fact_row_matches(array $row,array $fact)
{
    if((int)$row['filing_source_set_id']!==(int)$fact['filing_source_set_id'])return false;foreach(array('paid_worker','withheld_person','filing_taxable_income_minor','withholding_subject_taxable_income_minor','personal_income_tax_withheld_minor','foreign_insurer_nonmandatory_insurance_taxable_minor','foreign_insurer_nonmandatory_insurance_withheld_minor','petroleum_contract_exempt_taxable_minor','resolution_exempt_income_minor') as $field)if((int)$row[$field]!== (int)$fact[$field])return false;foreach(array('subject_key','tax_residency','employment_contract_term_class','currency_code','source_evidence_sha256','fact_sha256') as $field)if((string)$row[$field]!== (string)$fact[$field])return false;return true;
}

/** Append or replay one closed semantic source bundle. No mapping or filing artifact is produced. */
function pay_fil_001_tt80_admit_semantic_source_bundle($filing_source_set_id,array $taxpayer_profile,array $person_facts,array $bindings,&$error=null)
{
    $error=null;if(!pay_fil_001_require_access(PAY_FIL_001_PREPARE_PERMISSION,$error))return false;$actor=pay_fil_001_actor();$company=pay_fil_001_company();if($actor<=0||$company<0)return pay_fil_001_runtime_fail($error,'actor_or_company_missing');
    $set_id=pay_fil_001_posint($filing_source_set_id);if($set_id===false)return pay_fil_001_runtime_fail($error,'filing_source_set_invalid');
    $s=pay_fil_001_one('SELECT s.filing_source_set_id,s.filing_obligation_id,s.currency_code,o.company_id,o.jurisdiction_code,o.obligation_type,o.schema_id,o.schema_version,o.connector_id,o.connector_version,o.obligation_sha256 FROM '.TB_PREF.'hrm_pay_fil_001_source_sets s INNER JOIN '.TB_PREF.'hrm_pay_fil_001_obligations o ON o.filing_obligation_id=s.filing_obligation_id WHERE s.filing_source_set_id='.(int)$set_id.' LIMIT 2');
    if(!$s||(int)$s['company_id']!==$company)return pay_fil_001_runtime_fail($error,'filing_source_set_missing_or_scope_denied');if($s['jurisdiction_code']!=='VN'||$s['obligation_type']!=='personal-income-tax-withholding'||$s['currency_code']!=='VND'||$s['schema_id']!=='internal-reconciliation-only'||$s['schema_version']!=='1'||$s['connector_id']!=='none'||$s['connector_version']!=='0')return pay_fil_001_runtime_fail($error,'tt80_source_contract_not_eligible');
    $profile=pay_fil_001_tt80_semantic_source_validate_taxpayer_profile($taxpayer_profile,(int)$s['filing_obligation_id'],$error);if($profile===false)return false;
    $facts=array();$subjects=array();foreach($person_facts as $input){if(!is_array($input))return pay_fil_001_runtime_fail($error,'tt80_person_fact_invalid');$fact=pay_fil_001_tt80_semantic_source_validate_person_fact($input,$set_id,$error);if($fact===false)return false;if(isset($subjects[$fact['subject_key']]))return pay_fil_001_runtime_fail($error,'tt80_subject_duplicate_in_bundle');$subjects[$fact['subject_key']]=true;$facts[$fact['subject_key']]=$fact;}ksort($facts,SORT_STRING);
    $binding_inputs=array();foreach($bindings as $binding){if(!is_array($binding)||!pay_fil_001_tt80_semantic_source_closed_input($binding,array('subject_key','filing_source_link_id','source_link_sha256','binding_role')))return pay_fil_001_runtime_fail($error,'tt80_person_binding_fields_invalid');$subject=pay_fil_001_token($binding['subject_key'],128);$link_id=pay_fil_001_posint($binding['filing_source_link_id']);$role=pay_fil_001_token($binding['binding_role'],32);if($subject===false||$link_id===false||$role===false||!pay_fil_001_sha($binding['source_link_sha256'])||!isset($facts[$subject]))return pay_fil_001_runtime_fail($error,'tt80_person_binding_invalid');$key=$subject.'|'.$link_id.'|'.$role;if(isset($binding_inputs[$key]))return pay_fil_001_runtime_fail($error,'tt80_person_binding_duplicate');$binding_inputs[$key]=array('subject_key'=>$subject,'filing_source_link_id'=>$link_id,'source_link_sha256'=>$binding['source_link_sha256'],'binding_role'=>$role);}
    $bound=array();foreach($binding_inputs as $b)$bound[$b['subject_key']]=true;foreach($facts as $subject=>$fact)if(!isset($bound[$subject]))return pay_fil_001_runtime_fail($error,'tt80_person_fact_binding_required');
    if(!pay_fil_001_begin_owned($error))return false;$at=pay_fil_001_now();
    $prow=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_taxpayer_profiles WHERE profile_sha256='.db_escape($profile['profile_sha256']).' LIMIT 2');
    if($prow){if(!pay_fil_001_tt80_semantic_source_profile_row_matches($prow,$profile)){rollback_transaction();return pay_fil_001_runtime_fail($error,'tt80_taxpayer_profile_replay_mismatch');}$profile_id=(int)$prow['tt80_taxpayer_profile_id'];}else{
        $cols=pay_fil_001_tt80_semantic_custody_taxpayer_fields();$names=array('filing_obligation_id');$values=array((string)(int)$s['filing_obligation_id']);foreach($cols as $field){$names[]=$field;$values[]=db_escape($profile[$field]);}$names=array_merge($names,array('source_evidence_sha256','profile_sha256','captured_by','captured_at','created_at'));$values=array_merge($values,array(db_escape($profile['source_evidence_sha256']),db_escape($profile['profile_sha256']),(string)$actor,db_escape($at),db_escape($at)));$sql='INSERT INTO '.TB_PREF.'hrm_pay_fil_001_tt80_taxpayer_profiles ('.implode(',',$names).') VALUES ('.implode(',',$values).')';if(!db_query($sql)){rollback_transaction();return pay_fil_001_runtime_fail($error,'tt80_taxpayer_profile_insert_failed');}$profile_id=(int)db_insert_id();}
    $fact_rows=array();foreach($facts as $subject=>$fact){$row=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_person_facts WHERE fact_sha256='.db_escape($fact['fact_sha256']).' LIMIT 2');if($row){if(!pay_fil_001_tt80_semantic_source_fact_row_matches($row,$fact)){rollback_transaction();return pay_fil_001_runtime_fail($error,'tt80_person_fact_replay_mismatch');}$fact_id=(int)$row['tt80_person_fact_id'];}else{$names=array('filing_source_set_id');$values=array((string)$set_id);foreach(pay_fil_001_tt80_semantic_custody_person_fields() as $field){$names[]=$field;$values[]=in_array($field,array('paid_worker','withheld_person','filing_taxable_income_minor','withholding_subject_taxable_income_minor','personal_income_tax_withheld_minor','foreign_insurer_nonmandatory_insurance_taxable_minor','foreign_insurer_nonmandatory_insurance_withheld_minor','petroleum_contract_exempt_taxable_minor','resolution_exempt_income_minor'),true)?(string)(int)$fact[$field]:db_escape($fact[$field]);}$names=array_merge($names,array('source_evidence_sha256','fact_sha256','captured_by','captured_at','created_at'));$values=array_merge($values,array(db_escape($fact['source_evidence_sha256']),db_escape($fact['fact_sha256']),(string)$actor,db_escape($at),db_escape($at)));if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_fil_001_tt80_person_facts ('.implode(',',$names).') VALUES ('.implode(',',$values).')')){rollback_transaction();return pay_fil_001_runtime_fail($error,'tt80_person_fact_insert_failed');}$fact_id=(int)db_insert_id();}$fact_rows[$subject]=array('id'=>$fact_id,'sha'=>$fact['fact_sha256']);}
    $binding_hashes=array();foreach($binding_inputs as $b){$link=pay_fil_001_one('SELECT filing_source_link_id,filing_source_set_id,source_link_sha256 FROM '.TB_PREF.'hrm_pay_fil_001_source_links WHERE filing_source_link_id='.(int)$b['filing_source_link_id'].' LIMIT 2');if(!$link||(int)$link['filing_source_set_id']!==$set_id||!hash_equals((string)$link['source_link_sha256'],(string)$b['source_link_sha256'])){rollback_transaction();return pay_fil_001_runtime_fail($error,'tt80_person_binding_source_link_mismatch');}$fact=$fact_rows[$b['subject_key']];$basis=array('fact_sha256'=>$fact['sha'],'filing_source_link_id'=>(int)$b['filing_source_link_id'],'source_link_sha256'=>$b['source_link_sha256'],'binding_role'=>$b['binding_role']);$sha=pay_fil_001_hash($basis);$row=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_person_fact_links WHERE binding_sha256='.db_escape($sha).' LIMIT 2');if($row){if((int)$row['tt80_person_fact_id']!==(int)$fact['id']||(int)$row['filing_source_link_id']!==(int)$b['filing_source_link_id']||(string)$row['source_link_sha256']!==(string)$b['source_link_sha256']||(string)$row['binding_role']!==(string)$b['binding_role']){rollback_transaction();return pay_fil_001_runtime_fail($error,'tt80_person_binding_replay_mismatch');}}else{if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_fil_001_tt80_person_fact_links (tt80_person_fact_id,filing_source_link_id,source_link_sha256,binding_role,binding_sha256,created_at) VALUES ('.(int)$fact['id'].','.(int)$b['filing_source_link_id'].','.db_escape($b['source_link_sha256']).','.db_escape($b['binding_role']).','.db_escape($sha).','.db_escape($at).')')){rollback_transaction();return pay_fil_001_runtime_fail($error,'tt80_person_binding_insert_failed');}}$binding_hashes[]=$sha;}
    commit_transaction();$fact_hashes=array();foreach($fact_rows as $row)$fact_hashes[]=$row['sha'];sort($fact_hashes,SORT_STRING);sort($binding_hashes,SORT_STRING);return array('filing_obligation_id'=>(int)$s['filing_obligation_id'],'filing_source_set_id'=>$set_id,'tt80_taxpayer_profile_id'=>$profile_id,'taxpayer_profile_sha256'=>$profile['profile_sha256'],'person_fact_sha256s'=>$fact_hashes,'binding_sha256s'=>$binding_hashes,'semantic_mapping_published'=>false,'production_generation_allowed'=>false);
}

function pay_fil_001_tt80_semantic_source_profile_hash_from_row(array $row)
{
    $basis=array('filing_obligation_id'=>(int)$row['filing_obligation_id']);foreach(pay_fil_001_tt80_semantic_custody_taxpayer_fields() as $field)$basis[$field]=(string)$row[$field];$basis['source_evidence_sha256']=(string)$row['source_evidence_sha256'];return pay_fil_001_hash($basis);
}
function pay_fil_001_tt80_semantic_source_fact_hash_from_row(array $row)
{
    $basis=array('filing_source_set_id'=>(int)$row['filing_source_set_id']);foreach(pay_fil_001_tt80_semantic_custody_person_fields() as $field)$basis[$field]=in_array($field,array('paid_worker','withheld_person','filing_taxable_income_minor','withholding_subject_taxable_income_minor','personal_income_tax_withheld_minor','foreign_insurer_nonmandatory_insurance_taxable_minor','foreign_insurer_nonmandatory_insurance_withheld_minor','petroleum_contract_exempt_taxable_minor','resolution_exempt_income_minor'),true)?(int)$row[$field]:(string)$row[$field];$basis['source_evidence_sha256']=(string)$row['source_evidence_sha256'];return pay_fil_001_hash($basis);
}

/** Independently review an exact admitted bundle and append a deterministic completeness proof. */
function pay_fil_001_tt80_review_semantic_source_bundle($filing_source_set_id,$taxpayer_profile_sha256,array $person_fact_sha256s,$expected_subject_count,$expected_subject_set_sha256,$subject_roster_evidence_sha256,&$error=null)
{
    $error=null;if(!pay_fil_001_require_access(PAY_FIL_001_REVIEW_PERMISSION,$error))return false;$reviewer=pay_fil_001_actor();$company=pay_fil_001_company();if($reviewer<=0||$company<0)return pay_fil_001_runtime_fail($error,'actor_or_company_missing');$set_id=pay_fil_001_posint($filing_source_set_id);$expected=pay_fil_001_uint($expected_subject_count);if($set_id===false||$expected===false||!pay_fil_001_sha($taxpayer_profile_sha256)||!pay_fil_001_sha($expected_subject_set_sha256)||!pay_fil_001_sha($subject_roster_evidence_sha256))return pay_fil_001_runtime_fail($error,'tt80_semantic_review_input_invalid');
    $fact_shas=array();foreach($person_fact_sha256s as $sha){if(!pay_fil_001_sha($sha))return pay_fil_001_runtime_fail($error,'tt80_semantic_review_fact_hash_invalid');$fact_shas[]=$sha;}sort($fact_shas,SORT_STRING);if(count($fact_shas)!==count(array_unique($fact_shas)))return pay_fil_001_runtime_fail($error,'tt80_semantic_review_fact_hash_duplicate');
    $s=pay_fil_001_one('SELECT s.filing_source_set_id,s.filing_obligation_id,o.company_id,o.obligation_sha256 FROM '.TB_PREF.'hrm_pay_fil_001_source_sets s INNER JOIN '.TB_PREF.'hrm_pay_fil_001_obligations o ON o.filing_obligation_id=s.filing_obligation_id WHERE s.filing_source_set_id='.(int)$set_id.' LIMIT 2');if(!$s||(int)$s['company_id']!==$company)return pay_fil_001_runtime_fail($error,'filing_source_set_missing_or_scope_denied');
    $profile=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_taxpayer_profiles WHERE profile_sha256='.db_escape($taxpayer_profile_sha256).' LIMIT 2');if(!$profile||(int)$profile['filing_obligation_id']!==(int)$s['filing_obligation_id']||!hash_equals((string)$profile['profile_sha256'],(string)pay_fil_001_tt80_semantic_source_profile_hash_from_row($profile)))return pay_fil_001_runtime_fail($error,'tt80_taxpayer_profile_missing_or_tampered');
    $makers=array((int)$profile['captured_by']);$subjects=array();$source_evidence=array((string)$profile['source_evidence_sha256']);$link_hashes=array();$facts=array();$findings=array();
    foreach($fact_shas as $sha){$row=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_person_facts WHERE fact_sha256='.db_escape($sha).' LIMIT 2');if(!$row||(int)$row['filing_source_set_id']!==$set_id||!hash_equals((string)$row['fact_sha256'],(string)pay_fil_001_tt80_semantic_source_fact_hash_from_row($row)))return pay_fil_001_runtime_fail($error,'tt80_person_fact_missing_or_tampered');$subject=(string)$row['subject_key'];if(isset($subjects[$subject]))return pay_fil_001_runtime_fail($error,'tt80_semantic_review_subject_duplicate');$subjects[$subject]=true;$makers[]=(int)$row['captured_by'];$source_evidence[]=(string)$row['source_evidence_sha256'];$facts[]=$row;if((string)$row['tax_residency']==='unknown')$findings['tax_residency_unknown_present']=true;if((string)$row['employment_contract_term_class']==='unknown')$findings['employment_contract_term_unknown_present']=true;
        $links=pay_fil_001_rows('SELECT pl.filing_source_link_id,pl.source_link_sha256,pl.binding_role,pl.binding_sha256,sl.filing_source_set_id AS source_set_scope,sl.source_link_sha256 AS authoritative_source_link_sha256 FROM '.TB_PREF.'hrm_pay_fil_001_tt80_person_fact_links pl INNER JOIN '.TB_PREF.'hrm_pay_fil_001_source_links sl ON sl.filing_source_link_id=pl.filing_source_link_id WHERE pl.tt80_person_fact_id='.(int)$row['tt80_person_fact_id'].' ORDER BY pl.tt80_person_fact_link_id');if($links===false)return pay_fil_001_runtime_fail($error,'tt80_person_binding_read_failed');if(count($links)===0)$findings['person_fact_binding_missing']=true;foreach($links as $link){if((int)$link['source_set_scope']!==$set_id||!hash_equals((string)$link['source_link_sha256'],(string)$link['authoritative_source_link_sha256']))return pay_fil_001_runtime_fail($error,'tt80_person_binding_scope_or_hash_mismatch');$basis=array('fact_sha256'=>$sha,'filing_source_link_id'=>(int)$link['filing_source_link_id'],'source_link_sha256'=>(string)$link['source_link_sha256'],'binding_role'=>(string)$link['binding_role']);$want=pay_fil_001_hash($basis);if(!hash_equals((string)$link['binding_sha256'],(string)$want))return pay_fil_001_runtime_fail($error,'tt80_person_binding_tampered');$link_hashes[]=$want;}}
    foreach($makers as $maker)if($maker===$reviewer)return pay_fil_001_runtime_fail($error,'tt80_semantic_review_maker_checker_conflict');sort($makers,SORT_NUMERIC);$makers=array_values(array_unique($makers));$subject_keys=array_keys($subjects);sort($subject_keys,SORT_STRING);$actual_subject_set_sha=pay_fil_001_hash($subject_keys);if(count($subject_keys)!==(int)$expected)$findings['expected_subject_count_mismatch']=true;if(!hash_equals((string)$expected_subject_set_sha256,(string)$actual_subject_set_sha))$findings['expected_subject_set_mismatch']=true;
    sort($link_hashes,SORT_STRING);$source_evidence=array_values(array_unique($source_evidence));sort($source_evidence,SORT_STRING);$fact_set_sha=pay_fil_001_hash($fact_shas);$link_set_sha=pay_fil_001_hash($link_hashes);$source_evidence_set_sha=pay_fil_001_hash($source_evidence);$maker_set_sha=pay_fil_001_hash($makers);$finding_codes=array_keys($findings);sort($finding_codes,SORT_STRING);$findings_json=json_encode($finding_codes,JSON_UNESCAPED_SLASHES);if(!is_string($findings_json)||strlen($findings_json)>1024)return pay_fil_001_runtime_fail($error,'tt80_semantic_review_findings_invalid');$status=count($finding_codes)===0?'complete':'incomplete';$findings_sha=pay_fil_001_hash($finding_codes);
    $basis=array('filing_obligation_id'=>(int)$s['filing_obligation_id'],'filing_source_set_id'=>$set_id,'taxpayer_profile_sha256'=>(string)$profile['profile_sha256'],'expected_subject_count'=>(int)$expected,'expected_subject_set_sha256'=>(string)$expected_subject_set_sha256,'subject_roster_evidence_sha256'=>(string)$subject_roster_evidence_sha256,'person_fact_count'=>count($fact_shas),'person_fact_set_sha256'=>$fact_set_sha,'person_fact_link_count'=>count($link_hashes),'person_fact_link_set_sha256'=>$link_set_sha,'source_evidence_set_sha256'=>$source_evidence_set_sha,'maker_set_sha256'=>$maker_set_sha,'completeness_status'=>$status,'findings_sha256'=>$findings_sha,'reviewed_by'=>$reviewer);$review_sha=pay_fil_001_hash($basis);$existing=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_reviews WHERE review_sha256='.db_escape($review_sha).' LIMIT 2');if($existing)return array('tt80_semantic_review_id'=>(int)$existing['tt80_semantic_review_id'],'review_sha256'=>$review_sha,'completeness_status'=>(string)$existing['completeness_status'],'findings'=>$finding_codes,'replayed'=>true,'semantic_mapping_published'=>false);
    if(!pay_fil_001_begin_owned($error))return false;$at=pay_fil_001_now();$sql='INSERT INTO '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_reviews (filing_obligation_id,filing_source_set_id,tt80_taxpayer_profile_id,taxpayer_profile_sha256,expected_subject_count,expected_subject_set_sha256,subject_roster_evidence_sha256,person_fact_count,person_fact_set_sha256,person_fact_link_count,person_fact_link_set_sha256,source_evidence_set_sha256,maker_set_sha256,completeness_status,findings_json,findings_sha256,review_sha256,reviewed_by,reviewed_at,created_at) VALUES ('.(int)$s['filing_obligation_id'].','.$set_id.','.(int)$profile['tt80_taxpayer_profile_id'].','.db_escape($profile['profile_sha256']).','.(int)$expected.','.db_escape($expected_subject_set_sha256).','.db_escape($subject_roster_evidence_sha256).','.count($fact_shas).','.db_escape($fact_set_sha).','.count($link_hashes).','.db_escape($link_set_sha).','.db_escape($source_evidence_set_sha).','.db_escape($maker_set_sha).','.db_escape($status).','.db_escape($findings_json).','.db_escape($findings_sha).','.db_escape($review_sha).','.$reviewer.','.db_escape($at).','.db_escape($at).')';if(!db_query($sql)){rollback_transaction();$race=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_reviews WHERE review_sha256='.db_escape($review_sha).' LIMIT 2');if($race&&hash_equals((string)$race['review_sha256'],(string)$review_sha)&&(string)$race['completeness_status']===$status&&(int)$race['reviewed_by']===$reviewer)return array('tt80_semantic_review_id'=>(int)$race['tt80_semantic_review_id'],'review_sha256'=>$review_sha,'completeness_status'=>$status,'findings'=>$finding_codes,'replayed'=>true,'semantic_mapping_published'=>false,'production_generation_allowed'=>false);return pay_fil_001_runtime_fail($error,'tt80_semantic_review_insert_failed');}$id=(int)db_insert_id();commit_transaction();return array('tt80_semantic_review_id'=>$id,'review_sha256'=>$review_sha,'completeness_status'=>$status,'findings'=>$finding_codes,'replayed'=>false,'semantic_mapping_published'=>false,'production_generation_allowed'=>false);
}

/** Read the newest complete independent review for one source set. Read-only and non-authoritative for mapping publication. */
function pay_fil_001_tt80_semantic_source_review_status($filing_source_set_id,&$error=null)
{
    $error=null;$id=pay_fil_001_posint($filing_source_set_id);if($id===false)return pay_fil_001_runtime_fail($error,'filing_source_set_invalid');$q=db_query('SELECT tt80_semantic_review_id,review_sha256,completeness_status,expected_subject_count,person_fact_count,reviewed_by,reviewed_at FROM '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_reviews WHERE filing_source_set_id='.(int)$id.' AND completeness_status=\'complete\' ORDER BY tt80_semantic_review_id DESC LIMIT 1');if(!$q)return pay_fil_001_runtime_fail($error,'semantic_source_review_custody_unavailable');if(db_num_rows($q)===0){$error='semantic_source_admission_unreviewed';return false;}return db_fetch_assoc($q);
}
function pay_fil_001_tt80_semantic_source_admission_require_writer_source($patch)
{
    global $path_to_root;if(!pay_fil_001_tt80_semantic_source_admission_require_authority_source($patch))return false;
    $raw=@file_get_contents($path_to_root.'/hrm/includes/db/pay_fil_001_tt80_semantic_source_admission_db.inc');$start=is_string($raw)?strpos($raw,'function pay_fil_001_tt80_semantic_source_input_fail'):false;$cut=is_string($raw)?strpos($raw,'function pay_fil_001_tt80_semantic_source_admission_require_writer_source'):false;$writer=($start===false||$cut===false||$cut<=$start)?$raw:substr($raw,$start,$cut-$start);foreach(array('pay_fil_001_tt80_admit_semantic_source_bundle','PAY_FIL_001_PREPARE_PERMISSION','pay_fil_001_tt80_review_semantic_source_bundle','PAY_FIL_001_REVIEW_PERMISSION','tt80_semantic_review_maker_checker_conflict','subject_roster_evidence_sha256','semantic_mapping_published'=>false) as $key=>$value){$token=is_int($key)?$value:$key;if(!is_string($writer)||strpos($writer,(string)$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic admission/review writer source incomplete: '.$token.'.');}
    foreach(array('curl_','fsockopen(','stream_socket_client(','file_put_contents(','openssl_sign(') as $token)if(stripos($writer,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 TT80 semantic admission/review writer acquired forbidden external authority: '.$token.'.');return true;
}
?>
