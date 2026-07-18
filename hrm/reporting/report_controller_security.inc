<?php
/**
 * Require an HRM report implementation to run through its registered wrapper.
 *
 * The wrapper supplies normal session bootstrap and the report permission.
 * Direct implementation URLs have no supported compatibility contract.
 *
 * @param int $expected_report_id
 * @return void
 */
function hrm_require_report_controller($expected_report_id) {
	if (defined('HRM_REPORT_CONTROLLER_ID')
		&& (int)HRM_REPORT_CONTROLLER_ID === (int)$expected_report_id) {
		return;
	}

	error_log(sprintf(
		'[HRM_REPORT_SECURITY] report_id=%d outcome=direct_route_denied',
		(int)$expected_report_id
	));
	http_response_code(404);
	header('Cache-Control: no-store, private');
	header('X-Content-Type-Options: nosniff');
	echo 'Report route not found.';
	exit;
}
