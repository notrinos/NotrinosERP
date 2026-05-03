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
$page_security = 'SA_SETUPCOMPANY';
$path_to_root = '..';
include($path_to_root.'/includes/session.inc');

page(_($help_context = 'Company Setup'));

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');

include_once($path_to_root.'/admin/db/company_db.inc');
include_once($path_to_root.'/reporting/includes/tcpdf.php');

//-------------------------------------------------------------------------------------------------

/**
 * Rebuild active hooks and application switcher model for current session.
 *
 * This makes optional module changes (Manufacturing, Fixed Assets, HRM)
 * visible immediately in the sidebar application switcher.
 *
 * @return void
 */
function refresh_current_session_application_switcher() {
	if (!isset($_SESSION['App']))
		return;

	$selected_application_id = isset($_SESSION['App']->selected_application)
		? $_SESSION['App']->selected_application
		: '';

	install_hooks();
	$_SESSION['App']->init();

	if ($selected_application_id != '' && $_SESSION['App']->get_application($selected_application_id))
		$_SESSION['App']->selected_application = $selected_application_id;
	else
		$_SESSION['App']->selected_application = user_startup_tab();
}

/**
 * Get advanced company preference definitions added by optional feature phases.
 *
 * @return array
 */
function get_advanced_company_pref_definitions() {
	return array(
		'use_advanced_pricelists' => array('category' => 'sys', 'type' => 'tinyint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_quotation_templates' => array('category' => 'setup.company', 'type' => 'tinyint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_margin_display' => array('category' => 'setup.company', 'type' => 'tinyint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_sales_agreements' => array('category' => 'sales', 'type' => 'tinyint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'sales_agreement_expiry_alert_days' => array('category' => 'sales', 'type' => 'smallint', 'length' => 6, 'default' => '30', 'checkbox' => false),
		'use_discount_programs' => array('category' => 'sales', 'type' => 'tinyint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_advanced_commissions' => array('category' => 'setup.company', 'type' => 'tinyint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_advanced_credit_control' => array('category' => 'setup.company', 'type' => 'tinyint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'credit_check_on_order' => array('category' => 'setup.company', 'type' => 'tinyint', 'length' => 1, 'default' => '1', 'checkbox' => true),
		'credit_check_on_delivery' => array('category' => 'setup.company', 'type' => 'tinyint', 'length' => 1, 'default' => '1', 'checkbox' => true),
		'use_rma' => array('category' => 'sales', 'type' => 'tinyint', 'length' => 1, 'default' => '1', 'checkbox' => true),
		'use_purchase_requisitions' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'requisition_auto_approval_limit' => array('category' => 'purchase', 'type' => 'int', 'length' => 11, 'default' => '0', 'checkbox' => false),
		'use_purchase_rfq' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'rfq_default_deadline_days' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 6, 'default' => '14', 'checkbox' => false),
		'use_purchase_agreements' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'purchase_agreement_expiry_alert_days' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 6, 'default' => '30', 'checkbox' => false),
		'use_vendor_evaluation' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_vendor_pricelists' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_purchase_templates' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_3way_matching' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'default_matching_tolerance_pct' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 6, 'default' => '5', 'checkbox' => false),
		'use_procurement_planning' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'use_purchase_dashboard' => array('category' => 'purchase', 'type' => 'smallint', 'length' => 1, 'default' => '1', 'checkbox' => true),
		'warranty_provision_enabled' => array('category' => 'tracking', 'type' => 'TINYINT', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'warranty_provision_account' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 15, 'default' => '', 'checkbox' => false),
		'warranty_expense_account' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 15, 'default' => '', 'checkbox' => false),
		'warranty_provision_rate' => array('category' => 'tracking', 'type' => 'REAL', 'length' => 8, 'default' => '5.0', 'checkbox' => false),
		'regulatory_compliance_enabled' => array('category' => 'tracking', 'type' => 'TINYINT', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'dscsa_enabled' => array('category' => 'tracking', 'type' => 'TINYINT', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'fsma204_enabled' => array('category' => 'tracking', 'type' => 'TINYINT', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'udi_enabled' => array('category' => 'tracking', 'type' => 'TINYINT', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'fmda_enabled' => array('category' => 'tracking', 'type' => 'TINYINT', 'length' => 1, 'default' => '0', 'checkbox' => true),
		'dscsa_company_license' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 50, 'default' => '', 'checkbox' => false),
		'dscsa_company_dea' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 20, 'default' => '', 'checkbox' => false),
		'fsma204_firm_name' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 100, 'default' => '', 'checkbox' => false),
		'fsma204_fda_registration' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 30, 'default' => '', 'checkbox' => false),
		'udi_company_name' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 100, 'default' => '', 'checkbox' => false),
		'udi_issuing_agency' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 10, 'default' => 'GS1', 'checkbox' => false),
		'fmda_company_license' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 50, 'default' => '', 'checkbox' => false),
		'fmda_authority_reference' => array('category' => 'tracking', 'type' => 'VARCHAR', 'length' => 50, 'default' => '', 'checkbox' => false)
	);
}

/**
 * Ensure advanced company preference rows exist for older companies.
 *
 * @param array $definitions
 * @return array
 */
function ensure_advanced_company_preferences_exist($definitions) {
	$company_preferences = get_company_prefs();

	foreach ($definitions as $preference_name => $definition) {
		if (array_key_exists($preference_name, $company_preferences))
			continue;

		set_company_pref($preference_name, $definition['category'], $definition['type'], $definition['length'], $definition['default']);
		$company_preferences[$preference_name] = $definition['default'];

		if (isset($_SESSION['SysPrefs']->prefs) && is_array($_SESSION['SysPrefs']->prefs))
			$_SESSION['SysPrefs']->prefs[$preference_name] = $definition['default'];
	}

	return $company_preferences;
}

/**
 * Build a get_post field list for advanced company preferences.
 *
 * @param array $definitions
 * @return array
 */
function get_advanced_company_pref_post_fields($definitions) {
	$post_fields = array();

	foreach ($definitions as $preference_name => $definition) {
		if (!empty($definition['checkbox']))
			$post_fields[$preference_name] = 0;
		else
			$post_fields[] = $preference_name;
	}

	return $post_fields;
}

/**
 * Get preference names that control application navigation visibility.
 *
 * @return array
 */
function get_application_navigation_pref_names() {
	return array(
		'use_quotation_templates',
		'use_margin_display',
		'use_sales_agreements',
		'use_discount_programs',
		'use_advanced_commissions',
		'use_advanced_credit_control',
		'use_rma',
		'use_purchase_requisitions',
		'use_purchase_rfq',
		'use_purchase_agreements',
		'use_vendor_evaluation',
		'use_vendor_pricelists',
		'use_purchase_templates',
		'use_3way_matching',
		'use_procurement_planning',
		'use_purchase_dashboard'
	);
}

/**
 * Check whether any navigation-driven company preference has changed.
 *
 * @param array $company_preferences
 * @param array $preference_names
 * @return bool
 */
function have_navigation_company_preferences_changed($company_preferences, $preference_names) {
	foreach ($preference_names as $preference_name) {
		if ((int)@$company_preferences[$preference_name] !== (int)check_value($preference_name))
			return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------

$advanced_company_pref_definitions = get_advanced_company_pref_definitions();
$current_company_preferences = ensure_advanced_company_preferences_exist($advanced_company_pref_definitions);

if (isset($_POST['update']) && $_POST['update'] != '') {
	$input_error = 0;
	$optional_modules_changed =
		(int)$current_company_preferences['use_manufacturing'] !== (int)check_value('use_manufacturing')
		|| (int)$current_company_preferences['use_fixed_assets'] !== (int)check_value('use_fixed_assets')
		|| (int)$current_company_preferences['use_hrm'] !== (int)check_value('use_hrm')
		|| (int)@$current_company_preferences['use_crm'] !== (int)check_value('use_crm');
	$application_navigation_changed = $optional_modules_changed
		|| have_navigation_company_preferences_changed($current_company_preferences, get_application_navigation_pref_names());

	if (!check_num('login_tout', 10)) {
		display_error(_('Login timeout must be positive number not less than 10.'));
		set_focus('login_tout');
		$input_error = 1;
	}
	if (strlen($_POST['coy_name'])==0) {
		$input_error = 1;
		display_error(_('The company name must be entered.'));
		set_focus('coy_name');
	}
	if (!check_num('tax_prd', 1)) {
		display_error(_('Tax Periods must be positive number.'));
		set_focus('tax_prd');
		$input_error = 1;
	}
	if (!check_num('tax_last', 1)) {
		display_error(_('Tax Last Periods must be positive number.'));
		set_focus('tax_last');
		$input_error = 1;
	}
	if (!check_num('round_to', 1)) {
		display_error(_('Round Calculated field must be a positive number.'));
		set_focus('round_to');
		$input_error = 1;
	}
	if (!check_num('max_days_in_docs', 1)) {
		display_error(_('Max day range in Documents must be a positive number.'));
		set_focus('max_days_in_docs');
		$input_error = 1;
	}
	if (!check_num('sales_agreement_expiry_alert_days', 0)) {
		display_error(_('Sales agreement expiry alert days must be zero or a positive number.'));
		set_focus('sales_agreement_expiry_alert_days');
		$input_error = 1;
	}
	if (!check_num('requisition_auto_approval_limit', 0)) {
		display_error(_('Requisition auto approval limit must be zero or a positive number.'));
		set_focus('requisition_auto_approval_limit');
		$input_error = 1;
	}
	if (!check_num('rfq_default_deadline_days', 0)) {
		display_error(_('RFQ default deadline days must be zero or a positive number.'));
		set_focus('rfq_default_deadline_days');
		$input_error = 1;
	}
	if (!check_num('purchase_agreement_expiry_alert_days', 0)) {
		display_error(_('Purchase agreement expiry alert days must be zero or a positive number.'));
		set_focus('purchase_agreement_expiry_alert_days');
		$input_error = 1;
	}
	if (!check_num('default_matching_tolerance_pct', 0, 100)) {
		display_error(_('Default matching tolerance must be between 0 and 100.'));
		set_focus('default_matching_tolerance_pct');
		$input_error = 1;
	}
	if (!check_num('warranty_provision_rate', 0)) {
		display_error(_('Warranty provision rate must be zero or a positive number.'));
		set_focus('warranty_provision_rate');
		$input_error = 1;
	}
	if (check_value('warranty_provision_enabled')
		&& (trim($_POST['warranty_provision_account']) == '' || trim($_POST['warranty_expense_account']) == '')) {
		display_error(_('Both warranty GL accounts must be selected before enabling automatic warranty provision.'));
		set_focus('warranty_provision_account');
		$input_error = 1;
	}
	if ($_POST['add_pct'] != '' && !is_numeric($_POST['add_pct'])) {
		display_error(_('Add Price from Std Cost field must be number.'));
		set_focus('add_pct');
		$input_error = 1;
	}	
	if (isset($_FILES['pic']) && $_FILES['pic']['name'] != '') {
		if ($_FILES['pic']['error'] == UPLOAD_ERR_INI_SIZE) {
			display_error(_('The file size is over the maximum allowed.'));
			$input_error = 1;
		}
		elseif ($_FILES['pic']['error'] > 0) {
			display_error(_('Error uploading logo file.'));
			$input_error = 1;
		}
		$result = $_FILES['pic']['error'];
		$filename = company_path().'/images';
		if (!file_exists($filename))
			mkdir($filename);

		$filename .= '/'.clean_file_name($_FILES['pic']['name']);

		 //But check for the worst
		if (!in_array( substr($filename,-4), array('.jpg', '.JPG', '.png', '.PNG'))) {
			display_error(_('Only jpg and png files are supported - a file extension of .jpg or .png is expected'));
			$input_error = 1;
		}
		elseif ( $_FILES['pic']['size'] > ($SysPrefs->max_image_size * 1024)) { //File Size Check
			display_error(_('The file size is over the maximum allowed. The maximum size allowed in KB is').' '.$SysPrefs->max_image_size);
			$input_error = 1;
		}
		elseif ( $_FILES['pic']['type'] == 'text/plain' ) {  //File type Check
			display_error( _('Only graphics files can be uploaded'));
			$input_error = 1;
		}
		elseif (file_exists($filename)) {
			$result = unlink($filename);
			if (!$result) {
				display_error(_('The existing image could not be removed'));
				$input_error = 1;
			}
		}

		if ($input_error != 1) {
			$result  =  move_uploaded_file($_FILES['pic']['tmp_name'], $filename);
			$_POST['coy_logo'] = clean_file_name($_FILES['pic']['name']);
			if(!$result) {
				display_error(_('Error uploading logo file'));
				$input_error = 1;
			}
			else {
				$msg = check_image_file($filename);
				if ( $msg) {
					display_error( $msg);
					unlink($filename);
					$input_error = 1;
				}
			}
		}
	}
	if (check_value('del_coy_logo')) {
		$filename = company_path().'/images/'.clean_file_name($_POST['coy_logo']);
		if (file_exists($filename)) {
			$result = unlink($filename);
			if (!$result) {
				display_error(_('The existing image could not be removed'));
				$input_error = 1;
			}
		}
		$_POST['coy_logo'] = '';
	}
	if ($_POST['add_pct'] == '')
		$_POST['add_pct'] = -1;
	if ($_POST['round_to'] <= 0)
		$_POST['round_to'] = 1;
	if ($_POST['udi_issuing_agency'] == '')
		$_POST['udi_issuing_agency'] = 'GS1';
	if ($input_error != 1) {
		update_company_prefs(
			get_post(
				array_merge(
					array('coy_name', 'coy_no', 'gst_no', 'tax_prd', 'tax_last', 'postal_address', 'phone', 'fax', 'email', 'coy_logo', 'domicile', 'use_dimension', 'curr_default', 'f_year', 'shortname_name_in_list', 'no_customer_list'=>0, 'no_supplier_list'=>0, 'base_sales', 'ref_no_auto_increase'=>0, 'dim_on_recurrent_invoice'=>0, 'long_description_invoice'=>0, 'max_days_in_docs'=>180, 'company_logo_on_views'=>0, 'time_zone'=>0, 'company_logo_report'=>0, 'barcodes_on_stock'=>0, 'print_dialog_direct'=>0, 'add_pct', 'round_to', 'login_tout', 'auto_curr_reval', 'bcc_email', 'alternative_tax_include_on_docs', 'suppress_tax_rates', 'use_manufacturing', 'use_fixed_assets', 'use_hrm', 'use_crm'),
					get_advanced_company_pref_post_fields($advanced_company_pref_definitions)
				)
			)
		);

		$_SESSION['wa_current_user']->timeout = $_POST['login_tout'];

		if ($application_navigation_changed) {
			$_SESSION['company_setup_updated_notice'] = 1;
			$SysPrefs->refresh();
			refresh_current_session_application_switcher();
			meta_forward($_SERVER['PHP_SELF'], 'sel_app='.(isset($_SESSION['sel_app']) ? $_SESSION['sel_app'] : 'system'));
		}

		display_notification_centered(_('Company setup has been updated.'));
		set_focus('coy_name');
		$Ajax->activate('_page_body');
	}
}

if (isset($_SESSION['company_setup_updated_notice']) && $_SESSION['company_setup_updated_notice']) {
	display_notification_centered(_('Company setup has been updated.'));
	unset($_SESSION['company_setup_updated_notice']);
}

start_form(true);

$myrow = ensure_advanced_company_preferences_exist($advanced_company_pref_definitions);

$_POST['coy_name'] = $myrow['coy_name'];
$_POST['gst_no'] = $myrow['gst_no'];
$_POST['tax_prd'] = $myrow['tax_prd'];
$_POST['tax_last'] = $myrow['tax_last'];
$_POST['coy_no']  = $myrow['coy_no'];
$_POST['postal_address']  = $myrow['postal_address'];
$_POST['phone']  = $myrow['phone'];
$_POST['fax']  = $myrow['fax'];
$_POST['email']  = $myrow['email'];
$_POST['coy_logo']  = $myrow['coy_logo'];
$_POST['domicile']  = $myrow['domicile'];
$_POST['use_dimension']  = $myrow['use_dimension'];
$_POST['base_sales']  = $myrow['base_sales'];
$_POST['shortname_name_in_list']  = $myrow['shortname_name_in_list'];
$_POST['no_customer_list']  = $myrow['no_customer_list'];
$_POST['no_supplier_list']  = $myrow['no_supplier_list'];
$_POST['curr_default']  = $myrow['curr_default'];
$_POST['f_year']  = $myrow['f_year'];
$_POST['time_zone']  = $myrow['time_zone'];
$_POST['max_days_in_docs']  = $myrow['max_days_in_docs'];
$_POST['company_logo_report']  = $myrow['company_logo_report'];
$_POST['ref_no_auto_increase']  = $myrow['ref_no_auto_increase'];
$_POST['barcodes_on_stock']  = $myrow['barcodes_on_stock'];
$_POST['print_dialog_direct']  = $myrow['print_dialog_direct'];
$_POST['dim_on_recurrent_invoice']  = $myrow['dim_on_recurrent_invoice'];
$_POST['long_description_invoice']  = $myrow['long_description_invoice'];

if (!isset($myrow['company_logo_on_views'])) {
	set_company_pref('company_logo_on_views', 'setup.company', 'tinyint', 1, '0');
	$myrow['company_logo_on_views'] = get_company_pref('company_logo_on_views');
}
$_POST['company_logo_on_views']  = $myrow["company_logo_on_views"];

$_POST['version_id']  = $myrow['version_id'];
$_POST['add_pct'] = $myrow['add_pct'];
$_POST['login_tout'] = $myrow['login_tout'];
if ($_POST['add_pct'] == -1)
	$_POST['add_pct'] = '';
$_POST['round_to'] = $myrow['round_to'];	
$_POST['auto_curr_reval'] = $myrow['auto_curr_reval'];	
$_POST['del_coy_logo']  = 0;
$_POST['bcc_email']  = $myrow['bcc_email'];
$_POST['alternative_tax_include_on_docs']  = $myrow['alternative_tax_include_on_docs'];
$_POST['suppress_tax_rates']  = $myrow['suppress_tax_rates'];
$_POST['use_manufacturing']  = $myrow['use_manufacturing'];
$_POST['use_fixed_assets']  = $myrow['use_fixed_assets'];
$_POST['use_hrm']  = $myrow['use_hrm'];
$_POST['use_crm']  = @$myrow['use_crm'];
foreach ($advanced_company_pref_definitions as $preference_name => $definition) {
	$_POST[$preference_name] = isset($myrow[$preference_name]) ? $myrow[$preference_name] : $definition['default'];
}
if ($_POST['udi_issuing_agency'] == '')
	$_POST['udi_issuing_agency'] = 'GS1';

start_outer_table(TABLESTYLE2);

table_section(1);
table_section_title(_('General settings'));

text_row_ex(_('Name (to appear on reports):'), 'coy_name', 50, 50);
textarea_row(_('Address:'), 'postal_address', $_POST['postal_address'], 34, 5);
text_row_ex(_('Domicile:'), 'domicile', 25, 55);

text_row_ex(_('Phone Number:'), 'phone', 25, 55);
text_row_ex(_('Fax Number:'), 'fax', 25);
email_row_ex(_('Email Address:'), 'email', 50, 55);

email_row_ex(_('BCC Address for all outgoing mails:'), 'bcc_email', 50, 55);

text_row_ex(_('Official Company Number:'), 'coy_no', 25);
text_row_ex(_('GSTNo:'), 'gst_no', 25);
currencies_list_row(_('Home Currency:'), 'curr_default', $_POST['curr_default']);

label_row(_('Company Logo:'), $_POST['coy_logo']);
file_row(_('New Company Logo (.jpg)') . ':', 'pic', 'pic');
check_row(_('Delete Company Logo:'), 'del_coy_logo', $_POST['del_coy_logo']);

check_row(_('Time Zone on Reports:'), 'time_zone', $_POST['time_zone']);
check_row(_('Company Logo on Reports:'), 'company_logo_report', $_POST['company_logo_report']);
check_row(_('Use Barcodes on Stocks:'), 'barcodes_on_stock', $_POST['barcodes_on_stock']);
check_row(_('Auto Increase of Document References:'), 'ref_no_auto_increase', $_POST['ref_no_auto_increase']);
check_row(_('Use Dimensions on Recurrent Invoices:'), 'dim_on_recurrent_invoice', $_POST['dim_on_recurrent_invoice']);
check_row(_('Use Long Descriptions on Invoices:'), 'long_description_invoice', $_POST['long_description_invoice']);
check_row(_('Company Logo on Views'), 'company_logo_on_views', $_POST['company_logo_on_views']);
label_row(_('Database Scheme Version:'), $_POST['version_id']);

table_section(2);

table_section_title(_('General Ledger Settings'));
fiscalyears_list_row(_('Fiscal Year:'), 'f_year', $_POST['f_year']);
text_row_ex(_('Tax Periods:'), 'tax_prd', 10, 10, '', null, null, _('Months.'));
text_row_ex(_('Tax Last Period:'), 'tax_last', 10, 10, '', null, null, _('Months back.'));
check_row(_('Put alternative Tax Include on Docs:'), 'alternative_tax_include_on_docs', null);
check_row(_('Suppress Tax Rates on Docs:'), 'suppress_tax_rates', null);
check_row(_('Automatic Revaluation Currency Accounts:'), 'auto_curr_reval', $_POST['auto_curr_reval']);

table_section_title(_('Sales Pricing'));
sales_types_list_row(_('Base for auto price calculations:'), 'base_sales', $_POST['base_sales'], false, _('No base price list') );

text_row_ex(_('Add Price from Std Cost:'), 'add_pct', 10, 10, '', null, null, '%');
$curr = get_currency($_POST['curr_default']);
text_row_ex(_('Round calculated prices to nearest:'), 'round_to', 10, 10, '', null, null, $curr['hundreds_name']);
label_row('', '&nbsp;');


table_section_title(_('Optional Modules'));
check_row(_('Manufacturing:'), 'use_manufacturing', null);
check_row(_('Fixed Assets').':', 'use_fixed_assets', null);
number_list_row(_('Use Dimensions:'), 'use_dimension', null, 0, 2);
check_row(_('Human Resources Management:'), 'use_hrm', null);
check_row(_('CRM (Customer Relationship Management):'), 'use_crm', null);

table_section_title(_('User Interface Options'));

check_row(_('Short Name and Name in List:'), 'shortname_name_in_list', $_POST['shortname_name_in_list']);
check_row(_('Open Print Dialog Direct on Reports:'), 'print_dialog_direct', null);
check_row(_('Search Customer List:'), 'no_customer_list', null);
check_row(_('Search Supplier List:'), 'no_supplier_list', null);
text_row_ex(_('Login Timeout:'), 'login_tout', 10, 10, '', null, null, _('seconds'));
text_row_ex(_('Max day range in documents:'), 'max_days_in_docs', 10, 10, '', null, null, _('days.'));

table_section(1);

table_section_title(_('Advanced Sales Features'));
check_row(_('Enable Advanced Pricelists:'), 'use_advanced_pricelists', $_POST['use_advanced_pricelists']);
check_row(_('Enable Quotation Templates:'), 'use_quotation_templates', $_POST['use_quotation_templates']);
check_row(_('Show Margin Display and Reports:'), 'use_margin_display', $_POST['use_margin_display']);
check_row(_('Enable Sales Agreements:'), 'use_sales_agreements', $_POST['use_sales_agreements']);
text_row_ex(_('Sales Agreement Expiry Alert Days:'), 'sales_agreement_expiry_alert_days', 10, 10, '', null, null, _('days'));
check_row(_('Enable Discount Programs:'), 'use_discount_programs', $_POST['use_discount_programs']);
check_row(_('Enable Advanced Commissions:'), 'use_advanced_commissions', $_POST['use_advanced_commissions']);
check_row(_('Enable Advanced Credit Control:'), 'use_advanced_credit_control', $_POST['use_advanced_credit_control']);
check_row(_('Check Credit on Sales Orders:'), 'credit_check_on_order', $_POST['credit_check_on_order']);
check_row(_('Check Credit on Deliveries:'), 'credit_check_on_delivery', $_POST['credit_check_on_delivery']);
check_row(_('Enable Return Merchandise Authorization (RMA):'), 'use_rma', $_POST['use_rma']);

table_section_title(_('Warranty Provision Controls'));
check_row(_('Enable Automatic Warranty Provision:'), 'warranty_provision_enabled', $_POST['warranty_provision_enabled']);
gl_all_accounts_list_row(_('Warranty Provision Account:'), 'warranty_provision_account', $_POST['warranty_provision_account'], true, false);
gl_all_accounts_list_row(_('Warranty Expense Account:'), 'warranty_expense_account', $_POST['warranty_expense_account'], true, false);
small_amount_row(_('Warranty Provision Rate (%):'), 'warranty_provision_rate', $_POST['warranty_provision_rate']);
label_row(_('Detailed warranty setup:'), "<a href='$path_to_root/inventory/manage/warranty_provision_settings.php?sel_app=stock'>"._('Warranty Provision Settings')."</a>");

table_section_title(_('Regulatory Compliance Controls'));
check_row(_('Enable Regulatory Compliance Module:'), 'regulatory_compliance_enabled', $_POST['regulatory_compliance_enabled']);
check_row(_('Enable DSCSA Compliance:'), 'dscsa_enabled', $_POST['dscsa_enabled']);
text_row_ex(_('DSCSA State License Number:'), 'dscsa_company_license', 30, 50);
text_row_ex(_('DSCSA DEA Registration Number:'), 'dscsa_company_dea', 20, 20);
check_row(_('Enable FSMA 204 Compliance:'), 'fsma204_enabled', $_POST['fsma204_enabled']);
text_row_ex(_('FSMA Registered Firm Name:'), 'fsma204_firm_name', 40, 100);
text_row_ex(_('FSMA FDA Registration Number:'), 'fsma204_fda_registration', 30, 30);
check_row(_('Enable UDI Compliance:'), 'udi_enabled', $_POST['udi_enabled']);
text_row_ex(_('UDI Company Name:'), 'udi_company_name', 40, 100);
text_row_ex(_('UDI Issuing Agency:'), 'udi_issuing_agency', 20, 10);
check_row(_('Enable FMDA Compliance:'), 'fmda_enabled', $_POST['fmda_enabled']);
text_row_ex(_('FMDA Company License:'), 'fmda_company_license', 30, 50);
text_row_ex(_('FMDA Authority Reference:'), 'fmda_authority_reference', 30, 50);
label_row(_('Detailed regulatory setup:'), "<a href='$path_to_root/inventory/manage/regulatory_compliance.php?sel_app=stock'>"._('Regulatory Compliance')."</a>");

table_section(2);

table_section_title(_('Advanced Purchasing Features'));
check_row(_('Enable Purchase Requisitions:'), 'use_purchase_requisitions', $_POST['use_purchase_requisitions']);
text_row_ex(_('Requisition Auto Approval Limit:'), 'requisition_auto_approval_limit', 12, 20, '', null, null, $curr['curr_abrev']);
check_row(_('Enable Purchase RFQ:'), 'use_purchase_rfq', $_POST['use_purchase_rfq']);
text_row_ex(_('RFQ Default Deadline Days:'), 'rfq_default_deadline_days', 10, 10, '', null, null, _('days'));
check_row(_('Enable Purchase Agreements:'), 'use_purchase_agreements', $_POST['use_purchase_agreements']);
text_row_ex(_('Agreement Expiry Alert Days:'), 'purchase_agreement_expiry_alert_days', 10, 10, '', null, null, _('days'));
check_row(_('Enable Vendor Evaluation:'), 'use_vendor_evaluation', $_POST['use_vendor_evaluation']);
check_row(_('Enable Vendor Pricelists:'), 'use_vendor_pricelists', $_POST['use_vendor_pricelists']);
check_row(_('Enable Purchase Templates:'), 'use_purchase_templates', $_POST['use_purchase_templates']);
check_row(_('Enable 3-Way Matching:'), 'use_3way_matching', $_POST['use_3way_matching']);
text_row_ex(_('Default Matching Tolerance:'), 'default_matching_tolerance_pct', 10, 10, '', null, null, '%');
check_row(_('Enable Procurement Planning:'), 'use_procurement_planning', $_POST['use_procurement_planning']);
check_row(_('Enable Purchase Dashboard and Analytics:'), 'use_purchase_dashboard', $_POST['use_purchase_dashboard']);

end_outer_table(1);

hidden('coy_logo', $_POST['coy_logo']);
submit_center('update', _('Update'), true, '',  'default');

end_form(2);

end_page();
