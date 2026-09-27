<?php
/** PAY-API-001 pure provisioning request/review/approval and target validation. */
include_once(dirname(__FILE__).'/pay_api_001_authn.inc');
if (!defined('PAY_API_001_PROVISIONING_CONTRACT_VERSION')) define('PAY_API_001_PROVISIONING_CONTRACT_VERSION', '1.0.866');

function pay_api_001_provisioning_positive_id($value){return pay_api_001_positive_id($value);}
function pay_api_001_provisioning_sha256($value){return pay_api_001_is_sha256($value);}
function pay_api_001_provisioning_target_kind($value){return in_array((string)$value,array('resource','machine_principal','principal_scope','credential','credential_revocation'),true);}
function pay_api_001_provisioning_token($value,$max,$pattern='/^[a-z0-9][a-z0-9._-]*$/D')
{
    return is_string($value)&&$value!==''&&strlen($value)<=(int)$max&&preg_match($pattern,$value)===1?$value:false;
}
function pay_api_001_provisioning_datetime($value)
{
    if(!is_string($value)||preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',$value)!==1)return false;
    $d=DateTime::createFromFormat('!Y-m-d H:i:s',$value,new DateTimeZone('UTC'));
    return $d&&$d->format('Y-m-d H:i:s')===$value?$value:false;
}
function pay_api_001_provisioning_canonicalize($value)
{
    if(!is_array($value))return $value;
    $keys=array_keys($value);$assoc=count($value)>0&&$keys!==range(0,count($value)-1);
    if($assoc)ksort($value,SORT_STRING);
    foreach($value as $k=>$v)$value[$k]=pay_api_001_provisioning_canonicalize($v);
    return $value;
}
function pay_api_001_provisioning_hash($value)
{
    $json=json_encode(pay_api_001_provisioning_canonicalize($value),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    return is_string($json)?hash('sha256',$json):false;
}
function pay_api_001_provisioning_password_hash($value)
{
    if(!is_string($value)||$value===''||strlen($value)>255)return false;
    $info=password_get_info($value);
    return is_array($info)&&isset($info['algoName'])&&$info['algoName']!=='unknown';
}
function pay_api_001_provisioning_target_descriptor($kind,$company_id,$payload)
{
    $company=pay_api_001_uint_value($company_id,true,4294967295);
    if($company===false||!pay_api_001_provisioning_target_kind($kind)||!is_array($payload))return false;
    $n=array();$key=false;
    if($kind==='resource'){
        $rk=isset($payload['resource_key'])?pay_api_001_provisioning_token($payload['resource_key'],96,'/^[a-z0-9][a-z0-9._\/-]{0,95}$/D'):false;
        $cv=isset($payload['contract_version'])?pay_api_001_provisioning_token($payload['contract_version'],32,'/^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$/D'):false;
        $rkind=isset($payload['resource_kind'])&&in_array($payload['resource_kind'],array('collection','object'),true)?$payload['resource_kind']:false;
        foreach(array('company_scope_required','object_scope_required','read_capable','command_capable') as $b)if(!array_key_exists($b,$payload)||!in_array($payload[$b],array(0,1,true,false),true))return false;
        if($rk===false||$cv===false||$rkind===false||empty($payload['company_scope_required'])||empty($payload['read_capable'])||!empty($payload['command_capable']))return false;
        $n=array('resource_key'=>$rk,'contract_version'=>$cv,'resource_kind'=>$rkind,'company_scope_required'=>1,'object_scope_required'=>empty($payload['object_scope_required'])?0:1,'read_capable'=>1,'command_capable'=>0);
        $key='resource:'.$rk.':'.$cv;
    }elseif($kind==='machine_principal'){
        $pc=isset($payload['company_id'])?pay_api_001_uint_value($payload['company_id'],true,4294967295):false;
        $pk=isset($payload['principal_key'])?pay_api_001_provisioning_token($payload['principal_key'],96,'/^[a-z0-9][a-z0-9._-]{0,95}$/D'):false;
        $purpose=isset($payload['purpose_code'])?pay_api_001_provisioning_token($payload['purpose_code'],64):false;
        $name=isset($payload['display_name'])&&is_string($payload['display_name'])?trim($payload['display_name']):'';
        if($pc===false||$pc!==$company||$pk===false||$purpose===false||$name===''||strlen($name)>160)return false;
        $n=array('company_id'=>$pc,'principal_key'=>$pk,'display_name'=>$name,'purpose_code'=>$purpose,'principal_status'=>'active');$key='machine_principal:'.$pc.':'.$pk;
    }elseif($kind==='principal_scope'){
        $pid=isset($payload['api_principal_id'])?pay_api_001_positive_id($payload['api_principal_id']):false;$rid=isset($payload['api_resource_id'])?pay_api_001_positive_id($payload['api_resource_id']):false;
        if($pid===false||$rid===false||(isset($payload['action_code'])&&(string)$payload['action_code']!=='read'))return false;
        $ost=array_key_exists('object_scope_type',$payload)?$payload['object_scope_type']:null;$osk=array_key_exists('object_scope_key',$payload)?$payload['object_scope_key']:null;
        if(($ost===null)!==($osk===null))return false;
        if($ost!==null){$ost=pay_api_001_provisioning_token($ost,32);$osk=pay_api_001_provisioning_token($osk,128,'/^[A-Za-z0-9][A-Za-z0-9._:\/-]{0,127}$/D');if($ost===false||$osk===false)return false;}
        $n=array('api_principal_id'=>$pid,'api_resource_id'=>$rid,'action_code'=>'read','object_scope_type'=>$ost,'object_scope_key'=>$osk);$key='principal_scope:'.$pid.':'.$rid.':read:'.($ost===null?'-':$ost).':'.($osk===null?'-':$osk);
    }elseif($kind==='credential'){
        $pid=isset($payload['api_principal_id'])?pay_api_001_positive_id($payload['api_principal_id']):false;$ck=isset($payload['credential_key'])?pay_api_001_provisioning_token($payload['credential_key'],128,'/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D'):false;
        $scheme=isset($payload['verifier_scheme'])?(string)$payload['verifier_scheme']:'';$vh=isset($payload['secret_verifier_hash'])?$payload['secret_verifier_hash']:null;$vf=isset($payload['valid_from'])?pay_api_001_provisioning_datetime($payload['valid_from']):false;$ex=isset($payload['expires_at'])?pay_api_001_provisioning_datetime($payload['expires_at']):false;
        if($pid===false||$ck===false||$scheme!=='password_hash'||!pay_api_001_provisioning_password_hash($vh)||$vf===false||$ex===false||strcmp($ex,$vf)<=0)return false;
        $n=array('api_principal_id'=>$pid,'credential_key'=>$ck,'verifier_scheme'=>'password_hash','secret_verifier_hash'=>$vh,'valid_from'=>$vf,'expires_at'=>$ex);$key='credential:'.$ck;
    }else{
        $cid=isset($payload['api_credential_id'])?pay_api_001_positive_id($payload['api_credential_id']):false;$reason=isset($payload['reason_code'])?pay_api_001_provisioning_token($payload['reason_code'],64):false;
        if($cid===false||$reason===false)return false;$n=array('api_credential_id'=>$cid,'reason_code'=>$reason);$key='credential_revocation:'.$cid;
    }
    return array('target_kind'=>$kind,'company_id'=>$company,'target_key_sha256'=>hash('sha256',$key),'target_payload_sha256'=>pay_api_001_provisioning_hash($n),'normalized_payload'=>$n);
}
function pay_api_001_provision_request_gate($request)
{
    if(!is_array($request)||!isset($request['company_id'])||!pay_api_001_is_company_id($request['company_id'])||!isset($request['target_kind'])||!pay_api_001_provisioning_target_kind($request['target_kind']))return false;
    foreach(array('target_key_sha256','target_payload_sha256','request_sha256') as $k)if(!isset($request[$k])||!pay_api_001_provisioning_sha256($request[$k]))return false;
    if(!isset($request['reason_code'])||pay_api_001_provisioning_token($request['reason_code'],64)===false||!isset($request['requested_by'])||pay_api_001_provisioning_positive_id($request['requested_by'])===false)return false;
    return true;
}
function pay_api_001_provision_review_gate($request,$review)
{
    if(!pay_api_001_provision_request_gate($request)||!is_array($review))return false;$request_id=isset($request['provision_request_id'])?pay_api_001_provisioning_positive_id($request['provision_request_id']):false;
    if($request_id===false||!isset($review['provision_request_id'])||pay_api_001_provisioning_positive_id($review['provision_request_id'])!==$request_id)return false;$reviewer=isset($review['reviewed_by'])?pay_api_001_provisioning_positive_id($review['reviewed_by']):false;$maker=pay_api_001_provisioning_positive_id($request['requested_by']);
    if($reviewer===false||$reviewer===$maker||!isset($review['decision_code'])||!in_array((string)$review['decision_code'],array('approve','reject'),true))return false;
    if(!isset($review['request_sha256'])||!hash_equals((string)$request['request_sha256'],(string)$review['request_sha256'])||!isset($review['review_sha256'])||!pay_api_001_provisioning_sha256($review['review_sha256']))return false;return true;
}
function pay_api_001_provision_approval_gate($request,$review,$approval)
{
    if(!pay_api_001_provision_review_gate($request,$review)||(string)$review['decision_code']!=='approve'||!is_array($approval))return false;$request_id=pay_api_001_provisioning_positive_id($request['provision_request_id']);$review_id=isset($review['provision_review_id'])?pay_api_001_provisioning_positive_id($review['provision_review_id']):false;
    if($review_id===false||!isset($approval['provision_request_id'])||pay_api_001_provisioning_positive_id($approval['provision_request_id'])!==$request_id||!isset($approval['provision_review_id'])||pay_api_001_provisioning_positive_id($approval['provision_review_id'])!==$review_id)return false;
    $maker=pay_api_001_provisioning_positive_id($request['requested_by']);$reviewer=pay_api_001_provisioning_positive_id($review['reviewed_by']);$approver=isset($approval['approved_by'])?pay_api_001_provisioning_positive_id($approval['approved_by']):false;
    if($approver===false||$approver===$maker||$approver===$reviewer||!isset($approval['decision_code'])||(string)$approval['decision_code']!=='approve')return false;
    if(!isset($approval['request_sha256'])||!hash_equals((string)$request['request_sha256'],(string)$approval['request_sha256'])||!isset($approval['review_sha256'])||!hash_equals((string)$review['review_sha256'],(string)$approval['review_sha256'])||!isset($approval['approval_sha256'])||!pay_api_001_provisioning_sha256($approval['approval_sha256']))return false;return true;
}
function pay_api_001_provision_execution_gate($request,$review,$approval,$executor_id,$payload)
{
    if(!pay_api_001_provision_approval_gate($request,$review,$approval))return false;$executor=pay_api_001_positive_id($executor_id);if($executor===false||$executor===pay_api_001_positive_id($request['requested_by'])||$executor===pay_api_001_positive_id($review['reviewed_by'])||$executor===pay_api_001_positive_id($approval['approved_by']))return false;
    $d=pay_api_001_provisioning_target_descriptor((string)$request['target_kind'],$request['company_id'],$payload);if($d===false||!hash_equals((string)$request['target_key_sha256'],$d['target_key_sha256'])||!hash_equals((string)$request['target_payload_sha256'],$d['target_payload_sha256']))return false;return $d;
}
function pay_api_001_provisioning_execution_writer_enabled(){return true;}
?>
