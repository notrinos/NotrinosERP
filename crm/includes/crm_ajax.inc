<?php
/**
 * JSON responses and request authorization for CRM action endpoints.
 */

/** Emit JSON without legacy bootstrap/error markup, then stop the request. */
function crm_ajax_response(array $payload, $status_code)
{
    global $before_box, $messages;
    $before_box = '';
    $messages = array();
    while (ob_get_level() > 0) {
        ob_end_clean();
    }
    http_response_code($status_code);
    header('Content-Type: application/json; charset=UTF-8');
    header('Cache-Control: no-store');
    header('X-Content-Type-Options: nosniff');
    echo json_encode($payload);
    exit;
}

/** Enforce the page's existing permission and POST/CSRF before CRM reads or writes. */
function crm_ajax_require_access($page_security)
{
    global $SysPrefs;
    if (!isset($_SESSION['wa_current_user']) || !$_SESSION['wa_current_user']->logged_in()) {
        crm_ajax_response(array('error' => 'Authentication required'), 401);
    }
    // Preserve check_page_security() account and upgrade restrictions without HTML.
    if (!$_SESSION['wa_current_user']->check_user_access() || !user_check_access($page_security)) {
        crm_ajax_response(array('error' => 'Access denied'), 403);
    }
    if (empty($SysPrefs->db_ok)) {
        crm_ajax_response(array('error' => 'Database upgrade required'), 503);
    }
    if (!isset($_SERVER['REQUEST_METHOD']) || $_SERVER['REQUEST_METHOD'] !== 'POST') {
        header('Allow: POST');
        crm_ajax_response(array('error' => 'POST required'), 405);
    }
    // Bootstrap clears POST on CSRF failure. Also validate here for login POSTs,
    // which take a different bootstrap branch, and reject malformed token types.
    if (!isset($_POST['_token'], $_SESSION['csrf_token'])
        || !is_string($_POST['_token']) || !is_string($_SESSION['csrf_token'])
        || !check_csrf_token()) {
        crm_ajax_response(array('error' => 'Invalid CSRF token'), 403);
    }
}
