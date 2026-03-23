<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Phase 6: Approval Summary Report (rep950)

	Generates a PDF/Excel report showing approval workflow statistics
	including counts by status, average processing times, and detailed
	transaction listings for a selected date range.

	Report number: 950
	Security: SA_APPROVALINQUIRY

	PHP 5.6+ compatible.
***********************************************************************/
$page_security = 'SA_APPROVALINQUIRY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/approval/db/approval_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_rules_db.inc');
include_once($path_to_root . '/admin/db/approval_rules_setup_db.inc');

//----------------------------------------------------------------------------------------------------

print_approval_summary();

/**
 * Get approval summary data grouped by status for the given date range.
 *
 * @param string $from      Start date in SQL format
 * @param string $to        End date in SQL format
 * @param int    $trans_type Transaction type filter (-1 = all)
 * @return resource          DB result set
 */
function get_approval_summary_by_status($from, $to, $trans_type)
{
	$sql = "SELECT d.status,
			COUNT(*) AS draft_count,
			SUM(d.amount) AS total_amount,
			AVG(d.amount) AS avg_amount,
			AVG(TIMESTAMPDIFF(HOUR, d.submitted_date,
				COALESCE(d.completed_date, NOW()))) AS avg_hours
		FROM " . TB_PREF . "approval_drafts d
		WHERE d.submitted_date >= " . db_escape($from . ' 00:00:00') . "
		AND d.submitted_date <= " . db_escape($to . ' 23:59:59');

	if ($trans_type != -1) {
		$sql .= " AND d.trans_type = " . db_escape($trans_type);
	}

	$sql .= " GROUP BY d.status ORDER BY d.status";

	return db_query($sql, 'could not get approval status summary');
}

/**
 * Get approval summary grouped by transaction type.
 *
 * @param string $from      Start date in SQL format
 * @param string $to        End date in SQL format
 * @param int    $trans_type Transaction type filter (-1 = all)
 * @return resource          DB result set
 */
function get_approval_summary_by_type($from, $to, $trans_type)
{
	$sql = "SELECT d.trans_type,
			COUNT(*) AS draft_count,
			SUM(CASE WHEN d.status = 0 THEN 1 ELSE 0 END) AS pending_count,
			SUM(CASE WHEN d.status = 1 THEN 1 ELSE 0 END) AS approved_count,
			SUM(CASE WHEN d.status = 2 THEN 1 ELSE 0 END) AS rejected_count,
			SUM(CASE WHEN d.status = 3 THEN 1 ELSE 0 END) AS cancelled_count,
			SUM(d.amount) AS total_amount,
			AVG(CASE WHEN d.status = 1 THEN
				TIMESTAMPDIFF(HOUR, d.submitted_date, d.completed_date) ELSE NULL END) AS avg_approval_hours
		FROM " . TB_PREF . "approval_drafts d
		WHERE d.submitted_date >= " . db_escape($from . ' 00:00:00') . "
		AND d.submitted_date <= " . db_escape($to . ' 23:59:59');

	if ($trans_type != -1) {
		$sql .= " AND d.trans_type = " . db_escape($trans_type);
	}

	$sql .= " GROUP BY d.trans_type ORDER BY d.trans_type";

	return db_query($sql, 'could not get approval type summary');
}

/**
 * Get approval detail listing for the report.
 *
 * @param string $from        Start date in SQL format
 * @param string $to          End date in SQL format
 * @param int    $trans_type  Transaction type filter (-1 = all)
 * @param int    $status      Status filter (-1 = all)
 * @return resource            DB result set
 */
function get_approval_report_detail($from, $to, $trans_type, $status)
{
	$sql = "SELECT d.id, d.trans_type, d.reference, d.summary,
			d.amount, d.status, d.current_level,
			d.submitted_date, d.completed_date,
			d.approved_trans_no,
			w.name AS workflow_name,
			u.real_name AS submitter_name,
			(SELECT COUNT(*) FROM " . TB_PREF . "approval_actions a
			 WHERE a.draft_id = d.id AND a.action_type = 'approve') AS approval_count,
			TIMESTAMPDIFF(HOUR, d.submitted_date,
				COALESCE(d.completed_date, NOW())) AS hours_elapsed
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.submitted_by
		WHERE d.submitted_date >= " . db_escape($from . ' 00:00:00') . "
		AND d.submitted_date <= " . db_escape($to . ' 23:59:59');

	if ($trans_type != -1) {
		$sql .= " AND d.trans_type = " . db_escape($trans_type);
	}

	if ($status != -1) {
		$sql .= " AND d.status = " . db_escape($status);
	}

	$sql .= " ORDER BY d.submitted_date DESC";

	return db_query($sql, 'could not get approval report detail');
}

/**
 * Get approver activity summary for the period.
 *
 * @param string $from Start date in SQL format
 * @param string $to   End date in SQL format
 * @return resource     DB result set
 */
function get_approver_activity_summary($from, $to)
{
	$sql = "SELECT a.user_id, u.real_name,
			COUNT(*) AS action_count,
			SUM(CASE WHEN a.action_type = 'approve' THEN 1 ELSE 0 END) AS approvals,
			SUM(CASE WHEN a.action_type = 'reject' THEN 1 ELSE 0 END) AS rejections,
			SUM(CASE WHEN a.action_type = 'delegate' THEN 1 ELSE 0 END) AS delegations,
			SUM(CASE WHEN a.action_type = 'escalate' THEN 1 ELSE 0 END) AS escalations
		FROM " . TB_PREF . "approval_actions a
		LEFT JOIN " . TB_PREF . "users u ON u.id = a.user_id
		WHERE a.action_date >= " . db_escape($from . ' 00:00:00') . "
		AND a.action_date <= " . db_escape($to . ' 23:59:59') . "
		AND a.action_type IN ('approve', 'reject', 'delegate', 'escalate')
		GROUP BY a.user_id
		ORDER BY action_count DESC";

	return db_query($sql, 'could not get approver activity summary');
}

//----------------------------------------------------------------------------------------------------

/**
 * Generate the approval summary report.
 *
 * @return void
 */
function print_approval_summary()
{
	global $path_to_root, $systypes_array;

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$trans_type = $_POST['PARAM_2'];
	// SYS_TYPES_ALL returns '' for "No Type Filter" — normalize to -1 for consistency
	if ($trans_type === '' || $trans_type === ALL_TEXT) {
		$trans_type = -1;
	} else {
		$trans_type = (int)$trans_type;
	}
	$status = $_POST['PARAM_3'];
	// TEXT input, normalize: empty or non-numeric => -1 (all)
	if ($status === '' || !is_numeric($status)) {
		$status = -1;
	} else {
		$status = (int)$status;
	}
	$detail_level = $_POST['PARAM_4'];
	$comments = $_POST['PARAM_5'];
	$orientation = $_POST['PARAM_6'];
	$destination = $_POST['PARAM_7'];

	if ($destination) {
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	} else {
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');
	}

	$orientation = $orientation ? 'L' : 'P';
	$dec = user_price_dec();

	$from_sql = date2sql($from);
	$to_sql = date2sql($to);

	$status_labels = array(
		0 => _('Pending'),
		1 => _('Approved'),
		2 => _('Rejected'),
		3 => _('Cancelled'),
		4 => _('Expired')
	);

	$type_label = ($trans_type == -1) ? _('All') : (isset($systypes_array[$trans_type]) ? $systypes_array[$trans_type] : _('Unknown'));
	$status_label = ($status == -1) ? _('All') : (isset($status_labels[$status]) ? $status_labels[$status] : _('Unknown'));

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from, 'to' => $to),
		2 => array('text' => _('Transaction Type'), 'from' => $type_label, 'to' => ''),
		3 => array('text' => _('Status'), 'from' => $status_label, 'to' => '')
	);

	// =====================================================
	// Section 1: Summary by Status
	// =====================================================

	$cols = array(0, 100, 180, 260, 360, 460, 520);
	$headers = array(_('Status'), _('Count'), _('Total Amount'), _('Avg Amount'), _('Avg Hours'), _(''));
	$aligns = array('left', 'right', 'right', 'right', 'right', 'left');

	$rep = new FrontReport(_('Approval Workflow Summary'), 'ApprovalSummary', user_pagesize(), 9, $orientation);
	if ($orientation == 'L') {
		recalculate_cols($cols);
	}

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$rep->Font('bold');
	$rep->TextCol(0, 5, _('SUMMARY BY STATUS'));
	$rep->Font();
	$rep->NewLine(2);

	$status_data = get_approval_summary_by_status($from_sql, $to_sql, $trans_type);
	$grand_total_count = 0;
	$grand_total_amount = 0;

	while ($row = db_fetch($status_data)) {
		$status_text = isset($status_labels[$row['status']]) ? $status_labels[$row['status']] : _('Unknown');
		$rep->TextCol(0, 1, $status_text);
		$rep->AmountCol(1, 2, $row['draft_count'], 0);
		$rep->AmountCol(2, 3, $row['total_amount'], $dec);
		$rep->AmountCol(3, 4, $row['avg_amount'], $dec);
		$hours = round((float)$row['avg_hours'], 1);
		$rep->TextCol(4, 5, $hours . 'h');
		$rep->NewLine();

		$grand_total_count += (int)$row['draft_count'];
		$grand_total_amount += (float)$row['total_amount'];
	}

	$rep->Line($rep->row - 4);
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 1, _('Total'));
	$rep->AmountCol(1, 2, $grand_total_count, 0);
	$rep->AmountCol(2, 3, $grand_total_amount, $dec);
	$rep->Font();
	$rep->NewLine(2);

	// =====================================================
	// Section 2: Summary by Transaction Type
	// =====================================================

	$rep->Font('bold');
	$rep->TextCol(0, 5, _('SUMMARY BY TRANSACTION TYPE'));
	$rep->Font();
	$rep->NewLine(2);

	// Re-draw headers for this section
	$cols2 = array(0, 120, 170, 220, 270, 320, 420, 520);
	$headers2 = array(_('Type'), _('Total'), _('Pend.'), _('Appr.'), _('Rej.'), _('Amount'), _('Avg Hours'));

	$rep->Font('bold');
	for ($i = 0; $i < count($headers2); $i++) {
		$rep->TextCol($i, $i + 1, $headers2[$i]);
	}
	$rep->Font();
	$rep->NewLine();
	$rep->Line($rep->row + 4);
	$rep->NewLine();

	$type_data = get_approval_summary_by_type($from_sql, $to_sql, $trans_type);

	while ($row = db_fetch($type_data)) {
		$type_text = isset($systypes_array[$row['trans_type']]) ? $systypes_array[$row['trans_type']] : _('Unknown');
		$rep->TextCol(0, 1, $type_text);
		$rep->TextCol(1, 2, $row['draft_count']);
		$rep->TextCol(2, 3, $row['pending_count']);
		$rep->TextCol(3, 4, $row['approved_count']);
		$rep->TextCol(4, 5, $row['rejected_count']);
		$rep->AmountCol(5, 6, $row['total_amount'], $dec);
		$hours = $row['avg_approval_hours'] !== null ? round((float)$row['avg_approval_hours'], 1) . 'h' : '-';
		$rep->TextCol(6, 7, $hours);
		$rep->NewLine();
	}

	$rep->NewLine(2);

	// =====================================================
	// Section 3: Approver Activity
	// =====================================================

	$rep->Font('bold');
	$rep->TextCol(0, 5, _('APPROVER ACTIVITY'));
	$rep->Font();
	$rep->NewLine(2);

	$cols3 = array(0, 160, 230, 300, 380, 460, 520);
	$headers3 = array(_('Approver'), _('Actions'), _('Approvals'), _('Rejections'), _('Delegations'), _('Escalations'));

	$rep->Font('bold');
	for ($i = 0; $i < count($headers3); $i++) {
		$rep->TextCol($i, $i + 1, $headers3[$i]);
	}
	$rep->Font();
	$rep->NewLine();
	$rep->Line($rep->row + 4);
	$rep->NewLine();

	$activity_data = get_approver_activity_summary($from_sql, $to_sql);

	while ($row = db_fetch($activity_data)) {
		$name = $row['real_name'] ? $row['real_name'] : _('System');
		$rep->TextCol(0, 1, $name);
		$rep->TextCol(1, 2, $row['action_count']);
		$rep->TextCol(2, 3, $row['approvals']);
		$rep->TextCol(3, 4, $row['rejections']);
		$rep->TextCol(4, 5, $row['delegations']);
		$rep->TextCol(5, 6, $row['escalations']);
		$rep->NewLine();
	}

	// =====================================================
	// Section 4: Detail Listing (if requested)
	// =====================================================

	if ($detail_level) {
		$rep->NewLine(2);
		$rep->Font('bold');
		$rep->TextCol(0, 5, _('DETAIL LISTING'));
		$rep->Font();
		$rep->NewLine(2);

		$cols4 = array(0, 80, 170, 280, 350, 410, 470, 520);
		$headers4 = array(_('Reference'), _('Workflow'), _('Submitter'), _('Date'), _('Status'), _('Amount'), _('Hours'));

		$rep->Font('bold');
		for ($i = 0; $i < count($headers4); $i++) {
			$rep->TextCol($i, $i + 1, $headers4[$i]);
		}
		$rep->Font();
		$rep->NewLine();
		$rep->Line($rep->row + 4);
		$rep->NewLine();

		$detail_data = get_approval_report_detail($from_sql, $to_sql, $trans_type, $status);

		while ($row = db_fetch($detail_data)) {
			$status_text = isset($status_labels[$row['status']]) ? $status_labels[$row['status']] : '?';
			$rep->TextCol(0, 1, $row['reference']);
			$rep->TextCol(1, 2, $row['workflow_name']);
			$rep->TextCol(2, 3, $row['submitter_name']);
			$rep->TextCol(3, 4, sql2date($row['submitted_date']));
			$rep->TextCol(4, 5, $status_text);
			$rep->AmountCol(5, 6, $row['amount'], $dec);
			$rep->TextCol(6, 7, round((float)$row['hours_elapsed'], 1) . 'h');
			$rep->NewLine();

			if ($rep->row < $rep->bottomMargin + (2 * $rep->lineHeight)) {
				$rep->NewPage();
			}
		}
	}

	$rep->End();
}
