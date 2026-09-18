<?php
/** PAY-RULE-005 controlled immutable resolution execution. Not production-routed. */
if (!function_exists('hrm_pay_rule_005_seal_fact_set'))
    include_once($path_to_root.'/hrm/includes/payroll/jurisdiction_precedence_store.inc');

function hrm_pay_rule_005_idempotency_hash($key)
{
    if(!is_string($key)||strlen($key)<16||strlen($key)>200||preg_match('/[\x00-\x1F\x7F]/',$key))return false;
    return hash('sha256',$key);
}

function hrm_pay_rule_005_applicability_state_hash($rows)
{
    if(!is_array($rows))return false;$state=array();foreach($rows as $r){if(!isset($r['applicability_id'],$r['applicability_sha256']))return false;$state[]=array('applicability_id'=>(int)$r['applicability_id'],'applicability_sha256'=>(string)$r['applicability_sha256']);}
    usort($state,function($a,$b){return $a['applicability_id']<=>$b['applicability_id'];});return hrm_pay_rule_005_hash_payload($state);
}

function hrm_pay_rule_005_existing_resolution($employment_id,$assignment_id,$idem_hash,$request_hash)
{
    $r=db_query('SELECT r.resolution_id,r.request_sha256,d.outcome_token,d.selected_summary_sha256,d.resolution_sha256,d.explanation_token FROM '.TB_PREF.'hrm_pay_rule_resolution_requests r INNER JOIN '.TB_PREF.'hrm_pay_rule_resolution_decisions d ON d.resolution_id=r.resolution_id WHERE r.employment_id='.(int)$employment_id.' AND r.assignment_id='.(int)$assignment_id.' AND r.idempotency_key_hash='.db_escape($idem_hash).' LIMIT 2 FOR UPDATE','could not read PAY-RULE-005 exact replay');
    if(!$r||db_num_rows($r)===0)return null;if(db_num_rows($r)!==1)return false;$x=db_fetch_assoc($r);if((string)$x['request_sha256']!==$request_hash)return false;
    return array('resolution_id'=>(int)$x['resolution_id'],'outcome'=>(string)$x['outcome_token'],'selected_summary_sha256'=>(string)$x['selected_summary_sha256'],'resolution_sha256'=>(string)$x['resolution_sha256'],'explanation_token'=>(string)$x['explanation_token'],'exact_replay'=>true);
}

function hrm_pay_rule_005_record_resolution($scope,$fact_set,$request_hash,$idem_hash,$app_hash,$result,$actor)
{
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_resolution_requests (employment_id,assignment_id,fact_set_id,as_of_date,idempotency_key_hash,request_sha256,applicability_state_sha256,requested_by) VALUES ('
        .$scope['employment_id'].','.$scope['assignment_id'].','.$fact_set['fact_set_id'].','.db_escape($fact_set['as_of_date']).','.db_escape($idem_hash).','.db_escape($request_hash).','.db_escape($app_hash).','.$actor.')';
    if(!db_query($sql,'could not append PAY-RULE-005 resolution request'))return false;$resolution_id=(int)db_insert_id();if($resolution_id<=0)return false;
    $selected_ids=array();if(isset($result['selected'])&&is_array($result['selected']))foreach($result['selected'] as $g=>$row)$selected_ids[(int)$row['applicability_id']]=true;
    foreach(($result['trace']??array()) as $t){$state=isset($selected_ids[(int)$t['applicability_id']])?'selected':(($result['ok']??false)?'applicable':'rejected');$payload=array('resolution_id'=>$resolution_id,'applicability_id'=>(int)$t['applicability_id'],'rule_version_id'=>(int)$t['rule_version_id'],'precedence_rank'=>(int)$t['rank'],'specificity_depth'=>(int)$t['specificity'],'applicability_mode'=>(string)$t['mode'],'candidate_state'=>$state);$hash=hrm_pay_rule_005_hash_payload($payload);
        $sql='INSERT INTO '.TB_PREF.'hrm_pay_rule_resolution_candidates (resolution_id,applicability_id,rule_version_id,precedence_rank,specificity_depth,applicability_mode,candidate_state,candidate_sha256) VALUES ('.$resolution_id.','.$payload['applicability_id'].','.$payload['rule_version_id'].','.$payload['precedence_rank'].','.$payload['specificity_depth'].','.db_escape($payload['applicability_mode']).','.db_escape($state).','.db_escape($hash).')';if(!db_query($sql,'could not append PAY-RULE-005 resolution candidate trace'))return false;}
    $selected_summary=hrm_pay_rule_005_hash_payload($result['selected']??array());$outcome=hrm_pay_rule_005_ascii_token((string)($result['outcome']??'resolver_failure'),48);if($outcome===false)$outcome='resolver_failure';
    $explanation=isset($result['group'])?'group:'.$result['group']:'deterministic_resolution';$explanation=hrm_pay_rule_005_ascii_token($explanation,96);if($explanation===false)$explanation='deterministic_resolution';
    $resolution_hash=hrm_pay_rule_005_hash_payload(array('resolution_id'=>$resolution_id,'request_sha256'=>$request_hash,'applicability_state_sha256'=>$app_hash,'outcome'=>$outcome,'selected_summary_sha256'=>$selected_summary,'explanation_token'=>$explanation));
    if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_resolution_decisions (resolution_id,outcome_token,selected_summary_sha256,resolution_sha256,explanation_token,decided_by) VALUES ('.$resolution_id.','.db_escape($outcome).','.db_escape($selected_summary).','.db_escape($resolution_hash).','.db_escape($explanation).','.$actor.')','could not append PAY-RULE-005 resolution decision'))return false;
    if(hrm_pay_rule_005_audit('pay_rule_005_resolution',array('resolution_id'=>$resolution_id,'employment_id'=>$scope['employment_id'],'assignment_id'=>$scope['assignment_id']),'pay.rule_005.resolution.decided','pay_rule_005_resolution_execution',$scope['legal_entity_id'],array('outcome_token'=>$outcome,'request_sha256'=>$request_hash,'applicability_state_sha256'=>$app_hash,'resolution_sha256'=>$resolution_hash,'selected_summary_sha256'=>$selected_summary,'production_activation_allowed'=>false,'payroll_amounts_logged'=>false,'sensitive_identity_logged'=>false))===false)return false;
    return array('resolution_id'=>$resolution_id,'outcome'=>$outcome,'selected'=>$result['selected']??array(),'selected_summary_sha256'=>$selected_summary,'resolution_sha256'=>$resolution_hash,'explanation_token'=>$explanation,'exact_replay'=>false);
}

/**
 * Resolve one explicit Employment/Assignment/date request and preserve immutable
 * evidence. Non-resolved outcomes are recorded but never returned as usable
 * payroll decisions (`ok` is false).
 */
function hrm_pay_rule_005_resolve_and_record($employment_id,$assignment_id,$as_of_date,$required_groups,$selections,$idempotency_key)
{
    $actor=hrm_pay_rule_005_actor_id();$idem=hrm_pay_rule_005_idempotency_hash($idempotency_key);
    if($actor<=0||!hrm_pay_rule_005_resolve_allowed()||$idem===false||hrm_pay_rule_005_date($as_of_date)===false||!is_array($required_groups)||!is_array($selections))return false;
    $normalized_groups=array();foreach($required_groups as $group){$group=hrm_pay_rule_005_ascii_token($group,160);if($group===false)return false;$normalized_groups[$group]=true;}if(count($normalized_groups)===0)return false;$required_groups=array_keys($normalized_groups);sort($required_groups,SORT_STRING);ksort($selections,SORT_STRING);
    begin_transaction();$ok=true;$out=false;$scope=hrm_pay_rule_005_resolution_scope($employment_id,$assignment_id,$as_of_date,true);if(!is_array($scope))$ok=false;
    $fact_set_id=$ok?hrm_pay_rule_005_seal_fact_set($employment_id,$assignment_id,$as_of_date):false;$fact_set=$fact_set_id?hrm_pay_rule_005_load_fact_set($fact_set_id):false;if(!is_array($fact_set))$ok=false;
    $candidates=$ok?hrm_pay_rule_005_applicability_candidates($as_of_date,$required_groups,true):false;if(!is_array($candidates))$ok=false;$app_hash=$ok?hrm_pay_rule_005_applicability_state_hash($candidates):false;if($app_hash===false)$ok=false;
    $request_payload=$ok?array('employment_id'=>(int)$employment_id,'assignment_id'=>(int)$assignment_id,'as_of_date'=>$as_of_date,'fact_set_sha256'=>(string)$fact_set['fact_set_sha256'],'required_groups'=>array_values($required_groups),'selections'=>$selections,'applicability_state_sha256'=>$app_hash):array();$request_hash=$ok?hrm_pay_rule_005_hash_payload($request_payload):false;
    if($ok){$replay=hrm_pay_rule_005_existing_resolution($employment_id,$assignment_id,$idem,$request_hash);if($replay===false)$ok=false;elseif(is_array($replay)){$out=$replay;}}
    if($ok&&$out===false){$current=hrm_pay_rule_005_build_fact_material($scope,$as_of_date,true);if(!is_array($current)||(string)$current['source_state_sha256']!==(string)$fact_set['source_state_sha256']){$result=array('ok'=>false,'outcome'=>'stale_source','selected'=>array(),'trace'=>array());}else{$result=hrm_pay_rule_005_resolve($fact_set['items'],$candidates,$required_groups,$selections);}
        $candidates_after=hrm_pay_rule_005_applicability_candidates($as_of_date,$required_groups,true);$after_hash=hrm_pay_rule_005_applicability_state_hash($candidates_after);if($after_hash!==$app_hash)$result=array('ok'=>false,'outcome'=>'stale_applicability','selected'=>array(),'trace'=>$result['trace']??array());
        $out=hrm_pay_rule_005_record_resolution($scope,$fact_set,$request_hash,$idem,$app_hash,$result,$actor);if(!is_array($out))$ok=false;else$out['ok']=($out['outcome']==='resolved');}
    if($ok)commit_transaction();else cancel_transaction();return $ok?$out:false;
}

function hrm_pay_rule_005_recovery_policy()
{
    return array('selective_downgrade_allowed'=>false,'accepted_custody_rewrite_allowed'=>false,'forward_retry_allowed'=>true,'paired_backup_restore_allowed'=>true,'production_activation_allowed'=>false,'production_cutover_owner'=>'PAY-RULE-006');
}
