<?php
/** Screen-only HRM-ANL-001 bounded department x employment-type headcount inquiry. */
$page_security = 'SA_HRMREPORTS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_department_employment_type_headcount_db.inc');

page(_('Governed Headcount Matrix'));
if (!function_exists('get_company_pref') || version_compare((string)get_company_pref('version_id', true), '1.0.926', '<')) {
    display_error(_('Governed department x employment-type headcount is not enabled until database version 1.0.926.'));
    end_page(); exit;
}
$result=false; $error=null; $action=isset($_POST['anl_action'])?(string)$_POST['anl_action']:'';
if ($action!=='' && !check_csrf_token()) display_error(_('Invalid CSRF token.'));
elseif ($action==='run') {
    $as_at=isset($_POST['as_at_date'])?(string)$_POST['as_at_date']:'';
    if (!is_date($as_at)) display_error(_('A valid as-at date is required.'));
    else {
        $as_of_utc=date2sql($as_at).' 23:59:59';
        $result=hrm_anl_001_execute_department_employment_type_headcount($as_of_utc, HRM_ANL_001_DEPARTMENT_EMPLOYMENT_TYPE_HEADCOUNT_PURPOSE, false, false, $error);
        if ($result===false) display_error(_('Governed headcount matrix query rejected: ').$error);
    }
}

display_note(_('Screen-only department x employment-type aggregates for the already-approved active-headcount metric. The matrix exactly reconciles to the company total. If any non-zero matrix cell is below the minimum cohort threshold, every non-zero cell is suppressed to prevent reconstruction by subtraction. Employee detail, row/column marginals, arbitrary filters, export and drill-down remain unavailable.'),0,1);
start_form(); hidden('_token',ensure_csrf_token()); hidden('anl_action','run'); start_table(TABLESTYLE2); date_row(_('As at date:'),'as_at_date',isset($_POST['as_at_date'])?$_POST['as_at_date']:Today()); end_table(1); submit_center('run_governed_department_employment_type_headcount',_('Run Headcount Matrix')); end_form();
if (is_array($result)) {
    $labels=array('permanent'=>_('Permanent'),'contract'=>_('Contract'),'probation'=>_('Probation'),'intern'=>_('Intern'),'part_time'=>_('Part-time'),'other'=>_('Other / legacy code'));
    start_table(TABLESTYLE,"width='85%'"); table_header(array(_('Department'),_('Employment Type'),_('Result'),_('Unit'),_('Suppression')));
    foreach($result['rows'] as $row){start_row();label_cell(htmlspecialchars((string)$row['department_name'],ENT_QUOTES,'UTF-8'));$code=(string)$row['employment_type_code'];label_cell(htmlspecialchars(isset($labels[$code])?$labels[$code]:$code,ENT_QUOTES,'UTF-8'));if(!empty($row['suppressed']))label_cell(_('Suppressed'));else label_cell(htmlspecialchars((string)$row['value'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['unit_code'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['suppression_reason'],ENT_QUOTES,'UTF-8'));end_row();}
    end_table(1); display_note(_('Evidence recorded under snapshot ID ').(int)$result['snapshot_id']._(' and query audit ID ').(int)$result['query_audit_id']._('. No source-total hash, raw suppressed count, employee identifier, row/column marginal, export file or drill-down token is exposed by this screen.'));
}
end_page();
?>
