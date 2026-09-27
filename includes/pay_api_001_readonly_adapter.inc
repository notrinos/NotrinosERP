<?php
/** PAY-API-001 pre-route read-only adapter contract. No public HTTP route is enabled by this file. */
include_once(dirname(__FILE__).'/pay_api_001_authn.inc');
include_once(dirname(__FILE__).'/pay_api_001_rate_limit.inc');
if(!defined('PAY_API_001_READONLY_ADAPTER_CONTRACT_VERSION'))define('PAY_API_001_READONLY_ADAPTER_CONTRACT_VERSION','1.0.868');
function pay_api_001_readonly_page_request($limit,$cursor_sha256=null)
{
    $n=pay_api_001_uint_value($limit,false,100);if($n===false)return false;if($cursor_sha256!==null&&!pay_api_001_is_sha256($cursor_sha256))return false;return array('limit'=>$n,'cursor_sha256'=>$cursor_sha256===null?null:(string)$cursor_sha256);
}
function pay_api_001_readonly_adapter_gate($method,$transport_secure,$query,$authn,$resource,$scope,$company_id,$object_scope_type,$object_scope_key,$rate_decision,$limit,$cursor_sha256=null)
{
    $g=pay_api_001_readonly_request_gate($method,$transport_secure,$query,$authn,$resource,$scope,$company_id,$object_scope_type,$object_scope_key);if(empty($g['allowed']))return $g;
    if(!is_array($rate_decision)||empty($rate_decision['allowed'])||(string)(isset($rate_decision['code'])?$rate_decision['code']:'')!=='rate_limit_allowed')return array('allowed'=>false,'code'=>'rate_limit_required');
    foreach(array('company_id','api_principal_id','api_resource_id') as $k)if(!isset($rate_decision[$k])||(int)$rate_decision[$k]!== (int)$g[$k])return array('allowed'=>false,'code'=>'rate_limit_scope_mismatch');
    $page=pay_api_001_readonly_page_request($limit,$cursor_sha256);if($page===false)return array('allowed'=>false,'code'=>'pagination_invalid');return array('allowed'=>true,'code'=>'readonly_adapter_allowed','company_id'=>$g['company_id'],'api_principal_id'=>$g['api_principal_id'],'api_resource_id'=>$g['api_resource_id'],'page'=>$page);
}
function pay_api_001_public_readonly_http_route_enabled(){return false;}
?>
