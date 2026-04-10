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
 * Expiry Dashboard — overview of batch expiry status across all items.
 *
 * Features:
 *   - Summary cards (expired, critical, warning, ok, no expiry)
 *   - Expiring soon list with days-until-expiry
 *   - Already-expired batches still marked active (action list)
 *   - Aging analysis by category
 *   - Filter by item
 */
$page_security = 'SA_BATCHINQUIRY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Expiry Dashboard');

page($_SESSION['page_title'], false, false, '', '');

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');

//----------------------------------------------------------------------
// Handle AUTO-EXPIRE
//----------------------------------------------------------------------
start_form();

if (isset($_POST['auto_expire']) && $_POST['auto_expire'] != '') {
	$expired_count = auto_expire_batches();
	if ($expired_count > 0)
		display_notification(sprintf(_('%d batches have been marked as expired.'), $expired_count));
	else
		display_note(_('No active batches found with past expiry dates.'));
}

// Filter
if (list_updated('filter_stock_id'))
	$Ajax->activate('_page_body');

//======================================================================
//  F I L T E R
//======================================================================

start_table(TABLESTYLE_NOBORDER);
start_row();
batch_tracked_items_list_cells(_('Item:'), 'filter_stock_id', get_post('filter_stock_id'), true, true);
submit_cells('auto_expire', _('Auto-Expire Past Batches'), '', _('Mark all past-expiry active batches as expired'), true);
end_row();
end_table();

$filter_stock = get_post('filter_stock_id', '');

//======================================================================
//  S U M M A R Y   C A R D S
//======================================================================

$summary = get_batch_expiry_summary($filter_stock);
$total_active = $summary['expired'] + $summary['critical'] + $summary['warning'] + $summary['ok'] + $summary['no_expiry'];

echo "<h3 style='margin:15px 0 5px 0;'>" . _('Expiry Overview') . "</h3>";
echo "<div style='display:flex;flex-wrap:wrap;gap:10px;margin:10px 0 20px 0;'>";

// Expired card
echo "<div style='flex:1;min-width:140px;max-width:200px;padding:15px;border-radius:6px;background:#dc3545;color:#fff;text-align:center;'>";
echo "<div style='font-size:28px;font-weight:bold;'>" . $summary['expired'] . "</div>";
echo "<div style='font-size:12px;'>" . _('Expired') . "</div>";
echo "</div>";

// Critical card
echo "<div style='flex:1;min-width:140px;max-width:200px;padding:15px;border-radius:6px;background:#fd7e14;color:#fff;text-align:center;'>";
echo "<div style='font-size:28px;font-weight:bold;'>" . $summary['critical'] . "</div>";
echo "<div style='font-size:12px;'>" . _('Critical (≤30 days)') . "</div>";
echo "</div>";

// Warning card
echo "<div style='flex:1;min-width:140px;max-width:200px;padding:15px;border-radius:6px;background:#ffc107;color:#333;text-align:center;'>";
echo "<div style='font-size:28px;font-weight:bold;'>" . $summary['warning'] . "</div>";
echo "<div style='font-size:12px;'>" . _('Warning') . "</div>";
echo "</div>";

// OK card
echo "<div style='flex:1;min-width:140px;max-width:200px;padding:15px;border-radius:6px;background:#28a745;color:#fff;text-align:center;'>";
echo "<div style='font-size:28px;font-weight:bold;'>" . $summary['ok'] . "</div>";
echo "<div style='font-size:12px;'>" . _('OK') . "</div>";
echo "</div>";

// No expiry card
echo "<div style='flex:1;min-width:140px;max-width:200px;padding:15px;border-radius:6px;background:#6c757d;color:#fff;text-align:center;'>";
echo "<div style='font-size:28px;font-weight:bold;'>" . $summary['no_expiry'] . "</div>";
echo "<div style='font-size:12px;'>" . _('No Expiry Date') . "</div>";
echo "</div>";

echo "</div>";

//======================================================================
//  A C T I O N   L I S T  —  Expired batches still active
//======================================================================

$expired_active = get_expired_active_batches($filter_stock, 25);
$has_expired = false;

echo "<h3 style='margin:15px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
	. _('Action Required — Expired Batches Still Active') . "</h3>";

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Batch #'), _('Item'), _('Expiry Date'), _('Days Past'), _('Initial Qty'),
	_('Supplier'), '');
table_header($th);

$k = 0;
while ($row = db_fetch($expired_active)) {
	$has_expired = true;
	alt_table_row_color($k);

	$batch_link = '<a href="' . $path_to_root . '/inventory/inquiry/batch_inquiry.php?batch_id='
		. $row['id'] . '">' . $row['batch_no'] . '</a>';
	label_cell($batch_link);
	label_cell($row['stock_id'] . ' - ' . $row['item_description']);
	label_cell(batch_expiry_badge($row['expiry_date']));
	label_cell('<span style="color:red;font-weight:bold;">' . $row['days_past_expiry'] . '</span>', "align='right'");
	label_cell($row['initial_qty'] > 0 ? number_format2($row['initial_qty'], 2) : '-', "align='right'");
	label_cell($row['supplier_name'] ? $row['supplier_name'] : '-');

	// Edit link
	$edit_link = '<a href="' . $path_to_root . '/inventory/manage/stock_batches.php?stock_id='
		. urlencode($row['stock_id']) . '">' . _('Manage') . '</a>';
	label_cell($edit_link);

	end_row();
}

end_table();

if (!$has_expired)
	display_note(_('No expired batches require action. All clear!'), 0, 0, "style='color:green;'");

//======================================================================
//  E X P I R I N G   S O O N   L I S T
//======================================================================

$warning_days = (int)get_company_pref('expiry_warning_days');
if ($warning_days <= 0)
	$warning_days = 90;

echo "<h3 style='margin:20px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
	. sprintf(_('Expiring Within %d Days'), $warning_days) . "</h3>";

$expiring = get_expiring_batches($warning_days, $filter_stock, 50);
$has_expiring = false;

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Batch #'), _('Item'), _('Status'), _('Expiry Date'), _('Days Left'),
	_('Initial Qty'), _('Supplier'), '');
table_header($th);

$k = 0;
while ($row = db_fetch($expiring)) {
	$has_expiring = true;
	alt_table_row_color($k);

	$batch_link = '<a href="' . $path_to_root . '/inventory/inquiry/batch_inquiry.php?batch_id='
		. $row['id'] . '">' . $row['batch_no'] . '</a>';
	label_cell($batch_link);
	label_cell($row['stock_id'] . ' - ' . $row['item_description']);
	label_cell(batch_status_badge($row['status']));
	label_cell(batch_expiry_badge($row['expiry_date']));

	$days = (int)$row['days_until_expiry'];
	if ($days <= 30)
		label_cell('<span style="color:red;font-weight:bold;">' . $days . '</span>', "align='right'");
	else
		label_cell('<span style="color:orange;">' . $days . '</span>', "align='right'");

	label_cell($row['initial_qty'] > 0 ? number_format2($row['initial_qty'], 2) : '-', "align='right'");
	label_cell($row['supplier_name'] ? $row['supplier_name'] : '-');

	$view_link = '<a href="' . $path_to_root . '/inventory/inquiry/batch_inquiry.php?batch_id='
		. $row['id'] . '">' . _('View') . '</a>';
	label_cell($view_link);

	end_row();
}

end_table();

if (!$has_expiring)
	display_note(sprintf(_('No batches expiring within %d days.'), $warning_days), 0, 0, "style='color:green;'");

//======================================================================
//  A G I N G   A N A L Y S I S  (by item)
//======================================================================

echo "<h3 style='margin:20px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
	. _('Batch Aging Analysis by Item') . "</h3>";

$aging_sql = "SELECT sm.stock_id, sm.description,
		SUM(CASE WHEN b.expiry_date IS NOT NULL AND b.expiry_date < CURDATE() THEN 1 ELSE 0 END) AS expired_count,
		SUM(CASE WHEN b.expiry_date IS NOT NULL AND b.expiry_date >= CURDATE() AND b.expiry_date <= DATE_ADD(CURDATE(), INTERVAL 30 DAY) THEN 1 ELSE 0 END) AS critical_count,
		SUM(CASE WHEN b.expiry_date IS NOT NULL AND b.expiry_date > DATE_ADD(CURDATE(), INTERVAL 30 DAY) AND b.expiry_date <= DATE_ADD(CURDATE(), INTERVAL " . $warning_days . " DAY) THEN 1 ELSE 0 END) AS warning_count,
		SUM(CASE WHEN b.expiry_date IS NOT NULL AND b.expiry_date > DATE_ADD(CURDATE(), INTERVAL " . $warning_days . " DAY) THEN 1 ELSE 0 END) AS ok_count,
		SUM(CASE WHEN b.expiry_date IS NULL THEN 1 ELSE 0 END) AS no_expiry_count,
		COUNT(*) AS total_batches
	FROM " . TB_PREF . "stock_batches b
	INNER JOIN " . TB_PREF . "stock_master sm ON b.stock_id = sm.stock_id
	WHERE b.status='active' AND b.inactive=0";

if ($filter_stock !== '' && $filter_stock !== null)
	$aging_sql .= " AND b.stock_id=" . db_escape($filter_stock);

$aging_sql .= " GROUP BY b.stock_id ORDER BY expired_count DESC, critical_count DESC, sm.description";

$aging_result = db_query($aging_sql, 'could not get aging analysis');
$has_aging = false;

start_table(TABLESTYLE, "width='95%'");
$th = array(_('Item'), _('Total Batches'),
	_('Expired'), _('Critical ≤30d'), _('Warning'), _('OK'), _('No Expiry'));
table_header($th);

$k = 0;
while ($row = db_fetch($aging_result)) {
	$has_aging = true;
	alt_table_row_color($k);

	label_cell($row['stock_id'] . ' - ' . $row['description']);
	label_cell($row['total_batches'], "align='right'");

	// Color-coded counts
	$exp = (int)$row['expired_count'];
	$crit = (int)$row['critical_count'];
	$warn = (int)$row['warning_count'];
	$ok = (int)$row['ok_count'];
	$ne = (int)$row['no_expiry_count'];

	label_cell($exp > 0 ? '<span style="color:#fff;background:#dc3545;padding:2px 6px;border-radius:3px;">' . $exp . '</span>' : '0', "align='center'");
	label_cell($crit > 0 ? '<span style="color:#fff;background:#fd7e14;padding:2px 6px;border-radius:3px;">' . $crit . '</span>' : '0', "align='center'");
	label_cell($warn > 0 ? '<span style="color:#333;background:#ffc107;padding:2px 6px;border-radius:3px;">' . $warn . '</span>' : '0', "align='center'");
	label_cell($ok > 0 ? '<span style="color:#fff;background:#28a745;padding:2px 6px;border-radius:3px;">' . $ok . '</span>' : '0', "align='center'");
	label_cell($ne > 0 ? '<span style="color:#fff;background:#6c757d;padding:2px 6px;border-radius:3px;">' . $ne . '</span>' : '0', "align='center'");

	end_row();
}

end_table();

if (!$has_aging)
	display_note(_('No active batches found for aging analysis.'));

//======================================================================
//  S T A T U S   S U M M A R Y
//======================================================================

echo "<h3 style='margin:20px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
	. _('Batch Status Summary') . "</h3>";

$status_summary = get_batch_status_summary($filter_stock);

echo "<div style='display:flex;flex-wrap:wrap;gap:8px;margin:10px 0;'>";
foreach (get_batch_statuses() as $code => $label) {
	$count = isset($status_summary[$code]) ? $status_summary[$code] : 0;
	$color = get_batch_status_color($code);
	echo "<div style='display:inline-block;padding:6px 14px;border-radius:4px;background:" . $color
		. ";color:#fff;font-size:13px;'>"
		. $label . ': <strong>' . $count . '</strong></div>';
}
echo "</div>";

end_form();
end_page();
