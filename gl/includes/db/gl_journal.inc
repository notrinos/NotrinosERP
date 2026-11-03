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
require_once __DIR__.'/../../../dimensions/includes/dimension_financial_reference_writer.inc';

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
    if(!begin_transaction())dimension_financial_reference_failure();
    try {

	$pos = user_pos();
	// we have to select any branch_id for given AR_act - in fact w
	$sql = "INSERT INTO ".TB_PREF."debtor_trans (trans_no, type, debtor_no, branch_code, tran_date, reference, ov_amount, rate, pos_id)
		SELECT "
		.db_escape($trans_no).", ".db_escape($trans_type).", debtor_no,"
		.db_escape($branch_id).",'".date2sql($date)."', ".db_escape($ref).","
		.db_escape($amount).", ".db_escape($rate).", ".db_escape($pos)." 
		FROM ".TB_PREF."cust_branch WHERE branch_code=".db_escape($branch_id);


    $branch=db_query('SELECT debtor_no FROM '.TB_PREF.'cust_branch WHERE branch_code='.db_escape($branch_id).' FOR UPDATE');
    $customer=$branch?db_fetch_assoc($branch):false;if(!$customer)dimension_financial_reference_failure();
    $key=array('type'=>$trans_type,'trans_no'=>$trans_no,'debtor_no'=>$customer['debtor_no']);
    $result=dimension_financial_reference_write('debtor_trans',null,array('dimension_id'=>0,'dimension2_id'=>0),
        function($admitted)use($sql,$key){return db_query($sql)?$key:false;})!==false;
    if(!commit_transaction())dimension_financial_reference_failure();return $result;
    }catch(DimensionFinancialReferenceWriteException $exception){cancel_transaction();throw $exception;}
}

/*
*	Write headers in supp_trans for allocation purposes.
*	Amount in supplier currency
*/
function write_supp_journal($trans_type, $trans_no, $supp_id, $date, $ref, $amount, $rate, $src_ref) {
	$sql = "INSERT INTO ".TB_PREF."supp_trans (trans_no, type, supplier_id, tran_date, reference, ov_amount, rate, supp_reference)
		VALUES (".db_escape($trans_no).", ".db_escape($trans_type).", ".db_escape($supp_id).", '".date2sql($date)."',"
		.db_escape($ref).", ".db_escape($amount).",".db_escape($rate).",".db_escape($src_ref).")";

	dimension_financial_reference_write('supp_trans',null,array(),function($admitted)use($sql,$trans_type,$trans_no,$supp_id,&$result){$result=db_query($sql,'cannot write supp_journal');return $result?array('type'=>$trans_type,'trans_no'=>$trans_no,'supplier_id'=>$supp_id):false;});
    return $result;
}

/*
*	Write/update journal entries.
*/
function write_journal_entries(&$cart) {
    $original=clone $cart;
    foreach($original->gl_items as $index=>$line)$original->gl_items[$index]=clone $line;
    try {
	global $Refs;

	$date_ = $cart->tran_date;
	$ref   = $cart->reference;
	$memo_ = $cart->memo_;
	$trans_type = $cart->trans_type;
	$new = $cart->order_id == 0;
	$old_no = $cart->order_id;

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
	if ($old_no) {
		$aid = 0;
		$aid = has_attachment($trans_type, $old_no);
		if ($aid != 0) {
			move_trans_attachments($trans_type, $old_no, $trans_id);
			//$row = get_attachment($aid);
			//update_attachment($aid, $row['type_no'], $trans_id, $row['description'],
			//	$row['filename'], $row['unique_name'], $row['filesize'], $row['filetype']);
		}
	}
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
    if(isset($cart->reconciled_date)) {
        $date=$cart->reconciled_date;
        if(!is_string($date) || !preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D',$date))dimension_financial_reference_failure();
        dimension_financial_reference_group('bank_trans',ST_JOURNAL,$trans_id,function($row)use($date){
            return db_query('UPDATE '.TB_PREF.'bank_trans SET reconciled='.db_escape($date).' WHERE id='.(int)$row['id'])!==false;
        });
    }


	commit_transaction();

	return $trans_id;
    }catch(DimensionFinancialReferenceWriteException $exception){
        cancel_transaction();
        foreach(get_object_vars($cart) as $field=>$value)if(!array_key_exists($field,get_object_vars($original)))unset($cart->$field);
        foreach(get_object_vars($original) as $field=>$value)$cart->$field=$value;
        throw $exception;
    }

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
