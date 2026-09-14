<?php
/**
 * HRM-FND-007 durable Work Authorization expiry-delivery requirement custody.
 *
 * This defines approved requirements only. It deliberately does not select
 * recipients, enqueue messages, send email, create acknowledgements, or grant
 * access. Future delivery implementations must consume one exact approved row.
 */
if (!function_exists('hrm_work_authorization_validate_jurisdiction'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_db.inc');

function hrm_work_authorization_delivery_policy_statuses()
{
    return array('draft','approved','retired');
}

function hrm_work_authorization_delivery_requirements()
{
    return array('stateless_only','durable_notice_required','durable_acknowledgement_required');
}

function hrm_work_authorization_delivery_policy_validate_code($value)
{
    return is_string($value)
        && preg_match('/^[a-z0-9][a-z0-9._-]{2,63}$/D', $value) === 1;
}

function hrm_work_authorization_delivery_policy_validate_ref($value, $nullable = false)
{
    if (($value === null || $value === '') && $nullable) return true;
    return is_string($value)
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]{2,127}$/D', $value) === 1;
}

function hrm_work_authorization_delivery_policy_validate_metadata($row)
{
    $allowed=array('policy_code','version_no','predecessor_id','issuing_jurisdiction',
        'delivery_requirement','recipient_rule_ref','channel_rule_ref','recovery_rule_ref',
        'policy_status','effective_from','effective_to','qualified_review_ref','approved_at','approved_by');
    if(!is_array($row)||array_diff(array_keys($row),$allowed))return false;
    $version_no=isset($row['version_no'])?(int)$row['version_no']:0;
    $predecessor_id=isset($row['predecessor_id'])&&$row['predecessor_id']!==null?(int)$row['predecessor_id']:null;
    $requirement=isset($row['delivery_requirement'])?(string)$row['delivery_requirement']:'';
    $status=isset($row['policy_status'])?(string)$row['policy_status']:'';
    $recipient=isset($row['recipient_rule_ref'])?(string)$row['recipient_rule_ref']:'';
    $channel=isset($row['channel_rule_ref'])?(string)$row['channel_rule_ref']:'';
    $recovery=isset($row['recovery_rule_ref'])?(string)$row['recovery_rule_ref']:'';
    $review=isset($row['qualified_review_ref'])?(string)$row['qualified_review_ref']:'';
    $approved_at=isset($row['approved_at'])?$row['approved_at']:null;
    $approved_by=isset($row['approved_by'])&&$row['approved_by']!==null?(int)$row['approved_by']:null;
    if(!hrm_work_authorization_delivery_policy_validate_code(isset($row['policy_code'])?$row['policy_code']:null)
        ||$version_no<=0||($predecessor_id!==null&&$predecessor_id<=0)
        ||!hrm_work_authorization_validate_jurisdiction(isset($row['issuing_jurisdiction'])?$row['issuing_jurisdiction']:null)
        ||!in_array($requirement,hrm_work_authorization_delivery_requirements(),true)
        ||!in_array($status,hrm_work_authorization_delivery_policy_statuses(),true)
        ||!hrm_work_authorization_validate_date(isset($row['effective_from'])?$row['effective_from']:null,false)
        ||!hrm_work_authorization_validate_date(isset($row['effective_to'])?$row['effective_to']:null,true)
        ||(!empty($row['effective_to'])&&$row['effective_to']<$row['effective_from']))return false;
    $durable=$requirement!=='stateless_only';
    if($durable){
        if(!hrm_work_authorization_delivery_policy_validate_ref($recipient)
            ||!hrm_work_authorization_delivery_policy_validate_ref($channel)
            ||!hrm_work_authorization_delivery_policy_validate_ref($recovery))return false;
    }elseif($recipient!==''||$channel!==''||$recovery!=='')return false;
    if($status==='approved'){
        if(!hrm_work_authorization_delivery_policy_validate_ref($review)
            ||!is_string($approved_at)||preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',$approved_at)!==1
            ||(($dt=DateTime::createFromFormat('!Y-m-d H:i:s',$approved_at))===false)
            ||$dt->format('Y-m-d H:i:s')!==$approved_at||$approved_by===null||$approved_by<=0)return false;
    }elseif($approved_at!==null||$approved_by!==null)return false;
    return true;
}

/** Resolve exactly one approved delivery requirement for jurisdiction/date. */
function get_hrm_work_authorization_delivery_policy_as_of($jurisdiction,$as_of)
{
    if(!hrm_work_authorization_validate_jurisdiction($jurisdiction)||!hrm_work_authorization_validate_date($as_of,false))return false;
    $sql='SELECT p.delivery_policy_version_id,p.policy_code,p.version_no,p.predecessor_id,p.issuing_jurisdiction,'
        .'p.delivery_requirement,p.recipient_rule_ref,p.channel_rule_ref,p.recovery_rule_ref,p.policy_status,'
        .'p.effective_from,p.effective_to,p.qualified_review_ref,p.approved_at,p.approved_by '
        .'FROM '.TB_PREF.'hrm_work_authorization_delivery_policy_versions p '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorization_delivery_policy_versions s ON s.predecessor_id=p.delivery_policy_version_id '
        .'WHERE s.delivery_policy_version_id IS NULL AND p.issuing_jurisdiction='.db_escape($jurisdiction)
        ." AND p.policy_status='approved' AND p.effective_from<=".db_escape($as_of)
        .' AND (p.effective_to IS NULL OR p.effective_to>='.db_escape($as_of).') '
        .'ORDER BY p.policy_code,p.version_no,p.delivery_policy_version_id LIMIT 2';
    $r=db_query($sql,'could not resolve Work Authorization delivery requirement policy');
    if(!$r||db_num_rows($r)!==1)return false;
    $row=db_fetch_assoc($r);
    $metadata=array('policy_code'=>(string)$row['policy_code'],'version_no'=>(int)$row['version_no'],
        'predecessor_id'=>$row['predecessor_id']===null?null:(int)$row['predecessor_id'],
        'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],'delivery_requirement'=>(string)$row['delivery_requirement'],
        'recipient_rule_ref'=>(string)$row['recipient_rule_ref'],'channel_rule_ref'=>(string)$row['channel_rule_ref'],
        'recovery_rule_ref'=>(string)$row['recovery_rule_ref'],'policy_status'=>(string)$row['policy_status'],
        'effective_from'=>(string)$row['effective_from'],'effective_to'=>$row['effective_to']===null?null:(string)$row['effective_to'],
        'qualified_review_ref'=>(string)$row['qualified_review_ref'],'approved_at'=>$row['approved_at']===null?null:(string)$row['approved_at'],
        'approved_by'=>$row['approved_by']===null?null:(int)$row['approved_by']);
    if(!hrm_work_authorization_delivery_policy_validate_metadata($metadata))return false;
    $metadata['delivery_policy_version_id']=(int)$row['delivery_policy_version_id'];
    return $metadata;
}
