<?php
/** PAY-SEC-004 public anonymous recovery-redemption route. */
$account_recovery_raw_method = isset($_SERVER['REQUEST_METHOD']) ? (string)$_SERVER['REQUEST_METHOD'] : '';
$account_recovery_raw_content_type = isset($_SERVER['CONTENT_TYPE']) ? (string)$_SERVER['CONTENT_TYPE'] : '';
$account_recovery_raw_body = @file_get_contents('php://input');
$account_recovery_remote_addr = isset($_SERVER['REMOTE_ADDR']) ? (string)$_SERVER['REMOTE_ADDR'] : '';
$account_recovery_script_name = isset($_SERVER['SCRIPT_NAME']) ? (string)$_SERVER['SCRIPT_NAME'] : '';

define('FA_LOGOUT_PHP_FILE', '');
$page_security = 'SA_OPEN';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/account_recovery_redemption.inc');
include_once($path_to_root.'/includes/account_recovery_redemption_route.inc');

account_recovery_route_security_headers();
if (!account_recovery_route_is_anonymous_session()) {
    header('Location: '.account_recovery_route_local_index($account_recovery_script_name), true, 303);
    exit;
}

function account_recovery_public_page_start($title)
{
    global $path_to_root;
    echo "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    echo '<title>'.account_recovery_route_escape($title)."</title><link href='".$path_to_root."/themes/default/default.css' rel='stylesheet'>";
    echo '<link href="'.$path_to_root.'/themes/default/local_style/access.css" rel="stylesheet"></head><body id="loginscreen"><div class="login-card">';
    echo '<div class="login-logo"><img src="'.$path_to_root.'/themes/default/images/notrinos_erp.png" alt="NotrinosERP" height="50"></div>';
    echo '<div class="login-subtitle">'.account_recovery_route_escape($title).'</div>';
}
function account_recovery_public_page_end()
{
    global $path_to_root;
    echo '<div class="login-message"><a href="'.$path_to_root.'/index.php">Return to login</a></div></div></body></html>';
}
function account_recovery_public_company_options($selected = null)
{
    global $db_connections;
    if (!isset($db_connections) || !is_array($db_connections)) return '';
    $html = '';
    foreach ($db_connections as $i=>$connection) {
        if (!is_int($i) && !ctype_digit((string)$i)) continue;
        $id = (int)$i;
        $name = isset($connection['name']) ? (string)$connection['name'] : (string)$id;
        $html .= "<option value='".$id."'".($selected !== null && (int)$selected === $id ? ' selected' : '').'>'.account_recovery_route_escape($name)."</option>";
    }
    return $html;
}
function account_recovery_public_begin_form($csrf)
{
    global $path_to_root;
    account_recovery_public_page_start('Account recovery');
    echo '<div class="login-message">Use a recovery code ID and the matching recovery code. Account existence is never confirmed on this page.</div>';
    echo '<form method="post" action="'.$path_to_root.'/access/account_recovery.php" autocomplete="off">';
    echo '<input type="hidden" name="action" value="begin"><input type="hidden" name="recovery_csrf" value="'.account_recovery_route_escape($csrf).'">';
    echo '<div class="login-field"><label>Company</label><select name="company_login_name" required>'.account_recovery_public_company_options().'</select></div>';
    echo '<div class="login-field"><label>User name</label><input class="input" name="login_id" type="text" minlength="4" maxlength="60" autocomplete="username" required></div>';
    echo '<div class="login-field"><label>Recovery code ID</label><input class="input" name="recovery_material_id" type="text" inputmode="numeric" maxlength="20" required></div>';
    echo '<div class="login-submit"><input type="submit" value="Continue &#8250;"></div></form>';
    account_recovery_public_page_end();
}
function account_recovery_public_complete_form($company, $handle, $csrf)
{
    global $path_to_root;
    account_recovery_public_page_start('Complete account recovery');
    echo '<div class="login-message">If the recovery claim can be accepted, enter the matching recovery code and a new password. The response intentionally does not confirm whether the account or code ID exists.</div>';
    echo '<form method="post" action="'.$path_to_root.'/access/account_recovery.php" autocomplete="off">';
    echo '<input type="hidden" name="action" value="complete"><input type="hidden" name="company_login_name" value="'.(int)$company.'">';
    echo '<input type="hidden" name="request_handle" value="'.account_recovery_route_escape($handle).'"><input type="hidden" name="recovery_csrf" value="'.account_recovery_route_escape($csrf).'">';
    echo '<div class="login-field"><label>Recovery code</label><input class="input" name="recovery_secret" type="password" minlength="68" maxlength="68" autocomplete="one-time-code" required></div>';
    echo '<div class="login-field"><label>New password</label><input class="input" name="new_password" type="password" minlength="4" maxlength="512" autocomplete="new-password" required></div>';
    echo '<div class="login-field"><label>Confirm new password</label><input class="input" name="new_password_confirm" type="password" minlength="4" maxlength="512" autocomplete="new-password" required></div>';
    echo '<div class="login-submit"><input type="submit" value="Reset password &#8250;"></div></form>';
    account_recovery_public_page_end();
}
function account_recovery_public_result($success)
{
    account_recovery_public_page_start($success ? 'Password reset completed' : 'Recovery request not accepted');
    if ($success)
        echo '<div class="login-message">Password reset completed. Existing durable sessions and remaining recovery material were revoked. Sign in with the new password and create a fresh recovery set before relying on account recovery again.</div>';
    else
        echo '<div class="login-message">Recovery request not accepted. Start again with current recovery material or contact an authorized administrator.</div>';
    account_recovery_public_page_end();
}

$method = $account_recovery_raw_method;
if ($method === 'GET') {
    $csrf = account_recovery_route_issue_csrf();
    if (!is_string($csrf)) { http_response_code(503); account_recovery_public_result(false); exit; }
    account_recovery_public_begin_form($csrf);
    exit;
}
if ($method !== 'POST') {
    http_response_code(405); header('Allow: GET, POST'); account_recovery_public_result(false); exit;
}
if (!session_transport_is_https()) {
    http_response_code(426); account_recovery_public_result(false); exit;
}
$request = account_recovery_route_parse_request($method, $account_recovery_raw_content_type,
    is_string($account_recovery_raw_body) ? $account_recovery_raw_body : '');
$now = time();
$source_bucket_hash = account_recovery_route_source_bucket_hash($account_recovery_remote_addr);
if (!is_array($request) || $source_bucket_hash === false
    || !account_recovery_route_consume_csrf($request['csrf'], $now)
    || !account_recovery_route_consume_attempt($now)) {
    account_recovery_public_result(false); exit;
}
$company = (int)$request['company_id'];
if (!isset($db_connections[$company])) { account_recovery_public_result(false); exit; }
$_SESSION['wa_current_user']->set_company($company);
set_global_connection($company);
db_set_encoding($_SESSION['language']->encoding);
if (!account_recovery_route_activation_ready()) { http_response_code(503); account_recovery_public_result(false); exit; }

if ($request['action'] === 'begin') {
    $issued = account_recovery_route_issue_request($company, $request['login_id'], $request['material_id'], $source_bucket_hash, $now);
    $handle = is_array($issued) && !empty($issued['ok']) && isset($issued['request_secret']) ? (string)$issued['request_secret'] : account_recovery_route_fake_request_handle();
    $csrf = account_recovery_route_issue_csrf($now);
    if (!is_string($handle) || !is_string($csrf)) { account_recovery_public_result(false); exit; }
    account_recovery_public_complete_form($company, $handle, $csrf);
    exit;
}

if ($request['new_password'] !== $request['new_password_confirm']) { account_recovery_public_result(false); exit; }
$result = account_recovery_redemption_reset_password($company, $request['request_handle'], $source_bucket_hash,
    $request['recovery_secret'], $request['new_password'], $now);
$success = is_array($result) && !empty($result['ok'])
    && (isset($result['external_code']) ? $result['external_code'] : '') === 'credential_reset_completed';
if ($success) account_recovery_route_reset_attempts();
account_recovery_public_result($success);
