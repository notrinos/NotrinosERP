<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
$page_security = 'SA_BOMREP';
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/banking.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');
include_once($path_to_root . '/inventory/includes/db/items_category_db.inc');

//----------------------------------------------------------------------------------------------------

print_work_order_listing();

/**
 * Fetch selected work orders with item unit precision.
 *
 * @param string $items Stock ID, or an empty string for all items.
 * @param int $open_only Whether to include only open work orders.
 * @param string $location Location code, or an empty string for all locations.
 * @return resource Database result.
 */
function getTransactions($items, $open_only, $location) {
	$sql = "SELECT
		workorder.id,
		workorder.wo_ref,
		workorder.type,
		location.location_name,
		item.description,
		workorder.units_reqd,
		workorder.units_issued,
		workorder.date_,
		workorder.required_by,
		workorder.closed,
		workorder.stock_id,
		units.decimals
		FROM ".TB_PREF."workorders as workorder
			LEFT JOIN ".TB_PREF."voided v ON v.id=workorder.id and v.type=".ST_WORKORDER."
			INNER JOIN ".TB_PREF."stock_master as item ON workorder.stock_id=item.stock_id
			LEFT JOIN ".TB_PREF."item_units units ON units.abbr=item.units
			INNER JOIN ".TB_PREF."locations as location ON workorder.loc_code=location.loc_code
		WHERE ISNULL(v.id)
			";

	if ($open_only != 0)
		$sql .= " AND workorder.closed=0";

	if ($location != '')
		$sql .= " AND workorder.loc_code=".db_escape($location);

	if ($items != '')
		$sql .= " AND workorder.stock_id=".db_escape($items);
	
	$sql .=" ORDER BY workorder.id";	

	return db_query($sql, 'No transactions were returned');
}

/**
 * Build the work-order selection clauses used by each bulk GL query.
 *
 * @param string $items Stock ID, or an empty string for all items.
 * @param int $open_only Whether to include only open work orders.
 * @param string $location Location code, or an empty string for all locations.
 * @return string SQL WHERE clause.
 */
function get_selected_work_order_filter($items, $open_only, $location) {
	$sql = " WHERE ISNULL(v.id)";
	if ($open_only != 0)
		$sql .= " AND workorder.closed=0";
	if ($location != '')
		$sql .= " AND workorder.loc_code=".db_escape($location);
	if ($items != '')
		$sql .= " AND workorder.stock_id=".db_escape($items);
	return $sql;
}

/**
 * Fetch a bulk GL query and bucket its rows by work-order ID.
 *
 * @param string $sql Bulk GL query.
 * @param string $error_message Database error message.
 * @return array GL rows keyed by work-order ID.
 */
function fetch_work_order_gl_section($sql, $error_message) {
	$result = db_query($sql, $error_message);
	$rows = array();
	while ($row = db_fetch($result)) {
		$work_order_id = $row['workorder_id'];
		if (!isset($rows[$work_order_id]))
			$rows[$work_order_id] = array();
		$rows[$work_order_id][] = $row;
	}
	return $rows;
}

/**
 * Load all GL sections for the selected work orders in four bounded queries.
 *
 * @param string $items Stock ID, or an empty string for all items.
 * @param int $open_only Whether to include only open work orders.
 * @param string $location Location code, or an empty string for all locations.
 * @return array Section maps keyed by work-order ID.
 */
function get_work_order_gl_sections($items, $open_only, $location) {
	$filter = get_selected_work_order_filter($items, $open_only, $location);
	$work_order_join = " FROM ".TB_PREF."workorders workorder
		LEFT JOIN ".TB_PREF."voided v ON v.id=workorder.id AND v.type=".ST_WORKORDER;

	$production_sql = "SELECT workorder.id AS workorder_id, gl.type, gl.type_no,
			gl.tran_date, gl.account, chart.account_name, gl.amount, com.memo_"
		.$work_order_join."
		INNER JOIN ".TB_PREF."wo_manufacture rcv ON rcv.workorder_id=workorder.id
		INNER JOIN ".TB_PREF."gl_trans gl
			ON gl.type=".ST_MANURECEIVE." AND gl.type_no=rcv.id
		INNER JOIN ".TB_PREF."chart_master chart ON chart.account_code=gl.account
		LEFT JOIN ".TB_PREF."comments com ON gl.type=com.type AND gl.type_no=com.id"
		.$filter." AND gl.amount != 0
		ORDER BY workorder.id, gl.type, gl.type_no";

	$issue_sql = "SELECT workorder.id AS workorder_id, gl.type, gl.type_no,
			gl.tran_date, gl.account, chart.account_name, gl.amount, com.memo_"
		.$work_order_join."
		INNER JOIN ".TB_PREF."wo_issues issue ON issue.workorder_id=workorder.id
		INNER JOIN ".TB_PREF."gl_trans gl
			ON gl.type=".ST_MANUISSUE." AND gl.type_no=issue.issue_no
		INNER JOIN ".TB_PREF."chart_master chart ON chart.account_code=gl.account
		LEFT JOIN ".TB_PREF."comments com ON gl.type=com.type AND gl.type_no=com.id"
		.$filter." AND gl.amount != 0
		ORDER BY workorder.id, gl.type, gl.type_no";

	$cost_sql = "SELECT workorder.id AS workorder_id, gl.type, gl.type_no,
			gl.tran_date, gl.account, chart.account_name, gl.amount, com.memo_"
		.$work_order_join."
		INNER JOIN ".TB_PREF."wo_costing costing ON costing.workorder_id=workorder.id
		INNER JOIN ".TB_PREF."gl_trans gl
			ON gl.type=costing.trans_type AND gl.type_no=costing.trans_no
		INNER JOIN ".TB_PREF."chart_master chart ON chart.account_code=gl.account
		LEFT JOIN ".TB_PREF."comments com ON gl.type=com.type AND gl.type_no=com.id"
		.$filter." AND gl.amount != 0
		ORDER BY workorder.id, costing.id, gl.counter";

	$receival_sql = "SELECT workorder.id AS workorder_id, gl.type, gl.type_no,
			gl.tran_date, gl.account, chart.account_name, gl.amount, gl.memo_"
		.$work_order_join."
		INNER JOIN ".TB_PREF."gl_trans gl
			ON gl.type=".ST_WORKORDER." AND gl.type_no=workorder.id
		LEFT JOIN ".TB_PREF."chart_master chart ON chart.account_code=gl.account
		LEFT JOIN ".TB_PREF."refs refs ON gl.type=refs.type AND gl.type_no=refs.id
		LEFT JOIN ".TB_PREF."audit_trail audit
			ON gl.type=audit.type AND gl.type_no=audit.trans_no AND NOT ISNULL(audit.gl_seq)
		LEFT JOIN ".TB_PREF."users user ON audit.user=user.id
		LEFT JOIN ".TB_PREF."supp_trans supp
			ON gl.type_no=supp.trans_no AND supp.type=gl.type
			AND (gl.type<>".ST_JOURNAL." OR gl.person_id=supp.supplier_id)
		LEFT JOIN ".TB_PREF."grn_batch grn
			ON grn.id=gl.type_no AND gl.type=".ST_SUPPRECEIVE." AND gl.person_id=grn.supplier_id
		LEFT JOIN ".TB_PREF."debtor_trans debtor
			ON gl.type_no=debtor.trans_no AND debtor.type=gl.type
			AND (gl.type<>".ST_JOURNAL." OR gl.person_id=debtor.debtor_no)
		LEFT JOIN ".TB_PREF."bank_trans bank
			ON bank.type=gl.type AND bank.trans_no=gl.type_no AND bank.amount<>0
			AND bank.person_type_id=gl.person_type_id AND bank.person_id=gl.person_id
		LEFT JOIN ".TB_PREF."journal journal ON journal.type=gl.type AND journal.trans_no=gl.type_no"
		.$filter." AND gl.amount != 0
		ORDER BY workorder.id, gl.tran_date, gl.counter";

	return array(
		'productions' => fetch_work_order_gl_section($production_sql, 'The production GL transactions could not be retrieved'),
		'issues' => fetch_work_order_gl_section($issue_sql, 'The issue GL transactions could not be retrieved'),
		'costs' => fetch_work_order_gl_section($cost_sql, 'The cost GL transactions could not be retrieved'),
		'receivals' => fetch_work_order_gl_section($receival_sql, 'The receival GL transactions could not be retrieved'),
	);
}

/**
 * Render a preloaded GL section.
 *
 * @param FrontReport $rep Report renderer.
 * @param array $rows Preloaded GL rows.
 * @param string $title Section title.
 * @return void
 */
function print_gl_rows(&$rep, $rows, $title) {
	global $systypes_array;

	$dec = user_price_dec();

	if (count($rows)) {
		$rep->Line($rep->row -= 4);
		$rep->NewLine();
		$rep->Font('italic');
		$rep->TextCol(3, 11, $title);
		$rep->Font();
		$rep->Line($rep->row -= 4);
		foreach ($rows as $myrow) {
			$rep->NewLine();
			$rep->TextCol(0, 2, $systypes_array[$myrow['type']] . ' ' . $myrow['type_no'], -2);
			$rep->TextCol(2, 3, sql2date($myrow['tran_date']), -2);
			$rep->TextCol(3, 4, $myrow['account'], -2);
			$rep->TextCol(4, 5, $myrow['account_name'], -2);
			if ($myrow['amount'] > 0.0)
				$rep->AmountCol(5, 6, $myrow['amount'], $dec);
			else	
				$rep->AmountCol(6, 7, $myrow['amount'] * -1, $dec, -1);
			$rep->TextCol(8, 11, $myrow['memo_']);
		}
	}
}

//----------------------------------------------------------------------------------------------------

function print_work_order_listing() {
	global $path_to_root, $wo_types_array;

	$item = $_POST['PARAM_0'];
	$location = $_POST['PARAM_1'];
	$open_only = $_POST['PARAM_2'];
	$show_gl = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];
	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');

	if ($item == '')
		$items = _('All');
	else {
		$row = stock_master_entity::find($item);
		$items = $row['description']; 
	}

	if ($location == '')
		$loc = _('All');
	else
		$loc = get_location_name($location);

	$open = $open_only == 1 ? _('Yes') : _('No');
	$show = $show_gl == 1 ? _('Yes') : _('No');
	
	$cols = array(0, 100, 120, 165, 210, 275, 315, 375, 385, 440, 495, 555);

	$headers = array(_('Type'), '#', ('Reference'), _('Location'), _('Item'), _('Required'), _('Manufactured'), ' ', _('Date'), _('Required By'), _('Closed'));

	if ($show_gl) {
		$cols2 = $cols;
		$headers2 = array(_('Transaction'), ' ', _('Date'), _('Account Code'),' ' . _('Account Name'), _('Debit'), _('Credit'), ' ', _('Memo'));
	}	
	else {
		$cols2 = null;
		$headers2 = null;
	}	

	$aligns = array('left',	'left',	'left', 'left', 'left', 'right', 'right', 'left', 'left', 'left', 'left');

	$params =   array( 	0 => $comments,
						1 => array('text' => _('Items'), 'from' => $items, 'to' => ''),
						2 => array('text' => _('Location'), 'from' => $loc, 'to' => ''),
						3 => array('text' => _('Open Only'), 'from' => $open, 'to' => ''),
						4 => array('text' => _('Show GL Rows'), 'from' => $show, 'to' => ''),
					);

	$rep = new FrontReport(_('Work Order Listing'), 'WorkOrderListing', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	$rep->Font();
	if ($show_gl)
		$rep->Info($params, $cols2, $headers2, $aligns, $cols, $headers);
	else	
		$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$res = getTransactions($item, $open_only, $location);
	$gl_sections = $show_gl ? get_work_order_gl_sections($item, $open_only, $location) : array();
	while ($trans=db_fetch($res)) {
		$rep->TextCol(0, 1, $wo_types_array[$trans['type']]);
		$rep->TextCol(1, 2, $trans['id'], -1);
		$rep->TextCol(2, 3, $trans['wo_ref'], -1);
		$rep->TextCol(3, 4, $trans['location_name'], -1);
		$rep->TextCol(4, 5, $trans['description'], -1);
		$dec = $trans['decimals'] == -1 || $trans['decimals'] === null
			? user_qty_dec() : $trans['decimals'];
		$rep->AmountCol(5, 6, $trans['units_reqd'], $dec);
		$rep->AmountCol(6, 7, $trans['units_issued'], $dec);
		$rep->TextCol(7, 8, '', -1);
		$rep->TextCol(8, 9, sql2date($trans['date_']), -1);
		$rep->TextCol(9, 10, sql2date($trans['required_by']), -1);
		$rep->TextCol(10, 11, $trans['closed'] ? ' ' : _('No'), -1);
		if ($show_gl) {
			$rep->NewLine();
			$productions = isset($gl_sections['productions'][$trans['id']]) ? $gl_sections['productions'][$trans['id']] : array();
			print_gl_rows($rep, $productions, _('Finished Product Requirements'));

			$issues = isset($gl_sections['issues'][$trans['id']]) ? $gl_sections['issues'][$trans['id']] : array();
			print_gl_rows($rep, $issues, _('Additional Material Issues'));

			$costs = isset($gl_sections['costs'][$trans['id']]) ? $gl_sections['costs'][$trans['id']] : array();
			print_gl_rows($rep, $costs, _('Additional Costs'));

			$wo = isset($gl_sections['receivals'][$trans['id']]) ? $gl_sections['receivals'][$trans['id']] : array();
			print_gl_rows($rep, $wo, _('Finished Product Receival'));
			$rep->Line($rep->row - 2);
			$rep->NewLine();
		}
		$rep->NewLine();
	}
	$rep->Line($rep->row);
	$rep->End();
}
