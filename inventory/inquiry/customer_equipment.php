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
 * Customer Equipment Register — View all serialized items delivered to a customer,
 * with warranty status, claim history, and quick links to warranty claims and lifecycle.
 *
 * Access: SA_CUSTOMER_EQUIPMENT
 * URL params: ?customer_id=N (optional pre-select)
 */
$page_security = 'SA_CUSTOMER_EQUIPMENT';
$path_to_root = '../..';
include_once($path_to_root . '/includes/db_pager.inc');
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Customer Equipment Register');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/customer_equipment_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');

//----------------------------------------------------------------------
// Handle GET parameters
//----------------------------------------------------------------------
if (isset($_GET['customer_id']))
	$_POST['customer_id'] = $_GET['customer_id'];

//----------------------------------------------------------------------
// Filter form
//----------------------------------------------------------------------
start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

customer_list_cells(_('Customer:'), 'customer_id', null, false, true, false, true);

stock_items_list_cells(_('Item:'), 'stock_id', null, true, true);

$warranty_options = array(
	'all'           => _('All Equipment'),
	'active'        => _('Active Warranty'),
	'expiring_soon' => _('Expiring Soon (30 days)'),
	'expired'       => _('Expired Warranty'),
);
filter_cell_open(_('Warranty:'));
echo array_selector('warranty_filter', get_post('warranty_filter', 'all'),
	$warranty_options, array('select_submit' => true));
filter_cell_close();

$status_options = array(
	''          => _('All Statuses'),
	'delivered' => _('Delivered'),
	'in_repair' => _('In Repair'),
	'returned'  => _('Returned'),
	'recalled'  => _('Recalled'),
);
filter_cell_open(_('Status:'));
echo array_selector('status_filter', get_post('status_filter', ''),
	$status_options, array('select_submit' => true));
filter_cell_close();

submit_cells('RefreshInquiry', _('Apply Filter'), '', _('Refresh equipment list'), 'default');

end_row();
end_table();

//----------------------------------------------------------------------
// Refresh on filter changes
//----------------------------------------------------------------------
if (get_post('RefreshInquiry') || list_updated('customer_id')
	|| list_updated('stock_id') || list_updated('warranty_filter')
	|| list_updated('status_filter'))
{
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Summary cards
//----------------------------------------------------------------------
$customer_id = get_post('customer_id', 0);

if ($customer_id && $customer_id != '' && $customer_id != ALL_TEXT) {

	$summary = get_customer_equipment_summary($customer_id);

	div_start('summary_cards');
	echo "<div style='display:flex; flex-wrap:wrap; gap:10px; margin:10px 0;'>";

	display_summary_card(_('Total Equipment'), $summary['total'], '#17a2b8');
	display_summary_card(_('Active Warranty'), $summary['active_warranty'], '#28a745');
	display_summary_card(_('Expiring Soon'), $summary['expiring_soon'], '#ffc107', '#333');
	display_summary_card(_('Expired'), $summary['expired_warranty'], '#dc3545');
	display_summary_card(_('No Warranty'), $summary['no_warranty'], '#6c757d');
	display_summary_card(_('Open Claims'), $summary['open_claims'], '#fd7e14');

	echo "</div>";
	div_end();

	//----------------------------------------------------------------------
	// Equipment list (pager)
	//----------------------------------------------------------------------
	$sql = get_sql_for_customer_equipment(
		$customer_id,
		get_post('stock_id', ''),
		get_post('warranty_filter', 'all'),
		get_post('status_filter', '')
	);

	$cols = array(
		_('Serial No') => array('fun' => 'fmt_serial_link', 'ord' => ''),
		_('Item Code')  => array('name' => 'stock_id', 'ord' => ''),
		_('Description') => 'item_description',
		_('Status')     => array('fun' => 'fmt_serial_status'),
		_('Delivered')  => array('name' => 'delivery_date', 'type' => 'date', 'ord' => 'desc'),
		_('Warranty Start') => array('name' => 'warranty_start', 'type' => 'date'),
		_('Warranty End')   => array('name' => 'warranty_end', 'type' => 'date'),
		_('Warranty')   => array('fun' => 'fmt_warranty_badge'),
		_('Days Left')  => array('fun' => 'fmt_warranty_days', 'align' => 'right'),
		_('Claims')     => array('fun' => 'fmt_claim_count', 'align' => 'center'),
		array('insert' => true, 'fun' => 'fmt_warranty_claim_link'),
		array('insert' => true, 'fun' => 'fmt_lifecycle_link'),
	);

	$table =& new_db_pager('equipment_tbl', $sql, $cols);
	$table->width = '95%';
	display_db_pager($table);

	//----------------------------------------------------------------------
	// Warranty claims section
	//----------------------------------------------------------------------
	echo "<h3 style='margin:20px 0 10px 0; border-bottom:1px solid #ddd; padding-bottom:3px;'>"
		. _('Recent Warranty Claims') . "</h3>";

	$claims = get_customer_warranty_claims($customer_id, '', 10, 0);
	if (db_num_rows($claims) == 0) {
		display_note(_('No warranty claims found for this customer.'));
	} else {
		start_table(TABLESTYLE, "width='95%'");
		$th = array(_('Reference'), _('Item'), _('Serial'), _('Date'),
			_('Status'), _('Issue Type'), _('Chargeable'), '');
		table_header($th);

		$k = 0;
		while ($claim = db_fetch($claims)) {
			alt_table_row_color($k);

			label_cell($claim['reference']);
			label_cell($claim['item_description']);
			label_cell($claim['serial_no'] ? $claim['serial_no'] : '-');
			label_cell(sql2date($claim['claim_date']));
			label_cell(warranty_claim_status_badge($claim['status']));
			label_cell($claim['issue_type']);
			label_cell($claim['is_chargeable'] ? _('Yes') : _('No'), 'align=center');
			label_cell("<a href='" . $path_to_root
				. "/inventory/manage/warranty_claims.php?id=" . $claim['id'] . "'>"
				. _('View') . "</a>");
			end_row();
		}
		end_table();
	}

} else {
	display_note(_('Please select a customer to view their equipment register.'));
}

end_form();
end_page();

//======================================================================
// Column formatting functions
//======================================================================

/**
 * Format serial number as a hyperlink to serial inquiry.
 */
function fmt_serial_link($row) {
	global $path_to_root;
	return "<a href='" . $path_to_root . "/inventory/inquiry/serial_inquiry.php?serial_id="
		. $row['id'] . "'>" . htmlspecialchars($row['serial_no']) . "</a>";
}

/**
 * Format serial status with color badge.
 */
function fmt_serial_status($row) {
	$colors = array(
		'delivered'  => '#6c757d',
		'in_repair'  => '#fd7e14',
		'returned'   => '#6610f2',
		'recalled'   => '#dc3545',
	);
	$color = isset($colors[$row['status']]) ? $colors[$row['status']] : '#6c757d';
	$label = ucfirst(str_replace('_', ' ', $row['status']));
	return "<span style='background:{$color};color:#fff;padding:2px 8px;border-radius:3px;font-size:11px;'>"
		. $label . "</span>";
}

/**
 * Format warranty status badge.
 */
function fmt_warranty_badge($row) {
	$colors = array(
		'active'        => '#28a745',
		'expiring_soon' => '#ffc107',
		'expired'       => '#dc3545',
		'none'          => '#adb5bd',
	);
	$labels = array(
		'active'        => _('Active'),
		'expiring_soon' => _('Expiring'),
		'expired'       => _('Expired'),
		'none'          => _('None'),
	);
	$ws = $row['warranty_status'];
	$color = isset($colors[$ws]) ? $colors[$ws] : '#adb5bd';
	$text_color = ($ws == 'expiring_soon') ? '#333' : '#fff';
	$label = isset($labels[$ws]) ? $labels[$ws] : $ws;
	return "<span style='background:{$color};color:{$text_color};padding:2px 8px;border-radius:3px;font-size:11px;'>"
		. $label . "</span>";
}

/**
 * Format warranty days remaining.
 */
function fmt_warranty_days($row) {
	if ($row['warranty_status'] == 'none')
		return '-';
	$days = (int)$row['warranty_days_remaining'];
	if ($days < 0)
		return "<span style='color:#dc3545;'>" . sprintf(_('%d days ago'), abs($days)) . "</span>";
	if ($days <= 30)
		return "<span style='color:#ffc107;font-weight:bold;'>" . sprintf(_('%d days'), $days) . "</span>";
	return sprintf(_('%d days'), $days);
}

/**
 * Format claim count with link.
 */
function fmt_claim_count($row) {
	global $path_to_root;
	$count = (int)$row['claim_count'];
	if ($count == 0)
		return '0';
	return "<a href='" . $path_to_root
		. "/inventory/manage/warranty_claims.php?customer_id=" . get_post('customer_id')
		. "&serial_id=" . $row['id'] . "'>" . $count . "</a>";
}

/**
 * Create warranty claim link.
 */
function fmt_warranty_claim_link($row) {
	global $path_to_root;
	return "<a href='" . $path_to_root
		. "/inventory/manage/warranty_claims.php?AddNew=1&customer_id=" . get_post('customer_id')
		. "&serial_id=" . $row['id'] . "&stock_id=" . urlencode($row['stock_id'])
		. "'>" . _('New Claim') . "</a>";
}

/**
 * Lifecycle view link.
 */
function fmt_lifecycle_link($row) {
	global $path_to_root;
	return "<a href='" . $path_to_root
		. "/inventory/inquiry/serial_lifecycle.php?serial_id=" . $row['id']
		. "'>" . _('Lifecycle') . "</a>";
}

/**
 * Render a summary card.
 */
function display_summary_card($title, $value, $bg_color, $text_color = '#fff') {
	echo "<div style='background:{$bg_color};color:{$text_color};padding:12px 20px;border-radius:6px;min-width:120px;text-align:center;'>"
		. "<div style='font-size:24px;font-weight:bold;'>" . (int)$value . "</div>"
		. "<div style='font-size:12px;opacity:0.9;'>" . $title . "</div>"
		. "</div>";
}

/**
 * Render a warranty claim status badge.
 */
function warranty_claim_status_badge($status) {
	$colors = array(
		'open'         => '#dc3545',
		'acknowledged' => '#fd7e14',
		'in_repair'    => '#ffc107',
		'replaced'     => '#17a2b8',
		'resolved'     => '#28a745',
		'rejected'     => '#6c757d',
		'closed'       => '#343a40',
	);
	$color = isset($colors[$status]) ? $colors[$status] : '#6c757d';
	$text_color = ($status == 'in_repair') ? '#333' : '#fff';
	$label = ucfirst(str_replace('_', ' ', $status));
	return "<span style='background:{$color};color:{$text_color};padding:2px 8px;border-radius:3px;font-size:11px;'>"
		. $label . "</span>";
}
