<?php
/**********************************************************************
    Governed attendance raw-event import/export surface.
    HRM-TIM-002 deliberately does not import overtime totals/rates or approval.
***********************************************************************/
$page_security = 'SA_IMPORTATT';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/hrm_tim_002_governance_db.inc');
page(_("Import/Export Attendance"));

if (isset($_POST['download_template'])) {
    header('Content-Type: text/csv; charset=UTF-8');
    header('Content-Disposition: attachment; filename="attendance_raw_event_import_template.csv"');
    $out=fopen('php://output','w');
    fputcsv($out,array('source_item_id','assignment_id','event_kind','event_time_local','timezone_name','utc_offset_minutes'));
    fputcsv($out,array('DEVICE-ROW-0001','123','clock_in','2026-03-01 08:00:00','Asia/Bangkok','420'));
    fclose($out);return;
}

if (isset($_POST['export_attendance'])) {
    $from = get_post('from_date', begin_month(Today()));
    $to = get_post('to_date', end_month(Today()));
    header('Content-Type: text/csv; charset=UTF-8');
    header('Content-Disposition: attachment; filename="attendance_export_'.date('Ymd_His').'.csv"');
    $out=fopen('php://output','w');
    fputcsv($out,array('employee_id','date','regular_hours','overtime_hours','overtime_type_id','rate','status'));
    $sql = "SELECT employee_id, date, regular_hours, overtime_hours, overtime_type_id, rate, status FROM ".TB_PREF."attendance WHERE date >= ".db_escape(date2sql($from))." AND date <= ".db_escape(date2sql($to))." ORDER BY employee_id, date";
    $result = db_query($sql, 'could not export attendance');
    while ($row = db_fetch($result)) {
        fputcsv($out,array(
            hrm_tim_002_csv_safe_cell($row['employee_id']),
            hrm_tim_002_csv_safe_cell($row['date']),
            (float)$row['regular_hours'],(float)$row['overtime_hours'],
            (int)$row['overtime_type_id'],(float)$row['rate'],(int)$row['status']));
    }
    fclose($out);return;
}

if (isset($_POST['import_attendance'])) {
    $connector_key=trim((string)get_post('connector_key'));
    $source_batch_key=trim((string)get_post('source_batch_key'));
    $retry_of_batch_id=trim((string)get_post('retry_of_batch_id'));
    if (!isset($_FILES['csv_file']) || $_FILES['csv_file']['error'] != UPLOAD_ERR_OK)
        display_error(_('Please choose a valid CSV file.'));
    elseif ($connector_key==='' || $source_batch_key==='')
        display_error(_('Connector key and upstream source batch ID are required.'));
    else {
        $command_key='attendance-csv:'.hash('sha256',$connector_key.'|'.$source_batch_key.'|'.$retry_of_batch_id.'|'.hash_file('sha256',$_FILES['csv_file']['tmp_name']));
        $error=null;
        $result=hrm_tim_002_process_csv_import($_FILES['csv_file']['tmp_name'],$_FILES['csv_file']['name'],$connector_key,$source_batch_key,$command_key,$error,$retry_of_batch_id===''?null:(int)$retry_of_batch_id);
        if ($result===false)
            display_error(sprintf(_('Import was denied or quarantined: %s'),$error?:'unknown'));
        else if ($result['status']==='quarantined')
            display_error(sprintf(_('Batch quarantined. Accepted: %s, Quarantined: %s'),$result['accepted'],$result['quarantined']));
        else
            display_notification(sprintf(_('Governed raw-event import complete. Accepted/replayed: %s, Quarantined: %s'),$result['accepted'],$result['quarantined']));
    }
}
if (!isset($_POST['from_date'])) $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date'])) $_POST['to_date'] = end_month(Today());
start_form(true);start_table(TABLESTYLE2);
file_row(_('CSV File:'), 'csv_file', 'csv_file');
text_row(_('Connector key:'),'connector_key',get_post('connector_key'),32,64);
text_row(_('Upstream source batch ID:'),'source_batch_key',get_post('source_batch_key'),48,128);
text_row(_('Retry predecessor batch ID (optional):'),'retry_of_batch_id',get_post('retry_of_batch_id'),12,20);
label_row(_('Expected import columns:'), _('source_item_id, assignment_id, event_kind, event_time_local, timezone_name, utc_offset_minutes'));
label_row(_('Import boundary:'), _('Raw time events only. Imported totals, overtime rates, approval state, employee/legal-entity/payroll identifiers are not accepted as authority.'));
label_row(_('Retry rule:'), _('Retry requires the exact same connector, source batch ID and content, and may only consume the current quarantined predecessor once.'));
date_row(_('Export From Date:'), 'from_date');date_row(_('Export To Date:'), 'to_date');end_table(1);
submit_center('import_attendance', _('Import Raw Events'));submit_center('download_template', _('Download Template'));submit_center('export_attendance', _('Export Attendance'));end_form();end_page();
