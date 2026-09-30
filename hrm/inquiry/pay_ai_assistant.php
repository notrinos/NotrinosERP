<?php
/** Screen-only PAY-AI-001 deterministic headcount assistant and review surface. */
$page_security = 'SA_HRMREPORTS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/hrm/includes/db/pay_ai_001_runtime_db.inc');

page(_('Assistive Headcount Review'));
if (!function_exists('get_company_pref') || version_compare((string)get_company_pref('version_id', true), '1.0.936', '<')) {
    display_error(_('PAY-AI-001 assistive review is not enabled until database version 1.0.936.'));
    end_page(); exit;
}
if (pay_ai_001_force_disabled() || pay_ai_001_company_opted_out()) {
    display_error(_('PAY-AI-001 is disabled or opted out for this deployment.'));
    end_page(); exit;
}

$analysis = false;
$error = null;
$action = isset($_POST['pai_action']) ? (string)$_POST['pai_action'] : '';
if ($action !== '' && !check_csrf_token()) {
    display_error(_('Invalid CSRF token.'));
} elseif ($action === 'analyze') {
    $capability = isset($_POST['capability']) ? (string)$_POST['capability'] : '';
    $current_date = isset($_POST['current_date']) ? (string)$_POST['current_date'] : '';
    $baseline_date = isset($_POST['baseline_date']) ? (string)$_POST['baseline_date'] : '';
    if (!is_date($current_date) || !is_date($baseline_date)) {
        display_error(_('Valid current and baseline dates are required.'));
    } else {
        $analysis = pay_ai_001_run_headcount_assistance($capability, date2sql($current_date).' 23:59:59', date2sql($baseline_date).' 23:59:59', $error);
        if ($analysis === false) display_error(_('Assistive analysis rejected: ').$error);
    }
} elseif ($action === 'review') {
    $review = pay_ai_001_record_output_review($_POST['ai_output_id'] ?? null, $_POST['disposition_code'] ?? '', $_POST['reason_code'] ?? '', $error);
    if ($review === false) display_error(_('Assistive review rejected: ').$error);
    else display_notification(_('Human review recorded as immutable hash evidence. Review ID: ').(int)$review['ai_review_id']);
}

display_note(_('This screen compares only the already-governed total active-headcount aggregate. Output is deterministic, structured, assistive and non-authoritative. No external provider or network call is made; no employee identity, bank/tax detail, raw prompt, generated prose, export, drill-down, payroll mutation, approval, payment, GL or filing authority is available.'), 0, 1);

start_form(); hidden('_token', ensure_csrf_token()); hidden('pai_action', 'analyze');
start_table(TABLESTYLE2);
echo '<tr><td>'._('Capability:').'</td><td><select name="capability"><option value="anomaly_ranking">'._('Anomaly ranking').'</option><option value="variance_explanation">'._('Variance explanation').'</option></select></td></tr>';
date_row(_('Current as-at date:'), 'current_date', isset($_POST['current_date']) ? $_POST['current_date'] : Today());
date_row(_('Baseline as-at date:'), 'baseline_date', isset($_POST['baseline_date']) ? $_POST['baseline_date'] : add_days(Today(), -30));
end_table(1); submit_center('run_pay_ai_assistant', _('Run Assistive Analysis')); end_form();

if (is_array($analysis)) {
    $result = $analysis['result'];
    $custody = $analysis['custody'];
    display_note(_('Output label: ').htmlspecialchars((string)$result['generated_output_label'], ENT_QUOTES, 'UTF-8')._('. Human review is required. AI output ID: ').(int)$custody['ai_output_id']._('.'));
    start_table(TABLESTYLE, "width='75%'");
    table_header(array(_('Metric'), _('Score / Direction'), _('Magnitude'), _('Structured explanation')));
    foreach ($result['items'] as $item) {
        start_row();
        label_cell(htmlspecialchars((string)$item['metric_key'], ENT_QUOTES, 'UTF-8'));
        $score = isset($item['anomaly_score']) ? (string)(int)$item['anomaly_score'] : (string)($item['direction'] ?? '');
        label_cell(htmlspecialchars($score, ENT_QUOTES, 'UTF-8'));
        label_cell(htmlspecialchars((string)($item['magnitude_band'] ?? ''), ENT_QUOTES, 'UTF-8'));
        label_cell(htmlspecialchars((string)($item['explanation_code'] ?? $item['suggestion_code'] ?? 'structured_evidence_only'), ENT_QUOTES, 'UTF-8'));
        end_row();
    }
    if (!$result['items']) display_note(_('No bounded anomaly/variance item was produced for this comparison. This is not an approval or payroll decision.'));
    end_table(1);

    start_form(); hidden('_token', ensure_csrf_token()); hidden('pai_action', 'review'); hidden('ai_output_id', (int)$custody['ai_output_id']);
    start_table(TABLESTYLE2);
    echo '<tr><td>'._('Disposition:').'</td><td><select name="disposition_code"><option value="accepted">'._('Accepted as assistive evidence').'</option><option value="rejected">'._('Rejected').'</option><option value="needs_follow_up">'._('Needs follow-up').'</option></select></td></tr>';
    echo '<tr><td>'._('Reason:').'</td><td><select name="reason_code"><option value="confirmed">'._('Confirmed').'</option><option value="false_positive">'._('False positive').'</option><option value="needs_data_review">'._('Needs data review').'</option><option value="not_actionable">'._('Not actionable').'</option></select></td></tr>';
    end_table(1); submit_center('review_pay_ai_output', _('Record Human Review')); end_form();
}
end_page();
?>
