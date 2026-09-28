<?php
/** PAY-API-001 bounded public machine-authenticated read-only route. No payroll business-data dispatch is enabled. */
$pay_api_001_raw_method = isset($_SERVER['REQUEST_METHOD']) ? (string)$_SERVER['REQUEST_METHOD'] : '';
$pay_api_001_raw_query = isset($_SERVER['QUERY_STRING']) ? (string)$_SERVER['QUERY_STRING'] : '';
$pay_api_001_raw_server = $_SERVER;
define('FA_LOGOUT_PHP_FILE', '');
$page_security = 'SA_OPEN';
$path_to_root = '.';
include_once($path_to_root.'/includes/session_security.inc');
include_once($path_to_root.'/includes/pay_api_001_public_readonly_route.inc');

pay_api_001_public_route_security_headers();
$secure = session_transport_is_https();
if ($secure !== true) pay_api_001_public_route_respond(426);
if (strtoupper($pay_api_001_raw_method) !== 'GET') pay_api_001_public_route_respond(405);
$request = pay_api_001_public_route_parse_query($pay_api_001_raw_query);
$headers = pay_api_001_public_route_headers_from_server($pay_api_001_raw_server);
if ($request === false || $headers === false) pay_api_001_public_route_respond(400);

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/hrm/includes/db/pay_api_001_http_runtime_db.inc');

if (!isset($db_connections[$request['company_id']]) || !set_global_connection($request['company_id'])) pay_api_001_public_route_respond(401);
if (isset($_SESSION['language']) && is_object($_SESSION['language'])) db_set_encoding($_SESSION['language']->encoding);
if (!pay_api_001_public_readonly_http_route_enabled()) pay_api_001_public_route_respond(503);
$error = null;
$result = pay_api_001_preroute_read_request(
    $pay_api_001_raw_method,
    $secure,
    $headers,
    $request['query'],
    PAY_API_001_PUBLIC_READONLY_ROUTE_PATH,
    $request['company_id'],
    $request['resource_key'],
    $request['contract_version'],
    $request['object_scope_type'],
    $request['object_scope_key'],
    $request['limit'],
    $request['cursor_sha256'],
    null,
    $error
);
pay_api_001_public_route_respond(pay_api_001_public_route_http_status($result,$error));
?>
