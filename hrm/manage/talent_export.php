<?php
/** HRM-TAL-001 bounded exact-employee CSV download route. */
$page_security='SA_HRSETTINGS';
$path_to_root='../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_export_db.inc');

if (!function_exists('get_company_pref') || !hrm_tal_001_export_browser_allowed_for_database_version((string)get_company_pref('version_id', true))) {
    page(_('Talent Export')); display_error(_('Governed talent export is not enabled for this company version.')); end_page(); exit;
}
if (!isset($_SERVER['REQUEST_METHOD']) || strtoupper((string)$_SERVER['REQUEST_METHOD']) !== 'POST' || !check_csrf_token()) {
    page(_('Talent Export')); display_error(_('Invalid talent export request.')); end_page(); exit;
}
$employee_id = isset($_POST['employee_id']) ? trim((string)$_POST['employee_id']) : '';
$error = null;
$export = hrm_tal_001_export_exact_employee_csv($employee_id, $error);
if ($export === false) {
    page(_('Talent Export')); display_error(_('Talent export rejected: ').$error); end_page(); exit;
}
while (ob_get_level()) ob_end_clean();
header('X-Content-Type-Options: nosniff');
header('Cache-Control: no-store, private');
header('Pragma: no-cache');
header('Content-Type: text/csv; charset=UTF-8');
header('Content-Disposition: attachment; filename="'.$export['filename'].'"');
echo $export['content'];
exit;
?>
