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
 * Decode JSON custom data used by purchasing matching tables.
 *
 * @param mixed $custom_data
 * @return array
 */
function decode_purch_matching_custom_data($custom_data)
{
    if (is_array($custom_data))
        return $custom_data;

    if (!is_string($custom_data) || trim($custom_data) === '')
        return array();

    $decoded = json_decode($custom_data, true);
    if (is_array($decoded))
        return $decoded;

    $decoded = json_decode(stripslashes($custom_data), true);
    return is_array($decoded) ? $decoded : array();
}

/**
 * Build SQL-safe JSON value for custom_data fields.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_purch_matching_custom_data($custom_data)
{
    if ($custom_data === null)
        return 'NULL';

    if (is_string($custom_data)) {
        $decoded = decode_purch_matching_custom_data($custom_data);
        if (!empty($decoded))
            return db_escape(json_encode($decoded));
        return db_escape($custom_data);
    }

    return db_escape(json_encode((array)$custom_data));
}

/**
 * Persist the latest matching result in user session for UI rendering.
 *
 * @param array $result
 * @return void
 */
function set_last_purch_match_result($result)
{
    $_SESSION['purch_matching_last_result'] = is_array($result) ? $result : array();
}

/**
 * Read and optionally clear the latest matching result from session.
 *
 * @param bool $clear
 * @return array
 */
function get_last_purch_match_result($clear = false)
{
    $result = isset($_SESSION['purch_matching_last_result']) && is_array($_SESSION['purch_matching_last_result'])
        ? $_SESSION['purch_matching_last_result']
        : array();

    if ($clear)
        unset($_SESSION['purch_matching_last_result']);

    return $result;
}

/**
 * Clear the latest matching result from session.
 *
 * @return void
 */
function clear_last_purch_match_result()
{
    unset($_SESSION['purch_matching_last_result']);
}

/**
 * Insert one matching tolerance configuration row.
 *
 * @param string $match_type
 * @param string $tolerance_type
 * @param float  $tolerance_value
 * @param string $action_on_exceed
 * @param int    $supplier_id
 * @param mixed  $custom_data
 * @param int    $inactive
 * @return int
 */
function add_matching_config($match_type, $tolerance_type, $tolerance_value, $action_on_exceed, $supplier_id = 0, $custom_data = null, $inactive = 0)
{
    $sql = "INSERT INTO " . TB_PREF . "purch_matching_config (
            match_type, supplier_id, tolerance_type, tolerance_value, action_on_exceed, custom_data, inactive
        ) VALUES (
            " . db_escape($match_type) . ",
            " . (int)$supplier_id . ",
            " . db_escape($tolerance_type) . ",
            " . db_escape((float)$tolerance_value) . ",
            " . db_escape($action_on_exceed) . ",
            " . sql_purch_matching_custom_data($custom_data) . ",
            " . (int)$inactive . "
        )";

    db_query($sql, 'could not add purchase matching configuration');
    return db_insert_id();
}

/**
 * Update one matching tolerance configuration row.
 *
 * @param int    $id
 * @param string $match_type
 * @param string $tolerance_type
 * @param float  $tolerance_value
 * @param string $action_on_exceed
 * @param int    $supplier_id
 * @param mixed  $custom_data
 * @param int    $inactive
 * @return bool
 */
function update_matching_config($id, $match_type, $tolerance_type, $tolerance_value, $action_on_exceed, $supplier_id = 0, $custom_data = null, $inactive = 0)
{
    $sql = "UPDATE " . TB_PREF . "purch_matching_config SET
            match_type = " . db_escape($match_type) . ",
            supplier_id = " . (int)$supplier_id . ",
            tolerance_type = " . db_escape($tolerance_type) . ",
            tolerance_value = " . db_escape((float)$tolerance_value) . ",
            action_on_exceed = " . db_escape($action_on_exceed) . ",
            custom_data = " . sql_purch_matching_custom_data($custom_data) . ",
            inactive = " . (int)$inactive . "
        WHERE id = " . (int)$id;

    db_query($sql, 'could not update purchase matching configuration');
    return true;
}

/**
 * Fetch one matching configuration row by id.
 *
 * @param int $id
 * @return array|false
 */
function get_matching_config_row($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "purch_matching_config
        WHERE id = " . (int)$id;
    $result = db_query($sql, 'could not get purchase matching configuration row');
    return db_fetch($result);
}

/**
 * Fetch matching configuration rows by filters.
 *
 * @param int    $supplier_id
 * @param string $match_type
 * @param int    $include_inactive
 * @return resource
 */
function get_matching_config($supplier_id = -1, $match_type = '', $include_inactive = 0)
{
    $sql = "SELECT * FROM " . TB_PREF . "purch_matching_config WHERE 1=1";

    if ((int)$supplier_id >= 0)
        $sql .= " AND supplier_id = " . (int)$supplier_id;

    if ($match_type !== '')
        $sql .= " AND match_type = " . db_escape($match_type);

    if (!(int)$include_inactive)
        $sql .= " AND inactive = 0";

    $sql .= " ORDER BY supplier_id DESC, match_type";

    return db_query($sql, 'could not get purchase matching configuration');
}

/**
 * Resolve the effective tolerance config for one match type and supplier.
 *
 * @param string $match_type
 * @param int    $supplier_id
 * @return array
 */
function get_matching_tolerance($match_type, $supplier_id = 0)
{
    $sql = "SELECT * FROM " . TB_PREF . "purch_matching_config
        WHERE inactive = 0
            AND match_type = " . db_escape($match_type) . "
            AND supplier_id = " . (int)$supplier_id . "
        ORDER BY id DESC LIMIT 1";
    $result = db_query($sql, 'could not get supplier-level matching tolerance');
    $row = db_fetch($result);

    if (!$row) {
        $sql = "SELECT * FROM " . TB_PREF . "purch_matching_config
            WHERE inactive = 0
                AND match_type = " . db_escape($match_type) . "
                AND supplier_id = 0
            ORDER BY id DESC LIMIT 1";
        $result = db_query($sql, 'could not get default matching tolerance');
        $row = db_fetch($result);
    }

    if (!$row) {
        $fallback_percent = (float)get_company_pref('default_matching_tolerance_pct');
        if ($fallback_percent <= 0)
            $fallback_percent = 5;

        return array(
            'id' => 0,
            'match_type' => $match_type,
            'supplier_id' => 0,
            'tolerance_type' => 'percentage',
            'tolerance_value' => $fallback_percent,
            'action_on_exceed' => 'warn',
            'custom_data' => null,
            'inactive' => 0,
        );
    }

    return $row;
}

/**
 * Create one purchase matching exception record.
 *
 * @param string $exception_type
 * @param int    $trans_type
 * @param int    $trans_no
 * @param array  $details
 * @return int
 */
function create_matching_exception($exception_type, $trans_type, $trans_no, $details)
{
    $sql = "INSERT INTO " . TB_PREF . "purch_matching_exceptions (
            exception_type, trans_type, trans_no, supplier_id, po_number, grn_batch_id,
            stock_id, expected_value, actual_value, variance_amount, variance_percent,
            status, resolution_notes, resolved_by, resolved_date, created_date, custom_data
        ) VALUES (
            " . db_escape($exception_type) . ",
            " . (int)$trans_type . ",
            " . (int)$trans_no . ",
            " . (int)$details['supplier_id'] . ",
            " . (int)$details['po_number'] . ",
            " . (int)$details['grn_batch_id'] . ",
            " . db_escape($details['stock_id']) . ",
            " . db_escape((float)$details['expected_value']) . ",
            " . db_escape((float)$details['actual_value']) . ",
            " . db_escape((float)$details['variance_amount']) . ",
            " . db_escape((float)$details['variance_percent']) . ",
            " . db_escape(isset($details['status']) ? $details['status'] : 'open') . ",
            " . db_escape(isset($details['resolution_notes']) ? $details['resolution_notes'] : '') . ",
            " . (int)(isset($details['resolved_by']) ? $details['resolved_by'] : 0) . ",
            " . (isset($details['resolved_date']) && $details['resolved_date'] !== '' ? db_escape($details['resolved_date']) : 'NULL') . ",
            NOW(),
            " . sql_purch_matching_custom_data(isset($details['custom_data']) ? $details['custom_data'] : null) . "
        )";

    db_query($sql, 'could not create purchase matching exception');
    $exception_id = db_insert_id();

    if (function_exists('hook_invoke_all'))
        hook_invoke_all('purch_match_exception_created', $exception_id);

    return $exception_id;
}

/**
 * Retrieve matching exceptions by filters.
 *
 * @param string $status
 * @param int    $supplier_id
 * @param string $exception_type
 * @param string $date_from
 * @param string $date_to
 * @return resource
 */
function get_matching_exceptions($status = 'open', $supplier_id = 0, $exception_type = '', $date_from = '', $date_to = '')
{
    $sql = "SELECT ex.*, sup.supp_name, usr.real_name AS resolved_by_name
        FROM " . TB_PREF . "purch_matching_exceptions ex
        LEFT JOIN " . TB_PREF . "suppliers sup ON sup.supplier_id = ex.supplier_id
        LEFT JOIN " . TB_PREF . "users usr ON usr.id = ex.resolved_by
        WHERE 1=1";

    if ($status !== '' && $status !== ALL_TEXT)
        $sql .= " AND ex.status = " . db_escape($status);

    if ((int)$supplier_id > 0)
        $sql .= " AND ex.supplier_id = " . (int)$supplier_id;

    if ($exception_type !== '' && $exception_type !== ALL_TEXT)
        $sql .= " AND ex.exception_type = " . db_escape($exception_type);

    if ($date_from !== '' && is_date($date_from))
        $sql .= " AND DATE(ex.created_date) >= " . db_escape(date2sql($date_from));

    if ($date_to !== '' && is_date($date_to))
        $sql .= " AND DATE(ex.created_date) <= " . db_escape(date2sql($date_to));

    $sql .= " ORDER BY ex.created_date DESC, ex.id DESC";

    return db_query($sql, 'could not get purchase matching exceptions');
}

/**
 * Change status of one matching exception.
 *
 * @param int    $id
 * @param string $status
 * @param int    $resolved_by
 * @param string $notes
 * @return bool
 */
function update_matching_exception_status($id, $status, $resolved_by, $notes)
{
    $sql = "UPDATE " . TB_PREF . "purch_matching_exceptions SET
            status = " . db_escape($status) . ",
            resolution_notes = " . db_escape($notes) . ",
            resolved_by = " . (int)$resolved_by . ",
            resolved_date = NOW()
        WHERE id = " . (int)$id;

    db_query($sql, 'could not update purchase matching exception');

    if (function_exists('hook_invoke_all') && $status === 'resolved')
        hook_invoke_all('purch_match_exception_resolved', $id, $status);

    return true;
}

/**
 * Approve one matching exception.
 *
 * @param int    $id
 * @param int    $approved_by
 * @param string $notes
 * @return bool
 */
function approve_exception($id, $approved_by, $notes)
{
    return update_matching_exception_status($id, 'approved', $approved_by, $notes);
}

/**
 * Reject one matching exception.
 *
 * @param int    $id
 * @param int    $rejected_by
 * @param string $notes
 * @return bool
 */
function reject_exception($id, $rejected_by, $notes)
{
    return update_matching_exception_status($id, 'rejected', $rejected_by, $notes);
}

/**
 * Resolve one matching exception.
 *
 * @param int    $id
 * @param int    $resolved_by
 * @param string $notes
 * @return bool
 */
function resolve_exception($id, $resolved_by, $notes)
{
    return update_matching_exception_status($id, 'resolved', $resolved_by, $notes);
}

/**
 * Fetch one bill control policy row by id.
 *
 * @param int $id
 * @return array|false
 */
function get_bill_control_policy_row($id)
{
    $sql = "SELECT * FROM " . TB_PREF . "purch_bill_control_policy WHERE id = " . (int)$id;
    $result = db_query($sql, 'could not get bill control policy');
    return db_fetch($result);
}

/**
 * Fetch bill control policy rows.
 *
 * @param int $include_inactive
 * @return resource
 */
function get_bill_control_policies($include_inactive = 0)
{
    $sql = "SELECT * FROM " . TB_PREF . "purch_bill_control_policy WHERE 1=1";
    if (!(int)$include_inactive)
        $sql .= " AND inactive = 0";
    $sql .= " ORDER BY is_default DESC, id";

    return db_query($sql, 'could not get bill control policies');
}

/**
 * Resolve the active bill control policy.
 *
 * @param int $supplier_id
 * @return array
 */
function get_bill_control_policy($supplier_id = 0)
{
    $sql = "SELECT * FROM " . TB_PREF . "purch_bill_control_policy
        WHERE inactive = 0 AND is_default = 1
        ORDER BY id DESC LIMIT 1";
    $result = db_query($sql, 'could not get default bill control policy');
    $row = db_fetch($result);

    if (!$row) {
        $sql = "SELECT * FROM " . TB_PREF . "purch_bill_control_policy
            WHERE inactive = 0 ORDER BY id DESC LIMIT 1";
        $result = db_query($sql, 'could not get fallback bill control policy');
        $row = db_fetch($result);
    }

    if (!$row) {
        return array(
            'id' => 0,
            'name' => 'Default Policy',
            'bill_basis' => 'on_received',
            'require_grn_before_invoice' => 1,
            'require_po_for_invoice' => 0,
            'allow_over_invoice' => 0,
            'auto_match_on_grn' => 1,
            'is_default' => 1,
            'inactive' => 0,
            'custom_data' => null,
        );
    }

    return $row;
}

/**
 * Add one bill control policy.
 *
 * @param string $name
 * @param string $bill_basis
 * @param int    $require_grn_before_invoice
 * @param int    $require_po_for_invoice
 * @param int    $allow_over_invoice
 * @param int    $auto_match_on_grn
 * @param int    $is_default
 * @param int    $inactive
 * @param mixed  $custom_data
 * @return int
 */
function add_bill_control_policy($name, $bill_basis, $require_grn_before_invoice, $require_po_for_invoice, $allow_over_invoice, $auto_match_on_grn, $is_default, $inactive, $custom_data = null)
{
    if ((int)$is_default)
        db_query("UPDATE " . TB_PREF . "purch_bill_control_policy SET is_default = 0", 'could not reset default policy');

    $sql = "INSERT INTO " . TB_PREF . "purch_bill_control_policy (
            name, bill_basis, require_grn_before_invoice, require_po_for_invoice,
            allow_over_invoice, auto_match_on_grn, is_default, inactive, custom_data
        ) VALUES (
            " . db_escape($name) . ",
            " . db_escape($bill_basis) . ",
            " . (int)$require_grn_before_invoice . ",
            " . (int)$require_po_for_invoice . ",
            " . (int)$allow_over_invoice . ",
            " . (int)$auto_match_on_grn . ",
            " . (int)$is_default . ",
            " . (int)$inactive . ",
            " . sql_purch_matching_custom_data($custom_data) . "
        )";

    db_query($sql, 'could not add bill control policy');
    return db_insert_id();
}

/**
 * Update one bill control policy.
 *
 * @param int    $id
 * @param string $name
 * @param string $bill_basis
 * @param int    $require_grn_before_invoice
 * @param int    $require_po_for_invoice
 * @param int    $allow_over_invoice
 * @param int    $auto_match_on_grn
 * @param int    $is_default
 * @param int    $inactive
 * @param mixed  $custom_data
 * @return bool
 */
function update_bill_control_policy($id, $name, $bill_basis, $require_grn_before_invoice, $require_po_for_invoice, $allow_over_invoice, $auto_match_on_grn, $is_default, $inactive, $custom_data = null)
{
    if ((int)$is_default)
        db_query("UPDATE " . TB_PREF . "purch_bill_control_policy SET is_default = 0", 'could not reset default policy');

    $sql = "UPDATE " . TB_PREF . "purch_bill_control_policy SET
            name = " . db_escape($name) . ",
            bill_basis = " . db_escape($bill_basis) . ",
            require_grn_before_invoice = " . (int)$require_grn_before_invoice . ",
            require_po_for_invoice = " . (int)$require_po_for_invoice . ",
            allow_over_invoice = " . (int)$allow_over_invoice . ",
            auto_match_on_grn = " . (int)$auto_match_on_grn . ",
            is_default = " . (int)$is_default . ",
            inactive = " . (int)$inactive . ",
            custom_data = " . sql_purch_matching_custom_data($custom_data) . "
        WHERE id = " . (int)$id;

    db_query($sql, 'could not update bill control policy');
    return true;
}

/**
 * Validate invoice bill-control policy rules.
 *
 * @param int   $supplier_id
 * @param int   $po_number
 * @param int   $grn_batch_id
 * @param array $context
 * @return array
 */
function check_invoice_allowed($supplier_id, $po_number, $grn_batch_id, $context = array())
{
    $policy = get_bill_control_policy((int)$supplier_id);
    $allowed = true;
    $reason = '';

    if ((int)$policy['require_grn_before_invoice'] && empty($context['has_grn'])) {
        $allowed = false;
        $reason = _('Bill control policy requires a GRN before invoicing.');
    }

    if ($allowed && (int)$policy['require_po_for_invoice'] && empty($context['has_po'])) {
        $allowed = false;
        $reason = _('Bill control policy requires a purchase order reference before invoicing.');
    }

    if (function_exists('hook_invoke_all'))
        hook_invoke_all('purch_bill_control_check', $allowed, $supplier_id, $context);

    return array(
        'allowed' => (bool)$allowed,
        'reason' => $reason,
        'policy' => $policy,
    );
}

/**
 * Calculate an absolute tolerance limit from config and expected value.
 *
 * @param array $tolerance
 * @param float $expected_value
 * @return float
 */
function get_matching_absolute_tolerance_limit($tolerance, $expected_value)
{
    $value = isset($tolerance['tolerance_value']) ? (float)$tolerance['tolerance_value'] : 0;

    if (isset($tolerance['tolerance_type']) && $tolerance['tolerance_type'] === 'fixed_amount')
        return max(0, $value);

    return max(0, abs((float)$expected_value) * ($value / 100));
}

/**
 * Run one price variance check.
 *
 * @param array $po_line
 * @param array $invoice_line
 * @param array $tolerance
 * @return array
 */
function check_price_match($po_line, $invoice_line, $tolerance)
{
    $expected = isset($po_line['unit_price']) ? (float)$po_line['unit_price'] : 0;
    $actual = isset($invoice_line['unit_price']) ? (float)$invoice_line['unit_price'] : 0;
    $variance = $actual - $expected;
    $limit = get_matching_absolute_tolerance_limit($tolerance, $expected);

    $percent = 0;
    if (abs($expected) > 0.000001)
        $percent = ($variance / $expected) * 100;

    return array(
        'expected' => $expected,
        'actual' => $actual,
        'variance' => $variance,
        'variance_percent' => $percent,
        'tolerance_limit' => $limit,
        'exceeded' => abs($variance) > $limit,
    );
}

/**
 * Run one quantity variance check.
 *
 * @param array $po_line
 * @param array $grn_lines
 * @param array $invoice_line
 * @param array $tolerance
 * @return array
 */
function check_quantity_match($po_line, $grn_lines, $invoice_line, $tolerance)
{
    $received_qty = isset($grn_lines['received_qty']) ? (float)$grn_lines['received_qty'] : 0;
    $already_invoiced_qty = isset($grn_lines['already_invoiced_qty']) ? (float)$grn_lines['already_invoiced_qty'] : 0;
    $expected = max(0, $received_qty - $already_invoiced_qty);
    $actual = isset($invoice_line['quantity']) ? (float)$invoice_line['quantity'] : 0;
    $variance = $actual - $expected;
    $limit = get_matching_absolute_tolerance_limit($tolerance, $expected);

    $percent = 0;
    if (abs($expected) > 0.000001)
        $percent = ($variance / $expected) * 100;

    return array(
        'expected' => $expected,
        'actual' => $actual,
        'variance' => $variance,
        'variance_percent' => $percent,
        'tolerance_limit' => $limit,
        'exceeded' => abs($variance) > $limit,
    );
}

/**
 * Run one document-level total variance check.
 *
 * @param float $po_total
 * @param float $grn_total
 * @param float $invoice_total
 * @param array $tolerance
 * @return array
 */
function check_total_match($po_total, $grn_total, $invoice_total, $tolerance)
{
    $expected = $grn_total > 0 ? $grn_total : $po_total;
    $actual = (float)$invoice_total;
    $variance = $actual - $expected;
    $limit = get_matching_absolute_tolerance_limit($tolerance, $expected);

    $percent = 0;
    if (abs($expected) > 0.000001)
        $percent = ($variance / $expected) * 100;

    return array(
        'expected' => $expected,
        'actual' => $actual,
        'variance' => $variance,
        'variance_percent' => $percent,
        'tolerance_limit' => $limit,
        'exceeded' => abs($variance) > $limit,
    );
}

/**
 * Create and optionally persist one variance exception row payload.
 *
 * @param string $exception_type
 * @param int    $trans_type
 * @param int    $trans_no
 * @param int    $supplier_id
 * @param int    $po_number
 * @param int    $grn_batch_id
 * @param string $stock_id
 * @param array  $check_result
 * @param array  $tolerance
 * @param int    $persist
 * @return array
 */
function build_matching_exception_entry($exception_type, $trans_type, $trans_no, $supplier_id, $po_number, $grn_batch_id, $stock_id, $check_result, $tolerance, $persist = 0)
{
    $entry = array(
        'id' => 0,
        'exception_type' => $exception_type,
        'trans_type' => (int)$trans_type,
        'trans_no' => (int)$trans_no,
        'supplier_id' => (int)$supplier_id,
        'po_number' => (int)$po_number,
        'grn_batch_id' => (int)$grn_batch_id,
        'stock_id' => $stock_id,
        'expected_value' => (float)$check_result['expected'],
        'actual_value' => (float)$check_result['actual'],
        'variance_amount' => (float)$check_result['variance'],
        'variance_percent' => (float)$check_result['variance_percent'],
        'action_on_exceed' => isset($tolerance['action_on_exceed']) ? $tolerance['action_on_exceed'] : 'warn',
        'message' => '',
    );

    if ($exception_type === 'price_variance') {
        $entry['message'] = sprintf(
            _('Price variance on item %s. Expected %s, actual %s.'),
            $stock_id,
            price_format($entry['expected_value']),
            price_format($entry['actual_value'])
        );
    } elseif ($exception_type === 'quantity_variance') {
        $entry['message'] = sprintf(
            _('Quantity variance on item %s. Expected %s, actual %s.'),
            $stock_id,
            number_format2($entry['expected_value'], get_qty_dec($stock_id)),
            number_format2($entry['actual_value'], get_qty_dec($stock_id))
        );
    } else {
        $entry['message'] = sprintf(
            _('Total variance detected. Expected %s, actual %s.'),
            price_format($entry['expected_value']),
            price_format($entry['actual_value'])
        );
    }

    if ((int)$persist && (int)$trans_no > 0) {
        $entry['id'] = create_matching_exception(
            $exception_type,
            $trans_type,
            $trans_no,
            array(
                'supplier_id' => $entry['supplier_id'],
                'po_number' => $entry['po_number'],
                'grn_batch_id' => $entry['grn_batch_id'],
                'stock_id' => $entry['stock_id'],
                'expected_value' => $entry['expected_value'],
                'actual_value' => $entry['actual_value'],
                'variance_amount' => $entry['variance_amount'],
                'variance_percent' => $entry['variance_percent'],
                'custom_data' => array('action_on_exceed' => $entry['action_on_exceed']),
            )
        );
    }

    return $entry;
}

/**
 * Execute 3-way matching for supplier invoice/cart lines.
 *
 * @param int        $trans_type
 * @param int        $trans_no
 * @param int        $supplier_id
 * @param supp_trans $supp_trans
 * @param int        $persist_exceptions
 * @return array
 */
function perform_3way_match($trans_type, $trans_no, $supplier_id, $supp_trans = null, $persist_exceptions = 0)
{
    $result = array(
        'matched' => true,
        'exceptions' => array(),
        'totals' => array('po_total' => 0, 'grn_total' => 0, 'invoice_total' => 0),
        'actions' => array(),
        'should_block' => false,
        'requires_approval' => false,
        'warning_messages' => array(),
        'mode' => 'legacy',
        'bill_control_allowed' => true,
        'bill_control_reason' => '',
    );

    if (!get_company_pref('use_3way_matching'))
        return $result;

    $result['mode'] = '3way';

    if ($supp_trans === null || empty($supp_trans->grn_items))
        return $result;

    $has_grn = false;
    $has_po = false;

    $price_tolerance = get_matching_tolerance('price_variance', (int)$supplier_id);
    $qty_tolerance = get_matching_tolerance('quantity_variance', (int)$supplier_id);
    $total_tolerance = get_matching_tolerance('total_variance', (int)$supplier_id);

    foreach ($supp_trans->grn_items as $line) {
        $po_number = 0;
        $grn_batch_id = 0;
        $stock_id = isset($line->item_code) ? $line->item_code : '';

        if ((int)$line->id > 0) {
            $has_grn = true;
            $grn_batch_id = (int)get_grn_batch_from_item($line->id);
        }

        if ((int)$line->po_detail_item > 0) {
            $has_po = true;
            $po_data = get_po_details($line->po_detail_item);
            if ($po_data)
                $po_number = (int)$po_data['order_no'];
        }

        $price_check = check_price_match(
            array('unit_price' => (float)$line->order_price),
            array('unit_price' => (float)$line->chg_price),
            $price_tolerance
        );

        if ($price_check['exceeded']) {
            $exception = build_matching_exception_entry(
                'price_variance',
                $trans_type,
                $trans_no,
                $supplier_id,
                $po_number,
                $grn_batch_id,
                $stock_id,
                $price_check,
                $price_tolerance,
                $persist_exceptions
            );
            $result['exceptions'][] = $exception;
            $result['actions'][$exception['action_on_exceed']] = true;
        }

        $qty_check = check_quantity_match(
            array('ordered_qty' => (float)$line->qty_recd),
            array(
                'received_qty' => (float)$line->qty_recd,
                'already_invoiced_qty' => (float)$line->prev_quantity_inv,
            ),
            array('quantity' => (float)$line->this_quantity_inv),
            $qty_tolerance
        );

        if ($qty_check['exceeded']) {
            $exception = build_matching_exception_entry(
                'quantity_variance',
                $trans_type,
                $trans_no,
                $supplier_id,
                $po_number,
                $grn_batch_id,
                $stock_id,
                $qty_check,
                $qty_tolerance,
                $persist_exceptions
            );
            $result['exceptions'][] = $exception;
            $result['actions'][$exception['action_on_exceed']] = true;
        }

        $line_invoice_total = (float)$line->chg_price * (float)$line->this_quantity_inv;
        $line_po_total = (float)$line->order_price * (float)$line->this_quantity_inv;
        $line_grn_total = (float)$line->order_price * max(0, (float)$line->qty_recd - (float)$line->prev_quantity_inv);

        $result['totals']['invoice_total'] += $line_invoice_total;
        $result['totals']['po_total'] += $line_po_total;
        $result['totals']['grn_total'] += $line_grn_total;
    }

    $bill_control = check_invoice_allowed(
        (int)$supplier_id,
        0,
        0,
        array('has_grn' => $has_grn, 'has_po' => $has_po, 'trans_type' => $trans_type, 'trans_no' => $trans_no)
    );

    if (!$bill_control['allowed']) {
        $result['bill_control_allowed'] = false;
        $result['bill_control_reason'] = $bill_control['reason'];
        $result['actions']['block'] = true;
    }

    $total_check = check_total_match(
        $result['totals']['po_total'],
        $result['totals']['grn_total'],
        $result['totals']['invoice_total'],
        $total_tolerance
    );

    if ($total_check['exceeded']) {
        $exception = build_matching_exception_entry(
            'total_variance',
            $trans_type,
            $trans_no,
            $supplier_id,
            0,
            0,
            '',
            $total_check,
            $total_tolerance,
            $persist_exceptions
        );
        $result['exceptions'][] = $exception;
        $result['actions'][$exception['action_on_exceed']] = true;
    }

    $result['matched'] = count($result['exceptions']) == 0;
    $result['should_block'] = isset($result['actions']['block']) && $result['actions']['block'];
    $result['requires_approval'] = isset($result['actions']['require_approval']) && $result['actions']['require_approval'];

    foreach ($result['exceptions'] as $exception) {
        if ($exception['action_on_exceed'] === 'warn')
            $result['warning_messages'][] = $exception['message'];
    }

    if (function_exists('hook_invoke_all'))
        hook_invoke_all('purch_match_performed', $trans_no, $result);

    return $result;
}

/**
 * Return supplier invoice count of open matching exceptions.
 *
 * @param int $supplier_id
 * @return int
 */
function get_open_matching_exception_count($supplier_id = 0)
{
    $sql = "SELECT COUNT(*) FROM " . TB_PREF . "purch_matching_exceptions WHERE status = 'open'";
    if ((int)$supplier_id > 0)
        $sql .= " AND supplier_id = " . (int)$supplier_id;

    $result = db_query($sql, 'could not count open matching exceptions');
    $row = db_fetch_row($result);
    return (int)$row[0];
}
