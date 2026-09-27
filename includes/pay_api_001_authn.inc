<?php
/** PAY-API-001 pure credential verification and internal read-only request gate. */
if (!defined('PAY_API_001_AUTHN_CONTRACT_VERSION')) define('PAY_API_001_AUTHN_CONTRACT_VERSION', '1.0.862');

function pay_api_001_is_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
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
    if (is_int($value)) return $value >= 0;
    return is_string($value) && preg_match('/^(?:0|[1-9][0-9]*)$/D', $value) === 1;
}

function pay_api_001_readonly_request_gate($method, $transport_secure, $query, $authn, $resource, $scope, $requested_company_id, $requested_object_scope_type=null, $requested_object_scope_key=null)
{
    if (strtoupper((string)$method) !== 'GET') return array('allowed'=>false,'code'=>'method_not_read_only');
    if ($transport_secure !== true) return array('allowed'=>false,'code'=>'secure_transport_required');
    if (pay_api_001_query_contains_credential_material($query)) return array('allowed'=>false,'code'=>'query_credential_forbidden');
    if (!is_array($authn) || empty($authn['authenticated']) || (int)(isset($authn['api_principal_id']) ? $authn['api_principal_id'] : 0) <= 0 || !array_key_exists('company_id', $authn) || !pay_api_001_is_company_id($authn['company_id']))
        return array('allowed'=>false,'code'=>'authentication_required');
    if (!pay_api_001_is_company_id($requested_company_id) || (int)$authn['company_id'] !== (int)$requested_company_id)
        return array('allowed'=>false,'code'=>'company_scope_mismatch');
    if (!is_array($resource) || empty($resource['api_resource_id']) || empty($resource['read_capable']) || !empty($resource['command_capable']))
        return array('allowed'=>false,'code'=>'resource_not_read_only');
    if (!is_array($scope) || (int)(isset($scope['api_principal_id']) ? $scope['api_principal_id'] : 0) !== (int)$authn['api_principal_id'] || (int)(isset($scope['api_resource_id']) ? $scope['api_resource_id'] : 0) !== (int)$resource['api_resource_id'] || (string)(isset($scope['action_code']) ? $scope['action_code'] : '') !== 'read')
        return array('allowed'=>false,'code'=>'read_scope_required');
    if (!empty($resource['object_scope_required'])) {
        if ($requested_object_scope_type === null || $requested_object_scope_key === null || (string)(isset($scope['object_scope_type']) ? $scope['object_scope_type'] : '') !== (string)$requested_object_scope_type || (string)(isset($scope['object_scope_key']) ? $scope['object_scope_key'] : '') !== (string)$requested_object_scope_key)
            return array('allowed'=>false,'code'=>'object_scope_mismatch');
    }
    return array('allowed'=>true,'code'=>'read_allowed','api_principal_id'=>(int)$authn['api_principal_id'],'api_resource_id'=>(int)$resource['api_resource_id'],'company_id'=>(int)$requested_company_id);
}
?>
