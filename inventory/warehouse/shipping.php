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
 * Shipping Management — Shipment CRUD, package assignment, ship confirmation.
 *
 * Features:
 * - Create shipments, assign sealed packages
 * - Confirm shipment (ready for dispatch)
 * - Ship confirmation with carrier & tracking
 * - Shipment status tracking (draft → confirmed → shipped)
 * - Available packages list for assignment
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_SHIPPING';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_shipping_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Shipping Management'), false, false, '', $js);

simple_page_mode(true);

// =====================================================================
// Process Actions
// =====================================================================

// --- Add Shipment ---
if (isset($_POST['ADD_ITEM'])) {
	$warehouse = get_post('ship_warehouse');
	$carrier = trim(get_post('ship_carrier'));
	$tracking = trim(get_post('ship_tracking'));
	$ship_date = get_post('ship_date');
	$memo = trim(get_post('ship_memo'));

	if (empty($warehouse)) {
		display_error(_('Please select a warehouse.'));
	} else {
		$new_id = add_shipment($warehouse, $carrier, $tracking, $ship_date, null, $memo);
		display_notification(sprintf(_('Shipment #%d created.'), $new_id));
		$_POST['view_shipment'] = $new_id;
		$Mode = 'RESET';
	}
	$Ajax->activate('_page_body');
}

// --- Update Shipment ---
if (isset($_POST['UPDATE_ITEM'])) {
	$ship_id = (int)$_POST['selected_id'];
	$carrier = trim(get_post('ship_carrier'));
	$tracking = trim(get_post('ship_tracking'));
	$ship_date = get_post('ship_date');
	$memo = trim(get_post('ship_memo'));

	update_shipment($ship_id, $carrier, $tracking, $ship_date, null, $memo);
	display_notification(_('Shipment has been updated.'));
	$Mode = 'RESET';
	$Ajax->activate('_page_body');
}

// --- Add Package to Shipment ---
if (isset($_POST['AddPkgToShipment'])) {
	$ship_id = (int)get_post('current_shipment_id');
	$pkg_id = (int)get_post('add_package_id');

	if ($ship_id <= 0 || $pkg_id <= 0) {
		display_error(_('Please select a shipment and package.'));
	} else {
		$result = add_package_to_shipment($ship_id, $pkg_id);
		if ($result['success'])
			display_notification($result['message']);
		else
			display_error($result['message']);
	}
	$_POST['view_shipment'] = $ship_id;
	$Ajax->activate('_page_body');
}

// --- Remove Package from Shipment ---
if (isset($_POST['RemovePkg'])) {
	$ship_id = (int)get_post('current_shipment_id');
	$pkg_id = (int)$_POST['RemovePkg'];

	$result = remove_package_from_shipment($ship_id, $pkg_id);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);

	$_POST['view_shipment'] = $ship_id;
	$Ajax->activate('_page_body');
}

// --- Confirm Shipment ---
if (isset($_POST['ConfirmShipment'])) {
	$ship_id = (int)$_POST['ConfirmShipment'];
	$result = confirm_shipment($ship_id);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$_POST['view_shipment'] = $ship_id;
	$Ajax->activate('_page_body');
}

// --- Ship Shipment ---
if (isset($_POST['ShipShipment'])) {
	$ship_id = (int)$_POST['ShipShipment'];
	$carrier = trim(get_post('confirm_carrier'));
	$tracking = trim(get_post('confirm_tracking'));

	$result = ship_shipment($ship_id, $carrier, $tracking);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$_POST['view_shipment'] = $ship_id;
	$Ajax->activate('_page_body');
}

// --- Cancel Shipment ---
if (isset($_POST['CancelShipment'])) {
	$ship_id = (int)$_POST['CancelShipment'];
	$result = cancel_shipment($ship_id);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$Ajax->activate('_page_body');
}

// --- Delete Shipment (only draft with no packages) ---
if ($Mode == 'Delete') {
	$op = get_wh_operation($selected_id);
	if ($op && $op['op_status'] === 'draft') {
		$pkg_cnt = count_shipment_packages($selected_id);
		if ($pkg_cnt === 0) {
			$sql = "DELETE FROM " . TB_PREF . "wh_operations WHERE op_id=" . (int)$selected_id . " AND op_type='ship'";
			db_query($sql, 'could not delete shipment');
			display_notification(_('Shipment deleted.'));
		} else {
			display_error(_('Cannot delete shipment with packages. Remove packages first.'));
		}
	} else {
		display_error(_('Only draft shipments can be deleted.'));
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	unset($_POST['ship_warehouse'], $_POST['ship_carrier'], $_POST['ship_tracking'],
		$_POST['ship_date'], $_POST['ship_memo']);
}

// =====================================================================
// Filter Bar
// =====================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

// Warehouse filter
$sql_locs = "SELECT loc_code, location_name FROM " . TB_PREF . "locations WHERE wh_enabled = 1 ORDER BY location_name";
echo "<div class = 'filter-field'>";
echo combo_input('filter_warehouse', get_post('filter_warehouse'), $sql_locs, 'loc_code', 'location_name',
	array('spec_option' => _('All Warehouses'), 'spec_id' => '', 'select_submit' => true, 'order' => false));
echo '</div>';

// Status filter
$status_options = array(
	''          => _('All Statuses'),
	'draft'     => _('Draft'),
	'ready'     => _('Confirmed'),
	'done'      => _('Shipped'),
	'cancelled' => _('Cancelled'),
);
echo "<div class = 'filter-field'>";
echo array_selector('filter_ship_status', get_post('filter_ship_status'), $status_options, array('select_submit' => true));
echo '</div>';
submit_cells('RefreshList', _('Search'), '', _('Refresh list'), 'default');
end_row();
end_table(1);

// =====================================================================
// Status Summary Cards
// =====================================================================

$summary = get_shipment_status_summary();

echo '<div style="display:flex;gap:12px;margin:10px 0;flex-wrap:wrap;">';
$status_labels = get_shipment_statuses();
foreach ($summary as $status => $count) {
	if ($status === 'cancelled' && $count === 0) continue;
	$color = get_shipment_status_color($status);
	$label = isset($status_labels[$status]) ? $status_labels[$status] : $status;
	echo '<div style="background:#fff;border:2px solid ' . $color . ';border-radius:8px;padding:8px 16px;text-align:center;min-width:100px;">'
		. '<div style="font-size:22px;font-weight:bold;color:' . $color . ';">' . $count . '</div>'
		. '<div style="font-size:11px;color:#666;">' . htmlspecialchars($label) . '</div></div>';
}
echo '</div>';

// =====================================================================
// Shipment Detail View
// =====================================================================

$view_shipment = (int)get_post('view_shipment');
if ($view_shipment > 0) {
	$shipment = get_shipment($view_shipment);
	if ($shipment) {
		display_heading(sprintf(_('Shipment #%d'), $view_shipment));

		// Shipment info card
		echo '<div style="background:#f8f9fa;border:1px solid #dee2e6;border-radius:6px;padding:12px;margin:8px 0;">';
		echo '<table style="width:100%;">';
		echo '<tr><td><strong>' . _('Status:') . '</strong> ' . shipment_status_badge($shipment['op_status']) . '</td>';
		echo '<td><strong>' . _('Carrier:') . '</strong> ' . htmlspecialchars($shipment['carrier'] ?: '-') . '</td>';
		echo '<td><strong>' . _('Tracking:') . '</strong> ' . htmlspecialchars($shipment['tracking_number'] ?: '-') . '</td></tr>';
		echo '<tr><td><strong>' . _('Ship Date:') . '</strong> ' . ($shipment['ship_date'] ? sql2date($shipment['ship_date']) : '-') . '</td>';
		echo '<td><strong>' . _('Warehouse:') . '</strong> ' . htmlspecialchars($shipment['warehouse'] ?: '-') . '</td>';
		echo '<td><strong>' . _('Created:') . '</strong> ' . sql2date(substr($shipment['created_at'], 0, 10)) . '</td></tr>';
		if (!empty($shipment['memo'])) {
			echo '<tr><td colspan="3"><strong>' . _('Notes:') . '</strong> ' . htmlspecialchars($shipment['memo']) . '</td></tr>';
		}
		echo '</table></div>';

		// Packages in this shipment
		$ship_packages = get_shipment_packages($view_shipment);
		$ship_pkg_count = 0;

		echo '<h4>' . _('Packages in Shipment') . '</h4>';
		start_table(TABLESTYLE, "width='100%'");
		$th = array(_('#'), _('Package Code'), _('Type'), _('Status'), _('Weight'), _('Location'), _(''));
		table_header($th);

		$k = 0;
		while ($sp = db_fetch($ship_packages)) {
			$ship_pkg_count++;
			alt_table_row_color($k);

			label_cell($ship_pkg_count);
			label_cell(htmlspecialchars($sp['package_code']));
			label_cell(package_type_badge($sp['package_type']));
			label_cell(package_status_badge($sp['package_status']));
			label_cell($sp['weight'] ? number_format2((float)$sp['weight'], 2) . ' kg' : '-');
			label_cell($sp['location_name'] ? htmlspecialchars($sp['location_name']) : '-');

			echo '<td>';
			if (in_array($shipment['op_status'], array('draft', 'ready'))) {
				echo "<button class='ajaxsubmit' type='submit' aspect='default' name='RemovePkg' value='" . $sp['package_id'] . "' title='" . _('Remove from shipment') . "'>" . set_icon(ICON_DELETE) . "<span>" . _('Remove') . "</span></button>\n";
			} else {
				echo '-';
			}
			echo '</td>';

			end_row();
		}

		if ($ship_pkg_count === 0) {
			start_row();
			label_cell(_('No packages assigned yet.'), "colspan='7' style='text-align:center;padding:15px;color:#999;'");
			end_row();
		}
		end_table(1);

		// Add package to shipment (if draft or ready)
		if (in_array($shipment['op_status'], array('draft', 'ready'))) {
			hidden('current_shipment_id', $view_shipment);

			echo '<div style="background:#f0f8ff;border:1px solid #bee5eb;border-radius:4px;padding:10px;margin:8px 0;">';
			echo '<strong>' . _('Add Package:') . '</strong> ';

			// Available sealed packages dropdown
			$available = get_packages_available_for_shipping($shipment['warehouse']);
			$avail_options = array('' => _('-- Select Sealed Package --'));
			while ($avail = db_fetch($available)) {
				$avail_options[$avail['package_id']] = $avail['package_code']
					. ' (' . $avail['item_count'] . ' items, ' . number_format2((float)$avail['total_qty'], 0) . ' qty)';
			}
			echo array_selector('add_package_id', '', $avail_options);
			echo ' ';
			submit('AddPkgToShipment', _('Add Package'), true, _('Add selected package to this shipment'), 'default');
			echo '</div>';
		}

		// Action buttons
		echo '<div style="margin:10px 0;">';
		if ($shipment['op_status'] === 'draft') {
			echo "<button class='ajaxsubmit' type='submit' aspect='default' name='ConfirmShipment' value='" . $view_shipment . "' title='" . _('Confirm Shipment') . "'>" . set_icon(ICON_SUBMIT) . "<span>" . _('Confirm Shipment') . "</span></button>\n";
			echo ' ';
			echo "<button class='ajaxsubmit' type='submit' name='CancelShipment' value='" . $view_shipment . "' title='" . _('Cancel Shipment') . "'>" . set_icon(ICON_ESCAPE) . "<span>" . _('Cancel Shipment') . "</span></button>\n";
		} elseif (in_array($shipment['op_status'], array('ready', 'in_progress'))) {
			// Ship confirmation form
			echo '<div style="background:#fff3cd;border:1px solid #ffc107;border-radius:4px;padding:10px;margin:8px 0;">';
			echo '<strong>' . _('Ship Confirmation') . '</strong><br>';
			echo _('Carrier:') . ' <input type="text" name="confirm_carrier" size="30" maxlength="60" value="'
				. htmlspecialchars($shipment['carrier']) . '"> ';
			echo _('Tracking #:') . ' <input type="text" name="confirm_tracking" size="30" maxlength="100" value="'
				. htmlspecialchars($shipment['tracking_number']) . '"> ';
			echo "<button class='ajaxsubmit' type='submit' aspect='default' name='ShipShipment' value='" . $view_shipment . "' title='" . _('Confirm Ship') . "'>" . set_icon(ICON_SUBMIT) . "<span>" . _('Confirm Ship') . "</span></button>\n";
			echo '</div>';
			echo "<button class='ajaxsubmit' type='submit' name='CancelShipment' value='" . $view_shipment . "' title='" . _('Cancel Shipment') . "'>" . set_icon(ICON_ESCAPE) . "<span>" . _('Cancel Shipment') . "</span></button>\n";
		}
		echo '</div>';

		hidden('view_shipment', $view_shipment);
	}
}

// =====================================================================
// Shipments List
// =====================================================================

display_heading(_('Shipments'));

div_start('shipment_list');

$ship_filters = array();
if (!empty(get_post('filter_ship_status')))
	$ship_filters['status'] = get_post('filter_ship_status');
if (!empty(get_post('filter_warehouse')))
	$ship_filters['warehouse_loc_code'] = get_post('filter_warehouse');

$shipments = get_shipments($ship_filters);

start_table(TABLESTYLE, "width='100%'");
$th = array(_('#'), _('Shipment ID'), _('Status'), _('Carrier'), _('Tracking #'),
	_('Ship Date'), _('Packages'), _('Created'), _(''));
table_header($th);

$k = 0;
$row_num = 0;
while ($ship = db_fetch($shipments)) {
	$row_num++;
	alt_table_row_color($k);

	// Parse custom_data for carrier/tracking
	$carrier = '';
	$tracking = '';
	$ship_date = '';
	if (!empty($ship['custom_data'])) {
		$cd = json_decode($ship['custom_data'], true);
		if (is_array($cd)) {
			$carrier = isset($cd['carrier']) ? $cd['carrier'] : '';
			$tracking = isset($cd['tracking_number']) ? $cd['tracking_number'] : '';
			$ship_date = isset($cd['ship_date']) ? $cd['ship_date'] : '';
		}
	}

	label_cell($row_num);
	label_cell('#' . $ship['op_id']);
	label_cell(shipment_status_badge($ship['op_status']));
	label_cell($carrier ? htmlspecialchars($carrier) : '-');
	label_cell($tracking ? htmlspecialchars($tracking) : '-');
	label_cell($ship_date ? sql2date($ship_date) : '-');

	// Package count
	$pkg_cnt = count_shipment_packages((int)$ship['op_id']);
	label_cell($pkg_cnt);

	label_cell(sql2date(substr($ship['created_at'], 0, 10)));

	// Actions
	echo '<td nowrap>';
	echo "<button class='ajaxsubmit' type='submit' aspect='default' name='view_shipment' value='" . $ship['op_id'] . "' title='" . _('View shipment') . "'>" . set_icon(ICON_VIEW) . "<span>" . _('View') . "</span></button>\n";
	if ($ship['op_status'] === 'draft') {
		echo ' ';
		submit('Edit' . $ship['op_id'], _('Edit'), true, _('Edit'), false, ICON_EDIT);
		echo ' ';
		submit('Delete' . $ship['op_id'], _('Delete'), true, _('Delete'), false, ICON_DELETE);
	}
	echo '</td>';

	end_row();
}

if ($row_num === 0) {
	start_row();
	label_cell(_('No shipments found.'), "colspan='9' style='text-align:center;padding:20px;color:#999;'");
	end_row();
}

end_table(1);
div_end();

// =====================================================================
// New Shipment Form
// =====================================================================

display_heading($selected_id == -1 ? _('New Shipment') : _('Edit Shipment'));

start_table(TABLESTYLE2);

if ($Mode == 'Edit') {
	$ship_data = get_shipment($selected_id);
	if ($ship_data) {
		$_POST['ship_warehouse'] = $ship_data['warehouse'];
		$_POST['ship_carrier'] = $ship_data['carrier'];
		$_POST['ship_tracking'] = $ship_data['tracking_number'];
		$_POST['ship_date'] = $ship_data['ship_date'] ? sql2date($ship_data['ship_date']) : '';
		$_POST['ship_memo'] = $ship_data['memo'];
	}
}

// Warehouse selector
echo '<tr><td class="label">' . _('Warehouse:') . '</td><td>';
echo combo_input('ship_warehouse', get_post('ship_warehouse'), $sql_locs, 'loc_code', 'location_name',
	array('spec_option' => _('-- Select Warehouse --'), 'spec_id' => '', 'order' => false));
echo '</td></tr>';

text_row(_('Carrier:'), 'ship_carrier', get_post('ship_carrier'), 40, 60);
text_row(_('Tracking #:'), 'ship_tracking', get_post('ship_tracking'), 40, 100);
date_row(_('Ship Date:'), 'ship_date', null, true);
textarea_row(_('Notes:'), 'ship_memo', get_post('ship_memo'), 50, 3);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();
end_page();
