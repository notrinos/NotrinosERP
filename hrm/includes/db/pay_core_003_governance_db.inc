<?php
/**
 * PAY-CORE-003 governed append-only writers and controlled legacy conversion.
 * This file is deliberately not included by payroll_engine.inc.
 */
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_governance.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_eligibility.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_legacy_mapping_review.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_legacy_compatibility.inc');

function hrm_pay_core_003_actor(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function hrm_pay_core_003_access($permission){return function_exists('user_check_access')&&user_check_access($permission);}
function hrm_pay_core_003_insert_idempotent($table,$hash_col,$hash,$sql,$id_col,&$error){$q=db_query('SELECT '.$id_col.','.$hash_col.' FROM '.TB_PREF.$table.' WHERE '.$hash_col.'='.db_escape($hash).' LIMIT 2 FOR UPDATE');if(!$q){$error='lookup_failed';return false;}if(db_num_rows($q)===1){$x=db_fetch_assoc($q);return (int)$x[$id_col];}if(db_num_rows($q)>1){$error='ambiguous_replay';return false;}if(!db_query($sql,'could not append PAY-CORE-003 custody')){$error='insert_failed';return false;}return (int)db_insert_id();}

function hrm_pay_core_003_legacy_db_spec($kind)
{
    $fields=hrm_pay_core_003_legacy_source_fields($kind);
    $id=hrm_pay_core_003_legacy_source_id_field($kind);
    $tables=array('pay_element'=>'pay_elements','salary_structure'=>'salary_structure','employee_salary'=>'employee_salary');
    return $fields!==false&&$id!==false&&isset($tables[$kind])?array('table'=>$tables[$kind],'id'=>$id,'fields'=>$fields):false;
}

function hrm_pay_core_003_read_legacy_source($kind,$row_id,$for_update=false)
{
    $spec=hrm_pay_core_003_legacy_db_spec($kind);$row_id=(int)$row_id;
    if($spec===false||$row_id<=0)return false;
    $q=db_query('SELECT '.implode(',',$spec['fields']).' FROM '.TB_PREF.$spec['table'].' WHERE '.$spec['id'].'='.$row_id.' LIMIT 2'.($for_update?' FOR UPDATE':''));
    return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
}

function hrm_pay_core_003_propose_mapping($source_kind,$source_row_id,array $resolution,$supersedes_mapping_plan_id=null,&$error=null)
{
    $error=null;$maker=hrm_pay_core_003_actor();
    if($maker<=0||!hrm_pay_core_003_access('SA_HRM_MANAGE_PAY_CORE')){$error='access_denied';return false;}
    $legacy=hrm_pay_core_003_read_legacy_source($source_kind,$source_row_id,false);$source_sha=$legacy?hrm_pay_core_003_legacy_source_sha256($source_kind,$legacy):false;
    if($source_sha===false){$error='legacy_source_invalid';return false;}
    if($source_kind==='employee_salary')$resolution['source_sha256']=$source_sha;
    if(!hrm_pay_core_003_validate_mapping_resolution($source_kind,$resolution)){$error='resolution_invalid';return false;}
    $supersedes=$supersedes_mapping_plan_id===null?null:(int)$supersedes_mapping_plan_id;
    if($supersedes!==null){$q=db_query('SELECT source_kind,source_row_id,source_sha256 FROM '.TB_PREF.'hrm_pay_core_003_legacy_mapping_plans WHERE mapping_plan_id='.$supersedes.' LIMIT 2');$p=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$p||$p['source_kind']!==$source_kind||(int)$p['source_row_id']!==(int)$source_row_id||!hash_equals((string)$p['source_sha256'],$source_sha)){$error='superseded_plan_source_mismatch';return false;}$q=db_query('SELECT mapping_plan_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_mapping_plans WHERE supersedes_mapping_plan_id='.$supersedes.' LIMIT 2');if(!$q||db_num_rows($q)>0){$error='superseded_plan_already_replaced';return false;}}
    $payload_json=json_encode(hrm_pay_core_003_canonicalize($resolution),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_PRESERVE_ZERO_FRACTION);if(!is_string($payload_json)){$error='resolution_encoding_failed';return false;}$payload_sha=hash('sha256',$payload_json);$plan_sha=hrm_pay_core_003_mapping_plan_hash($source_kind,$source_row_id,$source_sha,$resolution,$supersedes,$maker);if($plan_sha===false){$error='mapping_plan_hash_failed';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_003_legacy_mapping_plans (source_kind,source_row_id,source_sha256,target_kind,target_payload_json,target_payload_sha256,supersedes_mapping_plan_id,mapping_plan_sha256,maker_id) VALUES ('.db_escape($source_kind).','.(int)$source_row_id.','.db_escape($source_sha).','.db_escape($resolution['target_kind']).','.db_escape($payload_json).','.db_escape($payload_sha).','.($supersedes===null?'NULL':$supersedes).','.db_escape($plan_sha).','.$maker.')';
    return hrm_pay_core_003_insert_idempotent('hrm_pay_core_003_legacy_mapping_plans','mapping_plan_sha256',$plan_sha,$sql,'mapping_plan_id',$error);
}

function hrm_pay_core_003_approve_mapping($mapping_plan_id,$decision_token,$evidence_sha256,&$error=null)
{
    $error=null;$checker=hrm_pay_core_003_actor();$mapping_plan_id=(int)$mapping_plan_id;
    if($checker<=0||!hrm_pay_core_003_access('SA_HRM_REVIEW_PAY_CORE')||!in_array($decision_token,array('approved','rejected'),true)||!hrm_pay_core_003_is_sha256($evidence_sha256)){$error='access_or_evidence_invalid';return false;}
    $q=db_query('SELECT mapping_plan_sha256,maker_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_mapping_plans WHERE mapping_plan_id='.$mapping_plan_id.' LIMIT 2 FOR UPDATE');$p=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$p||(int)$p['maker_id']===$checker){$error='distinct_checker_required';return false;}
    $q=db_query('SELECT mapping_plan_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_mapping_plans WHERE supersedes_mapping_plan_id='.$mapping_plan_id.' LIMIT 1');if(!$q||db_num_rows($q)>0){$error='superseded_plan_not_approvable';return false;}
    $approval_sha=hrm_pay_core_003_hash(array('mapping_plan_id'=>$mapping_plan_id,'mapping_plan_sha256'=>$p['mapping_plan_sha256'],'decision_token'=>$decision_token,'evidence_sha256'=>$evidence_sha256,'checker_id'=>$checker));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_003_legacy_mapping_approvals (mapping_plan_id,mapping_plan_sha256,decision_token,evidence_sha256,approval_sha256,checker_id) VALUES ('.$mapping_plan_id.','.db_escape($p['mapping_plan_sha256']).','.db_escape($decision_token).','.db_escape($evidence_sha256).','.db_escape($approval_sha).','.$checker.')';
    return hrm_pay_core_003_insert_idempotent('hrm_pay_core_003_legacy_mapping_approvals','approval_sha256',$approval_sha,$sql,'mapping_approval_id',$error);
}

function hrm_pay_core_003_append_element_bundle_internal(array $x,$actor,&$error)
{
    if(!hrm_pay_core_003_validate_element_version_bundle($x)){$error='element_bundle_invalid';return false;}
    $code=$x['element_code'];$identity_sha=hrm_pay_core_003_hash(array('element_code'=>$code));
    $q=db_query('SELECT pay_element_id,identity_sha256 FROM '.TB_PREF.'hrm_pay_elements WHERE element_code='.db_escape($code).' LIMIT 2 FOR UPDATE');
    if(!$q){$error='element_lookup_failed';return false;}
    if(db_num_rows($q)>1){$error='element_identity_ambiguous';return false;}
    if(db_num_rows($q)===1){$e=db_fetch_assoc($q);if(!hash_equals((string)$e['identity_sha256'],$identity_sha)){$error='element_identity_hash_mismatch';return false;}$element_id=(int)$e['pay_element_id'];}
    else {if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_elements (element_code,identity_sha256,created_by) VALUES ('.db_escape($code).','.db_escape($identity_sha).','.(int)$actor.')')){$error='element_insert_failed';return false;}$element_id=(int)db_insert_id();}
    $version_payload=array('pay_element_id'=>$element_id,'version_no'=>(int)$x['version_no'],'effective_from'=>$x['effective_from'],'effective_to'=>empty($x['effective_to'])?null:$x['effective_to'],'element_type'=>$x['element_type'],'calculation_mode'=>$x['calculation_mode'],'rule_artifact_sha256'=>empty($x['rule_artifact_sha256'])?null:$x['rule_artifact_sha256']);$version_sha=hrm_pay_core_003_hash($version_payload);
    $q=db_query('SELECT element_version_id,version_sha256 FROM '.TB_PREF.'hrm_pay_element_versions WHERE pay_element_id='.$element_id.' AND version_no='.(int)$x['version_no'].' LIMIT 2 FOR UPDATE');if(!$q){$error='version_lookup_failed';return false;}
    if(db_num_rows($q)>1){$error='version_ambiguous';return false;}
    if(db_num_rows($q)===1){$v=db_fetch_assoc($q);if(!hash_equals((string)$v['version_sha256'],$version_sha)){$error='version_replay_mismatch';return false;}$version_id=(int)$v['element_version_id'];}
    else {$q=db_query('SELECT version_no,effective_from,effective_to FROM '.TB_PREF.'hrm_pay_element_versions WHERE pay_element_id='.$element_id.' ORDER BY version_no DESC LIMIT 1 FOR UPDATE');$prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if($prev){if((int)$x['version_no']!==(int)$prev['version_no']+1||$x['effective_from']<=$prev['effective_from']){$error='version_chronology_invalid';return false;}if(empty($prev['effective_to'])||$x['effective_from']<=$prev['effective_to']){$error='predecessor_version_must_be_bounded';return false;}}elseif((int)$x['version_no']!==1){$error='first_version_must_be_one';return false;}
        $sql='INSERT INTO '.TB_PREF.'hrm_pay_element_versions (pay_element_id,version_no,effective_from,effective_to,element_type,calculation_mode,rule_artifact_sha256,version_sha256,created_by) VALUES ('.$element_id.','.(int)$x['version_no'].','.db_escape($x['effective_from']).','.(!empty($x['effective_to'])?db_escape($x['effective_to']):'NULL').','.db_escape($x['element_type']).','.db_escape($x['calculation_mode']).','.(!empty($x['rule_artifact_sha256'])?db_escape($x['rule_artifact_sha256']):'NULL').','.db_escape($version_sha).','.(int)$actor.')';if(!db_query($sql)){$error='element_version_insert_failed';return false;}$version_id=(int)db_insert_id();}
    $definition_ids=array();foreach($x['input_definitions'] as $d){$dh=hrm_pay_core_003_hash(array('element_version_id'=>$version_id,'input_code'=>$d['input_code'],'value_type'=>$d['value_type'],'unit_token'=>$d['unit_token'],'required_flag'=>(int)$d['required_flag']));$q=db_query('SELECT input_definition_id,definition_sha256 FROM '.TB_PREF.'hrm_pay_input_definitions WHERE element_version_id='.$version_id.' AND input_code='.db_escape($d['input_code']).' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1){$error='input_definition_lookup_invalid';return false;}if(db_num_rows($q)===1){$r=db_fetch_assoc($q);if(!hash_equals((string)$r['definition_sha256'],$dh)){$error='input_definition_replay_mismatch';return false;}$definition_ids[]=(int)$r['input_definition_id'];continue;}$sql='INSERT INTO '.TB_PREF.'hrm_pay_input_definitions (element_version_id,input_code,value_type,unit_token,required_flag,definition_sha256,created_by) VALUES ('.$version_id.','.db_escape($d['input_code']).','.db_escape($d['value_type']).','.db_escape($d['unit_token']).','.(int)$d['required_flag'].','.db_escape($dh).','.(int)$actor.')';if(!db_query($sql)){$error='input_definition_insert_failed';return false;}$definition_ids[]=(int)db_insert_id();}
    return array('pay_element_id'=>$element_id,'element_version_id'=>$version_id,'input_definition_ids'=>$definition_ids,'element_identity_sha256'=>$identity_sha,'element_version_sha256'=>$version_sha);
}

function hrm_pay_core_003_write_element_version_bundle(array $x,&$error=null)
{
    $error=null;$actor=hrm_pay_core_003_actor();if($actor<=0||!hrm_pay_core_003_access('SA_HRM_MANAGE_PAY_CORE')){$error='access_denied';return false;}
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$r=hrm_pay_core_003_append_element_bundle_internal($x,$actor,$error);if($manage){if($r!==false)commit_transaction();else cancel_transaction();}return $r;
}

function hrm_pay_core_003_append_eligibility_internal(array $x,$actor,&$error)
{
    if(!hrm_pay_core_003_validate_eligibility_version($x)){$error='eligibility_version_invalid';return false;}
    $element_version_id=(int)$x['element_version_id'];$q=db_query('SELECT element_version_id,effective_from,effective_to FROM '.TB_PREF.'hrm_pay_element_versions WHERE element_version_id='.$element_version_id.' LIMIT 2');$ev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$ev){$error='element_version_missing';return false;}if($x['effective_from']<$ev['effective_from']||(!empty($ev['effective_to'])&&(empty($x['effective_to'])||$x['effective_to']>$ev['effective_to']))){$error='eligibility_outside_element_version';return false;}
    $payload=array('element_version_id'=>$element_version_id,'version_no'=>(int)$x['version_no'],'effective_from'=>$x['effective_from'],'effective_to'=>empty($x['effective_to'])?null:$x['effective_to'],'eligibility_kind'=>$x['eligibility_kind'],'rule_artifact_sha256'=>empty($x['rule_artifact_sha256'])?null:$x['rule_artifact_sha256']);$sha=hrm_pay_core_003_hash($payload);
    $q=db_query('SELECT eligibility_version_id,eligibility_sha256 FROM '.TB_PREF.'hrm_pay_eligibility_versions WHERE element_version_id='.$element_version_id.' AND version_no='.(int)$x['version_no'].' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1){$error='eligibility_lookup_invalid';return false;}if(db_num_rows($q)===1){$r=db_fetch_assoc($q);if(!hash_equals((string)$r['eligibility_sha256'],$sha)){$error='eligibility_replay_mismatch';return false;}return array('eligibility_version_id'=>(int)$r['eligibility_version_id'],'eligibility_sha256'=>$sha);}
    $q=db_query('SELECT version_no,effective_from,effective_to FROM '.TB_PREF.'hrm_pay_eligibility_versions WHERE element_version_id='.$element_version_id.' ORDER BY version_no DESC LIMIT 1 FOR UPDATE');$prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if($prev){if((int)$x['version_no']!==(int)$prev['version_no']+1||$x['effective_from']<=$prev['effective_from']||empty($prev['effective_to'])||$x['effective_from']<=$prev['effective_to']){$error='eligibility_chronology_invalid';return false;}}elseif((int)$x['version_no']!==1){$error='first_eligibility_version_must_be_one';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_eligibility_versions (element_version_id,version_no,effective_from,effective_to,eligibility_kind,rule_artifact_sha256,eligibility_sha256,created_by) VALUES ('.$element_version_id.','.(int)$x['version_no'].','.db_escape($x['effective_from']).','.(!empty($x['effective_to'])?db_escape($x['effective_to']):'NULL').','.db_escape($x['eligibility_kind']).','.(!empty($x['rule_artifact_sha256'])?db_escape($x['rule_artifact_sha256']):'NULL').','.db_escape($sha).','.(int)$actor.')';if(!db_query($sql)){$error='eligibility_insert_failed';return false;}return array('eligibility_version_id'=>(int)db_insert_id(),'eligibility_sha256'=>$sha);
}

function hrm_pay_core_003_write_eligibility_version(array $x,&$error=null)
{
    $error=null;$actor=hrm_pay_core_003_actor();if($actor<=0||!hrm_pay_core_003_access('SA_HRM_MANAGE_PAY_CORE')){$error='access_denied';return false;}
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$r=hrm_pay_core_003_append_eligibility_internal($x,$actor,$error);if($manage){if($r!==false)commit_transaction();else cancel_transaction();}return $r;
}

function hrm_pay_core_003_append_approved_input_internal(array $x,$actor,$override_authorized,&$error)
{
    if(!hrm_pay_core_003_validate_approved_input_candidate($x)){$error='approved_input_invalid';return false;}
    $date=$x['effective_date'];$q=db_query('SELECT effective_from,effective_to FROM '.TB_PREF.'hrm_payroll_relationships WHERE payroll_relationship_id='.(int)$x['payroll_relationship_id'].' LIMIT 2');$rel=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$rel||$date<$rel['effective_from']||(!empty($rel['effective_to'])&&$date>$rel['effective_to'])){$error='relationship_scope_invalid';return false;}
    if(!empty($x['assignment_id'])){$q=db_query('SELECT effective_from,effective_to FROM '.TB_PREF.'hrm_payroll_relationship_members WHERE payroll_relationship_id='.(int)$x['payroll_relationship_id'].' AND assignment_id='.(int)$x['assignment_id'].' LIMIT 2');$mem=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$mem||$date<$mem['effective_from']||(!empty($mem['effective_to'])&&$date>$mem['effective_to'])){$error='assignment_membership_invalid';return false;}}
    $q=db_query('SELECT d.value_type,v.effective_from,v.effective_to FROM '.TB_PREF.'hrm_pay_input_definitions d INNER JOIN '.TB_PREF.'hrm_pay_element_versions v ON v.element_version_id=d.element_version_id WHERE d.input_definition_id='.(int)$x['input_definition_id'].' AND d.element_version_id='.(int)$x['element_version_id'].' LIMIT 2');$bind=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$bind||$bind['value_type']!==$x['value_type']||$date<$bind['effective_from']||(!empty($bind['effective_to'])&&$date>$bind['effective_to'])){$error='input_definition_binding_invalid';return false;}
    $override=isset($x['override_reason'])&&$x['override_reason']!==null?trim((string)$x['override_reason']):null;if($override!==null&&!$override_authorized){$error='override_authority_required';return false;}
    $supersedes=isset($x['supersedes_input_id'])&&$x['supersedes_input_id']!==null?(int)$x['supersedes_input_id']:null;if($supersedes!==null){if($override===null||!$override_authorized){$error='supersession_requires_authorized_override_reason';return false;}$q=db_query('SELECT payroll_relationship_id,input_definition_id,effective_date FROM '.TB_PREF.'hrm_payroll_approved_inputs WHERE approved_input_id='.$supersedes.' LIMIT 2 FOR UPDATE');$old=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$old||(int)$old['payroll_relationship_id']!==(int)$x['payroll_relationship_id']||(int)$old['input_definition_id']!==(int)$x['input_definition_id']||$old['effective_date']!==$date){$error='superseded_input_scope_mismatch';return false;}$q=db_query('SELECT approved_input_id FROM '.TB_PREF.'hrm_payroll_approved_inputs WHERE supersedes_input_id='.$supersedes.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>0){$error='superseded_input_already_replaced';return false;}}
    $payload=array('payroll_relationship_id'=>(int)$x['payroll_relationship_id'],'assignment_id'=>empty($x['assignment_id'])?null:(int)$x['assignment_id'],'element_version_id'=>(int)$x['element_version_id'],'input_definition_id'=>(int)$x['input_definition_id'],'effective_date'=>$date,'value_type'=>$x['value_type'],'value'=>$x['value'],'source_kind'=>$x['source_kind'],'source_event_id'=>$x['source_event_id'],'source_sha256'=>$x['source_sha256'],'approval_evidence_sha256'=>$x['approval_evidence_sha256'],'approved_by'=>(int)$actor,'override_reason'=>$override,'supersedes_input_id'=>$supersedes);$sha=hrm_pay_core_003_hash($payload);
    $q=db_query('SELECT approved_input_id,input_sha256 FROM '.TB_PREF.'hrm_payroll_approved_inputs WHERE input_definition_id='.(int)$x['input_definition_id'].' AND source_kind='.db_escape($x['source_kind']).' AND source_event_id='.db_escape($x['source_event_id']).' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1){$error='source_event_state_invalid';return false;}if(db_num_rows($q)===1){$r=db_fetch_assoc($q);if(!hash_equals((string)$r['input_sha256'],$sha)){$error='source_event_replay_mismatch';return false;}return array('approved_input_id'=>(int)$r['approved_input_id'],'input_sha256'=>$sha);}
    $decimal=$datev=$code=$boolean='NULL';if($x['value_type']==='decimal')$decimal=db_escape((string)$x['value']);elseif($x['value_type']==='date')$datev=db_escape($x['value']);elseif($x['value_type']==='code')$code=db_escape($x['value']);else $boolean=(int)((string)$x['value']==='1'||$x['value']===true);
    $sql='INSERT INTO '.TB_PREF.'hrm_payroll_approved_inputs (payroll_relationship_id,assignment_id,element_version_id,input_definition_id,effective_date,value_type,value_decimal,value_date,value_code,value_boolean,source_kind,source_event_id,source_sha256,approval_evidence_sha256,approved_by,approved_at,override_reason,supersedes_input_id,input_sha256) VALUES ('.(int)$x['payroll_relationship_id'].','.(!empty($x['assignment_id'])?(int)$x['assignment_id']:'NULL').','.(int)$x['element_version_id'].','.(int)$x['input_definition_id'].','.db_escape($date).','.db_escape($x['value_type']).','.$decimal.','.$datev.','.$code.','.$boolean.','.db_escape($x['source_kind']).','.db_escape($x['source_event_id']).','.db_escape($x['source_sha256']).','.db_escape($x['approval_evidence_sha256']).','.(int)$actor.',NOW(),'.($override!==null?db_escape($override):'NULL').','.($supersedes!==null?$supersedes:'NULL').','.db_escape($sha).')';if(!db_query($sql)){$error='approved_input_insert_failed';return false;}return array('approved_input_id'=>(int)db_insert_id(),'input_sha256'=>$sha);
}

function hrm_pay_core_003_write_approved_input(array $x,&$error=null)
{
    $error=null;$actor=hrm_pay_core_003_actor();if($actor<=0||!hrm_pay_core_003_access('SA_HRM_EXECUTE_PAY_CORE_INPUT')){$error='access_denied';return false;}$override=empty($x['override_reason'])?true:hrm_pay_core_003_access('SA_HRM_OVERRIDE_PAY_CORE_INPUT');$manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$r=hrm_pay_core_003_append_approved_input_internal($x,$actor,$override,$error);if($manage){if($r!==false)commit_transaction();else cancel_transaction();}return $r;
}

function hrm_pay_core_003_execute_mapping($mapping_plan_id,&$error=null)
{
    global $transaction_level;
    $error = null;
    $executor = hrm_pay_core_003_actor();
    $mapping_plan_id = (int)$mapping_plan_id;
    if ($executor <= 0 || !hrm_pay_core_003_access('SA_HRM_EXECUTE_PAY_CORE')) {
        $error='access_denied';
        return false;
    }
    // The receipt and all successor rows are one atomic command. Do not let a
    // caller-owned transaction accidentally commit a target after receipt failure.
    if (!empty($transaction_level)) {
        $error='top_level_transaction_required';
        return false;
    }
    begin_transaction();
    $q=db_query('SELECT p.*,a.mapping_approval_id,a.approval_sha256,a.checker_id FROM '.TB_PREF
        .'hrm_pay_core_003_legacy_mapping_plans p INNER JOIN '.TB_PREF
        .'hrm_pay_core_003_legacy_mapping_approvals a ON a.mapping_plan_id=p.mapping_plan_id '
        .'AND a.mapping_plan_sha256=p.mapping_plan_sha256 WHERE p.mapping_plan_id='.$mapping_plan_id
        ." AND a.decision_token='approved' LIMIT 2 FOR UPDATE");
    $plan=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if (!$plan || $executor === (int)$plan['maker_id'] || $executor === (int)$plan['checker_id']
        || (int)$plan['maker_id'] === (int)$plan['checker_id']) {
        cancel_transaction(); $error='three_party_separation_required'; return false;
    }
    if ($plan['target_kind'] === 'documented_exception') {
        cancel_transaction(); $error='documented_exception_not_executable'; return false;
    }
    $q=db_query('SELECT mapping_plan_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_mapping_plans '
        .'WHERE supersedes_mapping_plan_id='.$mapping_plan_id.' LIMIT 1 FOR UPDATE');
    if (!$q || db_num_rows($q)>0) {
        cancel_transaction(); $error='superseded_mapping_not_executable'; return false;
    }
    $q=db_query('SELECT conversion_receipt_id,executor_id FROM '.TB_PREF
        .'hrm_pay_core_003_legacy_conversion_receipts WHERE mapping_plan_id='.$mapping_plan_id
        .' LIMIT 2 FOR UPDATE');
    if (!$q || db_num_rows($q)>1) {
        cancel_transaction(); $error='conversion_state_invalid'; return false;
    }
    if (db_num_rows($q)===1) {
        $receipt=db_fetch_assoc($q);
        if ((int)$receipt['executor_id'] !== $executor) {
            cancel_transaction(); $error='replay_executor_mismatch'; return false;
        }
        $receipt_id=(int)$receipt['conversion_receipt_id'];
        commit_transaction();
        return $receipt_id;
    }
    $q=db_query('SELECT conversion_receipt_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_conversion_receipts '
        .'WHERE source_kind='.db_escape($plan['source_kind']).' AND source_row_id='.(int)$plan['source_row_id']
        .' AND source_sha256='.db_escape($plan['source_sha256']).' LIMIT 2 FOR UPDATE');
    if (!$q || db_num_rows($q)>0) {
        cancel_transaction(); $error='legacy_source_already_converted'; return false;
    }
    $legacy=hrm_pay_core_003_read_legacy_source($plan['source_kind'],(int)$plan['source_row_id'],true);
    $source_sha=$legacy?hrm_pay_core_003_legacy_source_sha256($plan['source_kind'],$legacy):false;
    if ($source_sha===false || !hash_equals((string)$plan['source_sha256'],$source_sha)) {
        cancel_transaction(); $error='legacy_source_changed'; return false;
    }
    $resolution=json_decode($plan['target_payload_json'],true);
    if (!is_array($resolution)
        || !hash_equals((string)$plan['target_payload_sha256'],(string)hrm_pay_core_003_hash($resolution))
        || !hrm_pay_core_003_validate_mapping_resolution($plan['source_kind'],$resolution)) {
        cancel_transaction(); $error='mapping_payload_invalid'; return false;
    }
    $target=false;
    if ($plan['target_kind']==='element_version_bundle') {
        $target=hrm_pay_core_003_append_element_bundle_internal($resolution,$executor,$error);
    } elseif ($plan['target_kind']==='eligibility_version') {
        $target=hrm_pay_core_003_append_eligibility_internal($resolution,$executor,$error);
    } elseif ($plan['target_kind']==='approved_input') {
        if (trim((string)$legacy['formula'])!==''
            || $resolution['effective_date']<$legacy['effective_from']
            || (!empty($legacy['effective_to']) && $resolution['effective_date']>$legacy['effective_to'])) {
            $error='legacy_employee_salary_not_safe_for_input_conversion';
        } else {
            $candidate=$resolution;
            $candidate['value']=$legacy['amount'];
            $candidate['source_sha256']=$source_sha;
            $target=hrm_pay_core_003_append_approved_input_internal(
                $candidate,$executor,empty($candidate['override_reason'])?true:hrm_pay_core_003_access('SA_HRM_OVERRIDE_PAY_CORE_INPUT'),$error);
        }
    }
    if ($target===false) {
        cancel_transaction();
        if ($error===null) $error='conversion_target_failed';
        return false;
    }
    $target_json=json_encode(hrm_pay_core_003_canonicalize($target),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    if (!is_string($target_json)) {
        cancel_transaction(); $error='conversion_target_encoding_failed'; return false;
    }
    $target_sha=hash('sha256',$target_json);
    $conversion_sha=hrm_pay_core_003_hash(array(
        'mapping_plan_sha256'=>$plan['mapping_plan_sha256'],
        'approval_sha256'=>$plan['approval_sha256'],
        'source_sha256'=>$source_sha,
        'target_set_sha256'=>$target_sha,
        'executor_id'=>$executor,
    ));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_core_003_legacy_conversion_receipts '
        .'(mapping_plan_id,mapping_approval_id,source_kind,source_row_id,source_sha256,target_kind,target_set_json,target_set_sha256,conversion_sha256,executor_id) VALUES ('
        .$mapping_plan_id.','.(int)$plan['mapping_approval_id'].','.db_escape($plan['source_kind']).','.(int)$plan['source_row_id'].','
        .db_escape($source_sha).','.db_escape($plan['target_kind']).','.db_escape($target_json).','.db_escape($target_sha).','
        .db_escape($conversion_sha).','.$executor.')';
    if (!db_query($sql)) {
        cancel_transaction(); $error='conversion_receipt_failed'; return false;
    }
    $receipt_id=(int)db_insert_id();
    commit_transaction();
    return $receipt_id;
}

function hrm_pay_core_003_reconcile_conversion($conversion_receipt_id,$status_token,$evidence_sha256,$recovery_action='none',$supersedes_reconciliation_id=null,&$error=null)
{
    $error=null;$reviewer=hrm_pay_core_003_actor();$conversion_receipt_id=(int)$conversion_receipt_id;
    $allowed=array('reconciled','unresolved','quarantined','recovery_confirmed');if($reviewer<=0||!hrm_pay_core_003_access('SA_HRM_REVIEW_PAY_CORE')||!in_array($status_token,$allowed,true)||!hrm_pay_core_003_is_sha256($evidence_sha256)){$error='access_or_evidence_invalid';return false;}
    if(($status_token==='reconciled'&&$recovery_action!=='none')||($status_token!=='reconciled'&&hrm_pay_core_003_token($recovery_action,48)===false)){$error='recovery_action_invalid';return false;}
    $q=db_query('SELECT source_kind,source_row_id,source_sha256,target_set_sha256,executor_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_conversion_receipts WHERE conversion_receipt_id='.$conversion_receipt_id.' LIMIT 2');$r=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$r||$reviewer===(int)$r['executor_id']){$error='distinct_reviewer_required';return false;}
    $q=db_query('SELECT reconciliation_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_conversion_reconciliations WHERE conversion_receipt_id='.$conversion_receipt_id.' ORDER BY reconciliation_id DESC LIMIT 1 FOR UPDATE');$latest=$q&&db_num_rows($q)===1?(int)db_fetch_assoc($q)['reconciliation_id']:null;$sup=$supersedes_reconciliation_id===null?null:(int)$supersedes_reconciliation_id;if(($latest===null&&$sup!==null)||($latest!==null&&$sup!==$latest)){$error='reconciliation_predecessor_mismatch';return false;}
    $sha=hrm_pay_core_003_hash(array('conversion_receipt_id'=>$conversion_receipt_id,'source_sha256'=>$r['source_sha256'],'target_set_sha256'=>$r['target_set_sha256'],'status_token'=>$status_token,'evidence_sha256'=>$evidence_sha256,'recovery_action'=>$recovery_action,'supersedes_reconciliation_id'=>$sup,'reviewer_id'=>$reviewer));$sql='INSERT INTO '.TB_PREF.'hrm_pay_core_003_legacy_conversion_reconciliations (conversion_receipt_id,source_kind,source_row_id,source_sha256,target_set_sha256,status_token,evidence_sha256,recovery_action,supersedes_reconciliation_id,reconciliation_sha256,reviewer_id) VALUES ('.$conversion_receipt_id.','.db_escape($r['source_kind']).','.(int)$r['source_row_id'].','.db_escape($r['source_sha256']).','.db_escape($r['target_set_sha256']).','.db_escape($status_token).','.db_escape($evidence_sha256).','.db_escape($recovery_action).','.($sup===null?'NULL':$sup).','.db_escape($sha).','.$reviewer.')';return hrm_pay_core_003_insert_idempotent('hrm_pay_core_003_legacy_conversion_reconciliations','reconciliation_sha256',$sha,$sql,'reconciliation_id',$error);
}

function hrm_pay_core_003_record_parity_evidence($conversion_receipt_id,$status_token,$evidence_sha256,&$error=null)
{
    $error=null;$reviewer=hrm_pay_core_003_actor();$conversion_receipt_id=(int)$conversion_receipt_id;if($reviewer<=0||!hrm_pay_core_003_access('SA_HRM_REVIEW_PAY_CORE')||!in_array($status_token,array('match','mismatch','unresolved'),true)||!hrm_pay_core_003_is_sha256($evidence_sha256)){$error='access_or_evidence_invalid';return false;}
    $q=db_query('SELECT source_kind,source_row_id,source_sha256,target_set_sha256,executor_id FROM '.TB_PREF.'hrm_pay_core_003_legacy_conversion_receipts WHERE conversion_receipt_id='.$conversion_receipt_id.' LIMIT 2');$r=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$r||$reviewer===(int)$r['executor_id']){$error='distinct_parity_reviewer_required';return false;}
    $q=db_query('SELECT status_token FROM '.TB_PREF.'hrm_pay_core_003_legacy_conversion_reconciliations WHERE conversion_receipt_id='.$conversion_receipt_id.' ORDER BY reconciliation_id DESC LIMIT 1');$rec=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$rec||!in_array($rec['status_token'],array('reconciled','recovery_confirmed'),true)){$error='reconciliation_required_before_parity';return false;}
    $projection=hrm_pay_core_003_parity_projection_sha256($r['source_sha256'],$r['target_set_sha256'],'legacy_compatibility');$sha=hrm_pay_core_003_hash(array('conversion_receipt_id'=>$conversion_receipt_id,'projection_sha256'=>$projection,'status_token'=>$status_token,'evidence_sha256'=>$evidence_sha256,'verified_by'=>$reviewer));$sql='INSERT INTO '.TB_PREF.'hrm_pay_core_003_compatibility_parity_evidence (conversion_receipt_id,source_kind,source_row_id,source_sha256,target_set_sha256,projection_sha256,status_token,evidence_sha256,verified_by,parity_sha256) VALUES ('.$conversion_receipt_id.','.db_escape($r['source_kind']).','.(int)$r['source_row_id'].','.db_escape($r['source_sha256']).','.db_escape($r['target_set_sha256']).','.db_escape($projection).','.db_escape($status_token).','.db_escape($evidence_sha256).','.$reviewer.','.db_escape($sha).')';return hrm_pay_core_003_insert_idempotent('hrm_pay_core_003_compatibility_parity_evidence','parity_sha256',$sha,$sql,'parity_evidence_id',$error);
}

function hrm_pay_core_003_can_view_sensitive_input(){return hrm_pay_core_003_access('SA_HRM_VIEW_PAY_CORE')&&hrm_pay_core_003_access('SA_HRM_VIEW_SALARY');}
