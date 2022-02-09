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
function add_journal($type, $trans_no, $amount, $tran_date, $currency, $reference, $source_ref='', $rate=1, $event_date='', $doc_date='') {
	$sql = "INSERT INTO ".TB_PREF."journal(
	`type`,`trans_no`, `amount`, `currency`, `rate`, `reference`, `source_ref`, `tran_date`,`event_date`,`doc_date`)
	VALUES("
	.db_escape($type).","
	.db_escape($trans_no).","
	.db_escape($amount).","
	.db_escape($currency).","
	.db_escape($rate).","
	.db_escape($reference).","
	.db_escape($source_ref).","
	."'".date2sql($tran_date)."','"
	.($event_date === '' ? '0000-00-00' : date2sql($event_date))."','"
	.($doc_date === '' ? '0000-00-00' : date2sql($doc_date))."')";

	return db_query($sql, 'cannot add journal entry');
}

function update_journal($type, $trans_no, $amount, $tran_date, $currency, $reference, $source_ref='', $rate=1, $event_date='', $doc_date='') {
	$sql = "UPDATE ".TB_PREF."journal SET "
	."`amount`=".db_escape($amount).","
	."`currency`=".db_escape($currency).","
	."`rate`=".db_escape($rate).","
	."`reference`=".db_escape($reference).","
	."`source_ref`=".db_escape($source_ref).","
	."`tran_date`='".date2sql($tran_date)."',"
	."`event_date`='".($event_date === '' ? '0000-00-00' : date2sql($event_date))."',"
	."`doc_date`='".($doc_date === '' ? '0000-00-00' : date2sql($doc_date))."'
	WHERE `type`=".db_escape($type)." AND " ."`trans_no`=".db_escape($trans_no);

	return db_query($sql, 'cannot update journal entry');
}

function delete_journal($type, $trans_no) {
	$sql = "DELETE FROM ".TB_PREF."journal WHERE `type`=".db_escape($type)." AND " ."`trans_no`=".db_escape($trans_no);

	return db_query($sql, 'cannot delete journal entry');
}

function get_journal($type, $trans_no) {
	$sql = "SELECT * FROM ".TB_PREF."journal WHERE `type`=".db_escape($type)." AND " ."`trans_no`=".db_escape($trans_no);

	$myrow= db_query($sql, 'cannot retrieve journal entry');
	return db_fetch($myrow);
}

/*
*	Write headers in debtor_trans for allocation purposes.
*	Amount in customer currency
*/
function write_cust_journal($trans_type, $trans_no, $branch_id, $date, $ref, $amount, $rate) {

	$pos = user_pos();
	// we have to select any branch_id for given AR_act - in fact w
	$sql = "INSERT INTO ".TB_PREF."debtor_trans (trans_no, type, debtor_no, branch_code, tran_date, reference, ov_amount, rate, pos_id)
		SELECT "
		.db_escape($trans_no).", ".db_escape($trans_type).", debtor_no,"
		.db_escape($branch_id).",'".date2sql($date)."', ".db_escape($ref).","
		.db_escape($amount).", ".db_escape($rate).", ".db_escape($pos)." 
		FROM ".TB_PREF."cust_branch WHERE branch_code=".db_escape($branch_id);

	return db_query($sql, 'cannot write cust_journal');
}

/*
*	Write headers in supp_trans for allocation purposes.
*	Amount in supplier currency
*/
function write_supp_journal($trans_type, $trans_no, $supp_id, $date, $ref, $amount, $rate, $src_ref) {
	$sql = "INSERT INTO ".TB_PREF."supp_trans (trans_no, type, supplier_id, tran_date, reference, ov_amount, rate, supp_reference)
		VALUES (".db_escape($trans_no).", ".db_escape($trans_type).", ".db_escape($supp_id).", '".date2sql($date)."',"
		.db_escape($ref).", ".db_escape($amount).",".db_escape($rate).",".db_escape($src_ref).")";

	return db_query($sql, 'cannot write supp_journal');
}

/*
*	Write/update journal entries.
*/
function write_journal_entries(&$cart) {
	global $Refs;

	$date_ = $cart->tran_date;
	$ref   = $cart->reference;
	$memo_ = $cart->memo_;
	$trans_type = $cart->trans_type;
	$new = $cart->order_id == 0;

	begin_transaction();

	hook_db_prewrite($cart, ST_JOURNAL);

	if (!$new) {
		$msg = void_transaction($trans_type, $cart->order_id, Today(), _('Document reentered.'));
		if ($msg) {
			display_error($msg);
			return false;
		}
	}

	$trans_id = $cart->order_id = get_next_trans_no($trans_type);
	$total = $cart->gl_items_total_debit();

	$cart->write_gl();

	add_journal($trans_type, $trans_id, $total,	$date_ , $cart->currency, $ref, $cart->source_ref, $cart->rate, $cart->event_date, $cart->doc_date);

	$Refs->save($trans_type, $trans_id, $ref);
	if ($new)
		add_comments($trans_type, $trans_id, $date_, $memo_);
	else
		update_comments($trans_type, $trans_id, null, $memo_);

	add_audit_trail($trans_type, $trans_id, $date_);

	hook_db_postwrite($cart, ST_JOURNAL);

	commit_transaction();

	return $trans_id;
}

function void_journal_trans($type, $type_no, $use_transaction=true) {
	if ($use_transaction)
		begin_transaction();

	$sql = "UPDATE ".TB_PREF."journal SET amount = 0 WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);

	db_query($sql, "could not void journal transaction for type=$type and trans_no=$type_no");

	void_bank_trans($type, $type_no, true);

	if ($use_transaction)
		commit_transaction();
}
