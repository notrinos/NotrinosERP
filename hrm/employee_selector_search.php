<?php
/** Read-only employee choices for a previously authorized, session-bound control. */
$path_to_root = '..';
$page_security = 'SA_DENIED'; // The registered control supplies the real page boundary.
header('Cache-Control: no-store, private');
header('X-Content-Type-Options: nosniff');
/** Keep bootstrap login/errors and fatal SQL details out of this JSON endpoint. */
function hrm_employee_selector_json_filter($output)
{
    if (isset($GLOBALS['hrm_employee_selector_response'])) {
        $response = $GLOBALS['hrm_employee_selector_response'];
    } else {
        $authenticated = isset($_SESSION['wa_current_user']) && $_SESSION['wa_current_user']->logged_in();
        $response = array('status' => $authenticated ? 503 : 401,
            'data' => array('error' => $authenticated ? 'search_unavailable' : 'login_required'));
    }
    if (!headers_sent()) {
        header_remove('Location');
        http_response_code($response['status']);
        header('Content-Type: application/json; charset=UTF-8');
    }
    return json_encode($response['data']);
}
ob_start('hrm_employee_selector_json_filter');
$hrm_employee_selector_output_level = ob_get_level();
// Reject login/business fields before bootstrap can act on them.
if (!empty($_GET) || !empty($_FILES)
    || array_diff(array_keys($_POST), array('control','term','cursor','selected','snapshot','_token')))
    hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
foreach (array('control' => 128, 'cursor' => 128, 'term' => 1024) as $field => $maximum)
    if (isset($_POST[$field]) && (!is_string($_POST[$field]) || strlen($_POST[$field]) > $maximum
        || preg_match('//u', $_POST[$field]) !== 1))
        hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
if (isset($_POST['snapshot']) && ($_POST['snapshot'] !== '1'
    || isset($_POST['term']) || isset($_POST['cursor']) || isset($_POST['selected'])))
    hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
if (isset($_POST['selected'])) {
    if (!is_array($_POST['selected']) || count($_POST['selected']) > 50)
        hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
    foreach ($_POST['selected'] as $id)
        if (!is_string($id) || strlen($id) > 255 || preg_match('//u', $id) !== 1)
            hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
}
if ($_SERVER['REQUEST_METHOD'] !== 'POST' || !isset($_POST['_token']) || !is_string($_POST['_token'])) {
    hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
}
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/ui/employee_selector_search.inc');
include_once($path_to_root.'/hrm/includes/db/employee_projection_control_db.inc');
include_once($path_to_root.'/reporting/includes/report_artifact_security.inc');

/** Send only JSON, discarding buffered shell/errors rather than leaking SQL/PII. */
function hrm_employee_selector_json_response($data, $status = 200)
{
    $GLOBALS['hrm_employee_selector_response'] = array('data' => $data, 'status' => $status);
    while (ob_get_level() > $GLOBALS['hrm_employee_selector_output_level']) ob_end_clean();
    exit;
}

$token = isset($_POST['control']) && is_string($_POST['control']) ? $_POST['control'] : '';
$grant = isset($_SESSION['hrm_employee_selectors'][$token]) ? $_SESSION['hrm_employee_selectors'][$token] : false;
if (!$grant || $grant['expires'] < time() || !check_csrf_token())
    hrm_employee_selector_json_response(array('error' => 'control_expired'), 403);
$settings = $grant['settings'];
$company = hrm_person_worker_current_company_id();
if ($settings['actor'] !== hrm_person_worker_actor_id() || $settings['company'] !== $company
    || !isset($db_connections[$company]['dbname'], $db_connections[$company]['tbpref'])
    || $settings['database'] !== $db_connections[$company]['dbname']
    || $settings['prefix'] !== $db_connections[$company]['tbpref']
    || !hrm_employee_selector_search_scope_is_current()
    || !$SysPrefs->db_ok || !$_SESSION['wa_current_user']->check_user_access()
    || !hrm_employee_selector_refresh_access($settings['area']))
    hrm_employee_selector_json_response(array('error' => 'access_denied'), 403);
// Refresh the established role representation before the independent name check.
$term = isset($_POST['term']) ? $_POST['term'] : '';
$cursor = isset($_POST['cursor']) ? $_POST['cursor'] : '';
$after = null;
$revision = null;
if (!is_string($cursor) || !is_string($term))
    hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
if ($cursor !== '') {
    $continuation = isset($_SESSION['hrm_employee_selector_cursors'][$cursor])
        ? $_SESSION['hrm_employee_selector_cursors'][$cursor] : false;
    if (!$continuation || $continuation['control'] !== $token || $continuation['term'] !== $term
        || $continuation['expires'] < time())
        hrm_employee_selector_json_response(array('error' => 'invalid_continuation'), 400);
    $after = $continuation['after'];
    $revision = $continuation['revision'];
}
$selected = isset($_POST['selected']) ? $_POST['selected'] : null;
try {
    // Preserve the existing full verification, including cold drift/parity costs.
    hrm_employee_projection_require_readable();
    if (isset($_POST['snapshot'])) {
        // Refresh the whole document snapshot; a popup never patches a stale list.
        $snapshot = hrm_employee_selector_snapshot_payload($settings, $grant['expires']);
        if ($snapshot === false)
            hrm_employee_selector_json_response(array('error' => 'choices_changed'), 409);
        hrm_employee_selector_json_response($snapshot);
    }
    $found = hrm_employee_selector_read_page($settings, $term, $after, $selected, $revision, true);
    if (is_array($found) && !empty($found['changed']))
        hrm_employee_selector_json_response(array('error' => 'choices_changed'), 409);
    if ($found === false)
        hrm_employee_selector_json_response(array('error' => 'invalid_request'), 400);
    $results = array();
    if ($selected === null && $after === null && $settings['spec_option'] !== false
        && (trim($term) === '' || trim($term) === '*' || stripos($settings['spec_option'], trim($term)) !== false))
        $results[] = array('id' => $settings['spec_id'], 'text' => $settings['spec_option']);
    foreach ($found['rows'] as $row)
        $results[] = array('id' => (string)$row['employee_id'],
            'text' => hrm_employee_selector_result_label($row, $settings, $found['batch']),
            'department' => !empty($settings['manager']) ? '' : $row['department_name'],
            'inactive' => !empty($row['inactive']));
    $next = '';
    if ($found['more']) {
        if (!isset($_SESSION['hrm_employee_selector_cursors'])) $_SESSION['hrm_employee_selector_cursors'] = array();
        foreach ($_SESSION['hrm_employee_selector_cursors'] as $key => $continuation)
            if ($continuation['expires'] < time()) unset($_SESSION['hrm_employee_selector_cursors'][$key]);
        if (count($_SESSION['hrm_employee_selector_cursors']) >= 100) array_shift($_SESSION['hrm_employee_selector_cursors']);
        $next = random_id();
        $_SESSION['hrm_employee_selector_cursors'][$next] = array('control' => $token, 'term' => $term,
            'after' => $found['after'], 'revision' => $found['revision'], 'expires' => time()+1800);
    }
    hrm_employee_selector_json_response(array('results' => $results,
        'pagination' => array('more' => $found['more']), 'cursor' => $next));
} catch (Exception $error) {
    hrm_employee_selector_json_response(array('error' => 'search_unavailable'), 503);
}
