<?php
/** PAY-API-001 pure credential verification and internal read-only request gate. */
if (!defined('PAY_API_001_AUTHN_CONTRACT_VERSION')) define('PAY_API_001_AUTHN_CONTRACT_VERSION', '1.0.868');

function pay_api_001_is_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

function pay_api_001_uint_value($value, $allow_zero=false, $max=null)
{
    if (is_int($value)) $n=$value;
    elseif (is_string($value) && preg_match('/^(?:0|[1-9][0-9]*)$/D', $value) === 1) {
        $n=(int)$value;
        if ((string)$n !== $value) return false;
    } else return false;
    if ($allow_zero ? $n < 0 : $n <= 0) return false;
    if ($max !== null && $n > (int)$max) return false;
    return $n;
}

function pay_api_001_positive_id($value)
{
    return pay_api_001_uint_value($value, false, PHP_INT_MAX);
}

function pay_api_001_verify_client_secret($presented_secret, $verifier_hash)
{
    if (!is_string($presented_secret) || $presented_secret === '' || strlen($presented_secret) > 4096)
        return false;
    if (!is_string($verifier_hash) || $verifier_hash === '' || strlen($verifier_hash) > 255)
        return false;
    return password_verify($presented_secret, $verifier_hash);
}

function pay_api_001_query_contains_credential_material($query, $depth=0)
{
    if (!is_array($query) || (int)$depth > 8) return true;
    $forbidden = array('access_token','token','secret','client_secret','api_key','apikey','authorization');
    foreach ($query as $key => $value) {
        if (in_array(strtolower((string)$key), $forbidden, true)) return true;
        if (is_array($value) && pay_api_001_query_contains_credential_material($value, (int)$depth + 1)) return true;
    }
    return false;
}

function pay_api_001_is_company_id($value)
{
    return pay_api_001_uint_value($value, true, 4294967295) !== false;
}

function pay_api_001_readonly_request_gate($method, $transport_secure, $query, $authn, $resource, $scope, $requested_company_id, $requested_object_scope_type=null, $requested_object_scope_key=null)
{
    if (strtoupper((string)$method) !== 'GET') return array('allowed'=>false,'code'=>'method_not_read_only');
    if ($transport_secure !== true) return array('allowed'=>false,'code'=>'secure_transport_required');
    if (pay_api_001_query_contains_credential_material($query)) return array('allowed'=>false,'code'=>'query_credential_forbidden');
    $principal_id=is_array($authn)&&isset($authn['api_principal_id'])?pay_api_001_positive_id($authn['api_principal_id']):false;
    $authn_company=is_array($authn)&&array_key_exists('company_id',$authn)?pay_api_001_uint_value($authn['company_id'],true,4294967295):false;
    if (!is_array($authn) || empty($authn['authenticated']) || $principal_id === false || $authn_company === false)
        return array('allowed'=>false,'code'=>'authentication_required');
    $requested_company=pay_api_001_uint_value($requested_company_id,true,4294967295);
    if ($requested_company === false || $authn_company !== $requested_company)
        return array('allowed'=>false,'code'=>'company_scope_mismatch');
    $resource_id=is_array($resource)&&isset($resource['api_resource_id'])?pay_api_001_positive_id($resource['api_resource_id']):false;
    if (!is_array($resource) || $resource_id===false || empty($resource['read_capable']) || !empty($resource['command_capable']))
        return array('allowed'=>false,'code'=>'resource_not_read_only');
    $scope_principal=is_array($scope)&&isset($scope['api_principal_id'])?pay_api_001_positive_id($scope['api_principal_id']):false;
    $scope_resource=is_array($scope)&&isset($scope['api_resource_id'])?pay_api_001_positive_id($scope['api_resource_id']):false;
    if (!is_array($scope) || $scope_principal !== $principal_id || $scope_resource !== $resource_id || (string)(isset($scope['action_code']) ? $scope['action_code'] : '') !== 'read')
        return array('allowed'=>false,'code'=>'read_scope_required');
    if (!empty($resource['object_scope_required'])) {
        if ($requested_object_scope_type === null || $requested_object_scope_key === null || (string)(isset($scope['object_scope_type']) ? $scope['object_scope_type'] : '') !== (string)$requested_object_scope_type || (string)(isset($scope['object_scope_key']) ? $scope['object_scope_key'] : '') !== (string)$requested_object_scope_key)
            return array('allowed'=>false,'code'=>'object_scope_mismatch');
    }
    return array('allowed'=>true,'code'=>'read_allowed','api_principal_id'=>$principal_id,'api_resource_id'=>$resource_id,'company_id'=>$requested_company);
}
?>
