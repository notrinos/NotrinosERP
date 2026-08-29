<?php
/** PAY-SEC-004 public existing-link OIDC authorization-start/login-entry route. */
$federation_oidc_raw_method = isset($_SERVER['REQUEST_METHOD']) ? (string)$_SERVER['REQUEST_METHOD'] : '';
$federation_oidc_raw_content_type = isset($_SERVER['CONTENT_TYPE']) ? (string)$_SERVER['CONTENT_TYPE'] : '';
$federation_oidc_raw_body = @file_get_contents('php://input');
$federation_oidc_script_name = isset($_SERVER['SCRIPT_NAME']) ? (string)$_SERVER['SCRIPT_NAME'] : '';

define('FA_LOGOUT_PHP_FILE', '');
$page_security = 'SA_OPEN';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/federation_oidc_existing_link_authorization_start_route.inc');
include_once($path_to_root.'/includes/federation_oidc_transaction.inc');
include_once($path_to_root.'/includes/federation_first_link_administration.inc');

federation_oidc_authorization_start_security_headers();
$request = federation_oidc_authorization_start_parse_request(
    $federation_oidc_raw_method,
    $federation_oidc_raw_content_type,
    is_string($federation_oidc_raw_body) ? $federation_oidc_raw_body : ''
);
$now = time();
if (!session_transport_is_https() || !federation_oidc_authorization_start_is_anonymous_session()
    || !is_array($request) || !federation_oidc_authorization_start_consume_csrf($request['csrf'], $now)
    || !federation_oidc_authorization_start_consume_attempt($now)) {
    federation_oidc_authorization_start_redirect_local($federation_oidc_script_name, 'restart');
    exit;
}

federation_first_link_pending_request_clear();
$company = (int)$request['company_id'];
if (!isset($db_connections[$company])) {
    federation_oidc_authorization_start_redirect_local($federation_oidc_script_name, 'failed');
    exit;
}
$_SESSION['wa_current_user']->set_company($company);
set_global_connection($company);
db_set_encoding($_SESSION['language']->encoding);

$selected = federation_oidc_authorization_start_enabled_configuration();
$callback_path = federation_oidc_authorization_start_callback_path($federation_oidc_script_name);
if (!is_array($selected) || $callback_path === false) {
    federation_oidc_authorization_start_redirect_local($federation_oidc_script_name, 'failed');
    exit;
}
$redirect_uri = federation_oidc_resolve_configured_redirect_uri($company, (int)$selected['verifier_config_id'], $callback_path);
if ($redirect_uri === false) {
    federation_oidc_authorization_start_redirect_local($federation_oidc_script_name, 'failed');
    exit;
}

$issued = federation_oidc_issue_transaction($company, (int)$selected['verifier_config_id'], $redirect_uri, FEDERATION_OIDC_TRANSACTION_DEFAULT_TTL);
if (!is_array($issued) || empty($issued['browser_callback_bound']) || !isset($issued['authorization_url'])
    || !federation_oidc_valid_https_uri((string)$issued['authorization_url'], true)) {
    federation_oidc_authorization_start_redirect_local($federation_oidc_script_name, 'failed');
    exit;
}
federation_oidc_authorization_start_reset_attempts();
federation_oidc_authorization_start_redirect_provider((string)$issued['authorization_url']);
exit;
