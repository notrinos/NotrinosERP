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
 * Warranty Provision GL Integration.
 *
 * Handles automatic warranty provision accrual at delivery time and
 * release when warranty expires or claim is resolved.
 *
 * GL posting pattern:
 *   Accrual at delivery:
 *     DR: Warranty Expense Account  (P&L)
 *     CR: Warranty Provision Account (Balance Sheet - Current Liability)
 *
 *   Release on warranty expiry or claim resolution:
 *     DR: Warranty Provision Account
 *     CR: Warranty Expense Account
 *
 * Tables: 0_warranty_provision_log, 0_gl_trans, 0_sys_prefs
 * Dependencies: gl/includes/db/gl_db_trans.inc
 *
 * @package NotrinosERP
 * @subpackage Inventory/WarrantyGL
 */

/**
 * Check if warranty provision posting is enabled and configured.
 *
 * @return array ['enabled' => bool, 'provision_account' => string, 'expense_account' => string, 'rate' => float]
 */
function get_warranty_provision_config()
{
	$enabled = get_company_pref('warranty_provision_enabled');
	$provision_account = get_company_pref('warranty_provision_account');
	$expense_account = get_company_pref('warranty_expense_account');
	$rate = get_company_pref('warranty_provision_rate');

	return array(
		'enabled'            => ($enabled == 1),
		'provision_account'  => $provision_account ? $provision_account : '',
		'expense_account'    => $expense_account ? $expense_account : '',
		'rate'               => ($rate !== false && $rate !== '') ? (float)$rate : 5.0,
	);
}

/**
 * Calculate warranty provision amount for a delivery line.
 *
 * Uses the configured provision rate applied to the item standard cost.
 * If serial has actual_cost, uses that instead.
 *
 * @param string $stock_id     Item code
 * @param float  $qty          Quantity delivered
 * @param float  $standard_cost Standard cost per unit
 * @param int    $serial_id    Optional serial id (for serial-specific cost)
 * @param int    $batch_id     Optional batch id (for batch-specific cost)
 * @return float Provision amount
 */
function calculate_warranty_provision_amount($stock_id, $qty, $standard_cost, $serial_id = null, $batch_id = null)
{
	$config = get_warranty_provision_config();
	if (!$config['enabled'] || $config['rate'] <= 0)
		return 0;

	$cost_per_unit = $standard_cost;

	// Use serial-specific cost if available
	if ($serial_id) {
		$cust_equip_path = dirname(__FILE__) . '/customer_equipment_db.inc';
		if (file_exists($cust_equip_path)) {
			include_once($cust_equip_path);
			if (function_exists('get_serial_specific_cost')) {
				$serial_cost = get_serial_specific_cost($serial_id);
				if ($serial_cost > 0)
					$cost_per_unit = $serial_cost;
			}
		}
	}
	// Use batch-specific cost if available
	elseif ($batch_id) {
		$cust_equip_path = dirname(__FILE__) . '/customer_equipment_db.inc';
		if (file_exists($cust_equip_path)) {
			include_once($cust_equip_path);
			if (function_exists('get_batch_specific_cost')) {
				$batch_cost = get_batch_specific_cost($batch_id);
				if ($batch_cost > 0)
					$cost_per_unit = $batch_cost;
			}
		}
	}

	// Provision = cost * quantity * rate%
	return round2($cost_per_unit * abs($qty) * ($config['rate'] / 100), user_price_dec());
}

/**
 * Post warranty provision accrual GL entries at delivery time.
 *
 * Called from the delivery process for items with warranty_days > 0.
 *
 * @param int    $trans_type     Transaction type (ST_CUSTDELIVERY)
 * @param int    $trans_no       Transaction number (delivery_no)
 * @param string $date_          Delivery date (display format)
 * @param string $stock_id       Item code
 * @param float  $qty            Quantity delivered
 * @param float  $standard_cost  Standard cost per unit
 * @param int    $customer_id    Customer debtor_no
 * @param int    $serial_id      Optional serial id
 * @param int    $batch_id       Optional batch id
 * @param string $warranty_end   Optional warranty end date (display format)
 * @param int    $dimension      Dimension 1
 * @param int    $dimension2     Dimension 2
 * @return bool True if provision was posted
 */
function post_warranty_provision_accrual($trans_type, $trans_no, $date_, $stock_id,
	$qty, $standard_cost, $customer_id, $serial_id = null, $batch_id = null,
	$warranty_end = null, $dimension = 0, $dimension2 = 0)
{
	$config = get_warranty_provision_config();

	if (!$config['enabled'] || $config['provision_account'] == '' || $config['expense_account'] == '')
		return false;

	// Check if item has warranty
	$sql = "SELECT warranty_days FROM " . TB_PREF . "stock_master WHERE stock_id=" . db_escape($stock_id);
	$result = db_query($sql, 'could not check warranty days');
	$item = db_fetch($result);
	if (!$item || !$item['warranty_days'] || $item['warranty_days'] <= 0)
		return false;

	$provision_amount = calculate_warranty_provision_amount($stock_id, $qty, $standard_cost, $serial_id, $batch_id);
	if ($provision_amount <= 0)
		return false;

	// Post GL entries
	// DR: Warranty Expense
	add_gl_trans_std_cost($trans_type, $trans_no, $date_,
		$config['expense_account'], $dimension, $dimension2,
		_('Warranty provision accrual'),
		$provision_amount,
		PT_CUSTOMER, $customer_id);

	// CR: Warranty Provision (liability)
	add_gl_trans_std_cost($trans_type, $trans_no, $date_,
		$config['provision_account'], $dimension, $dimension2,
		_('Warranty provision accrual'),
		-$provision_amount,
		PT_CUSTOMER, $customer_id);

	// Log the provision
	add_warranty_provision_log($trans_type, $trans_no, $stock_id, $serial_id, $batch_id,
		$customer_id, 'accrual', $provision_amount,
		$warranty_end ? date2sql($warranty_end) : null, $date_);

	return true;
}

/**
 * Release warranty provision when warranty expires or claim is resolved.
 *
 * @param int    $trans_type   Transaction type (e.g., ST_JOURNAL)
 * @param int    $trans_no     Transaction number
 * @param string $date_        Date (display format)
 * @param string $stock_id     Item code
 * @param int    $customer_id  Customer debtor_no
 * @param float  $amount       Amount to release (positive value)
 * @param int    $serial_id    Optional serial id
 * @param int    $batch_id     Optional batch id
 * @param string $notes        Release reason
 * @param int    $dimension    Dimension 1
 * @param int    $dimension2   Dimension 2
 * @return bool True if release was posted
 */
function release_warranty_provision($trans_type, $trans_no, $date_, $stock_id,
	$customer_id, $amount, $serial_id = null, $batch_id = null,
	$notes = '', $dimension = 0, $dimension2 = 0)
{
	$config = get_warranty_provision_config();

	if (!$config['enabled'] || $config['provision_account'] == '' || $config['expense_account'] == '')
		return false;

	if ($amount <= 0)
		return false;

	// DR: Warranty Provision (reduce liability)
	add_gl_trans_std_cost($trans_type, $trans_no, $date_,
		$config['provision_account'], $dimension, $dimension2,
		_('Warranty provision release') . ($notes ? ': ' . $notes : ''),
		$amount,
		PT_CUSTOMER, $customer_id);

	// CR: Warranty Expense (reduce expense)
	add_gl_trans_std_cost($trans_type, $trans_no, $date_,
		$config['expense_account'], $dimension, $dimension2,
		_('Warranty provision release') . ($notes ? ': ' . $notes : ''),
		-$amount,
		PT_CUSTOMER, $customer_id);

	add_warranty_provision_log($trans_type, $trans_no, $stock_id, $serial_id, $batch_id,
		$customer_id, 'release', $amount, null, $date_, $notes);

	return true;
}

/**
 * Add a record to the warranty provision log.
 *
 * @param int    $trans_type     Transaction type
 * @param int    $trans_no       Transaction number
 * @param string $stock_id       Item code
 * @param int    $serial_id      Serial id (nullable)
 * @param int    $batch_id       Batch id (nullable)
 * @param int    $customer_id    Customer debtor_no
 * @param string $provision_type 'accrual' or 'release'
 * @param float  $amount         Amount
 * @param string $warranty_end   SQL date (nullable)
 * @param string $tran_date      Display date
 * @param string $notes          Optional notes
 * @return int Inserted log id
 */
function add_warranty_provision_log($trans_type, $trans_no, $stock_id, $serial_id,
	$batch_id, $customer_id, $provision_type, $amount, $warranty_end, $tran_date, $notes = '')
{
	$sql = "INSERT INTO " . TB_PREF . "warranty_provision_log (
		trans_type, trans_no, stock_id, serial_id, batch_id, customer_id,
		provision_type, amount, warranty_end, tran_date, notes, created_by
	) VALUES ("
		. (int)$trans_type . ","
		. (int)$trans_no . ","
		. db_escape($stock_id) . ","
		. ($serial_id ? (int)$serial_id : 'NULL') . ","
		. ($batch_id ? (int)$batch_id : 'NULL') . ","
		. (int)$customer_id . ","
		. db_escape($provision_type) . ","
		. db_escape($amount) . ","
		. ($warranty_end ? "'" . db_escape($warranty_end) . "'" : 'NULL') . ","
		. "'" . date2sql($tran_date) . "',"
		. db_escape($notes) . ","
		. (int)$_SESSION['wa_current_user']->user
		. ")";
	db_query($sql, 'could not add warranty provision log');
	return db_insert_id();
}

/**
 * Get warranty provision log entries.
 *
 * @param int    $customer_id    Customer filter (0 for all)
 * @param string $stock_id       Item filter ('' for all)
 * @param string $provision_type 'accrual'|'release'|'' for all
 * @param string $from_date      Start date (display format, '' for all)
 * @param string $to_date        End date (display format, '' for all)
 * @return resource DB result
 */
function get_warranty_provision_log($customer_id = 0, $stock_id = '', $provision_type = '',
	$from_date = '', $to_date = '')
{
	$sql = "SELECT wpl.*, sm.description AS item_description,
			dm.name AS customer_name,
			sn.serial_no, sb.batch_no
		FROM " . TB_PREF . "warranty_provision_log wpl
		LEFT JOIN " . TB_PREF . "stock_master sm ON wpl.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "debtors_master dm ON wpl.customer_id = dm.debtor_no
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON wpl.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON wpl.batch_id = sb.id
		WHERE 1=1";

	if ($customer_id > 0)
		$sql .= " AND wpl.customer_id = " . (int)$customer_id;
	if ($stock_id !== '')
		$sql .= " AND wpl.stock_id = " . db_escape($stock_id);
	if ($provision_type !== '')
		$sql .= " AND wpl.provision_type = " . db_escape($provision_type);
	if ($from_date !== '')
		$sql .= " AND wpl.tran_date >= '" . date2sql($from_date) . "'";
	if ($to_date !== '')
		$sql .= " AND wpl.tran_date <= '" . date2sql($to_date) . "'";

	$sql .= " ORDER BY wpl.created_at DESC";

	return db_query($sql, 'could not get warranty provision log');
}

/**
 * Get total outstanding warranty provision balance for a customer or all customers.
 *
 * @param int $customer_id Customer id (0 for all)
 * @return float Outstanding provision balance
 */
function get_warranty_provision_balance($customer_id = 0)
{
	$sql = "SELECT
		COALESCE(SUM(CASE WHEN provision_type='accrual' THEN amount ELSE 0 END), 0)
		-
		COALESCE(SUM(CASE WHEN provision_type='release' THEN amount ELSE 0 END), 0)
		AS balance
		FROM " . TB_PREF . "warranty_provision_log";

	if ($customer_id > 0)
		$sql .= " WHERE customer_id = " . (int)$customer_id;

	$result = db_query($sql, 'could not get warranty provision balance');
	$row = db_fetch($result);
	return $row ? (float)$row['balance'] : 0;
}

/**
 * Get warranty provision summary by item.
 *
 * @return resource DB result with stock_id, item_description, accrued, released, balance
 */
function get_warranty_provision_summary_by_item()
{
	$sql = "SELECT wpl.stock_id, sm.description AS item_description,
		SUM(CASE WHEN wpl.provision_type='accrual' THEN wpl.amount ELSE 0 END) AS accrued,
		SUM(CASE WHEN wpl.provision_type='release' THEN wpl.amount ELSE 0 END) AS released,
		SUM(CASE WHEN wpl.provision_type='accrual' THEN wpl.amount ELSE 0 END)
		- SUM(CASE WHEN wpl.provision_type='release' THEN wpl.amount ELSE 0 END) AS balance
		FROM " . TB_PREF . "warranty_provision_log wpl
		LEFT JOIN " . TB_PREF . "stock_master sm ON wpl.stock_id = sm.stock_id
		GROUP BY wpl.stock_id
		HAVING balance <> 0
		ORDER BY balance DESC";

	return db_query($sql, 'could not get warranty provision summary');
}

/**
 * Process warranty provision for a delivery.
 *
 * Called from the sales delivery process hook to create warranty accruals
 * for all warranted items in the delivery.
 *
 * @param object $delivery     Delivery object (cart)
 * @param int    $delivery_no  Delivery transaction number
 * @param string $date_        Delivery date (display format)
 * @return int Number of provisions posted
 */
function process_delivery_warranty_provisions($delivery, $delivery_no, $date_)
{
	$config = get_warranty_provision_config();
	if (!$config['enabled'])
		return 0;

	$count = 0;
	$customer_id = $delivery->customer_id;

	foreach ($delivery->line_items as $line_no => $delivery_line) {
		if ($delivery_line->qty_dispatched <= 0)
			continue;

		// Check if item has warranty
		$sql = "SELECT warranty_days, track_by FROM " . TB_PREF . "stock_master
			WHERE stock_id=" . db_escape($delivery_line->stock_id);
		$result = db_query($sql, 'could not check item warranty');
		$item = db_fetch($result);

		if (!$item || !$item['warranty_days'] || $item['warranty_days'] <= 0)
			continue;

		// Calculate warranty end date
		$warranty_end = add_days($date_, $item['warranty_days']);

		// Get dimensions
		$customer = get_customer($delivery->customer_id);
		$stock_gl_code = get_stock_gl_code($delivery_line->stock_id);
		$dim = ($delivery->dimension_id != $customer['dimension_id'] ? $delivery->dimension_id :
			($customer['dimension_id'] != 0 ? $customer['dimension_id'] : $stock_gl_code['dimension_id']));
		$dim2 = ($delivery->dimension2_id != $customer['dimension2_id'] ? $delivery->dimension2_id :
			($customer['dimension2_id'] != 0 ? $customer['dimension2_id'] : $stock_gl_code['dimension2_id']));

		if (post_warranty_provision_accrual(
			ST_CUSTDELIVERY, $delivery_no, $date_,
			$delivery_line->stock_id, $delivery_line->qty_dispatched,
			$delivery_line->standard_cost, $customer_id,
			null, null, $warranty_end, $dim, $dim2
		)) {
			$count++;
		}
	}

	return $count;
}
