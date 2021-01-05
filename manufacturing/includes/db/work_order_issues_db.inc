<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
//--------------------------------------------------------------------------------------

function add_work_order_issue($woid, $ref, $to_work_order, $items, $location, $workcentre, $date_, $memo_) {
	global $Refs;

	begin_transaction();
	$args = func_get_args();
	$args = (object)array_combine(array('woid', 'ref', 'to_work_order', 'items', 'location', 
		'workcentre', 'date_', 'memo_'), $args);
	$args->trans_no = 0;
	hook_db_prewrite($args, ST_MANUISSUE);

	$sql = "INSERT INTO ".TB_PREF."wo_issues (workorder_id, reference, issue_date, loc_code, workcentre_id)
		VALUES (".db_escape($woid).", ".db_escape($ref).", '" .
		date2sql($date_) . "', ".db_escape($location).", ".db_escape($workcentre).")";
	db_query($sql, 'The work order issue could not be added');

	$number = db_insert_id();

	$issue_total = $total_cost = 0;

	$wo = get_work_order($woid);

	foreach ($items as $item) {
		if ($to_work_order)	// otherwise it is material return to inventory
			$item->quantity = -$item->quantity;

		$unit_cost = get_unit_cost($item->stock_id);
		// insert a -ve stock move for each item
		add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$item->quantity, $unit_cost);

		$sql = "INSERT INTO ".TB_PREF."wo_issue_items (issue_id, stock_id, qty_issued, unit_cost)
			SELECT ".db_escape($number).",".db_escape($item->stock_id).",".db_escape($item->quantity).", material_cost
			FROM ".TB_PREF."stock_master
			WHERE stock_id=".db_escape($item->stock_id);

		db_query($sql, 'A work order issue item could not be added');

		$unit_cost = get_unit_cost($item->stock_id);
		$issue_cost = $unit_cost * $item->quantity;

		$stockitem = get_item($item->stock_id);

		// Compatibility for Service Items
		if (!is_service($stockitem['mb_flag']))
			$ivaccount = $stockitem['inventory_account'];
		else
			$ivaccount = $stockitem['cogs_account'];

		$total_cost += add_gl_trans_std_cost(ST_MANUISSUE, $number, $date_, $ivaccount, 0, 0, $date_.': '._('Issue of').' '.$stockitem['description'], -$issue_cost);
	}

	$stockitem = get_item($wo['stock_id']);

	$wip_account = $stockitem['wip_account'];

	add_gl_trans_std_cost(ST_MANUISSUE, $number, $date_, $wip_account, 0, 0, $date_.': '._('Issue to').' '.$stockitem['description'], -$total_cost);

	if ($memo_)
		add_comments(ST_MANUISSUE, $number, $date_, $memo_);

	$Refs->save(ST_MANUISSUE, $number, $ref);
	add_audit_trail(ST_MANUISSUE, $number, $date_);

	$args->trans_no = $number;
	hook_db_postwrite($args, ST_MANUISSUE);

	commit_transaction();
}

//--------------------------------------------------------------------------------------

function get_work_order_issues($woid) {
	$sql = "SELECT * FROM ".TB_PREF."wo_issues WHERE workorder_id=".db_escape($woid)
	." ORDER BY issue_no";
	return db_query($sql, "The work order issues could not be retrieved");
}

function get_additional_issues($woid) {
	$sql = "SELECT issue.*, item.*, stock.mb_flag
		FROM ".TB_PREF."wo_issues issue, "
			.TB_PREF."wo_issue_items item
			LEFT JOIN ".TB_PREF."stock_master stock ON stock.stock_id=item.stock_id
		WHERE issue.issue_no=item.issue_id
		AND issue.workorder_id=".db_escape($woid)
		." ORDER BY item.id";
	return db_query($sql, 'The work order issues could not be retrieved');
}

//--------------------------------------------------------------------------------------

function get_work_order_issue($issue_no) {
	$sql = "SELECT DISTINCT issue.*, wo.stock_id, wo.closed,
		item.description, loc.location_name, center.name AS WorkCentreName
		FROM ".TB_PREF."wo_issues issue,"
			.TB_PREF."workorders wo,"
			.TB_PREF."stock_master item,"
			.TB_PREF."locations loc,"
			.TB_PREF."workcentres center
		WHERE issue_no=".db_escape($issue_no)."
		AND wo.id = issue.workorder_id
		AND loc.loc_code = issue.loc_code
		AND center.id = issue.workcentre_id
		AND item.stock_id = wo.stock_id";
	$result = db_query($sql, 'A work order issue could not be retrieved');

	return db_fetch($result);
}

//--------------------------------------------------------------------------------------

function get_work_order_issue_details($issue_no) {
	$sql = "SELECT issue.*, item.description, item.units
		FROM ".TB_PREF."wo_issue_items issue,"
			.TB_PREF."stock_master item
		WHERE issue_id=".db_escape($issue_no)."
		AND item.stock_id=issue.stock_id
		ORDER BY issue.id";
	return db_query($sql, 'The work order issue items could not be retrieved');
}

//--------------------------------------------------------------------------------------

function exists_work_order_issue($issue_no) {
	$sql = "SELECT issue_no FROM ".TB_PREF."wo_issues WHERE issue_no=".db_escape($issue_no);
	$result = db_query($sql, 'Cannot retreive a wo issue');

	return (db_num_rows($result) > 0);
}

//--------------------------------------------------------------------------------------

function check_void_wo_issue($issue_no) {
	$issue = get_work_order_issue($issue_no);

	return $issue['closed'] ? $issue['workorder_id'] : 0;
}

//--------------------------------------------------------------------------------------

function void_work_order_issue($type_no) {
	begin_transaction();
	hook_db_prevoid(ST_MANUISSUE, $type_no);

	$void_entry = get_voided_entry(ST_MANUISSUE, $type_no);
	if ($void_entry)
		return;

	if (check_void_wo_issue($type_no))
		return;

	// FIXME update issued material cost

	// void the actual issue items and their quantities
	$sql = "UPDATE ".TB_PREF."wo_issue_items SET qty_issued = 0 WHERE issue_id=".db_escape($type_no);

	db_query($sql, 'A work order issue item could not be voided');

	void_gl_trans(ST_MANUISSUE, $type_no);

	void_stock_move(ST_MANUISSUE, $type_no);

	commit_transaction();
}