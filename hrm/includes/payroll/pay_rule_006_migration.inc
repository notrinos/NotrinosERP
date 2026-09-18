<?php
/** PAY-RULE-006 legacy inventory/classification and immutable migration custody. */
include_once(dirname(__FILE__).'/pay_rule_006_contract.inc');
include_once(dirname(__FILE__).'/../payroll_formula_safe_profile.inc');

function hrm_pay_rule_006_actor_id(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function hrm_pay_rule_006_access($permission){return function_exists('user_check_access')&&user_check_access($permission);}

function hrm_pay_rule_006_audit($entity_type,$entity_key,$action,$reason,$restricted=array(),$company_id=null)
{
    global $path_to_root;if(!function_exists('append_domain_audit_event')){$root=isset($path_to_root)?$path_to_root:dirname(__FILE__).'/../../..';$audit=$root.'/includes/db/domain_audit_db.inc';if(!is_file($audit))return false;include_once($audit);}if(!function_exists('append_domain_audit_event'))return false;
    $company=$company_id===null?(function_exists('user_company')?(int)user_company():0):(int)$company_id;
    return append_domain_audit_event(array('company_id'=>$company,'legal_entity_id'=>0,'entity_type'=>$entity_type,'entity_key'=>$entity_key,'action'=>$action,'reason_code'=>$reason,'before'=>array(),'after'=>array('accepted'=>true),'restricted_diff'=>$restricted));
}

function hrm_pay_rule_006_synthetic_contexts(&$error=null)
{
    $path=dirname(__FILE__).'/../../../tests/fixtures/payroll/formula_migration/corpus.json';
    $corpus=hrm_legacy_formula_load_json_artifact($path,$error);if($corpus===false)return false;$v=hrm_legacy_formula_validate_corpus($corpus,$error);if($v===false)return false;return array_values($v['migration_contexts']);
}
function hrm_pay_rule_006_target_eval($formula,$variables)
{
    $receipt=null;$error=null;$values=$variables;$values['PERIOD_START']='2024-01-01';$values['PERIOD_END']='2024-01-31';$value=hrm_payroll_formula_safe_evaluate($formula,$values,$receipt,$error);return $value===false?false:(float)$value->decimalString();
}

/** Inventory distinct source formulas and classify without persisting formula text. */
function hrm_pay_rule_006_inventory_candidates(&$error=null)
{
    $error=null;if(!hrm_pay_rule_006_access('SA_HRM_MANAGE_PAY_RULE_MIGRATION')){$error='access_denied';return false;}
    $contexts=hrm_pay_rule_006_synthetic_contexts($error);if($contexts===false)return false;$out=array();
    foreach(hrm_legacy_formula_source_catalog() as $source=>$def){$q=db_query("SELECT TRIM(".$def['column'].") formula_source, COUNT(*) row_count FROM ".TB_PREF.$def['table']." WHERE TRIM(COALESCE(".$def['column'].",''))<>'' GROUP BY TRIM(".$def['column'].") ORDER BY SHA2(TRIM(".$def['column']."),256)",'could not inventory PAY-RULE-006 legacy formulas');if(!$q){$error='inventory_query_failed';return false;}
        while($r=db_fetch_assoc($q)){$formula=(string)$r['formula_source'];$c=hrm_legacy_formula_differential_classify($formula,$contexts,'hrm_legacy_formula_evaluate_for_differential','hrm_pay_rule_006_target_eval');$identity=hash('sha256',$source.'|'.$c['formula_hash']);$e=hrm_pay_rule_006_candidate_envelope($source,$identity,$c['formula_hash'],$c['classification'],$c['reason'],$c['translated_hash'],$c['result_set_hash'],(int)$r['row_count']);if($e===false){$error='candidate_invalid';return false;}$out[]=$e;}
    }return $out;
}

function hrm_pay_rule_006_persist_candidates(array $candidates,&$error=null)
{
    $error=null;$actor=hrm_pay_rule_006_actor_id();if($actor<=0||!hrm_pay_rule_006_access('SA_HRM_MANAGE_PAY_RULE_MIGRATION')){$error='access_denied';return false;}$manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$ok=true;$ids=array();$new_ids=array();
    foreach($candidates as $e){if(!is_array($e)||empty($e['candidate_sha256'])){$ok=false;$error='candidate_invalid';break;}$q=db_query('SELECT candidate_id,candidate_sha256 FROM '.TB_PREF.'hrm_pay_rule_migration_candidates WHERE candidate_sha256='.db_escape($e['candidate_sha256']).' LIMIT 2 FOR UPDATE');if(!$q){$ok=false;$error='candidate_lookup_failed';break;}if(db_num_rows($q)===1){$x=db_fetch_assoc($q);$ids[]=(int)$x['candidate_id'];continue;}if(db_num_rows($q)>1){$ok=false;$error='candidate_ambiguous';break;}
        $sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_migration_candidates (source_kind,source_identity,source_payload_sha256,classification_token,classification_reason,translated_sha256,result_set_sha256,row_count,candidate_sha256,created_by) VALUES ('.db_escape($e['source_kind']).','.db_escape($e['source_identity']).','.db_escape($e['source_payload_sha256']).','.db_escape($e['classification_token']).','.db_escape($e['classification_reason']).','.($e['translated_sha256']===null?'NULL':db_escape($e['translated_sha256'])).','.($e['result_set_sha256']===null?'NULL':db_escape($e['result_set_sha256'])).','.(int)$e['row_count'].','.db_escape($e['candidate_sha256']).','.$actor.')';if(!db_query($sql,'could not append PAY-RULE-006 migration candidate')){$ok=false;$error='candidate_insert_failed';break;}$new_id=(int)db_insert_id();$ids[]=$new_id;$new_ids[]=$new_id;}
    if($ok&&!empty($new_ids)&&hrm_pay_rule_006_audit('pay_rule_006_migration_candidate_set',array('candidate_ids'=>$new_ids),'pay.rule_006.migration_candidates_recorded','pay_rule_006_inventory_classification',array('candidate_count'=>count($new_ids),'formula_text_logged'=>false,'production_activation_allowed'=>false))===false){$ok=false;$error='audit_failed';}
    if($manage){if($ok)commit_transaction();else cancel_transaction();}return $ok?$ids:false;
}

/** Unsupported/manual/prohibited candidates require explicit exception review and can never become executable bindings. */
function hrm_pay_rule_006_register_exception($candidate_id,$disposition,$reason_token,$evidence_sha256,&$error=null)
{
    $error=null;$actor=hrm_pay_rule_006_actor_id();if($actor<=0||!hrm_pay_rule_006_access('SA_HRM_REVIEW_PAY_RULE_MIGRATION')){$error='access_denied';return false;}$disposition=hrm_pay_rule_006_token($disposition,40);$reason_token=hrm_pay_rule_006_token($reason_token,120);$evidence_sha256=hrm_pay_rule_006_sha($evidence_sha256);if($disposition===false||$reason_token===false||$evidence_sha256===false)return false;
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$ok=true;$id=false;$new=false;
    $q=db_query('SELECT candidate_id,classification_token,candidate_sha256,created_by FROM '.TB_PREF.'hrm_pay_rule_migration_candidates WHERE candidate_id='.(int)$candidate_id.' LIMIT 2 FOR UPDATE');$c=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$c||!in_array($c['classification_token'],array('manual_review','unsupported','prohibited'),true)||(int)$c['created_by']===$actor)$ok=false;
    $hash=$ok?hrm_pay_rule_006_hash(array('candidate_id'=>(int)$candidate_id,'candidate_sha256'=>$c['candidate_sha256'],'disposition'=>$disposition,'reason_token'=>$reason_token,'evidence_sha256'=>$evidence_sha256,'reviewer_id'=>$actor)):false;
    if($ok){$q=db_query('SELECT exception_id,exception_sha256 FROM '.TB_PREF.'hrm_pay_rule_migration_exceptions WHERE candidate_id='.(int)$candidate_id.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1)$ok=false;elseif(db_num_rows($q)===1){$x=db_fetch_assoc($q);if(hash_equals($hash,(string)$x['exception_sha256']))$id=(int)$x['exception_id'];else$ok=false;}else{if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_migration_exceptions (candidate_id,disposition,reason_token,evidence_sha256,reviewer_id,exception_sha256) VALUES ('.(int)$candidate_id.','.db_escape($disposition).','.db_escape($reason_token).','.db_escape($evidence_sha256).','.$actor.','.db_escape($hash).')','could not append PAY-RULE-006 exception'))$ok=false;else{$id=(int)db_insert_id();$new=true;}}}
    if($ok&&$new&&hrm_pay_rule_006_audit('pay_rule_006_migration_exception',array('candidate_id'=>(int)$candidate_id,'exception_id'=>$id),'pay.rule_006.migration_exception_recorded',$reason_token,array('candidate_sha256'=>$c['candidate_sha256'],'evidence_sha256'=>$evidence_sha256,'disposition'=>$disposition,'formula_text_logged'=>false,'production_activation_allowed'=>false))===false){$ok=false;$error='audit_failed';}
    if($manage){if($ok)commit_transaction();else cancel_transaction();}return $ok?$id:false;
}

/** Bind only exact/translated candidates to a published PAY-RULE-004 Rule Version. */
function hrm_pay_rule_006_bind_candidate($candidate_id,$rule_version_id,&$error=null)
{
    $error=null;$actor=hrm_pay_rule_006_actor_id();if($actor<=0||!hrm_pay_rule_006_access('SA_HRM_REVIEW_PAY_RULE_MIGRATION')){$error='access_denied';return false;}$manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$ok=true;$id=false;$new=false;
    $q=db_query('SELECT candidate_id,source_payload_sha256,classification_token,translated_sha256,candidate_sha256,created_by FROM '.TB_PREF.'hrm_pay_rule_migration_candidates WHERE candidate_id='.(int)$candidate_id.' LIMIT 2 FOR UPDATE');$c=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$c||!hrm_pay_rule_006_cutover_eligible_classification($c['classification_token'])||(int)$c['created_by']===$actor)$ok=false;
    if($ok){$q=db_query('SELECT rv.rule_version_id,rv.artifact_version_id,rv.artifact_sha256,p.publication_id,p.publication_sha256 FROM '.TB_PREF.'hrm_pay_rule_rule_versions rv INNER JOIN '.TB_PREF.'hrm_pay_rule_lifecycle_publications p ON p.rule_version_id=rv.rule_version_id LEFT JOIN '.TB_PREF.'hrm_pay_rule_lifecycle_events e ON e.publication_id=p.publication_id WHERE rv.rule_version_id='.(int)$rule_version_id.' AND e.event_id IS NULL LIMIT 2 FOR UPDATE');$v=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$v){$error='published_rule_version_required';$ok=false;}}
    if($ok){$target=$c['classification_token']==='translated'?$c['translated_sha256']:$c['source_payload_sha256'];$hash=hrm_pay_rule_006_hash(array('candidate_sha256'=>$c['candidate_sha256'],'rule_version_id'=>(int)$rule_version_id,'artifact_version_id'=>(int)$v['artifact_version_id'],'artifact_sha256'=>$v['artifact_sha256'],'publication_sha256'=>$v['publication_sha256'],'target_sha256'=>$target,'reviewer_id'=>$actor));$q=db_query('SELECT binding_id,binding_sha256 FROM '.TB_PREF.'hrm_pay_rule_translation_bindings WHERE candidate_id='.(int)$candidate_id.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1)$ok=false;elseif(db_num_rows($q)===1){$x=db_fetch_assoc($q);if(hash_equals($hash,(string)$x['binding_sha256']))$id=(int)$x['binding_id'];else$ok=false;}else{if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_translation_bindings (candidate_id,rule_version_id,artifact_version_id,target_sha256,publication_sha256,binding_sha256,bound_by) VALUES ('.(int)$candidate_id.','.(int)$rule_version_id.','.(int)$v['artifact_version_id'].','.db_escape($target).','.db_escape($v['publication_sha256']).','.db_escape($hash).','.$actor.')','could not append PAY-RULE-006 translation binding'))$ok=false;else{$id=(int)db_insert_id();$new=true;}}}
    if($ok&&$new&&hrm_pay_rule_006_audit('pay_rule_006_translation_binding',array('candidate_id'=>(int)$candidate_id,'binding_id'=>$id,'rule_version_id'=>(int)$rule_version_id),'pay.rule_006.translation_binding_recorded','pay_rule_006_governed_binding',array('candidate_sha256'=>$c['candidate_sha256'],'binding_sha256'=>$hash,'publication_sha256'=>$v['publication_sha256'],'formula_text_logged'=>false,'production_activation_allowed'=>false))===false){$ok=false;$error='audit_failed';}
    if($manage){if($ok)commit_transaction();else cancel_transaction();}return $ok?$id:false;
}
