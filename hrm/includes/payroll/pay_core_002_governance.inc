<?php
/** Pure PAY-CORE-002 governance and deterministic period generation. No DB/runtime authority. */
include_once(dirname(__FILE__).'/pay_core_002_contract.inc');
if(!defined('HRM_PAY_CORE_002_GOVERNANCE_VERSION'))define('HRM_PAY_CORE_002_GOVERNANCE_VERSION','1.0');
if(!defined('HRM_PAY_CORE_002_AUTOMATIC_SEED_ALLOWED'))define('HRM_PAY_CORE_002_AUTOMATIC_SEED_ALLOWED',false);
if(!defined('HRM_PAY_CORE_002_AUTOMATIC_LEGACY_CONVERSION_ALLOWED'))define('HRM_PAY_CORE_002_AUTOMATIC_LEGACY_CONVERSION_ALLOWED',false);

function hrm_pay_core_002_token($v,$max=96){if(!is_string($v))return false;$v=trim($v);return $v!==''&&strlen($v)<=$max&&preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]*$/D',$v)?$v:false;}
function hrm_pay_core_002_sha($v){return is_string($v)&&preg_match('/^[a-f0-9]{64}$/D',$v)?$v:false;}
function hrm_pay_core_002_currency($v){return is_string($v)&&preg_match('/^[A-Z]{3}$/D',$v)?$v:false;}
function hrm_pay_core_002_timezone($v){return is_string($v)&&strlen($v)<=64&&preg_match('/^[A-Za-z][A-Za-z0-9_.+-]*(?:\/[A-Za-z0-9_.+-]+)+$/D',$v)?$v:false;}
function hrm_pay_core_002_hash(array $v){ksort($v);return hash('sha256',json_encode($v,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE));}
function hrm_pay_core_002_validate_seed_proposal(array $x){
    foreach(array('calendar_code','calendar_name','group_code','group_name','legal_entity_id','currency_code','frequency','timezone_name','effective_from','period_plan') as $k)if(!array_key_exists($k,$x))return false;
    if(hrm_pay_core_002_token($x['calendar_code'],64)===false||hrm_pay_core_002_token($x['group_code'],64)===false)return false;
    if(!is_string($x['calendar_name'])||trim($x['calendar_name'])===''||strlen($x['calendar_name'])>160||!is_string($x['group_name'])||trim($x['group_name'])===''||strlen($x['group_name'])>160)return false;
    if((int)$x['legal_entity_id']<=0||hrm_pay_core_002_currency($x['currency_code'])===false||!in_array($x['frequency'],hrm_pay_core_002_allowed_frequencies(),true)||hrm_pay_core_002_timezone($x['timezone_name'])===false||!hrm_pay_core_002_validate_iso_date($x['effective_from']))return false;
    return hrm_pay_core_002_validate_period_plan($x['frequency'],$x['period_plan'],$x['effective_from'])!==false;
}
function hrm_pay_core_002_days_in_month($y,$m){return cal_days_in_month(CAL_GREGORIAN,(int)$m,(int)$y);}
function hrm_pay_core_002_period($start,$end,$pay,$cutoff,$post,$file,$key,$run='regular'){$p=array('period_start'=>$start,'period_end'=>$end,'cutoff_date'=>$cutoff,'pay_date'=>$pay,'post_date'=>$post,'file_date'=>$file,'run_type'=>$run);return hrm_pay_core_002_validate_period_contract($p)?array_merge(array('period_key'=>$key),$p):false;}
/** Standard-frequency generation is deterministic. Custom periods MUST be supplied explicitly. */
function hrm_pay_core_002_generate_periods($frequency,$start,$count,$offsets=array()){
    if(!in_array($frequency,hrm_pay_core_002_allowed_frequencies(),true)||!hrm_pay_core_002_validate_iso_date($start)||!is_int($count)||$count<1||$count>120)return false;
    if($frequency==='custom')return false;
    $cut=(int)($offsets['cutoff_days_before_pay']??0);$post=(int)($offsets['post_days_before_pay']??0);$file=(int)($offsets['file_days_before_pay']??0);foreach(array($cut,$post,$file) as $n)if($n<0||$n>31)return false;
    $cursor=new DateTimeImmutable($start.' 00:00:00',new DateTimeZone('UTC'));$out=array();
    for($i=0;$i<$count;$i++){
        $ps=$cursor;
        if($frequency==='weekly'){$pe=$ps->modify('+6 days');$cursor=$pe->modify('+1 day');}
        elseif($frequency==='biweekly'){$pe=$ps->modify('+13 days');$cursor=$pe->modify('+1 day');}
        elseif($frequency==='four_weekly'){$pe=$ps->modify('+27 days');$cursor=$pe->modify('+1 day');}
        elseif($frequency==='monthly'){$pe=$ps->modify('last day of this month');$cursor=$pe->modify('+1 day');}
        else { // semimonthly: 1-15 then 16-EOM; a valid anchor must be day 1 or 16.
            $d=(int)$ps->format('j');if($d===1){$pe=$ps->setDate((int)$ps->format('Y'),(int)$ps->format('n'),15);$cursor=$pe->modify('+1 day');}
            elseif($d===16){$pe=$ps->modify('last day of this month');$cursor=$pe->modify('+1 day');}else return false;
        }
        $pay=$pe;$cutoff=$pay->modify('-'.$cut.' days');$postd=$pay->modify('-'.$post.' days');$filed=$pay->modify('-'.$file.' days');$key=$ps->format('Ymd').'-'.$pe->format('Ymd');
        $row=hrm_pay_core_002_period($ps->format('Y-m-d'),$pe->format('Y-m-d'),$pay->format('Y-m-d'),$cutoff->format('Y-m-d'),$postd->format('Y-m-d'),$filed->format('Y-m-d'),$key);if($row===false)return false;$out[]=$row;
    }return $out;
}
function hrm_pay_core_002_validate_custom_periods(array $periods){if(!$periods)return false;$prev=null;foreach($periods as $p){if(!is_array($p)||empty($p['period_key'])||hrm_pay_core_002_token($p['period_key'],80)===false||!hrm_pay_core_002_validate_period_contract($p))return false;if($prev!==null&&$p['period_start']<=$prev)return false;$prev=$p['period_end'];}return true;}

function hrm_pay_core_002_validate_period_plan($frequency,$plan,$effective_from){if(!is_array($plan)||!hrm_pay_core_002_validate_iso_date($effective_from))return false;if($frequency==='custom'){if(!isset($plan['periods'])||!is_array($plan['periods'])||!hrm_pay_core_002_validate_custom_periods($plan['periods']))return false;if($plan['periods'][0]['period_start']<$effective_from)return false;return array_values($plan['periods']);}$count=isset($plan['count'])?(int)$plan['count']:0;if($count<1||$count>120)return false;$offsets=array('cutoff_days_before_pay'=>(int)($plan['cutoff_days_before_pay']??0),'post_days_before_pay'=>(int)($plan['post_days_before_pay']??0),'file_days_before_pay'=>(int)($plan['file_days_before_pay']??0));return hrm_pay_core_002_generate_periods($frequency,$effective_from,$count,$offsets);}
