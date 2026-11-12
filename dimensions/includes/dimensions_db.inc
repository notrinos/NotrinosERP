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

/**
 * Decide whether an axis may be assigned to a NEW or newly reassigned master.
 *
 * DIM-LFC-002: the enabled-axis posting preference governs new events only. The type must be a real
 * axis (1 or 2) and not above the company's enabled axis count; this is the server-side authority,
 * so a crafted POST cannot create a master on a disabled axis that the entry selector hides.
 *
 * @param mixed $type_ Requested axis (1 or 2)
 * @return bool True when the axis is currently open for new entries
 */
function dimension_axis_open_for_new_entry($type_) {
	if (!in_array($type_, array(1, 2, '1', '2'), true))
		return false;
	return (int)$type_ <= (int)get_company_pref('use_dimension');
}

function add_dimension($reference, $name, $type_, $date_, $due_date, $memo_) {
	global $Refs;

	if (!dimension_axis_open_for_new_entry($type_))
		return false;

	begin_transaction();

	$sql = "INSERT INTO ".TB_PREF."dimensions (reference, name, type_, date_, due_date) VALUES (".db_escape($reference).", ".db_escape($name).", ".db_escape($type_).", '".date2sql($date_)."', '".date2sql($due_date)."')";
	db_query($sql, 'could not add dimension');
	$id = db_insert_id();
	// C01: a key left behind by a deleted Dimension (retained reference and audit rows) is never recycled onto a new master.
	if (dimension_lifecycle_key_retained($id)) { cancel_transaction(); return false; }

	add_comments(ST_DIMENSION, $id, $date_, $memo_);

	$Refs->save(ST_DIMENSION, $id, $reference);

	commit_transaction();

	return $id;
}

function update_dimension($id, $name, $type_, $date_, $due_date, $memo_) {
	if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)
		|| !in_array($type_, array(1,2,'1','2'), true) || !begin_transaction()) return false;
	try {
		// A non-locking peek only chooses the lock order; the decision itself is taken on locked current reads.
		$peek = db_query("SELECT type_ FROM ".TB_PREF."dimensions WHERE id=".db_escape($id), 'could not read Dimension');
		$peeked = $peek ? db_fetch($peek) : false;
		if (!$peeked) return dimension_lifecycle_deny();
		if ((int)$peeked['type_'] !== (int)$type_) {
			// C01: an axis can only be reassigned while every owner family reports the Dimension unused. The recheck
			// takes the owner locks first and the master lock last, the same order every writer uses.
			if (!dimension_lifecycle_locked_unused($id, dimension_lifecycle_reassign_retained_evidence())) return dimension_lifecycle_deny();
		}
		$existing = db_query("SELECT * FROM ".TB_PREF."dimensions WHERE id=".db_escape($id)." FOR UPDATE", 'could not lock Dimension');
		$before = $existing ? db_fetch($existing) : false;
		// The axis must still be the one the decision was taken on; any change after the peek is a stale decision.
		if (!$before || (int)$before['type_'] !== (int)$peeked['type_']) return dimension_lifecycle_deny();

		$sql = "UPDATE ".TB_PREF."dimensions SET name=".db_escape($name).", type_ = ".db_escape($type_).", date_='".date2sql($date_)."', due_date='".date2sql($due_date)."' WHERE id = ".db_escape($id);

		if (!db_query($sql, 'could not update dimension')) { cancel_transaction(); return false; }

		update_comments(ST_DIMENSION, $id, null, $memo_);
		if (!dimension_lifecycle_audit($id, (int)$before['type_'] !== (int)$type_ ? 'axis reassigned' : 'updated', $before['reference'])) { cancel_transaction(); return false; }

		if (!commit_transaction()) return false;

		return $id;
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	}
}

/**
 * C01: delete one truly unused Dimension master.
 *
 * Inside one transaction the B10 locked, current-read recheck must report every owner family (scalar, retained
 * payload, protected HRM and payroll, shared, approval, session, extension) unused; used, unknown, a missing master,
 * a stale lock, a timeout or a deadlock all deny and roll back. Only the master row is deleted, then audited as
 * "deleted" in the same transaction (an audit failure rolls everything back). Nothing cascades: the reference
 * allocation and audit rows stay, so the number and history are never reused, remapped or erased; own comments, tags
 * and attachments block until their own pages remove them. add_dimension() refuses a recycled key.
 *
 * @param int|string $id Exact positive native Dimension identity
 * @return bool True only when the row was deleted and audited
 */
function delete_dimension($id) {
	$GLOBALS['dimension_lifecycle_last_denial'] = array('state'=>'unknown', 'owners'=>array());
	if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id) || !begin_transaction()) return false;
	try {
		if (!dimension_lifecycle_locked_unused($id, dimension_lifecycle_delete_retained_evidence())) return dimension_lifecycle_deny();
		$master = db_query("SELECT reference FROM ".TB_PREF."dimensions WHERE id=".db_escape($id)." FOR UPDATE", 'could not lock Dimension');
		$row = $master ? db_fetch($master) : false;
		if (!$row) return dimension_lifecycle_deny();
		if (!db_query("DELETE FROM ".TB_PREF."dimensions WHERE id=".db_escape($id), 'could not delete dimension')
			|| db_num_affected_rows() !== 1
			|| !dimension_lifecycle_audit($id, 'deleted', $row['reference'])) { cancel_transaction(); return false; }
		return commit_transaction();
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	}
}

function get_dimension($id, $allow_null=false) {
	$sql = "SELECT * FROM ".TB_PREF."dimensions	WHERE id=".db_escape($id);

	$result = db_query($sql, 'The dimension could not be retrieved');

	if (!$allow_null && db_num_rows($result) == 0)
		display_db_error("Could not find dimension $id", $sql);

	return db_fetch($result);
}

/**
 * Name one stored Dimension identity for display, including its historical state.
 *
 * A positive identity whose master row no longer exists is shown as an explicit unknown/deleted
 * Dimension instead of emitting a PHP warning or silently rendering blank, and a closed master is
 * marked as closed. Both are read-only state labels; they grant no reopen, edit or posting authority.
 *
 * @param int|string $id Stored native Dimension identity from a historical row
 * @param bool $html Whether the returned text is inserted into HTML
 * @param string $space Separator between reference and name
 * @return string Explicit historical display text
 */
function get_dimension_string($id, $html=false, $space=' ') {

	if ($id <= 0)
		return $html ? '&nbsp;' : '';
	$row = get_dimension($id, true);
	if (!is_array($row))
		$dim = sprintf(_('Unknown Dimension #%d (deleted)'), (int)$id);
	else {
		$dim = $row['reference'].$space.$row['name'];
		if ((int)$row['closed'] === 1)
			$dim .= ' ['._('closed').']';
	}

	return $html ? htmlspecialchars($dim, ENT_QUOTES, 'UTF-8') : $dim;
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
		// DIM-LFC-002: the enabled-axis posting preference governs new events only. It must never
		// narrow a historical search, so the caller's explicit Type/Open filters are the only scope.
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
