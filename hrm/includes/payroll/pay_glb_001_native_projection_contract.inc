<?php
/**
 * S39 explicit native CORE-010 projection and approval-intent contract.
 * Pure bytes/arithmetic only. Existing v1 projection, approval, cutover and
 * financial workflows remain authoritative and unchanged. No runtime caller.
 */
require_once dirname(__FILE__).'/pay_glb_001_native_execution_contract.inc';
require_once dirname(__FILE__).'/pay_core_004_contract.inc';
require_once dirname(__FILE__).'/pay_core_010_contract.inc';

/** Name the forward projection independently of legacy-payslip-v1. */
function pay_glb_001_native_projection_version() { return 'native-result-v2'; }

/** Hash new projection bytes in an explicit versioned domain. */
function pay_glb_001_native_projection_hash($kind,array $value)
{
    if (!in_array($kind,array('header','details','projection','intent'),true)) return false;
    $json=hrm_pay_core_004_canonical_json($value);
    return $json===false?false:hash('sha256',"PAY-GLB-001|native-projection-".$kind."|v2\n".$json);
}

/** Accept canonical physical CORE-004 DECIMAL(24,8), without rounding. */
function pay_glb_001_native_projection_decimal($value)
{
    return is_string($value) && preg_match('/^-?(0|[1-9][0-9]{0,15})\.[0-9]{8}$/D',$value)===1
        && $value!=='-0.00000000';
}

/** Losslessly transfer an exact sum into existing intent DECIMAL(20,6). */
function pay_glb_001_native_projection_intent_decimal($value,&$error=null)
{
    $error=null;
    if (!is_string($value) || preg_match('/^-?(0|[1-9][0-9]{0,13})\.[0-9]{6}00$/D',$value)!==1
        || $value==='-0.00000000') { $error='native_projection_intent_decimal_unrepresentable';return false; }
    return substr($value,0,-2);
}

/** Validate real helper identifiers before any legacy CORE integer conversion. */
function pay_glb_001_native_projection_helper_id($value,&$error=null)
{
    if (!is_int($value) && !is_string($value)) { $error='native_projection_id_type_invalid';return false; }
    return pay_glb_001_native_helper_id((string)$value,$error);
}

/** Check typed positive selection scope and exact execution-byte identity. */
function pay_glb_001_native_projection_selection(array $execution,array $selection,&$error=null)
{
    if (!pay_glb_001_native_receipt_validate('execution',$execution,$error)
        || !pay_glb_001_native_receipt_validate('selection',$selection,$error)
        || $execution['source_kind']!=='GOVERNED_NATIVE_V1' || $selection['event_kind']!=='select'
        || pay_glb_001_native_receipt_sha256_v1('execution',$execution,$error)!==$selection['native_execution_sha256']) {
        $error='native_projection_positive_selection_required';return false;
    }
    foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],0,6) as $field)
        if ($execution[$field]!==$selection[$field]) { $error='native_projection_selection_scope_mismatch';return false; }
    return true;
}

/**
 * Build one append-only projection artifact from actual immutable result bytes.
 * The caller must authenticate current selection, complete membership, source,
 * mapped employee and cutover. Hashes here do not establish those authorities.
 */
function pay_glb_001_native_projection_candidate_v2(array $execution,array $selection,array $member,
    array $snapshot,array $result,array $lines,array $traces,$employee_sha,&$error=null)
{
    $error=null;
    if (!pay_glb_001_native_projection_selection($execution,$selection,$error)
        || !pay_glb_001_native_receipt_validate('member',$member,$error)
        || !hrm_pay_core_004_is_sha256($employee_sha) || $member['assignment_id']===null
        || $member['native_execution_key']!==$execution['native_execution_key']
        || count($lines)<1 || count($lines)>4096 || count($traces)<1 || count($traces)>65536) {
        $error='native_projection_member_or_material_invalid';return false;
    }
    foreach (array('payroll_relationship_id','assignment_id','calendar_period_id','legal_entity_id') as $field) {
        if (!isset($snapshot[$field]) || pay_glb_001_native_projection_helper_id($snapshot[$field],$error)===false
            || (string)$snapshot[$field]!==($field==='assignment_id'||$field==='payroll_relationship_id'?$member[$field]:$execution[$field])) {
            $error='native_projection_snapshot_scope_invalid';return false;
        }
    }
    foreach (array('core004_result_id','core004_snapshot_id','assignment_id','payroll_relationship_id') as $field)
        if (pay_glb_001_native_helper_id($member[$field],$error)===false) return false;
    foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],0,6) as $field)
        if ($field!=='company_id' && pay_glb_001_native_helper_id($execution[$field],$error)===false) return false;
    $trace_sequence=0;
    foreach ($traces as $trace) {
        if (!is_array($trace) || !isset($trace['trace_seq'])
            || pay_glb_001_native_projection_helper_id($trace['trace_seq'],$error)!==++$trace_sequence) {
            $error='native_projection_trace_order_invalid';return false;
        }
    }
    $snapshot_sha=hrm_pay_core_004_snapshot_sha256($snapshot);
    $actual=hrm_pay_core_004_verify_result_bundle($snapshot_sha,$result,$lines,$traces);
    if ($snapshot_sha!==$member['snapshot_sha256'] || $actual===false || $actual!==$member['result_sha256']
        || !isset($result['result_sha256']) || $actual!==$result['result_sha256']
        || $result['line_set_sha256']!==$member['line_set_sha256'] || $result['trace_set_sha256']!==$member['trace_set_sha256']
        || $snapshot['currency']!==$execution['currency_code'] || $result['currency']!==$execution['currency_code']) {
        $error='native_projection_actual_bundle_mismatch';return false;
    }
    foreach (array('gross_amount','deduction_amount','net_amount') as $field)
        if (!pay_glb_001_native_projection_decimal($result[$field])) { $error='native_projection_amount_invalid';return false; }
    if (!pay_glb_001_native_totals_match($result['gross_amount'],$result['deduction_amount'],$result['net_amount'])) {
        $error='native_projection_result_totals_mismatch';return false;
    }
    $gross='0.00000000';$deduction='0.00000000';$details=array();$sequence=0;
    foreach ($lines as $line) {
        if (!isset($line['line_no'],$line['element_version_id']) || pay_glb_001_native_projection_helper_id($line['line_no'],$error)!==++$sequence
            || pay_glb_001_native_projection_helper_id($line['element_version_id'],$error)===false || $line['currency']!==$execution['currency_code']) {
            $error='native_projection_line_scope_or_order_invalid';return false;
        }
        foreach (array('amount','quantity','rate') as $field)
            if (!pay_glb_001_native_projection_decimal($line[$field])) { $error='native_projection_line_decimal_invalid';return false; }
        if ($line['line_kind']==='earning') $gross=pay_glb_001_native_decimal_add($gross,$line['amount'],$error);
        elseif ($line['line_kind']==='deduction') $deduction=pay_glb_001_native_decimal_add($deduction,$line['amount'],$error);
        if ($gross===false || $deduction===false) return false;
        $details[]=array('line_no'=>(string)$line['line_no'],'element_version_id'=>(string)$line['element_version_id'],
            'line_kind'=>$line['line_kind'],'currency'=>$line['currency'],'amount'=>$line['amount'],'quantity'=>$line['quantity'],
            'rate'=>$line['rate'],'source_sha256'=>$line['source_sha256'],'line_sha256'=>hrm_pay_core_004_result_line_sha256($line));
    }
    if ($gross!==$result['gross_amount'] || $deduction!==$result['deduction_amount']) {
        $error='native_projection_line_totals_mismatch';return false;
    }
    $binding=array('selection_sha256'=>pay_glb_001_native_receipt_sha256_v1('selection',$selection,$error),
        'native_execution_sha256'=>$selection['native_execution_sha256'],
        'member_sha256'=>pay_glb_001_native_receipt_sha256_v1('member',$member,$error));
    $header=array('projection_contract_version'=>pay_glb_001_native_projection_version())+$binding;
    foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],0,6) as $field) $header[$field]=$execution[$field];
    $header+=array('payroll_relationship_id'=>$member['payroll_relationship_id'],'assignment_id'=>$member['assignment_id'],
        'employee_id_sha256'=>$employee_sha,'result_id'=>$member['core004_result_id'],'result_sha256'=>$actual,
        'snapshot_id'=>$member['core004_snapshot_id'],'snapshot_sha256'=>$snapshot_sha,'currency_code'=>$result['currency'],
        'gross_8'=>$result['gross_amount'],'deduction_8'=>$result['deduction_amount'],'net_8'=>$result['net_amount']);
    $d=array();foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],1,5) as $field) $d[$field]=(int)$execution[$field];
    $d+=array('payroll_relationship_id'=>(int)$member['payroll_relationship_id'],'assignment_id'=>(int)$member['assignment_id'],
        'employee_id_sha256'=>$employee_sha,'result_id'=>(int)$member['core004_result_id'],'result_sha256'=>$actual,
        'projection_contract_version'=>pay_glb_001_native_projection_version(),
        'header_sha256'=>pay_glb_001_native_projection_hash('header',$header),'detail_set_sha256'=>pay_glb_001_native_projection_hash('details',$details));
    $d['projection_sha256']=pay_glb_001_native_projection_hash('projection',$d);
    return array('binding'=>$binding,'member'=>$member,'header'=>$header,'details'=>$details,'descriptor'=>$d);
}

/** Derive the existing sorted ID/hash set with exact totals and physical intent bounds. */
function pay_glb_001_native_projection_result_set_v2(array $rows,&$error=null)
{
    $error=null;
    if (count($rows)<1 || count($rows)>4096 || array_keys($rows)!==range(0,count($rows)-1)) {
        $error='native_projection_result_set_bound';return false;
    }
    $items=array();$seen=array();$currency=null;$sums=array('gross_amount'=>'0.00000000','deduction_amount'=>'0.00000000','net_amount'=>'0.00000000');
    foreach ($rows as $row) {
        if (!is_array($row) || array_keys($row)!==array('result_id','result_sha256','currency','gross_amount','deduction_amount','net_amount')
            || pay_glb_001_native_helper_id($row['result_id'],$error)===false || !hrm_pay_core_004_is_sha256($row['result_sha256'])
            || isset($seen[$row['result_id']]) || !is_string($row['currency']) || preg_match('/^[A-Z]{3}$/D',$row['currency'])!==1
            || ($currency!==null && $currency!==$row['currency'])) { $error='native_projection_result_set_row_invalid';return false; }
        $seen[$row['result_id']]=true;$currency=$row['currency'];
        foreach ($sums as $field=>$sum) {
            if (!pay_glb_001_native_projection_decimal($row[$field])) { $error='native_projection_result_set_decimal_invalid';return false; }
            $sums[$field]=pay_glb_001_native_decimal_add($sum,$row[$field],$error);if ($sums[$field]===false) return false;
        }
        if (!pay_glb_001_native_totals_match($row['gross_amount'],$row['deduction_amount'],$row['net_amount'])) {
            $error='native_projection_result_row_totals_mismatch';return false;
        }
        $items[]=array('result_id'=>(int)$row['result_id'],'result_sha256'=>$row['result_sha256']);
    }
    usort($items,function($a,$b){return $a['result_id']===$b['result_id']?0:($a['result_id']<$b['result_id']?-1:1);});
    $out=array('result_count'=>count($items),'result_set_sha256'=>hrm_pay_core_010_hash($items),'results'=>$items,'currency'=>$currency,'totals_8'=>$sums);
    foreach ($sums as $field=>$sum) {
        $out[$field]=pay_glb_001_native_projection_intent_decimal($sum,$error);if ($out[$field]===false) return false;
    }
    return $out;
}

/**
 * Build only a v2 intent descriptor for exact complete selected projections.
 * Actual CORE-010 insertion, cutover, central workflow and approval binding
 * must be verified transactionally by the future runtime consumer.
 */
function pay_glb_001_native_projection_approval_intent_v2(array $execution,array $selection,array $rows,
    array $projections,$cutover_sha,$key,$maker,&$error=null)
{
    $error=null;
    if (!pay_glb_001_native_projection_selection($execution,$selection,$error)
        || !hrm_pay_core_004_is_sha256($cutover_sha) || !hrm_pay_core_004_is_sha256($key)
        || pay_glb_001_native_helper_id($maker,$error)===false || !pay_glb_001_native_unsigned($maker,'4294967295')
        || count($projections)!==(int)$execution['member_count'] || array_keys($projections)!==range(0,count($projections)-1)) {
        $error='native_projection_intent_scope_invalid';return false;
    }
    $set=pay_glb_001_native_projection_result_set_v2($rows,$error);if ($set===false) return false;
    if ($set['result_count']!==(int)$execution['member_count'] || $set['result_set_sha256']!==$execution['core010_result_set_sha256']
        || $set['currency']!==$execution['currency_code'] || array_values($set['totals_8'])!==array($execution['gross_8'],$execution['deduction_8'],$execution['net_8'])) {
        $error='native_projection_complete_execution_mismatch';return false;
    }
    $members=array();$projection_rows=array();$seen=array();$row_by_id=array();foreach ($rows as $row) $row_by_id[$row['result_id']]=$row;
    foreach ($projections as $artifact) {
        if (!is_array($artifact) || array_keys($artifact)!==array('binding','member','header','details','descriptor')) {
            $error='native_projection_intent_artifact_invalid';return false;
        }
        $member=$artifact['member'];$header=$artifact['header'];$d=$artifact['descriptor'];
        $header_fields=explode(',','projection_contract_version,selection_sha256,native_execution_sha256,member_sha256,company_id,legal_entity_id,payroll_group_id,group_version_id,payroll_period_id,calendar_period_id,payroll_relationship_id,assignment_id,employee_id_sha256,result_id,result_sha256,snapshot_id,snapshot_sha256,currency_code,gross_8,deduction_8,net_8');
        $descriptor_fields=explode(',','legal_entity_id,payroll_group_id,group_version_id,payroll_period_id,calendar_period_id,payroll_relationship_id,assignment_id,employee_id_sha256,result_id,result_sha256,projection_contract_version,header_sha256,detail_set_sha256,projection_sha256');
        if (!is_array($member) || !is_array($header) || !is_array($d) || array_keys($header)!==$header_fields || array_keys($d)!==$descriptor_fields
            || !pay_glb_001_native_receipt_validate('member',$member,$error) || $member['assignment_id']===null
            || $member['native_execution_key']!==$execution['native_execution_key'] || isset($seen[$member['core004_result_id']])
            || !isset($row_by_id[$member['core004_result_id']])) { $error='native_projection_intent_member_invalid';return false; }
        $seen[$member['core004_result_id']]=true;
        $binding=array('selection_sha256'=>pay_glb_001_native_receipt_sha256_v1('selection',$selection,$error),
            'native_execution_sha256'=>$selection['native_execution_sha256'],'member_sha256'=>pay_glb_001_native_receipt_sha256_v1('member',$member,$error));
        $body=$d;unset($body['projection_sha256']);
        if ($artifact['binding']!==$binding || $d['projection_contract_version']!==pay_glb_001_native_projection_version()
            || $d['result_id']!==pay_glb_001_native_helper_id($member['core004_result_id'],$error) || $d['result_sha256']!==$member['result_sha256']
            || $d['assignment_id']!==pay_glb_001_native_helper_id($member['assignment_id'],$error)
            || $d['payroll_relationship_id']!==pay_glb_001_native_helper_id($member['payroll_relationship_id'],$error)
            || !hrm_pay_core_004_is_sha256($d['employee_id_sha256']) || $header['employee_id_sha256']!==$d['employee_id_sha256']
            || $header['company_id']!==$execution['company_id'] || $header['projection_contract_version']!==pay_glb_001_native_projection_version()
            || $header['payroll_relationship_id']!==$member['payroll_relationship_id'] || $header['assignment_id']!==$member['assignment_id']
            || $header['snapshot_id']!==$member['core004_snapshot_id'] || $header['snapshot_sha256']!==$member['snapshot_sha256']
            || $d['header_sha256']!==pay_glb_001_native_projection_hash('header',$header)
            || $d['detail_set_sha256']!==pay_glb_001_native_projection_hash('details',$artifact['details'])
            || $d['projection_sha256']!==pay_glb_001_native_projection_hash('projection',$body)) {
            $error='native_projection_intent_artifact_hash_invalid';return false;
        }
        foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],1,5) as $field)
            if ($d[$field]!==pay_glb_001_native_helper_id($execution[$field],$error) || $header[$field]!==$execution[$field]) {
                $error='native_projection_intent_projection_scope_invalid';return false;
            }
        foreach ($binding as $field=>$value) if ($header[$field]!==$value) { $error='native_projection_header_binding_invalid';return false; }
        if (!is_array($artifact['details']) || count($artifact['details'])<1 || count($artifact['details'])>4096
            || array_keys($artifact['details'])!==range(0,count($artifact['details'])-1)) { $error='native_projection_details_bound';return false; }
        $line_hashes=array();$gross='0.00000000';$deduction='0.00000000';$sequence=0;
        foreach ($artifact['details'] as $detail) {
            if (!is_array($detail) || array_keys($detail)!==explode(',','line_no,element_version_id,line_kind,currency,amount,quantity,rate,source_sha256,line_sha256')
                || pay_glb_001_native_helper_id($detail['line_no'],$error)!==++$sequence
                || pay_glb_001_native_helper_id($detail['element_version_id'],$error)===false
                || $detail['currency']!==$execution['currency_code']) { $error='native_projection_detail_scope_invalid';return false; }
            foreach (array('amount','quantity','rate') as $field)
                if (!pay_glb_001_native_projection_decimal($detail[$field])) { $error='native_projection_detail_decimal_invalid';return false; }
            $line_sha=hrm_pay_core_004_result_line_sha256($detail);
            if ($line_sha===false || $line_sha!==$detail['line_sha256']) { $error='native_projection_detail_line_hash_invalid';return false; }
            $line_hashes[]=$line_sha;
            if ($detail['line_kind']==='earning') $gross=pay_glb_001_native_decimal_add($gross,$detail['amount'],$error);
            elseif ($detail['line_kind']==='deduction') $deduction=pay_glb_001_native_decimal_add($deduction,$detail['amount'],$error);
            if ($gross===false || $deduction===false) return false;
        }
        if (hrm_pay_core_004_hash_set($line_hashes)!==$member['line_set_sha256'] || $gross!==$header['gross_8'] || $deduction!==$header['deduction_8']) {
            $error='native_projection_detail_member_mismatch';return false;
        }
        $row=$row_by_id[$member['core004_result_id']];
        foreach (array('result_id','result_sha256','currency_code'=>'currency','gross_8'=>'gross_amount','deduction_8'=>'deduction_amount','net_8'=>'net_amount') as $field=>$column) {
            if (is_int($field)) $field=$column;
            if ($header[$field]!==$row[$column]) { $error='native_projection_header_result_mismatch';return false; }
        }
        $members[]=$member;$projection_rows[]=array('result_id'=>$d['result_id'],'projection_sha256'=>$d['projection_sha256']);
    }
    $member_set=pay_glb_001_native_member_set_v1($members,$error);
    if ($member_set===false || $member_set['member_set_sha256']!==$execution['member_set_sha256']) { $error='native_projection_membership_seal_mismatch';return false; }
    $ps=hrm_pay_core_010_projection_set($projection_rows,$error);if ($ps===false) return false;
    $x=array();foreach (array_slice(pay_glb_001_native_receipt_fields()['execution'],1,5) as $field) $x[$field]=pay_glb_001_native_helper_id($execution[$field],$error);
    $x+=array('result_set_sha256'=>$set['result_set_sha256'],'result_count'=>$set['result_count'],'gross_amount'=>$set['gross_amount'],
        'deduction_amount'=>$set['deduction_amount'],'net_amount'=>$set['net_amount'],'projection_contract_version'=>pay_glb_001_native_projection_version(),
        'projection_set_sha256'=>$ps['projection_set_sha256'],'cutover_transition_sha256'=>$cutover_sha,'command_key'=>$key,'maker_id'=>(int)$maker);
    $x['command_sha256']=pay_glb_001_native_projection_hash('intent',$x);
    $x['intent_sha256']=hrm_pay_core_010_hash(array('command_sha256'=>$x['command_sha256'],'result_set_sha256'=>$x['result_set_sha256'],
        'projection_set_sha256'=>$x['projection_set_sha256'],'maker_id'=>$x['maker_id']));
    return $x;
}
