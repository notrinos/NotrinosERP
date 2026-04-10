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
 * Cross-Dock & Drop-Ship Management — Monitor cross-dock candidates,
 * create drop-ship POs, view history, and configure eligibility flags.
 *
 * Tabs:
 *   1. Cross-Dock Candidates — pending SO lines matched to eligible items
 *   2. Drop-Ship Orders — active drop-ship POs and their SO linkage
 *   3. Configuration — item eligibility flags and location cross-dock enable
 *   4. History — past cross-dock operations
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_CROSSDOCK_DROPSHIP';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/includes/db/inventory_db.inc');
include_once($path_to_root . '/purchasing/includes/db/po_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_crossdock_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Cross-Dock & Drop-Ship'), false, false, '', $js);

// =====================================================================
// TAB SETUP
// =====================================================================

$tabs = array(
	'crossdock'  => _('Cross-Dock Candidates'),
	'dropship'   => _('Drop-Ship Orders'),
	'config'     => _('Configuration'),
	'history'    => _('History'),
);

$current_tab = isset($_GET['tab']) ? $_GET['tab'] : (get_post('tab') ? get_post('tab') : 'crossdock');
if (!isset($tabs[$current_tab]))
	$current_tab = 'crossdock';

// =====================================================================
// HANDLE ACTIONS
// =====================================================================

// --- Create Drop-Ship PO ---
if (isset($_POST['CREATE_DS_PO'])) {
	$input_error = 0;

	$so_no = (int)get_post('ds_so_no');
	$so_line_id = (int)get_post('ds_so_line_id');
	$stock_id = get_post('ds_stock_id');
	$qty = input_num('ds_qty', 0);
	$supplier_id = (int)get_post('ds_supplier_id');

	if (!$so_no || !$so_line_id) {
		display_error(_('Invalid sales order reference.'));
		$input_error = 1;
	}
	if (empty($stock_id)) {
		display_error(_('No item specified.'));
		$input_error = 1;
	}
	if ($qty <= 0) {
		display_error(_('Quantity must be greater than zero.'));
		$input_error = 1;
	}
	if (!is_item_drop_ship_eligible($stock_id)) {
		display_error(sprintf(_('Item %s is not eligible for drop-shipping.'), $stock_id));
		$input_error = 1;
	}

	if ($input_error == 0) {
		// Get SO delivery info
		$so_sql = "SELECT deliver_to, delivery_address, from_stk_loc
			FROM " . TB_PREF . "sales_orders
			WHERE order_no = " . (int)$so_no . " AND trans_type = " . ST_SALESORDER;
		$so_result = db_query($so_sql, 'could not get SO');
		$so_row = db_fetch($so_result);
		$delivery_address = $so_row ? $so_row['delivery_address'] : '';
		$deliver_to = $so_row ? $so_row['deliver_to'] : '';
		$loc_code = $so_row ? $so_row['from_stk_loc'] : '';

		$result = create_drop_ship_po($so_no, $so_line_id, $stock_id, $qty,
			$supplier_id ? $supplier_id : null,
			$delivery_address, $deliver_to, $loc_code);

		if ($result['success']) {
			display_notification($result['message']);
		} else {
			display_error($result['message']);
		}
	}
	$Ajax->activate('_page_body');
}

// --- Update item flags ---
if (isset($_POST['UPDATE_ITEM_FLAGS'])) {
	$stock_id = get_post('config_stock_id');
	if (!empty($stock_id)) {
		$cross_dock = check_value('item_cross_dock_eligible');
		$drop_ship = check_value('item_drop_ship_eligible');
		update_item_crossdock_dropship_flags($stock_id, $cross_dock, $drop_ship);
		display_notification(sprintf(_('Flags updated for item %s.'), $stock_id));
	}
	$Ajax->activate('_page_body');
}

// --- Update location cross-dock flag ---
if (isset($_POST['UPDATE_LOC_FLAG'])) {
	$loc_code = get_post('config_loc_code');
	if (!empty($loc_code)) {
		$enabled = check_value('loc_cross_dock_enabled');
		update_location_cross_dock_flag($loc_code, $enabled);
		display_notification(sprintf(_('Cross-dock flag updated for location %s.'), $loc_code));
	}
	$Ajax->activate('_page_body');
}

// =====================================================================
// TAB NAVIGATION
// =====================================================================

$base_url = $_SERVER['PHP_SELF'] . '?';
echo '<div class="tab-navigation" style="margin-bottom:10px;">';
foreach ($tabs as $key => $label) {
	$style = 'padding:6px 16px; margin-right:4px; text-decoration:none; display:inline-block;';
	if ($key === $current_tab)
		$style .= ' font-weight:bold; border-bottom:2px solid #337ab7;';
	echo '<a href="' . $base_url . 'tab=' . $key . '" style="' . $style . '">' . $label . '</a>';
}
echo '</div>';

// =====================================================================
// TAB: Cross-Dock Candidates
// =====================================================================

if ($current_tab === 'crossdock') {
	start_form();

	div_start('crossdock_list');

	// Badge
	$cd_count = get_pending_cross_dock_count();
	display_heading(sprintf(_('Cross-Dock Candidates (%d pending)'), $cd_count));
	echo '<br>';

	if ($cd_count > 0) {
		// Show candidates grouped by location
		$loc_sql = "SELECT loc_code, location_name FROM " . TB_PREF . "locations
			WHERE cross_dock_enabled = 1 AND inactive = 0 ORDER BY location_name";
		$locs = db_query($loc_sql, 'could not get locations');

		while ($loc = db_fetch($locs)) {
			display_heading2(sprintf(_('Warehouse: %s'), $loc['location_name']));

			// Get all eligible pending SO lines for this location
			$sql = "SELECT sod.id AS so_line_id, sod.order_no, sod.stk_code,
					sod.description, (sod.quantity - sod.qty_sent) AS outstanding_qty,
					so.delivery_date, so.debtor_no, d.name AS customer_name,
					so.deliver_to, sm.cross_dock_eligible, sm.drop_ship_eligible
				FROM " . TB_PREF . "sales_order_details sod
				INNER JOIN " . TB_PREF . "sales_orders so ON sod.order_no = so.order_no
					AND so.trans_type = " . ST_SALESORDER . "
				INNER JOIN " . TB_PREF . "debtors_master d ON so.debtor_no = d.debtor_no
				INNER JOIN " . TB_PREF . "stock_master sm ON sod.stk_code = sm.stock_id
				WHERE so.from_stk_loc = " . db_escape($loc['loc_code']) . "
					AND (sod.quantity - sod.qty_sent) > 0
					AND sm.cross_dock_eligible = 1
					AND sod.drop_ship = 0
				ORDER BY so.delivery_date ASC, sod.order_no ASC";
			$result = db_query($sql, 'could not get candidates');

			$k = 0;
			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('SO #'), _('Item'), _('Description'), _('Qty Outstanding'),
				_('Delivery Date'), _('Customer'), _('Ship To'));
			table_header($th);

			while ($row = db_fetch($result)) {
				alt_table_row_color($k);
				label_cell($row['order_no']);
				label_cell($row['stk_code']);
				label_cell($row['description']);
				qty_cell($row['outstanding_qty']);
				label_cell(sql2date($row['delivery_date']));
				label_cell($row['customer_name']);
				label_cell($row['deliver_to']);
				end_row();
			}
			end_table(1);
		}
	} else {
		display_note(_('No cross-dock candidates found. Enable cross-dock on items and locations in the Configuration tab.'));
	}

	div_end();
	end_form();
}

// =====================================================================
// TAB: Drop-Ship Orders
// =====================================================================

if ($current_tab === 'dropship') {
	start_form();

	// --- Drop-ship PO creation form ---
	display_heading(_('Create Drop-Ship Purchase Order'));
	echo '<br>';

	start_table(TABLESTYLE2);

	text_row(_('Sales Order #:'), 'ds_so_no', get_post('ds_so_no', ''), 10, 10);
	text_row(_('SO Line ID:'), 'ds_so_line_id', get_post('ds_so_line_id', ''), 10, 10);
	echo "<tr><td class='label'>" . _('Item:') . "</td>";
	stock_items_list_cells(null, 'ds_stock_id', null, false, true);
	echo "</tr>";
	small_qty_row(_('Quantity:'), 'ds_qty', get_post('ds_qty', ''));
	echo "<tr><td class='label'>" . _('Supplier:') . "</td>";
	supplier_list_cells(null, 'ds_supplier_id', null, false, true);
	echo "</tr>";

	end_table(1);
	submit_center('CREATE_DS_PO', _('Create Drop-Ship PO'), true, '', 'default');

	// --- List existing drop-ship orders ---
	display_heading(_('Active Drop-Ship Orders'));
	echo '<br>';

	div_start('ds_list');

	$ds_orders = get_drop_ship_orders();
	$k = 0;
	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('PO #'), _('Supplier'), _('SO #'), _('Items'), _('Qty Ordered'),
		_('Qty Received'), _('Order Date'), _('Ship-To Address'), _('Status'));
	table_header($th);

	$has_rows = false;
	while ($row = db_fetch($ds_orders)) {
		$has_rows = true;
		alt_table_row_color($k);
		label_cell($row['order_no']);
		label_cell($row['supp_name']);
		label_cell($row['drop_ship_so_no']);
		label_cell($row['items']);
		qty_cell($row['total_qty']);
		qty_cell($row['total_received']);
		label_cell(sql2date($row['ord_date']));
		label_cell(substr($row['delivery_address'], 0, 50) . (strlen($row['delivery_address']) > 50 ? '...' : ''));

		$status = ((float)$row['total_received'] >= (float)$row['total_qty'])
			? '<span style="color:green;font-weight:bold;">' . _('Received') . '</span>'
			: '<span style="color:orange;font-weight:bold;">' . _('Pending') . '</span>';
		label_cell($status);
		end_row();
	}

	if (!$has_rows) {
		start_row();
		label_cell(_('No drop-ship orders found.'), "colspan=9 class='centered'");
		end_row();
	}

	end_table(1);
	div_end();

	// --- Drop-ship SO lines ---
	display_heading(_('Drop-Ship Sales Order Lines'));
	echo '<br>';

	div_start('ds_so_lines');

	$ds_lines = get_drop_ship_so_lines();
	$k = 0;
	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('SO #'), _('Item'), _('Description'), _('Qty'), _('Qty Sent'),
		_('Supplier'), _('PO #'), _('Customer'));
	table_header($th);

	$has_rows = false;
	while ($row = db_fetch($ds_lines)) {
		$has_rows = true;
		alt_table_row_color($k);
		label_cell($row['order_no']);
		label_cell($row['stk_code']);
		label_cell($row['description']);
		qty_cell($row['quantity']);
		qty_cell($row['qty_sent']);
		label_cell($row['supp_name'] ? $row['supp_name'] : '-');
		label_cell($row['drop_ship_po_no'] ? $row['drop_ship_po_no'] : '-');
		label_cell($row['customer_name']);
		end_row();
	}

	if (!$has_rows) {
		start_row();
		label_cell(_('No drop-ship SO lines found.'), "colspan=8 class='centered'");
		end_row();
	}

	end_table(1);
	div_end();

	end_form();
}

// =====================================================================
// TAB: Configuration
// =====================================================================

if ($current_tab === 'config') {
	start_form();

	// --- Item flags ---
	display_heading(_('Item Cross-Dock / Drop-Ship Eligibility'));
	echo '<br>';

	start_table(TABLESTYLE2);
	echo "<tr><td class='label'>" . _('Select Item:') . "</td>";
	stock_items_list_cells(null, 'config_stock_id', null, false, true);
	echo "</tr>";

	$config_stock = get_post('config_stock_id');
	$cd_flag = 0;
	$ds_flag = 0;
	if ($config_stock) {
		$item_sql = "SELECT cross_dock_eligible, drop_ship_eligible
			FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($config_stock);
		$item_r = db_query($item_sql, 'could not get item flags');
		$item_row = db_fetch($item_r);
		if ($item_row) {
			$cd_flag = $item_row['cross_dock_eligible'];
			$ds_flag = $item_row['drop_ship_eligible'];
		}
	}

	check_row(_('Cross-Dock Eligible:'), 'item_cross_dock_eligible', $cd_flag);
	check_row(_('Drop-Ship Eligible:'), 'item_drop_ship_eligible', $ds_flag);
	end_table(1);
	submit_center('UPDATE_ITEM_FLAGS', _('Update Item Flags'), true, '', 'default');

	br(2);

	// --- Location flag ---
	display_heading(_('Location Cross-Dock Setting'));
	echo '<br>';

	start_table(TABLESTYLE2);
	locations_list_row(_('Select Location:'), 'config_loc_code', null, false);

	$config_loc = get_post('config_loc_code');
	$loc_cd_flag = 0;
	if ($config_loc) {
		$loc_sql = "SELECT cross_dock_enabled FROM " . TB_PREF . "locations
			WHERE loc_code = " . db_escape($config_loc);
		$loc_r = db_query($loc_sql, 'could not get location flag');
		$loc_row = db_fetch($loc_r);
		if ($loc_row) {
			$loc_cd_flag = $loc_row['cross_dock_enabled'];
		}
	}

	check_row(_('Cross-Dock Enabled:'), 'loc_cross_dock_enabled', $loc_cd_flag);
	end_table(1);
	submit_center('UPDATE_LOC_FLAG', _('Update Location Flag'), true, '', 'default');

	br(2);

	// --- Show current eligible items ---
	display_heading(_('Currently Eligible Items'));
	echo '<br>';

	div_start('eligible_items');

	$eligible = db_query("SELECT sm.stock_id, sm.description,
			sm.cross_dock_eligible, sm.drop_ship_eligible,
			sc.description AS category
		FROM " . TB_PREF . "stock_master sm
		LEFT JOIN " . TB_PREF . "stock_category sc ON sm.category_id = sc.category_id
		WHERE sm.cross_dock_eligible = 1 OR sm.drop_ship_eligible = 1
		ORDER BY sm.stock_id", 'could not get eligible items');

	$k = 0;
	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('Item Code'), _('Description'), _('Category'), _('Cross-Dock'), _('Drop-Ship'));
	table_header($th);

	$has_rows = false;
	while ($row = db_fetch($eligible)) {
		$has_rows = true;
		alt_table_row_color($k);
		label_cell($row['stock_id']);
		label_cell($row['description']);
		label_cell($row['category']);
		label_cell($row['cross_dock_eligible'] ? _('Yes') : _('No'), "class='centered'");
		label_cell($row['drop_ship_eligible'] ? _('Yes') : _('No'), "class='centered'");
		end_row();
	}

	if (!$has_rows) {
		start_row();
		label_cell(_('No items flagged for cross-dock or drop-ship yet.'), "colspan=5 class='centered'");
		end_row();
	}

	end_table(1);

	// Show cross-dock enabled locations
	display_heading2(_('Cross-Dock Enabled Locations'));

	$locs = get_cross_dock_locations();
	$k = 0;
	start_table(TABLESTYLE, "width='60%'");
	$th = array(_('Location Code'), _('Location Name'));
	table_header($th);

	$has_rows = false;
	while ($row = db_fetch($locs)) {
		$has_rows = true;
		alt_table_row_color($k);
		label_cell($row['loc_code']);
		label_cell($row['location_name']);
		end_row();
	}

	if (!$has_rows) {
		start_row();
		label_cell(_('No locations have cross-dock enabled yet.'), "colspan=2 class='centered'");
		end_row();
	}

	end_table(1);
	div_end();

	end_form();
}

// =====================================================================
// TAB: History
// =====================================================================

if ($current_tab === 'history') {
	start_form();

	display_heading(_('Cross-Dock & Drop-Ship History'));
	echo '<br>';

	// Filters
	start_table(TABLESTYLE2);
	date_row(_('From Date:'), 'hist_from', '', true);
	date_row(_('To Date:'), 'hist_to', '', true);
	echo "<tr><td class='label'>" . _('Item:') . "</td>";
	stock_items_list_cells(null, 'hist_stock_id', null, _('All Items'), true);
	echo "</tr>";
	end_table(1);

	submit_center('REFRESH_HISTORY', _('Search'), true, '', 'default');
	br();

	div_start('history_list');

	// Cross-dock operations
	display_heading2(_('Cross-Dock Operations'));

	$from_dt = get_post('hist_from') ? date2sql(get_post('hist_from')) : null;
	$to_dt = get_post('hist_to') ? date2sql(get_post('hist_to')) : null;
	$hist_item = get_post('hist_stock_id');

	$history = get_cross_dock_history($from_dt, $to_dt, null, $hist_item ? $hist_item : null);

	$k = 0;
	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('Op ID'), _('Type'), _('Status'), _('GRN #'), _('Item'),
		_('Description'), _('Qty Planned'), _('Qty Done'), _('Created'), _('Completed'));
	table_header($th);

	$has_rows = false;
	while ($row = db_fetch($history)) {
		$has_rows = true;
		alt_table_row_color($k);
		label_cell($row['op_id']);
		label_cell($row['op_type']);
		label_cell($row['op_status']);
		label_cell($row['source_doc_no']);
		label_cell($row['stock_id']);
		label_cell($row['item_description']);
		qty_cell($row['qty_planned']);
		qty_cell($row['qty_done']);
		label_cell($row['created_at'] ? sql2date(substr($row['created_at'], 0, 10)) : '-');
		label_cell($row['completed_at'] ? sql2date(substr($row['completed_at'], 0, 10)) : '-');
		end_row();
	}

	if (!$has_rows) {
		start_row();
		label_cell(_('No cross-dock operations found.'), "colspan=10 class='centered'");
		end_row();
	}

	end_table(1);

	// Drop-ship PO history
	display_heading2(_('Drop-Ship Purchase Orders'));

	$ds_history = get_drop_ship_orders($from_dt, $to_dt);

	$k = 0;
	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('PO #'), _('Supplier'), _('SO #'), _('Items'), _('Qty'),
		_('Received'), _('Date'), _('Address'));
	table_header($th);

	$has_rows = false;
	while ($row = db_fetch($ds_history)) {
		$has_rows = true;
		alt_table_row_color($k);
		label_cell($row['order_no']);
		label_cell($row['supp_name']);
		label_cell($row['drop_ship_so_no']);
		label_cell($row['items']);
		qty_cell($row['total_qty']);
		qty_cell($row['total_received']);
		label_cell(sql2date($row['ord_date']));
		label_cell(substr($row['delivery_address'], 0, 40));
		end_row();
	}

	if (!$has_rows) {
		start_row();
		label_cell(_('No drop-ship orders found.'), "colspan=8 class='centered'");
		end_row();
	}

	end_table(1);
	div_end();

	end_form();
}

end_page();
