<?php
/**
 * PAY-CORE-009 pure restartable payroll-run/state-service contract.
 *
 * Side-effect free. No DB/session/request/network/GL/payment authority. Runtime
 * writers bind these descriptors to authoritative repository/database state.
 */
include_once(dirname(__FILE__).'/pay_core_004_contract.inc');
if(!defined('HRM_PAY_CORE_009_CONTRACT_VERSION')) define('HRM_PAY_CORE_009_CONTRACT_VERSION','1.0');
if(!defined('HRM_PAY_CORE_009_DEFAULT_CHUNK_SIZE')) define('HRM_PAY_CORE_009_DEFAULT_CHUNK_SIZE',10);
if(!defined('HRM_PAY_CORE_009_MAX_CHUNK_SIZE')) define('HRM_PAY_CORE_009_MAX_CHUNK_SIZE',25);
if(!defined('HRM_PAY_CORE_009_DEFAULT_LEASE_SECONDS')) define('HRM_PAY_CORE_009_DEFAULT_LEASE_SECONDS',120);
if(!defined('HRM_PAY_CORE_009_MAX_ATTEMPTS')) define('HRM_PAY_CORE_009_MAX_ATTEMPTS',3);
if(!defined('HRM_PAY_CORE_009_GL_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_009_GL_AUTHORITY_ALLOWED',false);
if(!defined('HRM_PAY_CORE_009_PAYMENT_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_009_PAYMENT_AUTHORITY_ALLOWED',false);
if(!defined('HRM_PAY_CORE_009_STATUTORY_AUTHORITY_ALLOWED')) define('HRM_PAY_CORE_009_STATUTORY_AUTHORITY_ALLOWED',false);
if(!defined('HRM_PAY_CORE_009_CALCULATOR_CUTOVER_ALLOWED')) define('HRM_PAY_CORE_009_CALCULATOR_CUTOVER_ALLOWED',false);

function hrm_pay_core_009_hash($v){return hrm_pay_core_004_hash($v);}
function hrm_pay_core_009_canonical_json($v){return hrm_pay_core_004_canonical_json($v);}
function hrm_pay_core_009_is_sha256($v){return is_string($v)&&preg_match('/^[a-f0-9]{64}$/D',$v)===1;}
function hrm_pay_core_009_token($v,$max=128){return is_string($v)&&strlen($v)>0&&strlen($v)<=$max&&preg_match('/^[A-Za-z0-9_.:-]+$/D',$v)===1?$v:false;}
function hrm_pay_core_009_states(){return array('queued','running','finalizing','completed','failed','quarantined','cancelled');}
function hrm_pay_core_009_item_states(){return array('pending','running','completed','failed','quarantined');}
function hrm_pay_core_009_allowed_transitions(){return array(
    'queued'=>array('running','cancelled'),
    'running'=>array('running','finalizing','failed','quarantined','cancelled'),
    'finalizing'=>array('completed','failed','quarantined'),
    'failed'=>array('running','quarantined','cancelled'),
    'quarantined'=>array('running','cancelled'),
    'completed'=>array(),
    'cancelled'=>array(),
);}
function hrm_pay_core_009_iso_utc($v){if(!is_string($v)||preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/D',$v)!==1)return false;$t=strtotime($v);return $t!==false&&gmdate('Y-m-d\TH:i:s\Z',$t)===$v?$v:false;}
function hrm_pay_core_009_date($v){return hrm_pay_core_004_is_iso_date($v)?$v:false;}

/** Explicit object scope. Calendar-period binding is nullable for legacy ad-hoc periods. */
function hrm_pay_core_009_scope(array $x,&$error=null)
{
    $error=null;
    $le=(int)($x['legal_entity_id']??0);$rel=(int)($x['payroll_relationship_id']??0);$worker=(int)($x['worker_id']??0);$person=(int)($x['person_id']??0);$assignment=(int)($x['assignment_id']??0);
    if($le<=0||$rel<=0||$worker<=0||$person<=0||$assignment<=0){$error='object_scope_required';return false;}
    $calendar=$x['calendar_period_id']??null;if($calendar!==null&&$calendar!==''){$calendar=(int)$calendar;if($calendar<=0){$error='calendar_scope_invalid';return false;}}else $calendar=null;
    return array('legal_entity_id'=>$le,'payroll_relationship_id'=>$rel,'worker_id'=>$worker,'person_id'=>$person,'assignment_id'=>$assignment,'calendar_period_id'=>$calendar);
}

/** Canonical run request. Creator deliberately excluded from business idempotency. */
function hrm_pay_core_009_run_request($period_name,$from_date,$to_date,array $items,$chunk_size,&$error=null)
{
    $error=null;$period_name=trim((string)$period_name);$from=hrm_pay_core_009_date($from_date);$to=hrm_pay_core_009_date($to_date);$chunk=(int)$chunk_size;
    if($period_name===''||strlen($period_name)>80||!$from||!$to||$from>$to||$chunk<=0||$chunk>HRM_PAY_CORE_009_MAX_CHUNK_SIZE||empty($items)){$error='run_request_invalid';return false;}
    $canon=array();$legal=null;$seen=array();
    foreach($items as $item){
        if(!is_array($item)){$error='run_item_invalid';return false;}
        $employee=trim((string)($item['employee_id']??''));$scope=hrm_pay_core_009_scope($item['scope']??array(),$se);
        if($employee===''||strlen($employee)>64||preg_match('/[\x00-\x1F\x7F]/',$employee)||$scope===false){$error=$scope===false?$se:'employee_identity_invalid';return false;}
        $eh=hash('sha256',$employee);if(isset($seen[$eh])){$error='duplicate_employee_identity';return false;}$seen[$eh]=1;
        if($legal===null)$legal=$scope['legal_entity_id'];elseif($legal!==$scope['legal_entity_id']){$error='mixed_legal_entity_run_denied';return false;}
        $canon[]=array('employee_id_sha256'=>$eh,'scope'=>$scope);
    }
    usort($canon,function($a,$b){return strcmp($a['employee_id_sha256'],$b['employee_id_sha256']);});
    $o=array('run_type'=>'legacy_payroll_prepare','legal_entity_id'=>$legal,'period_name'=>$period_name,'from_date'=>$from,'to_date'=>$to,'items'=>$canon,'chunk_size'=>$chunk);
    $o['request_sha256']=hrm_pay_core_009_hash($o);$o['run_key']='pc9-'.substr($o['request_sha256'],0,48);return $o;
}

function hrm_pay_core_009_state_descriptor($run_key,$state,$version,$request_sha256,$checkpoint_no,$lease_token_sha256=null,&$error=null)
{
    $error=null;$key=hrm_pay_core_009_token($run_key,96);$version=(int)$version;$checkpoint=(int)$checkpoint_no;
    if(!$key||!in_array($state,hrm_pay_core_009_states(),true)||$version<=0||$checkpoint<0||!hrm_pay_core_009_is_sha256($request_sha256)||($lease_token_sha256!==null&&!hrm_pay_core_009_is_sha256($lease_token_sha256))){$error='state_descriptor_invalid';return false;}
    $o=array('run_key'=>$key,'state'=>$state,'state_version'=>$version,'request_sha256'=>$request_sha256,'checkpoint_no'=>$checkpoint,'lease_token_sha256'=>$lease_token_sha256);$o['state_sha256']=hrm_pay_core_009_hash($o);return $o;
}

function hrm_pay_core_009_transition(array $current,$expected_state,$expected_version,$target_state,$command_key,$actor_id,&$error=null)
{
    $error=null;$cmd=hrm_pay_core_009_token($command_key,128);$actor=(int)$actor_id;$expected_version=(int)$expected_version;
    $cur=hrm_pay_core_009_state_descriptor($current['run_key']??'', $current['state']??'', $current['state_version']??0, $current['request_sha256']??'', $current['checkpoint_no']??0, $current['lease_token_sha256']??null, $ce);
    if($cur===false||!$cmd||$actor<=0||$expected_state!==$cur['state']||$expected_version!==$cur['state_version']){$error=$cur===false?$ce:'stale_state';return false;}
    $allowed=hrm_pay_core_009_allowed_transitions();if(!in_array($target_state,$allowed[$cur['state']],true)){$error='transition_denied';return false;}
    $next=hrm_pay_core_009_state_descriptor($cur['run_key'],$target_state,$cur['state_version']+1,$cur['request_sha256'],$cur['checkpoint_no'],$cur['lease_token_sha256'],$ne);if($next===false){$error=$ne;return false;}
    $t=array('run_key'=>$cur['run_key'],'from_state'=>$cur['state'],'to_state'=>$target_state,'expected_version'=>$cur['state_version'],'resulting_version'=>$next['state_version'],'command_key'=>$cmd,'predecessor_state_sha256'=>$cur['state_sha256'],'resulting_state_sha256'=>$next['state_sha256'],'actor_id'=>$actor);$t['transition_sha256']=hrm_pay_core_009_hash($t);return array('state'=>$next,'transition'=>$t);
}

function hrm_pay_core_009_lease($run_key,$worker_token,$lease_token_sha256,$acquired_at,$expires_at,$event,&$error=null)
{
    $error=null;$run=hrm_pay_core_009_token($run_key,96);$worker=hrm_pay_core_009_token($worker_token,128);$a=hrm_pay_core_009_iso_utc($acquired_at);$e=hrm_pay_core_009_iso_utc($expires_at);
    if(!$run||!$worker||!hrm_pay_core_009_is_sha256($lease_token_sha256)||!$a||!$e||$e<=$a||!in_array($event,array('acquired','heartbeat','released','expired_takeover'),true)){$error='lease_invalid';return false;}
    $o=array('run_key'=>$run,'worker_sha256'=>hash('sha256',$worker),'lease_token_sha256'=>$lease_token_sha256,'event'=>$event,'acquired_at'=>$a,'expires_at'=>$e);$o['lease_event_sha256']=hrm_pay_core_009_hash($o);return $o;
}
function hrm_pay_core_009_lease_is_expired($expires_at,$now){$e=hrm_pay_core_009_iso_utc($expires_at);$n=hrm_pay_core_009_iso_utc($now);return !$e||!$n||$e<=$n;}

function hrm_pay_core_009_item_descriptor($run_key,$ordinal,$employee_id,array $scope,$input_sha256,$state,$state_version,$attempt_no,$result_sha256=null,&$error=null)
{
    $error=null;$run=hrm_pay_core_009_token($run_key,96);$ordinal=(int)$ordinal;$employee=trim((string)$employee_id);$attempt=(int)$attempt_no;$version=(int)$state_version;$s=hrm_pay_core_009_scope($scope,$se);
    if(!$run||$ordinal<=0||$employee===''||strlen($employee)>64||$s===false||!hrm_pay_core_009_is_sha256($input_sha256)||!in_array($state,hrm_pay_core_009_item_states(),true)||$version<=0||$attempt<0||($result_sha256!==null&&!hrm_pay_core_009_is_sha256($result_sha256))){$error=$s===false?$se:'item_descriptor_invalid';return false;}
    $o=array('run_key'=>$run,'ordinal'=>$ordinal,'employee_id_sha256'=>hash('sha256',$employee),'scope'=>$s,'input_sha256'=>$input_sha256,'state'=>$state,'state_version'=>$version,'attempt_no'=>$attempt,'result_sha256'=>$result_sha256);$o['item_state_sha256']=hrm_pay_core_009_hash($o);return $o;
}

function hrm_pay_core_009_checkpoint($run_key,$checkpoint_no,$completed,$failed,$quarantined,$item_set_sha256,$state_sha256,$evidence_sha256,&$error=null)
{
    $error=null;$run=hrm_pay_core_009_token($run_key,96);$checkpoint=(int)$checkpoint_no;$completed=(int)$completed;$failed=(int)$failed;$quarantined=(int)$quarantined;
    if(!$run||$checkpoint<=0||min($completed,$failed,$quarantined)<0||!hrm_pay_core_009_is_sha256($item_set_sha256)||!hrm_pay_core_009_is_sha256($state_sha256)||!hrm_pay_core_009_is_sha256($evidence_sha256)){$error='checkpoint_invalid';return false;}
    $o=array('run_key'=>$run,'checkpoint_no'=>$checkpoint,'completed_items'=>$completed,'failed_items'=>$failed,'quarantined_items'=>$quarantined,'item_set_sha256'=>$item_set_sha256,'state_sha256'=>$state_sha256,'evidence_sha256'=>$evidence_sha256);$o['checkpoint_sha256']=hrm_pay_core_009_hash($o);return $o;
}

function hrm_pay_core_009_attempt($run_key,$ordinal,$attempt_no,$command_key,$worker_token,$predecessor_sha256,$outcome,$result_sha256,$error_code,$evidence_sha256,&$error=null)
{
    $error=null;$run=hrm_pay_core_009_token($run_key,96);$cmd=hrm_pay_core_009_token($command_key,128);$worker=hrm_pay_core_009_token($worker_token,128);$code=$error_code===null?null:hrm_pay_core_009_token($error_code,96);$result=$result_sha256===null?null:$result_sha256;
    if(!$run||(int)$ordinal<=0||(int)$attempt_no<=0||!$cmd||!$worker||!hrm_pay_core_009_is_sha256($predecessor_sha256)||!in_array($outcome,array('completed','failed','quarantined','stale'),true)||($result!==null&&!hrm_pay_core_009_is_sha256($result))||($code!==null&&!$code)||!hrm_pay_core_009_is_sha256($evidence_sha256)){$error='attempt_invalid';return false;}
    $o=array('run_key'=>$run,'ordinal'=>(int)$ordinal,'attempt_no'=>(int)$attempt_no,'command_key'=>$cmd,'worker_sha256'=>hash('sha256',$worker),'predecessor_state_sha256'=>$predecessor_sha256,'outcome'=>$outcome,'result_sha256'=>$result,'error_code'=>$code,'evidence_sha256'=>$evidence_sha256);$o['attempt_sha256']=hrm_pay_core_009_hash($o);return $o;
}

function hrm_pay_core_009_recovery($run_key,$action,$predecessor_state_sha256,$evidence_sha256,$actor_id,&$error=null)
{
    $error=null;$run=hrm_pay_core_009_token($run_key,96);$action=hrm_pay_core_009_token($action,64);$actor=(int)$actor_id;
    if(!$run||!in_array($action,array('resume_failed','resume_quarantined','expire_lease','reconcile','cancel'),true)||!hrm_pay_core_009_is_sha256($predecessor_state_sha256)||!hrm_pay_core_009_is_sha256($evidence_sha256)||$actor<=0){$error='recovery_invalid';return false;}
    $o=array('run_key'=>$run,'action'=>$action,'predecessor_state_sha256'=>$predecessor_state_sha256,'evidence_sha256'=>$evidence_sha256,'actor_id'=>$actor);$o['recovery_sha256']=hrm_pay_core_009_hash($o);return $o;
}
?>
