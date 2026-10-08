<?php
/**
 * PAY-CON-001 offline documentation-based Deel request/response adapter.
 * There are deliberately NO network I/O functions, sockets, credentials,
 * database writes, or live webhook handlers in this module.
 */
require_once(dirname(__FILE__).'/pay_con_001_deel_mock_contract.inc');

/** Validate a documented UUID without coercing input types. */
function pay_con_001_deel_mock_uuid($value)
{
    return is_string($value) && preg_match('/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/D', $value) === 1;
}

/** Construct only a whitelisted synthetic GET request with no credentials. */
function pay_con_001_deel_mock_build_request($route_name, $params, $company_id)
{
    // Company 0 is the ONLY synthetic fixture company. Do not infer authority.
    if (!is_string($route_name) || !is_int($company_id) || $company_id !== 0 || !is_array($params)) return false;
    $routes = pay_con_001_deel_mock_routes();
    if (!isset($routes[$route_name])) return false;
    $route = $routes[$route_name];
    $path = null;
    $query = array();
    if ($route_name === 'payroll_groups') {
        $allow = array('legalEntityId', 'calendarYear', 'includePayrollCalendar');
        foreach ($params as $key => $v) if (!in_array($key, $allow, true)) return false;
        if (!isset($params['legalEntityId']) || !pay_con_001_deel_mock_uuid($params['legalEntityId'])) return false;
        $query['legalEntityId'] = $params['legalEntityId'];
        if (array_key_exists('calendarYear', $params)) {
            if (!is_int($params['calendarYear']) || $params['calendarYear'] < 2020 || $params['calendarYear'] > 2035) return false;
            $query['calendarYear'] = $params['calendarYear'];
        }
        if (array_key_exists('includePayrollCalendar', $params)) {
            if (!is_bool($params['includePayrollCalendar'])) return false;
            $query['includePayrollCalendar'] = $params['includePayrollCalendar'];
        }
        $path = $route['path'];
    } elseif ($route_name === 'gross_to_net_csv') {
        if (count($params) !== 1 || !isset($params['id']) || !is_string($params['id'])
            || preg_match('/^[a-zA-Z0-9_-]{1,64}$/D', $params['id']) !== 1) return false;
        $path = str_replace('{id}', $params['id'], $route['path_template']);
    } else {
        return false;
    }
    return array(
        'mode' => 'OFFLINE_MOCK',
        'provider' => 'DEEL',
        'route' => $route_name,
        'method' => 'GET',
        'path' => $path,
        'query' => $query,
        'headers' => array('X-Version' => '2026-01-01', 'Accept' => $route_name === 'gross_to_net_csv' ? 'text/csv' : 'application/json'),
        'company_id' => $company_id,
        'mock_only' => true,
        'outbound_network_allowed' => false,
        'real_provider_admitted' => false
    );
}

/** The fixture file is solely invented synthetic test data, never provider evidence. */
function pay_con_001_deel_mock_load_fixture($fixture_file)
{
    $approved_fixture = realpath(dirname(__DIR__).'/tests/fixtures/payroll/pay_con_001_deel_mock_v1.json');
    if (!is_string($fixture_file) || $approved_fixture === false
        || realpath($fixture_file) !== $approved_fixture || !is_file($fixture_file)) return false;
    $data = json_decode((string)file_get_contents($fixture_file), true);
    if (!is_array($data) || ($data['fixture_kind'] ?? '') !== 'SYNTHETIC_NOT_VENDOR_RESPONSE'
        || ($data['provider'] ?? '') !== 'DEEL' || ($data['api_version'] ?? '') !== '2026-01-01'
        || ($data['company_id'] ?? null) !== 0) return false;
    if (!isset($data['payroll_groups']['data']) || !is_array($data['payroll_groups']['data'])
        || !isset($data['gross_to_net_csv']) || !is_string($data['gross_to_net_csv'])) return false;
    return pay_con_001_deel_mock_fixture_valid($data) ? $data : false;
}

/** Pin all decoded fixture content, including payloads, to reviewed synthetic values. */
function pay_con_001_deel_mock_fixture_valid($fixture)
{
    if (!is_array($fixture)) return false;
    $json = json_encode($fixture, JSON_UNESCAPED_SLASHES);
    return is_string($json) && hash_equals(
        'cbd4a59f58345df89de5154802207a5e36d1f878d7ca97ed9329027baffe97c9',
        hash('sha256', $json)
    );
}

/**
 * Route only a local synthetic fixture. An unexpected route, company, mode,
 * response status or different legalEntityId is an error, never a fallback to live.
 */
function pay_con_001_deel_mock_dispatch($request, $fixture, $synthetic_status = 200)
{
    if (!is_array($request) || !pay_con_001_deel_mock_fixture_valid($fixture) || !is_int($synthetic_status)
        || ($request['mode'] ?? '') !== 'OFFLINE_MOCK'
        || ($request['mock_only'] ?? false) !== true
        || ($request['outbound_network_allowed'] ?? true) !== false
        || ($request['real_provider_admitted'] ?? true) !== false
        || ($request['company_id'] ?? null) !== 0
        || ($fixture['company_id'] ?? null) !== 0
        || ($fixture['fixture_kind'] ?? '') !== 'SYNTHETIC_NOT_VENDOR_RESPONSE'
        || ($fixture['api_version'] ?? '') !== '2026-01-01'
        || ($request['provider'] ?? '') !== 'DEEL'
        || ($fixture['provider'] ?? '') !== 'DEEL'
        || ($request['headers']['X-Version'] ?? '') !== '2026-01-01'
        || ($request['method'] ?? '') !== 'GET') return false;
    if (!in_array($synthetic_status, array(200, 400, 401, 403, 404, 429, 500), true)) return false;
    // Reconstruct the complete request before any response, including simulated errors.
    if (!is_array($request['query'] ?? null) || !is_string($request['route'] ?? null)) return false;
    $params = $request['query'];
    if ($request['route'] === 'gross_to_net_csv') {
        if ($params !== array()) return false;
        $params = array('id' => $fixture['report_id']);
    }
    $canonical = pay_con_001_deel_mock_build_request($request['route'], $params, 0);
    if ($canonical === false || $request !== $canonical) return false;
    if ($request['route'] === 'payroll_groups'
        && $request['query']['legalEntityId'] !== $fixture['legal_entity_id']) return false;
    if ($synthetic_status !== 200) return array('status' => $synthetic_status, 'data' => null, 'mock_only' => true, 'authoritative' => false);
    if (($request['route'] ?? '') === 'payroll_groups' && ($request['path'] ?? '') === '/rest/payroll-groups') {
        $expected = $fixture['legal_entity_id'] ?? '';
        if (($request['query']['legalEntityId'] ?? '') !== $expected) return false;
        return array('status' => 200, 'data' => $fixture['payroll_groups'], 'mock_only' => true, 'authoritative' => false);
    }
    if (($request['route'] ?? '') === 'gross_to_net_csv'
        && ($request['path'] ?? '') === '/rest/gp/reports/'.($fixture['report_id'] ?? '').'/gross_to_net/csv') {
        return array('status' => 200, 'data' => $fixture['gross_to_net_csv'], 'mock_only' => true, 'authoritative' => false);
    }
    return false;
}

/**
 * Pure offline test of the Deel documented signature convention:
 * HMAC-SHA256(secret, "POST" + raw_body). Never register a webhook listener.
 */
function pay_con_001_deel_mock_verify_signature($raw_body, $signature, $synthetic_test_key)
{
    if (!is_string($raw_body) || !is_string($signature) || !is_string($synthetic_test_key)
        || strlen($synthetic_test_key) < 16 || strpos($synthetic_test_key, 'SYNTHETIC_') !== 0
        || preg_match('/^[0-9a-f]{64}$/D', $signature) !== 1) return false;
    $expected = hash_hmac('sha256', 'POST'.$raw_body, $synthetic_test_key);
    return hash_equals($expected, $signature);
}
?>
