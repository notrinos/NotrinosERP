<?php
/** PAY-FIL-001 statutory filing obligation/source-custody contract. Pure helpers only. */
if (!defined('PAY_FIL_001_CONTRACT_VERSION')) define('PAY_FIL_001_CONTRACT_VERSION','1.0');
if (!defined('PAY_FIL_001_PREPARE_PERMISSION')) define('PAY_FIL_001_PREPARE_PERMISSION','SA_HRM_PREPARE_PAY_FIL');
if (!defined('PAY_FIL_001_REVIEW_PERMISSION')) define('PAY_FIL_001_REVIEW_PERMISSION','SA_HRM_REVIEW_PAY_FIL');
if (!defined('PAY_FIL_001_AUTHORIZE_PERMISSION')) define('PAY_FIL_001_AUTHORIZE_PERMISSION','SA_HRM_AUTHORIZE_PAY_FIL');
if (!defined('PAY_FIL_001_SUBMIT_PERMISSION')) define('PAY_FIL_001_SUBMIT_PERMISSION','SA_HRM_SUBMIT_PAY_FIL');
if (!defined('PAY_FIL_001_RECOVER_PERMISSION')) define('PAY_FIL_001_RECOVER_PERMISSION','SA_HRM_RECOVER_PAY_FIL');
if (!defined('PAY_FIL_001_VIEW_PERMISSION')) define('PAY_FIL_001_VIEW_PERMISSION','SA_HRM_VIEW_PAY_FIL');

function pay_fil_001_authority_contract(){return array(
 'legal_entity_period_jurisdiction_binding'=>true,
 'many_to_many_exact_source_linkage'=>true,
 'regular_offcycle_correction_aggregation'=>true,
 'pinned_country_pack_schema_connector'=>true,
 'immutable_obligation_and_source_custody'=>true,
 'maker_checker_authorization_required'=>true,
 'signed_payload_hash_required'=>true,
 'submission_event_chain_required'=>true,
 'ack_reject_correct_amend_chain_required'=>true,
 'tax_data_restriction_required'=>true,
 'retention_evidence_required'=>true,
 'official_form_generation_without_named_schema'=>false,
 'direct_government_submission'=>false,
 'automatic_submission'=>false,
 'unreviewed_correction_or_amendment'=>false,
 'fabricated_historical_source_link'=>false,
 'historical_payroll_or_filing_mutation'=>false,
 'payment_authority'=>false,
 'gl_authority'=>false,
 'cross_company_filing'=>false,
 'network_authority'=>false,
 'connector_secret_custody'=>false,
 'selective_downgrade'=>false
 );}
function pay_fil_001_canonicalize($v){if(!is_array($v))return $v;$keys=array_keys($v);$assoc=$keys!==range(0,count($v)-1);if($assoc)ksort($v,SORT_STRING);foreach($v as $k=>$x)$v[$k]=pay_fil_001_canonicalize($x);return $v;}
function pay_fil_001_hash($v){$j=json_encode(pay_fil_001_canonicalize($v),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_PRESERVE_ZERO_FRACTION);return is_string($j)?hash('sha256',$j):false;}
function pay_fil_001_sha($v){return is_string($v)&&preg_match('/^[a-f0-9]{64}$/D',$v)===1;}
function pay_fil_001_posint($v){$s=(string)$v;return preg_match('/^[1-9][0-9]*$/D',$s)&&strlen($s)<=18?(int)$s:false;}
function pay_fil_001_uint($v){$s=(string)$v;return preg_match('/^(?:0|[1-9][0-9]*)$/D',$s)&&strlen($s)<=18?(int)$s:false;}
function pay_fil_001_token($v,$max=96){$v=trim((string)$v);return $v!==''&&strlen($v)<=$max&&preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:\/-]*$/D',$v)?$v:false;}
function pay_fil_001_date($v){$v=trim((string)$v);if(!preg_match('/^20[0-9]{2}-[0-9]{2}-[0-9]{2}$/D',$v))return false;$d=DateTime::createFromFormat('!Y-m-d',$v,new DateTimeZone('UTC'));$e=DateTime::getLastErrors();return $d&&(!is_array($e)||((int)$e['warning_count']===0&&(int)$e['error_count']===0))&&$d->format('Y-m-d')===$v?$v:false;}
function pay_fil_001_fail(&$e,$m){$e=$m;return false;}
function pay_fil_001_obligation_sha256(array $x){$keys=array('company_id','legal_entity_id','jurisdiction_code','obligation_type','period_start','period_end','due_date','filing_frequency','country_pack_id','country_pack_version','schema_id','schema_version','connector_id','connector_version');$p=array();foreach($keys as $k){if(!array_key_exists($k,$x))return false;$p[$k]=$x[$k];}return pay_fil_001_hash($p);}
function pay_fil_001_validate_obligation(array $x,&$e=null){$e=null;foreach(array('company_id','legal_entity_id','jurisdiction_code','obligation_type','period_start','period_end','due_date','filing_frequency','country_pack_id','country_pack_version','schema_id','schema_version','connector_id','connector_version') as $k)if(!array_key_exists($k,$x))return pay_fil_001_fail($e,'filing_obligation_field_missing');if(pay_fil_001_posint($x['company_id'])===false||pay_fil_001_posint($x['legal_entity_id'])===false||pay_fil_001_token($x['jurisdiction_code'],32)===false||pay_fil_001_token($x['obligation_type'],64)===false||pay_fil_001_date($x['period_start'])===false||pay_fil_001_date($x['period_end'])===false||pay_fil_001_date($x['due_date'])===false||$x['period_start']>$x['period_end']||$x['due_date']<$x['period_end']||pay_fil_001_token($x['filing_frequency'],24)===false||pay_fil_001_token($x['country_pack_id'],96)===false||pay_fil_001_token($x['country_pack_version'],32)===false||pay_fil_001_token($x['schema_id'],96)===false||pay_fil_001_token($x['schema_version'],32)===false||pay_fil_001_token($x['connector_id'],96)===false||pay_fil_001_token($x['connector_version'],32)===false)return pay_fil_001_fail($e,'filing_obligation_invalid');$p=$x;$p['company_id']=pay_fil_001_posint($x['company_id']);$p['legal_entity_id']=pay_fil_001_posint($x['legal_entity_id']);$p['obligation_sha256']=pay_fil_001_obligation_sha256($p);return $p;}
function pay_fil_001_source_link_sha256(array $x){$keys=array('source_set_sha256','link_no','source_kind','source_key','source_sha256','predecessor_source_sha256','run_kind','currency_code','amount_minor');$p=array();foreach($keys as $k){if(!array_key_exists($k,$x))return false;$p[$k]=$x[$k];}return pay_fil_001_hash($p);}
function pay_fil_001_validate_source_link(array $x,&$e=null){$e=null;foreach(array('source_set_sha256','link_no','source_kind','source_key','source_sha256','predecessor_source_sha256','run_kind','currency_code','amount_minor') as $k)if(!array_key_exists($k,$x))return pay_fil_001_fail($e,'filing_source_field_missing');$kinds=array('run','result','line','balance','liability');$runs=array('regular','offcycle','correction');if(!pay_fil_001_sha($x['source_set_sha256'])||pay_fil_001_posint($x['link_no'])===false||!in_array($x['source_kind'],$kinds,true)||pay_fil_001_token($x['source_key'],128)===false||!pay_fil_001_sha($x['source_sha256'])||($x['predecessor_source_sha256']!==''&&!pay_fil_001_sha($x['predecessor_source_sha256']))||!in_array($x['run_kind'],$runs,true)||!preg_match('/^[A-Z]{3}$/D',(string)$x['currency_code'])||!is_int($x['amount_minor'])&&!(is_string($x['amount_minor'])&&preg_match('/^-?[0-9]{1,18}$/D',$x['amount_minor'])))return pay_fil_001_fail($e,'filing_source_invalid');$p=$x;$p['link_no']=pay_fil_001_posint($x['link_no']);$p['amount_minor']=(int)$x['amount_minor'];$p['source_link_sha256']=pay_fil_001_source_link_sha256($p);return $p;}
?>
