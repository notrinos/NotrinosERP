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

$page_security = 'SA_PURCHMATCHCONFIG';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Purchase Matching Configuration'));

/**
 * Get matching type options.
 *
 * @return array
 */
function get_matching_type_options()
{
    return array(
        'price_variance' => _('Price Variance'),
        'quantity_variance' => _('Quantity Variance'),
        'total_variance' => _('Total Variance'),
    );
}

/**
 * Get tolerance type options.
 *
 * @return array
 */
function get_tolerance_type_options()
{
    return array(
        'percentage' => _('Percentage'),
        'fixed_amount' => _('Fixed Amount'),
    );
}

/**
 * Get action-on-exceed options.
 *
 * @return array
 */
function get_matching_action_options()
{
    return array(
        'warn' => _('Warn'),
        'block' => _('Block'),
        'require_approval' => _('Require Approval'),
    );
}

/**
 * Validate matching config form values.
 *
 * @return bool
 */
function can_save_matching_config()
{
    if (!array_key_exists(get_post('match_type'), get_matching_type_options())) {
        display_error(_('Select a valid matching type.'));
        set_focus('match_type');
        return false;
    }

    if (!array_key_exists(get_post('tolerance_type'), get_tolerance_type_options())) {
        display_error(_('Select a valid tolerance type.'));
        set_focus('tolerance_type');
        return false;
    }

    if (!array_key_exists(get_post('action_on_exceed'), get_matching_action_options())) {
        display_error(_('Select a valid action-on-exceed value.'));
        set_focus('action_on_exceed');
        return false;
    }

    if (!check_num('tolerance_value', 0)) {
        display_error(_('Tolerance value must be zero or greater.'));
        set_focus('tolerance_value');
        return false;
    }

    return true;
}

/**
 * Validate bill control policy inputs.
 *
 * @return bool
 */
function can_save_bill_policy()
{
    if (trim(get_post('policy_name')) === '') {
        display_error(_('Policy name is required.'));
        set_focus('policy_name');
        return false;
    }

    if (get_post('bill_basis') !== 'on_ordered' && get_post('bill_basis') !== 'on_received') {
        display_error(_('Select a valid bill basis.'));
        set_focus('bill_basis');
        return false;
    }

    return true;
}

/**
 * Reset matching config edit form defaults.
 *
 * @return void
 */
function reset_matching_config_form()
{
    $_POST['selected_match_id'] = 0;
    $_POST['match_type'] = 'price_variance';
    $_POST['supplier_id'] = 0;
    $_POST['tolerance_type'] = 'percentage';
    $_POST['tolerance_value'] = 5;
    $_POST['action_on_exceed'] = 'warn';
    $_POST['match_inactive'] = 0;
}

/**
 * Reset bill control policy edit form defaults.
 *
 * @return void
 */
function reset_bill_policy_form()
{
    $_POST['selected_policy_id'] = 0;
    $_POST['policy_name'] = '';
    $_POST['bill_basis'] = 'on_received';
    $_POST['require_grn_before_invoice'] = 1;
    $_POST['require_po_for_invoice'] = 0;
    $_POST['allow_over_invoice'] = 0;
    $_POST['auto_match_on_grn'] = 1;
    $_POST['policy_is_default'] = 0;
    $_POST['policy_inactive'] = 0;
}

if (!isset($_POST['selected_match_id']))
    reset_matching_config_form();
if (!isset($_POST['selected_policy_id']))
    reset_bill_policy_form();

$edit_match_id = find_submit('EditMatch');
if ($edit_match_id > 0) {
    $row = get_matching_config_row($edit_match_id);
    if ($row) {
        $_POST['selected_match_id'] = $row['id'];
        $_POST['match_type'] = $row['match_type'];
        $_POST['supplier_id'] = $row['supplier_id'];
        $_POST['tolerance_type'] = $row['tolerance_type'];
        $_POST['tolerance_value'] = $row['tolerance_value'];
        $_POST['action_on_exceed'] = $row['action_on_exceed'];
        $_POST['match_inactive'] = $row['inactive'];
    }
}

if (isset($_POST['SaveMatch']) && can_save_matching_config()) {
    if ((int)get_post('selected_match_id') > 0) {
        update_matching_config(
            (int)get_post('selected_match_id'),
            get_post('match_type'),
            get_post('tolerance_type'),
            input_num('tolerance_value'),
            get_post('action_on_exceed'),
            (int)get_post('supplier_id'),
            null,
            check_value('match_inactive') ? 1 : 0
        );
        display_notification(_('Matching configuration has been updated.'));
    } else {
        add_matching_config(
            get_post('match_type'),
            get_post('tolerance_type'),
            input_num('tolerance_value'),
            get_post('action_on_exceed'),
            (int)get_post('supplier_id'),
            null,
            check_value('match_inactive') ? 1 : 0
        );
        display_notification(_('Matching configuration has been added.'));
    }
    reset_matching_config_form();
}

if (isset($_POST['ResetMatch']))
    reset_matching_config_form();

$edit_policy_id = find_submit('EditPolicy');
if ($edit_policy_id > 0) {
    $row = get_bill_control_policy_row($edit_policy_id);
    if ($row) {
        $_POST['selected_policy_id'] = $row['id'];
        $_POST['policy_name'] = $row['name'];
        $_POST['bill_basis'] = $row['bill_basis'];
        $_POST['require_grn_before_invoice'] = $row['require_grn_before_invoice'];
        $_POST['require_po_for_invoice'] = $row['require_po_for_invoice'];
        $_POST['allow_over_invoice'] = $row['allow_over_invoice'];
        $_POST['auto_match_on_grn'] = $row['auto_match_on_grn'];
        $_POST['policy_is_default'] = $row['is_default'];
        $_POST['policy_inactive'] = $row['inactive'];
    }
}

if (isset($_POST['SavePolicy']) && can_save_bill_policy()) {
    if ((int)get_post('selected_policy_id') > 0) {
        update_bill_control_policy(
            (int)get_post('selected_policy_id'),
            trim(get_post('policy_name')),
            get_post('bill_basis'),
            check_value('require_grn_before_invoice') ? 1 : 0,
            check_value('require_po_for_invoice') ? 1 : 0,
            check_value('allow_over_invoice') ? 1 : 0,
            check_value('auto_match_on_grn') ? 1 : 0,
            check_value('policy_is_default') ? 1 : 0,
            check_value('policy_inactive') ? 1 : 0,
            null
        );
        display_notification(_('Bill control policy has been updated.'));
    } else {
        add_bill_control_policy(
            trim(get_post('policy_name')),
            get_post('bill_basis'),
            check_value('require_grn_before_invoice') ? 1 : 0,
            check_value('require_po_for_invoice') ? 1 : 0,
            check_value('allow_over_invoice') ? 1 : 0,
            check_value('auto_match_on_grn') ? 1 : 0,
            check_value('policy_is_default') ? 1 : 0,
            check_value('policy_inactive') ? 1 : 0,
            null
        );
        display_notification(_('Bill control policy has been added.'));
    }
    reset_bill_policy_form();
}

if (isset($_POST['ResetPolicy']))
    reset_bill_policy_form();

start_form();
hidden('selected_match_id', get_post('selected_match_id'));
hidden('selected_policy_id', get_post('selected_policy_id'));

display_heading(_('Matching Tolerance Configuration'));
start_table(TABLESTYLE2, "width='100%'");
start_row();
    label_cell(_('Match Type:'));
    echo "<td>" . array_selector('match_type', get_post('match_type'), get_matching_type_options(), array('class' => array('nosearch'))) . "</td>";
    supplier_list_cells(_('Supplier Override:'), 'supplier_id', get_post('supplier_id'), true, true);
end_row();
start_row();
    label_cell(_('Tolerance Type:'));
    echo "<td>" . array_selector('tolerance_type', get_post('tolerance_type'), get_tolerance_type_options(), array('class' => array('nosearch'))) . "</td>";
    amount_cells(_('Tolerance Value:'), 'tolerance_value', get_post('tolerance_value'));
end_row();
start_row();
    label_cell(_('Action On Exceed:'));
    echo "<td>" . array_selector('action_on_exceed', get_post('action_on_exceed'), get_matching_action_options(), array('class' => array('nosearch'))) . "</td>";
    check_cells(_('Inactive:'), 'match_inactive', get_post('match_inactive'));
end_row();
end_table(1);
submit_center_first('SaveMatch', _('Save Matching Config'), '', 'default');
submit_center_last('ResetMatch', _('Reset'));

br();
start_table(TABLESTYLE, "width='100%'");
$th = array(_('Match Type'), _('Supplier'), _('Tolerance'), _('Action'), _('Inactive'), '');
table_header($th);

$result = get_matching_config(-1, '', 1);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell(get_matching_type_options()[$row['match_type']]);
    label_cell((int)$row['supplier_id'] > 0 ? get_supplier_name($row['supplier_id']) : _('Default (All Suppliers)'));
    label_cell(($row['tolerance_type'] === 'percentage' ? number_format2($row['tolerance_value'], 2) . '%' : price_format($row['tolerance_value'])));
    label_cell(get_matching_action_options()[$row['action_on_exceed']]);
    label_cell($row['inactive'] ? _('Yes') : _('No'));
    submit_cells('EditMatch' . $row['id'], _('Edit'));
    end_row();
}
end_table(2);

br();

display_heading(_('Bill Control Policies'));
start_table(TABLESTYLE2, "width='100%'");
start_row();
    text_cells(_('Policy Name:'), 'policy_name', get_post('policy_name'), 40, 100);
    label_cell(_('Bill Basis:'));
    echo "<td>" . array_selector('bill_basis', get_post('bill_basis'), array('on_ordered' => _('On Ordered Qty'), 'on_received' => _('On Received Qty')), array('class' => array('nosearch'))) . "</td>";
end_row();
start_row();
    check_cells(_('Require GRN Before Invoice:'), 'require_grn_before_invoice', get_post('require_grn_before_invoice'));
    check_cells(_('Require PO For Invoice:'), 'require_po_for_invoice', get_post('require_po_for_invoice'));
end_row();
start_row();
    check_cells(_('Allow Over Invoice:'), 'allow_over_invoice', get_post('allow_over_invoice'));
    check_cells(_('Auto Match On GRN:'), 'auto_match_on_grn', get_post('auto_match_on_grn'));
end_row();
start_row();
    check_cells(_('Default Policy:'), 'policy_is_default', get_post('policy_is_default'));
    check_cells(_('Inactive:'), 'policy_inactive', get_post('policy_inactive'));
end_row();
end_table(1);
submit_center_first('SavePolicy', _('Save Bill Policy'));
submit_center_last('ResetPolicy', _('Reset'));

br();
start_table(TABLESTYLE, "width='100%'");
$th = array(_('Policy'), _('Bill Basis'), _('GRN Required'), _('PO Required'), _('Default'), _('Inactive'), '');
table_header($th);

$result = get_bill_control_policies(1);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['name']);
    label_cell($row['bill_basis'] === 'on_ordered' ? _('On Ordered Qty') : _('On Received Qty'));
    label_cell($row['require_grn_before_invoice'] ? _('Yes') : _('No'));
    label_cell($row['require_po_for_invoice'] ? _('Yes') : _('No'));
    label_cell($row['is_default'] ? _('Yes') : _('No'));
    label_cell($row['inactive'] ? _('Yes') : _('No'));
    submit_cells('EditPolicy' . $row['id'], _('Edit'));
    end_row();
}
end_table();

end_form();
end_page();
