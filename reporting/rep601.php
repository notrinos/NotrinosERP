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
$page_security = 'SA_BANKREP';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');

//----------------------------------------------------------------------------------------------------

print_bank_transactions();

//----------------------------------------------------------------------------------------------------

function get_bank_balance_to($to, $account) {
	$to = date2sql($to);
	$sql = "SELECT SUM(amount) FROM ".TB_PREF."bank_trans WHERE bank_act='$account'
	AND trans_date < '$to'";
	$result = db_query($sql, 'The starting balance on hand could not be calculated');
	$row = db_fetch_row($result);
	return $row[0];
}

function get_bank_transactions($from, $to, $account) {
	$from = date2sql($from);
	$to = date2sql($to);
	$sql = "SELECT * FROM ".TB_PREF."bank_trans
		WHERE bank_act = '$account'
		AND trans_date >= '$from'
		AND trans_date <= '$to'
		ORDER BY trans_date, id";

	return db_query($sql, "The transactions for '$account' could not be retrieved");
}

function print_bank_transactions() {
	global $path_to_root, $systypes_array;

	$acc = $_POST['PARAM_0'];
	$from = $_POST['PARAM_1'];
	$to = $_POST['PARAM_2'];
	$zero = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$rep = new FrontReport(_('Bank Statement'), 'BankStatement', user_pagesize(), 9, $orientation);
	$dec = user_price_dec();

	$cols = array(0, 90, 120, 170, 225, 350, 400, 460, 520);

	$aligns = array('left',	'left',	'left',	'left',	'left',	'right', 'right', 'right');

	$headers = array(_('Type'),	_('#'),	_('Reference'), _('Date'), _('Person/Item'),
		_('Debit'),	_('Credit'), _('Balance'));

	if ($orientation == 'L')
		recalculate_cols($cols);
	$sql = "SELECT id, bank_account_name, bank_curr_code, bank_account_number FROM ".TB_PREF."bank_accounts";
	if ($acc != ALL_TEXT)
		$sql .= " WHERE id = $acc";
	$result = db_query($sql, 'could not retreive bank accounts');
	while ($account=db_fetch($result)) {
		$act = $account['bank_account_name'].' - '.$account['bank_curr_code'].' - '.$account['bank_account_number'];
		$params =   array( 	0 => $comments,
			1 => array('text' => _('Period'), 'from' => $from, 'to' => $to),
			2 => array('text' => _('Bank Account'),'from' => $act,'to' => ''));

		$rep->Font();
		$rep->pageNumber = 0;
		$rep->Info($params, $cols, $headers, $aligns);
		$rep->NewPage();


		$prev_balance = get_bank_balance_to($from, $account['id']);

		$trans = get_bank_transactions($from, $to, $account['id']);

		$rows = db_num_rows($trans);
		if ($prev_balance != 0.0 || $rows != 0) {
			$rep->Font('bold');
			$rep->TextCol(0, 3,	$act);
			$rep->TextCol(3, 5, _('Opening Balance'));
			if ($prev_balance > 0.0)
				$rep->AmountCol(5, 6, abs($prev_balance), $dec);
			else
				$rep->AmountCol(6, 7, abs($prev_balance), $dec);
			$rep->Font();
			$total = $prev_balance;
			$rep->NewLine(2);
			$total_debit = $total_credit = 0;
			if ($rows > 0) {
				// Keep a running total as we loop through
				// the transactions.
				
				while ($myrow=db_fetch($trans)) {
					if ($zero == 0 && $myrow['amount'] == 0.0)
						continue;
					$total += $myrow['amount'];

					$rep->TextCol(0, 1, $systypes_array[$myrow['type']]);
					$rep->TextCol(1, 2,	$myrow['trans_no']);
					$rep->TextCol(2, 3,	$myrow['ref']);
					$rep->DateCol(3, 4,	$myrow['trans_date'], true);
					$rep->TextCol(4, 5,	payment_person_name($myrow['person_type_id'], $myrow['person_id'], false));
					if ($myrow['amount'] > 0.0) {
						$rep->AmountCol(5, 6, abs($myrow['amount']), $dec);
						$total_debit += abs($myrow['amount']);
					}
					else {
						$rep->AmountCol(6, 7, abs($myrow['amount']), $dec);
						$total_credit += abs($myrow['amount']);
					}
					$rep->AmountCol(7, 8, $total, $dec);
					$rep->NewLine();
					if ($rep->row < $rep->bottomMargin + $rep->lineHeight) {
						$rep->Line($rep->row - 2);
						$rep->NewPage();
					}
				}
				$rep->NewLine();
			}
			
			// Print totals for the debit and credit columns.
			$rep->TextCol(3, 5, _('Total Debit / Credit'));
			$rep->AmountCol(5, 6, $total_debit, $dec);
			$rep->AmountCol(6, 7, $total_credit, $dec);
			$rep->NewLine(2);

			$rep->Font('bold');
			$rep->TextCol(3, 5,	_('Ending Balance'));
			if ($total > 0.0)
				$rep->AmountCol(5, 6, abs($total), $dec);
			else
				$rep->AmountCol(6, 7, abs($total), $dec);
			$rep->Font();
			$rep->Line($rep->row - $rep->lineHeight + 4);
			$rep->NewLine(2, 1);
			
			// Print the difference between starting and ending balances.
			$net_change = ($total - $prev_balance); 
			$rep->TextCol(3, 5, _('Net Change'));
			if ($total > 0.0)
				$rep->AmountCol(5, 6, $net_change, $dec, 0, 0, 0, 0, null, 1, True);
			else
				$rep->AmountCol(6, 7, $net_change, $dec, 0, 0, 0, 0, null, 1, True);
		}
	}
	$rep->End();
}
