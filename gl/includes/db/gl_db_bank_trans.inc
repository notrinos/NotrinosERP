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
require_once __DIR__.'/../../../dimensions/includes/dimension_document_classification.inc';

/**
 * Insert one bank ledger row for a payment, deposit, transfer or journal bank line.
 *
 * `$classification` is the optional DIM-GL-001 declaration produced by
 * dimension_document_classification_row()/dimension_document_classification_line(): the exact pair
 * for a single-classified document or line, an explicit `mixed` marker when the document lines
 * disagree, or explicit `unassigned`. When it is supplied the row persists the declaration together
 * with `dimension_id`/`dimension2_id`, and the ordinary financial fence admits the pair exactly like
 * every other source owner. `null` keeps the characterized legacy adapter result byte-for-byte for
 * source families whose task has not converted them yet (customer/supplier payment wrappers): the
 * columns are omitted, the fence still reconciles them as 0/0, and no classification is inferred
 * from the last GL line. The amount is converted to the bank currency exactly as before.
 *
 * @param int $type Stored transaction type
 * @param int $trans_no Stored transaction number
 * @param mixed $bank_act Bank account id
 * @param string $ref Document reference
 * @param mixed $date_ Display date
 * @param float $amount Amount in $currency
 * @param mixed $person_type_id Native person type, or null
 * @param mixed $person_id Native person id
 * @param string $currency Currency of $amount
 * @param string $err_msg Native error message
 * @param float $rate Explicit exchange rate, or 0 to read the document rate
 * @param array|null $classification Retained document/line declaration
 * @return mixed Inserted bank row identity
 */
function add_bank_trans($type, $trans_no, $bank_act, $ref, $date_, $amount, $person_type_id, $person_id, $currency='', $err_msg='', $rate=0, $classification=null) {

	// convert $amount to the bank's currency
	if ($currency != '') {
		$bank_account_currency = get_bank_account_currency($bank_act);
		if ($rate == 0)
			$to_bank_currency = get_exchange_rate_from_to($currency, $bank_account_currency, $date_);
		else
			$to_bank_currency = 1 / $rate;
		$amount_bank = ($amount / $to_bank_currency);
	}
	else
		$amount_bank = $amount;
	$amount_bank = round2($amount_bank, user_price_dec());

	$columns = 'type, trans_no, bank_act, ref, trans_date, amount, person_type_id, person_id';
	$values = "$type, $trans_no, '$bank_act', ".db_escape($ref).", '".date2sql($date_)."',".db_escape($amount_bank).", ".db_escape($person_type_id).", ". db_escape($person_id);
	$next = array('dimension_id' => 0, 'dimension2_id' => 0);
	if ($classification !== null) {
		if (!is_array($classification) || !array_key_exists('dimension_id', $classification)
			|| !array_key_exists('dimension2_id', $classification)
			|| !array_key_exists('state', $classification)
			|| !array_key_exists('custom_data', $classification))
			dimension_financial_reference_failure();
		$declared = dimension_document_classification_read($classification['custom_data'],
			$classification['dimension_id'], $classification['dimension2_id']);
		if (!$declared['ok'] || !$declared['trusted'] || $declared['state'] !== $classification['state'])
			dimension_financial_reference_failure();
		$columns .= ', dimension_id, dimension2_id, custom_data';
		// Retained JSON is escaped byte-exactly: db_escape() HTML-encodes quotes, which would
		// corrupt the declaration and break the fence's exact payload re-read.
		$values .= ', '.$declared['pair']['dimension_id'].', '.$declared['pair']['dimension2_id']
			.", '".db_escape_function($classification['custom_data'])."'";
		$next = array('dimension_id' => $declared['pair']['dimension_id'],
			'dimension2_id' => $declared['pair']['dimension2_id'],
			'custom_data' => $classification['custom_data']);
	}
	$sql = "INSERT INTO ".TB_PREF."bank_trans (".$columns.") VALUES (".$values.")";

	if (empty($err_msg))
		$err_msg = 'The bank transaction could not be inserted';

    dimension_financial_reference_write('bank_trans',null,$next,
        function($admitted) use ($sql) {return db_query($sql)?db_insert_id():false;});
}

function exists_bank_trans($type, $type_no) {
	$sql = "SELECT trans_no FROM ".TB_PREF."bank_trans WHERE type=".db_escape($type)." AND trans_no=".db_escape($type_no);
	$result = db_query($sql, 'Cannot retreive a bank transaction');

	return (db_num_rows($result) > 0);
}

function get_bank_trans($type, $trans_no=null, $person_type_id=null, $person_id=null) {
	$sql = "SELECT bt.*, act.*,
		IFNULL(abs(dt.ov_amount), IFNULL(ABS(st.ov_amount), bt.amount)) settled_amount,
		IFNULL(abs(dt.ov_amount/bt.amount), IFNULL(ABS(st.ov_amount/bt.amount), 1)) settle_rate,
		IFNULL(debtor.curr_code, IFNULL(supplier.curr_code, act.bank_curr_code)) settle_curr

		FROM ".TB_PREF."bank_trans bt
			LEFT JOIN ".TB_PREF."debtor_trans dt ON dt.type=bt.type AND dt.trans_no=bt.trans_no
			LEFT JOIN ".TB_PREF."debtors_master debtor ON debtor.debtor_no = dt.debtor_no
			LEFT JOIN ".TB_PREF."supp_trans st ON st.type=bt.type AND st.trans_no=bt.trans_no
			LEFT JOIN ".TB_PREF."suppliers supplier ON supplier.supplier_id = st.supplier_id,
			".TB_PREF."bank_accounts act
		WHERE act.id=bt.bank_act ";
	if (isset($type))
		$sql .= " AND bt.type=".db_escape($type);
	if (isset($trans_no))
		$sql .= " AND bt.trans_no = ".db_escape($trans_no);
	if (isset($person_type_id))
		$sql .= " AND bt.person_type_id = ".db_escape($person_type_id);
	if (isset($person_id))
		$sql .= " AND bt.person_id = ".db_escape($person_id);
	$sql .= " ORDER BY trans_date, bt.id";

	return db_query($sql, 'failed query for bank transaction');
}

function get_bank_trans_for_bank_account($bank_account, $from, $to) {

	$sql = "SELECT t.* FROM ".TB_PREF."bank_trans t LEFT JOIN ".TB_PREF."voided v ON t.type=v.type AND t.trans_no=v.id
		WHERE t.bank_act = ".db_escape($bank_account)."
		AND ISNULL(v.date_)
		AND trans_date >= '".date2sql($from)."'
		AND trans_date <= '".date2sql($to)."'
		AND amount != 0
		ORDER BY trans_date, t.id";

	return db_query($sql, "The transactions for '".$bank_account."' could not be retrieved");
}

function get_balance_before_for_bank_account($bank_account, $from) {
	
	$sql = "SELECT SUM(amount) FROM ".TB_PREF."bank_trans WHERE bank_act=".db_escape($bank_account)." AND trans_date < '".date2sql($from)."'";

	$before_qty = db_query($sql, 'The starting balance on hand could not be calculated');
	$bfw_row = db_fetch_row($before_qty);

	return $bfw_row[0];
}

function get_gl_trans_value($account, $type, $trans_no) {
	$sql = "SELECT SUM(amount) FROM ".TB_PREF."gl_trans WHERE account=".db_escape($account)." AND type=".db_escape($type)." AND type_no=".db_escape($trans_no);

	$result = db_query($sql, 'query for gl trans value failed');

	$row = db_fetch_row($result);
	return $row[0];
}

function void_bank_trans($type, $type_no, $nested=false) {

	if (!$nested)
		begin_transaction();

    dimension_financial_reference_group('bank_trans',$type,$type_no,function($row) {
        return db_query('UPDATE '.TB_PREF.'bank_trans SET amount=0 WHERE id='.(int)$row['id'])!==false;
    });

	void_gl_trans($type, $type_no, true);

	// in case it's a customer trans - probably better to check first
	void_cust_allocations($type, $type_no);
	void_customer_trans($type, $type_no);

	// in case it's a supplier trans - probably better to check first
	void_supp_allocations($type, $type_no);
	void_supp_trans($type, $type_no);

	void_trans_tax_details($type, $type_no);

	if (!$nested)
		commit_transaction();
}

//
//	Check account history to find transaction which would exceed authorized overdraft for given account.
//	Returns null or transaction in conflict. Running balance is checked on daily basis only, 
//	to enable ID change after edition.
//	$delta_amount - tested change in bank balance at $date.
//
function check_bank_account_history($delta_amount, $bank_account, $date=null, $user=null, $balance_offset = 0) {
	if ($delta_amount >= 0 && isset($date))
		 return null;	// amount increase is always safe

	$balance = $date ? get_bank_account_limit($bank_account, $date, $user) : 0;

	if (!isset($balance) && isset($date))
		return null;	// unlimited account

	$balance += $balance_offset;
	if (floatcmp($balance, -$delta_amount) < 0)
		return array('amount' => $balance + $delta_amount, 'trans_date'=> date2sql($date));

	$balance += $delta_amount;

	$sql = "SELECT sum(amount) as amount, trans_date, trans_no, type FROM ".TB_PREF."bank_trans WHERE bank_act=".db_escape($bank_account);

	if ($date)
		$sql .= " AND trans_date > '".date2sql($date)."'";
	
	$sql .= " GROUP BY trans_date ORDER BY trans_date ASC";

	$history = db_query($sql, 'cannot retrieve cash account history');

	while ($trans = db_fetch($history)) {
		$balance += $trans['amount'];
		if (round2($balance, user_price_dec()) < 0) {
			$trans['amount'] = $balance;
			return $trans;
		}
	}

	return null;
}

//
//	Check bank transfer, deposit or customer deposit before voiding.
//
function check_void_bank_trans($type, $type_no) {
	$moves = get_bank_trans($type, $type_no);
	while ($trans = db_fetch($moves)) {
		if ($trans['amount'] > 0) // skip transfer input part
			return check_bank_account_history(-$trans['amount'], $trans['bank_act'], sql2date($trans['trans_date'])) == null;
	}
	return true;
}
