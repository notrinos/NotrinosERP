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

/**
 * DIM-GL-002: revenue/cost accrual schedule planning, classification and atomic posting.
 *
 * The accrual page used to build and post every period inline: it validated nothing about the
 * selected Dimension before the first journal, and a refused/closed classification half way through
 * the loop left the caller with an open transaction and several posted periods. This module moves
 * the whole schedule into one testable command:
 *
 *  - `accrual_schedule_plan()` validates the date range, the amount split and the classification
 *    before anything is written. The revenue/cost line is an explicit schedule source resolved by
 *    the shared DIM-FND-005 resolver and validated by the ordinary DIM-FND-004 new-event contract;
 *    the accrued-balance line is a non-P&L control leg the per-account policy must admit.
 *  - the amount rounding remainder stays in the first period exactly as the legacy page computed it,
 *    so the posted total equals the entered amount at currency precision;
 *  - `accrual_schedule_post()` posts all periods inside one transaction. Any refusal (a value closed
 *    concurrently, a policy change, a failed insert) cancels the whole schedule: no partial period,
 *    no orphan number and a retry after the cause is corrected starts from the first period.
 *
 * No default is resolved for a blank schedule selection: an explicit blank stays unassigned and is
 * subject to the company's own requiredness rule, never to a partner/item default.
 *
 * PHP 5.6 compatible syntax.
 */
require_once dirname(dirname(__DIR__)).'/dimensions/includes/dimension_derived_posting_policy.inc';

/**
 * Reviewed period split of one accrual schedule.
 *
 * @param array $input date_, amount, periods, freq
 * @return array ok, errors, codes, periods (date/amount), total, am, am0
 */
function accrual_schedule_split($input)
{
	$result = array('ok' => false, 'errors' => array(), 'codes' => array(),
		'periods' => array(), 'total' => 0.0, 'am' => 0.0, 'am0' => 0.0);
	$input = is_array($input) ? $input : array();
	$date = isset($input['date_']) ? (string)$input['date_'] : '';
	$amount = isset($input['amount']) && is_numeric($input['amount']) ? (float)$input['amount'] : 0.0;
	$periods = isset($input['periods']) && is_numeric($input['periods']) ? (int)$input['periods'] : 0;
	$freq = isset($input['freq']) && is_numeric($input['freq']) ? (int)$input['freq'] : 0;
	if (!is_date($date)) {
		$result['codes'][] = 'invalid_date';
		$result['errors'][] = _('The entered date is invalid.');
		return $result;
	}
	if (!is_date_in_fiscalyear($date)) {
		$result['codes'][] = 'closed_fiscal_year';
		$result['errors'][] = _('The entered date is out of fiscal year or is closed for further data entry.');
		return $result;
	}
	if ($amount == 0.0) {
		$result['codes'][] = 'zero_amount';
		$result['errors'][] = _('The amount can not be 0.');
		return $result;
	}
	if ($periods < 1) {
		$result['codes'][] = 'invalid_periods';
		$result['errors'][] = _('The periods must be greater than 0.');
		return $result;
	}
	if (!in_array($freq, array(1, 2, 3, 4), true)) {
		$result['codes'][] = 'invalid_frequency';
		$result['errors'][] = _('The selected accrual frequency is not part of the schedule contract.');
		return $result;
	}
	$per = $periods - 1;
	$date_ = $date;
	$current = $date;
	if ($freq == 3 || $freq == 4) {
		$date_ = begin_month($date); // avoid skip on shorter months
		$current = end_month($date); // avoid skip on shorter months
	}
	$lastdate = ($freq == 1 ? add_days($date_, 7 * $per) :
		($freq == 2 ? add_days($date_, 14 * $per) :
		($freq == 3 ? end_month(add_months($date_, $per)) :
		end_month(add_months($date_, 3 * $per)))));
	if (!is_date_in_fiscalyears($lastdate, false)) {
		$result['codes'][] = 'period_outside_fiscal_year';
		$result['errors'][] = _('Some of the period dates are outside the fiscal year or are closed for further data entry. Create a new fiscal year first!');
		return $result;
	}
	$am = round2($amount / $periods, user_price_dec());
	$am0 = ($am * $periods != $amount) ? $am + $amount - $am * $periods : $am;
	$rows = array();
	$total = 0.0;
	for ($index = 0; $index < $periods; $index++) {
		if ($index > 0) {
			switch ($freq) {
				case 1:
					$current = $date_ = add_days($date_, 7);
					break;
				case 2:
					$current = $date_ = add_days($date_, 14);
					break;
				case 3:
					$date_ = add_months($date_, 1);
					$current = end_month($date_);
					break;
				case 4:
					$date_ = add_months($date_, 3);
					$current = end_month($date_);
					break;
			}
			$am0 = $am;
		}
		$rows[] = array('date' => $current, 'amount' => $am0);
		$total += $am0;
	}
	$result['ok'] = true;
	$result['periods'] = $rows;
	$result['total'] = round2($total, user_price_dec());
	$result['am'] = $am;
	$result['am0'] = $rows[0]['amount'];
	return $result;
}

/**
 * Plan one accrual schedule and prove its classification before any posting.
 *
 * @param array $input date_, amount, periods, freq, acc_act, res_act, dimension_id, dimension2_id,
 *                     memo_
 * @return array ok, errors, codes, periods, total, am, am0, memo, accounts, classification
 */
function accrual_schedule_plan($input)
{
	$result = array('ok' => false, 'errors' => array(), 'codes' => array(), 'periods' => array(),
		'total' => 0.0, 'am' => 0.0, 'am0' => 0.0, 'memo' => '', 'accounts' => array(),
		'classification' => array());
	$input = is_array($input) ? $input : array();
	$split = accrual_schedule_split($input);
	if (!$split['ok']) {
		$result['codes'] = $split['codes'];
		$result['errors'] = $split['errors'];
		return $result;
	}
	$acc_act = isset($input['acc_act']) ? (string)$input['acc_act'] : '';
	$res_act = isset($input['res_act']) ? (string)$input['res_act'] : '';
	$dimension_id = isset($input['dimension_id']) ? $input['dimension_id'] : 0;
	$dimension2_id = isset($input['dimension2_id']) ? $input['dimension2_id'] : 0;
	$memo = isset($input['memo_']) ? (string)$input['memo_'] : '';
	if ($acc_act === '' || $res_act === '') {
		$result['codes'][] = 'missing_account';
		$result['errors'][] = _('Both the accrued balance account and the revenue/cost account are required.');
		return $result;
	}
	if ($acc_act === $res_act) {
		$result['codes'][] = 'same_account';
		$result['errors'][] = _('The accrued balance account and the revenue/cost account must differ.');
		return $result;
	}
	// DIM-GL-002: the economic schedule line is an explicit source of the event being created.
	$economic = dimension_derived_posting_resolve('schedule_line', $res_act, $dimension_id,
		$dimension2_id, 'accrual_schedule', false);
	if (!$economic['ok']) {
		$result['codes'] = $economic['codes'];
		foreach ($economic['errors'] as $error)
			$result['errors'][] = sprintf(_('Revenue / Cost Account: %s'), $error);
		return $result;
	}
	// The accrued balance is a control leg: admitted only on a non-P&L account, explicitly blank.
	$control = dimension_derived_posting_declaration('non_pnl_control', $acc_act, 0, 0);
	if (!$control['ok']) {
		$result['codes'] = array_merge($result['codes'], $control['codes']);
		foreach ($control['errors'] as $error)
			$result['errors'][] = sprintf(_('Accrued Balance Account: %s'), $error);
		return $result;
	}
	if ($memo === '')
		$memo = sprintf(_('Accruals for %s'), $split['total']);
	$result['ok'] = true;
	$result['periods'] = $split['periods'];
	$result['total'] = $split['total'];
	$result['am'] = $split['am'];
	$result['am0'] = $split['am0'];
	$result['memo'] = $memo;
	$result['accounts'] = array('acc_act' => $acc_act, 'res_act' => $res_act);
	$result['classification'] = array('economic' => $economic, 'control' => $control);
	return $result;
}

/**
 * Post every period of a planned schedule atomically.
 *
 * @param array $plan accrual_schedule_plan() result
 * @return array ok, errors, codes, trans_nos
 */
function accrual_schedule_post($plan)
{
	global $Refs;
	$result = array('ok' => false, 'errors' => array(), 'codes' => array(), 'trans_nos' => array());
	if (!is_array($plan) || empty($plan['ok']) || empty($plan['periods'])
		|| !isset($plan['accounts']['acc_act'], $plan['accounts']['res_act'])) {
		$result['codes'][] = 'invalid_plan';
		$result['errors'][] = _('Only a validated accrual schedule plan can be posted.');
		return $result;
	}
	$acc_act = $plan['accounts']['acc_act'];
	$res_act = $plan['accounts']['res_act'];
	$dimension_id = isset($plan['classification']['economic']['values'][1])
		? $plan['classification']['economic']['values'][1] : 0;
	$dimension2_id = isset($plan['classification']['economic']['values'][2])
		? $plan['classification']['economic']['values'][2] : 0;
	begin_transaction();
	try {
		foreach ($plan['periods'] as $period) {
			$cart = new items_cart(ST_JOURNAL);
			$cart->memo_ = $plan['memo'];
			$cart->reference = $Refs->get_next(ST_JOURNAL, null, $period['date']);
			$cart->tran_date = $cart->doc_date = $cart->event_date = $period['date'];
			$cart->add_gl_item($acc_act, 0, 0, -$period['amount'], $cart->reference);
			$acc_index = count($cart->gl_items) - 1;
			if ($acc_index < 0) {
				$result['codes'][] = 'invalid_schedule_line';
				$result['errors'][] = _('The accrued balance line could not be created.');
				cancel_transaction();
				return $result;
			}
			$cart->gl_items[$acc_index]->derived_role = 'non_pnl_control';
			$cart->add_gl_item($res_act, $dimension_id, $dimension2_id, $period['amount'],
				$cart->reference);
			$res_index = count($cart->gl_items) - 1;
			$cart->gl_items[$res_index]->derived_role = 'schedule_line';
			$trans_no = write_journal_entries($cart);
			if (!$trans_no) {
				$result['codes'][] = 'post_failed';
				$result['errors'][] = _('The accrual schedule could not be posted; no period was saved.');
				cancel_transaction();
				return $result;
			}
			$result['trans_nos'][] = $trans_no;
			$cart->clear_items();
			unset($cart);
		}
		commit_transaction();
		$result['ok'] = true;
		return $result;
	} catch (DimensionFinancialReferenceWriteException $exception) {
		cancel_transaction();
		$result['codes'][] = 'classification_refused';
		$result['errors'][] = sprintf(_('The accrual schedule was cancelled before any period was saved: %s'),
			$exception->getMessage());
		return $result;
	} catch (Exception $exception) {
		cancel_transaction();
		$result['codes'][] = 'post_failed';
		$result['errors'][] = _('The accrual schedule could not be posted; no period was saved.');
		return $result;
	} catch (Throwable $exception) {
		cancel_transaction();
		$result['codes'][] = 'post_failed';
		$result['errors'][] = _('The accrual schedule could not be posted; no period was saved.');
		return $result;
	}
}
