<?php
$page_security='SA_HRM_ESS_VIEW_TEAM';$path_to_root='../..';include($path_to_root.'/includes/session.inc');include_once(__DIR__.'/_common.inc');include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_manager_view_db.inc');
page(_('Team Skills & Certifications'));
if(!hrm_tal_001_manager_browser_allowed_for_database_version((string)get_company_pref('version_id',true))){display_error(_('Team talent view is not enabled for this company version.'));end_page();exit;}
$as_of=hrm_ess_002_today();$error=null;$skills=hrm_tal_001_read_skill_evidence_for_manager_team($as_of,$error);if($skills===false){display_error(_('Team talent scope could not be resolved safely.'));$skills=array();}
$error=null;$certs=hrm_tal_001_read_certification_evidence_for_manager_team($as_of,$error);if($certs===false){display_error(_('Team certification scope could not be resolved safely.'));$certs=array();}
hrm_ess_002_emit_shell_start(_('Team Skills & Certifications'),'team');
echo '<p>'._('This read-only view uses the authenticated manager session and the current effective date only. Direct team membership and explicitly approved, unrevoked ESS-002 team-view delegation are resolved server-side; this route accepts no employee selector or historical as-of date. Evidence hashes, source/credential references, payroll/compensation data and export are excluded.').'</p>';
hrm_ess_002_ui_table(_('Skills'),array(_('Employee'),_('Skill'),_('Category'),_('Evidence type'),_('Proficiency'),_('Evidence date'),_('Valid from'),_('Valid to'),_('Scope')),is_array($skills)?$skills:array(),array('employee_id','skill_name','category_code','evidence_type_code','proficiency_value','evidence_date','valid_from','valid_to','scope_origin'));
hrm_ess_002_ui_table(_('Certifications'),array(_('Employee'),_('Certification'),_('Issuer'),_('Category'),_('Issued'),_('Expires'),_('Verification'),_('Scope')),is_array($certs)?$certs:array(),array('employee_id','certification_name','issuer_name','category_code','issued_on','expires_on','verification_status_code','scope_origin'));
hrm_ess_002_emit_shell_end();end_page();
?>
