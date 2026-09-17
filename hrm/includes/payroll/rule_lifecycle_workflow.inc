<?php
/**
 * PAY-RULE-004 independent review, publication and terminal lifecycle workflow.
 *
 * Governance evidence only. This module is intentionally absent from
 * payroll_engine.inc; PAY-RULE-006 alone owns production routing/cutover.
 */
include_once(__DIR__.'/rule_lifecycle_store.inc');

if (!defined('HRM_PAY_RULE_004_REVIEW_WORKFLOW_VERSION'))
    define('HRM_PAY_RULE_004_REVIEW_WORKFLOW_VERSION', '1.0.490');
if (!defined('HRM_PAY_RULE_004_PUBLICATION_WORKFLOW_VERSION'))
    define('HRM_PAY_RULE_004_PUBLICATION_WORKFLOW_VERSION', '1.0.491');
if (!defined('HRM_PAY_RULE_004_TERMINAL_LIFECYCLE_VERSION'))
    define('HRM_PAY_RULE_004_TERMINAL_LIFECYCLE_VERSION', '1.0.493');
if (!defined('HRM_PAY_RULE_004_EMERGENCY_GOVERNANCE_VERSION'))
    define('HRM_PAY_RULE_004_EMERGENCY_GOVERNANCE_VERSION', '1.0.494');
if (!defined('HRM_PAY_RULE_004_EMERGENCY_PUBLICATION_ALLOWED'))
    define('HRM_PAY_RULE_004_EMERGENCY_PUBLICATION_ALLOWED', false);

function hrm_pay_rule_004_review_binding($version,$decision,$reason_token,$reviewer_id,&$error=null)
{
    $error=null;
    if(!is_array($version)||!isset($version['rule_version_id'],$version['author_id'],$version['artifact_sha256'],$version['plan_sha256'],$version['evidence_sha256'])){$error='review_version_invalid';return false;}
    if(!in_array($decision,array('approved','rejected'),true)){$error='review_decision_invalid';return false;}
    $reason=hrm_pay_rule_004_ascii_token($reason_token,96);if($reason===false){$error='review_reason_invalid';return false;}
    $reviewer_id=(int)$reviewer_id;if($reviewer_id<=0){$error='reviewer_invalid';return false;}
    if($reviewer_id===(int)$version['author_id']){$error='review_sod_violation';return false;}
    $hash=hrm_pay_rule_004_digest(array('rule_version_id'=>(int)$version['rule_version_id'],'artifact_sha256'=>(string)$version['artifact_sha256'],
        'plan_sha256'=>(string)$version['plan_sha256'],'regulatory_evidence_sha256'=>(string)$version['evidence_sha256'],
        'author_id'=>(int)$version['author_id'],'reviewer_id'=>$reviewer_id,'decision'=>$decision,'reason_token'=>$reason));
    if($hash===false){$error='review_hash_failed';return false;}
    return array('decision'=>$decision,'reason_token'=>$reason,'reviewer_id'=>$reviewer_id,'review_sha256'=>$hash);
}

function hrm_pay_rule_004_review_version($rule_version_id,$decision,$reason_token,$actor_id=null,$manage_transaction=true)
{
    global $path_to_root;
    if(!function_exists('append_domain_audit_event'))include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if($actor_id===null)$actor_id=hrm_pay_rule_004_store_actor_id();
    if((int)$actor_id<=0||!hrm_pay_rule_004_store_user_can('SA_HRM_REVIEW_PAY_RULE'))return false;
    if($manage_transaction)begin_transaction();$ok=true;$return_id=false;
    $version=hrm_pay_rule_004_read_version_internal((int)$rule_version_id,true,$error);if($version===false)$ok=false;
    if($ok){$lock=db_query('SELECT rule_id FROM '.TB_PREF.'hrm_pay_rule_rules WHERE rule_id='.(int)$version['rule_id'].' LIMIT 1 FOR UPDATE');if(!$lock||db_num_rows($lock)!==1)$ok=false;}
    $binding=false;if($ok){$binding=hrm_pay_rule_004_review_binding($version,$decision,$reason_token,(int)$actor_id,$error);if($binding===false)$ok=false;}
    if($ok){
        $q=db_query('SELECT lifecycle_review_id,review_decision,reason_token,reviewer_id,review_sha256 FROM '.TB_PREF.'hrm_pay_rule_lifecycle_reviews WHERE rule_version_id='.(int)$rule_version_id.' ORDER BY lifecycle_review_id FOR UPDATE');
        if(!$q)$ok=false;else if(db_num_rows($q)>1)$ok=false;else if(db_num_rows($q)===1){$r=db_fetch_assoc($q);if((string)$r['review_decision']===$binding['decision']&&(string)$r['reason_token']===$binding['reason_token']&&(int)$r['reviewer_id']===$binding['reviewer_id']&&(string)$r['review_sha256']===$binding['review_sha256'])$return_id=(int)$r['lifecycle_review_id'];else$ok=false;}
    }
    if($ok&&$return_id===false){$q=db_query('SELECT publication_id FROM '.TB_PREF.'hrm_pay_rule_lifecycle_publications WHERE rule_version_id='.(int)$rule_version_id.' LIMIT 1');if(!$q||db_num_rows($q)>0)$ok=false;}
    if($ok&&$return_id===false){
        $ok=db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_lifecycle_reviews (rule_version_id,review_decision,reason_token,reviewer_id,review_sha256) VALUES ('.
            (int)$rule_version_id.','.db_escape($binding['decision']).','.db_escape($binding['reason_token']).','.(int)$binding['reviewer_id'].','.db_escape($binding['review_sha256']).')','could not append PAY-RULE-004 lifecycle review')!==false;
        if($ok){$return_id=(int)db_insert_id();if($return_id<=0)$ok=false;}
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
            'entity_type'=>'pay_rule_004_lifecycle_review','entity_key'=>array('rule_version_id'=>(int)$rule_version_id,'lifecycle_review_id'=>$return_id),
            'action'=>'pay.rule_004.review_recorded','reason_code'=>$binding['reason_token'],'before'=>array('state'=>'draft'),
            'after'=>array('review_decision'=>$binding['decision']),'restricted_diff'=>array('review_sha256'=>$binding['review_sha256'],'author_id'=>(int)$version['author_id'],'reviewer_id'=>(int)$binding['reviewer_id'],'production_activation_allowed'=>false)));
            if($audit===false)$ok=false;}
    }
    if($manage_transaction){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return_id:false;
}

function hrm_pay_rule_004_interval_readiness($effective_from,$effective_to,$active_intervals,&$error=null)
{
    $error=null;if(hrm_pay_rule_004_date($effective_from)===false||hrm_pay_rule_004_date($effective_to)===false||strcmp($effective_from,$effective_to)>0){$error='publication_interval_invalid';return false;}
    if(!is_array($active_intervals)){$error='active_intervals_invalid';return false;}
    $nearest_before=null;$nearest_after=null;
    foreach($active_intervals as $row){
        if(!is_array($row)||!isset($row['effective_from'],$row['effective_to'])){$error='active_interval_invalid';return false;}
        $from=(string)$row['effective_from'];$to=(string)$row['effective_to'];$overlap=hrm_pay_rule_004_effective_intervals_overlap($effective_from,$effective_to,$from,$to);
        if($overlap===null){$error='active_interval_invalid';return false;}if($overlap){$error='effective_interval_collision';return false;}
        if(strcmp($to,$effective_from)<0&&($nearest_before===null||strcmp($to,$nearest_before)>0))$nearest_before=$to;
        if(strcmp($from,$effective_to)>0&&($nearest_after===null||strcmp($from,$nearest_after)<0))$nearest_after=$from;
    }
    $gap_before=false;$gap_after=false;
    if($nearest_before!==null){$next=gmdate('Y-m-d',strtotime($nearest_before.' +1 day'));$gap_before=$next!==$effective_from;}
    if($nearest_after!==null){$next=gmdate('Y-m-d',strtotime($effective_to.' +1 day'));$gap_after=$next!==$nearest_after;}
    return array('collision'=>false,'gap_before'=>$gap_before,'gap_after'=>$gap_after,'nearest_before'=>$nearest_before,'nearest_after'=>$nearest_after);
}

function hrm_pay_rule_004_publication_binding($version,$review,$preview,$publisher_id,$active_intervals,$emergency=null,&$error=null)
{
    $error=null;if(!is_array($version)||!is_array($review)){$error='publication_evidence_invalid';return false;}
    foreach(array('rule_version_id','author_id','artifact_sha256','plan_sha256','effective_from','effective_to','jurisdiction_token','evidence_sha256') as $k)if(!array_key_exists($k,$version)){$error='publication_version_missing_'.$k;return false;}
    foreach(array('lifecycle_review_id','review_decision','reviewer_id','review_sha256') as $k)if(!array_key_exists($k,$review)){$error='publication_review_missing_'.$k;return false;}
    if((string)$review['review_decision']!=='approved'){$error='publication_review_not_approved';return false;}
    if(!hrm_pay_rule_004_validate_publication_roles((int)$version['author_id'],(int)$review['reviewer_id'],(int)$publisher_id,$error))return false;
    if(!hrm_pay_rule_004_validate_impact_preview($preview,(string)$version['artifact_sha256'],(string)$version['plan_sha256'],$error))return false;
    $emergency_hash=null;$emergency_requester=null;
    if($emergency!==null&&$emergency!==false){
        foreach(array('requester_id','declared_at','emergency_sha256') as $k)if(!array_key_exists($k,$emergency)){$error='emergency_evidence_missing_'.$k;return false;}
        $emergency_requester=(int)$emergency['requester_id'];
        if($emergency_requester<=0||$emergency_requester===(int)$version['author_id']||$emergency_requester===(int)$review['reviewer_id']||$emergency_requester===(int)$publisher_id){$error='emergency_sod_violation';return false;}
        if(hrm_pay_rule_004_utc((string)$emergency['declared_at'])===false||hrm_pay_rule_004_sha256((string)$emergency['emergency_sha256'])===false){$error='emergency_evidence_invalid';return false;}
        if(strcmp((string)$preview['generated_at'],(string)$emergency['declared_at'])<0){$error='emergency_impact_preview_stale';return false;}
        $emergency_hash=(string)$emergency['emergency_sha256'];
    }
    $readiness=hrm_pay_rule_004_interval_readiness((string)$version['effective_from'],(string)$version['effective_to'],$active_intervals,$error);if($readiness===false)return false;
    $hash_parts=array('rule_version_id'=>(int)$version['rule_version_id'],'lifecycle_review_id'=>(int)$review['lifecycle_review_id'],
        'review_sha256'=>(string)$review['review_sha256'],'regulatory_evidence_sha256'=>(string)$version['evidence_sha256'],'jurisdiction_token'=>(string)$version['jurisdiction_token'],
        'effective_from'=>(string)$version['effective_from'],'effective_to'=>(string)$version['effective_to'],'artifact_sha256'=>(string)$version['artifact_sha256'],'plan_sha256'=>(string)$version['plan_sha256'],
        'impact_scope_token'=>(string)$preview['scope_token'],'impact_preview_sha256'=>(string)$preview['preview_sha256'],'impact_generated_at'=>(string)$preview['generated_at'],
        'author_id'=>(int)$version['author_id'],'reviewer_id'=>(int)$review['reviewer_id'],'publisher_id'=>(int)$publisher_id,'emergency'=>false);
    if($emergency_hash!==null){$hash_parts['emergency']=true;$hash_parts['emergency_change_sha256']=$emergency_hash;$hash_parts['emergency_requester_id']=$emergency_requester;}
    $hash=hrm_pay_rule_004_digest($hash_parts);
    if($hash===false){$error='publication_hash_failed';return false;}
    return array('publisher_id'=>(int)$publisher_id,'impact_scope_token'=>(string)$preview['scope_token'],'impact_preview_sha256'=>(string)$preview['preview_sha256'],
        'publication_sha256'=>$hash,'readiness'=>$readiness,'emergency_change_sha256'=>$emergency_hash,'emergency_requester_id'=>$emergency_requester);
}

function hrm_pay_rule_004_events_table_exists()
{
    $physical=TB_PREF.'hrm_pay_rule_lifecycle_events';
    $q=db_query('SELECT COUNT(*) n FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));$r=$q?db_fetch_assoc($q):false;return $r&&(int)$r['n']===1;
}

function hrm_pay_rule_004_publication_terminal_event($publication_id)
{
    if(!hrm_pay_rule_004_events_table_exists())return false;
    $q=db_query('SELECT event_id,event_type,successor_rule_version_id,event_sha256,event_at,actor_id,reason_token FROM '.TB_PREF.'hrm_pay_rule_lifecycle_events WHERE publication_id='.(int)$publication_id.' ORDER BY event_id LIMIT 2');
    if(!$q||db_num_rows($q)!==1)return false;return db_fetch_assoc($q);
}

function hrm_pay_rule_004_active_intervals_for_rule($rule_id,$exclude_rule_version_id=0)
{
    $q=db_query('SELECT p.publication_id,v.rule_version_id,v.effective_from,v.effective_to FROM '.TB_PREF.'hrm_pay_rule_lifecycle_publications p JOIN '.TB_PREF.'hrm_pay_rule_rule_versions v ON v.rule_version_id=p.rule_version_id WHERE v.rule_id='.(int)$rule_id.' AND v.rule_version_id<>'.(int)$exclude_rule_version_id.' ORDER BY v.effective_from,v.rule_version_id');
    if(!$q)return false;$rows=array();while($r=db_fetch_assoc($q)){if(hrm_pay_rule_004_publication_terminal_event((int)$r['publication_id'])!==false)continue;$rows[]=array('rule_version_id'=>(int)$r['rule_version_id'],'effective_from'=>(string)$r['effective_from'],'effective_to'=>(string)$r['effective_to']);}return $rows;
}

function hrm_pay_rule_004_publish_version($rule_version_id,$lifecycle_review_id,$preview,$actor_id=null,$manage_transaction=true)
{
    global $path_to_root;if(!function_exists('append_domain_audit_event'))include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if($actor_id===null)$actor_id=hrm_pay_rule_004_store_actor_id();if((int)$actor_id<=0||!hrm_pay_rule_004_store_user_can('SA_HRM_PUBLISH_PAY_RULE'))return false;
    if(HRM_PAY_RULE_004_PRODUCTION_ACTIVATION_ALLOWED!==false||HRM_PAY_RULE_004_EMERGENCY_PUBLICATION_ALLOWED!==false)return false;
    if($manage_transaction)begin_transaction();$ok=true;$return_id=false;$version=hrm_pay_rule_004_read_version_internal((int)$rule_version_id,true,$error);if($version===false)$ok=false;
    if($ok){$lock=db_query('SELECT rule_id FROM '.TB_PREF.'hrm_pay_rule_rules WHERE rule_id='.(int)$version['rule_id'].' LIMIT 1 FOR UPDATE');if(!$lock||db_num_rows($lock)!==1)$ok=false;}
    $review=false;if($ok){$q=db_query('SELECT lifecycle_review_id,rule_version_id,review_decision,reason_token,reviewer_id,review_sha256 FROM '.TB_PREF.'hrm_pay_rule_lifecycle_reviews WHERE lifecycle_review_id='.(int)$lifecycle_review_id.' AND rule_version_id='.(int)$rule_version_id.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)!==1)$ok=false;else$review=db_fetch_assoc($q);}
    $emergency=null;if($ok&&hrm_pay_rule_004_emergency_table_exists()){$emergency=hrm_pay_rule_004_read_emergency_change_internal((int)$rule_version_id,true,$emergency_error);if($emergency===false)$ok=false;}
    $binding=false;
    if($ok){
        $q=db_query('SELECT publication_id,lifecycle_review_id,publisher_id,impact_scope_token,impact_preview_sha256,publication_sha256 FROM '.TB_PREF.'hrm_pay_rule_lifecycle_publications WHERE rule_version_id='.(int)$rule_version_id.' LIMIT 2 FOR UPDATE');
        if(!$q||db_num_rows($q)>1)$ok=false;
        elseif(db_num_rows($q)===1){
            $binding=hrm_pay_rule_004_publication_binding($version,$review,$preview,(int)$actor_id,array(),$emergency,$error);
            if($binding===false)$ok=false;else{$r=db_fetch_assoc($q);if((int)$r['lifecycle_review_id']===(int)$lifecycle_review_id&&(int)$r['publisher_id']===(int)$binding['publisher_id']&&(string)$r['impact_scope_token']===$binding['impact_scope_token']&&(string)$r['impact_preview_sha256']===$binding['impact_preview_sha256']&&(string)$r['publication_sha256']===$binding['publication_sha256'])$return_id=(int)$r['publication_id'];else$ok=false;}
        }
    }
    if($ok&&$return_id===false){$intervals=hrm_pay_rule_004_active_intervals_for_rule((int)$version['rule_id'],(int)$rule_version_id);if($intervals===false)$ok=false;else{$binding=hrm_pay_rule_004_publication_binding($version,$review,$preview,(int)$actor_id,$intervals,$emergency,$error);if($binding===false)$ok=false;}}
    if($ok&&$return_id===false){$ok=db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_lifecycle_publications (rule_version_id,lifecycle_review_id,publisher_id,impact_scope_token,impact_preview_sha256,publication_sha256) VALUES ('.
        (int)$rule_version_id.','.(int)$lifecycle_review_id.','.(int)$binding['publisher_id'].','.db_escape($binding['impact_scope_token']).','.db_escape($binding['impact_preview_sha256']).','.db_escape($binding['publication_sha256']).')','could not append PAY-RULE-004 publication evidence')!==false;
        if($ok){$return_id=(int)db_insert_id();if($return_id<=0)$ok=false;}
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,'entity_type'=>'pay_rule_004_publication',
            'entity_key'=>array('rule_version_id'=>(int)$rule_version_id,'publication_id'=>$return_id),'action'=>'pay.rule_004.published','reason_code'=>'pay_rule_004_publication',
            'before'=>array('state'=>'draft'),'after'=>array('state'=>'published'),'restricted_diff'=>array('review_sha256'=>(string)$review['review_sha256'],'publication_sha256'=>$binding['publication_sha256'],
                'impact_preview_sha256'=>$binding['impact_preview_sha256'],'gap_before'=>$binding['readiness']['gap_before'],'gap_after'=>$binding['readiness']['gap_after'],'emergency_change_sha256'=>$binding['emergency_change_sha256'],'production_activation_allowed'=>false)));
            if($audit===false)$ok=false;}
    }
    if($manage_transaction){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return_id:false;
}

function hrm_pay_rule_004_lifecycle_event_binding($publication,$version,$event_type,$successor_rule_version_id,$reason_token,$actor_id,$event_at,&$error=null)
{
    $error=null;if(!in_array($event_type,array('withdrawn','superseded'),true)){$error='lifecycle_event_type_invalid';return false;}
    $reason=hrm_pay_rule_004_ascii_token($reason_token,96);if($reason===false){$error='lifecycle_reason_invalid';return false;}
    if(hrm_pay_rule_004_utc($event_at)===false){$error='lifecycle_event_at_invalid';return false;}
    if((int)$actor_id<=0){$error='lifecycle_actor_invalid';return false;}
    if(!is_array($publication)||!isset($publication['publication_id'],$publication['publication_sha256'])||!is_array($version)||!isset($version['rule_version_id'],$version['rule_id'])){$error='lifecycle_evidence_invalid';return false;}
    if($event_type==='withdrawn'&&$successor_rule_version_id!==null){$error='withdrawal_successor_forbidden';return false;}
    if($event_type==='superseded'&&(!is_int($successor_rule_version_id)||$successor_rule_version_id<=0||(int)$successor_rule_version_id===(int)$version['rule_version_id'])){$error='supersession_successor_invalid';return false;}
    $hash=hrm_pay_rule_004_digest(array('publication_id'=>(int)$publication['publication_id'],'publication_sha256'=>(string)$publication['publication_sha256'],
        'rule_version_id'=>(int)$version['rule_version_id'],'event_type'=>$event_type,'successor_rule_version_id'=>$successor_rule_version_id,'reason_token'=>$reason,'actor_id'=>(int)$actor_id,'event_at'=>$event_at));
    if($hash===false){$error='lifecycle_event_hash_failed';return false;}
    return array('event_type'=>$event_type,'successor_rule_version_id'=>$successor_rule_version_id,'reason_token'=>$reason,'actor_id'=>(int)$actor_id,'event_at'=>$event_at,'event_sha256'=>$hash);
}

function hrm_pay_rule_004_terminal_transition($rule_version_id,$event_type,$successor_rule_version_id,$reason_token,$event_at,$actor_id=null,$manage_transaction=true)
{
    global $path_to_root;if(!function_exists('append_domain_audit_event'))include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if($actor_id===null)$actor_id=hrm_pay_rule_004_store_actor_id();if((int)$actor_id<=0||!hrm_pay_rule_004_store_user_can('SA_HRM_WITHDRAW_PAY_RULE'))return false;
    if(!hrm_pay_rule_004_events_table_exists())return false;if($manage_transaction)begin_transaction();$ok=true;$return_id=false;
    $version=hrm_pay_rule_004_read_version_internal((int)$rule_version_id,true,$error);if($version===false)$ok=false;
    if($ok){$lock=db_query('SELECT rule_id FROM '.TB_PREF.'hrm_pay_rule_rules WHERE rule_id='.(int)$version['rule_id'].' LIMIT 1 FOR UPDATE');if(!$lock||db_num_rows($lock)!==1)$ok=false;}
    $publication=false;if($ok){$q=db_query('SELECT publication_id,rule_version_id,lifecycle_review_id,publisher_id,impact_scope_token,impact_preview_sha256,publication_sha256 FROM '.TB_PREF.'hrm_pay_rule_lifecycle_publications WHERE rule_version_id='.(int)$rule_version_id.' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)!==1)$ok=false;else$publication=db_fetch_assoc($q);}
    $binding=false;if($ok){$binding=hrm_pay_rule_004_lifecycle_event_binding($publication,$version,$event_type,$successor_rule_version_id,$reason_token,(int)$actor_id,$event_at,$error);if($binding===false)$ok=false;}
    if($ok){$q=db_query('SELECT event_id,event_type,successor_rule_version_id,actor_id,reason_token,DATE_FORMAT(event_at,\'%Y-%m-%dT%H:%i:%sZ\') event_at,event_sha256 FROM '.TB_PREF.'hrm_pay_rule_lifecycle_events WHERE publication_id='.(int)$publication['publication_id'].' LIMIT 2 FOR UPDATE');if(!$q||db_num_rows($q)>1)$ok=false;elseif(db_num_rows($q)===1){$r=db_fetch_assoc($q);$succ=$r['successor_rule_version_id']===null?null:(int)$r['successor_rule_version_id'];if((string)$r['event_type']===$binding['event_type']&&$succ===$binding['successor_rule_version_id']&&(int)$r['actor_id']===$binding['actor_id']&&(string)$r['reason_token']===$binding['reason_token']&&(string)$r['event_at']===$binding['event_at']&&(string)$r['event_sha256']===$binding['event_sha256'])$return_id=(int)$r['event_id'];else$ok=false;}}
    if($ok&&$return_id===false&&$event_type==='superseded'){
        $q=db_query('SELECT v.rule_version_id,v.rule_id,p.publication_id FROM '.TB_PREF.'hrm_pay_rule_rule_versions v JOIN '.TB_PREF.'hrm_pay_rule_lifecycle_publications p ON p.rule_version_id=v.rule_version_id WHERE v.rule_version_id='.(int)$successor_rule_version_id.' LIMIT 2 FOR UPDATE');
        if(!$q||db_num_rows($q)!==1)$ok=false;else{$s=db_fetch_assoc($q);if((int)$s['rule_id']!==(int)$version['rule_id']||hrm_pay_rule_004_publication_terminal_event((int)$s['publication_id'])!==false)$ok=false;}
    }
    if($ok&&$return_id===false){$db_event_at=str_replace('T',' ',rtrim($binding['event_at'],'Z'));$ok=db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_lifecycle_events (rule_version_id,publication_id,event_type,successor_rule_version_id,actor_id,reason_token,event_at,event_sha256) VALUES ('.
        (int)$rule_version_id.','.(int)$publication['publication_id'].','.db_escape($binding['event_type']).','.($binding['successor_rule_version_id']===null?'NULL':(int)$binding['successor_rule_version_id']).','.(int)$binding['actor_id'].','.db_escape($binding['reason_token']).','.db_escape($db_event_at).','.db_escape($binding['event_sha256']).')','could not append PAY-RULE-004 terminal lifecycle event')!==false;
        if($ok){$return_id=(int)db_insert_id();if($return_id<=0)$ok=false;}
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,'entity_type'=>'pay_rule_004_lifecycle_event',
            'entity_key'=>array('rule_version_id'=>(int)$rule_version_id,'event_id'=>$return_id),'action'=>'pay.rule_004.'.$binding['event_type'],'reason_code'=>$binding['reason_token'],
            'before'=>array('state'=>'published'),'after'=>array('state'=>$binding['event_type'],'successor_rule_version_id'=>$binding['successor_rule_version_id']),
            'restricted_diff'=>array('publication_sha256'=>(string)$publication['publication_sha256'],'event_sha256'=>$binding['event_sha256'],'production_activation_allowed'=>false)));
            if($audit===false)$ok=false;}
    }
    if($manage_transaction){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return_id:false;
}

function hrm_pay_rule_004_withdraw_version($rule_version_id,$reason_token,$event_at,$actor_id=null,$manage_transaction=true)
{
    return hrm_pay_rule_004_terminal_transition($rule_version_id,'withdrawn',null,$reason_token,$event_at,$actor_id,$manage_transaction);
}

function hrm_pay_rule_004_supersede_version($rule_version_id,$successor_rule_version_id,$reason_token,$event_at,$actor_id=null,$manage_transaction=true)
{
    return hrm_pay_rule_004_terminal_transition($rule_version_id,'superseded',$successor_rule_version_id,$reason_token,$event_at,$actor_id,$manage_transaction);
}

function hrm_pay_rule_004_emergency_table_exists()
{
    $physical=TB_PREF.'hrm_pay_rule_emergency_changes';
    $q=db_query('SELECT COUNT(*) n FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='.db_escape($physical));$r=$q?db_fetch_assoc($q):false;return $r&&(int)$r['n']===1;
}

function hrm_pay_rule_004_read_emergency_change_internal($rule_version_id,$for_update=false,&$error=null)
{
    $error=null;if(!hrm_pay_rule_004_emergency_table_exists())return null;
    $q=db_query('SELECT emergency_change_id,rule_version_id,requester_id,incident_reference,reason_token,DATE_FORMAT(declared_at,\'%Y-%m-%dT%H:%i:%sZ\') declared_at,emergency_sha256 FROM '.TB_PREF.'hrm_pay_rule_emergency_changes WHERE rule_version_id='.(int)$rule_version_id.' LIMIT 2'.($for_update?' FOR UPDATE':''));
    if(!$q||db_num_rows($q)>1){$error='emergency_evidence_ambiguous';return false;}if(db_num_rows($q)===0)return null;return db_fetch_assoc($q);
}

function hrm_pay_rule_004_emergency_binding($version,$incident_reference,$reason_token,$requester_id,$declared_at,&$error=null)
{
    $error=null;if(!is_array($version)||!isset($version['rule_version_id'],$version['author_id'],$version['artifact_sha256'],$version['plan_sha256'],$version['evidence_sha256'])){$error='emergency_version_invalid';return false;}
    $incident=hrm_pay_rule_004_ascii_token($incident_reference,128);$reason=hrm_pay_rule_004_ascii_token($reason_token,96);$declared=hrm_pay_rule_004_utc($declared_at);$requester_id=(int)$requester_id;
    if($incident===false){$error='emergency_incident_reference_invalid';return false;}if($reason===false){$error='emergency_reason_invalid';return false;}if($declared===false){$error='emergency_declared_at_invalid';return false;}
    if($requester_id<=0||$requester_id===(int)$version['author_id']){$error='emergency_requester_sod_violation';return false;}
    $hash=hrm_pay_rule_004_digest(array('rule_version_id'=>(int)$version['rule_version_id'],'artifact_sha256'=>(string)$version['artifact_sha256'],'plan_sha256'=>(string)$version['plan_sha256'],
        'regulatory_evidence_sha256'=>(string)$version['evidence_sha256'],'author_id'=>(int)$version['author_id'],'requester_id'=>$requester_id,'incident_reference'=>$incident,'reason_token'=>$reason,'declared_at'=>$declared));
    if($hash===false){$error='emergency_hash_failed';return false;}
    return array('requester_id'=>$requester_id,'incident_reference'=>$incident,'reason_token'=>$reason,'declared_at'=>$declared,'emergency_sha256'=>$hash);
}

function hrm_pay_rule_004_declare_emergency_change($rule_version_id,$incident_reference,$reason_token,$declared_at,$actor_id=null,$manage_transaction=true)
{
    global $path_to_root;if(!function_exists('append_domain_audit_event'))include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if($actor_id===null)$actor_id=hrm_pay_rule_004_store_actor_id();if((int)$actor_id<=0||!hrm_pay_rule_004_store_user_can('SA_HRM_DECLARE_PAY_RULE_EMERGENCY'))return false;
    if(HRM_PAY_RULE_004_EMERGENCY_PUBLICATION_ALLOWED!==false||!hrm_pay_rule_004_emergency_table_exists())return false;if($manage_transaction)begin_transaction();$ok=true;$return_id=false;
    $version=hrm_pay_rule_004_read_version_internal((int)$rule_version_id,true,$error);if($version===false)$ok=false;
    if($ok){$lock=db_query('SELECT rule_id FROM '.TB_PREF.'hrm_pay_rule_rules WHERE rule_id='.(int)$version['rule_id'].' LIMIT 1 FOR UPDATE');if(!$lock||db_num_rows($lock)!==1)$ok=false;}
    $binding=false;if($ok){$binding=hrm_pay_rule_004_emergency_binding($version,$incident_reference,$reason_token,(int)$actor_id,$declared_at,$error);if($binding===false)$ok=false;}
    if($ok){$existing=hrm_pay_rule_004_read_emergency_change_internal((int)$rule_version_id,true,$error);if($existing===false)$ok=false;elseif(is_array($existing)){if((int)$existing['requester_id']===$binding['requester_id']&&(string)$existing['incident_reference']===$binding['incident_reference']&&(string)$existing['reason_token']===$binding['reason_token']&&(string)$existing['declared_at']===$binding['declared_at']&&(string)$existing['emergency_sha256']===$binding['emergency_sha256'])$return_id=(int)$existing['emergency_change_id'];else$ok=false;}}
    if($ok&&$return_id===false){$q=db_query('SELECT lifecycle_review_id FROM '.TB_PREF.'hrm_pay_rule_lifecycle_reviews WHERE rule_version_id='.(int)$rule_version_id.' LIMIT 1 FOR UPDATE');if(!$q||db_num_rows($q)>0)$ok=false;}
    if($ok&&$return_id===false){$q=db_query('SELECT publication_id FROM '.TB_PREF.'hrm_pay_rule_lifecycle_publications WHERE rule_version_id='.(int)$rule_version_id.' LIMIT 1 FOR UPDATE');if(!$q||db_num_rows($q)>0)$ok=false;}
    if($ok&&$return_id===false){$db_declared=str_replace('T',' ',rtrim($binding['declared_at'],'Z'));$ok=db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_emergency_changes (rule_version_id,requester_id,incident_reference,reason_token,declared_at,emergency_sha256) VALUES ('.
        (int)$rule_version_id.','.(int)$binding['requester_id'].','.db_escape($binding['incident_reference']).','.db_escape($binding['reason_token']).','.db_escape($db_declared).','.db_escape($binding['emergency_sha256']).')','could not append PAY-RULE-004 emergency-change declaration')!==false;
        if($ok){$return_id=(int)db_insert_id();if($return_id<=0)$ok=false;}
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,'entity_type'=>'pay_rule_004_emergency_change',
            'entity_key'=>array('rule_version_id'=>(int)$rule_version_id,'emergency_change_id'=>$return_id),'action'=>'pay.rule_004.emergency_declared','reason_code'=>$binding['reason_token'],
            'before'=>array('state'=>'draft'),'after'=>array('state'=>'draft','emergency_change'=>true),'restricted_diff'=>array('incident_reference'=>$binding['incident_reference'],'emergency_sha256'=>$binding['emergency_sha256'],'impact_preview_must_postdate_declaration'=>true,'emergency_publication_bypass'=>false,'production_activation_allowed'=>false)));
            if($audit===false)$ok=false;}
    }
    if($manage_transaction){if($ok)commit_transaction();else cancel_transaction();}return $ok?$return_id:false;
}

function hrm_pay_rule_004_recovery_policy()
{
    return array('accepted_evidence_mutable'=>false,'exact_retry'=>'verification_only','failed_transaction'=>'rollback_and_retry_same_command',
        'accepted_correction'=>'new_immutable_successor_or_terminal_event','selective_downgrade'=>false,'emergency_publication_allowed'=>false,'emergency_change_requires_normal_publication'=>true,'emergency_impact_preview_must_postdate_declaration'=>true,'production_cutover_authorized'=>false);
}

function hrm_pay_rule_004_state_from_evidence($publication,$terminal_event)
{
    if($publication===false||$publication===null)return 'draft';
    if($terminal_event===false||$terminal_event===null)return 'published';
    if(is_array($terminal_event)&&isset($terminal_event['event_type'])&&in_array($terminal_event['event_type'],array('withdrawn','superseded'),true))return (string)$terminal_event['event_type'];
    return false;
}

function hrm_pay_rule_004_read_lifecycle($rule_version_id,&$error=null)
{
    $error=null;
    if(!hrm_pay_rule_004_store_user_can('SA_HRM_VIEW_PAY_RULE_LIFECYCLE')){$error='unauthorized';return false;}
    $version=hrm_pay_rule_004_read_version_internal((int)$rule_version_id,false,$error);if($version===false)return false;
    $reviews=array();$q=db_query('SELECT lifecycle_review_id,review_decision,reason_token,reviewer_id,review_sha256,reviewed_at FROM '.TB_PREF.'hrm_pay_rule_lifecycle_reviews WHERE rule_version_id='.(int)$rule_version_id.' ORDER BY lifecycle_review_id');if(!$q){$error='review_read_failed';return false;}while($r=db_fetch_assoc($q))$reviews[]=$r;
    $publication=false;$q=db_query('SELECT publication_id,lifecycle_review_id,publisher_id,impact_scope_token,impact_preview_sha256,publication_sha256,published_at FROM '.TB_PREF.'hrm_pay_rule_lifecycle_publications WHERE rule_version_id='.(int)$rule_version_id.' LIMIT 2');if(!$q||db_num_rows($q)>1){$error='publication_read_ambiguous';return false;}if(db_num_rows($q)===1)$publication=db_fetch_assoc($q);
    $emergency=hrm_pay_rule_004_read_emergency_change_internal((int)$rule_version_id,false,$emergency_error);if($emergency===false){$error=$emergency_error;return false;}
    $event=$publication!==false?hrm_pay_rule_004_publication_terminal_event((int)$publication['publication_id']):false;
    $state=hrm_pay_rule_004_state_from_evidence($publication,$event);if($state===false){$error='lifecycle_state_invalid';return false;}
    return array('version'=>$version,'reviews'=>$reviews,'emergency_change'=>$emergency,'publication'=>$publication,'terminal_event'=>$event,'state'=>$state,'production_activation_allowed'=>false);
}
