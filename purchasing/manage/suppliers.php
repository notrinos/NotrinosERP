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
$page_security = 'SA_SUPPLIER';
$path_to_root = '../..';
include($path_to_root.'/includes/db_pager.inc');
include_once($path_to_root.'/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Suppliers'), @$_REQUEST['popup'], false, '', $js);

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/ui/contacts_view.inc');
include_once($path_to_root.'/includes/ui/attachment.inc');
include_once($path_to_root.'/purchasing/includes/db/vendor_evaluation_db.inc');

check_db_has_tax_groups(_('There are no tax groups defined in the system. At least one tax group is required before proceeding.'));

if (isset($_GET['supplier_id'])) 
	$_POST['supplier_id'] = $_GET['supplier_id'];

$supplier_id = get_post('supplier_id', ''); 

function can_process() {
	/* actions to take once the user has clicked the submit button
	ie the page has called itself with some user input */

	//first off validate inputs sensible

	if (strlen($_POST['supp_name']) == 0 || $_POST['supp_name'] == '') {
		display_error(_('The supplier name must be entered.'));
		set_focus('supp_name');
		return false;
	}
	if (strlen($_POST['supp_ref']) == 0 || $_POST['supp_ref'] == '') {
		display_error(_('The supplier short name must be entered.'));
		set_focus('supp_ref');
		return false;
	}
	if (empty($_POST['supplier_id']) && key_in_foreign_table($_POST['supp_ref'], 'suppliers', 'supp_ref')) {
		display_error( _('Duplicated supplier short name found.'));
		set_focus('supp_ref');
		return false;
	}
	if (!check_num('evaluation_frequency_months', 1)) {
		display_error(_('Evaluation frequency must be at least one month.'));
		set_focus('evaluation_frequency_months');
		return false;
	}
	return true;
}

/**
 * Get the vendor tier selector options.
 *
 * @return array
 */
function supplier_vendor_tier_options() {
	return get_vendor_tiers();
}

/**
 * Render the vendor score summary block for one supplier.
 *
 * @param int $supplier_id
 * @return string
 */
function render_supplier_vendor_score_summary($supplier_id) {
	global $path_to_root;

	$supplier = get_supplier($supplier_id);
	if (!$supplier)
		return '';

	$score_items = array(
		array('label' => _('Overall'), 'value' => isset($supplier['overall_score']) ? round2($supplier['overall_score'], 2) : 0),
		array('label' => _('Quality'), 'value' => isset($supplier['quality_score']) ? round2($supplier['quality_score'], 2) : 0),
		array('label' => _('Delivery'), 'value' => isset($supplier['delivery_score']) ? round2($supplier['delivery_score'], 2) : 0),
		array('label' => _('Price'), 'value' => isset($supplier['price_score']) ? round2($supplier['price_score'], 2) : 0),
		array('label' => _('Service'), 'value' => isset($supplier['service_score']) ? round2($supplier['service_score'], 2) : 0),
		array('label' => _('On-Time %'), 'value' => isset($supplier['on_time_delivery_pct']) ? round2($supplier['on_time_delivery_pct'], 2) : 0),
		array('label' => _('Defect %'), 'value' => isset($supplier['defect_rate_pct']) ? round2($supplier['defect_rate_pct'], 2) : 0),
		array('label' => _('Lead Time'), 'value' => isset($supplier['lead_time_average']) ? (int)$supplier['lead_time_average'] : 0),
	);

	$html = '<div style="padding:12px;border:1px solid #d9dee3;background:#f8fafc;border-radius:4px;">';
	$html .= '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;">';
	$html .= '<div><strong>' . _('Current Tier:') . '</strong> ' . vendor_tier_badge(isset($supplier['vendor_tier']) ? $supplier['vendor_tier'] : 'standard') . '</div>';
	if (user_check_access('SA_VENDOREVALUATION'))
		$html .= '<div><a href="' . $path_to_root . '/purchasing/inquiry/vendor_scorecard.php?sel_app=AP&supplier_id=' . (int)$supplier_id . '">' . _('View Scorecard') . '</a></div>';
	$html .= '</div>';
	$html .= '<div style="display:flex;gap:10px;flex-wrap:wrap;margin-top:10px;">';
	foreach ($score_items as $score_item) {
		$html .= '<div style="min-width:105px;padding:8px 10px;background:#fff;border:1px solid #e2e8f0;border-radius:4px;">';
		$html .= '<div style="font-size:11px;color:#64748b;">' . $score_item['label'] . '</div>';
		$html .= '<div style="font-size:16px;font-weight:bold;color:#0f172a;">' . $score_item['value'] . '</div>';
		$html .= '</div>';
	}
	$html .= '</div>';
	$html .= '<div style="margin-top:10px;font-size:12px;color:#475569;">';
	$html .= '<strong>' . _('Last Evaluation:') . '</strong> ' . (!empty($supplier['last_evaluation_date']) ? sql2date($supplier['last_evaluation_date']) : '-') . '&nbsp;&nbsp;';
	$html .= '<strong>' . _('Next Evaluation:') . '</strong> ' . (!empty($supplier['next_evaluation_date']) ? sql2date($supplier['next_evaluation_date']) : '-') . '&nbsp;&nbsp;';
	$html .= '<strong>' . _('Vendor Category:') . '</strong> ' . (!empty($supplier['vendor_category']) ? htmlspecialchars($supplier['vendor_category']) : '-') . '</div>';
	$html .= '</div>';

	return $html;
}

function handle_submit(&$supplier_id) {
	global $path_to_root, $Ajax, $SysPrefs;
	
	if (!can_process())
		return;
	begin_transaction();
	if ($supplier_id)  {
		update_supplier($_POST['supplier_id'], $_POST['supp_name'], $_POST['supp_ref'], $_POST['address'], $_POST['supp_address'], $_POST['gst_no'], $_POST['website'], $_POST['supp_account_no'], $_POST['bank_account'], input_num('credit_limit', 0), $_POST['dimension_id'], $_POST['dimension2_id'], $_POST['curr_code'], $_POST['payment_terms'], $_POST['payable_account'], $_POST['purchase_account'], $_POST['payment_discount_account'], $_POST['notes'], $_POST['tax_group_id'], check_value('tax_included'), get_post('vendor_tier', 'standard'), get_post('vendor_category', ''), input_num('evaluation_frequency_months', 6), get_post('certifications', ''), get_post('approved_categories', ''));
		update_record_status($_POST['supplier_id'], $_POST['inactive'], 'suppliers', 'supplier_id');

		$Ajax->activate('supplier_id'); // in case of status change
		display_notification(_('Supplier has been updated.'));
	} 
	else {
		add_supplier($_POST['supp_name'], $_POST['supp_ref'], $_POST['address'], $_POST['supp_address'], $_POST['gst_no'], $_POST['website'], $_POST['supp_account_no'], $_POST['bank_account'], input_num('credit_limit',0), $_POST['dimension_id'], $_POST['dimension2_id'], $_POST['curr_code'], $_POST['payment_terms'], $_POST['payable_account'], $_POST['purchase_account'], $_POST['payment_discount_account'], $_POST['notes'], $_POST['tax_group_id'], check_value('tax_included'), get_post('vendor_tier', 'standard'), get_post('vendor_category', ''), input_num('evaluation_frequency_months', 6), get_post('certifications', ''), get_post('approved_categories', ''));

		$supplier_id = $_POST['supplier_id'] = db_insert_id();

		add_crm_person($_POST['supp_ref'], $_POST['contact'], '', $_POST['address'], $_POST['phone'], $_POST['phone2'], $_POST['fax'], $_POST['email'], $_POST['rep_lang'], '');

		add_crm_contact('supplier', 'general', $supplier_id, db_insert_id());

		display_notification(_('A new supplier has been added.'));
		$Ajax->activate('_page_body');
	}
	commit_transaction();
}

if (isset($_POST['submit'])) 
	handle_submit($supplier_id);

if (isset($_POST['delete']) && $_POST['delete'] != '') {
	//the link to delete a selected record was clicked instead of the submit button

	$cancel_delete = 0;

	// PREVENT DELETES IF DEPENDENT RECORDS IN 'supp_trans' , purch_orders

	if (key_in_foreign_table($_POST['supplier_id'], 'supp_trans', 'supplier_id')) {
		$cancel_delete = 1;
		display_error(_('Cannot delete this supplier because there are transactions that refer to this supplier.'));
	} 
	else {
		if (key_in_foreign_table($_POST['supplier_id'], 'purch_orders', 'supplier_id')) {
			$cancel_delete = 1;
			display_error(_('Cannot delete the supplier record because purchase orders have been created against this supplier.'));
		}

	}
	if ($cancel_delete == 0) {
		delete_supplier($_POST['supplier_id']);

		unset($_SESSION['supplier_id']);
		$supplier_id = '';
		$Ajax->activate('_page_body');
		display_notification('#'.$_POST['supplier_id'].' '._('Supplier has been deleted.'));
	} //end if Delete supplier
}

//--------------------------------------------------------------------------------------------

function supplier_settings(&$supplier_id) {
	global $page_nested;
	
	start_outer_table(TABLESTYLE2);

	table_section(1);

	if ($supplier_id) {
		//SupplierID exists - either passed when calling the form or from the form itself
		$myrow = get_supplier($_POST['supplier_id']);

		$_POST['supp_name'] = $myrow['supp_name'];
		$_POST['supp_ref'] = $myrow['supp_ref'];
		$_POST['address']  = $myrow['address'];
		$_POST['supp_address']  = $myrow['supp_address'];

		$_POST['gst_no']  = $myrow['gst_no'];
		$_POST['website']  = $myrow['website'];
		$_POST['supp_account_no']  = $myrow['supp_account_no'];
		$_POST['bank_account']  = $myrow['bank_account'];
		$_POST['dimension_id']  = $myrow['dimension_id'];
		$_POST['dimension2_id']  = $myrow['dimension2_id'];
		$_POST['curr_code']  = $myrow['curr_code'];
		$_POST['payment_terms']  = $myrow['payment_terms'];
		$_POST['credit_limit']  = price_format($myrow['credit_limit']);
		$_POST['tax_group_id'] = $myrow['tax_group_id'];
		$_POST['tax_included'] = $myrow['tax_included'];
		$_POST['payable_account']  = $myrow['payable_account'];
		$_POST['purchase_account']  = $myrow['purchase_account'];
		$_POST['payment_discount_account'] = $myrow['payment_discount_account'];
		$_POST['notes']  = $myrow['notes'];
		$_POST['inactive'] = $myrow['inactive'];
		$_POST['vendor_tier'] = isset($myrow['vendor_tier']) ? $myrow['vendor_tier'] : 'standard';
		$_POST['vendor_category'] = isset($myrow['vendor_category']) ? $myrow['vendor_category'] : '';
		$_POST['evaluation_frequency_months'] = isset($myrow['evaluation_frequency_months']) ? $myrow['evaluation_frequency_months'] : 6;
		$_POST['certifications'] = isset($myrow['certifications']) ? $myrow['certifications'] : '';
		$_POST['approved_categories'] = isset($myrow['approved_categories']) ? $myrow['approved_categories'] : '';
		$_POST['overall_score'] = isset($myrow['overall_score']) ? $myrow['overall_score'] : 0;
		$_POST['quality_score'] = isset($myrow['quality_score']) ? $myrow['quality_score'] : 0;
		$_POST['delivery_score'] = isset($myrow['delivery_score']) ? $myrow['delivery_score'] : 0;
		$_POST['price_score'] = isset($myrow['price_score']) ? $myrow['price_score'] : 0;
		$_POST['service_score'] = isset($myrow['service_score']) ? $myrow['service_score'] : 0;
	} 
	else {
		if (list_updated('supplier_id') || !isset($_POST['supp_name'])) {
			$_POST['supp_name'] = '';
			$_POST['supp_ref'] = '';
			$_POST['address'] = '';
			$_POST['supp_address'] = '';
			$_POST['tax_group_id'] = '';
			$_POST['website'] = '';
			$_POST['supp_account_no'] = '';
			$_POST['notes'] = '';
			$_POST['dimension_id'] = 0;
			$_POST['dimension2_id'] = 0;
			$_POST['tax_included'] = 0;
			$_POST['sales_type'] = -1;
			$_POST['gst_no'] = $_POST['bank_account'] = '';
			$_POST['payment_terms']  = '';
			$_POST['credit_limit'] = price_format(0);

			$company_record = get_company_prefs();
			$_POST['curr_code']  = $company_record['curr_default'];
			$_POST['payable_account'] = $company_record['creditors_act'];
			$_POST['purchase_account'] = ''; // default/item's cogs account
			$_POST['payment_discount_account'] = $company_record['pyt_discount_act'];
			$_POST['vendor_tier'] = 'standard';
			$_POST['vendor_category'] = '';
			$_POST['evaluation_frequency_months'] = 6;
			$_POST['certifications'] = '';
			$_POST['approved_categories'] = '';
		}
	}

	table_section_title(_('Basic Data'));

	text_row(_('Supplier Name:'), 'supp_name', null, 42, 60);
	text_row(_('Supplier Short Name:'), 'supp_ref', null, 30, 30);

	text_row(_('GSTNo:'), 'gst_no', null, 42, 40);
	link_row(_('Website:'), 'website', null, 35, 55);
	if ($supplier_id && !is_new_supplier($supplier_id) && (key_in_foreign_table($_POST['supplier_id'], 'supp_trans', 'supplier_id') || key_in_foreign_table($_POST['supplier_id'], 'purch_orders', 'supplier_id'))) {
		label_row(_("Supplier's Currency:"), $_POST['curr_code']);
		hidden('curr_code', $_POST['curr_code']);
	} 
	else 
		currencies_list_row(_("Supplier's Currency:"), 'curr_code', null);
	
	tax_groups_list_row(_('Tax Group:'), 'tax_group_id', null);
	text_row(_('Our Customer No:'), 'supp_account_no', null, 42, 40);

	table_section_title(_('Purchasing'));
	text_row(_('Bank Name/Account:'), 'bank_account', null, 42, 40);
	amount_row(_('Credit Limit:'), 'credit_limit', null);
	payment_terms_list_row(_('Payment Terms:'), 'payment_terms', null);

	table_section_title(_('Vendor Management'));
	echo "<tr><td class='label'>" . _('Vendor Tier:') . "</td><td>";
	echo array_selector('vendor_tier', get_post('vendor_tier', 'standard'), supplier_vendor_tier_options(), array('class' => array('nosearch')));
	echo "</td></tr>\n";
	text_row(_('Vendor Category:'), 'vendor_category', null, 32, 50);
	qty_row(_('Evaluation Frequency (months):'), 'evaluation_frequency_months', get_post('evaluation_frequency_months', 6), 6);
	textarea_row(_('Certifications:'), 'certifications', null, 35, 2);
	textarea_row(_('Approved Categories:'), 'approved_categories', null, 35, 2);
	if ($supplier_id)
		echo '<tr><td colspan="2">' . render_supplier_vendor_score_summary($supplier_id) . '</td></tr>';
	//
	// tax_included option from supplier record is used directly in update_average_cost() function,
	// therefore we can't edit the option after any transaction was done for the supplier.
	//
	if (is_new_supplier($supplier_id))
		check_row(_('Prices contain tax included:'), 'tax_included');
	else {
		hidden('tax_included');
		label_row(_('Prices contain tax included:'), $_POST['tax_included'] ? _('Yes') : _('No'));
	}

	if (!$supplier_id) table_section(2);

	table_section_title(_('Accounts'));
	gl_all_accounts_list_row(_('Accounts Payable Account:'), 'payable_account', $_POST['payable_account']);
	gl_all_accounts_list_row(_('Purchase Account:'), 'purchase_account', $_POST['purchase_account'],
		false, false, _('Use Item Inventory/COGS Account'));
	gl_all_accounts_list_row(_('Purchase Discount Account:'), 'payment_discount_account', $_POST['payment_discount_account']);
	if (!$supplier_id) {
		table_section_title(_('Contact Data'));
		text_row(_('Contact Person:'), 'contact', null, 42, 40);
		text_row(_('Phone Number:'), 'phone', null, 32, 30);
		text_row(_('Secondary Phone Number:'), 'phone2', null, 32, 30);
		table_section_title(_('Contact Data'));
		text_row(_('Fax Number:'), 'fax', null, 32, 30);
		email_row(_('E-mail:'), 'email', null, 35, 55);
		languages_list_row(_('Document Language:'), 'rep_lang', null, _('System default'));
	}
	else
		table_section(2);
	$dim = get_company_pref('use_dimension');
	if ($dim >= 1) {
		table_section_title(_('Dimension'));
		dimensions_list_row(_('Dimension').' 1:', 'dimension_id', null, true, ' ', false, 1);
		if ($dim > 1)
			dimensions_list_row(_('Dimension').' 2:', 'dimension2_id', null, true, ' ', false, 2);
	}
	if ($dim < 1)
		hidden('dimension_id', 0);
	if ($dim < 2)
		hidden('dimension2_id', 0);
	if (!$supplier_id)	
		table_section(2);

	table_section_title(_('Addresses'));
	textarea_row(_('Mailing Address:'), 'address', null, 35, 5);
	textarea_row(_('Physical Address:'), 'supp_address', null, 35, 5);

	table_section_title(_('General'));
	textarea_row(_('General Notes:'), 'notes', null, 35, 5);
	if ($supplier_id)
		record_status_list_row(_('Supplier status:'), 'inactive');
	end_outer_table(1);

	div_start('controls');
	if (@$_REQUEST['popup']) hidden('popup', 1);
	if ($supplier_id) {
		submit_center_first('submit', _('Update Supplier'), 
		  _('Update supplier data'), $page_nested ? true : false);
		submit_return('select', get_post('supplier_id'), _('Select this supplier and return to document entry.'));
		submit_center_last('delete', _('Delete Supplier'), 
		  _('Delete supplier data if have been never used'), true);
	}
	else 
		submit_center('submit', _('Add New Supplier Details'), true, '', false);
	
	div_end();
}

start_form(true);

if (db_has_suppliers()) {
	start_table(false, '', 3);
	start_row();
	supplier_list_cells(_('Select a supplier: '), 'supplier_id', null,
		  _('New supplier'), true, check_value('show_inactive'));
	check_cells(_('Show inactive:'), 'show_inactive', null, true);
	end_row();
	end_table();
	if (get_post('_show_inactive_update')) {
		$Ajax->activate('supplier_id');
		set_focus('supplier_id');
	}
} 
else 
	hidden('supplier_id', get_post('supplier_id'));

if (!$supplier_id)
	unset($_POST['_tabs_sel']); // force settings tab for new customer

tabbed_content_start('tabs', array(
		'settings' => array(_('&General settings'), $supplier_id),
		'contacts' => array(_('&Contacts'), $supplier_id),
		'transactions' => array(_('&Transactions'), (user_check_access('SA_SUPPTRANSVIEW') ? $supplier_id : null)),
		'orders' => array(_('Purchase &Orders'), (user_check_access('SA_SUPPTRANSVIEW') ? $supplier_id : null)),
		'attachments' => array(_('Attachments'), (user_check_access('SA_ATTACHDOCUMENT') ? $supplier_id : null)),
	));
	
	switch (get_post('_tabs_sel')) {
		default:
		case 'settings':
			supplier_settings($supplier_id); 
			break;
		case 'contacts':
			$contacts = new contacts('contacts', $supplier_id, 'supplier');
			$contacts->show();
			break;
		case 'transactions':
			$_GET['supplier_id'] = $supplier_id;
			include_once($path_to_root.'/purchasing/inquiry/supplier_inquiry.php');
			break;
		case 'orders':
			$_GET['supplier_id'] = $supplier_id;
			include_once($path_to_root.'/purchasing/inquiry/po_search_completed.php');
			break;
		case 'attachments':
			$_GET['trans_no'] = $supplier_id;
			$_GET['type_no']= ST_SUPPLIER;
			$attachments = new attachments('attachment', $supplier_id, 'suppliers');
			$attachments->show();
	};
br();
tabbed_content_end();
end_form();
end_page(@$_REQUEST['popup']);
