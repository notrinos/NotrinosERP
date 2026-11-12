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
$page_security = 'SA_GLREP';
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/admin/db/fiscalyears_db.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');

// DIM-LFC-002/H03: prn_redirect bootstraps with SA_OPEN, so the report rechecks its own authority
// before reading any parameter or GL row.
check_page_security('SA_GLREP');

//----------------------------------------------------------------------------------------------------

print_GL_transactions();

//----------------------------------------------------------------------------------------------------

function print_GL_transactions() {
	global $path_to_root, $systypes_array;

	$dimension = $dimension2 = 0;

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$fromacc = $_POST['PARAM_2'];
	$toacc = $_POST['PARAM_3'];
	// DIM-LFC-002: both historical axes are read on every preference so an axis-2 classification
	// remains reportable after axis 2 is disabled for new postings.
	$dimension = $_POST['PARAM_4'];
	$dimension2 = $_POST['PARAM_5'];
	$comments = isset($_POST['PARAM_6']) ? $_POST['PARAM_6'] : '';
	$orientation = isset($_POST['PARAM_7']) ? $_POST['PARAM_7'] : 0;
	$destination = isset($_POST['PARAM_8']) ? $_POST['PARAM_8'] : 0;
	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');
	$orientation = ($orientation ? 'L' : 'P');

	$rep = new FrontReport(_('GL Account Transactions'), 'GLAccountTransactions', user_pagesize(), 9, $orientation);
	$dec = user_price_dec();

	$cols = array(0, 65, 105, 125, 175, 230, 290, 345, 405, 465, 525);
	//------------0--1---2----3----4----5----6----7----8----9----10-------
	$headers = array(_('Type'),	_('Ref'), _('#'),	_('Date'), _('Dimension').' 1', _('Dimension').' 2', _('Person/Item'), _('Debit'),	_('Credit'), _('Balance'));
	$aligns = array('left', 'left', 'left',	'left',	'left',	'left',	'left',	'right', 'right', 'right');

	$params =   array( 	0 => $comments,
					1 => array('text' => _('Period'), 'from' => $from, 'to' => $to),
					2 => array('text' => _('Accounts'),'from' => $fromacc,'to' => $toacc),
					3 => array('text' => _('Dimension').' 1', 'from' => get_dimension_string($dimension), 'to' => ''),
					4 => array('text' => _('Dimension').' 2', 'from' => get_dimension_string($dimension2), 'to' => ''));
	if ($orientation == 'L')
		recalculate_cols($cols);

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$account_rows = chart_master_entity::all_with_types($fromacc, $toacc);

	foreach ($account_rows as $account) {
		if (is_account_balancesheet($account['account_code']))
			$begin = '';
		else {
			$begin = get_fiscalyear_begin_for_date($from);
			if (date1_greater_date2($begin, $from))
				$begin = $from;
			$begin = add_days($begin, -1);
		}
		$prev_balance = get_gl_balance_from_to($begin, $from, $account['account_code'], $dimension, $dimension2);

		$trans = get_gl_transactions($from, $to, -1, $account['account_code'], $dimension, $dimension2);
		$rows = db_num_rows($trans);
		if ($prev_balance == 0.0 && $rows == 0)
			continue;
		$rep->Font('bold');
		$rep->TextCol(0, 4,	$account['account_code'] . ' ' . $account['account_name'], -2);
		$rep->TextCol(4, 6, _('Opening Balance'));
		if ($prev_balance > 0.0)
			$rep->AmountCol(7, 8, abs($prev_balance), $dec);
		else
			$rep->AmountCol(8, 9, abs($prev_balance), $dec);
		$rep->Font();
		$total = $prev_balance;
		$rep->NewLine(2);
		if ($rows > 0) {
			while ($myrow=db_fetch($trans)) {
				$total += $myrow['amount'];

				$rep->TextCol(0, 1, $systypes_array[$myrow['type']], -2);
				$reference = get_reference($myrow['type'], $myrow['type_no']);
				$rep->TextCol(1, 2, $reference);
				$rep->TextCol(2, 3,	$myrow['type_no'], -2);
				$rep->DateCol(3, 4,	$myrow['tran_date'], true);
				$rep->TextCol(4, 5,	get_dimension_string($myrow['dimension_id']));
				$rep->TextCol(5, 6,	get_dimension_string($myrow['dimension2_id']));
				$txt = payment_person_name($myrow['person_type_id'], $myrow['person_id'], false);
				$memo = $myrow['memo_'];
				if ($txt != '') {
					if ($memo != '')
						$txt = $txt.'/'.$memo;
				}
				else
					$txt = $memo;
				$rep->TextCol(6, 7,	$txt, -2);
				if ($myrow['amount'] > 0.0)
					$rep->AmountCol(7, 8, abs($myrow['amount']), $dec);
				else
					$rep->AmountCol(8, 9, abs($myrow['amount']), $dec);
				$rep->TextCol(9, 10, number_format2($total, $dec));
				$rep->NewLine();
				if ($rep->row < $rep->bottomMargin + $rep->lineHeight) {
					$rep->Line($rep->row - 2);
					$rep->NewPage();
				}
			}
			$rep->NewLine();
		}
		$rep->Font('bold');
		$rep->TextCol(4, 6,	_('Ending Balance'));
		if ($total > 0.0)
			$rep->AmountCol(7, 8, abs($total), $dec);
		else
			$rep->AmountCol(8, 9, abs($total), $dec);
		$rep->Font();
		$rep->Line($rep->row - $rep->lineHeight + 4);
		$rep->NewLine(2, 1);
	}
	$rep->End();
}
