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
$page_security = 'SA_SALESCOMMISSION';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Commission Plans'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/sales/includes/db/sales_commission_db.inc');

// tab: 0 = Plans, 1 = Tiers, 2 = Assignments
$tab = isset($_GET['tab']) ? (int)$_GET['tab'] : (isset($_POST['tab']) ? (int)$_POST['tab'] : 0);

simple_page_mode(true);

// ============================================================================
// PLAN CRUD
// ============================================================================

/**
 * Validate commission plan form.
 *
 * @return bool
 */
function can_process_plan()
{
    if (strlen(trim(get_post('plan_name'))) == 0) {
        display_error(_('Plan name cannot be empty.'));
        set_focus('plan_name');
        return false;
    }
    return true;
}

if ($tab == 0) {
    if (($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') && can_process_plan()) {
        $date_start = get_post('date_start') !== '' ? date2sql(get_post('date_start')) : '';
        $date_end   = get_post('date_end')   !== '' ? date2sql(get_post('date_end'))   : '';

        if ($Mode == 'ADD_ITEM') {
            add_commission_plan(
                get_post('plan_name'),
                get_post('plan_type'),
                get_post('calculation_base'),
                get_post('period_type'),
                get_post('status'),
                $date_start,
                $date_end
            );
            display_notification(_('Commission plan has been added.'));
        } else {
            update_commission_plan(
                $selected_id,
                get_post('plan_name'),
                get_post('plan_type'),
                get_post('calculation_base'),
                get_post('period_type'),
                get_post('status'),
                $date_start,
                $date_end,
                check_value('inactive') ? 1 : 0
            );
            display_notification(_('Commission plan has been updated.'));
        }
        $Mode = 'RESET';
    }

    if ($Mode == 'Delete') {
        if (!delete_commission_plan($selected_id))
            display_error(_('Cannot delete this plan because commission entries already reference it.'));
        else
            display_notification(_('Commission plan has been deleted.'));
        $Mode = 'RESET';
    }
}

// ============================================================================
// TIER CRUD
// ============================================================================

if ($tab == 1) {
    $plan_id_for_tiers = isset($_GET['plan_id']) ? (int)$_GET['plan_id']
        : (isset($_POST['plan_id_for_tiers']) ? (int)$_POST['plan_id_for_tiers'] : 0);

    if (($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM')) {
        if (!check_num('threshold_from', 0) || !check_num('commission_rate', 0, 100)) {
            display_error(_('Threshold must be >= 0 and commission rate between 0-100.'));
        } else {
            $sort = (int)get_post('sort_order', 0);
            if ($Mode == 'ADD_ITEM') {
                add_commission_tier($plan_id_for_tiers, input_num('threshold_from'),
                    input_num('threshold_to'), input_num('commission_rate'),
                    input_num('fixed_bonus'), $sort);
                display_notification(_('Tier has been added.'));
            } else {
                update_commission_tier($selected_id, input_num('threshold_from'),
                    input_num('threshold_to'), input_num('commission_rate'),
                    input_num('fixed_bonus'), $sort);
                display_notification(_('Tier has been updated.'));
            }
            $Mode = 'RESET';
        }
    }

    if ($Mode == 'Delete') {
        delete_commission_tier($selected_id);
        display_notification(_('Tier has been deleted.'));
        $Mode = 'RESET';
    }
}

// ============================================================================
// ASSIGNMENT CRUD
// ============================================================================

if ($tab == 2) {
    if ($Mode == 'ADD_ITEM') {
        $date_start = get_post('assign_date_start') !== '' ? date2sql(get_post('assign_date_start')) : '';
        $date_end   = get_post('assign_date_end')   !== '' ? date2sql(get_post('assign_date_end'))   : '';
        assign_plan_to_salesman(
            (int)get_post('assign_plan_id'),
            (int)get_post('assign_salesman_id'),
            $date_start,
            $date_end,
            input_num('assign_target')
        );
        display_notification(_('Plan has been assigned to salesman.'));
        $Mode = 'RESET';
    }

    if ($Mode == 'Delete') {
        delete_commission_assignment($selected_id);
        display_notification(_('Assignment has been removed.'));
        $Mode = 'RESET';
    }
}

if ($Mode == 'RESET') {
    $selected_id = -1;
    unset($_POST['plan_name'], $_POST['plan_type'], $_POST['calculation_base'],
        $_POST['period_type'], $_POST['status'], $_POST['date_start'], $_POST['date_end'],
        $_POST['inactive'],
        $_POST['threshold_from'], $_POST['threshold_to'], $_POST['commission_rate'],
        $_POST['fixed_bonus'], $_POST['sort_order'],
        $_POST['assign_plan_id'], $_POST['assign_salesman_id'],
        $_POST['assign_date_start'], $_POST['assign_date_end'], $_POST['assign_target']);
}

// ============================================================================
// TAB NAVIGATION
// ============================================================================
$tab_labels = array(_('Plans'), _('Tiers'), _('Assignments'));
start_form();
hidden('tab', $tab);
echo "<div style='margin-bottom:10px'>";
foreach ($tab_labels as $t => $label) {
    $active = ($t == $tab) ? ' style="font-weight:bold;text-decoration:underline"' : '';
    echo "<a href='commission_plans.php?tab=".$t."'".$active.">".$label."</a>&nbsp;&nbsp;";
}
echo "</div>";
end_form();

// ============================================================================
// TAB 0: PLANS LIST + FORM
// ============================================================================
if ($tab == 0) {
    // List
    $result = get_commission_plans(false);
    start_table(TABLESTYLE, "width='70%'");
    $th = array(_('Name'), _('Type'), _('Base'), _('Period'), _('Status'),
                _('Start'), _('End'), '', '');
    table_header($th);
    $k = 0;
    while ($row = db_fetch($result)) {
        alt_table_row_color($k);
        label_cell($row['name']);
        label_cell(ucfirst($row['plan_type']));
        label_cell(ucfirst($row['calculation_base']));
        label_cell(ucfirst(str_replace('_', ' ', $row['period_type'])));
        label_cell(ucfirst($row['status']));
        label_cell($row['date_start'] ? sql2date($row['date_start']) : '-');
        label_cell($row['date_end']   ? sql2date($row['date_end'])   : '-');
        // Tiers shortcut
        label_cell("<a href='commission_plans.php?tab=1&plan_id=".(int)$row['id']."'>"._('Tiers')."</a>");
        edit_button_cell('Edit'.$row['id'], _('Edit'));
        end_row();
    }
    end_table(1);

    // Form
    start_table(TABLESTYLE2);
    if ($selected_id != -1 && $Mode == 'Edit') {
        $myrow = get_commission_plan($selected_id);
        $_POST['plan_name']        = $myrow['name'];
        $_POST['plan_type']        = $myrow['plan_type'];
        $_POST['calculation_base'] = $myrow['calculation_base'];
        $_POST['period_type']      = $myrow['period_type'];
        $_POST['status']           = $myrow['status'];
        $_POST['date_start']       = $myrow['date_start'] ? sql2date($myrow['date_start']) : '';
        $_POST['date_end']         = $myrow['date_end']   ? sql2date($myrow['date_end'])   : '';
        $_POST['inactive']         = $myrow['inactive'];
        hidden('selected_id', $selected_id);
    }

    text_row_ex(_('Plan Name:'), 'plan_name', 50);

    $plan_types = array('percentage'=>_('Flat Percentage'), 'tiered'=>_('Tiered'),
        'target_based'=>_('Target Based'), 'achievement'=>_('Achievement'));
    label_row(_('Plan Type:'),
        array_selector('plan_type', get_post('plan_type','percentage'), $plan_types));

    $calc_bases = array('revenue'=>_('Revenue'), 'margin'=>_('Margin'), 'quantity'=>_('Quantity'));
    label_row(_('Calculation Base:'),
        array_selector('calculation_base', get_post('calculation_base','revenue'), $calc_bases));

    $period_types = array('per_transaction'=>_('Per Transaction'), 'monthly'=>_('Monthly'),
        'quarterly'=>_('Quarterly'), 'yearly'=>_('Yearly'));
    label_row(_('Period Type:'),
        array_selector('period_type', get_post('period_type','per_transaction'), $period_types));

    $statuses = array('draft'=>_('Draft'), 'active'=>_('Active'), 'expired'=>_('Expired'));
    label_row(_('Status:'),
        array_selector('status', get_post('status','draft'), $statuses));

    date_row(_('Start Date:'), 'date_start', '', true);
    date_row(_('End Date:'),   'date_end',   '', true);

    if ($selected_id != -1)
        check_row(_('Inactive:'), 'inactive', get_post('inactive'));

    end_table(1);
    submit_add_or_update_center($selected_id == -1, '', 'both');
}

// ============================================================================
// TAB 1: TIERS
// ============================================================================
if ($tab == 1) {
    $plan_id_for_tiers = isset($_GET['plan_id']) ? (int)$_GET['plan_id']
        : (isset($_POST['plan_id_for_tiers']) ? (int)$_POST['plan_id_for_tiers'] : 0);

    if ($plan_id_for_tiers) {
        $plan = get_commission_plan($plan_id_for_tiers);
        display_notification(sprintf(_('Editing tiers for plan: %s'), '<b>'.htmlspecialchars($plan['name']).'</b>'));
    }

    hidden('plan_id_for_tiers', $plan_id_for_tiers);

    $result = get_commission_tiers($plan_id_for_tiers);
    start_table(TABLESTYLE, "width='60%'");
    $th = array(_('From'), _('To (0=unlimited)'), _('Rate %'), _('Fixed Bonus'), _('Order'), '', '');
    table_header($th);
    $k = 0;
    while ($row = db_fetch($result)) {
        alt_table_row_color($k);
        amount_cell($row['threshold_from']);
        amount_cell($row['threshold_to']);
        label_cell(percent_format($row['commission_rate']).' %', 'align=right');
        amount_cell($row['fixed_bonus']);
        label_cell($row['sort_order']);
        edit_button_cell('Edit'.$row['id'], _('Edit'));
        delete_button_cell('Delete'.$row['id'], _('Delete'));
        end_row();
    }
    end_table(1);

    start_table(TABLESTYLE2);
    if ($selected_id != -1 && $Mode == 'Edit') {
        $myrow_tier = db_fetch(db_query(
            "SELECT * FROM ".TB_PREF."sales_commission_tiers WHERE id = ".db_escape($selected_id),
            'Get tier'));
        $_POST['threshold_from']  = price_format($myrow_tier['threshold_from']);
        $_POST['threshold_to']    = price_format($myrow_tier['threshold_to']);
        $_POST['commission_rate'] = percent_format($myrow_tier['commission_rate']);
        $_POST['fixed_bonus']     = price_format($myrow_tier['fixed_bonus']);
        $_POST['sort_order']      = $myrow_tier['sort_order'];
        hidden('selected_id', $selected_id);
    }

    amount_row(_('Threshold From:'), 'threshold_from', price_format(0));
    amount_row(_('Threshold To (0=unlimited):'), 'threshold_to', price_format(0));
    percent_row(_('Commission Rate %:'), 'commission_rate');
    amount_row(_('Fixed Bonus:'), 'fixed_bonus', price_format(0));
    text_row_ex(_('Sort Order:'), 'sort_order', 5, 5, get_post('sort_order', '0'));
    end_table(1);
    submit_add_or_update_center($selected_id == -1, '', 'both');
}

// ============================================================================
// TAB 2: ASSIGNMENTS
// ============================================================================
if ($tab == 2) {
    // List all assignments across all plans/salesmen
    $sql = "SELECT a.*, p.name AS plan_name, s.salesman_name
            FROM ".TB_PREF."sales_commission_assignments a
            JOIN ".TB_PREF."sales_commission_plans p ON p.id = a.plan_id
            JOIN ".TB_PREF."salesman s ON s.salesman_code = a.salesman_id
            ORDER BY s.salesman_name, a.date_start DESC";
    $result = db_query($sql, 'Get all assignments');

    start_table(TABLESTYLE, "width='75%'");
    $th = array(_('Salesman'), _('Plan'), _('Start'), _('End'), _('Target'), '');
    table_header($th);
    $k = 0;
    while ($row = db_fetch($result)) {
        alt_table_row_color($k);
        label_cell($row['salesman_name']);
        label_cell($row['plan_name']);
        label_cell($row['date_start'] ? sql2date($row['date_start']) : '-');
        label_cell($row['date_end']   ? sql2date($row['date_end'])   : '-');
        amount_cell($row['target_amount']);
        delete_button_cell('Delete'.$row['id'], _('Remove'));
        end_row();
    }
    end_table(1);

    start_form();
    hidden('tab', 2);
    start_table(TABLESTYLE2);

    // Plan selector
    $plans_arr = array();
    $plans_res = get_commission_plans(true);
    while ($p = db_fetch($plans_res))
        $plans_arr[$p['id']] = $p['name'];
    label_row(_('Commission Plan:'),
        array_selector('assign_plan_id', get_post('assign_plan_id'), $plans_arr));

    // Salesman selector
    include_once($path_to_root . '/sales/includes/db/sales_groups_db.inc');
    $salesmen_arr = array();
    $salesmen_res = get_salesmen(false);
    while ($s = db_fetch($salesmen_res))
        $salesmen_arr[$s['salesman_code']] = $s['salesman_name'];
    label_row(_('Sales Person:'),
        array_selector('assign_salesman_id', get_post('assign_salesman_id'), $salesmen_arr));

    date_row(_('Start Date:'), 'assign_date_start', '', true);
    date_row(_('End Date:'),   'assign_date_end',   '', true);
    amount_row(_('Target Amount:'), 'assign_target', price_format(0));
    end_table(1);
    submit_center('ADD_ITEM', _('Assign Plan'), true, '', 'both');
    end_form();
}

end_page();
