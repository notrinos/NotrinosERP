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

include_once($path_to_root . '/purchasing/includes/db/suppliers_entity.inc');

function add_supplier($supp_name, $supp_ref, $address, $supp_address, $gst_no, $website, $supp_account_no, $bank_account, $credit_limit, $dimension_id, $dimension2_id, $curr_code, $payment_terms, $payable_account, $purchase_account, $payment_discount_account, $notes, $tax_group_id, $tax_included, $vendor_tier = 'standard', $vendor_category = '', $evaluation_frequency_months = 6, $certifications = '', $approved_categories = '') {
	return suppliers_entity::create(array(
		'supp_name' => $supp_name,
		'supp_ref' => $supp_ref,
		'address' => $address,
		'supp_address' => $supp_address,
		'gst_no' => $gst_no,
		'website' => $website,
		'supp_account_no' => $supp_account_no,
		'bank_account' => $bank_account,
		'credit_limit' => $credit_limit,
		'dimension_id' => $dimension_id,
		'dimension2_id' => $dimension2_id,
		'curr_code' => $curr_code,
		'payment_terms' => $payment_terms,
		'payable_account' => $payable_account,
		'purchase_account' => $purchase_account,
		'payment_discount_account' => $payment_discount_account,
		'notes' => $notes,
		'tax_group_id' => $tax_group_id,
		'tax_included' => $tax_included ? 1 : 0,
		'vendor_tier' => $vendor_tier,
		'vendor_category' => $vendor_category,
		'evaluation_frequency_months' => (int)$evaluation_frequency_months,
		'certifications' => $certifications,
		'approved_categories' => $approved_categories,
	));
}

function update_supplier($supplier_id, $supp_name, $supp_ref, $address, $supp_address, $gst_no, $website, $supp_account_no, $bank_account, $credit_limit, $dimension_id, $dimension2_id, $curr_code, $payment_terms, $payable_account, $purchase_account, $payment_discount_account, $notes, $tax_group_id, $tax_included, $vendor_tier = 'standard', $vendor_category = '', $evaluation_frequency_months = 6, $certifications = '', $approved_categories = '') {
	return suppliers_entity::modify((int)$supplier_id, array(
		'supp_name' => $supp_name,
		'supp_ref' => $supp_ref,
		'address' => $address,
		'supp_address' => $supp_address,
		'gst_no' => $gst_no,
		'website' => $website,
		'supp_account_no' => $supp_account_no,
		'bank_account' => $bank_account,
		'credit_limit' => $credit_limit,
		'dimension_id' => $dimension_id,
		'dimension2_id' => $dimension2_id,
		'curr_code' => $curr_code,
		'payment_terms' => $payment_terms,
		'payable_account' => $payable_account,
		'purchase_account' => $purchase_account,
		'payment_discount_account' => $payment_discount_account,
		'notes' => $notes,
		'tax_group_id' => $tax_group_id,
		'tax_included' => $tax_included ? 1 : 0,
		'vendor_tier' => $vendor_tier,
		'vendor_category' => $vendor_category,
		'evaluation_frequency_months' => (int)$evaluation_frequency_months,
		'certifications' => $certifications,
		'approved_categories' => $approved_categories,
	));
}

function delete_supplier($supplier_id) {
	return suppliers_entity::remove((int)$supplier_id);
}

function get_supplier_details($supplier_id, $to=null, $all=true) {

	if ($to == null)
		$todate = date('Y-m-d');
	else
		$todate = date2sql($to);
	$past1 = get_company_pref('past_due_days');
	$past2 = 2 * $past1;
	// removed - supp_trans.alloc from all summations

	if ($all)
		$value = "(trans.ov_amount + trans.ov_gst + trans.ov_discount)";
	else	
		$value = "IF (trans.type=".ST_SUPPINVOICE." OR trans.type=".ST_BANKDEPOSIT." OR (trans.type=".ST_JOURNAL." AND (trans.ov_amount + trans.ov_gst + trans.ov_discount)>0), 
			(trans.ov_amount + trans.ov_gst + trans.ov_discount - trans.alloc),
			(trans.ov_amount + trans.ov_gst + trans.ov_discount + trans.alloc))";

	$due = "IF (trans.type=".ST_SUPPINVOICE." OR trans.type=".ST_SUPPCREDIT.",trans.due_date,trans.tran_date)";
	$sql = "SELECT supp.supp_name, supp.curr_code, ".TB_PREF."payment_terms.terms,

		Sum(IFNULL($value,0)) AS Balance,

		Sum(IF ((TO_DAYS('$todate') - TO_DAYS($due)) > 0,$value,0)) AS Due,
		Sum(IF ((TO_DAYS('$todate') - TO_DAYS($due)) > $past1,$value,0)) AS Overdue1,
		Sum(IF ((TO_DAYS('$todate') - TO_DAYS($due)) > $past2,$value,0)) AS Overdue2,
		supp.credit_limit - Sum(IFNULL(IF(trans.type=".ST_SUPPCREDIT.", -1, 1) 
			* (ov_amount + ov_gst + ov_discount),0)) as cur_credit,
		supp.tax_group_id

		FROM ".TB_PREF."suppliers supp
			 LEFT JOIN ".TB_PREF."supp_trans trans ON supp.supplier_id = trans.supplier_id AND trans.tran_date <= '$todate',
			 ".TB_PREF."payment_terms

		WHERE
			 supp.payment_terms = ".TB_PREF."payment_terms.terms_indicator
			 AND supp.supplier_id = $supplier_id ";
	if (!$all)
		$sql .= "AND $value <> 0 "; 
	$sql .= "GROUP BY
			supp.supp_name,
			".TB_PREF."payment_terms.terms,
			".TB_PREF."payment_terms.days_before_due,
			".TB_PREF."payment_terms.day_in_following_month";
	$result = db_query($sql, 'The customer details could not be retrieved');
	$supp = db_fetch($result);

	return $supp;
}

function get_supplier($supplier_id) {
	return suppliers_entity::find((int)$supplier_id);
}

function get_supplier_name($supplier_id) {
	return suppliers_entity::name((int)$supplier_id);
}

function get_supplier_accounts($supplier_id) {
	$sql = "SELECT payable_account,purchase_account,payment_discount_account, dimension_id, dimension2_id FROM ".TB_PREF."suppliers WHERE supplier_id=".db_escape($supplier_id);

	$result = db_query($sql, 'could not get supplier accounts');

	return db_fetch($result);
}

function get_supplier_contacts($supplier_id, $action=null) {
	$results = array();
	$res = get_crm_persons('supplier', $action, $supplier_id);
	while($contact = db_fetch($res))
		$results[] = $contact;

	return $results;
}

function get_current_supp_credit($supplier_id) {
	$suppdet = get_supplier_details($supplier_id);
	return $suppdet['cur_credit'];
}

function is_new_supplier($id) {
	$tables = array('supp_trans', 'grn_batch', 'purch_orders', 'purch_data');

	return !key_in_foreign_table($id, $tables, 'supplier_id');
}

function get_supplier_currency($supplier_id) {
	$sql = "SELECT curr_code FROM ".TB_PREF."suppliers WHERE supplier_id = ".db_escape($supplier_id);

	$result = db_query($sql, "Retreive currency of supplier $supplier_id");

	$myrow=db_fetch_row($result);
	return $myrow ? $myrow[0] : get_company_currency();
}

function get_suppliers_search($supplier) {
	global $SysPrefs;

	if (isset($SysPrefs->max_rows_in_search))
		$limit = $SysPrefs->max_rows_in_search;
	else
		$limit = 10;

	$sql = "SELECT s.supplier_id, s.supp_name, s.supp_ref, s.address, s.gst_no, p.phone, p.phone2
		FROM ".TB_PREF."suppliers s 
		LEFT JOIN ".TB_PREF."crm_contacts ct ON ct.type = 'supplier' AND ct.action = 'general' AND ct.entity_id = s.supplier_id 
		LEFT JOIN ".TB_PREF."crm_persons p ON ct.person_id = p.id 
		WHERE (s.supp_name LIKE " . db_escape("%" . $supplier. "%") . " OR 
			s.supp_ref LIKE " . db_escape("%" . $supplier. "%") . " OR 
			s.address LIKE " . db_escape("%" . $supplier. "%") . " OR 
			gst_no LIKE " . db_escape("%" . $supplier. "%") . " OR
			p.phone LIKE " . db_escape("%" . $supplier. "%") . " OR 
			p.phone2 LIKE " . db_escape("%" . $supplier. "%") . ")
		GROUP BY s.supplier_id ORDER BY supp_name LIMIT 0,".(int)($limit);

	return db_query($sql, 'Failed in retreiving supplier list.');
}
