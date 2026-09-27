<?php
/** PAY-API-001 pure HTTP pre-controller contract. No public route is exposed by this file. */
include_once(dirname(__FILE__).'/pay_api_001_readonly_adapter.inc');
if (!defined('PAY_API_001_HTTP_READONLY_CONTRACT_VERSION')) define('PAY_API_001_HTTP_READONLY_CONTRACT_VERSION', '1.0.871');

function pay_api_001_http_header_value($headers, $name)
{
    if (!is_array($headers) || !is_string($name) || $name === '') return false;
    $want = strtolower($name); $found = false; $value = null;
    foreach ($headers as $k=>$v) {
        if (strtolower((string)$k) !== $want) continue;
        if ($found || !is_string($v) || strpos($v, "\r") !== false || strpos($v, "\n") !== false) return false;
        $found = true; $value = trim($v);
    }
    return $found ? $value : null;
}

function pay_api_001_http_basic_credentials($authorization)
{
    if (!is_string($authorization) || $authorization === '' || strlen($authorization) > 8192) return false;
    if (preg_match('/^Basic[ \t]+([A-Za-z0-9+\/]+={0,2})$/D', $authorization, $m) !== 1) return false;
    $decoded = base64_decode($m[1], true);
    if (!is_string($decoded) || strlen($decoded) > 4225) return false;
    $pos = strpos($decoded, ':');
    if ($pos === false) return false;
    $key = substr($decoded, 0, $pos); $secret = substr($decoded, $pos + 1);
    if ($key === '' || strlen($key) > 128 || preg_match('/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D', $key) !== 1) return false;
    if ($secret === '' || strlen($secret) > 4096) return false;
    return array('credential_key'=>$key, 'presented_secret'=>$secret);
}

function pay_api_001_http_request_id_sha256($request_id)
{
    if (!is_string($request_id) || $request_id === '' || strlen($request_id) > 128) return false;
    if (preg_match('/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D', $request_id) !== 1) return false;
    return hash('sha256', $request_id);
}

function pay_api_001_http_query_canonical($value, $depth=0)
{
    if ((int)$depth > 8) return false;
    if (is_array($value)) {
        $out = array();
        foreach ($value as $k=>$v) {
            if (!is_int($k) && (!is_string($k) || $k === '' || strlen($k) > 128)) return false;
            $n = pay_api_001_http_query_canonical($v, (int)$depth + 1);
            if ($n === false && $v !== false) return false;
            $out[$k] = $n;
        }
        if (count($out) > 1) ksort($out, SORT_STRING);
        return $out;
    }
    if ($value === null || is_bool($value) || is_int($value)) return $value;
    if (is_string($value) && strlen($value) <= 1024 && strpos($value, "\0") === false) return $value;
    return false;
}

function pay_api_001_http_request_target_sha256($path, $query)
{
    if (!is_string($path) || $path === '' || strlen($path) > 256 || preg_match('/^[\/A-Za-z0-9._-]+$/D', $path) !== 1) return false;
    if (!is_array($query) || pay_api_001_query_contains_credential_material($query)) return false;
    $q = pay_api_001_http_query_canonical($query);
    if ($q === false) return false;
    $json = json_encode(array('path'=>$path,'query'=>$q), JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
    return is_string($json) ? hash('sha256', $json) : false;
}

function pay_api_001_http_resource_key($value)
{
    return is_string($value) && strlen($value) <= 96 && preg_match('/^[a-z0-9][a-z0-9._\/-]{0,95}$/D', $value) === 1 ? $value : false;
}

function pay_api_001_http_contract_version($value)
{
    return is_string($value) && strlen($value) <= 32 && preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$/D', $value) === 1 ? $value : false;
}

function pay_api_001_http_preroute_envelope($method, $transport_secure, $headers, $query, $path, $company_id, $resource_key, $contract_version, $object_scope_type, $object_scope_key, $limit, $cursor_sha256=null)
{
    if (strtoupper((string)$method) !== 'GET') return array('allowed'=>false,'code'=>'method_not_read_only');
    if ($transport_secure !== true) return array('allowed'=>false,'code'=>'secure_transport_required');
    if (!is_array($query) || pay_api_001_query_contains_credential_material($query)) return array('allowed'=>false,'code'=>'query_credential_forbidden');
    $authorization = pay_api_001_http_header_value($headers, 'Authorization');
    $credentials = pay_api_001_http_basic_credentials($authorization);
    if ($credentials === false) return array('allowed'=>false,'code'=>'authentication_required');
    $request_id = pay_api_001_http_header_value($headers, 'X-Request-Id');
    $correlation = pay_api_001_http_request_id_sha256($request_id);
    if ($correlation === false) return array('allowed'=>false,'code'=>'request_correlation_required');
    $company = pay_api_001_uint_value($company_id, true, 4294967295);
    if ($company === false) return array('allowed'=>false,'code'=>'company_scope_mismatch');
    $resource = pay_api_001_http_resource_key($resource_key);
    $contract = pay_api_001_http_contract_version($contract_version);
    if ($resource === false || $contract === false) return array('allowed'=>false,'code'=>'resource_contract_invalid');
    if (($object_scope_type === null) !== ($object_scope_key === null)) return array('allowed'=>false,'code'=>'object_scope_invalid');
    if ($object_scope_type !== null) {
        if (!is_string($object_scope_type) || preg_match('/^[a-z0-9][a-z0-9._-]{0,31}$/D',$object_scope_type)!==1) return array('allowed'=>false,'code'=>'object_scope_invalid');
        if (!is_string($object_scope_key) || preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]{0,127}$/D',$object_scope_key)!==1) return array('allowed'=>false,'code'=>'object_scope_invalid');
    }
    $page = pay_api_001_readonly_page_request($limit, $cursor_sha256);
    if ($page === false) return array('allowed'=>false,'code'=>'pagination_invalid');
    $target = pay_api_001_http_request_target_sha256($path, $query);
    if ($target === false) return array('allowed'=>false,'code'=>'request_target_invalid');
    return array(
        'allowed'=>true,
        'code'=>'http_preroute_envelope_valid',
        'company_id'=>$company,
        'credential_key'=>$credentials['credential_key'],
        'presented_secret'=>$credentials['presented_secret'],
        'correlation_sha256'=>$correlation,
        'resource_key'=>$resource,
        'resource_key_sha256'=>hash('sha256',$resource.':'.$contract),
        'contract_version'=>$contract,
        'object_scope_type'=>$object_scope_type,
        'object_scope_key'=>$object_scope_key,
        'page'=>$page,
        'request_target_sha256'=>$target
    );
}

function pay_api_001_public_readonly_controller_source_ready(){return true;}
?>
