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
 * Serial Number Lifecycle — Complete visual timeline showing every event
 * in a serial number's life: receive, QC, transfer, deliver, return,
 * warranty, recall, production traceability, etc.
 *
 * Provides:
 *   - Summary statistics header (total events, days in service, etc.)
 *   - Visual chronological timeline of all events
 *   - Production traceability section (component → product links)
 *   - Delivery history (which customers received this serial)
 *   - Receipt history (which supplier provided this serial)
 */
$page_security = 'SA_TRACEABILITY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Serial Number Lifecycle');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);

page($_SESSION['page_title'], isset($_GET['serial_id']), false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_audit_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');
include_once($path_to_root . '/manufacturing/includes/db/production_traceability_db.inc');

//----------------------------------------------------------------------
// Handle GET parameter
//----------------------------------------------------------------------
if (isset($_GET['serial_id']))
	$_POST['serial_id'] = (int)$_GET['serial_id'];

if (isset($_GET['serial_no']) && isset($_GET['stock_id'])) {
	$found = get_serial_number_by_code($_GET['serial_no'], $_GET['stock_id']);
	if ($found)
		$_POST['serial_id'] = $found['id'];
}

//----------------------------------------------------------------------
// Handle search
//----------------------------------------------------------------------
if (isset($_POST['search_serial'])) {
	$search = trim(get_post('search_text'));
	if ($search !== '') {
		$result = get_serial_numbers('', '', '', $search, false, '', '', 1, 0);
		$row = db_fetch($result);
		if ($row) {
			$_POST['serial_id'] = $row['id'];
			$Ajax->activate('_page_body');
		} else {
			display_error(sprintf(_('No serial number found matching "%s".'), htmlspecialchars($search)));
		}
	}
}

//======================================================================
//  S E A R C H   F O R M
//======================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

text_cells(_('Search Serial:'), 'search_text', get_post('search_text'), 30, 100,
	_('Enter serial number to search'));
submit_cells('search_serial', _('Find'), '', _('Search for serial number'), 'default');

end_row();
end_table();

//======================================================================
//  S E R I A L   L I F E C Y C L E   D I S P L A Y
//======================================================================

$serial_id = get_post('serial_id', 0);

if ($serial_id > 0) {
	$serial = get_serial_number($serial_id);

	if (!$serial) {
		display_error(_('Serial number record not found.'));
	} else {

		// ─────────────────────────────────────────────────────────
		// Header
		// ─────────────────────────────────────────────────────────
		echo "<div style='margin:15px 0;'>";
		echo "<h3 style='margin:0 0 5px 0;'>"
			. sprintf(_('Serial Lifecycle: %s'), '<strong>' . htmlspecialchars($serial['serial_no']) . '</strong>')
			. " &mdash; " . serial_status_badge($serial['status'])
			. "</h3>";
		echo "<div style='color:#666;font-size:12px;'>"
			. sprintf(_('Item: %s — %s'), htmlspecialchars($serial['stock_id']), htmlspecialchars($serial['item_description']))
			. "</div>";
		echo "</div>";

		// ─────────────────────────────────────────────────────────
		// Summary Statistics Cards
		// ─────────────────────────────────────────────────────────
		$summary = get_serial_lifecycle_summary($serial_id);

		echo "<div style='display:flex;gap:15px;margin:10px 0 20px 0;flex-wrap:wrap;'>";

		display_lifecycle_stat_card(_('Movements'), $summary['total_movements'], '#337ab7');
		display_lifecycle_stat_card(_('QC Inspections'), $summary['total_inspections'], '#5bc0de');
		display_lifecycle_stat_card(_('Warranty Claims'), $summary['total_warranty_claims'], '#f0ad4e');
		display_lifecycle_stat_card(_('Recalls'), $summary['total_recalls'], '#d9534f');
		display_lifecycle_stat_card(_('Days in Service'), $summary['days_in_service'], '#5cb85c');

		if ($summary['first_event_date'])
			display_lifecycle_stat_card(_('First Event'), sql2date($summary['first_event_date']), '#777');
		if ($summary['last_event_date'])
			display_lifecycle_stat_card(_('Last Event'), sql2date($summary['last_event_date']), '#777');

		echo "</div>";

		// ─────────────────────────────────────────────────────────
		// Quick Links
		// ─────────────────────────────────────────────────────────
		echo "<div style='margin:10px 0;'>";
		echo "<a href='{$path_to_root}/inventory/inquiry/serial_inquiry.php?serial_id={$serial_id}'>"
			. _('View Serial Details') . "</a>";
		echo " | <a href='{$path_to_root}/inventory/manage/serial_numbers.php'>"
			. _('Manage Serials') . "</a>";
		echo "</div>";

		// ─────────────────────────────────────────────────────────
		// Production Traceability — Reverse (what went INTO this serial?)
		// ─────────────────────────────────────────────────────────
		$components_result = trace_serial_reverse_to_components($serial_id);
		$has_components = false;
		$components = array();
		while ($row = db_fetch($components_result)) {
			$has_components = true;
			$components[] = $row;
		}

		if ($has_components) {
			echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #5cb85c;padding-bottom:3px;color:#5cb85c;'>"
				. _('Production Traceability — Components Used') . "</h4>";
			echo "<p style='color:#666;font-size:12px;margin:0 0 10px 0;'>"
				. _('These component materials were consumed during production of this serial.') . "</p>";

			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('Work Order'), _('Component'), _('Description'), _('Serial #'),
				_('Batch #'), _('Batch Expiry'), _('Qty Used'));
			table_header($th);

			$k = 0;
			foreach ($components as $comp) {
				alt_table_row_color($k);
				label_cell($comp['wo_ref'] ? $comp['wo_ref'] : '#' . $comp['work_order_id']);
				label_cell(htmlspecialchars($comp['component_stock_id']));
				label_cell(htmlspecialchars($comp['component_description']));

				if ($comp['component_serial_no']) {
					label_cell("<a href='serial_lifecycle.php?serial_id=" . $comp['component_serial_id'] . "'>"
						. htmlspecialchars($comp['component_serial_no']) . "</a>");
				} else {
					label_cell('-');
				}

				if ($comp['component_batch_no']) {
					label_cell("<a href='batch_lifecycle.php?batch_id=" . $comp['component_batch_id'] . "'>"
						. htmlspecialchars($comp['component_batch_no']) . "</a>");
				} else {
					label_cell('-');
				}

				label_cell($comp['component_batch_expiry'] ? sql2date($comp['component_batch_expiry']) : '-');
				qty_cell($comp['component_qty']);
				end_row();
			}
			end_table();
		}

		// ─────────────────────────────────────────────────────────
		// Production Traceability — Forward (what was PRODUCED from this serial as component?)
		// ─────────────────────────────────────────────────────────
		$products_result = trace_serial_forward_to_products($serial_id);
		$has_products = false;
		$products = array();
		while ($row = db_fetch($products_result)) {
			$has_products = true;
			$products[] = $row;
		}

		if ($has_products) {
			echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #337ab7;padding-bottom:3px;color:#337ab7;'>"
				. _('Production Traceability — Used in Products') . "</h4>";
			echo "<p style='color:#666;font-size:12px;margin:0 0 10px 0;'>"
				. _('This serial was consumed as a component in the following finished products.') . "</p>";

			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('Work Order'), _('Finished Product'), _('Description'), _('Serial #'),
				_('Batch #'), _('Qty'));
			table_header($th);

			$k = 0;
			foreach ($products as $prod) {
				alt_table_row_color($k);
				label_cell($prod['wo_ref'] ? $prod['wo_ref'] : '#' . $prod['work_order_id']);
				label_cell(htmlspecialchars($prod['finished_stock_id']));
				label_cell(htmlspecialchars($prod['finished_description']));

				if ($prod['finished_serial_no']) {
					$badge = '';
					if (function_exists('serial_status_badge') && $prod['finished_serial_status'])
						$badge = ' ' . serial_status_badge($prod['finished_serial_status']);
					label_cell("<a href='serial_lifecycle.php?serial_id=" . $prod['finished_serial_id'] . "'>"
						. htmlspecialchars($prod['finished_serial_no']) . "</a>" . $badge);
				} else {
					label_cell('-');
				}

				if ($prod['finished_batch_no']) {
					label_cell("<a href='batch_lifecycle.php?batch_id=" . $prod['finished_batch_id'] . "'>"
						. htmlspecialchars($prod['finished_batch_no']) . "</a>");
				} else {
					label_cell('-');
				}

				qty_cell($prod['component_qty']);
				end_row();
			}
			end_table();
		}

		// ─────────────────────────────────────────────────────────
		// Delivery History (which customers received this serial?)
		// ─────────────────────────────────────────────────────────
		$deliveries = get_serial_delivery_history($serial_id);
		if (!empty($deliveries)) {
			echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #f0ad4e;padding-bottom:3px;color:#f0ad4e;'>"
				. _('Customer Delivery History') . "</h4>";

			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('Delivery #'), _('Date'), _('Item'), _('Qty'), _('Customer'));
			table_header($th);

			$k = 0;
			foreach ($deliveries as $del) {
				alt_table_row_color($k);
				label_cell(get_trans_view_str(ST_CUSTDELIVERY, $del['delivery_no']));
				label_cell(sql2date($del['delivery_date']));
				label_cell(htmlspecialchars($del['stock_id']) . ' - ' . htmlspecialchars($del['item_description']));
				qty_cell($del['qty']);
				label_cell(htmlspecialchars($del['customer_name']));
				end_row();
			}
			end_table();
		}

		// ─────────────────────────────────────────────────────────
		// Receipt History (which supplier provided this serial?)
		// ─────────────────────────────────────────────────────────
		$receipts = get_serial_receipt_history($serial_id);
		if (!empty($receipts)) {
			echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #5bc0de;padding-bottom:3px;color:#5bc0de;'>"
				. _('Supplier Receipt History') . "</h4>";

			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('GRN #'), _('Date'), _('Item'), _('Qty'), _('Supplier'));
			table_header($th);

			$k = 0;
			foreach ($receipts as $rec) {
				alt_table_row_color($k);
				label_cell(get_trans_view_str(ST_SUPPRECEIVE, $rec['grn_no']));
				label_cell(sql2date($rec['receipt_date']));
				label_cell(htmlspecialchars($rec['stock_id']) . ' - ' . htmlspecialchars($rec['item_description']));
				qty_cell($rec['qty']);
				label_cell($rec['supplier_name'] ? htmlspecialchars($rec['supplier_name']) : '-');
				end_row();
			}
			end_table();
		}

		// ─────────────────────────────────────────────────────────
		// Complete Lifecycle Timeline (all events chronologically)
		// ─────────────────────────────────────────────────────────
		echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #333;padding-bottom:3px;'>"
			. _('Complete Lifecycle Timeline') . "</h4>";
		echo "<p style='color:#666;font-size:12px;margin:0 0 10px 0;'>"
			. _('All events in chronological order (newest first). This log is immutable and cannot be edited.') . "</p>";

		$events = get_serial_lifecycle_events($serial_id);

		if (empty($events)) {
			display_note(_('No lifecycle events recorded for this serial number.'));
		} else {
			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('Date'), _('Event Type'), _('Trans #'), _('Reference'),
				_('Detail'), _('Notes'), _('User'));
			table_header($th);

			$k = 0;
			foreach ($events as $event) {
				alt_table_row_color($k);

				label_cell(sql2date($event['event_date']));
				label_cell(lifecycle_event_badge($event['event_type']));

				// Transaction link
				if ($event['trans_type'] > 0 && $event['trans_no'] > 0)
					label_cell(get_trans_view_str($event['trans_type'], $event['trans_no']));
				else
					label_cell('-');

				label_cell($event['reference'] ? htmlspecialchars($event['reference']) : '-');
				label_cell($event['detail'] ? $event['detail'] : '-');
				label_cell($event['notes'] ? htmlspecialchars($event['notes']) : '-');
				label_cell($event['user_name'] ? htmlspecialchars($event['user_name']) : '-');

				end_row();
			}
			end_table();
		}
	}
}

end_form();
end_page();

//======================================================================
//  H E L P E R   F U N C T I O N S
//======================================================================

/**
 * Display a summary statistics card.
 *
 * @param string $label  Card label
 * @param mixed  $value  Card value (number or text)
 * @param string $color  Border/accent color
 */
function display_lifecycle_stat_card($label, $value, $color) {
	echo "<div style='border:1px solid #ddd;border-left:4px solid $color;padding:10px 15px;"
		. "min-width:100px;background:#fff;border-radius:3px;'>"
		. "<div style='font-size:20px;font-weight:bold;color:$color;'>" . htmlspecialchars($value) . "</div>"
		. "<div style='font-size:11px;color:#888;margin-top:2px;'>" . htmlspecialchars($label) . "</div>"
		. "</div>";
}
