<?php
/** HRM-TAL-001 ESS self-only skills/certifications/performance view. */
$page_security = 'SA_HRM_ESS_VIEW_SELF';
$path_to_root = '../..';
include($path_to_root . '/includes/session.inc');
include_once(__DIR__ . '/_common.inc');
include_once($path_to_root . '/hrm/includes/db/hrm_tal_001_self_view_db.inc');

page(_('My Talent Profile'));
$database_version = function_exists('get_company_pref') ? (string)get_company_pref('version_id', true) : '';
if (!hrm_tal_001_self_service_view_allowed_for_database_version($database_version)) {
    display_error(_('Talent self-service is not enabled until database version 1.0.947.'));
    end_page();
    exit;
}

$error = null;
$skills = hrm_tal_001_read_skill_evidence_for_self($error);
if ($skills === false) {
    display_error(_('Skills view unavailable: ') . $error);
    $skills = array();
}

$error = null;
$certs = hrm_tal_001_read_certification_evidence_for_self($error);
if ($certs === false) {
    display_error(_('Certifications view unavailable: ') . $error);
    $certs = array();
}

$performance = array();
$performance_enabled = hrm_tal_001_performance_self_service_view_allowed_for_database_version($database_version);
if ($performance_enabled) {
    $error = null;
    $performance = hrm_tal_001_read_performance_for_self($error);
    if ($performance === false) {
        display_error(_('Performance view unavailable: ') . $error);
        $performance = array();
    }
}

hrm_ess_002_emit_shell_start(_('My Talent Profile'), 'talent');
echo '<p>' . _('This read-only view resolves your Employee scope from the signed-in ESS identity. It accepts no employee identifier from the browser and exposes no source, credential, verification, actor, or other sensitive reference hashes.') . '</p>';
hrm_ess_002_ui_table(
    _('Skills'),
    array(_('Skill'), _('Category'), _('Evidence'), _('Proficiency'), _('Evidence date'), _('Valid from'), _('Valid to')),
    $skills,
    array('skill_name', 'category_code', 'evidence_type_code', 'proficiency_value', 'evidence_date', 'valid_from', 'valid_to')
);
hrm_ess_002_ui_table(
    _('Certifications'),
    array(_('Certification'), _('Issuer'), _('Category'), _('Issued'), _('Expires'), _('Verification')),
    $certs,
    array('certification_name', 'issuer_name', 'category_code', 'issued_on', 'expires_on', 'verification_status_code')
);
if ($performance_enabled) {
    hrm_ess_002_ui_table(
        _('Performance'),
        array(_('Cycle'), _('Purpose'), _('Review type'), _('Window start'), _('Window end'), _('Assessment role'), _('Dimension'), _('Rating'), _('Evidence date')),
        $performance,
        array('cycle_name', 'purpose_code', 'review_type_code', 'review_window_start', 'review_window_end', 'assessment_role_code', 'dimension_code', 'rating_value', 'evidence_date')
    );
}
hrm_ess_002_emit_shell_end();
end_page();
?>
