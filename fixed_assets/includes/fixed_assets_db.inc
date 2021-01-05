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

function process_fixed_asset_depreciation($stock_id, $gl_rows, $refline, $memo_) {
	global $Refs;

	$row = get_item($stock_id);
	$old_cost = $row['material_cost'];

	begin_transaction();

	$date_ = Today();
	if (!is_date_in_fiscalyear($date_))
		$date_ = end_fiscalyear();

	$stock_gl_code = get_stock_gl_code($stock_id);

	$cart = new items_cart(ST_JOURNAL);
	$cart->tran_date = $cart->doc_date = $cart->event_date = $date_;
	$cart->tran_date = end_fiscalyear();
	$cart->reference = $Refs->get_next(ST_JOURNAL, $refline, $cart->tran_date, $date_);

	$value_of_change = 0;
	foreach ($gl_rows as $row) {
		$value_of_change += round($row['value'], 2);

		$cart->add_gl_item($stock_gl_code['adjustment_account'], $stock_gl_code['dimension_id'], $stock_gl_code['dimension2_id'], -$row['value'], '', null, null, $row['date']);
		$cart->add_gl_item($stock_gl_code['cogs_account'], 0, 0, $row['value'], '', null, null, $row['date']);
	}

	$new_cost = $old_cost - $value_of_change;

	if (empty($memo_))
		$cart->memo_ = sprintf(_('Fixed asset has been deprecated by the value of %s'), number_format2($value_of_change, user_price_dec()));
	else
		$cart->memo_ = $memo_;

	$trans_no = write_journal_entries($cart);

	$sql = "UPDATE ".TB_PREF."stock_master SET depreciation_date='".date2sql($cart->tran_date)."', material_cost=".db_escape($new_cost)." WHERE stock_id=".db_escape($stock_id);
	db_query($sql, 'The depreciation start date could not be updated');

	commit_transaction();

	return $trans_no;
}

//--------------------------------------------------------------------------------------------------

function get_fixed_asset_move($stock_id, $tran_type) {

	$sql = "SELECT * FROM ".TB_PREF."stock_moves WHERE stock_id=".db_escape($stock_id)." AND type=".db_escape($tran_type);

	$result = db_query($sql, 'cannot retrieve fixed asset move');

	if (db_num_rows($result) == 0)
		return false;

	$row = db_fetch_assoc($result);
	return $row;
}

function get_fixed_asset_disposal($stock_id) {

	$sql = "SELECT * FROM ".TB_PREF."stock_moves WHERE stock_id=".db_escape($stock_id)." AND (type=".ST_INVADJUST." OR type=".ST_CUSTDELIVERY.") ORDER BY tran_date DESC LIMIT 1";

	$result = db_query($sql, 'cannot retrieve fixed asset move');

	if (db_num_rows($result) == 0)
		return false;

	$row = db_fetch_assoc($result);
	return $row;
}

function get_fixed_asset_purchase($stock_id) {
	$sql = "SELECT * FROM ".TB_PREF."stock_master m, ".TB_PREF."supp_invoice_items i, ".TB_PREF."supp_trans t WHERE m.stock_id=".db_escape($stock_id)." AND i.supp_trans_no=t.trans_no AND m.stock_id=i.stock_id ORDER BY t.tran_date DESC LIMIT 1";

	$result = db_query($sql, 'cannot retrieve fixed asset move');

	if (db_num_rows($result) == 0)
		return false;

	$row = db_fetch_assoc($result);
	return $row;
}

//--------------------------------------------------------------------------------------------------

function get_fixed_asset_class($id) {
	$sql = "SELECT * FROM ".TB_PREF."stock_fa_class WHERE fa_class_id=".db_escape($id);

	$result = db_query($sql, 'a fixed asset class could not be retrieved');

	return db_fetch($result);
}

//--------------------------------------------------------------------------------------------------

function get_sql_for_fixed_assets($show_inactive = false) {
	$sql = "SELECT s.stock_id, c.description, s.units, s.description as name, s.depreciation_rate, s.depreciation_method, s.inactive, s.material_cost, s.purchase_cost, s.depreciation_factor FROM ".TB_PREF."stock_master s LEFT JOIN ".TB_PREF."stock_fa_class c ON s.fa_class_id=c.fa_class_id WHERE s.mb_flag='F'";

	if (!$show_inactive)
		$sql .= " AND !s.inactive ";

	$sql .= " ORDER BY c.description ";

	return $sql;
}

//--------------------------------------------------------------------------------------------------

//function get_sql_for_fa_journal_inquiry($stock_id)
//{
//
//	$sql = "SELECT	IF(ISNULL(a.gl_seq),0,a.gl_seq) as gl_seq,
//		gl.tran_date,
//		gl.type as trans_type,
//		gl.type_no as trans_no,
//		IFNULL(max(supp.supp_name), max(cust.name)) as name,
//		refs.reference,
//		SUM(IF(gl.amount>0, gl.amount,0)) as amount,
//		com.memo_,
//		IF(ISNULL(u.user_id),'',u.user_id) as user_id";
//
//	$sql.= " FROM ".TB_PREF."gl_trans as gl
//		 LEFT JOIN ".TB_PREF."audit_trail as a ON
//			(gl.type=a.type AND gl.type_no=a.trans_no)
//		 LEFT JOIN ".TB_PREF."comments as com ON
//			(gl.type=com.type AND gl.type_no=com.id)
//		 LEFT JOIN ".TB_PREF."refs as refs ON
//			(gl.type=refs.type AND gl.type_no=refs.id)
//		 LEFT JOIN ".TB_PREF."users as u ON
//			a.user=u.id
//	 	 LEFT JOIN ".TB_PREF."debtor_trans dt ON dt.type=gl.type AND gl.type_no=dt.trans_no
//	 	 LEFT JOIN ".TB_PREF."debtors_master cust ON gl.person_type_id=2 AND gl.person_id=cust.debtor_no
//	 	 LEFT JOIN ".TB_PREF."supp_trans st ON st.type=gl.type AND gl.type_no=st.trans_no
//	 	 LEFT JOIN ".TB_PREF."suppliers supp ON gl.person_type_id=3 AND gl.person_id=supp.supplier_id
//		 WHERE gl.amount!=0
//     AND NOT ISNULL(a.gl_seq)";
//
////	if (!$alsoclosed) {
////		$sql .= " AND gl_seq=0";
////	}
//// 	else
//// 		$sql .= " AND NOT ISNULL(a.gl_seq)";
//
//	$sql .= " GROUP BY tran_date, gl_seq, trans_type, trans_no";
//
//	return $sql;
//}

//--------------------------------------------------------------------------------------------------
