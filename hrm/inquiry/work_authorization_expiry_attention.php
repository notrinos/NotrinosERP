<?php
/**
 * HRM-FND-007 permissioned Work Authorization expiry-attention inquiry.
 *
 * Read-only, bounded, and metadata-only. No email, queue/task row, export,
 * acknowledgement, or Work Authorization mutation is performed here.
 */
$page_security = 'SA_EMPLOYEE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/work_authorization_expiry_attention_db.inc');

page(_("Work Authorization Expiry Attention"));

if (!hrm_work_authorization_user_can_view_masked()) {
    display_error(_("You are not authorized to view restricted Work Authorization expiry attention."));
    end_page();
    exit;
}

$as_of = date2sql(Today());
$rows = get_hrm_work_authorization_expiry_attention_feed($as_of, 100);
if ($rows === false) {
    display_error(_("Work Authorization expiry attention is unavailable because governed policy/readiness custody is ambiguous or incomplete."));
    end_page();
    exit;
}

display_note(_("Read-only restricted view. Results are capped at 100 and contain no permit identifier, sponsor, restriction text, evidence identifier, email-delivery state, or payroll decision."), 0, 1);

start_table(TABLESTYLE, "width='100%'");
$th = array(
    _("Worker"),
    _("Jurisdiction"),
    _("Policy"),
    _("Valid To"),
    _("Days Remaining")
);
table_header($th);

$k = 0;
foreach ($rows as $row) {
    alt_table_row_color($k);
    label_cell((string)(int)$row['worker_id']);
    label_cell(htmlspecialchars((string)$row['issuing_jurisdiction'], ENT_QUOTES, 'UTF-8'));
    label_cell(htmlspecialchars((string)$row['policy_code'], ENT_QUOTES, 'UTF-8'));
    label_cell(htmlspecialchars((string)$row['valid_to'], ENT_QUOTES, 'UTF-8'));
    label_cell((string)(int)$row['days_remaining']);
    end_row();
}
if (!$rows)
    label_row(_("Status"), _("No governed Work Authorization expiry-attention candidates are currently available."), "colspan=1", "colspan=4");
end_table(1);

end_page();
