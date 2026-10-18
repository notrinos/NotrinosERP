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
require_once __DIR__.'/dimension_lifecycle_db.inc';

function add_dimension($reference, $name, $type_, $date_, $due_date, $memo_) {
	global $Refs;

	begin_transaction();

	$sql = "INSERT INTO ".TB_PREF."dimensions (reference, name, type_, date_, due_date) VALUES (".db_escape($reference).", ".db_escape($name).", ".db_escape($type_).", '".date2sql($date_)."', '".date2sql($due_date)."')";
	db_query($sql, 'could not add dimension');
	$id = db_insert_id();

	add_comments(ST_DIMENSION, $id, $date_, $memo_);

	$Refs->save(ST_DIMENSION, $id, $reference);

	commit_transaction();

	return $id;
}

function update_dimension($id, $name, $type_, $date_, $due_date, $memo_) {
	if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)
		|| !in_array($type_, array(1,2,'1','2'), true) || !begin_transaction()) return false;
	try {
		$existing = db_query("SELECT * FROM ".TB_PREF."dimensions WHERE id=".db_escape($id)." FOR UPDATE", 'could not lock Dimension');
		$before = $existing ? db_fetch($existing) : false;
		if (!$before) { cancel_transaction(); return false; }
		if ((int)$before['type_'] !== (int)$type_) {
			// Retained payload/session/extension ownership and all writer fences are
			// not yet closed under DIM-LFC-001B. Preserve identity even if GL is empty.
			cancel_transaction();
			return false;
		}

		$sql = "UPDATE ".TB_PREF."dimensions SET name=".db_escape($name).", type_ = ".db_escape($type_).", date_='".date2sql($date_)."', due_date='".date2sql($due_date)."' WHERE id = ".db_escape($id);

		if (!db_query($sql, 'could not update dimension')) { cancel_transaction(); return false; }

		update_comments(ST_DIMENSION, $id, null, $memo_);
		if (!dimension_lifecycle_audit($id, 'updated', $before['reference'])) { cancel_transaction(); return false; }

		if (!commit_transaction()) return false;

		return $id;
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	}
}

/** Keep deletion closed until every owner and historical inverse is fenced. */
function delete_dimension($id) {
	if (!dimension_lifecycle_authorized()) return false;
	// A scalar scan cannot prove absence of retained carts, protected payroll
	// custody or external/inactive extension references. No cascade is safe.
	return false;
}

function get_dimension($id, $allow_null=false) {
	$sql = "SELECT * FROM ".TB_PREF."dimensions	WHERE id=".db_escape($id);

	$result = db_query($sql, 'The dimension could not be retrieved');

	if (!$allow_null && db_num_rows($result) == 0)
		display_db_error("Could not find dimension $id", $sql);

	return db_fetch($result);
}

function get_dimension_string($id, $html=false, $space=' ') {

	if ($id <= 0)
		$dim = $html ? '&nbsp;' : '';
	else {
		$row = get_dimension($id, true);
		$dim = $row['reference'].$space.$row['name'];
	}

	return $dim;
}

function get_dimensions() {
	$sql = "SELECT * FROM ".TB_PREF."dimensions ORDER BY date_";

	return db_query($sql, 'The dimensions could not be retrieved');
}

function dimension_has_deposits($id) {
	return dimension_has_payments($id);
}

function dimension_has_payments($id) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."gl_trans WHERE dimension_id = ".db_escape($id). " OR dimension2_id = ".db_escape($id);
	$res = db_query($sql, 'Transactions could not be calculated');
	$row = db_fetch_row($res);
	return $row[0] > 0;
}

function dimension_is_closed($id) {
	$result = get_dimension($id);
	return $result['closed'] == '1';
}

function close_dimension($id) {
	if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id) || !begin_transaction()) return false;
	try {
		$result = db_query("SELECT reference,closed FROM ".TB_PREF."dimensions WHERE id=".db_escape($id)." FOR UPDATE", 'could not lock Dimension');
		$row = $result ? db_fetch($result) : false;
		if (!$row) { cancel_transaction(); return false; }
		if ((int)$row['closed'] !== 1) {
			if (!db_query("UPDATE ".TB_PREF."dimensions SET closed='1' WHERE id = ".db_escape($id), 'could not close dimension')
				|| !dimension_lifecycle_audit($id, 'closed', $row['reference'])) { cancel_transaction(); return false; }
		}
		return commit_transaction();
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	}
}

function reopen_dimension($id) {
	if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id) || !begin_transaction()) return false;
	try {
		$result = db_query("SELECT reference,closed FROM ".TB_PREF."dimensions WHERE id=".db_escape($id)." FOR UPDATE", 'could not lock Dimension');
		$row = $result ? db_fetch($result) : false;
		if (!$row) { cancel_transaction(); return false; }
		if ((int)$row['closed'] !== 0) {
			if (!db_query("UPDATE ".TB_PREF."dimensions SET closed='0' WHERE id = ".db_escape($id), 'could not reopen dimension')
				|| !dimension_lifecycle_audit($id, 'reopened', $row['reference'])) { cancel_transaction(); return false; }
		}
		return commit_transaction();
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	}
}

function get_dimension_balance_all($id, $from, $to) {
	$sql = "SELECT account, coa.account_name, sum(amount) AS amt FROM ".TB_PREF."gl_trans trans,".TB_PREF."chart_master coa WHERE trans.account = coa.account_code AND (dimension_id = ".db_escape($id)." OR dimension2_id = ".db_escape($id).") AND tran_date >= '".date2sql($from)."' AND tran_date <= '".date2sql($to)."' GROUP BY account";
	return db_query($sql, 'Transactions could not be calculated');
}

function get_dimension_balance($id, $from, $to) {
	
	$sql = "SELECT SUM(amount) FROM ".TB_PREF."gl_trans WHERE tran_date >= '" .	date2sql($from) . "' AND tran_date <= '".date2sql($to)."' AND (dimension_id = ".db_escape($id)." OR dimension2_id = ".db_escape($id).")";
	$res = db_query($sql, 'Sum of transactions could not be calculated');
	$row = db_fetch_row($res);

	return $row[0];
}

function get_sql_for_search_dimensions($dim, $from, $to, $order='', $type = -1, $open=false, $overdue=false) {
	$sql = "SELECT dim.id, dim.reference, dim.name, dim.type_, dim.date_, dim.due_date, dim.closed FROM ".TB_PREF."dimensions as dim WHERE id > 0";

	if ($order)
		$sql .= " AND reference LIKE ".db_escape("%". $order . "%");
	else {
		if ($dim == 1)
			$sql .= " AND type_=1";
		if ($open)
			$sql .= " AND closed=0";
		if ($type > 0)
			$sql .= " AND type_=".db_escape($type);
		if ($overdue)
			$sql .= " AND due_date < '".date2sql(Today())."'";

		$sql .= " AND date_ >= '".date2sql($from)."' AND date_ <= '".date2sql($to)."'";
	}

	return $sql;
}
