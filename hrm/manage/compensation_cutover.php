<?php
/** Compensation go-live maker route: opening inventories and cutover plans, each approved in the shared approval workflow. */
$page_security='SA_HRM_MIGRATE_COMPENSATION';
$path_to_root='../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_cmp_001_governance_db.inc');
page(_('Compensation Go-Live Plan'));
$a=isset($_POST['cmp_action'])?(string)$_POST['cmp_action']:'';
$approval_text=array('pending'=>_('waiting for approval in the Approval Dashboard'),'approved'=>_('approved'),
	'decided'=>_('already reviewed'));
if($a!==''&&!check_csrf_token())display_error(_('Invalid CSRF token.'));
elseif($a==='inventory'){
	$e=null;
	$r=hrm_cmp_001_inventory_opening(trim((string)$_POST['employee_id']),is_date($_POST['as_of_date'])?date2sql($_POST['as_of_date']):'',
		trim((string)$_POST['currency_code']),trim((string)$_POST['pay_basis']),trim((string)$_POST['evidence_sha256']),$e);
	$r===false?display_error(_('Opening inventory rejected: ').$e):display_notification(sprintf(_('Opening inventory %d recorded, %s.'),
		$r['opening_inventory_id'],$approval_text[$r['approval_status']]));
}elseif($a==='plan'){
	$e=null;
	$r=hrm_cmp_001_plan_cutover((int)$_POST['opening_review_id'],trim((string)$_POST['request_key']),$e);
	$r===false?display_error(_('Cutover plan rejected: ').$e):display_notification(sprintf(_('Cutover plan %d recorded, %s.'),
		$r['cutover_plan_id'],$approval_text[$r['approval_status']]));
}
start_form();
hidden('_token',ensure_csrf_token());
hidden('cmp_action','inventory');
start_table(TABLESTYLE2);
text_row_ex(_('Employee ID:'),'employee_id',24,20);
date_row(_('As-of Date:'),'as_of_date');
text_row_ex(_('Currency:'),'currency_code',5,3);
text_row_ex(_('Pay Basis:'),'pay_basis',20,24);
text_row_ex(_('Evidence SHA-256:'),'evidence_sha256',68,64);
end_table(1);
submit_center('inventory',_('Record Opening Inventory'));
end_form();
start_form();hidden('_token',ensure_csrf_token());hidden('cmp_action','plan');start_table(TABLESTYLE2);text_row_ex(_('Approved Opening Review ID:'),'opening_review_id',12,12);text_row_ex(_('Idempotency Request Key:'),'request_key',50,128);end_table(1);submit_center('plan',_('Create Cutover Plan'));end_form();
display_note(_('A different user approves each opening inventory and cutover plan in the Approval Dashboard. A third user executes the approved plan.'));
end_page();
?>
