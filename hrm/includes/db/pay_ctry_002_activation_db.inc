<?php
/**
 * PAY-CTRY-002 approved activation execution and append-only binding custody.
 *
 * It records an independently approved lifecycle decision and one exact scope
 * binding event atomically after a fresh single-use step-up event. Runtime
 * consumers are wired by pay_ctry_002_runtime_db.inc and consume only the
 * immutable metadata produced here.
 */
$pay_ctry_002_activation_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$pay_ctry_002_activation_dependencies = array(
    array('/includes/pay_ctry_002_contract.inc', 'pay_ctry_002_validate_execution_descriptor'),
    array('/hrm/includes/db/pay_ctry_002_schema_db.inc', 'pay_ctry_002_schema_groups'),
    array('/includes/federation_assurance_consumer.inc', 'federation_assurance_consumer_current_session_id'),
    array('/includes/db/domain_audit_db.inc', 'append_domain_audit_event'),
);
foreach ($pay_ctry_002_activation_dependencies as $dependency) {
    if (function_exists($dependency[1])) continue;
    $candidate = $pay_ctry_002_activation_root.$dependency[0];
    if (is_file($candidate)) include_once($candidate);
}
unset($pay_ctry_002_activation_root, $pay_ctry_002_activation_dependencies, $dependency, $candidate);

function pay_ctry_002_activation_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}
function pay_ctry_002_activation_company_id()
{
    return function_exists('user_company') ? (int)user_company() : -1;
}
function pay_ctry_002_activation_access()
{
    return function_exists('user_check_access') && user_check_access(PAY_CTRY_002_EXECUTOR_PERMISSION);
}
function pay_ctry_002_activation_hash_equals($a,$b)
{
    return is_string($a)&&is_string($b)&&function_exists('hash_equals')?hash_equals($a,$b):((string)$a===(string)$b);
}
function pay_ctry_002_activation_fetch_one($sql)
{
    $q=db_query($sql);if(!$q||db_num_rows($q)!==1)return false;return db_fetch_assoc($q);
}
function pay_ctry_002_activation_insert_id()
{
    return function_exists('db_insert_id')?(int)db_insert_id():0;
}
function pay_ctry_002_activation_nullable_uint_sql($value)
{
    return $value===null||$value===''||$value===0||$value==='0'?'NULL':(string)(int)$value;
}
function pay_ctry_002_activation_nullable_text_sql($value)
{
    return $value===null||$value===''?'NULL':db_escape((string)$value);
}

/** Exact action hash consumed by the single-use privileged-assurance event. */
function pay_ctry_002_execution_action_hash(array $row)
{
    foreach(array('activation_proposal_id','activation_approval_id','site_installation_id','company_id','scope_type','action_type','effective_from','proposal_sha256','approval_sha256','impact_sha256') as $k)
        if(!array_key_exists($k,$row))return false;
    $scope=array('company_id'=>$row['company_id'],'scope_type'=>$row['scope_type'],'legal_entity_id'=>isset($row['legal_entity_id'])?$row['legal_entity_id']:null,'payroll_scope_key'=>isset($row['payroll_scope_key'])?$row['payroll_scope_key']:'');
    $error=null;if(!pay_ctry_002_validate_scope($scope,$error))return false;
    return pay_ctry_002_hash(array(
        'action_class'=>PAY_CTRY_002_EXECUTION_ACTION_CLASS,
        'activation_proposal_id'=>(int)$row['activation_proposal_id'],
        'activation_approval_id'=>(int)$row['activation_approval_id'],
        'site_installation_id'=>(int)$row['site_installation_id'],
        'company_id'=>(int)$row['company_id'],
        'scope_type'=>(string)$row['scope_type'],
        'legal_entity_id'=>empty($row['legal_entity_id'])?null:(int)$row['legal_entity_id'],
        'payroll_scope_key'=>empty($row['payroll_scope_key'])?null:(string)$row['payroll_scope_key'],
        'action_type'=>(string)$row['action_type'],
        'effective_from'=>(string)$row['effective_from'],
        'effective_to'=>empty($row['effective_to'])?null:(string)$row['effective_to'],
        'proposal_sha256'=>(string)$row['proposal_sha256'],
        'approval_sha256'=>(string)$row['approval_sha256'],
        'impact_sha256'=>(string)$row['impact_sha256']));
}
function pay_ctry_002_execution_assurance_event_for_update($session_id,$action_hash)
{
    if((int)$session_id<=0||pay_ctry_002_sha256($action_hash)===false)return false;
    $q=db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,issued_at,expires_at,consumed_at FROM ".TB_PREF."user_session_assurance_events WHERE session_id=".(int)$session_id." AND action_class=".db_escape(PAY_CTRY_002_EXECUTION_ACTION_CLASS)." AND action_hash=".db_escape($action_hash)." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE",'could not lock PAY-CTRY-002 execution assurance event');
    if(!$q)return false;$row=db_fetch_assoc($q);if(!$row)return null;return db_fetch_assoc($q)===false?$row:false;
}
function pay_ctry_002_consume_execution_assurance($company,$action_hash,$now_epoch)
{
    if(!function_exists('federation_assurance_consumer_current_session_id')||!function_exists('federation_assurance_consumer_custody_for_update')||!function_exists('federation_assurance_consumer_current_custody_valid')||!function_exists('federation_assurance_consumer_restore_level_one_sql')||!function_exists('append_domain_audit_event'))return false;
    $session_id=federation_assurance_consumer_current_session_id();if($session_id<=0)return false;
    $custody=federation_assurance_consumer_custody_for_update($session_id);if(!federation_assurance_consumer_current_custody_valid($custody,$now_epoch,true))return false;
    $event=pay_ctry_002_execution_assurance_event_for_update($session_id,$action_hash);
    if(!is_array($event)||(int)$event['assurance_level']!==2||(string)$event['assurance_method']!=='oidc_acr_amr'||(string)$event['action_class']!==PAY_CTRY_002_EXECUTION_ACTION_CLASS||!pay_ctry_002_activation_hash_equals((string)$event['action_hash'],$action_hash)||$event['consumed_at']!==null)return false;
    $issued=strtotime((string)$event['issued_at'].' UTC');$expires=strtotime((string)$event['expires_at'].' UTC');if($issued===false||$expires===false||$now_epoch<$issued||$now_epoch>=$expires)return false;
    $at=gmdate('Y-m-d H:i:s',$now_epoch);
    if(!db_query('UPDATE '.TB_PREF.'user_session_assurance_events SET consumed_at='.db_escape($at).' WHERE assurance_event_id='.(int)$event['assurance_event_id'].' AND consumed_at IS NULL AND expires_at>'.db_escape($at),'could not consume PAY-CTRY-002 execution assurance')||(function_exists('db_num_affected_rows')&&(int)db_num_affected_rows()!==1))return false;
    $restore=federation_assurance_consumer_restore_level_one_sql($custody,$now_epoch);if($restore===false||!db_query('UPDATE '.TB_PREF.'user_sessions SET '.$restore.' WHERE id='.$session_id." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'",'could not reset PAY-CTRY-002 execution session assurance')||(function_exists('db_num_affected_rows')&&(int)db_num_affected_rows()!==1))return false;
    if(append_domain_audit_event(array('company_id'=>(int)$company,'legal_entity_id'=>0,'entity_type'=>'security_session_assurance','entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),'action'=>'security.session.assurance.consumed','reason_code'=>'pay_ctry_002_activation_execution','before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),'restricted_diff'=>array('action_class'=>PAY_CTRY_002_EXECUTION_ACTION_CLASS,'action_hash'=>$action_hash,'single_use_event_consumed'=>true,'session_assurance_reset_to_level_one'=>true,'payroll_result_mutated'=>false,'historical_payroll_accounting_rows_rewritten'=>0)))===false)return false;
    return (int)$event['assurance_event_id'];
}

/** Consume a separate single-use step-up class for suspension/revocation. */
function pay_ctry_002_lifecycle_assurance_event_for_update($session_id,$action_hash)
{
    if((int)$session_id<=0||pay_ctry_002_sha256($action_hash)===false)return false;
    $q=db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,issued_at,expires_at,consumed_at FROM ".TB_PREF."user_session_assurance_events WHERE session_id=".(int)$session_id." AND action_class=".db_escape(PAY_CTRY_002_LIFECYCLE_ACTION_CLASS)." AND action_hash=".db_escape($action_hash)." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE",'could not lock PAY-CTRY-002 lifecycle assurance event');
    if(!$q)return false;$row=db_fetch_assoc($q);if(!$row)return null;return db_fetch_assoc($q)===false?$row:false;
}
function pay_ctry_002_consume_lifecycle_assurance($company,$action_hash,$now_epoch)
{
    if(!function_exists('federation_assurance_consumer_current_session_id')||!function_exists('federation_assurance_consumer_custody_for_update')||!function_exists('federation_assurance_consumer_current_custody_valid')||!function_exists('federation_assurance_consumer_restore_level_one_sql')||!function_exists('append_domain_audit_event'))return false;
    $session_id=federation_assurance_consumer_current_session_id();if($session_id<=0)return false;$custody=federation_assurance_consumer_custody_for_update($session_id);if(!federation_assurance_consumer_current_custody_valid($custody,$now_epoch,true))return false;
    $event=pay_ctry_002_lifecycle_assurance_event_for_update($session_id,$action_hash);if(!is_array($event)||(int)$event['assurance_level']!==2||(string)$event['assurance_method']!=='oidc_acr_amr'||(string)$event['action_class']!==PAY_CTRY_002_LIFECYCLE_ACTION_CLASS||!pay_ctry_002_activation_hash_equals((string)$event['action_hash'],$action_hash)||$event['consumed_at']!==null)return false;
    $issued=strtotime((string)$event['issued_at'].' UTC');$expires=strtotime((string)$event['expires_at'].' UTC');if($issued===false||$expires===false||$now_epoch<$issued||$now_epoch>=$expires)return false;$at=gmdate('Y-m-d H:i:s',$now_epoch);
    if(!db_query('UPDATE '.TB_PREF.'user_session_assurance_events SET consumed_at='.db_escape($at).' WHERE assurance_event_id='.(int)$event['assurance_event_id'].' AND consumed_at IS NULL AND expires_at>'.db_escape($at),'could not consume PAY-CTRY-002 lifecycle assurance')||(function_exists('db_num_affected_rows')&&(int)db_num_affected_rows()!==1))return false;
    $restore=federation_assurance_consumer_restore_level_one_sql($custody,$now_epoch);if($restore===false||!db_query('UPDATE '.TB_PREF.'user_sessions SET '.$restore.' WHERE id='.$session_id." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'",'could not reset PAY-CTRY-002 lifecycle session assurance')||(function_exists('db_num_affected_rows')&&(int)db_num_affected_rows()!==1))return false;
    if(append_domain_audit_event(array('company_id'=>(int)$company,'legal_entity_id'=>0,'entity_type'=>'security_session_assurance','entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),'action'=>'security.session.assurance.consumed','reason_code'=>'pay_ctry_002_activation_lifecycle','before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),'restricted_diff'=>array('action_class'=>PAY_CTRY_002_LIFECYCLE_ACTION_CLASS,'action_hash'=>$action_hash,'single_use_event_consumed'=>true,'session_assurance_reset_to_level_one'=>true,'payroll_result_mutated'=>false,'historical_payroll_accounting_rows_rewritten'=>0)))===false)return false;
    return (int)$event['assurance_event_id'];
}

/** A predecessor suspension or hard revoke blocks a new execution receipt. */
function pay_ctry_002_activation_lifecycle_clear_for_execution($binding_id,$now_epoch,&$error=null)
{
    $error=null;if((int)$binding_id<=0)return true;
    if(!function_exists('hrm_migration_table_exists')||!hrm_migration_table_exists('pay_ctry_activation_suspensions')||!hrm_migration_table_exists('pay_ctry_activation_revocations')){$error='lifecycle_schema_unavailable';return false;}
    $at=gmdate('Y-m-d H:i:s',(int)$now_epoch);$q=db_query('SELECT revocation_id FROM '.TB_PREF.'pay_ctry_activation_revocations WHERE binding_id='.(int)$binding_id.' AND effective_at<='.db_escape($at).' ORDER BY effective_at DESC,revocation_id DESC LIMIT 1 FOR UPDATE');if(!$q){$error='revocation_state_unavailable';return false;}if(db_num_rows($q)!==0){$error='hard_revoked_for_new_execution';return false;}
    $q=db_query('SELECT action_type FROM '.TB_PREF.'pay_ctry_activation_suspensions WHERE binding_id='.(int)$binding_id.' AND effective_at<='.db_escape($at).' ORDER BY effective_at DESC,suspension_id DESC LIMIT 1 FOR UPDATE');if(!$q){$error='suspension_state_unavailable';return false;}if(db_num_rows($q)===1&&db_fetch_assoc($q)['action_type']==='suspend'){$error='activation_suspended_for_new_execution';return false;}return true;
}

/** Re-check immutable install roots and current PAY-CTRY-001 trust immediately before execution. */
function pay_ctry_002_activation_pack_trust_for_update(array $proposal,$now_epoch,&$error)
{
    $error=null;$installation_id=(int)$proposal['site_installation_id'];$pack_version_id=(int)$proposal['pack_version_id'];
    if($installation_id<=0||$pack_version_id<=0){$error='activation_installation_invalid';return false;}
    $sql='SELECT s.site_installation_id,s.pack_version_id,s.semantic_root AS installation_semantic_root,s.evidence_root AS installation_evidence_root,'
        .'v.semantic_root AS pack_semantic_root,v.evidence_root AS pack_evidence_root,v.publisher_code,v.signer_code,v.key_id,'
        .'j.pack_version_id AS journal_pack_version_id,j.verification_report_id,'
        .'r.pack_version_id AS report_pack_version_id,r.final_status,r.semantic_root AS report_semantic_root,r.evidence_root AS report_evidence_root,'
        .'pu.publisher_id,pu.valid_from AS publisher_valid_from,pu.valid_to AS publisher_valid_to,'
        .'si.signer_id,tk.trust_key_id,tk.valid_from AS key_valid_from,tk.valid_to AS key_valid_to,tk.trusted_at '
        .'FROM '.TB_PREF.'pay_ctry_site_installations s '
        .'INNER JOIN '.TB_PREF.'pay_ctry_pack_versions v ON v.pack_version_id=s.pack_version_id '
        .'INNER JOIN '.TB_PREF.'pay_ctry_site_operation_journal j ON j.operation_event_id=s.operation_event_id '
        .'INNER JOIN '.TB_PREF.'pay_ctry_verification_reports r ON r.verification_report_id=j.verification_report_id '
        .'INNER JOIN '.TB_PREF.'pay_ctry_publishers pu ON pu.publisher_code=v.publisher_code '
        .'INNER JOIN '.TB_PREF.'pay_ctry_signers si ON si.signer_code=v.signer_code AND si.publisher_id=pu.publisher_id '
        .'INNER JOIN '.TB_PREF.'pay_ctry_trust_keys tk ON tk.key_id=v.key_id AND tk.signer_id=si.signer_id '
        .'WHERE s.site_installation_id='.$installation_id.' LIMIT 2 FOR UPDATE';
    $q=db_query($sql,'could not lock PAY-CTRY-002 installed pack trust custody');if(!$q||db_num_rows($q)!==1){$error='installed_pack_trust_custody_missing';return false;}$r=db_fetch_assoc($q);
    if((int)$r['pack_version_id']!==$pack_version_id||(int)$r['journal_pack_version_id']!==$pack_version_id||(int)$r['report_pack_version_id']!==$pack_version_id||(string)$r['final_status']!=='accepted'){$error='installed_pack_verification_invalid';return false;}
    foreach(array(array('installation_semantic_root','pack_semantic_root'),array('installation_evidence_root','pack_evidence_root'),array('report_semantic_root','pack_semantic_root'),array('report_evidence_root','pack_evidence_root')) as $pair)if(!pay_ctry_002_activation_hash_equals((string)$r[$pair[0]],(string)$r[$pair[1]])){$error='installed_pack_root_mismatch';return false;}
    foreach(array(array('publisher_valid_from','publisher_valid_to'),array('key_valid_from','key_valid_to')) as $pair){$from=strtotime((string)$r[$pair[0]].' UTC');$to=$r[$pair[1]]===null?null:strtotime((string)$r[$pair[1]].' UTC');if($from===false||$now_epoch<$from||($to!==null&&($to===false||$now_epoch>$to))){$error='installed_pack_trust_expired_or_not_yet_valid';return false;}}
    $trusted=strtotime((string)$r['trusted_at'].' UTC');if($trusted===false||$now_epoch<$trusted){$error='installed_pack_trust_time_invalid';return false;}
    $pq=db_query('SELECT publisher_revocation_id FROM '.TB_PREF.'pay_ctry_publisher_revocations WHERE publisher_id='.(int)$r['publisher_id'].' LIMIT 2 FOR UPDATE','could not lock PAY-CTRY-002 publisher revocation custody');if(!$pq||db_num_rows($pq)>1){$error='publisher_revocation_state_ambiguous';return false;}if(db_num_rows($pq)!==0){$error='publisher_revoked_for_new_execution';return false;}
    $kq=db_query('SELECT key_revocation_id FROM '.TB_PREF.'pay_ctry_trust_key_revocations WHERE trust_key_id='.(int)$r['trust_key_id'].' LIMIT 2 FOR UPDATE','could not lock PAY-CTRY-002 key revocation custody');if(!$kq||db_num_rows($kq)>1){$error='key_revocation_state_ambiguous';return false;}if(db_num_rows($kq)!==0){$error='key_revoked_for_new_execution';return false;}
    $sq=db_query('SELECT effective_at FROM '.TB_PREF.'pay_ctry_trust_key_supersessions WHERE predecessor_key_id='.(int)$r['trust_key_id'].' LIMIT 2 FOR UPDATE','could not lock PAY-CTRY-002 key supersession custody');if(!$sq||db_num_rows($sq)>1){$error='key_supersession_state_ambiguous';return false;}if(db_num_rows($sq)===1){$sup=db_fetch_assoc($sq);$at=strtotime((string)$sup['effective_at'].' UTC');if($at===false){$error='key_supersession_time_invalid';return false;}if($now_epoch>=$at){$error='key_superseded_for_new_execution';return false;}}
    return true;
}

function pay_ctry_002_activation_joined_row_for_update($proposal_id)
{
    $sql='SELECT p.*,a.activation_approval_id,a.decision_token,a.approval_sha256,a.checker_id,a.proposal_sha256 AS approval_proposal_sha256,'
        .'i.impact_sha256 AS impact_row_sha256,i.site_installation_id AS impact_site_installation_id,i.company_id AS impact_company_id,i.scope_type AS impact_scope_type,i.legal_entity_id AS impact_legal_entity_id,i.payroll_scope_key AS impact_payroll_scope_key,i.action_type AS impact_action_type,i.effective_from AS impact_effective_from,i.effective_to AS impact_effective_to,i.dependency_lock_sha256 AS impact_dependency_lock_sha256,i.binding_snapshot_sha256 AS impact_binding_snapshot_sha256,i.open_run_count,i.open_approval_count,i.downstream_intent_count,i.blocking_finding_count,'
        .'s.pack_version_id,s.semantic_root,s.evidence_root FROM '.TB_PREF.'pay_ctry_activation_proposals p INNER JOIN '.TB_PREF.'pay_ctry_activation_approvals a ON a.activation_proposal_id=p.activation_proposal_id INNER JOIN '.TB_PREF.'pay_ctry_activation_impact_simulations i ON i.impact_simulation_id=p.impact_simulation_id INNER JOIN '.TB_PREF.'pay_ctry_site_installations s ON s.site_installation_id=p.site_installation_id WHERE p.activation_proposal_id='.(int)$proposal_id.' LIMIT 2 FOR UPDATE';
    return pay_ctry_002_activation_fetch_one($sql);
}
function pay_ctry_002_activation_row_is_exact(array $r,$executor,&$error)
{
    $error=null;
    if((string)$r['decision_token']!=='approved'){$error='proposal_not_approved';return false;}
    if(!pay_ctry_002_distinct_maker_checker_executor($r['maker_id'],$r['checker_id'],$executor)){$error='three_party_separation_required';return false;}
    if((int)$r['company_id']!==pay_ctry_002_activation_company_id()){$error='company_scope_mismatch';return false;}
    foreach(array('site_installation_id'=>'impact_site_installation_id','company_id'=>'impact_company_id','scope_type'=>'impact_scope_type','action_type'=>'impact_action_type','effective_from'=>'impact_effective_from','dependency_lock_sha256'=>'impact_dependency_lock_sha256','binding_snapshot_sha256'=>'impact_binding_snapshot_sha256') as $a=>$b)if((string)$r[$a]!== (string)$r[$b]){$error='impact_proposal_drift';return false;}
    $le1=empty($r['legal_entity_id'])?0:(int)$r['legal_entity_id'];$le2=empty($r['impact_legal_entity_id'])?0:(int)$r['impact_legal_entity_id'];$ps1=empty($r['payroll_scope_key'])?'':(string)$r['payroll_scope_key'];$ps2=empty($r['impact_payroll_scope_key'])?'':(string)$r['impact_payroll_scope_key'];$to1=empty($r['effective_to'])?'':(string)$r['effective_to'];$to2=empty($r['impact_effective_to'])?'':(string)$r['impact_effective_to'];
    if($le1!==$le2||$ps1!==$ps2||$to1!==$to2||!pay_ctry_002_activation_hash_equals((string)$r['proposal_sha256'],(string)$r['approval_proposal_sha256'])||!pay_ctry_002_activation_hash_equals((string)$r['impact_sha256'],(string)$r['impact_row_sha256'])){$error='approval_or_scope_drift';return false;}
    foreach(array('open_run_count','open_approval_count','downstream_intent_count','blocking_finding_count') as $k)if((int)$r[$k]!==0){$error='impact_not_clear';return false;}
    if(!in_array((string)$r['action_type'],pay_ctry_002_executable_binding_actions(),true)){$error='action_deferred_to_later_lifecycle';return false;}
    return true;
}
function pay_ctry_002_scope_binding_leaf_for_update(array $r)
{
    $legal=empty($r['legal_entity_id'])?' IS NULL':'='.(int)$r['legal_entity_id'];$payroll=empty($r['payroll_scope_key'])?' IS NULL':'='.db_escape((string)$r['payroll_scope_key']);
    $sql='SELECT binding_id,activation_id,action_type,effective_from,effective_to,binding_sha256 FROM '.TB_PREF.'pay_ctry_activation_bindings WHERE company_id='.(int)$r['company_id'].' AND scope_type='.db_escape((string)$r['scope_type']).' AND legal_entity_id'.$legal.' AND payroll_scope_key'.$payroll.' ORDER BY binding_id DESC LIMIT 2 FOR UPDATE';
    $q=db_query($sql);if(!$q||db_num_rows($q)>1)return false;return db_num_rows($q)===1?db_fetch_assoc($q):null;
}
function pay_ctry_002_existing_execution_for_update($proposal_id)
{
    $q=db_query('SELECT a.activation_id,a.activation_proposal_id,a.activation_approval_id,a.activation_sha256,a.executor_id,b.binding_id,b.binding_sha256,b.bound_by FROM '.TB_PREF.'pay_ctry_activations a LEFT JOIN '.TB_PREF.'pay_ctry_activation_bindings b ON b.activation_id=a.activation_id WHERE a.activation_proposal_id='.(int)$proposal_id.' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1)return false;return db_num_rows($q)===1?db_fetch_assoc($q):null;
}

/**
 * Atomically record one independently approved activation and its exact scope binding.
 * No runtime payroll resolver consumes these rows in this batch.
 */
function pay_ctry_002_execute_approved_activation($proposal_id,$now_epoch=null,&$error=null)
{
    $error=null;$proposal_id=pay_ctry_002_uint($proposal_id,false);$actor=pay_ctry_002_activation_actor_id();$company=pay_ctry_002_activation_company_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($proposal_id===false||$actor<=0||$company<0||$now_epoch<=0||!pay_ctry_002_activation_access()){$error='execution_unauthorized_or_invalid';return false;}
    if(!empty($GLOBALS['transaction_level'])){$error='execution_requires_owned_transaction';return false;}
    begin_transaction();$ok=false;$out=false;
    do{
        $row=pay_ctry_002_activation_joined_row_for_update($proposal_id);if(!is_array($row)||!pay_ctry_002_activation_row_is_exact($row,$actor,$error)||!pay_ctry_002_activation_pack_trust_for_update($row,$now_epoch,$error))break;
        $existing=pay_ctry_002_existing_execution_for_update($proposal_id);if($existing===false){$error='execution_replay_ambiguous';break;}
        if(is_array($existing)){
            if(empty($existing['binding_id'])||(int)$existing['executor_id']!==$actor||(int)$existing['bound_by']!==$actor){$error='execution_replay_mismatch';break;}
            $ok=true;$out=array('activation_id'=>(int)$existing['activation_id'],'binding_id'=>(int)$existing['binding_id'],'activation_sha256'=>(string)$existing['activation_sha256'],'binding_sha256'=>(string)$existing['binding_sha256'],'status'=>'already_executed');break;
        }
        $leaf=pay_ctry_002_scope_binding_leaf_for_update($row);if($leaf===false){$error='binding_lineage_ambiguous';break;}$pred_action=is_array($leaf)?(string)$leaf['action_type']:null;
        if(is_array($leaf)&&!pay_ctry_002_activation_lifecycle_clear_for_execution((int)$leaf['binding_id'],$now_epoch,$error))break;
        if(!pay_ctry_002_binding_transition_allowed((string)$row['action_type'],$pred_action)){$error='binding_transition_invalid';break;}
        if(is_array($leaf)&&strcmp((string)$row['effective_from'],(string)$leaf['effective_from'])<0){$error='binding_effective_date_precedes_predecessor';break;}
        $action_hash=pay_ctry_002_execution_action_hash($row);if($action_hash===false){$error='execution_action_hash_invalid';break;}
        $assurance_event_id=pay_ctry_002_consume_execution_assurance($company,$action_hash,$now_epoch);if($assurance_event_id===false){$error='fresh_step_up_required';break;}
        $activation_key='ctry2.activation.'.substr(pay_ctry_002_hash(array('proposal_id'=>$proposal_id,'proposal_sha256'=>$row['proposal_sha256'],'approval_sha256'=>$row['approval_sha256'])),0,48);
        $descriptor=array('activation_key'=>$activation_key,'activation_proposal_id'=>$proposal_id,'activation_approval_id'=>(int)$row['activation_approval_id'],'site_installation_id'=>(int)$row['site_installation_id'],'company_id'=>(int)$row['company_id'],'scope_type'=>(string)$row['scope_type'],'legal_entity_id'=>empty($row['legal_entity_id'])?null:(int)$row['legal_entity_id'],'payroll_scope_key'=>empty($row['payroll_scope_key'])?null:(string)$row['payroll_scope_key'],'action_type'=>(string)$row['action_type'],'effective_from'=>(string)$row['effective_from'],'effective_to'=>empty($row['effective_to'])?null:(string)$row['effective_to'],'dependency_lock_sha256'=>(string)$row['dependency_lock_sha256'],'binding_snapshot_sha256'=>(string)$row['binding_snapshot_sha256'],'proposal_sha256'=>(string)$row['proposal_sha256'],'approval_sha256'=>(string)$row['approval_sha256'],'assurance_event_id'=>$assurance_event_id,'execution_action_sha256'=>$action_hash,'executor_id'=>$actor);
        $descriptor['activation_sha256']=pay_ctry_002_hash($descriptor);if(!pay_ctry_002_validate_execution_descriptor($descriptor,$row['maker_id'],$row['checker_id'],$error))break;
        $now=gmdate('Y-m-d H:i:s',$now_epoch);
        $sql='INSERT INTO '.TB_PREF.'pay_ctry_activations (activation_key,activation_proposal_id,activation_approval_id,site_installation_id,company_id,scope_type,legal_entity_id,payroll_scope_key,action_type,effective_from,effective_to,dependency_lock_sha256,binding_snapshot_sha256,proposal_sha256,approval_sha256,assurance_event_id,execution_action_sha256,activation_sha256,executor_id,created_at) VALUES ('.db_escape($descriptor['activation_key']).','.$proposal_id.','.(int)$descriptor['activation_approval_id'].','.(int)$descriptor['site_installation_id'].','.$company.','.db_escape($descriptor['scope_type']).','.pay_ctry_002_activation_nullable_uint_sql($descriptor['legal_entity_id']).','.pay_ctry_002_activation_nullable_text_sql($descriptor['payroll_scope_key']).','.db_escape($descriptor['action_type']).','.db_escape($descriptor['effective_from']).','.pay_ctry_002_activation_nullable_text_sql($descriptor['effective_to']).','.db_escape($descriptor['dependency_lock_sha256']).','.db_escape($descriptor['binding_snapshot_sha256']).','.db_escape($descriptor['proposal_sha256']).','.db_escape($descriptor['approval_sha256']).','.$assurance_event_id.','.db_escape($descriptor['execution_action_sha256']).','.db_escape($descriptor['activation_sha256']).','.$actor.','.db_escape($now).')';
        if(!db_query($sql,'could not append PAY-CTRY-002 activation execution')){$error='activation_insert_failed';break;}$activation_id=pay_ctry_002_activation_insert_id();if($activation_id<=0){$error='activation_identity_missing';break;}
        $binding=array('activation_id'=>$activation_id,'predecessor_binding_id'=>is_array($leaf)?(int)$leaf['binding_id']:null,'site_installation_id'=>(int)$row['site_installation_id'],'company_id'=>$company,'scope_type'=>(string)$row['scope_type'],'legal_entity_id'=>empty($row['legal_entity_id'])?null:(int)$row['legal_entity_id'],'payroll_scope_key'=>empty($row['payroll_scope_key'])?null:(string)$row['payroll_scope_key'],'action_type'=>(string)$row['action_type'],'effective_from'=>(string)$row['effective_from'],'effective_to'=>empty($row['effective_to'])?null:(string)$row['effective_to'],'activation_sha256'=>$descriptor['activation_sha256'],'bound_by'=>$actor);
        $binding['binding_sha256']=pay_ctry_002_hash($binding);if(!pay_ctry_002_validate_binding_descriptor($binding,$pred_action,$error))break;
        $bsql='INSERT INTO '.TB_PREF.'pay_ctry_activation_bindings (activation_id,predecessor_binding_id,site_installation_id,company_id,scope_type,legal_entity_id,payroll_scope_key,action_type,effective_from,effective_to,activation_sha256,binding_sha256,bound_by,created_at) VALUES ('.$activation_id.','.pay_ctry_002_activation_nullable_uint_sql($binding['predecessor_binding_id']).','.(int)$binding['site_installation_id'].','.$company.','.db_escape($binding['scope_type']).','.pay_ctry_002_activation_nullable_uint_sql($binding['legal_entity_id']).','.pay_ctry_002_activation_nullable_text_sql($binding['payroll_scope_key']).','.db_escape($binding['action_type']).','.db_escape($binding['effective_from']).','.pay_ctry_002_activation_nullable_text_sql($binding['effective_to']).','.db_escape($binding['activation_sha256']).','.db_escape($binding['binding_sha256']).','.$actor.','.db_escape($now).')';
        if(!db_query($bsql,'could not append PAY-CTRY-002 activation binding')){$error='binding_insert_failed';break;}$binding_id=pay_ctry_002_activation_insert_id();if($binding_id<=0){$error='binding_identity_missing';break;}
        if(!function_exists('append_domain_audit_event')||append_domain_audit_event(array('company_id'=>$company,'legal_entity_id'=>empty($row['legal_entity_id'])?0:(int)$row['legal_entity_id'],'entity_type'=>'pay_ctry_activation','entity_key'=>array('activation_id'=>$activation_id,'binding_id'=>$binding_id),'action'=>'pay.ctry.activation.executed','reason_code'=>'independently_approved_country_pack_activation','before'=>array('predecessor_binding_id'=>$binding['predecessor_binding_id']),'after'=>array('action_type'=>$binding['action_type'],'effective_from'=>$binding['effective_from'],'scope_type'=>$binding['scope_type']),'restricted_diff'=>array('maker_checker_executor_distinct'=>true,'fresh_step_up_consumed'=>true,'impact_open_run_count'=>0,'impact_open_approval_count'=>0,'impact_downstream_intent_count'=>0,'impact_blocking_finding_count'=>0,'runtime_resolver_changed'=>false,'suspension_authority_added'=>false,'outbox_delivery_added'=>false,'historical_payroll_accounting_rows_rewritten'=>0)))===false){$error='activation_audit_failed';break;}
        $ok=true;$out=array('activation_id'=>$activation_id,'binding_id'=>$binding_id,'activation_sha256'=>$descriptor['activation_sha256'],'binding_sha256'=>$binding['binding_sha256'],'status'=>'executed');
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?$out:false;
}
?>
