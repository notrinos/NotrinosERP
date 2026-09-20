<?php
/** HRM-TIM-001 deterministic work-pattern and approved-time contract. */
if (!defined('HRM_TIM_001_CONTRACT_VERSION')) define('HRM_TIM_001_CONTRACT_VERSION', '1.0');
if (!defined('HRM_TIM_001_PAYROLL_ADAPTER_VERSION')) define('HRM_TIM_001_PAYROLL_ADAPTER_VERSION', 'approved-time-pay-core-003-v1');
if (!defined('HRM_TIM_001_PAYROLL_CALC_AUTHORITY_ALLOWED')) define('HRM_TIM_001_PAYROLL_CALC_AUTHORITY_ALLOWED', false);
if (!defined('HRM_TIM_001_PAYMENT_AUTHORITY_ALLOWED')) define('HRM_TIM_001_PAYMENT_AUTHORITY_ALLOWED', false);
if (!defined('HRM_TIM_001_GL_AUTHORITY_ALLOWED')) define('HRM_TIM_001_GL_AUTHORITY_ALLOWED', false);
if (!defined('HRM_TIM_001_STATUTORY_AUTHORITY_ALLOWED')) define('HRM_TIM_001_STATUTORY_AUTHORITY_ALLOWED', false);
if (!defined('HRM_TIM_001_OVERTIME_QUALIFICATION_ALLOWED')) define('HRM_TIM_001_OVERTIME_QUALIFICATION_ALLOWED', false);

function hrm_tim_001_canonicalize($value)
{
    if (!is_array($value)) return $value;
    $keys = array_keys($value);
    $assoc = $keys !== range(0, count($value) - 1);
    if ($assoc) ksort($value, SORT_STRING);
    foreach ($value as $k => $v) $value[$k] = hrm_tim_001_canonicalize($v);
    return $value;
}
function hrm_tim_001_json($value)
{
    $json = json_encode(hrm_tim_001_canonicalize($value), JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    return is_string($json) ? $json : false;
}
function hrm_tim_001_hash($value)
{
    $json = hrm_tim_001_json($value);
    return $json === false ? false : hash('sha256', $json);
}
function hrm_tim_001_is_sha256($value){return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;}
function hrm_tim_001_token($value, $max=128)
{
    $value = trim((string)$value);
    return $value !== '' && strlen($value) <= (int)$max && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]*$/D', $value) ? $value : false;
}
function hrm_tim_001_date($value)
{
    if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2}$/D', $value) !== 1) return false;
    $d = DateTimeImmutable::createFromFormat('!Y-m-d', $value, new DateTimeZone('UTC'));
    return $d && $d->format('Y-m-d') === $value ? $value : false;
}
function hrm_tim_001_clock($value)
{
    if (!is_string($value) || preg_match('/^(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d$/D', $value) !== 1) return false;
    return $value;
}
function hrm_tim_001_timezone($value)
{
    $value = trim((string)$value);
    if ($value === '' || strlen($value) > 64) return false;
    try { new DateTimeZone($value); } catch (Exception $e) { return false; }
    return $value;
}
function hrm_tim_001_utc_datetime($value)
{
    if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', $value) !== 1) return false;
    $d = DateTimeImmutable::createFromFormat('!Y-m-d H:i:s', $value, new DateTimeZone('UTC'));
    return $d && $d->format('Y-m-d H:i:s') === $value ? $value : false;
}
function hrm_tim_001_local_utc_candidates($local, $timezone, &$error=null)
{
    $error = null;
    if (hrm_tim_001_timezone($timezone) === false || !is_string($local)
        || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', $local) !== 1) {
        $error = 'local_time_invalid'; return false;
    }
    $naive = DateTimeImmutable::createFromFormat('!Y-m-d H:i:s', $local, new DateTimeZone('UTC'));
    if (!$naive || $naive->format('Y-m-d H:i:s') !== $local) { $error='local_time_invalid'; return false; }
    $naive_ts = $naive->getTimestamp(); $tz = new DateTimeZone($timezone);
    $transitions = $tz->getTransitions($naive_ts - 172800, $naive_ts + 172800);
    $offsets = array();
    foreach ($transitions as $t) $offsets[(int)$t['offset']] = true;
    if (empty($offsets)) $offsets[(int)$tz->getOffset($naive)] = true;
    $out = array();
    foreach (array_keys($offsets) as $offset) {
        $candidate = $naive_ts - (int)$offset;
        $rendered = (new DateTimeImmutable('@'.$candidate))->setTimezone($tz)->format('Y-m-d H:i:s');
        if ($rendered !== $local) continue;
        $out[$candidate] = array('utc'=>gmdate('Y-m-d H:i:s', $candidate),'offset_minutes'=>(int)($offset/60));
    }
    ksort($out, SORT_NUMERIC); return array_values($out);
}
function hrm_tim_001_local_to_utc($local, $timezone, $offset_minutes=null, &$error=null)
{
    $candidates = hrm_tim_001_local_utc_candidates($local, $timezone, $error);
    if ($candidates === false) return false;
    if (count($candidates) === 0) { $error='nonexistent_local_time'; return false; }
    if ($offset_minutes !== null) {
        if (!is_int($offset_minutes) && (!is_string($offset_minutes) || preg_match('/^-?[0-9]+$/D',$offset_minutes)!==1)) { $error='offset_invalid'; return false; }
        $wanted=(int)$offset_minutes; if($wanted < -1440 || $wanted > 1440){$error='offset_invalid';return false;} $matches=array();
        foreach ($candidates as $c) if ((int)$c['offset_minutes']===$wanted) $matches[]=$c;
        if (count($matches)!==1) { $error='offset_does_not_resolve_local_time'; return false; }
        return $matches[0];
    }
    if (count($candidates)!==1) { $error='ambiguous_local_time'; return false; }
    return $candidates[0];
}
function hrm_tim_001_resolve_local_segment($date,$start,$end,$crosses_midnight,$timezone,$start_offset_minutes=null,$end_offset_minutes=null,&$error=null)
{
    $error=null; if(hrm_tim_001_date($date)===false||hrm_tim_001_clock($start)===false||hrm_tim_001_clock($end)===false||hrm_tim_001_timezone($timezone)===false){$error='segment_boundary_invalid';return false;}
    $end_date=$date; if($crosses_midnight){$d=new DateTimeImmutable($date,new DateTimeZone('UTC'));$end_date=$d->modify('+1 day')->format('Y-m-d');}
    $s=hrm_tim_001_local_to_utc($date.' '.$start,$timezone,$start_offset_minutes,$error);if($s===false)return false;
    $e=hrm_tim_001_local_to_utc($end_date.' '.$end,$timezone,$end_offset_minutes,$error);if($e===false)return false;
    $st=strtotime($s['utc'].' UTC');$et=strtotime($e['utc'].' UTC');if($et<=$st){$error='segment_non_positive_duration';return false;}
    $seconds=$et-$st;if($seconds%60!==0){$error='segment_subminute_duration';return false;}
    return array('start_utc'=>$s['utc'],'end_utc'=>$e['utc'],'duration_minutes'=>(int)($seconds/60),'start_offset_minutes'=>$s['offset_minutes'],'end_offset_minutes'=>$e['offset_minutes']);
}
function hrm_tim_001_minutes_of_day($clock){if(hrm_tim_001_clock($clock)===false)return false;$p=array_map('intval',explode(':',$clock));if($p[2]!==0)return false;return $p[0]*60+$p[1];}
function hrm_tim_001_work_pattern_descriptor(array $x,&$error=null)
{
    $error=null;foreach(array('worker_id','employment_id','assignment_id','legal_entity_id','effective_from','timezone_name','cycle_days','pattern_code','segments','created_by') as $k)if(!array_key_exists($k,$x)){$error='work_pattern_field_missing';return false;}
    foreach(array('worker_id','employment_id','assignment_id','legal_entity_id','created_by') as $k)if((int)$x[$k]<=0){$error='work_pattern_scope_invalid';return false;}
    $from=hrm_tim_001_date($x['effective_from']);$to=empty($x['effective_to'])?null:hrm_tim_001_date($x['effective_to']);if($from===false||($to!==null&&($to===false||$to<$from))){$error='work_pattern_interval_invalid';return false;}
    $tz=hrm_tim_001_timezone($x['timezone_name']);$code=hrm_tim_001_token($x['pattern_code'],64);$source=isset($x['source_kind'])?hrm_tim_001_token($x['source_kind'],32):'governed';$cycle=(int)$x['cycle_days'];if($tz===false||$code===false||$source!=='governed'||$cycle<1||$cycle>366||!is_array($x['segments'])||count($x['segments'])===0){$error='work_pattern_definition_invalid';return false;}
    $segments=array();$intervals=array();$seqs=array();
    foreach($x['segments'] as $seg){foreach(array('cycle_day','segment_sequence','local_start_time','local_end_time','crosses_midnight','paid_minutes','break_minutes') as $k)if(!array_key_exists($k,$seg)){$error='work_pattern_segment_field_missing';return false;}$day=(int)$seg['cycle_day'];$seq=(int)$seg['segment_sequence'];$st=hrm_tim_001_minutes_of_day($seg['local_start_time']);$en=hrm_tim_001_minutes_of_day($seg['local_end_time']);$cross=(int)$seg['crosses_midnight']===1?1:0;$paid=(int)$seg['paid_minutes'];$break=(int)$seg['break_minutes'];$rest=array_key_exists('rest_minutes',$seg)?(int)$seg['rest_minutes']:0;if($day<1||$day>$cycle||$seq<1||$st===false||$en===false||$paid<0||$break<0||$rest<0){$error='work_pattern_segment_invalid';return false;}$key=$day.':'.$seq;if(isset($seqs[$key])){$error='work_pattern_segment_duplicate_sequence';return false;}$seqs[$key]=true;$start=(($day-1)*1440)+$st;$end=(($day-1)*1440)+$en+($cross?1440:0);if($end<=$start||$paid+$break+$rest>$end-$start){$error='work_pattern_segment_duration_invalid';return false;}$payload=array('cycle_day'=>$day,'segment_sequence'=>$seq,'local_start_time'=>$seg['local_start_time'],'local_end_time'=>$seg['local_end_time'],'crosses_midnight'=>$cross,'paid_minutes'=>$paid,'break_minutes'=>$break,'rest_minutes'=>$rest);$payload['segment_sha256']=hrm_tim_001_hash($payload);$segments[]=$payload;$intervals[]=array($start,$end,$payload['segment_sha256']);}
    usort($intervals,function($a,$b){return $a[0]===$b[0]?strcmp($a[2],$b[2]):($a[0]<$b[0]?-1:1);});$cycle_minutes=$cycle*1440;$extended=$intervals;foreach($intervals as $iv)$extended[]=array($iv[0]+$cycle_minutes,$iv[1]+$cycle_minutes,$iv[2]);usort($extended,function($a,$b){return $a[0]===$b[0]?strcmp($a[2],$b[2]):($a[0]<$b[0]?-1:1);});for($i=1;$i<count($extended);$i++)if($extended[$i][0]<$extended[$i-1][1]&&$extended[$i][0]<$cycle_minutes*2&&$extended[$i-1][0]<$cycle_minutes){$error='work_pattern_segment_overlap';return false;}
    usort($segments,function($a,$b){return $a['cycle_day']===$b['cycle_day']?($a['segment_sequence']<$b['segment_sequence']?-1:1):($a['cycle_day']<$b['cycle_day']?-1:1);});
    $p=array('worker_id'=>(int)$x['worker_id'],'employment_id'=>(int)$x['employment_id'],'assignment_id'=>(int)$x['assignment_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],'effective_from'=>$from,'effective_to'=>$to,'timezone_name'=>$tz,'cycle_days'=>$cycle,'pattern_code'=>$code,'source_kind'=>$source,'predecessor_pattern_id'=>empty($x['predecessor_pattern_id'])?null:(int)$x['predecessor_pattern_id'],'segments'=>$segments,'created_by'=>(int)$x['created_by']);$p['pattern_sha256']=hrm_tim_001_hash($p);return $p;
}
function hrm_tim_001_raw_event_descriptor(array $x,&$error=null)
{
    $error=null;
    foreach(array('worker_id','employment_id','assignment_id','legal_entity_id','source_channel','source_event_key','recorded_at_utc','effective_local_at','timezone_name','effective_utc_at','utc_offset_minutes','event_type','source_payload_sha256','ingest_command_key','recorded_by') as $k)
        if(!array_key_exists($k,$x)){$error='raw_event_field_missing';return false;}
    foreach(array('worker_id','employment_id','assignment_id','legal_entity_id','recorded_by') as $k)
        if((int)$x[$k]<=0){$error='raw_event_scope_invalid';return false;}
    $channel=hrm_tim_001_token($x['source_channel'],32);
    $event=hrm_tim_001_token($x['source_event_key'],128);
    $command=hrm_tim_001_token($x['ingest_command_key'],128);
    $tz=hrm_tim_001_timezone($x['timezone_name']);
    $recorded=hrm_tim_001_utc_datetime($x['recorded_at_utc']);
    $effective_utc=hrm_tim_001_utc_datetime($x['effective_utc_at']);
    if(!is_int($x['utc_offset_minutes']) && (!is_string($x['utc_offset_minutes']) || preg_match('/^-?[0-9]+$/D',$x['utc_offset_minutes'])!==1)){$error='raw_event_offset_invalid';return false;}
    $offset=(int)$x['utc_offset_minutes'];
    if($channel===false||$event===false||$command===false||$tz===false||$recorded===false||$effective_utc===false||$offset < -1440||$offset > 1440||!hrm_tim_001_is_sha256($x['source_payload_sha256'])){$error='raw_event_identity_invalid';return false;}
    if(!is_string($x['effective_local_at'])||preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D',$x['effective_local_at'])!==1){$error='raw_event_local_time_invalid';return false;}
    $resolve_error=null;$resolved=hrm_tim_001_local_to_utc($x['effective_local_at'],$tz,$offset,$resolve_error);
    if($resolved===false||$resolved['utc']!==$effective_utc){$error='raw_event_time_resolution_mismatch';return false;}
    $type=(string)$x['event_type'];
    if(!in_array($type,array('clock_in','clock_out','manual_anchor','legacy_attendance'),true)){$error='raw_event_type_invalid';return false;}
    $p=array(
        'worker_id'=>(int)$x['worker_id'],'employment_id'=>(int)$x['employment_id'],'assignment_id'=>(int)$x['assignment_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],
        'source_channel'=>$channel,'source_event_key'=>$event,'recorded_at_utc'=>$recorded,'effective_local_at'=>$x['effective_local_at'],'timezone_name'=>$tz,
        'effective_utc_at'=>$effective_utc,'utc_offset_minutes'=>$offset,'event_type'=>$type,'source_payload_sha256'=>$x['source_payload_sha256'],
        'predecessor_event_id'=>empty($x['predecessor_event_id'])?null:(int)$x['predecessor_event_id']);
    /* Event identity binds source fact + resolved time, not retry command/actor custody. */
    $event_sha256=hrm_tim_001_hash($p);
    $p['ingest_command_key']=$command;$p['recorded_by']=(int)$x['recorded_by'];$p['event_sha256']=$event_sha256;return $p;
}
function hrm_tim_001_reviewed_entry_descriptor(array $x,&$error=null)
{
    $error=null;
    foreach(array('worker_id','employment_id','assignment_id','legal_entity_id','work_date','period_start_utc','period_end_utc','source_event_set_sha256','work_pattern_sha256','allocation_set_sha256','review_command_key','reviewer_id') as $k)
        if(!array_key_exists($k,$x)){$error='review_field_missing';return false;}
    foreach(array('worker_id','employment_id','assignment_id','legal_entity_id','reviewer_id') as $k)
        if((int)$x[$k]<=0){$error='review_scope_invalid';return false;}
    $date=hrm_tim_001_date($x['work_date']);$s=hrm_tim_001_utc_datetime($x['period_start_utc']);$e=hrm_tim_001_utc_datetime($x['period_end_utc']);$cmd=hrm_tim_001_token($x['review_command_key'],128);
    if($date===false||$s===false||$e===false||$cmd===false||!hrm_tim_001_is_sha256($x['source_event_set_sha256'])||!hrm_tim_001_is_sha256($x['work_pattern_sha256'])||!hrm_tim_001_is_sha256($x['allocation_set_sha256'])){$error='review_identity_invalid';return false;}
    $seconds=strtotime($e.' UTC')-strtotime($s.' UTC');if($seconds<=0||$seconds%60!==0||$seconds>172800){$error='review_duration_invalid';return false;}
    $p=array('worker_id'=>(int)$x['worker_id'],'employment_id'=>(int)$x['employment_id'],'assignment_id'=>(int)$x['assignment_id'],'legal_entity_id'=>(int)$x['legal_entity_id'],'work_date'=>$date,'period_start_utc'=>$s,'period_end_utc'=>$e,'duration_minutes'=>(int)($seconds/60),'source_event_set_sha256'=>$x['source_event_set_sha256'],'work_pattern_sha256'=>$x['work_pattern_sha256'],'allocation_set_sha256'=>$x['allocation_set_sha256'],'review_command_key'=>$cmd,'predecessor_entry_id'=>empty($x['predecessor_entry_id'])?null:(int)$x['predecessor_entry_id'],'reviewer_id'=>(int)$x['reviewer_id']);$p['review_sha256']=hrm_tim_001_hash($p);return $p;
}
function hrm_tim_001_allocation_descriptor(array $x,&$error=null)
{
    $error=null;foreach(array('reviewed_entry_id','payroll_relationship_id','assignment_id','allocation_category','quantity_minutes') as $k)if(!array_key_exists($k,$x)){$error='allocation_field_missing';return false;}
    $rid=(int)$x['reviewed_entry_id'];$rel=(int)$x['payroll_relationship_id'];$assignment=(int)$x['assignment_id'];$minutes=(int)$x['quantity_minutes'];$cat=(string)$x['allocation_category'];
    if($rid<0||$rel<=0||$assignment<=0||$minutes<=0||!in_array($cat,array('worked_time','legacy_regular','legacy_overtime','paid_break'),true)){$error='allocation_invalid';return false;}
    $p=array('reviewed_entry_id'=>$rid,'payroll_relationship_id'=>$rel,'assignment_id'=>$assignment,'allocation_category'=>$cat,'quantity_minutes'=>$minutes,'unit_code'=>'minute');$p['allocation_sha256']=hrm_tim_001_hash($p);return $p;
}
function hrm_tim_001_allocation_set_descriptor(array $rows,&$error=null)
{
    $error=null;if(!$rows){$error='allocation_set_empty';return false;}$seen=array();$norm=array();$total=0;foreach($rows as $r){if(!isset($r['allocation_sha256'],$r['quantity_minutes'])||!hrm_tim_001_is_sha256($r['allocation_sha256'])||(int)$r['quantity_minutes']<=0){$error='allocation_set_invalid';return false;}if(isset($seen[$r['allocation_sha256']])){$error='allocation_set_duplicate';return false;}$seen[$r['allocation_sha256']]=true;$norm[]=$r['allocation_sha256'];$total+=(int)$r['quantity_minutes'];}sort($norm,SORT_STRING);return array('allocation_count'=>count($norm),'total_minutes'=>$total,'allocation_set_sha256'=>hrm_tim_001_hash($norm));
}
function hrm_tim_001_allocation_set(array $rows,&$error=null){return hrm_tim_001_allocation_set_descriptor($rows,$error);}
function hrm_tim_001_approval_descriptor(array $x,&$error=null)
{
    $error=null;foreach(array('reviewed_entry_id','review_sha256','allocation_set_sha256','approval_evidence_sha256','approval_command_key','reviewer_id','approver_id') as $k)if(!array_key_exists($k,$x)){$error='approval_field_missing';return false;}
    $review=(int)$x['reviewed_entry_id'];$reviewer=(int)$x['reviewer_id'];$approver=(int)$x['approver_id'];$cmd=hrm_tim_001_token($x['approval_command_key'],128);
    if($review<=0||$reviewer<=0||$approver<=0||$reviewer===$approver||$cmd===false||!hrm_tim_001_is_sha256($x['review_sha256'])||!hrm_tim_001_is_sha256($x['allocation_set_sha256'])||!hrm_tim_001_is_sha256($x['approval_evidence_sha256'])){$error=$reviewer===$approver?'self_approval_denied':'approval_invalid';return false;}
    $p=array('reviewed_entry_id'=>$review,'review_sha256'=>$x['review_sha256'],'allocation_set_sha256'=>$x['allocation_set_sha256'],'approval_evidence_sha256'=>$x['approval_evidence_sha256'],'approval_command_key'=>$cmd,'reviewer_id'=>$reviewer,'approver_id'=>$approver);$p['approval_sha256']=hrm_tim_001_hash($p);return $p;
}
function hrm_tim_001_payroll_mapping_descriptor(array $x,&$error=null)
{
    $error=null;foreach(array('legal_entity_id','allocation_category','element_version_id','input_definition_id','unit_token','effective_from','created_by') as $k)if(!array_key_exists($k,$x)){$error='mapping_field_missing';return false;}
    $cat=(string)$x['allocation_category'];$unit=(string)$x['unit_token'];$from=hrm_tim_001_date($x['effective_from']);$to=empty($x['effective_to'])?null:hrm_tim_001_date($x['effective_to']);$pred=empty($x['predecessor_mapping_id'])?null:(int)$x['predecessor_mapping_id'];
    if((int)$x['legal_entity_id']<=0||(int)$x['element_version_id']<=0||(int)$x['input_definition_id']<=0||(int)$x['created_by']<=0||!in_array($cat,array('worked_time','legacy_regular','legacy_overtime','paid_break'),true)||!in_array($unit,array('minutes','hours'),true)||$from===false||($to!==null&&($to===false||$to<$from))){$error='mapping_invalid';return false;}
    $p=array('legal_entity_id'=>(int)$x['legal_entity_id'],'allocation_category'=>$cat,'element_version_id'=>(int)$x['element_version_id'],'input_definition_id'=>(int)$x['input_definition_id'],'unit_token'=>$unit,'effective_from'=>$from,'effective_to'=>$to,'predecessor_mapping_id'=>$pred,'created_by'=>(int)$x['created_by']);$p['mapping_sha256']=hrm_tim_001_hash($p);return $p;
}
function hrm_tim_001_mapping_descriptor(array $x,&$error=null){if(isset($x['time_category'])&&!isset($x['allocation_category']))$x['allocation_category']=$x['time_category'];return hrm_tim_001_payroll_mapping_descriptor($x,$error);}
function hrm_tim_001_decimal_hours_to_minutes_exact($value,&$error=null)
{
    $error=null;$s=trim((string)$value);if(!preg_match('/^([0-9]+)(?:\.([0-9]{1,8}))?$/D',$s,$m)){$error='legacy_hours_invalid';return false;}$whole=(int)$m[1];$frac=isset($m[2])?str_pad($m[2],8,'0'):str_repeat('0',8);$scaled=$whole*100000000+(int)$frac;$num=$scaled*60;if($num%100000000!==0){$error='legacy_hours_not_exact_minute';return false;}return (int)($num/100000000);
}
function hrm_tim_001_minutes_to_decimal_hours($minutes)
{
    if(!is_int($minutes)&&(!is_string($minutes)||preg_match('/^[0-9]+$/D',$minutes)!==1))return false;$minutes=(int)$minutes;if($minutes<0)return false;$scaled_num=$minutes*100000000;$scaled=intdiv($scaled_num,60);$rem=$scaled_num%60;if($rem*2>=60)$scaled++;$whole=intdiv($scaled,100000000);$frac=$scaled%100000000;return sprintf('%d.%08d',$whole,$frac);
}
?>
