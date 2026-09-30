<?php
/** HRM-TAL-001 bounded skills/certifications governance browser surface. */
$page_security = 'SA_HRSETTINGS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_catalog_db.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_evidence_db.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_view_db.inc');

page(_('Talent Skills & Certifications'));
if (!function_exists('get_company_pref') || !hrm_tal_001_governed_admin_ui_allowed_for_database_version((string)get_company_pref('version_id', true))) {
    display_error(_('Governed talent browser access is not enabled until database version 1.0.945.'));
    end_page();
    exit;
}

function htal1_ui_value($key)
{
    return isset($_POST[$key]) ? trim((string)$_POST[$key]) : '';
}
function htal1_ui_notice($label, $result, $id_key)
{
    $suffix = !empty($result['idempotent']) ? _(' (idempotent replay)') : '';
    display_notification($label.' '.(int)$result[$id_key].$suffix);
}

$skill_rows = false;
$cert_rows = false;
$action = isset($_POST['tal_action']) ? (string)$_POST['tal_action'] : '';
if ($action !== '' && !check_csrf_token()) {
    display_error(_('Invalid CSRF token.'));
} elseif ($action === 'skill_definition') {
    $error = null;
    $result = hrm_tal_001_register_skill_definition(array(
        'skill_code'=>htal1_ui_value('skill_code'),
        'skill_name'=>htal1_ui_value('skill_name'),
        'category_code'=>htal1_ui_value('skill_category_code'),
        'description'=>htal1_ui_value('skill_description'),
        'proficiency_scale_code'=>htal1_ui_value('proficiency_scale_code'),
        'effective_from'=>htal1_ui_value('skill_effective_from'),
        'effective_to'=>htal1_ui_value('skill_effective_to'),
    ), $error);
    if ($result === false) display_error(_('Skill definition rejected: ').$error);
    else htal1_ui_notice(_('Skill definition recorded with immutable ID'), $result, 'skill_definition_id');
} elseif ($action === 'certification_definition') {
    $error = null;
    $result = hrm_tal_001_register_certification_definition(array(
        'certification_code'=>htal1_ui_value('certification_code'),
        'certification_name'=>htal1_ui_value('certification_name'),
        'issuer_name'=>htal1_ui_value('issuer_name'),
        'category_code'=>htal1_ui_value('certification_category_code'),
        'expiry_policy_code'=>htal1_ui_value('expiry_policy_code'),
        'default_validity_days'=>htal1_ui_value('default_validity_days'),
        'effective_from'=>htal1_ui_value('certification_effective_from'),
        'effective_to'=>htal1_ui_value('certification_effective_to'),
    ), $error);
    if ($result === false) display_error(_('Certification definition rejected: ').$error);
    else htal1_ui_notice(_('Certification definition recorded with immutable ID'), $result, 'certification_definition_id');
} elseif ($action === 'skill_evidence') {
    $error = null;
    $result = hrm_tal_001_record_skill_evidence(array(
        'skill_definition_id'=>htal1_ui_value('skill_definition_id'),
        'employee_id'=>htal1_ui_value('skill_employee_id'),
        'evidence_type_code'=>htal1_ui_value('evidence_type_code'),
        'source_reference_sha256'=>htal1_ui_value('source_reference_sha256'),
        'proficiency_value'=>htal1_ui_value('proficiency_value'),
        'evidence_date'=>htal1_ui_value('skill_evidence_date'),
        'valid_from'=>htal1_ui_value('skill_valid_from'),
        'valid_to'=>htal1_ui_value('skill_valid_to'),
    ), $error);
    if ($result === false) display_error(_('Skill evidence rejected: ').$error);
    else htal1_ui_notice(_('Skill evidence recorded with immutable ID'), $result, 'employee_skill_evidence_id');
} elseif ($action === 'certification_evidence') {
    $error = null;
    $result = hrm_tal_001_record_certification_evidence(array(
        'certification_definition_id'=>htal1_ui_value('certification_definition_id'),
        'employee_id'=>htal1_ui_value('certification_employee_id'),
        'credential_reference_sha256'=>htal1_ui_value('credential_reference_sha256'),
        'issued_on'=>htal1_ui_value('issued_on'),
        'expires_on'=>htal1_ui_value('expires_on'),
        'verification_status_code'=>htal1_ui_value('verification_status_code'),
        'verification_evidence_sha256'=>htal1_ui_value('verification_evidence_sha256'),
    ), $error);
    if ($result === false) display_error(_('Certification evidence rejected: ').$error);
    else htal1_ui_notice(_('Certification evidence recorded with immutable ID'), $result, 'employee_certification_evidence_id');
} elseif ($action === 'read_employee') {
    $employee_id = htal1_ui_value('read_employee_id');
    $error = null;
    $skill_rows = hrm_tal_001_read_skill_evidence_for_employee($employee_id, $error);
    if ($skill_rows === false) display_error(_('Skill evidence query rejected: ').$error);
    $error = null;
    $cert_rows = hrm_tal_001_read_certification_evidence_for_employee($employee_id, $error);
    if ($cert_rows === false) display_error(_('Certification evidence query rejected: ').$error);
}

display_note(_('This governed surface is limited to authenticated company-scoped SA_HRSETTINGS users. It appends immutable definitions/evidence and reads one exact employee at a time. It cannot update/delete evidence, perform bulk/team/self export, mutate recruitment/training/appraisal, change payroll/compensation, or make employment decisions. From database version 1.0.953, SA_HRSETTINGS may export one exact employee at a time through the separately audited bounded CSV route.'), 0, 1);

start_form(); hidden('_token', ensure_csrf_token()); hidden('tal_action', 'skill_definition');
start_table(TABLESTYLE2); table_section_title(_('Register skill definition'));
text_row_ex(_('Skill code:'), 'skill_code', 36, 64); text_row_ex(_('Skill name:'), 'skill_name', 52, 140); text_row_ex(_('Category code:'), 'skill_category_code', 36, 64);
text_row_ex(_('Description:'), 'skill_description', 70, 2000); text_row_ex(_('Proficiency scale (none / level_1_5 / percent_0_100):'), 'proficiency_scale_code', 24, 32);
text_row_ex(_('Effective from (YYYY-MM-DD):'), 'skill_effective_from', 16, 10); text_row_ex(_('Effective to (optional YYYY-MM-DD):'), 'skill_effective_to', 16, 10);
end_table(1); submit_center('record_skill_definition', _('Record Skill Definition')); end_form();

start_form(); hidden('_token', ensure_csrf_token()); hidden('tal_action', 'certification_definition');
start_table(TABLESTYLE2); table_section_title(_('Register certification definition'));
text_row_ex(_('Certification code:'), 'certification_code', 36, 64); text_row_ex(_('Certification name:'), 'certification_name', 52, 140); text_row_ex(_('Issuer name:'), 'issuer_name', 52, 140);
text_row_ex(_('Category code:'), 'certification_category_code', 36, 64); text_row_ex(_('Expiry policy (none / fixed_days / explicit_on_evidence):'), 'expiry_policy_code', 28, 32);
text_row_ex(_('Default validity days (fixed_days only):'), 'default_validity_days', 12, 10); text_row_ex(_('Effective from (YYYY-MM-DD):'), 'certification_effective_from', 16, 10); text_row_ex(_('Effective to (optional YYYY-MM-DD):'), 'certification_effective_to', 16, 10);
end_table(1); submit_center('record_certification_definition', _('Record Certification Definition')); end_form();

start_form(); hidden('_token', ensure_csrf_token()); hidden('tal_action', 'skill_evidence');
start_table(TABLESTYLE2); table_section_title(_('Record employee skill evidence'));
text_row_ex(_('Skill definition ID:'), 'skill_definition_id', 12, 20); text_row_ex(_('Employee ID:'), 'skill_employee_id', 24, 20); text_row_ex(_('Evidence type (assessment / credential / manager_attestation / employee_declaration):'), 'evidence_type_code', 28, 32);
text_row_ex(_('Source reference SHA-256:'), 'source_reference_sha256', 68, 64); text_row_ex(_('Proficiency value (when required):'), 'proficiency_value', 16, 32); text_row_ex(_('Evidence date (YYYY-MM-DD):'), 'skill_evidence_date', 16, 10);
text_row_ex(_('Valid from (YYYY-MM-DD):'), 'skill_valid_from', 16, 10); text_row_ex(_('Valid to (optional YYYY-MM-DD):'), 'skill_valid_to', 16, 10);
end_table(1); submit_center('record_skill_evidence', _('Record Skill Evidence')); end_form();

start_form(); hidden('_token', ensure_csrf_token()); hidden('tal_action', 'certification_evidence');
start_table(TABLESTYLE2); table_section_title(_('Record employee certification evidence'));
text_row_ex(_('Certification definition ID:'), 'certification_definition_id', 12, 20); text_row_ex(_('Employee ID:'), 'certification_employee_id', 24, 20); text_row_ex(_('Credential reference SHA-256:'), 'credential_reference_sha256', 68, 64);
text_row_ex(_('Issued on (YYYY-MM-DD):'), 'issued_on', 16, 10); text_row_ex(_('Expires on (optional YYYY-MM-DD):'), 'expires_on', 16, 10); text_row_ex(_('Verification status (unverified / verified / rejected):'), 'verification_status_code', 20, 32);
text_row_ex(_('Verification evidence SHA-256:'), 'verification_evidence_sha256', 68, 64);
end_table(1); submit_center('record_certification_evidence', _('Record Certification Evidence')); end_form();

start_form(); hidden('_token', ensure_csrf_token()); hidden('tal_action', 'read_employee');
start_table(TABLESTYLE2); table_section_title(_('Restricted employee evidence view')); text_row_ex(_('Exact Employee ID:'), 'read_employee_id', 24, 20); end_table(1);
submit_center('read_employee_talent', _('View Governed Talent Evidence')); end_form();


if (function_exists('get_company_pref') && hrm_tal_001_export_browser_allowed_for_database_version((string)get_company_pref('version_id', true))) {
    echo "<form method='post' action='talent_export.php'>";
    hidden('_token', ensure_csrf_token());
    echo "<table class='".TABLESTYLE2."'><tr><td>"._('Exact Employee ID for bounded export:')."</td><td><input type='text' name='employee_id' maxlength='20'></td></tr></table>";
    echo "<div class='submit'><input type='submit' value='"._('Export Governed Talent CSV')."'></div>";
    echo "</form>";
}

if (is_array($skill_rows)) {
    start_table(TABLESTYLE, "width='95%'"); table_header(array(_('Skill'), _('Category'), _('Evidence type'), _('Proficiency'), _('Evidence date'), _('Valid from'), _('Valid to')));
    foreach ($skill_rows as $row) { start_row(); label_cell(htmlspecialchars((string)$row['skill_name'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['category_code'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['evidence_type_code'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['proficiency_value'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['evidence_date'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['valid_from'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['valid_to'], ENT_QUOTES, 'UTF-8')); end_row(); }
    end_table(1);
}
if (is_array($cert_rows)) {
    start_table(TABLESTYLE, "width='95%'"); table_header(array(_('Certification'), _('Issuer'), _('Category'), _('Issued'), _('Expires'), _('Verification')));
    foreach ($cert_rows as $row) { start_row(); label_cell(htmlspecialchars((string)$row['certification_name'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['issuer_name'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['category_code'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['issued_on'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['expires_on'], ENT_QUOTES, 'UTF-8')); label_cell(htmlspecialchars((string)$row['verification_status_code'], ENT_QUOTES, 'UTF-8')); end_row(); }
    end_table(1);
}
end_page();
?>
