<?php
/**
 * PAY-CORE-003 governed append-only writers (element versions, eligibility, approved inputs) and the legacy source
 * reader. The legacy mapping approval and conversion chain had no page or caller and was removed; its tables keep
 * their history. This file is deliberately not included by payroll_engine.inc.
 */
require_once(__DIR__.'/../../../includes/value_helpers.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_governance.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_eligibility.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_legacy_mapping_review.inc');
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_003_legacy_compatibility.inc');

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



function hrm_pay_core_003_append_element_bundle_internal(array $x,$actor,&$error)
{
    if(!hrm_pay_core_003_validate_element_version_bundle($x)){$error='element_bundle_invalid';return false;}
    $code=$x['element_code'];$identity_sha=hrm_pay_core_003_hash(array('element_code'=>$code));
    $q=db_query('SELECT pay_element_id,identity_sha256 FROM '.TB_PREF.'hrm_pay_elements WHERE element_code='.db_escape($code).' LIMIT 2 FOR UPDATE');
    if(!$q){$error='element_lookup_failed';return false;}
    if(db_num_rows($q)>1){$error='element_identity_ambiguous';return false;}
    if(db_num_rows($q)===1){$e=db_fetch_assoc($q);if(!hash_equals((string)$e['identity_sha256'],$identity_sha)){$error='element_identity_hash_mismatch';return false;}$element_id=(int)$e['pay_element_id'];}
    else {if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_elements (element_code,identity_sha256,created_by) VALUES ('.db_escape($code).','.db_escape($identity_sha).','.(int)$actor.')')){$error='element_insert_failed';return false;}$element_id=(int)db_insert_id();}
    $version_payload=array('pay_element_id'=>$element_id,'version_no'=>(int)$x['version_no'],'effective_from'=>$x['effective_from'],
      'effective_to'=>empty($x['effective_to'])?null:$x['effective_to'],'element_type'=>$x['element_type'],'calculation_mode'=>$x['calculation_mode'],
      'rule_artifact_sha256'=>empty($x['rule_artifact_sha256'])?null:$x['rule_artifact_sha256']);
    $version_sha=hrm_pay_core_003_hash($version_payload);
    $q=db_query('SELECT element_version_id,version_sha256 FROM '.TB_PREF.'hrm_pay_element_versions WHERE pay_element_id='.$element_id.' AND version_no='.(int)$x['version_no'].' LIMIT 2 FOR UPDATE');if(!$q){$error='version_lookup_failed';return false;}
    if(db_num_rows($q)>1){$error='version_ambiguous';return false;}
    if(db_num_rows($q)===1){$v=db_fetch_assoc($q);if(!hash_equals((string)$v['version_sha256'],$version_sha)){$error='version_replay_mismatch';return false;}$version_id=(int)$v['element_version_id'];}
    else {
      $q=db_query('SELECT version_no,effective_from,effective_to FROM '.TB_PREF.'hrm_pay_element_versions WHERE pay_element_id='
        .$element_id.' ORDER BY version_no DESC LIMIT 1 FOR UPDATE');
      $prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
      if($prev){
        if((int)$x['version_no']!==(int)$prev['version_no']+1||$x['effective_from']<=$prev['effective_from']){
          $error='version_chronology_invalid';
          return false;
        }
        if(empty($prev['effective_to'])||$x['effective_from']<=$prev['effective_to']){
          $error='predecessor_version_must_be_bounded';
          return false;
        }
      }elseif((int)$x['version_no']!==1){
        $error='first_version_must_be_one';
        return false;
      }
        $sql='INSERT INTO '.TB_PREF.'hrm_pay_element_versions (pay_element_id,version_no,effective_from,effective_to,element_type,calculation_mode,rule_artifact_sha256,version_sha256,created_by) VALUES ('
          .$element_id.','.(int)$x['version_no'].','.db_escape($x['effective_from']).','.(!empty($x['effective_to'])?db_escape($x['effective_to']):'NULL')
          .','.db_escape($x['element_type']).','.db_escape($x['calculation_mode']).','.(!empty($x['rule_artifact_sha256'])?db_escape($x['rule_artifact_sha256']):'NULL')
          .','.db_escape($version_sha).','.(int)$actor.')';
        if(!db_query($sql)){
          $error='element_version_insert_failed';
          return false;
        }
        $version_id=(int)db_insert_id();
        }
    $definition_ids=array();
    foreach($x['input_definitions'] as $d){
      $dh=hrm_pay_core_003_hash(array('element_version_id'=>$version_id,'input_code'=>$d['input_code'],'value_type'=>$d['value_type'],
        'unit_token'=>$d['unit_token'],'required_flag'=>(int)$d['required_flag']));
      $q=db_query('SELECT input_definition_id,definition_sha256 FROM '.TB_PREF.'hrm_pay_input_definitions WHERE element_version_id='
        .$version_id.' AND input_code='.db_escape($d['input_code']).' LIMIT 2 FOR UPDATE');
      if(!$q||db_num_rows($q)>1){
        $error='input_definition_lookup_invalid';
        return false;
      }
      if(db_num_rows($q)===1){
        $r=db_fetch_assoc($q);
        if(!hash_equals((string)$r['definition_sha256'],$dh)){
          $error='input_definition_replay_mismatch';
          return false;
        }
        $definition_ids[]=(int)$r['input_definition_id'];
        continue;
      }
      $sql='INSERT INTO '.TB_PREF.'hrm_pay_input_definitions (element_version_id,input_code,value_type,unit_token,required_flag,definition_sha256,created_by) VALUES ('
        .$version_id.','.db_escape($d['input_code']).','.db_escape($d['value_type']).','.db_escape($d['unit_token']).','.(int)$d['required_flag']
        .','.db_escape($dh).','.(int)$actor.')';
      if(!db_query($sql)){
        $error='input_definition_insert_failed';
        return false;
      }
      $definition_ids[]=(int)db_insert_id();
    }
    return array('pay_element_id'=>$element_id,'element_version_id'=>$version_id,'input_definition_ids'=>$definition_ids,'element_identity_sha256'=>$identity_sha,'element_version_sha256'=>$version_sha);
}

function hrm_pay_core_003_write_element_version_bundle(array $x,&$error=null)
{
    $error=null;$actor=current_user_id_or_zero();if($actor<=0||!current_user_has_access('SA_HRM_MANAGE_PAY_CORE')){$error='access_denied';return false;}
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$r=hrm_pay_core_003_append_element_bundle_internal($x,$actor,$error);if($manage){if($r!==false)commit_transaction();else cancel_transaction();}return $r;
}

function hrm_pay_core_003_append_eligibility_internal(array $x,$actor,&$error)
{
    if(!hrm_pay_core_003_validate_eligibility_version($x)){$error='eligibility_version_invalid';return false;}
    $element_version_id=(int)$x['element_version_id'];
    $q=db_query('SELECT element_version_id,effective_from,effective_to FROM '.TB_PREF.'hrm_pay_element_versions WHERE element_version_id='
      .$element_version_id.' LIMIT 2');
    $ev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$ev){
      $error='element_version_missing';
      return false;
    }
    if($x['effective_from']<$ev['effective_from']||(!empty($ev['effective_to'])&&(empty($x['effective_to'])||$x['effective_to']>$ev['effective_to']))){
      $error='eligibility_outside_element_version';
      return false;
    }
    $payload=array('element_version_id'=>$element_version_id,'version_no'=>(int)$x['version_no'],'effective_from'=>$x['effective_from'],'effective_to'=>empty($x['effective_to'])?null:$x['effective_to'],'eligibility_kind'=>$x['eligibility_kind'],'rule_artifact_sha256'=>empty($x['rule_artifact_sha256'])?null:$x['rule_artifact_sha256']);$sha=hrm_pay_core_003_hash($payload);
    $q=db_query('SELECT eligibility_version_id,eligibility_sha256 FROM '.TB_PREF.'hrm_pay_eligibility_versions WHERE element_version_id='
      .$element_version_id.' AND version_no='.(int)$x['version_no'].' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){
      $error='eligibility_lookup_invalid';
      return false;
    }
    if(db_num_rows($q)===1){
      $r=db_fetch_assoc($q);
      if(!hash_equals((string)$r['eligibility_sha256'],$sha)){
        $error='eligibility_replay_mismatch';
        return false;
      }
      return array('eligibility_version_id'=>(int)$r['eligibility_version_id'],'eligibility_sha256'=>$sha);
    }
    $q=db_query('SELECT version_no,effective_from,effective_to FROM '.TB_PREF.'hrm_pay_eligibility_versions WHERE element_version_id='
      .$element_version_id.' ORDER BY version_no DESC LIMIT 1 FOR UPDATE');
    $prev=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if($prev){
      if((int)$x['version_no']!==(int)$prev['version_no']+1||$x['effective_from']<=$prev['effective_from']||empty($prev['effective_to'])||$x['effective_from']<=$prev['effective_to']){
        $error='eligibility_chronology_invalid';
        return false;
      }
    }elseif((int)$x['version_no']!==1){
      $error='first_eligibility_version_must_be_one';
      return false;
    }
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_eligibility_versions (element_version_id,version_no,effective_from,effective_to,eligibility_kind,rule_artifact_sha256,eligibility_sha256,created_by) VALUES ('
      .$element_version_id.','.(int)$x['version_no'].','.db_escape($x['effective_from']).','.(!empty($x['effective_to'])?db_escape($x['effective_to']):'NULL')
      .','.db_escape($x['eligibility_kind']).','.(!empty($x['rule_artifact_sha256'])?db_escape($x['rule_artifact_sha256']):'NULL')
      .','.db_escape($sha).','.(int)$actor.')';
    if(!db_query($sql)){
      $error='eligibility_insert_failed';
      return false;
    }
    return array('eligibility_version_id'=>(int)db_insert_id(),'eligibility_sha256'=>$sha);
}

function hrm_pay_core_003_write_eligibility_version(array $x,&$error=null)
{
    $error=null;$actor=current_user_id_or_zero();if($actor<=0||!current_user_has_access('SA_HRM_MANAGE_PAY_CORE')){$error='access_denied';return false;}
    $manage=empty($GLOBALS['transaction_level']);if($manage)begin_transaction();$r=hrm_pay_core_003_append_eligibility_internal($x,$actor,$error);if($manage){if($r!==false)commit_transaction();else cancel_transaction();}return $r;
}

function hrm_pay_core_003_append_approved_input_internal(array $x,$actor,$override_authorized,&$error)
{
    if(!hrm_pay_core_003_validate_approved_input_candidate($x)){$error='approved_input_invalid';return false;}
    $date=$x['effective_date'];
    $q=db_query('SELECT effective_from,effective_to FROM '.TB_PREF.'hrm_payroll_relationships WHERE payroll_relationship_id='
      .(int)$x['payroll_relationship_id'].' LIMIT 2');
    $rel=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$rel||$date<$rel['effective_from']||(!empty($rel['effective_to'])&&$date>$rel['effective_to'])){
      $error='relationship_scope_invalid';
      return false;
    }
    if(!empty($x['assignment_id'])){
      $q=db_query('SELECT effective_from,effective_to FROM '.TB_PREF.'hrm_payroll_relationship_members WHERE payroll_relationship_id='
        .(int)$x['payroll_relationship_id'].' AND assignment_id='.(int)$x['assignment_id'].' LIMIT 2');
      $mem=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
      if(!$mem||$date<$mem['effective_from']||(!empty($mem['effective_to'])&&$date>$mem['effective_to'])){
        $error='assignment_membership_invalid';
        return false;
      }
    }
    $q=db_query('SELECT d.value_type,v.effective_from,v.effective_to FROM '.TB_PREF.'hrm_pay_input_definitions d INNER JOIN '
      .TB_PREF.'hrm_pay_element_versions v ON v.element_version_id=d.element_version_id WHERE d.input_definition_id='.(int)$x['input_definition_id']
      .' AND d.element_version_id='.(int)$x['element_version_id'].' LIMIT 2');
    $bind=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$bind||$bind['value_type']!==$x['value_type']||$date<$bind['effective_from']||(!empty($bind['effective_to'])&&$date>$bind['effective_to'])){
      $error='input_definition_binding_invalid';
      return false;
    }
    $override=isset($x['override_reason'])&&$x['override_reason']!==null?trim((string)$x['override_reason']):null;if($override!==null&&!$override_authorized){$error='override_authority_required';return false;}
    $supersedes=isset($x['supersedes_input_id'])&&$x['supersedes_input_id']!==null?(int)$x['supersedes_input_id']:null;
    if($supersedes!==null){
      if($override===null||!$override_authorized){
        $error='supersession_requires_authorized_override_reason';
        return false;
      }
      $q=db_query('SELECT payroll_relationship_id,input_definition_id,effective_date FROM '.TB_PREF.'hrm_payroll_approved_inputs WHERE approved_input_id='
        .$supersedes.' LIMIT 2 FOR UPDATE');
      $old=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
      if(!$old||(int)$old['payroll_relationship_id']!==(int)$x['payroll_relationship_id']||(int)$old['input_definition_id']!==(int)$x['input_definition_id']||$old['effective_date']!==$date){
        $error='superseded_input_scope_mismatch';
        return false;
      }
      $q=db_query('SELECT approved_input_id FROM '.TB_PREF.'hrm_payroll_approved_inputs WHERE supersedes_input_id='.$supersedes
        .' LIMIT 2 FOR UPDATE');
      if(!$q||db_num_rows($q)>0){
        $error='superseded_input_already_replaced';
        return false;
      }
    }
    $payload=array('payroll_relationship_id'=>(int)$x['payroll_relationship_id'],'assignment_id'=>empty($x['assignment_id'])?null:(int)$x['assignment_id'],
      'element_version_id'=>(int)$x['element_version_id'],'input_definition_id'=>(int)$x['input_definition_id'],'effective_date'=>$date,
      'value_type'=>$x['value_type'],'value'=>$x['value'],'source_kind'=>$x['source_kind'],'source_event_id'=>$x['source_event_id'],
      'source_sha256'=>$x['source_sha256'],'approval_evidence_sha256'=>$x['approval_evidence_sha256'],'approved_by'=>(int)$actor,
      'override_reason'=>$override,'supersedes_input_id'=>$supersedes);
    $sha=hrm_pay_core_003_hash($payload);
    $q=db_query('SELECT approved_input_id,input_sha256 FROM '.TB_PREF.'hrm_payroll_approved_inputs WHERE input_definition_id='
      .(int)$x['input_definition_id'].' AND source_kind='.db_escape($x['source_kind']).' AND source_event_id='.db_escape($x['source_event_id'])
      .' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){
      $error='source_event_state_invalid';
      return false;
    }
    if(db_num_rows($q)===1){
      $r=db_fetch_assoc($q);
      if(!hash_equals((string)$r['input_sha256'],$sha)){
        $error='source_event_replay_mismatch';
        return false;
      }
      return array('approved_input_id'=>(int)$r['approved_input_id'],'input_sha256'=>$sha);
    }
    $decimal=$datev=$code=$boolean='NULL';if($x['value_type']==='decimal')$decimal=db_escape((string)$x['value']);elseif($x['value_type']==='date')$datev=db_escape($x['value']);elseif($x['value_type']==='code')$code=db_escape($x['value']);else $boolean=(int)((string)$x['value']==='1'||$x['value']===true);
    $sql='INSERT INTO '.TB_PREF.'hrm_payroll_approved_inputs (payroll_relationship_id,assignment_id,element_version_id,input_definition_id,effective_date,value_type,value_decimal,value_date,value_code,value_boolean,source_kind,source_event_id,source_sha256,approval_evidence_sha256,approved_by,approved_at,override_reason,supersedes_input_id,input_sha256) VALUES ('
      .(int)$x['payroll_relationship_id'].','.(!empty($x['assignment_id'])?(int)$x['assignment_id']:'NULL').','.(int)$x['element_version_id']
      .','.(int)$x['input_definition_id'].','.db_escape($date).','.db_escape($x['value_type']).','.$decimal.','.$datev.','
      .$code.','.$boolean.','.db_escape($x['source_kind']).','.db_escape($x['source_event_id']).','.db_escape($x['source_sha256'])
      .','.db_escape($x['approval_evidence_sha256']).','.(int)$actor.',NOW(),'.($override!==null?db_escape($override):'NULL')
      .','.($supersedes!==null?$supersedes:'NULL').','.db_escape($sha).')';
    if(!db_query($sql)){
      $error='approved_input_insert_failed';
      return false;
    }
    return array('approved_input_id'=>(int)db_insert_id(),'input_sha256'=>$sha);
}

function hrm_pay_core_003_write_approved_input(array $x,&$error=null)
{
    $error=null;
    $actor=current_user_id_or_zero();
    if($actor<=0||!current_user_has_access('SA_HRM_EXECUTE_PAY_CORE_INPUT')){
      $error='access_denied';
      return false;
    }
    $override=empty($x['override_reason'])?true:current_user_has_access('SA_HRM_OVERRIDE_PAY_CORE_INPUT');
    $manage=empty($GLOBALS['transaction_level']);
    if($manage)begin_transaction();
    $r=hrm_pay_core_003_append_approved_input_internal($x,$actor,$override,$error);
    if($manage){
      if($r!==false)commit_transaction();
      else cancel_transaction();
    }
    return $r;
}




function hrm_pay_core_003_can_view_sensitive_input(){return current_user_has_access('SA_HRM_VIEW_PAY_CORE')&&current_user_has_access('SA_HRM_VIEW_SALARY');}
