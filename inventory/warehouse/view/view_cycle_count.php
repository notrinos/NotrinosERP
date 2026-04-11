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
 * Cycle Count Detail / Count Entry View
 *
 * Displays count header, variance summary, and count lines.
 * In enter mode (?enter=1), allows entering counted quantities.
 *
 * Session 16 of UNIFIED_INVENTORY_IMPLEMENTATION_PLAN.md
 */

$page_security = 'SA_WAREHOUSE_CYCLE_COUNT';
$path_to_root = '../../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_counting_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

$count_id = isset($_GET['count_id']) ? intval($_GET['count_id']) : intval(get_post('count_id'));
$enter_mode = (isset($_GET['enter']) && $_GET['enter'] == '1') || get_post('enter_mode');

$count = get_cycle_count($count_id);
if (!$count) {
    display_error(_('Count session not found.'));
    page(_('Cycle Count'), true, false, '', $js);
    end_page();
    exit;
}

$title = _('Cycle Count') . ' #' . $count_id;
if ($enter_mode)
    $title .= ' — ' . _('Enter Counts');

page(_($help_context = $title), true, false, '', $js);

//----------------------------------------------------------------------
// Handle count entry saves (when in enter mode)
//----------------------------------------------------------------------
if (isset($_POST['SaveCounts'])) {
    $saved = 0;
    $lines = get_cycle_count_lines($count_id);
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
    // Refresh count data
    $count = get_cycle_count($count_id);
    $Ajax->activate('_page_body');
}

// Handle add manual line
if (isset($_POST['AddManualLine'])) {
    if ($count['status'] == 'in_progress') {
        $manual_bin = intval(get_post('manual_bin_id'));
        $manual_stock = get_post('manual_stock_id');
        $manual_qty = floatval(get_post('manual_counted_qty'));

        if (!$manual_bin || !$manual_stock) {
            display_error(_('Please select both a bin and an item for the manual line.'));
        } elseif ($manual_qty < 0) {
            display_error(_('Counted quantity cannot be negative.'));
        } else {
            add_manual_count_line($count_id, $manual_bin, $manual_stock, $manual_qty,
                null, null, _('Manually added — unexpected item'));
            display_notification(_('Manual count line added.'));
            $count = get_cycle_count($count_id);
        }
    }
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Header Info
//----------------------------------------------------------------------
echo '<div style="max-width:1000px;margin:0 auto;">';

start_table(TABLESTYLE2, "width='60%'");
label_row(_('Count #:'), '<b>' . $count_id . '</b>');
label_row(_('Date:'), sql2date($count['count_date']));
label_row(_('Warehouse:'), $count['location_name']);
label_row(_('Plan:'), $count['plan_name'] ? $count['plan_name'] : _('Manual count'));
label_row(_('Status:'), count_status_badge($count['status']));
if ($count['counted_by'])
    label_row(_('Counted By:'), $count['counted_by']);
if ($count['approved_by'])
    label_row(_('Approved By:'), $count['approved_by']);
if ($count['memo'])
    label_row(_('Notes:'), $count['memo']);
end_table(1);

//----------------------------------------------------------------------
// Variance Summary Cards (only when counting has started)
//----------------------------------------------------------------------
if ($count['status'] != 'draft') {
    $var_summary = get_count_variance_summary($count_id);
    $total_lines = intval($count['total_lines']);
    $counted = intval($count['counted_lines']);

    echo '<div style="display:flex;flex-wrap:wrap;gap:10px;margin:10px 0;">';

    echo '<div style="flex:1;min-width:120px;padding:10px;background:#007bff;color:#fff;'
        . 'border-radius:6px;text-align:center;">';
    echo '<div style="font-size:20px;font-weight:700;">' . $total_lines . '</div>';
    echo '<div style="font-size:11px;">' . _('Total Lines') . '</div>';
    echo '</div>';

    echo '<div style="flex:1;min-width:120px;padding:10px;background:'
        . ($counted == $total_lines ? '#28a745' : '#17a2b8') . ';color:#fff;'
        . 'border-radius:6px;text-align:center;">';
    echo '<div style="font-size:20px;font-weight:700;">' . $counted . '</div>';
    echo '<div style="font-size:11px;">' . _('Counted') . '</div>';
    echo '</div>';

    echo '<div style="flex:1;min-width:120px;padding:10px;background:'
        . ($var_summary['variance_lines'] > 0 ? '#dc3545' : '#28a745') . ';color:#fff;'
        . 'border-radius:6px;text-align:center;">';
    echo '<div style="font-size:20px;font-weight:700;">' . intval($var_summary['variance_lines']) . '</div>';
    echo '<div style="font-size:11px;">' . _('With Variance') . '</div>';
    echo '</div>';

    echo '<div style="flex:1;min-width:120px;padding:10px;background:#6c757d;color:#fff;'
        . 'border-radius:6px;text-align:center;">';
    echo '<div style="font-size:20px;font-weight:700;">'
        . number_format2(abs($var_summary['total_variance_qty']), 2) . '</div>';
    echo '<div style="font-size:11px;">' . _('Total Variance Qty') . '</div>';
    echo '</div>';

    echo '<div style="flex:1;min-width:120px;padding:10px;background:#6c757d;color:#fff;'
        . 'border-radius:6px;text-align:center;">';
    echo '<div style="font-size:20px;font-weight:700;">'
        . number_format2(abs($var_summary['total_variance_value']), user_price_dec()) . '</div>';
    echo '<div style="font-size:11px;">' . _('Total Variance Value') . '</div>';
    echo '</div>';

    echo '</div>';
}

//----------------------------------------------------------------------
// Count Lines
//----------------------------------------------------------------------
if ($enter_mode && $count['status'] == 'in_progress') {
    // Entry mode form
    start_form();
    hidden('count_id', $count_id);
    hidden('enter_mode', '1');
    hidden('counting_count_id', $count_id);
}

$lines = get_cycle_count_lines($count_id);

// Determine blind mode by checking if all system_qty are 0 on a draft that was generated blind
$blind = false;
if ($count['status'] == 'in_progress') {
    // Check if first line has system_qty visible = 0 and there is actual bin_stock
    // We use a simpler approach: look at count plan or just check if system_qty shown
    // For now, we show system qty only when not in enter_mode OR when count is past in_progress
    $blind = $enter_mode; // In enter mode, we'll check if system_qty is all 0
}

start_table(TABLESTYLE, "width='95%'");

if ($enter_mode && $count['status'] == 'in_progress') {
    $th = array(_('#'), _('Bin'), _('Item'), _('Description'), _('System Qty'),
        _('Counted Qty'), _('Variance'), _('Batch'), _('Serial'), _('Notes'));
} else {
    $th = array(_('#'), _('Bin'), _('Item'), _('Description'), _('System Qty'),
        _('Counted Qty'), _('Variance'), _('Batch'), _('Serial'), _('Notes'), _('Posted'));
}
table_header($th);

$k = 0;
$line_num = 0;
$all_blind = true;
while ($line = db_fetch($lines)) {
    $line_num++;
    alt_table_row_color($k);

    label_cell($line_num);
    label_cell($line['bin_code']);
    label_cell($line['stock_id']);
    label_cell($line['item_description']);

    // System qty — respect blind mode
    $sys_qty = floatval($line['system_qty']);
    $dec = get_qty_dec($line['stock_id']);

    if ($sys_qty != 0) $all_blind = false;

    if ($enter_mode && $count['status'] == 'in_progress' && $sys_qty == 0 && !$line['counted_qty']) {
        // Possibly blind mode — show "Hidden"
        label_cell('<span style="color:#999;font-style:italic;">' . _('Hidden') . '</span>', 'align=right');
    } else {
        label_cell(number_format2($sys_qty, $dec), 'align=right');
    }

    // Counted qty
    if ($enter_mode && $count['status'] == 'in_progress') {
        echo '<td align="right">';
        echo '<input type="text" name="counted_' . $line['line_id'] . '" '
            . 'value="' . ($line['counted_qty'] !== null ? number_format2($line['counted_qty'], $dec) : '') . '" '
            . 'size="10" style="text-align:right;padding:2px 4px;">';
        echo '</td>';
    } else {
        if ($line['counted_qty'] !== null) {
            label_cell(number_format2($line['counted_qty'], $dec), 'align=right');
        } else {
            label_cell('<span style="color:#999;">—</span>', 'align=right');
        }
    }

    // Variance
    if ($line['counted_qty'] !== null) {
        $variance = floatval($line['variance_qty']);
        if (abs($variance) > 0.001) {
            $vcolor = $variance > 0 ? '#28a745' : '#dc3545';
            label_cell('<span style="color:' . $vcolor . ';font-weight:600;">'
                . ($variance > 0 ? '+' : '') . number_format2($variance, $dec)
                . '</span>', 'align=right');
        } else {
            label_cell('<span style="color:#28a745;">0</span>', 'align=right');
        }
    } else {
        label_cell('-', 'align=right');
    }

    // Batch / Serial
    label_cell($line['batch_no'] ? $line['batch_no'] : '-');
    label_cell($line['serial_no'] ? $line['serial_no'] : '-');

    // Notes
    if ($enter_mode && $count['status'] == 'in_progress') {
        echo '<td>';
        echo '<input type="text" name="line_memo_' . $line['line_id'] . '" '
            . 'value="' . htmlspecialchars($line['memo'] ? $line['memo'] : '') . '" '
            . 'size="15" style="padding:2px 4px;">';
        echo '</td>';
    } else {
        label_cell($line['memo'] ? $line['memo'] : '');
        // Posted indicator
        label_cell($line['adjustment_posted'] ? '<span style="color:#28a745;">&#10003;</span>' : '-', 'align=center');
    }

    end_row();
}

end_table(1);

// Count entry buttons
if ($enter_mode && $count['status'] == 'in_progress') {
    echo '<div style="text-align:center;margin:10px 0;">';
    echo '<button type="submit" name="SaveCounts" class="ajaxsubmit" '
        . 'style="padding:8px 25px;cursor:pointer;background:#28a745;color:#fff;border:none;border-radius:4px;font-size:14px;">'
        . _('Save Counts') . '</button>';
    echo '</div>';

    // Manual line section
    echo '<div style="margin:15px 0;padding:10px;background:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;">';
    echo '<b>' . _('Add Unexpected Item:') . '</b><br>';

    // Bin selector
    $bin_sql = "SELECT wl.loc_id, CONCAT(wl.loc_code, ' - ', wl.loc_name) AS display_name, (1 - wl.is_active) AS inactive
        FROM " . TB_PREF . "wh_locations wl
        INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
        WHERE wl.warehouse_loc_code = " . db_escape($count['warehouse_loc_code']) . "
        AND lt.can_store = 1 ORDER BY wl.loc_code";
    echo _('Bin:') . ' ';
    echo combo_input('manual_bin_id', '', $bin_sql, 'loc_id', 'display_name',
        array('order' => false));
    echo ' &nbsp;';

    // Item selector
    echo _('Item:') . ' ';
    stock_items_list('manual_stock_id', '', false, true);
    echo ' &nbsp;';

    echo _('Counted Qty:') . ' ';
    echo '<input type="text" name="manual_counted_qty" value="" size="8" style="text-align:right;padding:2px 4px;"> ';

    echo '<button type="submit" name="AddManualLine" class="ajaxsubmit" '
        . 'style="padding:4px 12px;cursor:pointer;">' . _('Add Line') . '</button>';
    echo '</div>';

    end_form();
}

//----------------------------------------------------------------------
// Navigation buttons
//----------------------------------------------------------------------
echo '<div style="text-align:center;margin:15px 0;">';

echo '<a href="' . $path_to_root . '/inventory/warehouse/cycle_counts.php?" '
    . 'style="padding:8px 20px;background:#6c757d;color:#fff;text-decoration:none;border-radius:4px;margin:0 5px;">'
    . _('Back to Cycle Counts') . '</a>';

if (!$enter_mode && $count['status'] == 'in_progress') {
    echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_cycle_count.php?count_id='
        . $count_id . '&enter=1" '
        . 'style="padding:8px 20px;background:#ffc107;color:#000;text-decoration:none;border-radius:4px;margin:0 5px;">'
        . _('Enter Counts') . '</a>';
}

echo '<a href="javascript:window.print();" '
    . 'style="padding:8px 20px;background:#17a2b8;color:#fff;text-decoration:none;border-radius:4px;margin:0 5px;">'
    . _('Print') . '</a>';

echo '</div>';

echo '</div>'; // Close max-width wrapper

echo '<style>
@media print {
    .navbar, .fa_banner, .company_logo_area, .company_name_area, div[class*="sidebar"],
    a[href*="cycle_counts.php"], a[href*="enter=1"], a[href*="window.print"],
    button, input[type="submit"], .ajaxsubmit { display: none !important; }
    body { font-size: 11pt; }
    table { page-break-inside: auto; }
    tr { page-break-inside: avoid; }
}
</style>';

end_page(true);
