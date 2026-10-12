<?php
/** Versioned native operational-calendar display; civil dates never become statutory or exact-time authority. */
require_once __DIR__.'/pay_glb_001_native_reader_view.inc';
require_once dirname(__DIR__).'/hrm/includes/payroll/pay_core_002_governance.inc';

/** Require the original CORE002 canonical hash rather than a copied digest assertion. */
function pay_glb_001_native_calendar_hash_matches(array $row,$field,array $payload)
{
    return isset($row[$field]) && pay_glb_001_sha256($row[$field])
        && hash_equals($row[$field],hrm_pay_core_002_hash($payload));
}

/** Reconstruct existing proposal/checker/executor custody and every generated period before admitting calendar facts. */
function pay_glb_001_native_calendar_verify(array $b,$calendar_period_id,&$error=null)
{
    $error=null;
    if(!pay_glb_001_exact_keys($b,array('proposal','approval','execution','calendar','version','group','group_version','periods'))
        || !is_string($calendar_period_id) || !ctype_digit($calendar_period_id) || (int)$calendar_period_id<1){$error='native_calendar_bundle_invalid';return false;}
    foreach($b as $value)if(!is_array($value)){$error='native_calendar_bundle_invalid';return false;}
    $p=$b['proposal'];$a=$b['approval'];$e=$b['execution'];$c=$b['calendar'];$v=$b['version'];$g=$b['group'];$gv=$b['group_version'];
    $required=array('proposal'=>array('proposal_id','proposal_key','legal_entity_id','currency_code','calendar_code','calendar_name','group_code','group_name','frequency_token','timezone_name','effective_from','period_plan_json','period_plan_sha256','proposal_sha256','maker_id'),
        'approval'=>array('approval_id','proposal_id','proposal_sha256','decision_token','evidence_sha256','approval_sha256','checker_id'),
        'execution'=>array('execution_id','proposal_id','approval_id','calendar_id','calendar_version_id','payroll_group_id','group_version_id','currency_code','period_count','period_set_sha256','execution_sha256','executor_id'),
        'calendar'=>array('calendar_id','calendar_code','calendar_name','legal_entity_id','calendar_sha256','created_by'),
        'version'=>array('calendar_version_id','calendar_id','version_no','frequency_token','effective_from','effective_to','timezone_name','version_sha256','created_by'),
        'group'=>array('payroll_group_id','group_code','group_name','legal_entity_id','group_sha256','created_by'),
        'group_version'=>array('group_version_id','payroll_group_id','version_no','calendar_version_id','effective_from','effective_to','version_sha256','created_by'));
    foreach($required as $kind=>$fields)foreach($fields as $field){
        if(!array_key_exists($field,$b[$kind])){$error='native_calendar_bundle_incomplete';return false;}
        if($b[$kind][$field]!==null && !is_string($b[$kind][$field])){$error='native_calendar_sql_scalar_invalid';return false;}
    }
    if(!pay_glb_001_sha256($p['period_plan_sha256']) || !is_string($p['period_plan_json'])){$error='native_calendar_plan_invalid';return false;}
    $maker=(int)$p['maker_id'];$checker=(int)$a['checker_id'];$executor=(int)$e['executor_id'];
    if(min($maker,$checker,$executor)<1 || count(array_unique(array($maker,$checker,$executor)))!==3
        || $a['decision_token']!=='approved' || !pay_glb_001_sha256($a['evidence_sha256'])
        || (string)$a['proposal_id']!==(string)$p['proposal_id'] || $a['proposal_sha256']!==$p['proposal_sha256']
        || (string)$e['proposal_id']!==(string)$p['proposal_id'] || (string)$e['approval_id']!==(string)$a['approval_id']){
        $error='native_calendar_governance_invalid';return false;
    }
    $proposal=array();foreach(array('proposal_key','currency_code','calendar_code','calendar_name','group_code','group_name','frequency_token','timezone_name','effective_from','period_plan_sha256') as $field)$proposal[$field]=$p[$field];
    $proposal['legal_entity_id']=(int)$p['legal_entity_id'];$proposal['maker_id']=$maker;
    $plan=is_string($p['period_plan_json'])?json_decode($p['period_plan_json'],true):false;
    $seed_input=$proposal;$seed_input['frequency']=$p['frequency_token'];$seed_input['period_plan']=$plan;
    $periods=is_array($plan)?hrm_pay_core_002_validate_period_plan($p['frequency_token'],$plan,$p['effective_from']):false;
    if(!$periods || !hrm_pay_core_002_validate_seed_proposal($seed_input) || !pay_glb_001_native_calendar_hash_matches($p,'proposal_sha256',$proposal)
        || !hash_equals($p['period_plan_sha256'],hash('sha256',$p['period_plan_json']))
        || !pay_glb_001_native_calendar_hash_matches($a,'approval_sha256',array('proposal_id'=>(int)$p['proposal_id'],
            'proposal_sha256'=>$p['proposal_sha256'],'decision_token'=>'approved','evidence_sha256'=>$a['evidence_sha256'],'checker_id'=>$checker))){
        $error='native_calendar_proposal_or_approval_hash_invalid';return false;
    }
    if(!is_string($p['timezone_name']) || !in_array($p['timezone_name'],DateTimeZone::listIdentifiers(),true)
        || (string)$v['version_no']!=='1' || (string)$gv['version_no']!=='1' || $v['effective_to']!==null || $gv['effective_to']!==null
        || (string)$c['calendar_id']!==(string)$e['calendar_id'] || (string)$v['calendar_id']!==(string)$c['calendar_id']
        || (string)$v['calendar_version_id']!==(string)$e['calendar_version_id']
        || (string)$g['payroll_group_id']!==(string)$e['payroll_group_id'] || (string)$gv['payroll_group_id']!==(string)$g['payroll_group_id']
        || (string)$gv['group_version_id']!==(string)$e['group_version_id'] || (string)$gv['calendar_version_id']!==(string)$v['calendar_version_id']
        || (string)$c['legal_entity_id']!==(string)$p['legal_entity_id'] || (string)$g['legal_entity_id']!==(string)$p['legal_entity_id']
        || $c['calendar_code']!==$p['calendar_code'] || $g['group_code']!==$p['group_code']
        || $c['calendar_name']!==$p['calendar_name'] || $g['group_name']!==$p['group_name']
        || $v['timezone_name']!==$p['timezone_name'] || $v['frequency_token']!==$p['frequency_token']
        || $v['effective_from']!==$p['effective_from'] || $gv['effective_from']!==$p['effective_from'] || $e['currency_code']!==$p['currency_code']){
        $error='native_calendar_scope_or_timezone_invalid';return false;
    }
    foreach(array($c,$v,$g,$gv) as $row)if((int)$row['created_by']!==$executor){$error='native_calendar_executor_invalid';return false;}
    if(!pay_glb_001_native_calendar_hash_matches($c,'calendar_sha256',array('calendar_code'=>$p['calendar_code'],'legal_entity_id'=>(int)$p['legal_entity_id']))
        || !pay_glb_001_native_calendar_hash_matches($v,'version_sha256',array('calendar_id'=>(int)$c['calendar_id'],'version_no'=>1,'frequency_token'=>$p['frequency_token'],'effective_from'=>$p['effective_from'],'timezone_name'=>$p['timezone_name']))
        || !pay_glb_001_native_calendar_hash_matches($g,'group_sha256',array('group_code'=>$p['group_code'],'legal_entity_id'=>(int)$p['legal_entity_id']))
        || !pay_glb_001_native_calendar_hash_matches($gv,'version_sha256',array('payroll_group_id'=>(int)$g['payroll_group_id'],'version_no'=>1,'calendar_version_id'=>(int)$v['calendar_version_id'],'effective_from'=>$p['effective_from']))){
        $error='native_calendar_identity_hash_invalid';return false;
    }
    if(count($b['periods'])!==count($periods) || (int)$e['period_count']!==count($periods)
        || array_keys($b['periods'])!==range(0,count($periods)-1)){$error='native_calendar_period_set_incomplete';return false;}
    $hashes=array();$selected=null;$seen=array();
    foreach($b['periods'] as $i=>$row){
        if(!is_array($row)){$error='native_calendar_period_invalid';return false;}
        $fields=array('calendar_period_id','calendar_version_id','period_key','period_start','period_end','cutoff_date','pay_date','post_date','file_date','run_type','period_sha256','created_by');
        foreach($fields as $field)if(!array_key_exists($field,$row)){$error='native_calendar_period_invalid';return false;}
        if((string)$row['calendar_version_id']!==(string)$v['calendar_version_id'] || (int)$row['created_by']!==$executor
            || isset($seen[$row['calendar_period_id']])){$error='native_calendar_period_invalid';return false;}
        $seen[$row['calendar_period_id']]=true;$payload=array('calendar_version_id'=>(int)$v['calendar_version_id']);
        foreach(array('period_key','period_start','period_end','cutoff_date','pay_date','post_date','file_date','run_type') as $field){
            if($row[$field]!==$periods[$i][$field]){$error='native_calendar_generated_period_mismatch';return false;}$payload[$field]=$row[$field];}
        if(!pay_glb_001_native_calendar_hash_matches($row,'period_sha256',$payload)){$error='native_calendar_period_hash_invalid';return false;}
        $hashes[]=$row['period_sha256'];if((string)$row['calendar_period_id']===$calendar_period_id)$selected=$row;
    }
    $set=hash('sha256',implode('|',$hashes));
    if(!$selected || $e['period_set_sha256']!==$set || !pay_glb_001_native_calendar_hash_matches($e,'execution_sha256',
        array('proposal_sha256'=>$p['proposal_sha256'],'approval_sha256'=>$a['approval_sha256'],'calendar_version_id'=>(int)$v['calendar_version_id'],
            'group_version_id'=>(int)$gv['group_version_id'],'currency_code'=>$p['currency_code'],'period_count'=>count($hashes),'period_set_sha256'=>$set,'executor_id'=>$executor))){
        $error='native_calendar_execution_hash_invalid';return false;
    }
    return array('state'=>'GOVERNED_OPERATIONAL_DATES','cutoff_date'=>$selected['cutoff_date'],'pay_date'=>$selected['pay_date'],
        'timezone_name'=>$v['timezone_name'],'proof_sha256'=>hash('sha256',json_encode(array('domain'=>'PAY-GLB-001-NATIVE-CALENDAR-V1',
            'execution_sha256'=>$e['execution_sha256'],'period_sha256'=>$selected['period_sha256']),JSON_UNESCAPED_SLASHES)));
}

/** Keep missing approval/execution provenance explicit; never infer dates from source scaffolding. */
function pay_glb_001_native_calendar_unknown()
{
    return array('state'=>'UNKNOWN_UNADMITTED','cutoff_date'=>'','pay_date'=>'','timezone_name'=>'','proof_sha256'=>hash('sha256','PAY-GLB-001-NATIVE-CALENDAR-UNKNOWN-V1'));
}

/** Minimize independently verified calendars while preserving V1 payroll/financial privacy semantics. */
function pay_glb_001_native_calendar_minimize(array $rows,$observed_at_utc,&$error=null)
{
    $legacy=array();$calendars=array();
    foreach($rows as $row){
        if(!isset($row['calendar']) || !is_array($row['calendar'])){$error='native_calendar_view_missing';return false;}
        $calendar=$row['calendar'];unset($row['calendar']);
        if(!pay_glb_001_exact_keys($calendar,array('state','cutoff_date','pay_date','timezone_name','proof_sha256'))
            || !pay_glb_001_sha256($calendar['proof_sha256'])
            || !in_array($calendar['state'],array('GOVERNED_OPERATIONAL_DATES','UNKNOWN_UNADMITTED'),true)){$error='native_calendar_view_invalid';return false;}
        if($calendar['state']==='GOVERNED_OPERATIONAL_DATES'){
            if(!pay_glb_001_native_reader_date($calendar['cutoff_date']) || !pay_glb_001_native_reader_date($calendar['pay_date'])
                || $calendar['cutoff_date']>$calendar['pay_date'] || !is_string($calendar['timezone_name'])
                || !in_array($calendar['timezone_name'],DateTimeZone::listIdentifiers(),true)){$error='native_calendar_view_dates_invalid';return false;}
        }elseif($calendar['cutoff_date']!=='' || $calendar['pay_date']!=='' || $calendar['timezone_name']!==''){
            $error='native_calendar_unknown_cannot_supply_dates';return false;
        }
        $legacy[]=$row;$calendars[]=$calendar;
    }
    $view=pay_glb_001_native_reader_minimize($legacy,$observed_at_utc,$error);if($view===false)return false;
    $view['schema']='PAY_GLB_001_NATIVE_MINIMUM_VIEW_V2';
    foreach($view['rows'] as $i=>&$row){
        $calendar=$calendars[$i];$known=$row['population_disclosure']==='AT_LEAST_FIVE' && $calendar['state']==='GOVERNED_OPERATIONAL_DATES';
        $row['calendar_state']=$known?'GOVERNED_OPERATIONAL_DATES':'UNKNOWN_UNADMITTED';
        $row['cutoff_date']=$known?$calendar['cutoff_date']:'';$row['pay_date']=$known?$calendar['pay_date']:'';
        $row['timezone_name']=$known?$calendar['timezone_name']:'';$row['cutoff_date_relation']='UNKNOWN';
        if($known){$date=(new DateTimeImmutable($observed_at_utc))->setTimezone(new DateTimeZone($calendar['timezone_name']))->format('Y-m-d');
            $row['cutoff_date_relation']=$calendar['cutoff_date']<$date?'PAST_DATE':($calendar['cutoff_date']===$date?'CURRENT_DATE':'FUTURE_DATE');}
        $row['deadline_precision']='CIVIL_DATE_ONLY';
    }unset($row);
    return $view;
}

/** Render V2 operational dates only after revalidating the public view and original V1 privacy rules. */
function pay_glb_001_native_calendar_render(array $view)
{
    if(!pay_glb_001_exact_keys($view,array('schema','observed_at_utc','rows')) || $view['schema']!=='PAY_GLB_001_NATIVE_MINIMUM_VIEW_V2'
        || !is_array($view['rows']) || count($view['rows'])<1 || count($view['rows'])>25 || !pay_glb_001_iso_utc($view['observed_at_utc']))return false;
    $legacy=$view;$legacy['schema']='PAY_GLB_001_NATIVE_MINIMUM_VIEW_V1';$cells=array();
    $fields=array('calendar_state','cutoff_date','pay_date','timezone_name','cutoff_date_relation','deadline_precision');
    foreach($view['rows'] as $i=>$row){
        foreach($fields as $field)if(!isset($row[$field]) || !is_string($row[$field]))return false;
        if($row['deadline_precision']!=='CIVIL_DATE_ONLY')return false;
        $known=$row['calendar_state']==='GOVERNED_OPERATIONAL_DATES';
        if($known){
            if($row['population_disclosure']!=='AT_LEAST_FIVE' || !pay_glb_001_native_reader_date($row['cutoff_date'])
                || !pay_glb_001_native_reader_date($row['pay_date']) || $row['cutoff_date']>$row['pay_date']
                || !in_array($row['timezone_name'],DateTimeZone::listIdentifiers(),true))return false;
            $date=(new DateTimeImmutable($view['observed_at_utc']))->setTimezone(new DateTimeZone($row['timezone_name']))->format('Y-m-d');
            $relation=$row['cutoff_date']<$date?'PAST_DATE':($row['cutoff_date']===$date?'CURRENT_DATE':'FUTURE_DATE');
            if($row['cutoff_date_relation']!==$relation)return false;
            $cells[]=array($row['cutoff_date'],$row['pay_date'],$row['timezone_name'],'Date only; exact time unknown');
        }else{
            if($row['calendar_state']!=='UNKNOWN_UNADMITTED' || $row['cutoff_date']!=='' || $row['pay_date']!==''
                || $row['timezone_name']!=='' || $row['cutoff_date_relation']!=='UNKNOWN')return false;
            $cells[]=array('Unknown','Unknown','Unknown','Exact time unknown');
        }
        foreach($fields as $field)unset($legacy['rows'][$i][$field]);
    }
    $html=pay_glb_001_native_reader_render($legacy);if($html===false)return false;
    $pieces=explode('</tr>',$html);if(count($pieces)!==count($cells)+2)return false;
    $pieces[0].='<th>Operational cutoff date</th><th>Operational pay date</th><th>IANA timezone</th><th>Deadline precision</th>';
    foreach($cells as $i=>$values)foreach($values as $value)$pieces[$i+1].='<td>'.htmlspecialchars($value,ENT_QUOTES,'UTF-8').'</td>';
    return implode('</tr>',$pieces);
}
