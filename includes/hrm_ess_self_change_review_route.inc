<?php
/** HRM-ESS-001 checker/executor BOLA-safe read projections for bounded self-change workflow. */
if(!function_exists('hrm_ess_self_change_route_security_headers'))include_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'hrm_ess_self_change_route.inc');

function hrm_ess_self_change_review_positive_id($value)
{
    if(is_int($value))return $value>0?$value:false;
    if(!is_string($value)||preg_match('/^[1-9][0-9]{0,18}$/D',$value)!==1)return false;
    $v=(int)$value;return $v>0?$v:false;
}

function hrm_ess_self_change_review_checker_authorized()
{
    return function_exists('hrm_ess_self_change_user_can_approve')&&hrm_ess_self_change_user_can_approve();
}
function hrm_ess_self_change_review_executor_authorized()
{
    return function_exists('hrm_ess_self_change_user_can_execute')&&hrm_ess_self_change_user_can_execute();
}

/** Candidate list exposes control metadata and field names only; no ciphertext/plaintext values. */
function hrm_ess_self_change_review_candidates($mode,$limit=50)
{
    $actor=hrm_ess_self_change_actor_id();$limit=(int)$limit;
    if($actor<=0||$limit<1||$limit>100||!in_array($mode,array('checker','executor'),true))return false;
    if($mode==='checker'&&!hrm_ess_self_change_review_checker_authorized())return false;
    if($mode==='executor'&&!hrm_ess_self_change_review_executor_authorized())return false;
    $status=$mode==='checker'?"r.request_status IN ('draft','approved')":"r.request_status='approved'";
    $separation=$mode==='executor'?' AND r.approved_by IS NOT NULL AND r.approved_by<>'.$actor:'';
    $q=db_query('SELECT r.request_id,r.request_status,r.requested_by,r.approved_by,r.created_at,r.approved_at,r.expires_at,r.row_version,r.user_id,r.worker_id,r.request_nonce_hash,r.payload_ciphertext,r.payload_key_id,r.payload_digest '
        .'FROM '.TB_PREF.'hrm_ess_self_change_requests r WHERE '.$status.' AND r.requested_by<>'.$actor.$separation
        .' AND r.expires_at>UTC_TIMESTAMP() ORDER BY r.request_id ASC LIMIT '.$limit,'could not read ESS self-change review candidates');
    if(!$q)return false;$out=array();
    while($row=db_fetch_assoc($q)){
        $payload=hrm_ess_self_change_open_payload($row);
        if(!is_array($payload))return false;$fields=array_keys($payload);sort($fields);
        $out[]=array('request_id'=>(int)$row['request_id'],'request_status'=>(string)$row['request_status'],
            'requested_by'=>(int)$row['requested_by'],'approved_by'=>$row['approved_by']===null?null:(int)$row['approved_by'],
            'created_at'=>(string)$row['created_at'],'approved_at'=>$row['approved_at']===null?null:(string)$row['approved_at'],
            'expires_at'=>(string)$row['expires_at'],'row_version'=>(int)$row['row_version'],'field_names'=>$fields);
    }
    return $out;
}

/** Resolve one selected request only after role, object-state and maker/checker/executor separation checks. */
function hrm_ess_self_change_review_detail($mode,$request_id)
{
    $request_id=hrm_ess_self_change_review_positive_id($request_id);$actor=hrm_ess_self_change_actor_id();
    if($request_id===false||$actor<=0||!in_array($mode,array('checker','executor'),true))return false;
    if($mode==='checker'&&!hrm_ess_self_change_review_checker_authorized())return false;
    if($mode==='executor'&&!hrm_ess_self_change_review_executor_authorized())return false;
    $request=get_hrm_ess_self_change_request($request_id,false);if(!is_array($request)||(int)$request['requested_by']===$actor)return false;
    if($mode==='checker'&&!in_array((string)$request['request_status'],array('draft','approved'),true))return false;
    if($mode==='executor'&&((string)$request['request_status']!=='approved'||(int)$request['approved_by']<=0||(int)$request['approved_by']===$actor))return false;
    $payload=hrm_ess_self_change_open_payload($request);if(!is_array($payload))return false;
    $values=array();foreach(hrm_ess_self_change_allowed_fields() as $field)if(array_key_exists($field,$payload))$values[$field]=(string)$payload[$field];
    return array('request_id'=>(int)$request['request_id'],'request_status'=>(string)$request['request_status'],
        'requested_by'=>(int)$request['requested_by'],'approved_by'=>$request['approved_by']===null?null:(int)$request['approved_by'],
        'created_at'=>(string)$request['created_at'],'approved_at'=>$request['approved_at']===null?null:(string)$request['approved_at'],
        'expires_at'=>(string)$request['expires_at'],'row_version'=>(int)$request['row_version'],'values'=>$values,
        'request_digest'=>(string)$request['request_digest']);
}

function hrm_ess_self_change_review_approval_digest($request_id)
{
    $request_id=hrm_ess_self_change_review_positive_id($request_id);$actor=hrm_ess_self_change_actor_id();if($request_id===false||$actor<=0)return false;
    $request=get_hrm_ess_self_change_request($request_id,false);if(!is_array($request)||!hrm_ess_self_change_digest_is_valid((string)$request['request_digest']))return false;
    return hash('sha256',implode("\0",array('hrm-ess-self-change-approval-v1',(string)$request['request_digest'],$actor)));
}

function hrm_ess_self_change_review_field_labels()
{
    return array('address'=>_('Address'),'city'=>_('City'),'state'=>_('State/Province'),'country'=>_('Country'),
        'phone'=>_('Phone'),'mobile'=>_('Mobile'),'email'=>_('Work email'),'personal_email'=>_('Personal email'),
        'emergency_name'=>_('Emergency contact name'),'emergency_relation'=>_('Emergency contact relation'),'emergency_phone'=>_('Emergency contact phone'));
}

function hrm_ess_self_change_review_render_values($detail)
{
    if(!is_array($detail)||!isset($detail['values'])||!is_array($detail['values']))return false;$labels=hrm_ess_self_change_review_field_labels();
    echo '<table class="tablestyle2" role="table" aria-label="'.htmlspecialchars(_('Requested bounded profile changes'),ENT_QUOTES,'UTF-8').'">';
    echo '<tr><th scope="col">'.htmlspecialchars(_('Field'),ENT_QUOTES,'UTF-8').'</th><th scope="col">'.htmlspecialchars(_('Requested value'),ENT_QUOTES,'UTF-8').'</th></tr>';
    foreach($detail['values'] as $field=>$value){$label=$labels[$field]??$field;echo '<tr><th scope="row">'.htmlspecialchars($label,ENT_QUOTES,'UTF-8').'</th><td>'.htmlspecialchars((string)$value,ENT_QUOTES,'UTF-8').'</td></tr>';}
    echo '</table>';return true;
}
