<?php
/** PAY-SEC-004 public existing-link OIDC callback route. */
$federation_oidc_raw_method = isset($_SERVER['REQUEST_METHOD']) ? (string)$_SERVER['REQUEST_METHOD'] : '';
$federation_oidc_raw_query = isset($_SERVER['QUERY_STRING']) ? (string)$_SERVER['QUERY_STRING'] : '';
$federation_oidc_script_name = isset($_SERVER['SCRIPT_NAME']) ? (string)$_SERVER['SCRIPT_NAME'] : '';

define('FA_LOGOUT_PHP_FILE', '');
$page_security = 'SA_OPEN';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/federation_oidc_existing_link_callback_route.inc');
include_once($path_to_root.'/includes/federation_first_link_administration.inc');

federation_oidc_callback_route_security_headers();
$request = federation_oidc_callback_route_parse_request($federation_oidc_raw_method, $federation_oidc_raw_query);
$now = time();
if (!session_transport_is_https() || !federation_oidc_callback_route_is_anonymous_session()
    || !federation_oidc_callback_route_consume_attempt($now) || !is_array($request)) {
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'restart');
    exit;
}

$binding = federation_oidc_callback_route_pending_binding($request['state'], $now);
if (!is_array($binding)
    || !federation_oidc_callback_route_redirect_matches_entrypoint((string)$binding['redirect_uri'], $federation_oidc_script_name)) {
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'restart');
    exit;
}

$company = (int)$binding['company_id'];
if (!isset($db_connections[$company])) {
    federation_oidc_callback_route_clear_pending($request['state']);
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'failed');
    exit;
}
$_SESSION['wa_current_user']->set_company($company);
set_global_connection($company);
db_set_encoding($_SESSION['language']->encoding);

if (!federation_oidc_callback_route_transaction_matches_binding($request['state'], $binding)) {
    federation_oidc_callback_route_clear_pending($request['state']);
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'restart');
    exit;
}

if ($request['kind'] === 'error') {
    federation_oidc_callback_route_terminalize_authorization_error($company, $request['state'], $request['error']);
    federation_oidc_callback_route_clear_pending($request['state']);
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'failed');
    exit;
}

include_once($path_to_root.'/includes/federation_oidc_existing_link_orchestration.inc');
$result = federation_oidc_orchestrate_existing_link_callback(
    $company,
    $request['state'],
    $request['code'],
    (string)$binding['redirect_uri']
);
$status = is_array($result) && isset($result['status']) ? (string)$result['status'] : 'denied';
if ($status === 'authenticated') {
    federation_first_link_pending_request_clear();
    federation_oidc_callback_route_clear_pending($request['state']);
    federation_oidc_callback_route_reset_attempts();
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'authenticated');
    exit;
}
if ($status === 'retry_later') {
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'retry');
    exit;
}
if ($status === 'link_required' && isset($result['verifier_decision_id'])
    && federation_first_link_pending_request_bind($company, (int)$result['verifier_decision_id'], time())) {
    federation_oidc_callback_route_clear_pending($request['state']);
    federation_oidc_callback_route_redirect($federation_oidc_script_name, 'link_required');
    exit;
}
federation_first_link_pending_request_clear();
federation_oidc_callback_route_clear_pending($request['state']);
federation_oidc_callback_route_redirect($federation_oidc_script_name, 'failed');
exit;
