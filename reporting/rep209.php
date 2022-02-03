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

$page_security = $_POST['PARAM_0'] == $_POST['PARAM_1'] ? 'SA_SUPPTRANSVIEW' : 'SA_SUPPBULKREP';
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/db/crm_contacts_db.inc');
include_once($path_to_root . '/taxes/tax_calc.inc');

//----------------------------------------------------------------------------------------------------

print_po();

//----------------------------------------------------------------------------------------------------

function get_supp_po($order_no) {
	$sql = "SELECT po.*, supplier.supp_name, supplier.supp_account_no,supplier.tax_included,
		supplier.gst_no AS tax_id,
		supplier.curr_code, supplier.payment_terms, loc.location_name,
		supplier.address, supplier.contact, supplier.tax_group_id
		FROM ".TB_PREF."purch_orders po,"
			.TB_PREF."suppliers supplier,"
			.TB_PREF."locations loc
		WHERE po.supplier_id = supplier.supplier_id
		AND loc.loc_code = into_stock_location
		AND po.order_no = ".db_escape($order_no);
	$result = db_query($sql, 'The order cannot be retrieved');
	return db_fetch($result);
}

function get_po_details($order_no) {
	$sql = "SELECT poline.*, units
		FROM ".TB_PREF."purch_order_details poline
			LEFT JOIN ".TB_PREF."stock_master item ON poline.item_code=item.stock_id
		WHERE order_no =".db_escape($order_no)." ";
	$sql .= " ORDER BY po_detail_item";
	return db_query($sql, 'Retreive order Line Items');
}

function print_po() {
	global $path_to_root, $SysPrefs;

	include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$currency = $_POST['PARAM_2'];
	$email = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];

	if (!$from || !$to) return;

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	$cols = array(4, 60, 225, 300, 340, 385, 450, 515);

	// $headers in doctext.inc
	$aligns = array('left',	'left',	'left', 'right', 'left', 'right', 'right');

	$params = array('comments' => $comments);

	$cur = get_company_Pref('curr_default');

	if ($email == 0)
		$rep = new FrontReport(_('PURCHASE ORDER'), 'PurchaseOrderBulk', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	for ($i = $from; $i <= $to; $i++) {
		$myrow = get_supp_po($i);
		if ($currency != ALL_TEXT && $myrow['curr_code'] != $currency) {
			continue;
		}
		$baccount = get_default_bank_account($myrow['curr_code']);
		$params['bankaccount'] = $baccount['id'];

		if ($email == 1) {
			$rep = new FrontReport('', '', user_pagesize(), 9, $orientation);
			$rep->title = _('PURCHASE ORDER');
			$rep->filename = 'PurchaseOrder' . $i . '.pdf';
		}	
		$rep->currency = $cur;
		$rep->Font();
		$rep->Info($params, $cols, null, $aligns);

		$contacts = get_supplier_contacts($myrow['supplier_id'], 'order');
		$rep->SetCommonData($myrow, null, $myrow, $baccount, ST_PURCHORDER, $contacts);
		$rep->SetHeaderType('Header2');
		$rep->NewPage();

		$result = get_po_details($i);
		$SubTotal = 0;
		$items = $prices = array();
		while ($myrow2=db_fetch($result)) {
			$data = get_purchase_data($myrow['supplier_id'], $myrow2['item_code']);
			if ($data !== false) {
				if ($data['supplier_description'] != '')
					$myrow2['description'] = $data['supplier_description'];
				if ($data['suppliers_uom'] != '')
					$myrow2['units'] = $data['suppliers_uom'];
				if ($data['conversion_factor'] != 1) {
					$myrow2['unit_price'] = round2($myrow2['unit_price'] * $data['conversion_factor'], user_price_dec());
					$myrow2['quantity_ordered'] = round2($myrow2['quantity_ordered'] / $data['conversion_factor'], user_qty_dec());
				}
			}
			$Net = round2(($myrow2['unit_price'] * $myrow2['quantity_ordered']), user_price_dec());
			$prices[] = $Net;
			$items[] = $myrow2['item_code'];
			$SubTotal += $Net;
			$dec2 = 0;
			$DisplayPrice = price_decimal_format($myrow2['unit_price'],$dec2);
			$DisplayQty = number_format2($myrow2['quantity_ordered'],get_qty_dec($myrow2['item_code']));
			$DisplayNet = number_format2($Net,$dec);
			if ($SysPrefs->show_po_item_codes()) {
				$rep->TextCol(0, 1,	$myrow2['item_code'], -2);
				$rep->TextCol(1, 2,	$myrow2['description'], -2);
			}
			else
				$rep->TextCol(0, 2,	$myrow2['description'], -2);
			$rep->TextCol(2, 3,	sql2date($myrow2['delivery_date']), -2);
			$rep->TextCol(3, 4,	$DisplayQty, -2);
			$rep->TextCol(4, 5,	$myrow2['units'], -2);
			$rep->TextCol(5, 6,	$DisplayPrice, -2);
			$rep->TextCol(6, 7,	$DisplayNet, -2);
			$rep->NewLine(1);
			if ($rep->row < $rep->bottomMargin + (15 * $rep->lineHeight))
				$rep->NewPage();
		}
		if ($myrow['comments'] != '') {
			$rep->NewLine();
			$rep->TextColLines(1, 4, $myrow['comments'], -2);
		}
		$DisplaySubTot = number_format2($SubTotal,$dec);

		$rep->row = $rep->bottomMargin + (15 * $rep->lineHeight);
		$doctype = ST_PURCHORDER;

		$rep->TextCol(3, 6, _('Sub-total'), -2);
		$rep->TextCol(6, 7,	$DisplaySubTot, -2);
		$rep->NewLine();

		$tax_items = get_tax_for_items($items, $prices, 0,
		  $myrow['tax_group_id'], $myrow['tax_included'],  null, TCA_LINES);
		$first = true;
		foreach($tax_items as $tax_item) {
			if ($tax_item['Value'] == 0)
				continue;
			$DisplayTax = number_format2($tax_item['Value'], $dec);

			$tax_type_name = $tax_item['tax_type_name'];

			if ($myrow['tax_included']) {
				if ($SysPrefs->alternative_tax_include_on_docs() == 1) {
					if ($first) {
						$rep->TextCol(3, 6, _('Total Tax Excluded'), -2);
						$rep->TextCol(6, 7,	number_format2($tax_item['net_amount'], $dec), -2);
						$rep->NewLine();
					}
					$rep->TextCol(3, 6, $tax_type_name, -2);
					$rep->TextCol(6, 7,	$DisplayTax, -2);
					$first = false;
				}
				else
					$rep->TextCol(3, 7, _('Included') . ' ' . $tax_type_name . _('Amount') . ': ' . $DisplayTax, -2);
			}
			else {
				$SubTotal += $tax_item['Value'];
				$rep->TextCol(3, 6, $tax_type_name, -2);
				$rep->TextCol(6, 7,	$DisplayTax, -2);
			}
			$rep->NewLine();
		}

		$rep->NewLine();
		$DisplayTotal = number_format2($SubTotal, $dec);
		$rep->Font('bold');
		$rep->TextCol(3, 6, _('TOTAL PO'), - 2);
		$rep->TextCol(6, 7,	$DisplayTotal, -2);
		$words = price_in_words($SubTotal, ST_PURCHORDER);
		if ($words != '') {
			$rep->NewLine(1);
			$rep->TextCol(1, 7, $myrow['curr_code'] . ': ' . $words, - 2);
		}
		$rep->Font();
		if ($email == 1) {
			$myrow['DebtorName'] = $myrow['supp_name'];

			if ($myrow['reference'] == '')
				$myrow['reference'] = $myrow['order_no'];
			$rep->End($email);
		}
	}
	if ($email == 0)
		$rep->End();
}
