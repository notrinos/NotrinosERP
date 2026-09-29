<?php
/** Screen-only HRM-ANL-001 bounded department headcount aggregate inquiry. */
$page_security = 'SA_HRMREPORTS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_department_headcount_db.inc');

page(_('Governed Headcount by Department'));
if (!function_exists('get_company_pref') || version_compare((string)get_company_pref('version_id', true), '1.0.918', '<')) {
    display_error(_('Governed department headcount is not enabled until database version 1.0.918.'));
    end_page();
    exit;
}

$result = false;
$error = null;
$action = isset($_POST['anl_action']) ? (string)$_POST['anl_action'] : '';
if ($action !== '' && !check_csrf_token()) {
    display_error(_('Invalid CSRF token.'));
} elseif ($action === 'run') {
    $as_at = isset($_POST['as_at_date']) ? (string)$_POST['as_at_date'] : '';
    if (!is_date($as_at)) {
        display_error(_('A valid as-at date is required.'));
    } else {
        $as_of_utc = date2sql($as_at).' 23:59:59';
        $result = hrm_anl_001_execute_department_headcount($as_of_utc, HRM_ANL_001_DEPARTMENT_HEADCOUNT_PURPOSE, false, false, $error);
        if ($result === false)
            display_error(_('Governed department headcount query rejected: ').$error);
    }
}

display_note(_('Screen-only department aggregates for the already-approved active-headcount metric. Small cohorts are suppressed and complementary suppression is applied when needed to prevent subtraction disclosure. Employee detail, arbitrary filters, export and drill-down remain unavailable.'), 0, 1);

start_form();
hidden('_token', ensure_csrf_token());
hidden('anl_action', 'run');
start_table(TABLESTYLE2);
date_row(_('As at date:'), 'as_at_date', isset($_POST['as_at_date']) ? $_POST['as_at_date'] : Today());
end_table(1);
submit_center('run_governed_department_headcount', _('Run Department Headcount'));
end_form();

if (is_array($result)) {
    start_table(TABLESTYLE, "width='75%'");
    table_header(array(_('Department'), _('Result'), _('Unit'), _('Suppression')));
    foreach ($result['rows'] as $row) {
        start_row();
        label_cell(htmlspecialchars((string)$row['department_name'], ENT_QUOTES, 'UTF-8'));
        if (!empty($row['suppressed']))
            label_cell(_('Suppressed'));
        else
            label_cell(htmlspecialchars((string)$row['value'], ENT_QUOTES, 'UTF-8'));
        label_cell(htmlspecialchars((string)$row['unit_code'], ENT_QUOTES, 'UTF-8'));
        label_cell(htmlspecialchars((string)$row['suppression_reason'], ENT_QUOTES, 'UTF-8'));
        end_row();
    }
    end_table(1);
    display_note(_('Evidence recorded under snapshot ID ').(int)$result['snapshot_id']._(' and query audit ID ').(int)$result['query_audit_id']._('. No source-total hash, raw suppressed count, employee identifier, export file or drill-down token is exposed by this screen.'));
}

end_page();
?>
