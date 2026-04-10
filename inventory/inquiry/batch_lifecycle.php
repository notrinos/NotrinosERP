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
 * Batch/Lot Lifecycle — Complete journey view showing every event
 * in a batch's life: creation, receipt, QC, distribution, consumption,
 * production traceability, recall, and delivery to customers.
 *
 * Provides:
 *   - Summary statistics header (movements, inbound/outbound, etc.)
 *   - Forward trace: batch → customers (direct & via production)
 *   - Reverse trace: batch → components used in its production
 *   - Forward production trace: batch (as component) → finished products
 *   - Complete chronological event timeline
 */
$page_security = 'SA_TRACEABILITY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Batch / Lot Lifecycle');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);

page($_SESSION['page_title'], isset($_GET['batch_id']), false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_audit_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');
include_once($path_to_root . '/manufacturing/includes/db/production_traceability_db.inc');

//----------------------------------------------------------------------
// Handle GET parameter
//----------------------------------------------------------------------
if (isset($_GET['batch_id']))
	$_POST['batch_id'] = (int)$_GET['batch_id'];

if (isset($_GET['batch_no']) && isset($_GET['stock_id'])) {
	$found = get_stock_batch_by_code($_GET['batch_no'], $_GET['stock_id']);
	if ($found)
		$_POST['batch_id'] = $found['id'];
}

//----------------------------------------------------------------------
// Handle search
//----------------------------------------------------------------------
if (isset($_POST['search_batch'])) {
	$search = trim(get_post('search_text'));
	if ($search !== '') {
		$result = get_stock_batches('', '', $search, false, '', 0, 1, 0);
		$row = db_fetch($result);
		if ($row) {
			$_POST['batch_id'] = $row['id'];
			$Ajax->activate('_page_body');
		} else {
			display_error(sprintf(_('No batch found matching "%s".'), htmlspecialchars($search)));
		}
	}
}

//======================================================================
//  S E A R C H   F O R M
//======================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

text_cells(_('Search Batch:'), 'search_text', get_post('search_text'), 30, 100,
	_('Enter batch number to search'));
submit_cells('search_batch', _('Find'), '', _('Search for batch number'), 'default');

end_row();
end_table();

//======================================================================
//  B A T C H   L I F E C Y C L E   D I S P L A Y
//======================================================================

$batch_id = get_post('batch_id', 0);

if ($batch_id > 0) {
	$batch = get_stock_batch($batch_id);

	if (!$batch) {
		display_error(_('Batch record not found.'));
	} else {

		// ─────────────────────────────────────────────────────────
		// Header
		// ─────────────────────────────────────────────────────────
		echo "<div style='margin:15px 0;'>";
		echo "<h3 style='margin:0 0 5px 0;'>"
			. sprintf(_('Batch Lifecycle: %s'), '<strong>' . htmlspecialchars($batch['batch_no']) . '</strong>')
			. " &mdash; " . batch_status_badge($batch['status']);
		if ($batch['expiry_date'])
			echo " &mdash; " . batch_expiry_badge($batch['expiry_date']);
		echo "</h3>";
		echo "<div style='color:#666;font-size:12px;'>"
			. sprintf(_('Item: %s — %s'), htmlspecialchars($batch['stock_id']), htmlspecialchars($batch['item_description']))
			. "</div>";
		echo "</div>";

		// ─────────────────────────────────────────────────────────
		// Summary Statistics Cards
		// ─────────────────────────────────────────────────────────
		$summary = get_batch_lifecycle_summary($batch_id);

		echo "<div style='display:flex;gap:15px;margin:10px 0 20px 0;flex-wrap:wrap;'>";

		display_batch_lifecycle_stat_card(_('Movements'), $summary['total_movements'], '#337ab7');
		display_batch_lifecycle_stat_card(_('QC Inspections'), $summary['total_inspections'], '#5bc0de');
		display_batch_lifecycle_stat_card(_('Recalls'), $summary['total_recalls'], '#d9534f');
		display_batch_lifecycle_stat_card(_('Total Inbound'),
			'+' . number_format2($summary['total_inbound'], get_qty_dec()), '#5cb85c');
		display_batch_lifecycle_stat_card(_('Total Outbound'),
			'-' . number_format2($summary['total_outbound'], get_qty_dec()), '#f0ad4e');
		display_batch_lifecycle_stat_card(_('Net Qty'),
			number_format2($summary['total_inbound'] - $summary['total_outbound'], get_qty_dec()), '#333');

		if ($summary['first_event_date'])
			display_batch_lifecycle_stat_card(_('First Event'), sql2date($summary['first_event_date']), '#777');
		if ($summary['last_event_date'])
			display_batch_lifecycle_stat_card(_('Last Event'), sql2date($summary['last_event_date']), '#777');

		echo "</div>";

		// ─────────────────────────────────────────────────────────
		// Quick Links
		// ─────────────────────────────────────────────────────────
		echo "<div style='margin:10px 0;'>";
		echo "<a href='{$path_to_root}/inventory/inquiry/batch_inquiry.php?batch_id={$batch_id}'>"
			. _('View Batch Details') . "</a>";
		echo " | <a href='{$path_to_root}/inventory/manage/stock_batches.php'>"
			. _('Manage Batches') . "</a>";
		echo " | <a href='{$path_to_root}/inventory/inquiry/expiry_dashboard.php'>"
			. _('Expiry Dashboard') . "</a>";
		echo "</div>";

		// ─────────────────────────────────────────────────────────
		// Forward Trace: Batch → Customers (direct + production)
		// ─────────────────────────────────────────────────────────
		$customers = trace_batch_forward_to_customers($batch_id);
		if (!empty($customers)) {
			echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #f0ad4e;padding-bottom:3px;color:#f0ad4e;'>"
				. _('Forward Trace — Affected Customers') . "</h4>";
			echo "<p style='color:#666;font-size:12px;margin:0 0 10px 0;'>"
				. _('All customers who received products containing or produced from this batch.') . "</p>";

			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('Customer'), _('Delivery #'), _('Date'), _('Product'),
				_('Qty'), _('Trace Path'));
			table_header($th);

			$k = 0;
			foreach ($customers as $cust) {
				alt_table_row_color($k);
				label_cell(htmlspecialchars($cust['customer_name']));
				label_cell($cust['delivery_no']
					? get_trans_view_str(ST_CUSTDELIVERY, $cust['delivery_no'])
					: '-');
				label_cell($cust['delivery_date'] ? sql2date($cust['delivery_date']) : '-');
				label_cell(htmlspecialchars($cust['stock_id']) . ' - ' . htmlspecialchars($cust['item_description']));
				qty_cell($cust['qty_delivered']);

				$path_label = $cust['trace_path'] === 'direct'
					? "<span style='color:#5cb85c;'>" . _('Direct Delivery') . "</span>"
					: "<span style='color:#337ab7;'>" . _('Via Production') . "</span>";
				label_cell($path_label);

				end_row();
			}
			end_table();
		}

		// ─────────────────────────────────────────────────────────
		// Production Traceability — Reverse (what went INTO this batch?)
		// ─────────────────────────────────────────────────────────
		$components_result = trace_batch_reverse_to_components($batch_id);
		$components = array();
		while ($row = db_fetch($components_result)) {
			$components[] = $row;
		}

		if (!empty($components)) {
			echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #5cb85c;padding-bottom:3px;color:#5cb85c;'>"
				. _('Production Traceability — Components Used') . "</h4>";
			echo "<p style='color:#666;font-size:12px;margin:0 0 10px 0;'>"
				. _('Component materials consumed during production of this batch.') . "</p>";

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
		// Production Traceability — Forward (batch used as component → products)
		// ─────────────────────────────────────────────────────────
		$products_result = trace_batch_forward_to_products($batch_id);
		$products = array();
		while ($row = db_fetch($products_result)) {
			$products[] = $row;
		}

		if (!empty($products)) {
			echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #337ab7;padding-bottom:3px;color:#337ab7;'>"
				. _('Production Traceability — Used in Products') . "</h4>";
			echo "<p style='color:#666;font-size:12px;margin:0 0 10px 0;'>"
				. _('This batch was consumed as a component in the following finished products.') . "</p>";

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
		// Location Breakdown (stock by location)
		// ─────────────────────────────────────────────────────────
		echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #666;padding-bottom:3px;'>"
			. _('Current Stock by Location') . "</h4>";
		display_batch_location_breakdown($batch_id);

		// ─────────────────────────────────────────────────────────
		// Complete Lifecycle Timeline
		// ─────────────────────────────────────────────────────────
		echo "<h4 style='margin:20px 0 5px 0;border-bottom:2px solid #333;padding-bottom:3px;'>"
			. _('Complete Lifecycle Timeline') . "</h4>";
		echo "<p style='color:#666;font-size:12px;margin:0 0 10px 0;'>"
			. _('All events in chronological order (newest first). This log is immutable and cannot be edited.') . "</p>";

		$events = get_batch_lifecycle_events($batch_id);

		if (empty($events)) {
			display_note(_('No lifecycle events recorded for this batch.'));
		} else {
			start_table(TABLESTYLE, "width='95%'");
			$th = array(_('Date'), _('Event Type'), _('Trans #'), _('Reference'),
				_('Location'), _('Quantity'), _('Detail'), _('User'));
			table_header($th);

			$k = 0;
			foreach ($events as $event) {
				alt_table_row_color($k);

				label_cell(sql2date($event['event_date']));
				label_cell(lifecycle_event_badge($event['event_type']));

				// Transaction link
				if (isset($event['trans_type']) && $event['trans_type'] > 0 && $event['trans_no'] > 0)
					label_cell(get_trans_view_str($event['trans_type'], $event['trans_no']));
				else
					label_cell('-');

				label_cell($event['reference'] ? htmlspecialchars($event['reference']) : '-');

				// Location
				label_cell(isset($event['location']) && $event['location']
					? htmlspecialchars($event['location']) : '-');

				// Quantity (colored)
				if (isset($event['quantity']) && $event['quantity'] !== null) {
					$qty_color = $event['quantity'] >= 0 ? '#5cb85c' : '#d9534f';
					label_cell("<span style='color:$qty_color;font-weight:bold;'>"
						. $event['quantity_str'] . "</span>", "align='right'");
				} else {
					label_cell('-', "align='right'");
				}

				label_cell($event['detail'] ? $event['detail'] : '-');
				label_cell(isset($event['user_name']) && $event['user_name']
					? htmlspecialchars($event['user_name']) : '-');

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
 * Display a summary statistics card for batch lifecycle.
 *
 * @param string $label  Card label
 * @param mixed  $value  Card value (number or text)
 * @param string $color  Border/accent color
 */
function display_batch_lifecycle_stat_card($label, $value, $color) {
	echo "<div style='border:1px solid #ddd;border-left:4px solid $color;padding:10px 15px;"
		. "min-width:100px;background:#fff;border-radius:3px;'>"
		. "<div style='font-size:20px;font-weight:bold;color:$color;'>" . htmlspecialchars($value) . "</div>"
		. "<div style='font-size:11px;color:#888;margin-top:2px;'>" . htmlspecialchars($label) . "</div>"
		. "</div>";
}
