<?php
/** PAY-API-001 public read-only HTTP route boundary. No payroll business-data dispatcher is present. */
include_once(dirname(__FILE__).'/pay_api_001_http_readonly.inc');
if (!defined('PAY_API_001_PUBLIC_READONLY_ROUTE_CONTRACT_VERSION')) define('PAY_API_001_PUBLIC_READONLY_ROUTE_CONTRACT_VERSION', '1.0.873');
if (!defined('PAY_API_001_PUBLIC_READONLY_ROUTE_PATH')) define('PAY_API_001_PUBLIC_READONLY_ROUTE_PATH', '/payroll_api.php');

/** Parse the exact bounded public query without accepting PHP array/query ambiguity. */
function pay_api_001_public_route_parse_query($raw_query)
{
    if (!is_string($raw_query) || strlen($raw_query) > 4096) return false;
    $pairs = $raw_query === '' ? array() : explode('&', $raw_query);
    $out = array();
    $allowed = array('company'=>true,'resource'=>true,'contract'=>true,'scope_type'=>true,'scope_key'=>true,'limit'=>true,'cursor'=>true);
    foreach ($pairs as $pair) {
        if ($pair === '' || preg_match('/%(?![0-9A-Fa-f]{2})/', $pair) === 1) return false;
        $pos = strpos($pair, '=');
        $raw_key = $pos === false ? $pair : substr($pair, 0, $pos);
        $raw_value = $pos === false ? '' : substr($pair, $pos + 1);
        $key = rawurldecode($raw_key); $value = rawurldecode($raw_value);
        if (!isset($allowed[$key]) || isset($out[$key]) || strpos($key, '[') !== false || strpos($key, ']') !== false || strpos($value, "\0") !== false) return false;
        $out[$key] = $value;
    }
    foreach (array('company','resource','contract') as $required) if (!isset($out[$required]) || $out[$required] === '') return false;
    if ((isset($out['scope_type']) && !isset($out['scope_key'])) || (!isset($out['scope_type']) && isset($out['scope_key']))) return false;
    $company = pay_api_001_uint_value($out['company'], true, 4294967295);
    $resource = pay_api_001_http_resource_key($out['resource']);
    $contract = pay_api_001_http_contract_version($out['contract']);
    $limit = isset($out['limit']) ? pay_api_001_uint_value($out['limit'], false, 100) : 50;
    $cursor = isset($out['cursor']) && $out['cursor'] !== '' ? $out['cursor'] : null;
    if ($company === false || $resource === false || $contract === false || $limit === false || ($cursor !== null && !pay_api_001_is_sha256($cursor))) return false;
    $scope_type = isset($out['scope_type']) ? $out['scope_type'] : null;
    $scope_key = isset($out['scope_key']) ? $out['scope_key'] : null;
    if ($scope_type !== null && (preg_match('/^[a-z0-9][a-z0-9._-]{0,31}$/D',$scope_type)!==1 || preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]{0,127}$/D',$scope_key)!==1)) return false;
    return array('company_id'=>$company,'resource_key'=>$resource,'contract_version'=>$contract,'object_scope_type'=>$scope_type,'object_scope_key'=>$scope_key,'limit'=>$limit,'cursor_sha256'=>$cursor,'query'=>$out);
}

/** Build the two machine-authentication headers from trusted server variables only. */
function pay_api_001_public_route_headers_from_server($server)
{
    if (!is_array($server)) return false;
    $headers = array(); $auth = array();
    foreach (array('HTTP_AUTHORIZATION','REDIRECT_HTTP_AUTHORIZATION') as $key) if (isset($server[$key]) && (string)$server[$key] !== '') $auth[] = (string)$server[$key];
    if (count($auth) > 1) return false;
    if (count($auth) === 1) $headers['Authorization'] = $auth[0];
    if (isset($server['HTTP_X_REQUEST_ID']) && (string)$server['HTTP_X_REQUEST_ID'] !== '') $headers['X-Request-Id'] = (string)$server['HTTP_X_REQUEST_ID'];
    return $headers;
}

/** Convert internal pre-controller outcomes to a minimized public status. */
function pay_api_001_public_route_http_status($result, $runtime_error=null)
{
    if ($result === false || ($runtime_error !== null && $runtime_error !== '')) return 503;
    if (!is_array($result)) return 503;
    if (!empty($result['allowed'])) return 204;
    $code = isset($result['code']) ? (string)$result['code'] : '';
    if ($code === 'method_not_read_only') return 405;
    if ($code === 'secure_transport_required') return 426;
    if ($code === 'authentication_required') return 401;
    if ($code === 'authorization_required' || $code === 'company_scope_mismatch') return 403;
    if ($code === 'rate_limit_exceeded') return 429;
    if (in_array($code,array('query_credential_forbidden','request_correlation_required','resource_contract_invalid','object_scope_invalid','pagination_invalid','request_target_invalid'),true)) return 400;
    return 403;
}

/** Return a generic public error code that does not disclose credential/resource existence. */
function pay_api_001_public_route_error_body($status)
{
    $status=(int)$status;
    if ($status===400) return array('ok'=>false,'error'=>'invalid_request');
    if ($status===401) return array('ok'=>false,'error'=>'authentication_required');
    if ($status===403) return array('ok'=>false,'error'=>'access_denied');
    if ($status===405) return array('ok'=>false,'error'=>'method_not_allowed');
    if ($status===426) return array('ok'=>false,'error'=>'secure_transport_required');
    if ($status===429) return array('ok'=>false,'error'=>'rate_limit_exceeded');
    return array('ok'=>false,'error'=>'service_unavailable');
}

/** Emit non-cacheable JSON/security headers for the bounded public route. */
function pay_api_001_public_route_security_headers()
{
    if (headers_sent()) return false;
    header('Cache-Control: no-store, max-age=0');
    header('Pragma: no-cache');
    header('X-Content-Type-Options: nosniff');
    header('Content-Type: application/json; charset=utf-8');
    return true;
}

/** Emit one minimized response and terminate the request. */
function pay_api_001_public_route_respond($status)
{
    $status=(int)$status;
    if ($status===405) header('Allow: GET');
    if ($status===401) header('WWW-Authenticate: Basic realm="NotrinosERP Payroll API", charset="UTF-8"');
    http_response_code($status);
    if ($status!==204) echo json_encode(pay_api_001_public_route_error_body($status));
    exit;
}

function pay_api_001_public_readonly_route_source_ready(){return true;}
?>
