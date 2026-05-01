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
 * Cycle Count Management Page
 *
 * Two tabs: Count Plans (CRUD) and Count Sessions (lifecycle management + count entry).
 *
 * Session 16 of UNIFIED_INVENTORY_IMPLEMENTATION_PLAN.md
 */

$page_security = 'SA_WAREHOUSE_CYCLE_COUNT';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_counting_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

page(_($help_context = 'Cycle Counting'), false, false, '', $js);

//----------------------------------------------------------------------
// Determine which tab is active: plans or counts
//----------------------------------------------------------------------
if (isset($_POST['tab_plans'])) {
    $_POST['tab'] = 'plans';
    $Ajax->activate('_page_body');
} elseif (isset($_POST['tab_counts'])) {
    $_POST['tab'] = 'counts';
    $Ajax->activate('_page_body');
} elseif (!isset($_POST['tab']))
    $_POST['tab'] = 'counts';

$active_tab = $_POST['tab'];

// Plan mode uses its own set of mode vars
$plan_selected_id = get_post('plan_selected_id', -1);
$plan_mode = '';

if (isset($_POST['PlanEdit'])) {
    $plan_selected_id = key($_POST['PlanEdit']);
    $plan_mode = 'edit';
}
if (isset($_POST['PlanDelete'])) {
    $plan_selected_id = key($_POST['PlanDelete']);
    $plan_mode = 'delete';
}

// Count mode
simple_page_mode(true);

//----------------------------------------------------------------------
// Action Handlers — Count Plans
//----------------------------------------------------------------------

if (isset($_POST['AddPlan'])) {
    $plan_name = trim($_POST['plan_name']);
    $count_method = $_POST['count_method'];
    $plan_warehouse = get_post('plan_warehouse');
    if ($plan_warehouse == '' || $plan_warehouse == -1) $plan_warehouse = null;
    $frequency_days = intval($_POST['frequency_days']);
    $abc_class = get_post('plan_abc_class');
    if ($abc_class == '' || $abc_class == -1) $abc_class = null;

    if (strlen($plan_name) == 0) {
        display_error(_('Plan name cannot be empty.'));
    } elseif ($frequency_days < 1) {
        display_error(_('Frequency must be at least 1 day.'));
    } else {
        add_cycle_count_plan($plan_name, $count_method, $plan_warehouse, $frequency_days, $abc_class);
        display_notification(_('Count plan has been created.'));
        $plan_selected_id = -1;
        $plan_mode = '';
    }
    $active_tab = 'plans';
    $Ajax->activate('_page_body');
}

if (isset($_POST['UpdatePlan'])) {
    $plan_name = trim($_POST['plan_name']);
    $count_method = $_POST['count_method'];
    $plan_warehouse = get_post('plan_warehouse');
    if ($plan_warehouse == '' || $plan_warehouse == -1) $plan_warehouse = null;
    $frequency_days = intval($_POST['frequency_days']);
    $abc_class = get_post('plan_abc_class');
    if ($abc_class == '' || $abc_class == -1) $abc_class = null;
    $plan_active = check_value('plan_active') ? 1 : 0;

    if (strlen($plan_name) == 0) {
        display_error(_('Plan name cannot be empty.'));
    } else {
        update_cycle_count_plan($plan_selected_id, $plan_name, $count_method,
            $plan_warehouse, $frequency_days, $abc_class, $plan_active);
        display_notification(_('Count plan has been updated.'));
        $plan_selected_id = -1;
        $plan_mode = '';
    }
    $active_tab = 'plans';
    $Ajax->activate('_page_body');
}

if ($plan_mode == 'delete' && $plan_selected_id > 0) {
    if (can_delete_cycle_count_plan($plan_selected_id)) {
        delete_cycle_count_plan($plan_selected_id);
        display_notification(_('Count plan has been deleted.'));
    } else {
        display_error(_('Cannot delete this plan because it has associated count sessions.'));
    }
    $plan_selected_id = -1;
    $plan_mode = '';
    $active_tab = 'plans';
    $Ajax->activate('_page_body');
}

if (isset($_POST['CancelPlan'])) {
    $plan_selected_id = -1;
    $plan_mode = '';
    $active_tab = 'plans';
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Action Handlers — Count Sessions
//----------------------------------------------------------------------

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    $count_date = $_POST['count_date'];
    $count_warehouse = $_POST['count_warehouse'];
    $count_plan_id = get_post('count_plan_id');
    if ($count_plan_id == '' || $count_plan_id == -1) $count_plan_id = null;
    $count_memo = $_POST['count_memo'];
    $blind_mode = check_value('blind_mode') ? 1 : 0;

    if (!is_date($count_date)) {
        display_error(_('Invalid count date.'));
    } elseif (!$count_warehouse || $count_warehouse == -1 || $count_warehouse == '') {
        display_error(_('Please select a warehouse.'));
    } else {
        if ($Mode == 'ADD_ITEM') {
            $new_count_id = add_cycle_count($count_date, $count_warehouse, $count_plan_id, $count_memo);
            $lines = generate_count_lines($new_count_id, $count_plan_id, $blind_mode);
            display_notification(sprintf(_('Count session #%d created with %d lines.'), $new_count_id, $lines));
        } else {
            update_cycle_count($selected_id, $count_date, $count_warehouse, $count_plan_id, $count_memo);
            display_notification(_('Count session has been updated.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (can_delete_cycle_count($selected_id)) {
        delete_cycle_count($selected_id);
        display_notification(_('Count session has been deleted.'));
    } else {
        display_error(_('Cannot delete this count session. Only draft counts can be deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = -1;
    unset($_POST['count_date']);
    unset($_POST['count_warehouse']);
    unset($_POST['count_plan_id']);
    unset($_POST['count_memo']);
    unset($_POST['blind_mode']);
}

// Lifecycle actions
if (isset($_POST['StartCount'])) {
    $cid = key($_POST['StartCount']);
    if (start_cycle_count($cid))
        display_notification(sprintf(_('Count #%d started.'), $cid));
    else
        display_error(_('Cannot start this count. It must be in Draft status.'));
    $Ajax->activate('_page_body');
}

if (isset($_POST['SubmitReview'])) {
    $cid = key($_POST['SubmitReview']);
    if (submit_cycle_count_for_review($cid))
        display_notification(sprintf(_('Count #%d submitted for review.'), $cid));
    else
        display_error(_('Cannot submit. All lines must be counted before submitting for review.'));
    $Ajax->activate('_page_body');
}

if (isset($_POST['ApproveCount'])) {
    $cid = key($_POST['ApproveCount']);
    if (approve_cycle_count($cid))
        display_notification(sprintf(_('Count #%d approved.'), $cid));
    else
        display_error(_('Cannot approve this count.'));
    $Ajax->activate('_page_body');
}

if (isset($_POST['RecountCount'])) {
    $cid = key($_POST['RecountCount']);
    if (recount_cycle_count($cid))
        display_notification(sprintf(_('Count #%d sent back for recounting.'), $cid));
    else
        display_error(_('Cannot recount this count.'));
    $Ajax->activate('_page_body');
}

if (isset($_POST['PostCount'])) {
    $cid = key($_POST['PostCount']);
    $adj_id = post_cycle_count_adjustments($cid);
    if ($adj_id !== false) {
        if ($adj_id > 0)
            display_notification(sprintf(_('Count #%d posted. Inventory adjustment #%d created.'), $cid, $adj_id));
        else
            display_notification(sprintf(_('Count #%d posted. No variances to adjust.'), $cid));
    } else {
        display_error(_('Cannot post this count. It must be in Approved status.'));
    }
    $Ajax->activate('_page_body');
}

if (isset($_POST['CancelCount'])) {
    $cid = key($_POST['CancelCount']);
    if (cancel_cycle_count($cid))
        display_notification(sprintf(_('Count #%d cancelled and deleted.'), $cid));
    else
        display_error(_('Cannot cancel this count.'));
    $Ajax->activate('_page_body');
}

// Record count lines
if (isset($_POST['SaveCounts'])) {
    $cid = intval($_POST['counting_count_id']);
    $count = get_cycle_count($cid);
    if ($count && $count['status'] == 'in_progress') {
        $saved = 0;
        $lines = get_cycle_count_lines($cid);
        while ($line = db_fetch($lines)) {
            $key = 'counted_' . $line['line_id'];
            if (isset($_POST[$key]) && $_POST[$key] !== '') {
                $qty = floatval($_POST[$key]);
                if ($qty < 0) {
                    display_error(sprintf(_('Counted quantity cannot be negative (line %d).'), $line['line_id']));
                    continue;
                }
                $memo_key = 'line_memo_' . $line['line_id'];
                $line_memo = isset($_POST[$memo_key]) ? $_POST[$memo_key] : '';
                record_count_line($line['line_id'], $qty, $line_memo);
                $saved++;
            }
        }
        if ($saved > 0)
            display_notification(sprintf(_('%d count lines saved.'), $saved));
    }
    $Ajax->activate('_page_body');
}

// Add manual count line (unexpected item found)
if (isset($_POST['AddManualLine'])) {
    $cid = intval($_POST['counting_count_id']);
    $count = get_cycle_count($cid);
    if ($count && $count['status'] == 'in_progress') {
        $manual_bin = intval(get_post('manual_bin_id'));
        $manual_stock = get_post('manual_stock_id');
        $manual_qty = floatval(get_post('manual_counted_qty'));

        if (!$manual_bin || !$manual_stock) {
            display_error(_('Please select both a bin and an item for the manual line.'));
        } elseif ($manual_qty < 0) {
            display_error(_('Counted quantity cannot be negative.'));
        } else {
            add_manual_count_line($cid, $manual_bin, $manual_stock, $manual_qty,
                null, null, _('Manually added — unexpected item'));
            display_notification(_('Manual count line added.'));
        }
    }
    $Ajax->activate('_page_body');
}

// Auto-generate due counts
if (isset($_POST['AutoGenerate'])) {
    $blind = check_value('auto_blind') ? true : false;
    $generated = auto_generate_due_counts($blind);
    if ($generated > 0)
        display_notification(sprintf(_('%d count sessions auto-generated from due plans.'), $generated));
    else
        display_notification(_('No plans are currently due for counting.'));
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Rendering
//----------------------------------------------------------------------

start_form();

// Tab switcher
echo '<div style="margin-bottom:10px;">';
echo '<button type="submit" name="tab_counts" value="1" class="ajaxsubmit" '
    . 'style="padding:8px 20px;margin-right:5px;font-weight:600;border:1px solid #ccc;cursor:pointer;'
    . ($active_tab == 'counts' ? 'background:#007bff;color:#fff;border-color:#007bff;' : 'background:#f8f9fa;color:#333;')
    . 'border-radius:4px 4px 0 0;">'
    . _('Count Sessions') . '</button>';
echo '<button type="submit" name="tab_plans" value="1" class="ajaxsubmit" '
    . 'style="padding:8px 20px;margin-right:5px;font-weight:600;border:1px solid #ccc;cursor:pointer;'
    . ($active_tab == 'plans' ? 'background:#007bff;color:#fff;border-color:#007bff;' : 'background:#f8f9fa;color:#333;')
    . 'border-radius:4px 4px 0 0;">'
    . _('Count Plans') . '</button>';
echo '</div>';

hidden('tab', $active_tab);

//======================================================================
// TAB: Count Plans
//======================================================================
if ($active_tab == 'plans') {

    // ----- Plans List -----
    div_start('plans_list');
    display_heading2(_('Cycle Count Plans'));

    $filter_method = get_post('filter_plan_method');
    $plans = get_cycle_count_plans(null, ($filter_method && $filter_method != '') ? $filter_method : null, true);

    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('#'), _('Plan Name'), _('Method'), _('Warehouse'), _('Frequency'),
        _('ABC Class'), _('Last Count'), _('Next Count'), _('Counts'), _('Active'), '');
    table_header($th);

    $k = 0;
    while ($plan = db_fetch($plans)) {
        alt_table_row_color($k);
        label_cell($plan['plan_id']);
        label_cell($plan['plan_name']);
        label_cell(count_method_badge($plan['count_method']));
        label_cell($plan['location_name'] ? $plan['location_name'] : _('All Warehouses'));
        label_cell($plan['frequency_days'] . ' ' . _('days'));
        label_cell($plan['abc_class'] ? $plan['abc_class'] : '-');
        label_cell($plan['last_count_date'] ? sql2date($plan['last_count_date']) : '-');

        // Highlight overdue plans
        $next_date = $plan['next_count_date'];
        if ($next_date && $next_date <= date('Y-m-d')) {
            label_cell('<span style="color:#dc3545;font-weight:600;">' . sql2date($next_date) . ' (' . _('Due') . ')</span>');
        } else {
            label_cell($next_date ? sql2date($next_date) : '-');
        }

        label_cell($plan['total_counts'], 'align=center');
        label_cell($plan['active'] ? _('Yes') : '<span style="color:#999;">' . _('No') . '</span>');

        // Action buttons
        echo '<td nowrap>';
        echo '<button type="submit" name="PlanEdit[' . $plan['plan_id'] . ']" class="ajaxsubmit"'
            . ' style="padding:2px 8px;margin:1px;cursor:pointer;">' . _('Edit') . '</button>';
        echo '<button type="submit" name="PlanDelete[' . $plan['plan_id'] . ']" class="ajaxsubmit"'
            . ' style="padding:2px 8px;margin:1px;cursor:pointer;color:#dc3545;">' . _('Delete') . '</button>';
        echo '</td>';

        end_row();
    }

    end_table(1);

    // Auto-generate section
    echo '<div style="margin:10px 0;padding:10px;background:#f0f8ff;border:1px solid #b8daff;border-radius:4px;">';
    $due_count = get_due_plans_count();
    echo '<b>' . _('Auto-Generate:') . '</b> ';
    echo sprintf(_('%d plan(s) are due for counting.'), $due_count) . ' &nbsp;';
    echo '<label><input type="checkbox" name="auto_blind" value="1"> ' . _('Blind count mode') . '</label> &nbsp;';
    echo '<button type="submit" name="AutoGenerate" class="ajaxsubmit" '
        . 'style="padding:4px 12px;cursor:pointer;">' . _('Generate Due Counts') . '</button>';
    echo '</div>';

    div_end();

    // ----- Plan Add/Edit Form -----
    div_start('plan_form');

    if ($plan_mode == 'edit' && $plan_selected_id > 0) {
        $plan_data = get_cycle_count_plan($plan_selected_id);
        display_heading2(_('Edit Count Plan'));
    } else {
        $plan_data = null;
        display_heading2(_('New Count Plan'));
    }

    start_table(TABLESTYLE2);

    text_row(_('Plan Name:'), 'plan_name', $plan_data ? $plan_data['plan_name'] : '', 40, 100);

    $methods = get_count_methods();
    array_selector_row(_('Count Method:'), 'count_method',
        $plan_data ? $plan_data['count_method'] : 'full', $methods);

    // Warehouse selector
    $wh_sql = "SELECT loc_code, location_name, inactive FROM " . TB_PREF . "locations WHERE wh_enabled = 1";
    echo '<tr><td class="label">' . _('Warehouse:') . '</td><td>';
    echo combo_input('plan_warehouse', $plan_data ? $plan_data['warehouse_loc_code'] : '',
        $wh_sql, 'loc_code', 'location_name',
        array('spec_option' => _('All Warehouses'), 'spec_id' => '', 'order' => false));
    echo '</td></tr>';

    small_amount_row(_('Frequency (days):'), 'frequency_days',
        $plan_data ? $plan_data['frequency_days'] : 30, null, null, 0);

    $abc_options = array('' => _('N/A'), 'A' => _('Class A'), 'B' => _('Class B'), 'C' => _('Class C'));
    array_selector_row(_('ABC Class Filter:'), 'plan_abc_class',
        $plan_data ? ($plan_data['abc_class'] ? $plan_data['abc_class'] : '') : '', $abc_options);

    if ($plan_data) {
        check_row(_('Active:'), 'plan_active', $plan_data['active']);
        hidden('plan_selected_id', $plan_selected_id);
    }

    end_table(0);

    echo '<div style="text-align:center;margin:10px 0;">';
    if ($plan_data) {
        echo '<button type="submit" name="UpdatePlan" class="ajaxsubmit" '
            . 'style="padding:5px 15px;cursor:pointer;">' . _('Update Plan') . '</button> ';
        echo '<button type="submit" name="CancelPlan" class="ajaxsubmit" '
            . 'style="padding:5px 15px;cursor:pointer;">' . _('Cancel') . '</button>';
    } else {
        echo '<button type="submit" name="AddPlan" class="ajaxsubmit" '
            . 'style="padding:5px 15px;cursor:pointer;">' . _('Add Plan') . '</button>';
    }
    echo '</div>';

    div_end();

//======================================================================
// TAB: Count Sessions
//======================================================================
} else {

    // ----- Summary Cards -----
    $wh_filter = get_post('filter_warehouse');
    $status_filter = get_post('filter_status');

    $summary = get_count_status_summary($wh_filter && $wh_filter != '' ? $wh_filter : null);

    echo '<div style="display:flex;flex-wrap:wrap;gap:10px;margin-bottom:15px;">';
    $cards = array(
        array('Draft', $summary['draft'], '#6c757d'),
        array('In Progress', $summary['in_progress'], '#17a2b8'),
        array('Review', $summary['review'], '#ffc107'),
        array('Approved', $summary['approved'], '#28a745'),
        array('Posted', $summary['posted'], '#007bff'),
    );
    foreach ($cards as $c) {
        echo '<div style="flex:1;min-width:120px;padding:12px;background:' . $c[2] . ';color:#fff;'
            . 'border-radius:6px;text-align:center;">';
        echo '<div style="font-size:24px;font-weight:700;">' . $c[1] . '</div>';
        echo '<div style="font-size:12px;">' . _($c[0]) . '</div>';
        echo '</div>';
    }
    echo '</div>';

    // ----- Filters -----
    echo '<div style="margin-bottom:10px;padding:8px;background:#f8f9fa;border-radius:4px;">';
    echo '<b>' . _('Filters:') . '</b> &nbsp;';

    // Warehouse
    $wh_sql = "SELECT loc_code, location_name, inactive FROM " . TB_PREF . "locations WHERE wh_enabled = 1";
    echo combo_input('filter_warehouse', $wh_filter,
        $wh_sql, 'loc_code', 'location_name',
        array('spec_option' => _('All Warehouses'), 'spec_id' => '', 'order' => false,
              'select_submit' => true));
    echo ' &nbsp;';

    // Status
    $stat_options = array('' => _('All Statuses'), 'active' => _('Active (Draft/In Progress/Review)'),
        'draft' => _('Draft'), 'in_progress' => _('In Progress'), 'review' => _('Review'),
        'approved' => _('Approved'), 'posted' => _('Posted'));
    echo array_selector('filter_status', $status_filter, $stat_options,
        array('select_submit' => true));
    echo '</div>';

    // Determine status filter for query
    $query_status = null;
    if ($status_filter == 'active')
        $query_status = array('draft', 'in_progress', 'review');
    elseif ($status_filter && $status_filter != '')
        $query_status = $status_filter;

    // ----- Count Sessions List -----
    div_start('counts_list');

    $counts = get_cycle_counts(
        ($wh_filter && $wh_filter != '') ? $wh_filter : null,
        $query_status, null, 100
    );

    start_table(TABLESTYLE, "width='100%'");
    $th = array(_('#'), _('Date'), _('Warehouse'), _('Plan'), _('Status'),
        _('Lines'), _('Progress'), _('Variances'), '');
    table_header($th);

    $k = 0;
    while ($row = db_fetch($counts)) {
        alt_table_row_color($k);
        // ID as link to view page
        label_cell('<a href="' . $path_to_root . '/inventory/warehouse/view/view_cycle_count.php?count_id='
            . $row['count_id'] . '">#' . $row['count_id'] . '</a>');
        label_cell(sql2date($row['count_date']));
        label_cell($row['location_name']);
        label_cell($row['plan_name'] ? $row['plan_name'] : '-');
        label_cell(count_status_badge($row['status']));
        label_cell($row['total_lines'], 'align=center');

        // Progress bar
        $total = intval($row['total_lines']);
        $counted = intval($row['counted_lines']);
        $pct = $total > 0 ? round(($counted / $total) * 100) : 0;
        $bar_color = $pct == 100 ? '#28a745' : ($pct > 50 ? '#17a2b8' : '#ffc107');
        echo '<td>';
        echo '<div style="width:100px;background:#e9ecef;border-radius:4px;overflow:hidden;display:inline-block;vertical-align:middle;">';
        echo '<div style="width:' . $pct . '%;background:' . $bar_color . ';height:16px;"></div>';
        echo '</div> ';
        echo '<span style="font-size:11px;">' . $counted . '/' . $total . ' (' . $pct . '%)</span>';
        echo '</td>';

        // Variance summary
        if ($row['status'] != 'draft' && $total > 0) {
            $var_summary = get_count_variance_summary($row['count_id']);
            if ($var_summary['variance_lines'] > 0) {
                echo '<td style="color:#dc3545;font-weight:600;">'
                    . $var_summary['variance_lines'] . ' ' . _('lines')
                    . '</td>';
            } else {
                echo '<td style="color:#28a745;">' . _('None') . '</td>';
            }
        } else {
            echo '<td>-</td>';
        }

        // Action buttons
        echo '<td nowrap>';
        $st = $row['status'];
        $cid = $row['count_id'];

        if ($st == 'draft') {
            echo '<button type="submit" name="StartCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;background:#17a2b8;color:#fff;border:none;border-radius:3px;">'
                . _('Start') . '</button>';
            edit_button_cell("Edit$cid", _('Edit'));
            echo '<button type="submit" name="CancelCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;color:#dc3545;">'
                . _('Delete') . '</button>';
        } elseif ($st == 'in_progress') {
            echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_cycle_count.php?count_id='
                . $cid . '&enter=1" style="padding:2px 8px;margin:1px;background:#ffc107;color:#000;'
                . 'text-decoration:none;border-radius:3px;font-size:12px;">'
                . _('Enter Counts') . '</a> ';
            echo '<button type="submit" name="SubmitReview[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;background:#28a745;color:#fff;border:none;border-radius:3px;">'
                . _('Submit') . '</button>';
            echo '<button type="submit" name="CancelCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;color:#dc3545;">'
                . _('Cancel') . '</button>';
        } elseif ($st == 'review') {
            echo '<button type="submit" name="ApproveCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;background:#28a745;color:#fff;border:none;border-radius:3px;">'
                . _('Approve') . '</button>';
            echo '<button type="submit" name="RecountCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;background:#ffc107;color:#000;border:none;border-radius:3px;">'
                . _('Recount') . '</button>';
            echo '<button type="submit" name="CancelCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;color:#dc3545;">'
                . _('Cancel') . '</button>';
        } elseif ($st == 'approved') {
            echo '<button type="submit" name="PostCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;background:#007bff;color:#fff;border:none;border-radius:3px;">'
                . _('Post Adjustments') . '</button>';
            echo '<button type="submit" name="RecountCount[' . $cid . ']" class="ajaxsubmit"'
                . ' style="padding:2px 8px;margin:1px;cursor:pointer;background:#ffc107;color:#000;border:none;border-radius:3px;">'
                . _('Recount') . '</button>';
        } elseif ($st == 'posted') {
            echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_cycle_count.php?count_id='
                . $cid . '" style="padding:2px 8px;text-decoration:none;color:#007bff;">'
                . _('View') . '</a>';
        }
        echo '</td>';

        end_row();

        // Expandable detail: show variance lines for review/approved/posted
        if (in_array($st, array('review', 'approved', 'posted'))) {
            $var_lines_result = get_cycle_count_lines($cid, true);
            $var_count = 0;
            $var_rows = array();
            while ($vl = db_fetch($var_lines_result)) {
                $var_rows[] = $vl;
                $var_count++;
            }
            if ($var_count > 0) {
                echo '<tr><td colspan="9" style="padding:5px 20px;background:#fff8f8;">';
                echo '<b style="color:#dc3545;">' . _('Variance Lines:') . '</b>';
                echo '<table style="width:100%;font-size:12px;margin-top:4px;">';
                echo '<tr style="background:#f5f5f5;"><th>' . _('Bin') . '</th><th>' . _('Item') . '</th>'
                    . '<th>' . _('System') . '</th><th>' . _('Counted') . '</th><th>' . _('Variance') . '</th>'
                    . '<th>' . _('Batch') . '</th><th>' . _('Serial') . '</th></tr>';
                foreach ($var_rows as $vl) {
                    $vcolor = $vl['variance_qty'] > 0 ? '#28a745' : '#dc3545';
                    echo '<tr>';
                    echo '<td>' . $vl['bin_code'] . '</td>';
                    echo '<td>' . $vl['stock_id'] . ' - ' . $vl['item_description'] . '</td>';
                    echo '<td align="right">' . number_format2($vl['system_qty'], get_qty_dec($vl['stock_id'])) . '</td>';
                    echo '<td align="right">' . number_format2($vl['counted_qty'], get_qty_dec($vl['stock_id'])) . '</td>';
                    echo '<td align="right" style="color:' . $vcolor . ';font-weight:600;">'
                        . ($vl['variance_qty'] > 0 ? '+' : '') . number_format2($vl['variance_qty'], get_qty_dec($vl['stock_id']))
                        . '</td>';
                    echo '<td>' . ($vl['batch_no'] ? $vl['batch_no'] : '-') . '</td>';
                    echo '<td>' . ($vl['serial_no'] ? $vl['serial_no'] : '-') . '</td>';
                    echo '</tr>';
                }
                echo '</table></td></tr>';
            }
        }
    }

    end_table(1);
    div_end();

    // ----- New Count Session Form -----
    if ($selected_id == -1) {
        div_start('count_form');
        display_heading2(_('New Count Session'));

        start_table(TABLESTYLE2);

        date_row(_('Count Date:'), 'count_date', '', null, 0, 0, 0);

        // Warehouse selector
        $wh_sql2 = "SELECT loc_code, location_name, inactive FROM " . TB_PREF . "locations WHERE wh_enabled = 1";
        echo '<tr><td class="label">' . _('Warehouse:') . '</td><td>';
        echo combo_input('count_warehouse', '',
            $wh_sql2, 'loc_code', 'location_name',
            array('order' => false));
        echo '</td></tr>';

        // Plan selector (optional)
        $plan_sql = "SELECT plan_id, plan_name, (1-active) AS inactive FROM " . TB_PREF . "wh_cycle_count_plans WHERE active = 1";
        echo '<tr><td class="label">' . _('From Plan (optional):') . '</td><td>';
        echo combo_input('count_plan_id', '',
            $plan_sql, 'plan_id', 'plan_name',
            array('spec_option' => _('No plan (manual)'), 'spec_id' => '', 'order' => false));
        echo '</td></tr>';

        check_row(_('Blind Count (hide system qty):'), 'blind_mode', 0);
        textarea_row(_('Notes:'), 'count_memo', '', 40, 3);

        end_table(0);

        submit_add_or_update_center($selected_id == -1, '', true);

        div_end();
    } else {
        // Edit mode
        $edit_count = get_cycle_count($selected_id);
        if ($edit_count && $edit_count['status'] == 'draft') {
            div_start('count_form');
            display_heading2(_('Edit Count Session'));

            start_table(TABLESTYLE2);

            $_POST['count_date'] = sql2date($edit_count['count_date']);
            date_row(_('Count Date:'), 'count_date', '', null, 0, 0, 0);

            $wh_sql2 = "SELECT loc_code, location_name, inactive FROM " . TB_PREF . "locations WHERE wh_enabled = 1";
            echo '<tr><td class="label">' . _('Warehouse:') . '</td><td>';
            echo combo_input('count_warehouse', $edit_count['warehouse_loc_code'],
                $wh_sql2, 'loc_code', 'location_name',
                array('order' => false));
            echo '</td></tr>';

            $plan_sql = "SELECT plan_id, plan_name, (1-active) AS inactive FROM " . TB_PREF . "wh_cycle_count_plans WHERE active = 1";
            echo '<tr><td class="label">' . _('From Plan (optional):') . '</td><td>';
            echo combo_input('count_plan_id', $edit_count['plan_id'] ? $edit_count['plan_id'] : '',
                $plan_sql, 'plan_id', 'plan_name',
                array('spec_option' => _('No plan (manual)'), 'spec_id' => '', 'order' => false));
            echo '</td></tr>';

            textarea_row(_('Notes:'), 'count_memo', $edit_count['memo'], 40, 3);

            end_table(0);

            submit_add_or_update_center(false, '', true);

            div_end();
        }
    }
}

end_form();
end_page();
