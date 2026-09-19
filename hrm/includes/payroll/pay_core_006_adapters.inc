<?php
/** Non-authoritative PAY-CORE-006 source adapters. No production route is wired here. */
include_once(dirname(__FILE__).'/pay_core_006_contract.inc');
function hrm_pay_core_006_payroll_result_adapter(array $line,array $binding,&$error=null){
    $error=null;if(!hrm_pay_core_006_is_sha256($line['result_sha256']??'')||!hrm_pay_core_006_is_sha256($line['line_sha256']??'')){$error='payroll_result_custody_invalid';return false;}
    return hrm_pay_core_006_movement(array('definition_sha256'=>$binding['definition_sha256']??'','scope'=>$binding['scope']??array(),'source_system'=>'pay_core_004','source_type'=>'result_line','source_event_key'=>'result-line:'.($line['result_line_id']??0),'source_event_sha256'=>hrm_pay_core_006_hash(array($line['result_sha256'],$line['line_sha256'])),'movement_kind'=>'payroll','effective_date'=>$binding['effective_date']??'','recognition_date'=>$binding['recognition_date']??'','currency_code'=>$line['currency']??null,'unit_code'=>'money','amount'=>$line['amount']??'0'),$error);
}
function hrm_pay_core_006_legacy_leave_opening_adapter(array $row,array $binding,&$error=null){
    $error=null;foreach(array('entitled','carried_forward','taken','pending','adjusted') as $k)if(!array_key_exists($k,$row)){$error='leave_aggregate_incomplete';return false;}
    try{$amount=hrm_pay_core_006_sub(hrm_pay_core_006_add(hrm_pay_core_006_add((string)$row['entitled'],(string)$row['carried_forward']),(string)$row['adjusted']),hrm_pay_core_006_add((string)$row['taken'],(string)$row['pending']));}catch(Throwable $e){$error='leave_aggregate_invalid';return false;}
    return array('definition_sha256'=>$binding['definition_sha256']??'','scope'=>$binding['scope']??array(),'source_type'=>'leave_balance_aggregate','source_sha256'=>hrm_pay_core_006_hash($row),'provenance_state'=>'aggregate_only','as_of_date'=>$binding['as_of_date']??'','currency_code'=>null,'unit_code'=>'days','amount'=>$amount);
}
function hrm_pay_core_006_legacy_loan_opening_adapter(array $row,array $binding,&$error=null){
    $error=null;if(!array_key_exists('outstanding_amount',$row)){$error='loan_aggregate_incomplete';return false;}try{$amount=hrm_pay_core_006_decimal((string)$row['outstanding_amount']);}catch(Throwable $e){$error='loan_aggregate_invalid';return false;}
    return array('definition_sha256'=>$binding['definition_sha256']??'','scope'=>$binding['scope']??array(),'source_type'=>'loan_outstanding_aggregate','source_sha256'=>hrm_pay_core_006_hash($row),'provenance_state'=>'aggregate_only','as_of_date'=>$binding['as_of_date']??'','currency_code'=>$binding['currency_code']??null,'unit_code'=>'money','amount'=>$amount);
}
?>