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
include_once($path_to_root . '/crm/includes/db/crm_leads_entity.inc');
include_once($path_to_root . '/crm/includes/db/crm_communication_db.inc');
include_once($path_to_root . '/sales/includes/db/customers_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_conversion_classification_db.inc');
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

$lead = crm_leads_entity::find_joined($lead_id);
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
    if ($lead['is_opportunity']) {
        display_error(_('This lead is already an opportunity.'));
    } else {
        $stage_id = (int)$_POST['stage_id'];

        begin_transaction();
        crm_leads_entity::convert_to_opportunity($lead_id, $stage_id);
        commit_transaction();

        display_notification(sprintf(
            _('Lead "%s" has been converted to an Opportunity.'),
            $lead['title']
        ));

        meta_forward('opportunity_entry.php', 'LeadID=' . $lead_id . crm_sel_app_param());
        exit;
    }
}

//--------------------------------------------------------------------------
// Handle Convert to Customer (DIM-CRM-001: explicit classification, single customer)
//--------------------------------------------------------------------------

/**
 * Companion rows written in the conversion transaction after the customer exists: its default branch,
 * the optional CRM contact and the carried-forward communications.
 *
 * @param int $customer_id New customer identity
 * @param array $lead Locked lead row
 * @return int CRM person identity, or 0 when no contact was created
 */
function crm_convert_lead_companions($customer_id, $lead) {
    $lead = crm_leads_entity::find_joined((int)$lead['id']);
    $address = $lead['address'];
    $branch_ref = $lead['lead_ref'];
    $branch_sql = "INSERT INTO " . TB_PREF . "cust_branch (
        debtor_no, br_name, branch_ref, br_address, br_post_address,
        salesman, area, tax_group_id,
        sales_account, receivables_account, payment_discount_account,
        sales_discount_account, default_location, default_ship_via,
        group_no, notes
    ) VALUES ("
        . db_escape($customer_id) . ", "
        . db_escape(trim($_POST['customer_name'])) . ", "
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

    $person_id = 0;
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

        $link_sql = "INSERT INTO " . TB_PREF . "crm_contacts (
            person_id, type, action, entity_id
        ) VALUES ("
            . db_escape($person_id) . ", 'customer', 'general', " . db_escape($customer_id)
            . ")";
        db_query($link_sql, 'could not link CRM contact');
    }

    if (get_crm_setting('carry_forward_communication', '1') == '1') {
        copy_lead_communications_to_customer((int)$lead['id'], $customer_id);
    }
    return (int)$person_id;
}

/**
 * Show the outcome of a conversion and stop the page.
 *
 * @param array $outcome crm_convert_lead_to_customer()/crm_convert_lead_link_customer() result
 * @param array $lead Lead row
 * @return void
 */
function crm_convert_lead_finish($outcome, $lead) {
    global $path_to_root;

    if ($outcome['state'] === 'already_converted' || $outcome['state'] === 'already_linked') {
        display_notification(sprintf(
            _('Lead "%s" is already converted to Customer #%d; no second customer was created.'),
            $lead['title'], $outcome['customer_id']
        ));
    } elseif ($outcome['state'] === 'linked') {
        display_notification(sprintf(
            _('Lead "%s" has been linked to the existing Customer #%d. The customer classification was not changed.'),
            $lead['title'], $outcome['customer_id']
        ));
    } else {
        display_notification(sprintf(
            _('Lead "%s" has been converted to Customer #%d.'),
            $lead['title'], $outcome['customer_id']
        ));
        if (count($outcome['follow_up'])) {
            display_warning(sprintf(
                _('Customer #%d was created without a required category. It is listed in the conversion follow-up queue until it is classified.'),
                $outcome['customer_id']
            ));
        }
    }
    if (crm_conversion_can_manage_customer()) {
        hyperlink_params($path_to_root . '/sales/manage/customers.php', _('View Customer'), 'debtor_no=' . $outcome['customer_id']);
        echo '<br>';
    }
    hyperlink_params($path_to_root . '/crm/inquiry/conversion_follow_up.php', _('Conversion follow-up'), 'sel_app=crm');
    echo '<br>';
    hyperlink_params($path_to_root . '/crm/manage/leads.php', _('Back to Leads'), 'sel_app=crm');
    end_page();
    exit;
}

if (isset($_POST['ConvertToCustomer'])) {
    $input_error = 0;
    $customer_name = trim($_POST['customer_name']);
    $payment_terms = (int)$_POST['payment_terms'];

    if (!crm_conversion_can_manage_customer()) {
        display_error(_('You are not authorized to create customers.'));
        $input_error = 1;
    }

    if (strlen($customer_name) < 1) {
        display_error(_('Customer name is required.'));
        set_focus('customer_name');
        $input_error = 1;
    }

    if ((int)$_POST['sales_type'] <= 0) {
        display_error(_('Sales type / price list is required.'));
        set_focus('sales_type');
        $input_error = 1;
    }

    if ($payment_terms <= 0) {
        display_error(_('Payment terms are required.'));
        set_focus('payment_terms');
        $input_error = 1;
    }

    $decision = crm_conversion_decision_from_values(
        get_post('crm_dimension_id', 0), get_post('crm_dimension2_id', 0),
        get_post('_crm_classification_unassigned_1', 0), get_post('_crm_classification_unassigned_2', 0));
    if ($decision === false) {
        display_error(_('A category cannot be both selected and explicitly unassigned.'));
        $input_error = 1;
    }

    if ($input_error == 0) {
        try {
            $outcome = crm_convert_lead_to_customer($lead_id, array(
                'customer_name' => $customer_name,
                'debtor_ref'    => $lead['lead_ref'],
                'address'       => $lead['address'],
                'curr_code'     => get_company_pref('curr_default'),
                'credit_limit'  => (float)get_company_pref('default_credit_limit'),
                'sales_type'    => (int)$_POST['sales_type'],
                'payment_terms' => $payment_terms,
            ), $decision, 'crm_convert_lead_companions');
            crm_convert_lead_finish($outcome, $lead);
        } catch (DimensionFinancialReferenceWriteException $exception) {
            display_error($exception->getMessage());
        }
    }
}

//--------------------------------------------------------------------------
// Handle Link to an existing customer (no duplicate, no classification copy)
//--------------------------------------------------------------------------

if (isset($_POST['LinkExistingCustomer'])) {
    try {
        $outcome = crm_convert_lead_link_customer($lead_id, (int)get_post('existing_customer_id', 0));
        crm_convert_lead_finish($outcome, $lead);
    } catch (DimensionFinancialReferenceWriteException $exception) {
        display_error($exception->getMessage());
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

if ((int)$lead['linked_customer_id'] > 0) {
    display_note(sprintf(
        _('This lead is already converted to Customer #%d. It cannot be converted again, so no second customer is created.'),
        (int)$lead['linked_customer_id']
    ));
    if (crm_conversion_can_manage_customer() && (int)$lead['linked_customer_id'] > 0) {
        hyperlink_params($path_to_root . '/sales/manage/customers.php', _('View Customer'), 'debtor_no=' . (int)$lead['linked_customer_id']);
        echo '<br>';
    }
    hyperlink_params($path_to_root . '/crm/inquiry/conversion_follow_up.php', _('Conversion follow-up'), 'sel_app=crm');
    end_form();
    end_page();
    exit;
}

if (!crm_conversion_can_manage_customer()) {
    display_note(_('Creating or linking a customer needs customer-master authority. Ask an authorized user to convert this lead.'));
    end_form();
    end_page();
    exit;
}

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

// DIM-CRM-001: the finance classification of the new customer is an explicit decision, never a guess.
if (crm_conversion_ready()) {
    $required_axes = crm_conversion_required_axes();
    foreach (array(1, 2) as $crm_axis) {
        if ($crm_axis > crm_conversion_enabled_axes())
            continue;
        $crm_suffix = $crm_axis == 2 ? '2' : '';
        $crm_label = dimension_category_policy_label($crm_axis) . (in_array($crm_axis, $required_axes) ? ' *' : '') . ':';
        dimensions_list_row($crm_label, 'crm_dimension' . $crm_suffix . '_id', null, true,
            _('Choose a category'), false, $crm_axis, false);
        check_row(in_array($crm_axis, $required_axes)
            ? _('Leave unassigned and list it in the conversion follow-up queue')
            : _('Leave this category explicitly unassigned'), '_crm_classification_unassigned_' . $crm_axis);
    }
    if (count($required_axes))
        label_row('', _('* The company requires this category. An unassigned required category is kept as a visible follow-up.'));
}

end_table(1);

echo "<center>";
submit('ConvertToCustomer', _('Convert to Customer'), true, '', 'default');
echo "</center><br>";

// Link to an existing customer instead of creating a duplicate.
display_heading(_('Or link to an existing customer'));
start_table(TABLESTYLE2);
$link_sql = "SELECT debtor_no, name FROM " . TB_PREF . "debtors_master WHERE inactive=0 ORDER BY name";
$link_res = db_query($link_sql);
$link_options = array(0 => _('-- Select Customer --'));
while ($link_customer = db_fetch($link_res)) {
    $link_options[$link_customer['debtor_no']] = $link_customer['name'];
}
array_selector_row(_('Existing Customer:'), 'existing_customer_id', null, $link_options);
label_row('', _('Linking keeps the customer as it is: its classification is not copied or changed.'));
end_table(1);
echo "<center>";
submit('LinkExistingCustomer', _('Link Existing Customer'), true, '', false);
echo "</center><br>";

end_form();
end_page();

