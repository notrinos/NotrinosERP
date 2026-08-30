<?php
/** PAY-SEC-004 authenticated public payroll-post OIDC step-up route. */
$payroll_step_up_raw_method = isset($_SERVER['REQUEST_METHOD']) ? (string)$_SERVER['REQUEST_METHOD'] : '';
$payroll_step_up_raw_content_type = isset($_SERVER['CONTENT_TYPE']) ? (string)$_SERVER['CONTENT_TYPE'] : '';
$payroll_step_up_raw_query = isset($_SERVER['QUERY_STRING']) ? (string)$_SERVER['QUERY_STRING'] : '';
$payroll_step_up_raw_body = $payroll_step_up_raw_method === 'POST' ? @file_get_contents('php://input') : '';
$payroll_step_up_script_name = isset($_SERVER['SCRIPT_NAME']) ? (string)$_SERVER['SCRIPT_NAME'] : '';

$page_security = 'SA_PAYROLLPOST';
$path_to_root = '../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/federation_oidc_payroll_post_step_up.inc');
include_once($path_to_root.'/includes/federation_oidc_payroll_post_step_up_route.inc');

federation_oidc_payroll_step_up_route_security_headers();
$now = time();
if (!session_transport_is_https() || !federation_oidc_payroll_step_up_route_authenticated()
    || !federation_oidc_payroll_step_up_route_consume_attempt($now)) {
    federation_oidc_payroll_step_up_route_redirect_local($payroll_step_up_script_name, 'failed');
    exit;
}
$company = function_exists('user_company') ? (int)user_company() : -1;
$callback_path = federation_oidc_payroll_step_up_route_callback_path($payroll_step_up_script_name);
if ($company < 0 || $callback_path === false) {
    federation_oidc_payroll_step_up_route_redirect_local($payroll_step_up_script_name, 'failed');
    exit;
}

if ($payroll_step_up_raw_method === 'POST') {
    $request = federation_oidc_payroll_step_up_route_parse_start(
        $payroll_step_up_raw_method,
        $payroll_step_up_raw_content_type,
        is_string($payroll_step_up_raw_body) ? $payroll_step_up_raw_body : ''
    );
    if (!is_array($request)) {
        federation_oidc_payroll_step_up_route_redirect_local($payroll_step_up_script_name, 'failed');
        exit;
    }
    $result = federation_oidc_start_payroll_post_step_up(
        $company,
        (int)$request['period_id'],
        (string)$request['csrf'],
        $callback_path
    );
    $status = is_array($result) && isset($result['status']) ? (string)$result['status'] : 'denied';
    if ($status === 'authorization_required' && isset($result['authorization_url'])
        && federation_oidc_payroll_step_up_route_redirect_provider((string)$result['authorization_url']))
        exit;
    federation_oidc_payroll_step_up_route_redirect_local(
        $payroll_step_up_script_name,
        $status === 'retry_later' ? 'retry' : 'failed',
        (int)$request['period_id']
    );
    exit;
}

$request = federation_oidc_payroll_step_up_route_parse_callback($payroll_step_up_raw_method, $payroll_step_up_raw_query);
if (!is_array($request)) {
    federation_oidc_payroll_step_up_route_redirect_local($payroll_step_up_script_name, 'failed');
    exit;
}
$pending = federation_oidc_payroll_step_up_pending($company, (string)$request['state'], $now);
$period_id = is_array($pending) && isset($pending['period_id']) ? (int)$pending['period_id'] : 0;
if (!is_array($pending)) {
    federation_oidc_payroll_step_up_route_redirect_local($payroll_step_up_script_name, 'failed');
    exit;
}
if ($request['kind'] === 'error') {
    federation_oidc_payroll_step_up_route_terminalize_error($company, (string)$request['state'], (string)$request['error']);
    federation_oidc_payroll_step_up_forget_pending((string)$request['state']);
    federation_oidc_payroll_step_up_route_redirect_local($payroll_step_up_script_name, 'failed', $period_id);
    exit;
}
$result = federation_oidc_complete_payroll_post_step_up(
    $company,
    (string)$request['state'],
    (string)$request['code'],
    $callback_path
);
$status = is_array($result) && isset($result['status']) ? (string)$result['status'] : 'denied';
if ($status === 'assured') {
    federation_oidc_payroll_step_up_route_reset_attempts();
    federation_oidc_payroll_step_up_route_redirect_local($payroll_step_up_script_name, 'assured', $period_id);
    exit;
}
if ($status === 'retry_later') {
    // The authorization code is intentionally never persisted by this route. Abandon
    // only the browser correlation so a new explicit start can be issued immediately;
    // the unconsumed verifier transaction/claim remains immutable and expires normally.
    federation_oidc_payroll_step_up_forget_pending((string)$request['state']);
}
federation_oidc_payroll_step_up_route_redirect_local(
    $payroll_step_up_script_name,
    $status === 'retry_later' ? 'retry' : 'failed',
    $period_id
);
exit;
