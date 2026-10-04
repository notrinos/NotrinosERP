<?php
/** PAY-ECO-001 governed marketplace linkage to existing PAY-CTRY-002 installation/activation custody. */
include_once($path_to_root.'/includes/pay_eco_001_contract.inc');
include_once($path_to_root.'/includes/pay_ctry_002_contract.inc');
include_once(dirname(__FILE__).'/pay_eco_001_catalog_publication_db.inc');
include_once(dirname(__FILE__).'/pay_ctry_002_runtime_db.inc');

function pay_eco_001_marketplace_exact_row($sql,&$error,$allow_none=false){return pay_eco_001_onboarding_exact_row($sql,$error,$allow_none);}
function pay_eco_001_marketplace_current_publication($catalog_publication_id,&$error,$now_epoch,$locked=true)
{
    $error=null;$suffix=$locked?' FOR UPDATE':'';
    $row=pay_eco_001_marketplace_exact_row(
        'SELECT cp.*,c.pack_version_id,d.decision,aa.result AS artifact_result FROM '.TB_PREF.'pay_eco_001_catalog_publications cp '
        .'INNER JOIN '.TB_PREF.'pay_eco_001_pack_candidates c ON c.catalog_candidate_id=cp.catalog_candidate_id '
        .'INNER JOIN '.TB_PREF.'pay_eco_001_catalog_decisions d ON d.catalog_decision_id=cp.catalog_decision_id '
        .'INNER JOIN '.TB_PREF.'pay_eco_001_artifact_admissions aa ON aa.artifact_admission_id=cp.artifact_admission_id '
        .'WHERE cp.catalog_publication_id='.(int)$catalog_publication_id.' LIMIT 2'.$suffix,$error,false);
    if($row===false)return false;
    if((string)$row['decision']!=='admit_for_review'||(string)$row['artifact_result']!=='admitted')return pay_eco_001_fail($error,'marketplace_publication_not_current');
    if(!pay_eco_001_catalog_lifecycle_guard((int)$row['catalog_publication_id'],$error,$now_epoch,$locked))return false;
    $cert=pay_eco_001_catalog_publication_certification((int)$row['certification_publication_id'],(int)$row['pack_version_id'],(string)$row['semantic_root'],(string)$row['evidence_root'],$now_epoch,$error,$locked);if($cert===false)return false;
    $catalog=pay_eco_001_catalog_publication_verify_release((int)$row['catalog_release_id'],(int)$row['catalog_admission_id'],$now_epoch,$error,$locked);if($catalog===false)return false;
    if(!hash_equals((string)$catalog['release']['catalog_root_sha256'],(string)$row['catalog_root_sha256']))return pay_eco_001_fail($error,'marketplace_catalog_root_drift');
    $nowSql=gmdate('Y-m-d H:i:s',(int)$now_epoch);
    $current=pay_eco_001_marketplace_exact_row('SELECT snapshot_sha256 FROM '.TB_PREF.'pay_eco_001_vulnerability_snapshots WHERE issued_at<='.pay_ctry_001_sql_hex($nowSql).' AND expires_at>='.pay_ctry_001_sql_hex($nowSql).' ORDER BY issued_at DESC,vulnerability_snapshot_id DESC LIMIT 1'.$suffix,$error,false);
    if($current===false||!hash_equals((string)$current['snapshot_sha256'],(string)$row['vulnerability_snapshot_sha256']))return pay_eco_001_fail($error,'marketplace_vulnerability_snapshot_not_current');
    return $row;
}
function pay_eco_001_marketplace_audit($entity_type,$entity_id,$action,array $after,$actor,&$error)
{
    if(!function_exists('append_domain_audit_event'))return pay_eco_001_fail($error,'marketplace_audit_unavailable');
    $audit=append_domain_audit_event(array('company_id'=>PAY_CTRY_001_SITE_COMPANY_ID,'legal_entity_id'=>0,'entity_type'=>$entity_type,'entity_key'=>array($entity_type.'_id'=>(int)$entity_id),'action'=>$action,'reason_code'=>'governed_existing_authority_link','before'=>array(),'after'=>$after,'restricted_diff'=>array('actor_id'=>(int)$actor,'append_only'=>true,'direct_installation_authority'=>false,'direct_company_activation_authority'=>false,'historical_payroll_accounting_rows_rewritten'=>0)));
    return $audit===false?pay_eco_001_fail($error,'marketplace_audit_failed'):true;
}
function pay_eco_001_record_marketplace_installation($request,&$error,$now_epoch=null)
{
    $error=null;if(!pay_eco_001_validate_marketplace_installation_request($request,$error))return false;
    $actor=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($actor<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_MARKETPLACE_INSTALL_PERMISSION))return pay_eco_001_fail($error,'marketplace_installation_authority_denied');
    if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $publication=pay_eco_001_marketplace_current_publication((int)$request['catalog_publication_id'],$error,$now_epoch,true);if($publication===false)break;
        $install=pay_eco_001_marketplace_exact_row('SELECT * FROM '.TB_PREF.'pay_ctry_site_installations WHERE site_installation_id='.(int)$request['site_installation_id'].' LIMIT 2 FOR UPDATE',$error,false);if($install===false)break;
        if((int)$install['pack_version_id']!==(int)$publication['pack_version_id']){$error='marketplace_installation_pack_mismatch';break;}
        if(!hash_equals((string)$install['semantic_root'],(string)$publication['semantic_root'])||!hash_equals((string)$install['evidence_root'],(string)$publication['evidence_root'])){$error='marketplace_installation_root_mismatch';break;}
        $existing=pay_eco_001_marketplace_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_marketplace_installations WHERE catalog_publication_id='.(int)$publication['catalog_publication_id'].' AND site_installation_id='.(int)$install['site_installation_id'].' LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break;
        if(is_array($existing)){
            if((int)$existing['pack_version_id']!==(int)$install['pack_version_id']||!hash_equals((string)$existing['publication_sha256'],(string)$publication['publication_sha256'])||!hash_equals((string)$existing['receipt_sha256'],(string)$install['receipt_sha256'])){$error='marketplace_installation_replay_conflict';break;}
            $ok=true;$out=array('marketplace_installation_id'=>(int)$existing['marketplace_installation_id'],'replayed'=>true,'direct_installation_authority'=>false,'company_activation'=>false);break;
        }
        $recordedAt=gmdate('Y-m-d\TH:i:s\Z',$now_epoch);
        $e=array('catalog_publication_id'=>(int)$publication['catalog_publication_id'],'site_installation_id'=>(int)$install['site_installation_id'],'pack_version_id'=>(int)$install['pack_version_id'],'publication_sha256'=>(string)$publication['publication_sha256'],'receipt_sha256'=>(string)$install['receipt_sha256'],'recorded_by'=>$actor,'recorded_at'=>$recordedAt,'marketplace_installation_sha256'=>'');
        $e['marketplace_installation_sha256']=pay_eco_001_marketplace_installation_sha256($e);if(!pay_eco_001_validate_marketplace_installation_evidence($e,$error))break;
        $nowSql=gmdate('Y-m-d H:i:s',$now_epoch);
        $sql='INSERT INTO '.TB_PREF.'pay_eco_001_marketplace_installations (catalog_publication_id,site_installation_id,pack_version_id,publication_sha256,receipt_sha256,recorded_by,recorded_at,marketplace_installation_sha256,created_at) VALUES ('.(int)$e['catalog_publication_id'].','.(int)$e['site_installation_id'].','.(int)$e['pack_version_id'].','.pay_ctry_001_sql_hex($e['publication_sha256']).','.pay_ctry_001_sql_hex($e['receipt_sha256']).','.$actor.','.pay_ctry_001_sql_hex($nowSql).','.pay_ctry_001_sql_hex($e['marketplace_installation_sha256']).','.pay_ctry_001_sql_hex($nowSql).')';
        if(!db_query($sql)){$error='marketplace_installation_insert_failed';break;}$id=pay_ctry_001_insert_id();if($id<=0){$error='marketplace_installation_identity_missing';break;}
        if(!pay_eco_001_marketplace_audit('pay_eco_marketplace_installation',$id,'pay.eco.marketplace_installation.linked',array('catalog_publication_id'=>(int)$e['catalog_publication_id'],'site_installation_id'=>(int)$e['site_installation_id'],'pack_version_id'=>(int)$e['pack_version_id'],'direct_installation_authority'=>false,'automatic_activation'=>false,'historical_payroll_accounting_rows_rewritten'=>0),$actor,$error))break;
        $ok=true;$out=array('marketplace_installation_id'=>$id,'replayed'=>false,'direct_installation_authority'=>false,'company_activation'=>false);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();return $ok?$out:false;
}
function pay_eco_001_record_marketplace_company_activation($request,&$error,$now_epoch=null)
{
    $error=null;if(!pay_eco_001_validate_marketplace_activation_request($request,$error))return false;
    $actor=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($actor<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_MARKETPLACE_ACTIVATION_PERMISSION))return pay_eco_001_fail($error,'marketplace_activation_authority_denied');
    if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $mi=pay_eco_001_marketplace_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_marketplace_installations WHERE marketplace_installation_id='.(int)$request['marketplace_installation_id'].' LIMIT 2 FOR UPDATE',$error,false);if($mi===false)break;
        $publication=pay_eco_001_marketplace_current_publication((int)$mi['catalog_publication_id'],$error,$now_epoch,true);if($publication===false)break;
        if((int)$publication['pack_version_id']!==(int)$mi['pack_version_id']){$error='marketplace_activation_pack_mismatch';break;}
        $row=pay_eco_001_marketplace_exact_row('SELECT a.*,b.binding_id,b.predecessor_binding_id,b.site_installation_id AS binding_site_installation_id,b.company_id AS binding_company_id,b.scope_type AS binding_scope_type,b.legal_entity_id AS binding_legal_entity_id,b.payroll_scope_key AS binding_payroll_scope_key,b.action_type AS binding_action_type,b.effective_from AS binding_effective_from,b.effective_to AS binding_effective_to,b.activation_sha256 AS binding_activation_sha256,b.binding_sha256 FROM '.TB_PREF.'pay_ctry_activations a INNER JOIN '.TB_PREF.'pay_ctry_activation_bindings b ON b.activation_id=a.activation_id WHERE a.activation_id='.(int)$request['activation_id'].' AND b.binding_id='.(int)$request['binding_id'].' LIMIT 2 FOR UPDATE',$error,false);if($row===false)break;
        if((int)$row['site_installation_id']!==(int)$mi['site_installation_id']||(int)$row['binding_site_installation_id']!==(int)$mi['site_installation_id']){$error='marketplace_activation_installation_mismatch';break;}
        if((int)$row['company_id']!==(int)$row['binding_company_id']||(string)$row['scope_type']!==(string)$row['binding_scope_type']||(string)$row['action_type']!==(string)$row['binding_action_type']||!hash_equals((string)$row['activation_sha256'],(string)$row['binding_activation_sha256'])){$error='marketplace_activation_binding_mismatch';break;}
        if(!in_array((string)$row['action_type'],array('activate','upgrade','rollback'),true)){$error='marketplace_activation_action_not_active';break;}
        $runtimeError=null;$runtime=pay_ctry_002_runtime_resolution_for_update((int)$row['company_id'],$row['legal_entity_id']===null?null:(int)$row['legal_entity_id'],$row['payroll_scope_key']===null?null:(string)$row['payroll_scope_key'],gmdate('Y-m-d',$now_epoch),$now_epoch,true,$runtimeError);
        if($runtime===false){$error='marketplace_activation_runtime_'.$runtimeError;break;}
        if(!isset($runtime['status'],$runtime['binding_id'],$runtime['activation_id'],$runtime['site_installation_id'],$runtime['pack_version_id'])||(string)$runtime['status']!=='governed'||(int)$runtime['binding_id']!==(int)$row['binding_id']||(int)$runtime['activation_id']!==(int)$row['activation_id']||(int)$runtime['site_installation_id']!==(int)$mi['site_installation_id']||(int)$runtime['pack_version_id']!==(int)$mi['pack_version_id']){$error='marketplace_activation_runtime_binding_not_current';break;}
        $existing=pay_eco_001_marketplace_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_marketplace_activations WHERE activation_id='.(int)$row['activation_id'].' OR binding_id='.(int)$row['binding_id'].' LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break;
        if(is_array($existing)){
            if((int)$existing['marketplace_installation_id']!==(int)$mi['marketplace_installation_id']||!hash_equals((string)$existing['activation_sha256'],(string)$row['activation_sha256'])||!hash_equals((string)$existing['binding_sha256'],(string)$row['binding_sha256'])){$error='marketplace_activation_replay_conflict';break;}
            $ok=true;$out=array('marketplace_activation_id'=>(int)$existing['marketplace_activation_id'],'replayed'=>true,'direct_company_activation_authority'=>false,'runtime_guarded'=>true);break;
        }
        $recordedAt=gmdate('Y-m-d\TH:i:s\Z',$now_epoch);
        $e=array('marketplace_installation_id'=>(int)$mi['marketplace_installation_id'],'activation_id'=>(int)$row['activation_id'],'binding_id'=>(int)$row['binding_id'],'company_id'=>(int)$row['company_id'],'scope_type'=>(string)$row['scope_type'],'activation_sha256'=>(string)$row['activation_sha256'],'binding_sha256'=>(string)$row['binding_sha256'],'recorded_by'=>$actor,'recorded_at'=>$recordedAt,'marketplace_activation_sha256'=>'');
        $e['marketplace_activation_sha256']=pay_eco_001_marketplace_activation_sha256($e);if(!pay_eco_001_validate_marketplace_activation_evidence($e,$error))break;
        $nowSql=gmdate('Y-m-d H:i:s',$now_epoch);
        $legal=$row['legal_entity_id']===null?'NULL':(int)$row['legal_entity_id'];$scope=$row['payroll_scope_key']===null?'NULL':pay_ctry_001_sql_hex((string)$row['payroll_scope_key']);$to=$row['effective_to']===null?'NULL':pay_ctry_001_sql_hex((string)$row['effective_to']);
        $sql='INSERT INTO '.TB_PREF.'pay_eco_001_marketplace_activations (marketplace_installation_id,activation_id,binding_id,company_id,scope_type,legal_entity_id,payroll_scope_key,action_type,effective_from,effective_to,activation_sha256,binding_sha256,recorded_by,recorded_at,marketplace_activation_sha256,created_at) VALUES ('.(int)$e['marketplace_installation_id'].','.(int)$e['activation_id'].','.(int)$e['binding_id'].','.(int)$e['company_id'].','.pay_ctry_001_sql_hex($e['scope_type']).','.$legal.','.$scope.','.pay_ctry_001_sql_hex((string)$row['action_type']).','.pay_ctry_001_sql_hex((string)$row['effective_from']).','.$to.','.pay_ctry_001_sql_hex($e['activation_sha256']).','.pay_ctry_001_sql_hex($e['binding_sha256']).','.$actor.','.pay_ctry_001_sql_hex($nowSql).','.pay_ctry_001_sql_hex($e['marketplace_activation_sha256']).','.pay_ctry_001_sql_hex($nowSql).')';
        if(!db_query($sql)){$error='marketplace_activation_insert_failed';break;}$id=pay_ctry_001_insert_id();if($id<=0){$error='marketplace_activation_identity_missing';break;}
        if(!pay_eco_001_marketplace_audit('pay_eco_marketplace_activation',$id,'pay.eco.marketplace_activation.linked',array('marketplace_installation_id'=>(int)$mi['marketplace_installation_id'],'activation_id'=>(int)$row['activation_id'],'binding_id'=>(int)$row['binding_id'],'company_id'=>(int)$row['company_id'],'scope_type'=>(string)$row['scope_type'],'runtime_guarded'=>true,'direct_company_activation_authority'=>false,'payroll_authority'=>false,'historical_payroll_accounting_rows_rewritten'=>0),$actor,$error))break;
        $ok=true;$out=array('marketplace_activation_id'=>$id,'replayed'=>false,'direct_company_activation_authority'=>false,'runtime_guarded'=>true);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();return $ok?$out:false;
}
?>
