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
 * Production traceability CRUD and trace queries.
 *
 * Table: 0_production_traceability
 * Links consumed components (serial/batch) to produced finished goods (serial/batch)
 * via work orders for forward and reverse traceability.
 */

/**
 * Add a production traceability record linking a consumed component to a finished product.
 *
 * @param int    $work_order_id       Work order ID
 * @param int    $finished_serial_id  Finished good serial ID (null if batch-only)
 * @param int    $finished_batch_id   Finished good batch ID (null if serial-only)
 * @param string $component_stock_id  Component item code
 * @param int    $component_serial_id Component serial ID consumed (null if batch-only)
 * @param int    $component_batch_id  Component batch ID consumed (null if batch-only)
 * @param float  $component_qty       Quantity of component consumed
 * @return int   Inserted record ID
 */
function add_production_traceability($work_order_id, $finished_serial_id, $finished_batch_id,
	$component_stock_id, $component_serial_id, $component_batch_id, $component_qty)
{
	$sql = "INSERT INTO " . TB_PREF . "production_traceability (
		work_order_id, finished_serial_id, finished_batch_id,
		component_stock_id, component_serial_id, component_batch_id,
		component_qty, created_at
	) VALUES (
		" . (int)$work_order_id . ",
		" . ($finished_serial_id !== null ? (int)$finished_serial_id : 'NULL') . ",
		" . ($finished_batch_id !== null ? (int)$finished_batch_id : 'NULL') . ",
		" . db_escape($component_stock_id) . ",
		" . ($component_serial_id !== null ? (int)$component_serial_id : 'NULL') . ",
		" . ($component_batch_id !== null ? (int)$component_batch_id : 'NULL') . ",
		" . (float)$component_qty . ",
		NOW()
	)";
	db_query($sql, 'could not add production traceability record');
	return db_insert_id();
}

/**
 * Forward trace: find all finished products that used a specific component serial.
 *
 * @param int $component_serial_id Component serial ID
 * @return resource DB result with finished product details
 */
function trace_forward_by_serial($component_serial_id) {
	$sql = "SELECT pt.*,
			wo.stock_id AS finished_stock_id,
			sm.description AS finished_description,
			sn.serial_no AS finished_serial_no,
			sb.batch_no AS finished_batch_no,
			csn.serial_no AS component_serial_no,
			csm.description AS component_description
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master sm ON wo.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON pt.finished_serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON pt.finished_batch_id = sb.id
		LEFT JOIN " . TB_PREF . "serial_numbers csn ON pt.component_serial_id = csn.id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		WHERE pt.component_serial_id = " . (int)$component_serial_id
		. " ORDER BY pt.created_at DESC";
	return db_query($sql, 'could not trace forward by serial');
}

/**
 * Forward trace: find all finished products that used a specific component batch.
 *
 * @param int $component_batch_id Component batch ID
 * @return resource DB result with finished product details
 */
function trace_forward_by_batch($component_batch_id) {
	$sql = "SELECT pt.*,
			wo.stock_id AS finished_stock_id,
			sm.description AS finished_description,
			sn.serial_no AS finished_serial_no,
			sb.batch_no AS finished_batch_no,
			cb.batch_no AS component_batch_no,
			csm.description AS component_description
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master sm ON wo.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON pt.finished_serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON pt.finished_batch_id = sb.id
		LEFT JOIN " . TB_PREF . "stock_batches cb ON pt.component_batch_id = cb.id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		WHERE pt.component_batch_id = " . (int)$component_batch_id
		. " ORDER BY pt.created_at DESC";
	return db_query($sql, 'could not trace forward by batch');
}

/**
 * Reverse trace: find all components used to produce a specific finished serial.
 *
 * @param int $finished_serial_id Finished good serial ID
 * @return resource DB result with component details
 */
function trace_reverse_by_serial($finished_serial_id) {
	$sql = "SELECT pt.*,
			wo.stock_id AS finished_stock_id,
			sm.description AS finished_description,
			sn.serial_no AS finished_serial_no,
			csn.serial_no AS component_serial_no,
			cb.batch_no AS component_batch_no,
			csm.description AS component_description
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master sm ON wo.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON pt.finished_serial_id = sn.id
		LEFT JOIN " . TB_PREF . "serial_numbers csn ON pt.component_serial_id = csn.id
		LEFT JOIN " . TB_PREF . "stock_batches cb ON pt.component_batch_id = cb.id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		WHERE pt.finished_serial_id = " . (int)$finished_serial_id
		. " ORDER BY pt.component_stock_id, pt.created_at DESC";
	return db_query($sql, 'could not trace reverse by serial');
}

/**
 * Reverse trace: find all components used to produce a specific finished batch.
 *
 * @param int $finished_batch_id Finished good batch ID
 * @return resource DB result with component details
 */
function trace_reverse_by_batch($finished_batch_id) {
	$sql = "SELECT pt.*,
			wo.stock_id AS finished_stock_id,
			sm.description AS finished_description,
			sb.batch_no AS finished_batch_no,
			csn.serial_no AS component_serial_no,
			cb.batch_no AS component_batch_no,
			csm.description AS component_description
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master sm ON wo.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON pt.finished_batch_id = sb.id
		LEFT JOIN " . TB_PREF . "serial_numbers csn ON pt.component_serial_id = csn.id
		LEFT JOIN " . TB_PREF . "stock_batches cb ON pt.component_batch_id = cb.id
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		WHERE pt.finished_batch_id = " . (int)$finished_batch_id
		. " ORDER BY pt.component_stock_id, pt.created_at DESC";
	return db_query($sql, 'could not trace reverse by batch');
}

/**
 * Get all traceability records for a work order.
 *
 * @param int $work_order_id Work order ID
 * @return resource DB result
 */
function get_wo_traceability($work_order_id) {
	$sql = "SELECT pt.*,
			csm.description AS component_description,
			csn.serial_no AS component_serial_no,
			cb.batch_no AS component_batch_no,
			sn.serial_no AS finished_serial_no,
			sb.batch_no AS finished_batch_no
		FROM " . TB_PREF . "production_traceability pt
		LEFT JOIN " . TB_PREF . "stock_master csm ON pt.component_stock_id = csm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers csn ON pt.component_serial_id = csn.id
		LEFT JOIN " . TB_PREF . "stock_batches cb ON pt.component_batch_id = cb.id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON pt.finished_serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON pt.finished_batch_id = sb.id
		WHERE pt.work_order_id = " . (int)$work_order_id
		. " ORDER BY pt.component_stock_id, pt.created_at";
	return db_query($sql, 'could not get WO traceability');
}

/**
 * Get traceability records for a specific WO issue (component issue transaction).
 *
 * @param int $work_order_id Work order ID
 * @param string $component_stock_id Component item code
 * @return resource DB result
 */
function get_wo_component_traceability($work_order_id, $component_stock_id) {
	$sql = "SELECT pt.*,
			csn.serial_no AS component_serial_no,
			cb.batch_no AS component_batch_no
		FROM " . TB_PREF . "production_traceability pt
		LEFT JOIN " . TB_PREF . "serial_numbers csn ON pt.component_serial_id = csn.id
		LEFT JOIN " . TB_PREF . "stock_batches cb ON pt.component_batch_id = cb.id
		WHERE pt.work_order_id = " . (int)$work_order_id
		. " AND pt.component_stock_id = " . db_escape($component_stock_id)
		. " ORDER BY pt.created_at";
	return db_query($sql, 'could not get WO component traceability');
}

/**
 * Delete traceability records for a work order (used when voiding).
 *
 * @param int $work_order_id Work order ID
 */
function delete_wo_traceability($work_order_id) {
	$sql = "DELETE FROM " . TB_PREF . "production_traceability WHERE work_order_id=" . (int)$work_order_id;
	db_query($sql, 'could not delete WO traceability');
}

/**
 * Get customer delivery info for serials produced from a specific component batch.
 * "Which customers got products made from batch X?"
 *
 * @param int $component_batch_id Component batch ID
 * @return resource DB result with delivery/customer details
 */
function trace_batch_to_customers($component_batch_id) {
	$sql = "SELECT DISTINCT
			pt.work_order_id,
			wo.stock_id AS finished_stock_id,
			sm.description AS finished_description,
			sn.serial_no AS finished_serial_no,
			sb.batch_no AS finished_batch_no,
			sn.customer_id,
			dm.name AS customer_name,
			smov.trans_no AS delivery_no,
			smov.tran_date AS delivery_date
		FROM " . TB_PREF . "production_traceability pt
		INNER JOIN " . TB_PREF . "workorders wo ON pt.work_order_id = wo.id
		INNER JOIN " . TB_PREF . "stock_master sm ON wo.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON pt.finished_serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON pt.finished_batch_id = sb.id
		LEFT JOIN " . TB_PREF . "debtors_master dm ON sn.customer_id = dm.debtor_no
		LEFT JOIN " . TB_PREF . "stock_moves smov ON smov.stock_id = wo.stock_id
			AND smov.type = " . ST_CUSTDELIVERY . "
			AND smov.serial_id = sn.id
		WHERE pt.component_batch_id = " . (int)$component_batch_id
		. " ORDER BY pt.work_order_id, sn.serial_no";
	return db_query($sql, 'could not trace batch to customers');
}
