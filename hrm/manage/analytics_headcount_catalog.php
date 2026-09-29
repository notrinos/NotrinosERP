<?php
/** HRM-ANL-001 two-person catalog governance UI for the bounded headcount metric. */
$page_security = 'SA_HRMREPORTS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_catalog_db.inc');

page(_('Governed Headcount Catalog'));
if (!function_exists('get_company_pref') || version_compare((string)get_company_pref('version_id', true), '1.0.913', '<')) {
    display_error(_('Governed headcount catalog UI is not enabled until database version 1.0.913.'));
    end_page();
    exit;
}

$action = isset($_POST['anl_action']) ? (string)$_POST['anl_action'] : '';
if ($action !== '' && !check_csrf_token()) {
    display_error(_('Invalid CSRF token.'));
} elseif ($action === 'review') {
    $valid_from = trim(isset($_POST['valid_from_utc']) ? (string)$_POST['valid_from_utc'] : '');
    $expires_at = trim(isset($_POST['expires_at_utc']) ? (string)$_POST['expires_at_utc'] : '');
    $error = null;
    $result = hrm_anl_001_review_headcount_catalog($valid_from, $expires_at, $error);
    if ($result === false) {
        display_error(_('Catalog review rejected: ').$error);
    } else {
        display_notification(_('Review recorded. Give this immutable review SHA-256 to a different authorized approver: ').htmlspecialchars((string)$result['review_event_sha256'], ENT_QUOTES, 'UTF-8'));
    }
} elseif ($action === 'approve') {
    $review_sha = trim(isset($_POST['review_event_sha256']) ? (string)$_POST['review_event_sha256'] : '');
    $error = null;
    $result = hrm_anl_001_approve_headcount_catalog($review_sha, $error);
    if ($result === false) {
        display_error(_('Catalog approval rejected: ').$error);
    } else {
        display_notification(_('Exact built-in headcount catalog approved under immutable two-person custody.'));
    }
}

display_note(_('This page can only review and approve the built-in aggregate active-headcount definition and fixed small-cohort suppression policy. Review and approval must be performed by different authenticated users. It cannot create arbitrary metrics, enable export or drill-down, change HR/payroll data, or seed catalog rows automatically.'), 0, 1);

start_form();
hidden('_token', ensure_csrf_token());
hidden('anl_action', 'review');
start_table(TABLESTYLE2);
text_row_ex(_('Valid from UTC (YYYY-MM-DD HH:MM:SS):'), 'valid_from_utc', 24, 19);
text_row_ex(_('Expires at UTC (YYYY-MM-DD HH:MM:SS):'), 'expires_at_utc', 24, 19);
end_table(1);
submit_center('review_catalog', _('Record Catalog Review'));
end_form();

start_form();
hidden('_token', ensure_csrf_token());
hidden('anl_action', 'approve');
start_table(TABLESTYLE2);
text_row_ex(_('Review event SHA-256:'), 'review_event_sha256', 68, 64);
end_table(1);
submit_center('approve_catalog', _('Approve Reviewed Catalog'));
end_form();

end_page();
?>
