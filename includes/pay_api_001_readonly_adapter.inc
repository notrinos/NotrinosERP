<?php
/** PAY-API-001 pre-route read-only adapter contract. No public HTTP route is enabled by this file. */
include_once(dirname(__FILE__).'/pay_api_001_authn.inc');
include_once(dirname(__FILE__).'/pay_api_001_rate_limit.inc');
include_once(dirname(__FILE__).'/pay_api_001_contract.inc');
if(!defined('PAY_API_001_READONLY_ADAPTER_CONTRACT_VERSION'))define('PAY_API_001_READONLY_ADAPTER_CONTRACT_VERSION','1.0.873');
function pay_api_001_readonly_page_request($limit,$cursor_sha256=null)
{
    $n=pay_api_001_uint_value($limit,false,100);if($n===false)return false;if($cursor_sha256!==null&&!pay_api_001_is_sha256($cursor_sha256))return false;return array('limit'=>$n,'cursor_sha256'=>$cursor_sha256===null?null:(string)$cursor_sha256);
}
function pay_api_001_readonly_adapter_gate($method,$transport_secure,$query,$authn,$resource,$scope,$company_id,$object_scope_type,$object_scope_key,$rate_decision,$limit,$cursor_sha256=null)
{
    $g=pay_api_001_readonly_request_gate($method,$transport_secure,$query,$authn,$resource,$scope,$company_id,$object_scope_type,$object_scope_key);if(empty($g['allowed']))return $g;
    if(!is_array($rate_decision)||empty($rate_decision['allowed'])||(string)(isset($rate_decision['code'])?$rate_decision['code']:'')!=='rate_limit_allowed')return array('allowed'=>false,'code'=>'rate_limit_required');
    $rate_company=isset($rate_decision['company_id'])?pay_api_001_uint_value($rate_decision['company_id'],true,4294967295):false;
    $rate_principal=isset($rate_decision['api_principal_id'])?pay_api_001_positive_id($rate_decision['api_principal_id']):false;
    $rate_resource=isset($rate_decision['api_resource_id'])?pay_api_001_positive_id($rate_decision['api_resource_id']):false;
    if($rate_company===false||$rate_principal===false||$rate_resource===false||$rate_company!==$g['company_id']||$rate_principal!==$g['api_principal_id']||$rate_resource!==$g['api_resource_id'])return array('allowed'=>false,'code'=>'rate_limit_scope_mismatch');
    $page=pay_api_001_readonly_page_request($limit,$cursor_sha256);if($page===false)return array('allowed'=>false,'code'=>'pagination_invalid');return array('allowed'=>true,'code'=>'readonly_adapter_allowed','company_id'=>$g['company_id'],'api_principal_id'=>$g['api_principal_id'],'api_resource_id'=>$g['api_resource_id'],'page'=>$page);
}
function pay_api_001_public_readonly_http_route_enabled_for_database_version($database_version)
{
    return pay_api_001_public_readonly_route_allowed_for_database_version($database_version);
}
function pay_api_001_public_readonly_http_route_enabled()
{
    if (!function_exists('get_company_pref')) return false;
    $version = get_company_pref('version_id', true);
    return is_string($version) && pay_api_001_public_readonly_http_route_enabled_for_database_version($version);
}
?>
