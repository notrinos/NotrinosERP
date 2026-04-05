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
 * CRM Convert Lead - Convert Lead to Opportunity or Customer
 *
 * Workflow:
 * 1. Lead -> Opportunity (promote within CRM pipeline)
 * 2. Lead/Opportunity -> Customer (create debtor in ERP)
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_LEAD';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_communication_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

page(_($help_context = 'Convert Lead'));

//--------------------------------------------------------------------------
// Get lead
//--------------------------------------------------------------------------

$lead_id = isset($_GET['LeadID']) ? (int)$_GET['LeadID'] : (int)get_post('LeadID', 0);
if ($lead_id <= 0) {
    display_error(_('No lead specified.'));
    hyperlink_params($path_to_root . '/crm/manage/leads.php', _('Back to Leads'), 'sel_app=crm');
    end_page();
    exit;
}

$lead = get_crm_lead($lead_id);
if (!$lead) {
    display_error(_('Lead not found.'));
    hyperlink_params($path_to_root . '/crm/manage/leads.php', _('Back to Leads'), 'sel_app=crm');
    end_page();
    exit;
}

//--------------------------------------------------------------------------
// Handle Convert to Opportunity
//--------------------------------------------------------------------------

if (isset($_POST['ConvertToOpportunity'])) {
    $stage_id = (int)$_POST['stage_id'];

    begin_transaction();
    convert_lead_to_opportunity($lead_id, $stage_id);
    commit_transaction();

    display_notification(sprintf(
        _('Lead "%s" has been converted to an Opportunity.'),
        $lead['title']
    ));

    meta_forward('opportunity_entry.php', 'LeadID=' . $lead_id . crm_sel_app_param());
    exit;
}

//--------------------------------------------------------------------------
// Handle Convert to Customer
//--------------------------------------------------------------------------

if (isset($_POST['ConvertToCustomer'])) {
    $input_error = 0;

    if (strlen(trim($_POST['customer_name'])) < 1) {
        display_error(_('Customer name is required.'));
        set_focus('customer_name');
        $input_error = 1;
    }

    if ((int)$_POST['sales_type'] <= 0) {
        display_error(_('Sales type / price list is required.'));
        set_focus('sales_type');
        $input_error = 1;
    }

    if ($input_error == 0) {
        begin_transaction();

        // Create debtor (customer)
        $customer_name = $_POST['customer_name'];
        $address       = $lead['address'];
        $tax_id        = '';
        $curr_code     = get_company_pref('curr_default');
        $sales_type    = (int)$_POST['sales_type'];
        $dimension_id  = 0;
        $dimension2_id = 0;
        $credit_status = 1; // Good
        $payment_terms = (int)$_POST['payment_terms'];
        $discount      = 0;
        $pymt_discount = 0;
        $credit_limit  = (float)get_company_pref('default_credit_limit');

        // Generate debtor_ref from lead_ref
        $debtor_ref = $lead['lead_ref'];

        // Insert into debtors_master
        $sql = "INSERT INTO " . TB_PREF . "debtors_master (
            name, debtor_ref, address, tax_id, curr_code, sales_type, dimension_id, dimension2_id,
            credit_status, payment_terms, discount, pymt_discount, credit_limit, notes
        ) VALUES ("
            . db_escape($customer_name) . ", "
            . db_escape($debtor_ref) . ", "
            . db_escape($address) . ", "
            . db_escape($tax_id) . ", "
            . db_escape($curr_code) . ", "
            . db_escape($sales_type) . ", "
            . db_escape($dimension_id) . ", "
            . db_escape($dimension2_id) . ", "
            . db_escape($credit_status) . ", "
            . db_escape($payment_terms) . ", "
            . db_escape($discount) . ", "
            . db_escape($pymt_discount) . ", "
            . db_escape($credit_limit) . ", "
            . db_escape('')
            . ")";
        db_query($sql, 'could not add customer');
        $customer_id = db_insert_id();

        // Add branch for the customer
        $branch_ref = $debtor_ref;
        $branch_sql = "INSERT INTO " . TB_PREF . "cust_branch (
            debtor_no, br_name, branch_ref, br_address, br_post_address,
            salesman, area, tax_group_id,
            sales_account, receivables_account, payment_discount_account,
            sales_discount_account, default_location, default_ship_via,
            group_no, notes
        ) VALUES ("
            . db_escape($customer_id) . ", "
            . db_escape($customer_name) . ", "
            . db_escape($branch_ref) . ", "
            . db_escape($address) . ", "
            . db_escape($address) . ", "
            . "0, 1, 1, "
            . db_escape(get_company_pref('default_sales_act')) . ", "
            . db_escape(get_company_pref('debtors_act')) . ", "
            . db_escape(get_company_pref('pyt_discount_act')) . ", "
            . db_escape(get_company_pref('default_sales_discount_act')) . ", "
            . db_escape('DEF') . ", "
            . "1, 0, '')";
        db_query($branch_sql, 'could not add customer branch');

        // Create CRM contact for the customer if auto-create is on
        if (get_crm_setting('auto_create_contact', '1') == '1') {
            $contact_sql = "INSERT INTO " . TB_PREF . "crm_persons (
                ref, name, name2, address, phone, phone2, email, notes
            ) VALUES ("
                . db_escape($lead['lead_ref']) . ", "
                . db_escape($lead['title']) . ", "
                . db_escape($lead['company_name']) . ", "
                . db_escape($lead['address'] . ', ' . $lead['city'] . ' ' . $lead['state'] . ' ' . $lead['postal_code'] . ', ' . $lead['country']) . ", "
                . db_escape($lead['phone']) . ", "
                . db_escape($lead['mobile']) . ", "
                . db_escape($lead['email']) . ", "
                . db_escape('')
                . ")";
            db_query($contact_sql, 'could not add CRM contact');
            $person_id = db_insert_id();

            // Link contact to customer
            $link_sql = "INSERT INTO " . TB_PREF . "crm_contacts (
                person_id, type, action, entity_id
            ) VALUES ("
                . db_escape($person_id) . ", 'customer', 'general', " . db_escape($customer_id)
                . ")";
            db_query($link_sql, 'could not link CRM contact');
        }

        // Update lead with customer_id and mark as converted
        $update_sql = "UPDATE " . TB_PREF . "crm_leads SET
            linked_customer_id = " . db_escape($customer_id) . ",
            lead_status = " . db_escape(CRM_LEAD_CONVERTED) . "
            WHERE id = " . db_escape($lead_id);
        db_query($update_sql, 'could not update lead');

        // Copy communications if setting is on
        if (get_crm_setting('carry_forward_communication', '1') == '1') {
            copy_lead_communications_to_customer($lead_id, $customer_id);
        }

        commit_transaction();

        display_notification(sprintf(
            _('Lead "%s" has been converted to Customer #%d.'),
            $lead['title'],
            $customer_id
        ));
        hyperlink_params($path_to_root . '/sales/manage/customers.php', _('View Customer'), 'debtor_no=' . $customer_id);
        echo '<br>';
        hyperlink_params($path_to_root . '/crm/manage/leads.php', _('Back to Leads'), 'sel_app=crm');
        end_page();
        exit;
    }
}

//--------------------------------------------------------------------------
// Display Lead Summary
//--------------------------------------------------------------------------

start_form();
hidden('LeadID', $lead_id);

display_heading(sprintf(_('Convert Lead: %s'), htmlspecialchars($lead['title'])));

start_table(TABLESTYLE2);
label_row(_('Reference:'), $lead['lead_ref']);
label_row(_('Lead Name:'), $lead['title']);
label_row(_('Organization:'), $lead['company_name']);
label_row(_('Email:'), $lead['email']);
label_row(_('Phone:'), $lead['phone']);
label_row(_('Status:'), crm_status_badge($lead['lead_status'], crm_lead_statuses()));
if ($lead['is_opportunity']) {
    label_row(_('Type:'), '<strong>' . _('Opportunity') . '</strong>');
    if ($lead['stage_name'])
        label_row(_('Stage:'), $lead['stage_name']);
}
end_table(1);

//--------------------------------------------------------------------------
// Option 1: Convert to Opportunity (only if not already one)
//--------------------------------------------------------------------------

if (!$lead['is_opportunity']) {
    display_heading(_('Option 1: Convert to Opportunity'));
    start_table(TABLESTYLE2);
    crm_sales_stage_list_row(_('Initial Stage:'), 'stage_id', null);
    end_table(1);

    echo "<center>";
    submit('ConvertToOpportunity', _('Convert to Opportunity'), true, '', 'default');
    echo "</center><br>";
}

//--------------------------------------------------------------------------
// Option 2: Convert to Customer
//--------------------------------------------------------------------------

display_heading($lead['is_opportunity'] ? _('Convert to Customer') : _('Option 2: Convert to Customer'));
start_table(TABLESTYLE2);

$default_name = $lead['company_name'] ? $lead['company_name'] : $lead['title'];
text_row(_('Customer Name:'), 'customer_name', $default_name, 60, 100);

// Sales type/price list
$st_sql = "SELECT id, sales_type FROM " . TB_PREF . "sales_types ORDER BY sales_type";
$st_res = db_query($st_sql);
$st_options = array(0 => _('-- Select --'));
while ($st = db_fetch($st_res)) {
    $st_options[$st['id']] = $st['sales_type'];
}
array_selector_row(_('Sales Type / Price List:'), 'sales_type', null, $st_options);

// Payment terms
$pt_sql = "SELECT terms_indicator, terms FROM " . TB_PREF . "payment_terms ORDER BY terms";
$pt_res = db_query($pt_sql);
$pt_options = array(0 => _('-- Select --'));
while ($pt = db_fetch($pt_res)) {
    $pt_options[$pt['terms_indicator']] = $pt['terms'];
}
array_selector_row(_('Payment Terms:'), 'payment_terms', null, $pt_options);

end_table(1);

echo "<center>";
submit('ConvertToCustomer', _('Convert to Customer'), true, '', 'default');
echo "</center><br>";

end_form();
end_page();

