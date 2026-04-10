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
 * Warranty Provision Settings — configure GL accounts and rates for
 * automatic warranty provision accrual/release.
 *
 * Access: SA_TRACKINGSETTINGS
 */
$page_security = 'SA_TRACKINGSETTINGS';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Warranty Provision Settings');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/warranty_provision_db.inc');

//----------------------------------------------------------------------
// Save settings
//----------------------------------------------------------------------
if (isset($_POST['save'])) {
	$provision_account = get_post('warranty_provision_account', '');
	$expense_account = get_post('warranty_expense_account', '');
	$rate = input_num('warranty_provision_rate', 5.0);
	$enabled = check_value('warranty_provision_enabled') ? 1 : 0;

	// Validate: if enabling, both accounts must be set
	if ($enabled && ($provision_account == '' || $expense_account == '')) {
		display_error(_('Both Warranty Provision Account and Warranty Expense Account must be set to enable automatic provisioning.'));
	} else {
		set_company_pref('warranty_provision_account', 'tracking', 'VARCHAR', 15, $provision_account);
		set_company_pref('warranty_expense_account', 'tracking', 'VARCHAR', 15, $expense_account);
		set_company_pref('warranty_provision_rate', 'tracking', 'REAL', 8, $rate);
		set_company_pref('warranty_provision_enabled', 'tracking', 'TINYINT', 1, $enabled);
		display_notification(_('Warranty provision settings have been updated.'));
	}
}

//----------------------------------------------------------------------
// Load current settings
//----------------------------------------------------------------------
$config = get_warranty_provision_config();

//----------------------------------------------------------------------
// Form
//----------------------------------------------------------------------
start_form();

start_outer_table(TABLESTYLE2);

table_section(1);
table_section_title(_('Warranty Provision GL Settings'));

check_row(_('Enable Automatic Warranty Provision:'), 'warranty_provision_enabled', $config['enabled']);

gl_all_accounts_list_row(_('Warranty Provision Account (Liability):'), 'warranty_provision_account',
	$config['provision_account'], true, false, _('Select a liability account for warranty provision'));

gl_all_accounts_list_row(_('Warranty Expense Account (P&&L):'), 'warranty_expense_account',
	$config['expense_account'], true, false, _('Select an expense account for warranty costs'));

small_amount_row(_('Default Provision Rate (%):'), 'warranty_provision_rate',
	$config['rate'], null, _('Percentage of item cost to accrue as warranty provision'));

table_section(2);
table_section_title(_('How It Works'));

label_row(_('At Delivery:'), _('When a warranted item is delivered, an accrual is posted:'));
label_row('', _('DR: Warranty Expense Account (P&L)'));
label_row('', _('CR: Warranty Provision Account (Liability)'));
label_row('', '');
label_row(_('Amount:'), _('Item Cost x Quantity x Provision Rate (%)'));
label_row('', '');
label_row(_('On Expiry/Resolution:'), _('When warranty expires or claim is resolved, provision is released:'));
label_row('', _('DR: Warranty Provision Account'));
label_row('', _('CR: Warranty Expense Account'));

// Show current balance
$balance = get_warranty_provision_balance();
label_row('', '');
label_row(_('Current Outstanding Provision:'), price_format($balance),
	'style="font-weight:bold; font-size:14px;"');

end_outer_table(1);

submit_center('save', _('Save Settings'), true, '', 'default');

end_form();
end_page();
