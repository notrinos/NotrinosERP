<?php
/** Read-only self-scoped PAY-CTRY-007 regulatory update inbox. */
$page_security='SA_HRM_VIEW_PAY_CTRY_UPDATE';
$path_to_root='../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/pay_ctry_007_notification_db.inc');
page(_('Regulatory Country-Pack Updates'));
$actor=pay_ctry_007_actor_id();
$rows=pay_ctry_007_get_visible_deliveries($actor,100);
display_note(_('Read-only self-scoped notice center. It contains country-pack metadata only; no employee facts, automatic pack activation, payroll action, payment, posting, filing, email, SMS, or webhook delivery occurs here.'),0,1);
start_table(TABLESTYLE,"width='100%'");
table_header(array(_('Pack'),_('Current'),_('Successor'),_('Severity'),_('Deadline'),_('Required action')));
$k=0;
foreach($rows as $row){alt_table_row_color($k);label_cell(htmlspecialchars((string)$row['pack_code'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['current_pack_version'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['successor_pack_version'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['severity'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['deadline_at'],ENT_QUOTES,'UTF-8'));label_cell(htmlspecialchars((string)$row['required_action'],ENT_QUOTES,'UTF-8'));end_row();}
if(!$rows)label_row(_('Status'),_('No delivered, current, unacknowledged regulatory update notices are available for your user and company scope.'),'colspan=1','colspan=5');
end_table(1);end_page();
?>
