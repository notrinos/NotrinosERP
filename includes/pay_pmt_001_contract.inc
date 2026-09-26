<?php
/** PAY-PMT-001 payment-intent, batch and provider-boundary contract. Pure helpers only. */
if (!defined('PAY_PMT_001_CONTRACT_VERSION')) define('PAY_PMT_001_CONTRACT_VERSION','1.0');
if (!defined('PAY_PMT_001_PREPARE_PERMISSION')) define('PAY_PMT_001_PREPARE_PERMISSION','SA_HRM_PREPARE_PAY_PMT');
if (!defined('PAY_PMT_001_REVIEW_PERMISSION')) define('PAY_PMT_001_REVIEW_PERMISSION','SA_HRM_REVIEW_PAY_PMT');
if (!defined('PAY_PMT_001_RELEASE_PERMISSION')) define('PAY_PMT_001_RELEASE_PERMISSION','SA_HRM_RELEASE_PAY_PMT');
if (!defined('PAY_PMT_001_TRANSMIT_PERMISSION')) define('PAY_PMT_001_TRANSMIT_PERMISSION','SA_HRM_TRANSMIT_PAY_PMT');
if (!defined('PAY_PMT_001_RECOVER_PERMISSION')) define('PAY_PMT_001_RECOVER_PERMISSION','SA_HRM_RECOVER_PAY_PMT');
if (!defined('PAY_PMT_001_VIEW_PERMISSION')) define('PAY_PMT_001_VIEW_PERMISSION','SA_HRM_VIEW_PAY_PMT');

function pay_pmt_001_authority_contract(){return array(
 'approved_result_hash_binding'=>true,'pay_gl_reconciliation_binding'=>true,'immutable_payment_intent'=>true,
 'immutable_batch_item_custody'=>true,'effective_bank_version_binding'=>true,'split_account_preservation'=>true,
 'maker_checker_release_required'=>true,'provider_renderer_contract_required'=>true,'file_checksum_required'=>true,
 'transmission_event_chain_required'=>true,'return_reissue_chain_required'=>true,'independent_reconciliation_required'=>true,
 'provider_secret_external_custody_required'=>true,
 'direct_bank_transaction_execution'=>false,'direct_bank_file_transmission'=>false,'automatic_batch_release'=>false,
 'automatic_beneficiary_selection'=>false,'raw_bank_account_material_in_general_custody'=>false,'payment_replay'=>false,
 'historical_payment_mutation'=>false,'gl_authority'=>false,'statutory_filing_authority'=>false,
 'cross_company_payment'=>false,'network_authority'=>false,'selective_downgrade'=>false);}
function pay_pmt_001_canonicalize($v){if(!is_array($v))return $v;$keys=array_keys($v);$assoc=$keys!==range(0,count($v)-1);if($assoc)ksort($v,SORT_STRING);foreach($v as $k=>$x)$v[$k]=pay_pmt_001_canonicalize($x);return $v;}
function pay_pmt_001_hash($v){$j=json_encode(pay_pmt_001_canonicalize($v),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_PRESERVE_ZERO_FRACTION);return is_string($j)?hash('sha256',$j):false;}
function pay_pmt_001_sha($v){return is_string($v)&&preg_match('/^[a-f0-9]{64}$/D',$v)===1;}
function pay_pmt_001_posint($v){$s=(string)$v;return preg_match('/^[1-9][0-9]*$/D',$s)&&strlen($s)<=18?(int)$s:false;}
function pay_pmt_001_uint($v){$s=(string)$v;return preg_match('/^(?:0|[1-9][0-9]*)$/D',$s)&&strlen($s)<=18?(int)$s:false;}
function pay_pmt_001_currency($v){$v=strtoupper(trim((string)$v));return preg_match('/^[A-Z]{3}$/D',$v)?$v:false;}
function pay_pmt_001_token($v,$max=96){$v=trim((string)$v);return $v!==''&&strlen($v)<=$max&&preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:\/-]*$/D',$v)?$v:false;}
function pay_pmt_001_fail(&$e,$m){$e=$m;return false;}
function pay_pmt_001_validate_item(array $x,&$e=null){$e=null;foreach(array('beneficiary_subject_sha256','source_result_sha256','source_line_key','currency_code','amount_minor','payment_election_id','payment_election_row_version','bank_allocation_sha256') as $k)if(!array_key_exists($k,$x))return pay_pmt_001_fail($e,'payment_item_field_missing');if(!pay_pmt_001_sha($x['beneficiary_subject_sha256'])||!pay_pmt_001_sha($x['source_result_sha256'])||pay_pmt_001_token($x['source_line_key'],96)===false||pay_pmt_001_currency($x['currency_code'])===false||pay_pmt_001_posint($x['amount_minor'])===false||pay_pmt_001_posint($x['payment_election_id'])===false||pay_pmt_001_posint($x['payment_election_row_version'])===false||!pay_pmt_001_sha($x['bank_allocation_sha256']))return pay_pmt_001_fail($e,'payment_item_invalid');$p=$x;$p['currency_code']=pay_pmt_001_currency($x['currency_code']);$p['amount_minor']=pay_pmt_001_posint($x['amount_minor']);$p['payment_election_id']=pay_pmt_001_posint($x['payment_election_id']);$p['payment_election_row_version']=pay_pmt_001_posint($x['payment_election_row_version']);$p['item_sha256']=pay_pmt_001_hash($p);return $p;}
function pay_pmt_001_validate_allocations(array $rows,$amount_minor,&$e=null){$e=null;$amount=pay_pmt_001_posint($amount_minor);if($amount===false||empty($rows))return pay_pmt_001_fail($e,'payment_allocation_set_invalid');$seen=array();$weight=0;$allocated=0;$out=array();foreach($rows as $row){if(!is_array($row))return pay_pmt_001_fail($e,'payment_allocation_invalid');foreach(array('allocation_no','person_bank_account_id','bank_account_row_version','allocation_weight_ppm','allocated_amount_minor','beneficiary_token_sha256') as $k)if(!array_key_exists($k,$row))return pay_pmt_001_fail($e,'payment_allocation_field_missing');$n=pay_pmt_001_posint($row['allocation_no']);$bank=pay_pmt_001_posint($row['person_bank_account_id']);$ver=pay_pmt_001_posint($row['bank_account_row_version']);$w=pay_pmt_001_posint($row['allocation_weight_ppm']);$a=pay_pmt_001_uint($row['allocated_amount_minor']);if($n===false||isset($seen[$n])||$bank===false||$ver===false||$w===false||$w>1000000||$a===false||!pay_pmt_001_sha($row['beneficiary_token_sha256']))return pay_pmt_001_fail($e,'payment_allocation_invalid');$seen[$n]=true;$weight+=$w;$allocated+=$a;$p=$row;$p['allocation_no']=$n;$p['person_bank_account_id']=$bank;$p['bank_account_row_version']=$ver;$p['allocation_weight_ppm']=$w;$p['allocated_amount_minor']=$a;$p['allocation_sha256']=pay_pmt_001_hash($p);$out[$n]=$p;}ksort($out,SORT_NUMERIC);$expected=1;foreach($out as $n=>$row)if((int)$n!==$expected++)return pay_pmt_001_fail($e,'payment_allocation_order_invalid');if($weight!==1000000||$allocated!==$amount)return pay_pmt_001_fail($e,'payment_allocation_reconciliation_failed');return array_values($out);}
function pay_pmt_001_item_set_sha256(array $items){$hashes=array();foreach($items as $x){if(!is_array($x)||!isset($x['item_sha256'])||!pay_pmt_001_sha($x['item_sha256']))return false;$hashes[]=$x['item_sha256'];}sort($hashes,SORT_STRING);return empty($hashes)?false:pay_pmt_001_hash($hashes);}
?>
