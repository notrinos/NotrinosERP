<?php
/** Screen-only HRM-ANL-001 bounded employment-type headcount aggregate inquiry. */
$page_security = 'SA_HRMREPORTS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_employment_type_headcount_db.inc');

page(_('Governed Headcount by Employment Type'));
if (!function_exists('get_company_pref') || version_compare((string)get_company_pref('version_id', true), '1.0.922', '<')) {
    display_error(_('Governed employment-type headcount is not enabled until database version 1.0.922.'));
    end_page(); exit;
}
$result=false; $error=null; $action=isset($_POST['anl_action'])?(string)$_POST['anl_action']:'';
if ($action!=='' && !check_csrf_token()) display_error(_('Invalid CSRF token.'));
elseif ($action==='run') {
    $as_at=isset($_POST['as_at_date'])?(string)$_POST['as_at_date']:'';
    if (!is_date($as_at)) display_error(_('A valid as-at date is required.'));
    else {
        $as_of_utc=date2sql($as_at).' 23:59:59';
        $result=hrm_anl_001_execute_employment_type_headcount($as_of_utc, HRM_ANL_001_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE, false, false, $error);
        if ($result===false) display_error(_('Governed employment-type headcount query rejected: ').$error);
    }
}

display_note(_('Screen-only employment-type aggregates for the already-approved active-headcount metric. The five fixed HRM employment-type codes plus an other bucket are reconciled to the company total. Small cohorts are suppressed and complementary suppression is applied when needed. Employee detail, arbitrary filters, export and drill-down remain unavailable.'),0,1);
start_form(); hidden('_token',ensure_csrf_token()); hidden('anl_action','run'); start_table(TABLESTYLE2); date_row(_('As at date:'),'as_at_date',isset($_POST['as_at_date'])?$_POST['as_at_date']:Today()); end_table(1); submit_center('run_governed_employment_type_headcount',_('Run Employment-Type Headcount')); end_form();
if (is_array($result)) {
    $labels=array('permanent'=>_('Permanent'),'contract'=>_('Contract'),'probation'=>_('Probation'),'intern'=>_('Intern'),'part_time'=>_('Part-time'),'other'=>_('Other / legacy code'));
    start_table(TABLESTYLE,"width='75%'"); table_header(array(_('Employment Type'),_('Result'),_('Unit'),_('Suppression')));
    foreach($result['rows'] as $row){start_row();$code=(string)$row['employment_type_code'];label_cell(htmlspecialchars(isset($labels[$code])?$labels[$code]:$code,ENT_QUOTES,'UTF-8'));if(!empty($row['suppressed']))label_cell(_('Suppressed'));else label_cell(htmlspecialchars((string)$row['value'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['unit_code'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['suppression_reason'],ENT_QUOTES,'UTF-8'));end_row();}
    end_table(1); display_note(_('Evidence recorded under snapshot ID ').(int)$result['snapshot_id']._(' and query audit ID ').(int)$result['query_audit_id']._('. No source-total hash, raw suppressed count, employee identifier, export file or drill-down token is exposed by this screen.'));
}
end_page();
?>
