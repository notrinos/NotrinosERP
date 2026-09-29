<?php
/** Screen-only HRM-ANL-001 bounded headcount inquiry. */
$page_security = 'SA_HRMREPORTS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_headcount_db.inc');

page(_('Governed Headcount'));
if (!function_exists('get_company_pref') || version_compare((string)get_company_pref('version_id', true), '1.0.914', '<')) {
    display_error(_('Governed headcount inquiry is not enabled until database version 1.0.914.'));
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
        $result = hrm_anl_001_execute_headcount_metric($as_of_utc, HRM_ANL_001_HEADCOUNT_PURPOSE, false, false, $error);
        if ($result === false)
            display_error(_('Governed headcount query rejected: ').$error);
    }
}

display_note(_('Screen-only governed aggregate. Company scope is taken from the authenticated session. The runtime requires the exact two-person-approved built-in catalog, reconciles independent source totals, suppresses cohorts below five, and records immutable snapshot/query evidence. Export, drill-down, department detail and employee identifiers remain unavailable.'), 0, 1);

start_form();
hidden('_token', ensure_csrf_token());
hidden('anl_action', 'run');
start_table(TABLESTYLE2);
date_row(_('As at date:'), 'as_at_date', isset($_POST['as_at_date']) ? $_POST['as_at_date'] : Today());
end_table(1);
submit_center('run_governed_headcount', _('Run Governed Headcount'));
end_form();

if (is_array($result)) {
    start_table(TABLESTYLE, "width='70%'");
    table_header(array(_('Metric'), _('As of UTC'), _('Result'), _('Unit'), _('Suppression band')));
    start_row();
    label_cell(htmlspecialchars((string)$result['metric_key'], ENT_QUOTES, 'UTF-8'));
    label_cell(htmlspecialchars((string)$result['as_of_utc'], ENT_QUOTES, 'UTF-8'));
    if (!empty($result['suppressed']))
        label_cell(_('Suppressed'));
    else
        label_cell(htmlspecialchars((string)$result['value'], ENT_QUOTES, 'UTF-8'));
    label_cell(htmlspecialchars((string)$result['unit_code'], ENT_QUOTES, 'UTF-8'));
    label_cell(htmlspecialchars((string)$result['cohort_size_band_code'], ENT_QUOTES, 'UTF-8'));
    end_row();
    end_table(1);
    display_note(_('Evidence recorded under snapshot ID ').(int)$result['snapshot_id']._(' and query audit ID ').(int)$result['query_audit_id']._('. No source-total hash, employee row, department breakdown, export file or drill-down token is exposed by this screen.'));
}

end_page();
?>
