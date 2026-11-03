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

require_once __DIR__.'/../../../dimensions/includes/dimension_reference_writer_fence.inc';

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

	return post_warranty_provision_effect($trans_type, $trans_no, $date_, $stock_id,
        $customer_id, 'accrual', $provision_amount, $serial_id, $batch_id,
        $warranty_end ? date2sql($warranty_end) : null, '', $dimension, $dimension2);
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
    return post_warranty_provision_effect($trans_type, $trans_no, $date_, $stock_id,
        $customer_id, 'release', $amount, $serial_id, $batch_id, null, $notes, $dimension, $dimension2);
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
 * @param int $dimension As-posted first axis
 * @param int $dimension2 As-posted second axis
 * @param int|null $gl_trans_id Actual debit counter; null only for a retained legacy inverse
 * @param int|null $source_accrual Original log id for history-only delivery reversal
 * @return int|bool Inserted log id, true for an existing inverse, or false
 */
function add_warranty_provision_log($trans_type, $trans_no, $stock_id, $serial_id,
    $batch_id, $customer_id, $provision_type, $amount, $warranty_end, $tran_date, $notes = '',
    $dimension = 0, $dimension2 = 0, $gl_trans_id = null, $source_accrual = null)
{
    $coordinates = array('dimension_id'=>$dimension, 'dimension2_id'=>$dimension2);
    if (!is_string($stock_id) || !is_string($notes)) return false;
    $encoding = $_SESSION['language']->encoding === 'iso-8859-2' ? 'ISO-8859-1' : $_SESSION['language']->encoding;
    $stock_id = html_specials_encode(html_entity_decode($stock_id, ENT_QUOTES, $encoding));
    $notes = html_specials_encode(html_entity_decode($notes, ENT_QUOTES, $encoding));
    return dimension_ordinary_master_write('warranty_provision_log', $source_accrual, $coordinates,
        function($admitted) use ($trans_type, $trans_no, $stock_id, $serial_id, $batch_id,
            $customer_id, $provision_type, $amount, $warranty_end, $tran_date, $notes, $gl_trans_id, $source_accrual) {
            if (!in_array($provision_type, array('accrual','release'), true) || !is_numeric($amount)
                || !is_finite((float)$amount) || $amount <= 0) return false;
            $gl = null;
            if ($gl_trans_id !== null && $gl_trans_id !== '') {
                if (!dimension_writer_reference_id($gl_trans_id)) return false;
                $rows = db_query('SELECT * FROM '.TB_PREF.'gl_trans WHERE counter='.(int)$gl_trans_id.' FOR UPDATE');
                $gl = $rows ? db_fetch($rows) : false;
                if (!$gl) return false;
            } elseif ($source_accrual === null) return false;
            if ($gl && ((int)$gl['type'] !== (int)$trans_type || (int)$gl['type_no'] !== (int)$trans_no
                || $gl['tran_date'] !== date2sql($tran_date)
                || (int)$gl['dimension_id'] !== $admitted['dimension_id']
                || (int)$gl['dimension2_id'] !== $admitted['dimension2_id'])) return false;
            if ($source_accrual !== null) {
                // This is a history-only release of an already-zeroed delivery
                // posting. Reuse the retained coordinates, even closed/orphaned.
                $rows = db_query('SELECT * FROM '.TB_PREF.'warranty_provision_log WHERE id='.(int)$source_accrual.' FOR UPDATE');
                $source = $rows ? db_fetch($rows) : false;
                if (!$source || $source['provision_type'] !== 'accrual' || $provision_type !== 'release'
                    || (int)$trans_type !== ST_CUSTDELIVERY || ($gl && (float)$gl['amount'] != 0)
                    || (int)$source['trans_type'] !== (int)$trans_type || (int)$source['trans_no'] !== (int)$trans_no
                    || (int)$source['gl_trans_id'] !== (int)$gl_trans_id || (float)$source['amount'] !== (float)$amount
                    || $source['stock_id'] !== $stock_id || (int)$source['customer_id'] !== (int)$customer_id
                    || (int)$source['dimension_id'] !== $admitted['dimension_id']
                    || (int)$source['dimension2_id'] !== $admitted['dimension2_id']
                    || $source['serial_id'] != $serial_id || $source['batch_id'] != $batch_id
                    || $source['warranty_end'] !== $warranty_end || $source['tran_date'] !== date2sql($tran_date)
                    || $notes !== 'Delivery GL void: accrual '.(int)$source_accrual) return false;
                $rows = db_query('SELECT trans_no FROM '.TB_PREF.'debtor_trans WHERE type='.(int)$trans_type.' AND trans_no='.(int)$trans_no.' FOR UPDATE');
                if (!$rows || !db_fetch($rows)) return false;
                $rows = db_query('SELECT counter FROM '.TB_PREF.'gl_trans WHERE type='.(int)$trans_type.' AND type_no='.(int)$trans_no.' AND amount<>0 FOR UPDATE');
                if (!$rows || db_fetch($rows)) return false;
                // An older unlinked journal release cannot establish which
                // delivery effect it reversed. Preserve it for reconciliation.
                if (!$gl) {
                    $rows = db_query('SELECT id FROM '.TB_PREF.'warranty_provision_log WHERE trans_type='.ST_JOURNAL.' AND trans_no='.(int)$trans_no
                        ." AND provision_type='release' AND stock_id=".db_escape($stock_id).' AND customer_id='.(int)$customer_id.' FOR UPDATE');
                    if (!$rows || db_fetch($rows)) return false;
                }
                $rows = db_query('SELECT id FROM '.TB_PREF.'warranty_provision_log WHERE trans_type='.(int)$trans_type
                    .' AND trans_no='.(int)$trans_no.' AND provision_type=\'release\' AND notes='.db_escape($notes).' FOR UPDATE');
                if (!$rows) return false;
                if (db_fetch($rows)) return true; // Same original effect is released once.
            } elseif ((float)$gl['amount'] !== (float)$amount) return false;
            $rows = db_query('SELECT id FROM '.TB_PREF.'warranty_provision_log WHERE gl_trans_id='.(int)$gl_trans_id.' AND provision_type='.db_escape($provision_type).' FOR UPDATE');
            if (!$rows || db_fetch($rows)) return false;
            $sql = 'INSERT INTO '.TB_PREF.'warranty_provision_log (trans_type,trans_no,stock_id,serial_id,batch_id,customer_id,'
                .'provision_type,amount,warranty_end,tran_date,notes,created_by,dimension_id,dimension2_id,gl_trans_id) VALUES ('
                .(int)$trans_type.','.(int)$trans_no.','.db_escape($stock_id).','
                .($serial_id ? (int)$serial_id : 'NULL').','.($batch_id ? (int)$batch_id : 'NULL').','.(int)$customer_id.','
                .db_escape($provision_type).','.db_escape($amount).','.($warranty_end ? db_escape($warranty_end) : 'NULL').','
                .db_escape(date2sql($tran_date)).','.db_escape($notes).','.(int)$_SESSION['wa_current_user']->user.','
                .$admitted['dimension_id'].','.$admitted['dimension2_id'].','.($gl_trans_id ? (int)$gl_trans_id : 'NULL').')';
            if (!db_query($sql)) return false;
            $id = db_insert_id();
            $rows = db_query('SELECT * FROM '.TB_PREF.'warranty_provision_log WHERE id='.(int)$id.' FOR UPDATE');
            $after = $rows ? db_fetch($rows) : false;
            if (!$after || (int)$after['dimension_id'] !== $admitted['dimension_id']
                || (int)$after['dimension2_id'] !== $admitted['dimension2_id'] || (int)$after['gl_trans_id'] !== (int)$gl_trans_id
                || (int)$after['trans_type'] !== (int)$trans_type || (int)$after['trans_no'] !== (int)$trans_no
                || $after['stock_id'] !== $stock_id || $after['serial_id'] != $serial_id || $after['batch_id'] != $batch_id
                || (int)$after['customer_id'] !== (int)$customer_id || $after['provision_type'] !== $provision_type
                || (float)$after['amount'] !== (float)$amount || $after['warranty_end'] !== $warranty_end
                || $after['tran_date'] !== date2sql($tran_date) || $after['notes'] !== $notes
                || (int)$after['created_by'] !== (int)$_SESSION['wa_current_user']->user) return false;
            return db_preserve_insert_id($id) ? $id : false;
        });
}

/** Post the paired GL and its immutable reference log under the same master locks. */
function post_warranty_provision_effect($type, $number, $date, $stock, $customer, $kind,
    $amount, $serial, $batch, $end, $notes, $dimension, $dimension2)
{
    $config = get_warranty_provision_config();
    if (!$config['enabled'] || !$config['expense_account'] || !$config['provision_account']
        || $config['expense_account'] === $config['provision_account']
        || !is_numeric($amount) || !is_finite((float)$amount) || $amount <= 0
        || !is_date($date) || !is_date_in_fiscalyear($date)) return false;
    $amount = round2($amount, user_price_dec());
    if ($amount <= 0) return false;
    return dimension_ordinary_master_write('warranty_provision_log', null,
        array('dimension_id'=>$dimension,'dimension2_id'=>$dimension2),
        function($admitted) use ($config,$type,$number,$date,$stock,$customer,$kind,$amount,$serial,$batch,$end,$notes) {
            $debit = $kind === 'accrual' ? $config['expense_account'] : $config['provision_account'];
            $credit = $kind === 'accrual' ? $config['provision_account'] : $config['expense_account'];
            foreach (array($debit=>$amount, $credit=>-$amount) as $account=>$value) {
                $rows = db_query('SELECT account_code FROM '.TB_PREF.'chart_master WHERE account_code='.db_escape($account).' AND inactive=0 FOR UPDATE');
                if (!$rows || !db_fetch($rows)) return false;
                if (add_gl_trans_std_cost($type,$number,$date,$account,$admitted['dimension_id'],$admitted['dimension2_id'],
                    $kind === 'accrual' ? _('Warranty provision accrual') : _('Warranty provision release'),$value,PT_CUSTOMER,$customer,null) === false) return false;
                $counter = db_insert_id();
                $rows = db_query('SELECT * FROM '.TB_PREF.'gl_trans WHERE counter='.(int)$counter.' FOR UPDATE');
                $gl = $rows ? db_fetch($rows) : false;
                if (!$gl || (int)$gl['type'] !== (int)$type || (int)$gl['type_no'] !== (int)$number
                    || $gl['account'] !== (string)$account || (float)$gl['amount'] !== (float)$value
                    || (int)$gl['dimension_id'] !== $admitted['dimension_id'] || (int)$gl['dimension2_id'] !== $admitted['dimension2_id']) return false;
                if ($value > 0) $debit_id = $counter;
            }
            return add_warranty_provision_log($type,$number,$stock,$serial,$batch,$customer,$kind,$amount,$end,$date,$notes,
                $admitted['dimension_id'],$admitted['dimension2_id'],$debit_id) !== false;
        });
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
    if (!begin_transaction()) return false;
    $rows = db_query('SELECT * FROM '.TB_PREF.'debtor_trans WHERE type='.ST_CUSTDELIVERY.' AND trans_no='.(int)$delivery_no.' FOR UPDATE');
     $header = $rows ? db_fetch($rows) : false;
    if (!$header) { cancel_transaction(); return false; }
    $date_ = sql2date($header['tran_date']);
    $rows = db_query('SELECT * FROM '.TB_PREF.'debtors_master WHERE debtor_no='.(int)$header['debtor_no'].' FOR UPDATE');
    $customer = $rows ? db_fetch($rows) : false;
    if (!$customer) { cancel_transaction(); return false; }
    $rows = db_query('SELECT * FROM '.TB_PREF.'debtor_trans_details WHERE debtor_trans_type='.ST_CUSTDELIVERY.' AND debtor_trans_no='.(int)$delivery_no.' ORDER BY id FOR UPDATE');
    if (!$rows) { cancel_transaction(); return false; }
    $details = array(); while ($detail = db_fetch($rows)) $details[] = $detail;
    $rows = db_query('SELECT w.id FROM '.TB_PREF.'warranty_provision_log w JOIN '.TB_PREF.'gl_trans g ON g.counter=w.gl_trans_id WHERE w.trans_type='.ST_CUSTDELIVERY.' AND w.trans_no='.(int)$delivery_no." AND w.provision_type='accrual' AND g.amount<>0 FOR UPDATE");
    if (!$rows) { cancel_transaction(); return false; }
    if (db_fetch($rows)) { commit_transaction(); return 0; }

	$count = 0;
	$customer_id = $header['debtor_no'];

	foreach ($details as $detail) {
        $delivery_line = (object)array('stock_id'=>$detail['stock_id'],'qty_dispatched'=>$detail['quantity'],'standard_cost'=>$detail['standard_cost']);
		if ($delivery_line->qty_dispatched <= 0)
			continue;

		// Check if item has warranty
		$sql = "SELECT warranty_days, track_by FROM " . TB_PREF . "stock_master
			WHERE stock_id=" . db_escape($delivery_line->stock_id).' FOR UPDATE';
		$result = db_query($sql, 'could not check item warranty');
		$item = db_fetch($result);

		if (!$item || !$item['warranty_days'] || $item['warranty_days'] <= 0)
			continue;

		// Calculate warranty end date
		$warranty_end = add_days($date_, $item['warranty_days']);

		// Get dimensions
		$stock_gl_code = get_stock_gl_code($delivery_line->stock_id);
		$dim = ($header['dimension_id'] != $customer['dimension_id'] ? $header['dimension_id'] :
			($customer['dimension_id'] != 0 ? $customer['dimension_id'] : $stock_gl_code['dimension_id']));
		$dim2 = ($header['dimension2_id'] != $customer['dimension2_id'] ? $header['dimension2_id'] :
			($customer['dimension2_id'] != 0 ? $customer['dimension2_id'] : $stock_gl_code['dimension2_id']));

        if (calculate_warranty_provision_amount($delivery_line->stock_id, $delivery_line->qty_dispatched, $delivery_line->standard_cost) <= 0) continue;
		if (post_warranty_provision_accrual(
			ST_CUSTDELIVERY, $delivery_no, $date_,
			$delivery_line->stock_id, $delivery_line->qty_dispatched,
			$delivery_line->standard_cost, $customer_id,
			null, null, $warranty_end, $dim, $dim2
		)) {
			$count++;
		} else { cancel_transaction(); return false; }
	}

	if (!commit_transaction()) { cancel_transaction(); return false; }
	return $count;
}

/**
 * Reverse warranty provision accruals when a delivery is voided.
 *
 * Append release history after the native owner has zeroed delivery GL.
 * Retain original accruals and their classification; never post another journal.
 *
 * @param int $trans_type Transaction type (ST_CUSTDELIVERY)
 * @param int $trans_no   Transaction number (delivery_no)
 * @return int Number of provisions reversed
 */
function reverse_delivery_warranty_provisions($trans_type, $trans_no)
{
    if ((int)$trans_type !== ST_CUSTDELIVERY || !begin_transaction()) return false;
    $rows = db_query('SELECT trans_no FROM '.TB_PREF.'debtor_trans WHERE type='.(int)$trans_type.' AND trans_no='.(int)$trans_no.' FOR UPDATE');
    if (!$rows || !db_fetch($rows)) { cancel_transaction(); return false; }
    // The native delivery owns these GL effects. Its edit/void zeroes them
    // before this call. Never manufacture a second ST_JOURNAL reversal.
    $rows = db_query('SELECT counter FROM '.TB_PREF.'gl_trans WHERE type='.(int)$trans_type.' AND type_no='.(int)$trans_no.' AND amount<>0 FOR UPDATE');
    if (!$rows || db_fetch($rows)) { cancel_transaction(); return false; }
    $rows = db_query('SELECT * FROM '.TB_PREF.'warranty_provision_log WHERE trans_type='.(int)$trans_type.' AND trans_no='.(int)$trans_no." AND provision_type='accrual' ORDER BY id FOR UPDATE");
    if (!$rows) { cancel_transaction(); return false; }
    $entries = array();
    while ($entry = db_fetch($rows)) $entries[] = $entry;
    $count = 0;
    foreach ($entries as $entry) {
        $notes = 'Delivery GL void: accrual '.(int)$entry['id'];
        $rows = db_query('SELECT id FROM '.TB_PREF.'warranty_provision_log WHERE trans_type='.(int)$trans_type.' AND trans_no='.(int)$trans_no." AND provision_type='release' AND notes=".db_escape($notes).' FOR UPDATE');
        if (!$rows) { cancel_transaction(); return false; }
        if (db_fetch($rows)) continue;
        // A legacy log retains its original unknown coordinates/link. The
        // current delivery owner proves that all its GL has been zeroed.
        if (!add_warranty_provision_log($trans_type,$trans_no,$entry['stock_id'],
            $entry['serial_id'],$entry['batch_id'],$entry['customer_id'],'release',$entry['amount'],$entry['warranty_end'],
            sql2date($entry['tran_date']),$notes,$entry['dimension_id'],$entry['dimension2_id'],$entry['gl_trans_id'],$entry['id'])) {
            cancel_transaction(); return false;
        }
        $count++;
    }
    if (!commit_transaction()) { cancel_transaction(); return false; }
    return $count;
}
