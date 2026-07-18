<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
***********************************************************************/

/**
 * Authenticated controller for the Visual Formula Designer JSON surface.
 */

// Capture any legacy include-file prefix output so JSON responses remain valid.
ob_start();

$page_security = 'SA_DENIED';
$path_to_root = '.';
$formula_designer_requested_endpoint = isset($_GET['endpoint']) ? strtolower(trim((string)$_GET['endpoint'])) : '';
$formula_designer_requested_module = isset($_REQUEST['module']) ? strtolower(trim((string)$_REQUEST['module'])) : '';
$formula_designer_request_method = strtoupper(isset($_SERVER['REQUEST_METHOD']) ? (string)$_SERVER['REQUEST_METHOD'] : 'GET');

include_once($path_to_root.'/includes/session.inc');
require_once($path_to_root.'/includes/formula_designer/API/designer_api_security.inc');

$endpoint = $formula_designer_requested_endpoint;
$module = $formula_designer_requested_module;
$method = $formula_designer_request_method;
$required_method = formula_designer_api_required_method($endpoint);

if ($required_method === '' || formula_designer_api_allowed_areas($module) === array()) {
	formula_designer_api_log_access($module, $endpoint, 'denied_invalid_route', $method);
	formula_designer_api_json_response(array('ok' => false, 'error' => 'Unknown designer API route.'), 404);
}

if ($method !== $required_method) {
	formula_designer_api_log_access($module, $endpoint, 'denied_method', $method);
	formula_designer_api_json_response(array('ok' => false, 'error' => 'Request method is not allowed.'), 405);
}

if (!formula_designer_api_user_can_access($module)) {
	formula_designer_api_log_access($module, $endpoint, 'denied_authorization', $method);
	formula_designer_api_json_response(array('ok' => false, 'error' => 'Access denied.'), 403);
}

// session.inc removes POST data after a failed standard CSRF check. Requiring
// the surviving token here makes the API fail closed instead of processing an
// empty request after that rejection.
if ($method === 'POST' && (!isset($_POST['_token']) || $_POST['_token'] === '')) {
	formula_designer_api_log_access($module, $endpoint, 'denied_csrf', $method);
	formula_designer_api_json_response(array('ok' => false, 'error' => 'Invalid CSRF token.'), 403);
}

if (!formula_designer_api_rate_limit_allowed($module, $endpoint, $method)) {
	formula_designer_api_log_access($module, $endpoint, 'denied_rate_limit', $method);
	formula_designer_api_json_response(array('ok' => false, 'error' => 'Request rate limit exceeded.'), 429);
}

formula_designer_api_log_access($module, $endpoint, 'access_granted', $method);

define('FORMULA_DESIGNER_AUTHORIZED_CONTROLLER', true);
define('FORMULA_DESIGNER_API_NO_AUTO_RUN', true);
require_once($path_to_root.'/includes/formula_designer/API/DesignerAPI.php');

switch ($endpoint) {
	case 'fields':
		FormulaDesigner_API_DesignerAPI::renderFields();
		break;
	case 'functions':
		FormulaDesigner_API_DesignerAPI::renderFunctions();
		break;
	case 'templates':
		require_once($path_to_root.'/includes/formula_designer/API/DesignerTemplateAPI.php');
		FormulaDesigner_API_DesignerTemplateAPI::handleRequest();
		break;
	case 'validate':
		require_once($path_to_root.'/includes/formula_designer/API/DesignerValidateAPI.php');
		FormulaDesigner_API_DesignerValidateAPI::handleRequest();
		break;
	case 'explain':
		require_once($path_to_root.'/includes/formula_designer/API/DesignerExplainAPI.php');
		FormulaDesigner_API_DesignerExplainAPI::handleRequest();
		break;
	case 'ai':
		require_once($path_to_root.'/includes/formula_designer/API/DesignerAIApi.php');
		FormulaDesigner_API_DesignerAIApi::handleRequest();
		break;
}
