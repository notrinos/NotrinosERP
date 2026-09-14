<?php
/** HRM-FND-007 restricted Work Authorization retention/legal-hold review. */
$page_security = 'SA_EMPLOYEE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/work_authorization_retention_review_db.inc');

page(_("Work Authorization Retention Review"));
if (!hrm_work_authorization_user_can_view_masked()) {
    display_error(_("You are not authorized to review restricted Work Authorization retention state."));
    end_page(); exit;
}
$as_of = date2sql(Today());
$rows = get_hrm_work_authorization_retention_review_feed($as_of, 100);
if ($rows === false) {
    display_error(_("Work Authorization retention review is unavailable because approved retention or legal-hold governance custody is incomplete or ambiguous."));
    end_page(); exit;
}
display_note(_("Read-only restricted review. No row in this page authorizes deletion or anonymization. Results are capped at 100 and expose no permit, sponsor, restriction, evidence-document identifier, recipient, or delivery-channel data."), 0, 1);
start_table(TABLESTYLE, "width='100%'");
table_header(array(_("Worker"),_("Jurisdiction"),_("Type"),_("Valid To"),_("Retained Until"),_("Active Holds"),_("Evidence Locked"),_("Review State")));
$k=0;
foreach ($rows as $row) {
    alt_table_row_color($k);
    label_cell((string)(int)$row['worker_id']);
    label_cell(htmlspecialchars((string)$row['issuing_jurisdiction'],ENT_QUOTES,'UTF-8'));
    label_cell(htmlspecialchars((string)$row['authorization_type'],ENT_QUOTES,'UTF-8'));
    label_cell(htmlspecialchars((string)($row['valid_to']===null?'':$row['valid_to']),ENT_QUOTES,'UTF-8'));
    label_cell(htmlspecialchars((string)($row['retained_until']===null?'':$row['retained_until']),ENT_QUOTES,'UTF-8'));
    label_cell((string)(int)$row['active_hold_count']);
    label_cell(!empty($row['evidence_retention_locked'])?_("Yes"):_('No'));
    label_cell(htmlspecialchars((string)$row['review_state'],ENT_QUOTES,'UTF-8'));
    end_row();
}
if (!$rows) label_row(_("Status"),_("No Work Authorization records are available for governed retention review."),"colspan=1","colspan=7");
end_table(1); end_page();
