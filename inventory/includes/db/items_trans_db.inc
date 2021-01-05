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
//-------------------------------------------------------------------------------------------------------------

function stock_cost_update($stock_id, $material_cost, $labour_cost, $overhead_cost, $last_cost, $refline, $memo_) {
	$mb_flag = get_mb_flag($stock_id);

	$update_no = -1;

	if (is_service($mb_flag)) {
		$sql = "UPDATE ".TB_PREF."stock_master SET material_cost=".db_escape($material_cost)."
		WHERE stock_id=".db_escape($stock_id);

		db_query($sql, 'The cost details for the inventory item could not be updated');

		return $update_no;
	}

	begin_transaction();

	$sql = "UPDATE ".TB_PREF."stock_master SET material_cost=".db_escape($material_cost).", 
		labour_cost=".db_escape($labour_cost).",
		overhead_cost=".db_escape($overhead_cost)."
		WHERE stock_id=".db_escape($stock_id);
	db_query($sql, 'The cost details for the inventory item could not be updated');

	$qoh = get_qoh_on_date($stock_id);

	$date_ = Today();
	if (!is_date_in_fiscalyear($date_))
		$date_ = end_fiscalyear();

	if ($qoh > 0) {
		$new_cost = $material_cost + $labour_cost + $overhead_cost;

		$value_of_change = round2($qoh * ($new_cost - $last_cost), user_price_dec());

		if ($value_of_change != 0) {
			global $Refs;
			$stock_gl_code = get_stock_gl_code($stock_id);

			$cart = new items_cart(ST_COSTUPDATE);
			$cart->tran_date = $cart->doc_date = $cart->event_date = $date_;
			if (!is_date_in_fiscalyear($cart->tran_date))
				$cart->tran_date = end_fiscalyear();
			$cart->reference = $Refs->get_next(ST_COSTUPDATE, $refline, $cart->tran_date, $date_);

			if (empty($memo_))
				$cart->memo_ = sprintf(_('Cost was %s changed to %s x quantity on hand of %s'),
					number_format2($last_cost, 2), number_format2($new_cost), $qoh);
			else
				$cart->memo_ = $memo_;

			$cart->add_gl_item($stock_gl_code['adjustment_account'],
				$stock_gl_code['dimension_id'], $stock_gl_code['dimension2_id'], -$value_of_change);
			$cart->add_gl_item($stock_gl_code['inventory_account'], 0, 0, $value_of_change);

			write_journal_entries($cart);
		}
	}

	if ($update_no != -1)
		add_audit_trail(ST_COSTUPDATE, $update_no, $date_);
	commit_transaction();

	return $update_no;
}