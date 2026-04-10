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
 * Serial & Batch Audit Trail — Immutable Lifecycle History & Traceability
 *
 * Provides consolidated lifecycle timeline views for serial numbers
 * and batches by aggregating data from multiple sources:
 *   - serial_movements / batch_movements (core movement ledger)
 *   - quality_inspections (QC events)
 *   - warranty_claims (warranty events)
 *   - recall_items (recall events)
 *   - production_traceability (manufacturing links)
 *   - stock_moves (delivery/GRN links for customer/supplier context)
 *
 * All functions are read-only — no INSERT/UPDATE/DELETE. The underlying
 * tables provide the immutable audit trail.
 */

/**
 * Get the complete lifecycle timeline for a serial number.
 *
 * Aggregates events from serial_movements, quality_inspections,
 * warranty_claims, and recall_items into a single chronological list.
 *
 * @param int $serial_id  Serial number record ID
 * @return array  Array of event objects sorted by date descending
 */
function get_serial_lifecycle_events($serial_id) {
	$serial_id = (int)$serial_id;
	$events = array();

	// 1. Serial movements (core ledger)
	$sql = "SELECT sm.id, sm.tran_date, sm.trans_type, sm.trans_no, sm.reference,
			sm.from_loc, sm.to_loc, sm.from_status, sm.to_status,
			sm.notes, sm.created_by, sm.created_at,
			fl.location_name AS from_location_name,
			tl.location_name AS to_location_name,
			u.real_name AS user_name
		FROM " . TB_PREF . "serial_movements sm
		LEFT JOIN " . TB_PREF . "locations fl ON sm.from_loc = fl.loc_code
		LEFT JOIN " . TB_PREF . "locations tl ON sm.to_loc = tl.loc_code
		LEFT JOIN " . TB_PREF . "users u ON sm.created_by = u.id
		WHERE sm.serial_id = $serial_id
		ORDER BY sm.tran_date DESC, sm.id DESC";
	$result = db_query($sql, 'could not get serial movements for lifecycle');
	while ($row = db_fetch($result)) {
		$event = array(
			'event_type'   => 'movement',
			'event_date'   => $row['tran_date'],
			'event_time'   => $row['created_at'],
			'trans_type'   => $row['trans_type'],
			'trans_no'     => $row['trans_no'],
			'reference'    => $row['reference'],
			'from_loc'     => $row['from_location_name'],
			'to_loc'       => $row['to_location_name'],
			'from_status'  => $row['from_status'],
			'to_status'    => $row['to_status'],
			'notes'        => $row['notes'],
			'user_name'    => $row['user_name'],
			'detail'       => build_movement_detail($row),
		);
		$events[] = $event;
	}

	// 2. Quality inspections
	$sql = "SELECT qi.id, qi.inspection_date, qi.reference, qi.result,
			qi.notes, qi.created_at,
			u.real_name AS user_name,
			insp.real_name AS inspector_name
		FROM " . TB_PREF . "quality_inspections qi
		LEFT JOIN " . TB_PREF . "users u ON qi.created_by = u.id
		LEFT JOIN " . TB_PREF . "users insp ON qi.inspector_id = insp.id
		WHERE qi.serial_id = $serial_id
		ORDER BY qi.inspection_date DESC";
	$result = db_query($sql, 'could not get QC events for lifecycle');
	while ($row = db_fetch($result)) {
		$events[] = array(
			'event_type'   => 'quality_inspection',
			'event_date'   => $row['inspection_date'],
			'event_time'   => $row['created_at'],
			'trans_type'   => 0,
			'trans_no'     => $row['id'],
			'reference'    => $row['reference'],
			'from_loc'     => null,
			'to_loc'       => null,
			'from_status'  => null,
			'to_status'    => $row['result'],
			'notes'        => $row['notes'],
			'user_name'    => $row['inspector_name'] ? $row['inspector_name'] : $row['user_name'],
			'detail'       => sprintf(_('QC Inspection: %s — Result: %s'), $row['reference'], strtoupper($row['result'])),
		);
	}

	// 3. Warranty claims
	$sql = "SELECT wc.id, wc.reference, wc.claim_date, wc.status,
			wc.issue_type, wc.issue_description, wc.resolution_description,
			wc.resolution_date, wc.repair_cost,
			dm.name AS customer_name,
			u.real_name AS user_name
		FROM " . TB_PREF . "warranty_claims wc
		LEFT JOIN " . TB_PREF . "debtors_master dm ON wc.customer_id = dm.debtor_no
		LEFT JOIN " . TB_PREF . "users u ON wc.created_by = u.id
		WHERE wc.serial_id = $serial_id
		ORDER BY wc.claim_date DESC";
	$result = db_query($sql, 'could not get warranty events for lifecycle');
	while ($row = db_fetch($result)) {
		$events[] = array(
			'event_type'   => 'warranty_claim',
			'event_date'   => $row['claim_date'],
			'event_time'   => null,
			'trans_type'   => 0,
			'trans_no'     => $row['id'],
			'reference'    => $row['reference'],
			'from_loc'     => null,
			'to_loc'       => null,
			'from_status'  => null,
			'to_status'    => $row['status'],
			'notes'        => $row['issue_description'],
			'user_name'    => $row['user_name'],
			'detail'       => sprintf(_('Warranty Claim %s: %s — %s (Customer: %s)'),
				$row['reference'], ucfirst($row['issue_type']), ucfirst($row['status']),
				$row['customer_name'] ? $row['customer_name'] : _('N/A')),
		);
	}

	// 4. Recall items
	$sql = "SELECT ri.id, ri.status, ri.notification_date, ri.return_date,
			ri.resolution_date, ri.notes,
			rc.reference AS campaign_reference, rc.title AS campaign_title,
			rc.severity, rc.created_at AS campaign_created_at
		FROM " . TB_PREF . "recall_items ri
		INNER JOIN " . TB_PREF . "recall_campaigns rc ON ri.recall_id = rc.id
		WHERE ri.serial_id = $serial_id
		ORDER BY ri.notification_date DESC";
	$result = db_query($sql, 'could not get recall events for lifecycle');
	while ($row = db_fetch($result)) {
		$events[] = array(
			'event_type'   => 'recall',
			'event_date'   => $row['notification_date'] ? $row['notification_date'] : substr($row['campaign_created_at'], 0, 10),
			'event_time'   => $row['campaign_created_at'],
			'trans_type'   => 0,
			'trans_no'     => $row['id'],
			'reference'    => $row['campaign_reference'],
			'from_loc'     => null,
			'to_loc'       => null,
			'from_status'  => null,
			'to_status'    => $row['status'],
			'notes'        => $row['notes'],
			'user_name'    => null,
			'detail'       => sprintf(_('Recall Campaign %s: %s — Status: %s (Severity: %s)'),
				$row['campaign_reference'], $row['campaign_title'],
				ucfirst($row['status']), ucfirst($row['severity'])),
		);
	}

	// Sort all events by date descending, then by event_time descending
	usort($events, function($a, $b) {
		$date_cmp = strcmp($b['event_date'], $a['event_date']);
		if ($date_cmp !== 0) return $date_cmp;
		$time_a = $a['event_time'] ? $a['event_time'] : '';
		$time_b = $b['event_time'] ? $b['event_time'] : '';
		return strcmp($time_b, $time_a);
	});

	return $events;
}

/**
 * Get the complete lifecycle timeline for a batch.
 *
 * Aggregates events from batch_movements, quality_inspections,
 * recall_items, and delivery/customer links.
 *
 * @param int $batch_id  Batch record ID
 * @return array  Array of event objects sorted by date descending
 */
function get_batch_lifecycle_events($batch_id) {
	$batch_id = (int)$batch_id;
	$events = array();

	// 1. Batch movements (core ledger)
	$sql = "SELECT bm.id, bm.tran_date, bm.trans_type, bm.trans_no,
			bm.loc_code, bm.quantity, bm.reference, bm.notes,
			bm.created_by, bm.created_at,
			l.location_name,
			u.real_name AS user_name
		FROM " . TB_PREF . "batch_movements bm
		LEFT JOIN " . TB_PREF . "locations l ON bm.loc_code = l.loc_code
		LEFT JOIN " . TB_PREF . "users u ON bm.created_by = u.id
		WHERE bm.batch_id = $batch_id
		ORDER BY bm.tran_date DESC, bm.id DESC";
	$result = db_query($sql, 'could not get batch movements for lifecycle');
	while ($row = db_fetch($result)) {
		$qty_str = $row['quantity'] >= 0
			? '+' . number_format2($row['quantity'], get_qty_dec())
			: number_format2($row['quantity'], get_qty_dec());
		$events[] = array(
			'event_type'   => 'movement',
			'event_date'   => $row['tran_date'],
			'event_time'   => $row['created_at'],
			'trans_type'   => $row['trans_type'],
			'trans_no'     => $row['trans_no'],
			'reference'    => $row['reference'],
			'location'     => $row['location_name'],
			'quantity'     => $row['quantity'],
			'quantity_str' => $qty_str,
			'notes'        => $row['notes'],
			'user_name'    => $row['user_name'],
			'detail'       => build_batch_movement_detail($row),
		);
	}

	// 2. Quality inspections
	$sql = "SELECT qi.id, qi.inspection_date, qi.reference, qi.result,
			qi.notes, qi.created_at,
			u.real_name AS user_name,
			insp.real_name AS inspector_name
		FROM " . TB_PREF . "quality_inspections qi
		LEFT JOIN " . TB_PREF . "users u ON qi.created_by = u.id
		LEFT JOIN " . TB_PREF . "users insp ON qi.inspector_id = insp.id
		WHERE qi.batch_id = $batch_id
		ORDER BY qi.inspection_date DESC";
	$result = db_query($sql, 'could not get batch QC events for lifecycle');
	while ($row = db_fetch($result)) {
		$events[] = array(
			'event_type'   => 'quality_inspection',
			'event_date'   => $row['inspection_date'],
			'event_time'   => $row['created_at'],
			'trans_type'   => 0,
			'trans_no'     => $row['id'],
			'reference'    => $row['reference'],
			'location'     => null,
			'quantity'     => null,
			'quantity_str' => null,
			'notes'        => $row['notes'],
			'user_name'    => $row['inspector_name'] ? $row['inspector_name'] : $row['user_name'],
			'detail'       => sprintf(_('QC Inspection: %s — Result: %s'), $row['reference'], strtoupper($row['result'])),
		);
	}

	// 3. Recall items linked to this batch
	$sql = "SELECT ri.id, ri.status, ri.notification_date, ri.notes,
			rc.reference AS campaign_reference, rc.title AS campaign_title,
			rc.severity, rc.created_at AS campaign_created_at
		FROM " . TB_PREF . "recall_items ri
		INNER JOIN " . TB_PREF . "recall_campaigns rc ON ri.recall_id = rc.id
		WHERE ri.batch_id = $batch_id
		ORDER BY ri.notification_date DESC";
	$result = db_query($sql, 'could not get batch recall events for lifecycle');
	while ($row = db_fetch($result)) {
		$events[] = array(
			'event_type'   => 'recall',
			'event_date'   => $row['notification_date'] ? $row['notification_date'] : substr($row['campaign_created_at'], 0, 10),
			'event_time'   => $row['campaign_created_at'],
			'trans_type'   => 0,
			'trans_no'     => $row['id'],
			'reference'    => $row['campaign_reference'],
			'location'     => null,
			'quantity'     => null,
			'quantity_str' => null,
			'notes'        => $row['notes'],
			'user_name'    => null,
			'detail'       => sprintf(_('Recall Campaign %s: %s — Status: %s (Severity: %s)'),
				$row['campaign_reference'], $row['campaign_title'],
				ucfirst($row['status']), ucfirst($row['severity'])),
		);
	}

	// Sort all events by date descending, then by event_time descending
	usort($events, function($a, $b) {
		$date_cmp = strcmp($b['event_date'], $a['event_date']);
		if ($date_cmp !== 0) return $date_cmp;
		$time_a = $a['event_time'] ? $a['event_time'] : '';
		$time_b = $b['event_time'] ? $b['event_time'] : '';
		return strcmp($time_b, $time_a);
	});

	return $events;
}

/**
 * Forward trace: given a batch, find all customers who received products
 * produced from or directly containing this batch.
 *
 * Two paths:
 *   1. Direct delivery — batch shipped directly to customer via delivery note
 *   2. Production chain — batch consumed in manufacturing, finished goods delivered
 *
 * @param int $batch_id  Batch record ID
 * @return array  Array of customer records with delivery details
 */
function trace_batch_forward_to_customers($batch_id) {
	$batch_id = (int)$batch_id;
	$customers = array();

	// Path 1: Direct deliveries of this batch
	$sql = "SELECT DISTINCT
			smov.trans_no AS delivery_no,
			smov.tran_date AS delivery_date,
			smov.stock_id,
			sm.description AS item_description,
			ABS(smov.qty) AS qty_delivered,
			dt.debtor_no AS customer_id,
			dm.name AS customer_name,
			'direct' AS trace_path
		FROM " . TB_PREF . "stock_moves smov
		INNER JOIN " . TB_PREF . "stock_master sm ON smov.stock_id = sm.stock_id
		INNER JOIN " . TB_PREF . "debtor_trans dt ON dt.trans_no = smov.trans_no AND dt.type = " . ST_CUSTDELIVERY . "
		INNER JOIN " . TB_PREF . "debtors_master dm ON dt.debtor_no = dm.debtor_no
		WHERE smov.type = " . ST_CUSTDELIVERY . "
		AND smov.batch_id = $batch_id
		AND smov.qty < 0
		ORDER BY smov.tran_date DESC";
	$result = db_query($sql, 'could not trace batch direct deliveries');
	while ($row = db_fetch($result)) {
		$customers[] = $row;
	}

	// Path 2: via production traceability (batch consumed → finished good → delivery)
	$sql = "SELECT DISTINCT
			smov.trans_no AS delivery_no,
			smov.tran_date AS delivery_date,
			wo.stock_id AS stock_id,
			fsm.description AS item_description,
			ABS(smov.qty) AS qty_delivered,
			dt.debtor_no AS customer_id,
			dm.name AS customer_name,
			'production' AS trace_path
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master fsm ON wo.stock_id = fsm.stock_id
		INNER JOIN " . TB_PREF . "stock_moves smov ON smov.stock_id = wo.stock_id
			AND smov.type = " . ST_CUSTDELIVERY . "
			AND smov.qty < 0
			AND (smov.serial_id = pt.finished_serial_id OR smov.batch_id = pt.finished_batch_id)
		INNER JOIN " . TB_PREF . "debtor_trans dt ON dt.trans_no = smov.trans_no AND dt.type = " . ST_CUSTDELIVERY . "
		INNER JOIN " . TB_PREF . "debtors_master dm ON dt.debtor_no = dm.debtor_no
		WHERE pt.component_batch_id = $batch_id
		ORDER BY smov.tran_date DESC";
	$result = db_query($sql, 'could not trace batch production deliveries');
	while ($row = db_fetch($result)) {
		$customers[] = $row;
	}

	return $customers;
}

/**
 * Reverse trace: given a serial number, find all component batches/serials used
 * in its production (via production traceability table).
 *
 * @param int $serial_id  Serial number record ID
 * @return array  Array of component records with batch/serial details
 */
function trace_serial_reverse_to_components($serial_id) {
	$serial_id = (int)$serial_id;

	$sql = "SELECT pt.*,
			wo.wo_ref,
			wo.stock_id AS finished_stock_id,
			fsm.description AS finished_description,
			csm.description AS component_description,
			csn.serial_no AS component_serial_no,
			cb.batch_no AS component_batch_no,
			cb.expiry_date AS component_batch_expiry,
			cb.status AS component_batch_status
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master fsm ON wo.stock_id = fsm.stock_id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers csn ON pt.component_serial_id = csn.id
		LEFT JOIN " . TB_PREF . "stock_batches cb ON pt.component_batch_id = cb.id
		WHERE pt.finished_serial_id = $serial_id
		ORDER BY pt.component_stock_id, pt.created_at DESC";
	return db_query($sql, 'could not trace serial reverse to components');
}

/**
 * Forward trace: given a serial number (as a component), find all finished products
 * it was used to produce.
 *
 * @param int $serial_id  Serial number record ID (used as a component)
 * @return resource DB result with finished product details
 */
function trace_serial_forward_to_products($serial_id) {
	$serial_id = (int)$serial_id;

	$sql = "SELECT pt.*,
			wo.wo_ref,
			wo.stock_id AS finished_stock_id,
			fsm.description AS finished_description,
			fsn.serial_no AS finished_serial_no,
			fsn.status AS finished_serial_status,
			fsb.batch_no AS finished_batch_no,
			csm.description AS component_description
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master fsm ON wo.stock_id = fsm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers fsn ON pt.finished_serial_id = fsn.id
		LEFT JOIN " . TB_PREF . "stock_batches fsb ON pt.finished_batch_id = fsb.id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		WHERE pt.component_serial_id = $serial_id
		ORDER BY pt.created_at DESC";
	return db_query($sql, 'could not trace serial forward to products');
}

/**
 * Get customer delivery history for a specific serial number.
 *
 * @param int $serial_id  Serial number record ID
 * @return array  Array of delivery records
 */
function get_serial_delivery_history($serial_id) {
	$serial_id = (int)$serial_id;
	$deliveries = array();

	$sql = "SELECT smov.trans_no AS delivery_no,
			smov.tran_date AS delivery_date,
			smov.stock_id,
			sm.description AS item_description,
			ABS(smov.qty) AS qty,
			dt.debtor_no AS customer_id,
			dm.name AS customer_name
		FROM " . TB_PREF . "stock_moves smov
		INNER JOIN " . TB_PREF . "stock_master sm ON smov.stock_id = sm.stock_id
		INNER JOIN " . TB_PREF . "debtor_trans dt ON dt.trans_no = smov.trans_no AND dt.type = " . ST_CUSTDELIVERY . "
		INNER JOIN " . TB_PREF . "debtors_master dm ON dt.debtor_no = dm.debtor_no
		WHERE smov.type = " . ST_CUSTDELIVERY . "
		AND smov.serial_id = $serial_id
		ORDER BY smov.tran_date DESC";
	$result = db_query($sql, 'could not get serial delivery history');
	while ($row = db_fetch($result)) {
		$deliveries[] = $row;
	}
	return $deliveries;
}

/**
 * Get supplier receipt (GRN) history for a specific serial number.
 *
 * @param int $serial_id  Serial number record ID
 * @return array  Array of receipt records
 */
function get_serial_receipt_history($serial_id) {
	$serial_id = (int)$serial_id;
	$receipts = array();

	$sql = "SELECT smov.trans_no AS grn_no,
			smov.tran_date AS receipt_date,
			smov.stock_id,
			sm.description AS item_description,
			ABS(smov.qty) AS qty,
			s.supplier_id,
			s.supp_name AS supplier_name
		FROM " . TB_PREF . "stock_moves smov
		INNER JOIN " . TB_PREF . "stock_master sm ON smov.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "grn_batch grn ON grn.id = smov.trans_no AND smov.type = " . ST_SUPPRECEIVE . "
		LEFT JOIN " . TB_PREF . "suppliers s ON grn.supplier_id = s.supplier_id
		WHERE smov.type = " . ST_SUPPRECEIVE . "
		AND smov.serial_id = $serial_id
		ORDER BY smov.tran_date DESC";
	$result = db_query($sql, 'could not get serial receipt history');
	while ($row = db_fetch($result)) {
		$receipts[] = $row;
	}
	return $receipts;
}

/**
 * Get serial number summary statistics for the lifecycle page header.
 *
 * @param int $serial_id  Serial number record ID
 * @return array  Summary statistics
 */
function get_serial_lifecycle_summary($serial_id) {
	$serial_id = (int)$serial_id;
	$summary = array(
		'total_movements' => 0,
		'total_inspections' => 0,
		'total_warranty_claims' => 0,
		'total_recalls' => 0,
		'first_event_date' => null,
		'last_event_date' => null,
		'days_in_service' => 0,
	);

	$sql = "SELECT COUNT(*) AS cnt, MIN(tran_date) AS first_date, MAX(tran_date) AS last_date
		FROM " . TB_PREF . "serial_movements WHERE serial_id = $serial_id";
	$row = db_fetch(db_query($sql, 'count serial movements'));
	$summary['total_movements'] = (int)$row['cnt'];
	$summary['first_event_date'] = $row['first_date'];
	$summary['last_event_date'] = $row['last_date'];

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "quality_inspections WHERE serial_id = $serial_id";
	$row = db_fetch(db_query($sql, 'count serial QC'));
	$summary['total_inspections'] = (int)$row['cnt'];

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "warranty_claims WHERE serial_id = $serial_id";
	$row = db_fetch(db_query($sql, 'count serial warranty'));
	$summary['total_warranty_claims'] = (int)$row['cnt'];

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "recall_items WHERE serial_id = $serial_id";
	$row = db_fetch(db_query($sql, 'count serial recalls'));
	$summary['total_recalls'] = (int)$row['cnt'];

	if ($summary['first_event_date'] && $summary['last_event_date']) {
		$d1 = new DateTime($summary['first_event_date']);
		$d2 = new DateTime($summary['last_event_date']);
		$summary['days_in_service'] = $d1->diff($d2)->days;
	}

	return $summary;
}

/**
 * Get batch summary statistics for the lifecycle page header.
 *
 * @param int $batch_id  Batch record ID
 * @return array  Summary statistics
 */
function get_batch_lifecycle_summary($batch_id) {
	$batch_id = (int)$batch_id;
	$summary = array(
		'total_movements' => 0,
		'total_inspections' => 0,
		'total_recalls' => 0,
		'first_event_date' => null,
		'last_event_date' => null,
		'total_inbound' => 0,
		'total_outbound' => 0,
	);

	$sql = "SELECT COUNT(*) AS cnt, MIN(tran_date) AS first_date, MAX(tran_date) AS last_date,
			SUM(CASE WHEN quantity > 0 THEN quantity ELSE 0 END) AS total_in,
			SUM(CASE WHEN quantity < 0 THEN ABS(quantity) ELSE 0 END) AS total_out
		FROM " . TB_PREF . "batch_movements WHERE batch_id = $batch_id";
	$row = db_fetch(db_query($sql, 'count batch movements'));
	$summary['total_movements'] = (int)$row['cnt'];
	$summary['first_event_date'] = $row['first_date'];
	$summary['last_event_date'] = $row['last_date'];
	$summary['total_inbound'] = (float)$row['total_in'];
	$summary['total_outbound'] = (float)$row['total_out'];

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "quality_inspections WHERE batch_id = $batch_id";
	$row = db_fetch(db_query($sql, 'count batch QC'));
	$summary['total_inspections'] = (int)$row['cnt'];

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "recall_items WHERE batch_id = $batch_id";
	$row = db_fetch(db_query($sql, 'count batch recalls'));
	$summary['total_recalls'] = (int)$row['cnt'];

	return $summary;
}

/**
 * Build a human-readable detail string for a serial movement event.
 *
 * @param array $row  Movement row from serial_movements + joins
 * @return string  Description of the event
 */
function build_movement_detail($row) {
	global $systypes_array;

	$parts = array();

	// Transaction type context
	if ($row['trans_type'] > 0 && isset($systypes_array[$row['trans_type']])) {
		$parts[] = $systypes_array[$row['trans_type']];
	}

	// Location info
	if ($row['from_location_name'] && $row['to_location_name']) {
		$parts[] = sprintf(_('%s → %s'), $row['from_location_name'], $row['to_location_name']);
	} elseif ($row['to_location_name']) {
		$parts[] = sprintf(_('Into %s'), $row['to_location_name']);
	} elseif ($row['from_location_name']) {
		$parts[] = sprintf(_('From %s'), $row['from_location_name']);
	}

	// Status change
	if ($row['from_status'] && $row['to_status']) {
		$parts[] = sprintf(_('Status: %s → %s'), ucfirst($row['from_status']), ucfirst($row['to_status']));
	} elseif ($row['to_status']) {
		$parts[] = sprintf(_('Status: %s'), ucfirst($row['to_status']));
	}

	return implode(' — ', $parts);
}

/**
 * Build a human-readable detail string for a batch movement event.
 *
 * @param array $row  Movement row from batch_movements + joins
 * @return string  Description of the event
 */
function build_batch_movement_detail($row) {
	global $systypes_array;

	$parts = array();

	// Transaction type context
	if ($row['trans_type'] > 0 && isset($systypes_array[$row['trans_type']])) {
		$parts[] = $systypes_array[$row['trans_type']];
	}

	// Location info
	if ($row['location_name']) {
		$parts[] = sprintf(_('Location: %s'), $row['location_name']);
	}

	// Quantity
	if ($row['quantity'] >= 0) {
		$parts[] = sprintf(_('Qty: +%s'), number_format2($row['quantity'], get_qty_dec()));
	} else {
		$parts[] = sprintf(_('Qty: %s'), number_format2($row['quantity'], get_qty_dec()));
	}

	return implode(' — ', $parts);
}

/**
 * Get the event type display label.
 *
 * @param string $event_type  Internal event type key
 * @return string  Translated label
 */
function get_lifecycle_event_type_label($event_type) {
	$labels = array(
		'movement'            => _('Stock Movement'),
		'quality_inspection'  => _('Quality Inspection'),
		'warranty_claim'      => _('Warranty Claim'),
		'recall'              => _('Recall'),
	);
	return isset($labels[$event_type]) ? $labels[$event_type] : ucfirst($event_type);
}

/**
 * Get the CSS color for an event type badge.
 *
 * @param string $event_type  Internal event type key
 * @return string  CSS color code
 */
function get_lifecycle_event_type_color($event_type) {
	$colors = array(
		'movement'            => '#337ab7',  // blue
		'quality_inspection'  => '#5bc0de',  // teal
		'warranty_claim'      => '#f0ad4e',  // orange
		'recall'              => '#d9534f',  // red
	);
	return isset($colors[$event_type]) ? $colors[$event_type] : '#777';
}

/**
 * Render a lifecycle event type badge.
 *
 * @param string $event_type  Internal event type key
 * @return string  HTML span with badge styling
 */
function lifecycle_event_badge($event_type) {
	$label = get_lifecycle_event_type_label($event_type);
	$color = get_lifecycle_event_type_color($event_type);
	return "<span style='display:inline-block;padding:2px 8px;border-radius:3px;"
		. "font-size:11px;color:#fff;background-color:$color;'>"
		. htmlspecialchars($label) . "</span>";
}

/**
 * Forward traceability: given a batch, find all production traceability records
 * (which finished goods were produced from components of this batch).
 *
 * @param int $batch_id  Component batch ID
 * @return resource  DB result with finished product details
 */
function trace_batch_forward_to_products($batch_id) {
	$batch_id = (int)$batch_id;

	$sql = "SELECT pt.*,
			wo.wo_ref,
			wo.stock_id AS finished_stock_id,
			fsm.description AS finished_description,
			fsn.serial_no AS finished_serial_no,
			fsn.status AS finished_serial_status,
			fsb.batch_no AS finished_batch_no,
			csm.description AS component_description
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master fsm ON wo.stock_id = fsm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers fsn ON pt.finished_serial_id = fsn.id
		LEFT JOIN " . TB_PREF . "stock_batches fsb ON pt.finished_batch_id = fsb.id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		WHERE pt.component_batch_id = $batch_id
		ORDER BY pt.created_at DESC";
	return db_query($sql, 'could not trace batch forward to products');
}

/**
 * Reverse trace: given a batch (as finished product), find all component
 * batches/serials consumed during its production.
 *
 * @param int $batch_id  Finished product batch ID
 * @return resource  DB result with component details
 */
function trace_batch_reverse_to_components($batch_id) {
	$batch_id = (int)$batch_id;

	$sql = "SELECT pt.*,
			wo.wo_ref,
			wo.stock_id AS finished_stock_id,
			fsm.description AS finished_description,
			csm.description AS component_description,
			csn.serial_no AS component_serial_no,
			cb.batch_no AS component_batch_no,
			cb.expiry_date AS component_batch_expiry,
			cb.status AS component_batch_status
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master fsm ON wo.stock_id = fsm.stock_id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers csn ON pt.component_serial_id = csn.id
		LEFT JOIN " . TB_PREF . "stock_batches cb ON pt.component_batch_id = cb.id
		WHERE pt.finished_batch_id = $batch_id
		ORDER BY pt.component_stock_id, pt.created_at DESC";
	return db_query($sql, 'could not trace batch reverse to components');
}
