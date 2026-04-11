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
 * Report 311 — Traceability Report (Forward/Reverse)
 *
 * Forward trace: batch → all products → all customers
 * Reverse trace: serial → all component batches/serials
 * Combined: full lifecycle audit for a serial or batch
 *
 * Parameters:
 *   PARAM_0: Trace Direction (0=Forward from Batch, 1=Reverse from Serial, 2=Full Serial Lifecycle)
 *   PARAM_1: Serial/Batch Number (text)
 *   PARAM_2: Comments
 *   PARAM_3: Orientation
 *   PARAM_4: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_TRACEABILITY';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_audit_db.inc');
include_once($path_to_root . '/manufacturing/includes/db/production_traceability_db.inc');

//----------------------------------------------------------------------------------------------------

print_traceability_report();

function print_traceability_report() {
	global $path_to_root, $systypes_array;

	$trace_direction = $_POST['PARAM_0'];
	$search_text = trim($_POST['PARAM_1']);
	$comments = $_POST['PARAM_2'];
	$orientation = $_POST['PARAM_3'];
	$destination = $_POST['PARAM_4'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');

	// Determine report type
	$direction_labels = array(
		0 => _('Forward Trace (Batch → Customers)'),
		1 => _('Reverse Trace (Serial → Components)'),
		2 => _('Full Serial Lifecycle'),
	);
	$direction_label = isset($direction_labels[$trace_direction])
		? $direction_labels[$trace_direction]
		: _('Unknown');

	$params = array(
		0 => $comments,
		1 => array('text' => _('Trace Direction'), 'from' => $direction_label, 'to' => ''),
		2 => array('text' => _('Serial/Batch #'), 'from' => $search_text, 'to' => ''),
	);

	if ($search_text === '') {
		print_traceability_no_data_report(
			_('Traceability Report'),
			$params,
			$orientation,
			$destination,
			_('Please enter a serial or batch number.')
		);
		return;
	}

	// ════════════════════════════════════════════════════════════════
	// FORWARD TRACE: Batch → Products → Customers
	// ════════════════════════════════════════════════════════════════
	if ($trace_direction == 0) {
		print_forward_trace_report($search_text, $params, $orientation, $destination);
	}
	// ════════════════════════════════════════════════════════════════
	// REVERSE TRACE: Serial → Components
	// ════════════════════════════════════════════════════════════════
	elseif ($trace_direction == 1) {
		print_reverse_trace_report($search_text, $params, $orientation, $destination);
	}
	// ════════════════════════════════════════════════════════════════
	// FULL SERIAL LIFECYCLE
	// ════════════════════════════════════════════════════════════════
	elseif ($trace_direction == 2) {
		print_serial_lifecycle_report($search_text, $params, $orientation, $destination);
	}
}

/**
 * Forward trace report: Batch → Products → Customers
 */
function print_forward_trace_report($search_text, $params, $orientation, $destination) {
	global $path_to_root, $systypes_array;

	// Find the batch
	$batch = find_batch_by_text($search_text);
	if (!$batch) {
		print_traceability_no_data_report(
			_('Forward Traceability Report'),
			$params,
			$orientation,
			$destination,
			sprintf(_('Batch "%s" not found.'), $search_text)
		);
		return;
	}

	$cols = array(0, 80, 160, 280, 340, 420, 515);
	$headers = array(
		_('Customer'),
		_('Delivery #'),
		_('Product'),
		_('Qty'),
		_('Date'),
		_('Trace Path'),
	);
	$aligns = array('left', 'left', 'left', 'right', 'left', 'left');

	$rep = new FrontReport(_('Forward Traceability Report'), 'TraceForward', user_pagesize(), 9, $orientation);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	// Report header info
	$rep->Font('bold');
	$rep->TextCol(0, 6, sprintf(_('Batch: %s — Item: %s — %s — Status: %s'),
		$batch['batch_no'], $batch['stock_id'], $batch['item_description'],
		ucfirst($batch['status'])));
	$rep->NewLine();
	if ($batch['expiry_date']) {
		$rep->TextCol(0, 6, sprintf(_('Expiry: %s   Mfg: %s'),
			sql2date($batch['expiry_date']),
			$batch['manufacturing_date'] ? sql2date($batch['manufacturing_date']) : _('N/A')));
		$rep->NewLine();
	}
	$rep->Font();
	$rep->Line($rep->row - 2);
	$rep->NewLine();

	// Section 1: Production traceability (batch → finished products)
	$products_result = trace_forward_by_batch($batch['id']);
	$has_products = false;
	while ($row = db_fetch($products_result)) {
		if (!$has_products) {
			$rep->Font('bold');
			$rep->TextCol(0, 6, _('Production: Finished Products Made From This Batch'));
			$rep->Font();
			$rep->NewLine();
			$has_products = true;
		}
		$text = sprintf('%s — %s',
			$row['finished_stock_id'] ? $row['finished_stock_id'] : _('N/A'),
			$row['finished_description'] ? $row['finished_description'] : '');
		if ($row['finished_serial_no'])
			$text .= ' [SN: ' . $row['finished_serial_no'] . ']';
		if ($row['finished_batch_no'])
			$text .= ' [Batch: ' . $row['finished_batch_no'] . ']';
		$rep->TextCol(0, 5, '  ' . $text);
		$rep->AmountCol(3, 4, $row['component_qty'], get_qty_dec());
		$rep->NewLine();
	}
	if ($has_products)
		$rep->NewLine();

	// Section 2: Customer deliveries
	$customers = trace_batch_forward_to_customers($batch['id']);
	if (!empty($customers)) {
		$rep->Font('bold');
		$rep->TextCol(0, 6, _('Affected Customers'));
		$rep->Font();
		$rep->NewLine();
		$rep->Line($rep->row - 2);
		$rep->NewLine();

		foreach ($customers as $cust) {
			$rep->TextCol(0, 1, $cust['customer_name']);
			$rep->TextCol(1, 2, $cust['delivery_no'] ? '#' . $cust['delivery_no'] : '-');
			$rep->TextCol(2, 3, $cust['stock_id'] . ' - ' . $cust['item_description']);
			$rep->AmountCol(3, 4, $cust['qty_delivered'], get_qty_dec());
			$rep->TextCol(4, 5, $cust['delivery_date'] ? sql2date($cust['delivery_date']) : '-');
			$rep->TextCol(5, 6, $cust['trace_path'] === 'direct' ? _('Direct') : _('Via Production'));
			$rep->NewLine();
		}
	} else {
		$rep->TextCol(0, 6, _('No customer deliveries found for this batch.'));
		$rep->NewLine();
	}

	$rep->Line($rep->row - 4);
	$rep->NewLine();
	$rep->TextCol(0, 6, sprintf(_('Total affected customers: %d'),
		count(array_unique(array_column($customers, 'customer_id')))));
	$rep->NewLine();
	$rep->TextCol(0, 6, sprintf(_('Total deliveries: %d'), count($customers)));

	$rep->End();
}

/**
 * Reverse trace report: Serial → Components
 */
function print_reverse_trace_report($search_text, $params, $orientation, $destination) {
	global $path_to_root, $systypes_array;

	// Find the serial
	$serial = find_serial_by_text($search_text);
	if (!$serial) {
		print_traceability_no_data_report(
			_('Reverse Traceability Report'),
			$params,
			$orientation,
			$destination,
			sprintf(_('Serial number "%s" not found.'), $search_text)
		);
		return;
	}

	$cols = array(0, 60, 140, 260, 340, 420, 500);
	$headers = array(
		_('WO'),
		_('Component'),
		_('Description'),
		_('Serial #'),
		_('Batch #'),
		_('Qty Used'),
	);
	$aligns = array('left', 'left', 'left', 'left', 'left', 'right');

	$rep = new FrontReport(_('Reverse Traceability Report'), 'TraceReverse', user_pagesize(), 9, $orientation);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	// Report header info
	$rep->Font('bold');
	$rep->TextCol(0, 6, sprintf(_('Serial: %s — Item: %s — %s — Status: %s'),
		$serial['serial_no'], $serial['stock_id'], $serial['item_description'],
		ucfirst($serial['status'])));
	$rep->NewLine();
	$rep->Font();
	$rep->Line($rep->row - 2);
	$rep->NewLine();

	// Component trace
	$components_result = trace_serial_reverse_to_components($serial['id']);
	$has_data = false;
	while ($row = db_fetch($components_result)) {
		$has_data = true;
		$rep->TextCol(0, 1, $row['wo_ref'] ? $row['wo_ref'] : '#' . $row['work_order_id']);
		$rep->TextCol(1, 2, $row['component_stock_id']);
		$rep->TextCol(2, 3, $row['component_description']);
		$rep->TextCol(3, 4, $row['component_serial_no'] ? $row['component_serial_no'] : '-');
		$rep->TextCol(4, 5, $row['component_batch_no'] ? $row['component_batch_no'] : '-');
		$rep->AmountCol(5, 6, $row['component_qty'], get_qty_dec());
		$rep->NewLine();
	}

	if (!$has_data) {
		$rep->TextCol(0, 6, _('No production traceability data found for this serial number.'));
		$rep->TextCol(0, 6, _('This serial may not have been produced via a work order, or traceability was not recorded.'));
		$rep->NewLine(2);
	}

	// Also show forward (as component) products
	$products_result = trace_serial_forward_to_products($serial['id']);
	$has_forward = false;
	while ($row = db_fetch($products_result)) {
		if (!$has_forward) {
			$rep->NewLine();
			$rep->Font('bold');
			$rep->TextCol(0, 6, _('Finished Products Using This Serial as Component'));
			$rep->Font();
			$rep->NewLine();
			$rep->Line($rep->row - 2);
			$rep->NewLine();
			$has_forward = true;
		}
		$text = sprintf('%s — %s', $row['finished_stock_id'], $row['finished_description']);
		if ($row['finished_serial_no'])
			$text .= ' [SN: ' . $row['finished_serial_no'] . ']';
		if ($row['finished_batch_no'])
			$text .= ' [Batch: ' . $row['finished_batch_no'] . ']';
		$rep->TextCol(0, 1, $row['wo_ref'] ? $row['wo_ref'] : '#' . $row['work_order_id']);
		$rep->TextCol(1, 5, $text);
		$rep->AmountCol(5, 6, $row['component_qty'], get_qty_dec());
		$rep->NewLine();
	}

	$rep->End();
}

/**
 * Full serial lifecycle report: every event in chronological order
 */
function print_serial_lifecycle_report($search_text, $params, $orientation, $destination) {
	global $path_to_root, $systypes_array;

	// Find the serial
	$serial = find_serial_by_text($search_text);
	if (!$serial) {
		print_traceability_no_data_report(
			_('Serial Lifecycle Report'),
			$params,
			$orientation,
			$destination,
			sprintf(_('Serial number "%s" not found.'), $search_text)
		);
		return;
	}

	$cols = array(0, 70, 160, 230, 310, 515);
	$headers = array(
		_('Date'),
		_('Event Type'),
		_('Reference'),
		_('Detail'),
		_('User'),
	);
	$aligns = array('left', 'left', 'left', 'left', 'left');

	$rep = new FrontReport(_('Serial Lifecycle Report'), 'SerialLifecycle', user_pagesize(), 9, $orientation);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	// Report header info
	$rep->Font('bold');
	$rep->TextCol(0, 5, sprintf(_('Serial: %s — Item: %s — %s'),
		$serial['serial_no'], $serial['stock_id'], $serial['item_description']));
	$rep->NewLine();
	$rep->TextCol(0, 5, sprintf(_('Status: %s'), ucfirst($serial['status'])));
	$rep->NewLine();
	$rep->Font();

	// Summary stats
	$summary = get_serial_lifecycle_summary($serial['id']);
	$rep->TextCol(0, 5, sprintf(_('Movements: %d | QC: %d | Warranty: %d | Recalls: %d | Days in service: %d'),
		$summary['total_movements'], $summary['total_inspections'],
		$summary['total_warranty_claims'], $summary['total_recalls'],
		$summary['days_in_service']));
	$rep->NewLine();
	$rep->Line($rep->row - 2);
	$rep->NewLine();

	// Lifecycle events
	$events = get_serial_lifecycle_events($serial['id']);

	if (empty($events)) {
		$rep->TextCol(0, 5, _('No lifecycle events recorded.'));
	} else {
		$event_type_labels = array(
			'movement'            => _('Movement'),
			'quality_inspection'  => _('QC'),
			'warranty_claim'      => _('Warranty'),
			'recall'              => _('Recall'),
		);

		foreach ($events as $event) {
			$rep->TextCol(0, 1, sql2date($event['event_date']));
			$type_label = isset($event_type_labels[$event['event_type']])
				? $event_type_labels[$event['event_type']]
				: ucfirst($event['event_type']);
			$rep->TextCol(1, 2, $type_label);
			$rep->TextCol(2, 3, $event['reference'] ? $event['reference'] : '-');

			// Detail — truncate if too long
			$detail = $event['detail'] ? $event['detail'] : '-';
			if (strlen($detail) > 80)
				$detail = substr($detail, 0, 77) . '...';
			$rep->TextCol(3, 4, $detail);

			$rep->TextCol(4, 5, $event['user_name'] ? $event['user_name'] : '-');
			$rep->NewLine();

			if ($rep->row < $rep->bottomMargin + 30)
				$rep->NewPage();
		}
	}

	$rep->Line($rep->row - 4);
	$rep->NewLine();
	$rep->TextCol(0, 5, sprintf(_('Total events: %d'), count($events)));
	$rep->TextCol(0, 5, _('This report is generated from an immutable audit trail.'));

	$rep->End();
}

function print_traceability_no_data_report($title, $params, $orientation, $destination, $message) {
	$cols = array(0, 515);
	$headers = array(_('Message'));
	$aligns = array('left');

	$rep = new FrontReport($title, 'TraceabilityNoData', user_pagesize(), 9, $orientation);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();
	$rep->TextCol(0, 1, $message);
	$rep->NewLine();
	$rep->TextCol(0, 1, _('No traceability records found for the selected input.'));
	$rep->End();
}

/**
 * Find a serial number by text search (serial_no across all items).
 *
 * @param string $search_text  Serial number text to find
 * @return array|false  Serial record or false
 */
function find_serial_by_text($search_text) {
	if (empty($search_text))
		return false;

	$sql = "SELECT sn.*, sm.description AS item_description
		FROM " . TB_PREF . "serial_numbers sn
		INNER JOIN " . TB_PREF . "stock_master sm ON sn.stock_id = sm.stock_id
		WHERE sn.serial_no = " . db_escape($search_text) . "
		LIMIT 1";
	$result = db_query($sql, 'could not find serial');
	$row = db_fetch($result);
	return $row ? $row : false;
}

/**
 * Find a batch by text search (batch_no across all items).
 *
 * @param string $search_text  Batch number text to find
 * @return array|false  Batch record or false
 */
function find_batch_by_text($search_text) {
	if (empty($search_text))
		return false;

	$sql = "SELECT sb.*, sm.description AS item_description
		FROM " . TB_PREF . "stock_batches sb
		INNER JOIN " . TB_PREF . "stock_master sm ON sb.stock_id = sm.stock_id
		WHERE sb.batch_no = " . db_escape($search_text) . "
		LIMIT 1";
	$result = db_query($sql, 'could not find batch');
	$row = db_fetch($result);
	return $row ? $row : false;
}
