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
 * Add a new sales discount program.
 *
 * @param string $name             Program name
 * @param string $program_type     Type: 'coupon','loyalty','automatic','volume'
 * @param string $date_start       Start date (YYYY-MM-DD) or empty
 * @param string $date_end         End date (YYYY-MM-DD) or empty
 * @param double $min_order_amount Minimum order amount to qualify
 * @param double $min_quantity     Minimum quantity to qualify
 * @param string $reward_type      Reward type: 'percentage_discount','fixed_discount','free_product','free_shipping'
 * @param double $reward_value     Discount value (% or fixed amount)
 * @param array  $options          Additional options array
 * @return int The new program ID
 */
function add_discount_program($name, $program_type, $date_start, $date_end,
	$min_order_amount, $min_quantity, $reward_type, $reward_value, $options = array()) {

	$date_start_sql = (!empty($date_start)) ? "'".date2sql($date_start)."'" : 'NULL';
	$date_end_sql   = (!empty($date_end))   ? "'".date2sql($date_end)."'"   : 'NULL';

	$applicable_items      = isset($options['applicable_items'])      ? $options['applicable_items']      : '';
	$applicable_categories = isset($options['applicable_categories']) ? $options['applicable_categories'] : '';
	$applicable_customers  = isset($options['applicable_customers'])  ? $options['applicable_customers']  : '';
	$applicable_sales_types= isset($options['applicable_sales_types'])? $options['applicable_sales_types']: '';
	$reward_product_id     = isset($options['reward_product_id'])     ? $options['reward_product_id']     : '';
	$reward_max_amount     = isset($options['reward_max_amount'])     ? (double)$options['reward_max_amount'] : 0;
	$usage_limit           = isset($options['usage_limit'])           ? (int)$options['usage_limit']      : 0;
	$per_customer_limit    = isset($options['per_customer_limit'])    ? (int)$options['per_customer_limit']: 0;
	$stackable             = isset($options['stackable'])             ? (int)$options['stackable']        : 0;
	$priority              = isset($options['priority'])              ? (int)$options['priority']         : 10;
	$custom_data           = isset($options['custom_data'])           ? $options['custom_data']           : array();

	$sql = "INSERT INTO ".TB_PREF."sales_discount_programs
		(name, program_type, status, date_start, date_end,
		 min_order_amount, min_quantity,
		 applicable_items, applicable_categories, applicable_customers, applicable_sales_types,
		 reward_type, reward_value, reward_product_id, reward_max_amount,
		 usage_limit, per_customer_limit, stackable, priority, inactive, custom_data)
		VALUES (
			".db_escape($name).",
			".db_escape($program_type).",
			'draft',
			".$date_start_sql.",
			".$date_end_sql.",
			".db_escape($min_order_amount).",
			".db_escape($min_quantity).",
			".db_escape($applicable_items).",
			".db_escape($applicable_categories).",
			".db_escape($applicable_customers).",
			".db_escape($applicable_sales_types).",
			".db_escape($reward_type).",
			".db_escape($reward_value).",
			".db_escape($reward_product_id).",
			".db_escape($reward_max_amount).",
			".db_escape($usage_limit).",
			".db_escape($per_customer_limit).",
			".db_escape($stackable).",
			".db_escape($priority).",
			0,
			".db_escape(json_encode($custom_data))."
		)";

	db_query($sql, 'Sales discount program cannot be added');
	return db_insert_id();
}

/**
 * Update an existing sales discount program.
 *
 * @param int   $id          Program ID
 * @param array $update_data Associative array of fields to update
 * @return bool True on success
 */
function update_discount_program($id, $update_data) {
	$allowed_string = array('name','program_type','status','applicable_items',
		'applicable_categories','applicable_customers','applicable_sales_types',
		'reward_type','reward_product_id','custom_data');
	$allowed_num = array('min_order_amount','min_quantity','reward_value','reward_max_amount',
		'usage_limit','usage_count','per_customer_limit','stackable','priority','inactive');
	$allowed_date = array('date_start','date_end');

	$set_clause = array();
	foreach ($update_data as $field => $value) {
		if (in_array($field, $allowed_string)) {
			if ($field === 'custom_data')
				$set_clause[] = "$field = ".db_escape(is_array($value) ? json_encode($value) : $value);
			else
				$set_clause[] = "$field = ".db_escape($value);
		} elseif (in_array($field, $allowed_num)) {
			$set_clause[] = "$field = ".db_escape($value);
		} elseif (in_array($field, $allowed_date)) {
			$set_clause[] = "$field = ".(!empty($value) ? "'".date2sql($value)."'" : 'NULL');
		}
	}

	if (empty($set_clause))
		return true;

	$sql = "UPDATE ".TB_PREF."sales_discount_programs SET "
		.implode(", ", $set_clause)
		." WHERE id = ".db_escape($id);
	db_query($sql, 'Sales discount program cannot be updated');
	return true;
}

/**
 * Delete a discount program. Only allowed when status is 'draft'.
 *
 * @param int $id Program ID
 * @return bool True on success, false if not allowed
 */
function delete_discount_program($id) {
	$prog = get_discount_program($id);
	if (!$prog || $prog['status'] !== 'draft')
		return false;
	// Delete coupons belonging to this program first
	db_query("DELETE FROM ".TB_PREF."sales_coupons WHERE program_id = ".db_escape($id),
		'Sales coupons cannot be deleted');
	db_query("DELETE FROM ".TB_PREF."sales_discount_programs WHERE id = ".db_escape($id),
		'Sales discount program cannot be deleted');
	return true;
}

/**
 * Retrieve a single discount program by ID.
 *
 * @param int $id Program ID
 * @return array|false Row array or false
 */
function get_discount_program($id) {
	$sql = "SELECT * FROM ".TB_PREF."sales_discount_programs WHERE id = ".db_escape($id);
	$result = db_query($sql, 'Cannot get sales discount program');
	return db_fetch($result);
}

/**
 * Retrieve all discount programs.
 *
 * @param bool $active_only When true, return only active (status='active') programs
 * @return resource DB result
 */
function get_discount_programs($active_only = true) {
	$where = $active_only ? "WHERE status = 'active' AND inactive = 0" : "WHERE 1=1";
	$sql = "SELECT * FROM ".TB_PREF."sales_discount_programs $where ORDER BY priority ASC, name ASC";
	return db_query($sql, 'Cannot get sales discount programs');
}

/**
 * Retrieve programs applicable to a given order context.
 * Returns programs matching customer, sales_type, date and order amount.
 *
 * @param int    $debtor_no    Customer debtor number
 * @param int    $sales_type   Sales type ID
 * @param string $date         Order date (YYYY-MM-DD or display format)
 * @param double $order_amount Order gross total before discounts
 * @param array  $items        Array of ['stock_id'=>'', 'qty'=>0, 'category_id'=>'']
 * @return array Array of matching program rows
 */
function get_applicable_programs($debtor_no, $sales_type, $date, $order_amount, $items = array()) {
	$date_sql = date2sql($date);

	$sql = "SELECT * FROM ".TB_PREF."sales_discount_programs
		WHERE status = 'active'
		  AND inactive = 0
		  AND (date_start IS NULL OR date_start <= '$date_sql')
		  AND (date_end   IS NULL OR date_end   >= '$date_sql')
		  AND (usage_limit = 0 OR usage_count < usage_limit)
		ORDER BY priority ASC, name ASC";

	$result = db_query($sql, 'Cannot get applicable discount programs');
	$matching = array();

	while ($prog = db_fetch($result)) {
		// Check minimum order amount
		if ($prog['min_order_amount'] > 0 && $order_amount < $prog['min_order_amount'])
			continue;

		// Check customer restriction
		if (!empty($prog['applicable_customers'])) {
			$allowed_customers = array_map('intval', explode(',', $prog['applicable_customers']));
			if (!in_array((int)$debtor_no, $allowed_customers))
				continue;
		}

		// Check sales type restriction
		if (!empty($prog['applicable_sales_types'])) {
			$allowed_types = array_map('intval', explode(',', $prog['applicable_sales_types']));
			if (!in_array((int)$sales_type, $allowed_types))
				continue;
		}

		// Check item or category restriction (if any items passed)
		if (!empty($prog['applicable_items']) && !empty($items)) {
			$allowed_items = array_map('trim', explode(',', $prog['applicable_items']));
			$item_match = false;
			foreach ($items as $item) {
				if (in_array($item['stock_id'], $allowed_items)) {
					$item_match = true;
					break;
				}
			}
			if (!$item_match)
				continue;
		}

		if (!empty($prog['applicable_categories']) && !empty($items)) {
			$allowed_cats = array_map('trim', explode(',', $prog['applicable_categories']));
			$cat_match = false;
			foreach ($items as $item) {
				if (!empty($item['category_id']) && in_array($item['category_id'], $allowed_cats)) {
					$cat_match = true;
					break;
				}
			}
			if (!$cat_match)
				continue;
		}

		$matching[] = $prog;
	}

	return $matching;
}

// ============================================================================
// COUPON FUNCTIONS
// ============================================================================

/**
 * Generate coupon codes for a discount program.
 *
 * @param int    $program_id  Program ID
 * @param int    $count       Number of coupons to generate
 * @param string $prefix      Optional prefix for code
 * @param int    $debtor_no   0 = generic; >0 = customer-specific
 * @param string $valid_from  Start date (optional)
 * @param string $valid_until Expiry date (optional)
 * @param int    $usage_limit Max uses per coupon (default 1)
 * @return int Number of coupons created
 */
function generate_coupons($program_id, $count, $prefix = '', $debtor_no = 0,
	$valid_from = '', $valid_until = '', $usage_limit = 1) {

	$created = 0;
	$valid_from_sql  = (!empty($valid_from))  ? "'".date2sql($valid_from)."'"  : 'NULL';
	$valid_until_sql = (!empty($valid_until)) ? "'".date2sql($valid_until)."'" : 'NULL';

	for ($i = 0; $i < $count; $i++) {
		$code = strtoupper($prefix . substr(md5(uniqid(mt_rand(), true)), 0, 8));
		// Ensure uniqueness
		$check = db_query("SELECT id FROM ".TB_PREF."sales_coupons WHERE code = ".db_escape($code));
		if (db_num_rows($check) > 0)
			continue;

		$sql = "INSERT INTO ".TB_PREF."sales_coupons
			(code, program_id, debtor_no, valid_from, valid_until, usage_limit, is_active)
			VALUES (
				".db_escape($code).",
				".db_escape($program_id).",
				".db_escape($debtor_no).",
				".$valid_from_sql.",
				".$valid_until_sql.",
				".db_escape($usage_limit).",
				1
			)";
		db_query($sql, 'Coupon cannot be inserted');
		$created++;
	}
	return $created;
}

/**
 * Validate a coupon code against a customer and date.
 *
 * @param string $code      Coupon code
 * @param int    $debtor_no Customer debtor number
 * @param string $date      Check date (display format or YYYY-MM-DD)
 * @return array ['valid'=>bool, 'program'=>row|null, 'coupon'=>row|null, 'message'=>string]
 */
function validate_coupon($code, $debtor_no, $date) {
	$result = array('valid' => false, 'program' => null, 'coupon' => null, 'message' => '');

	// Allow hook to override validation
	hook_invoke_all('sales_coupon_validate', $result, $code, $debtor_no);
	if ($result['valid'])
		return $result; // hook already resolved it

	$date_sql = date2sql($date);

	$sql = "SELECT c.*, p.name AS program_name, p.status AS prog_status,
			p.reward_type, p.reward_value, p.reward_max_amount,
			p.min_order_amount, p.inactive AS prog_inactive,
			p.per_customer_limit, p.stackable
		FROM ".TB_PREF."sales_coupons c
		JOIN ".TB_PREF."sales_discount_programs p ON p.id = c.program_id
		WHERE c.code = ".db_escape($code);
	$res = db_query($sql, 'Cannot validate coupon');
	$coupon = db_fetch($res);

	if (!$coupon) {
		$result['message'] = _('Coupon code not found.');
		return $result;
	}

	if (!$coupon['is_active']) {
		$result['message'] = _('This coupon is no longer active.');
		return $result;
	}

	if ($coupon['prog_status'] !== 'active' || $coupon['prog_inactive']) {
		$result['message'] = _('The promotion associated with this coupon is not active.');
		return $result;
	}

	if ($coupon['valid_from'] !== null && $coupon['valid_from'] > $date_sql) {
		$result['message'] = _('This coupon is not yet valid.');
		return $result;
	}

	if ($coupon['valid_until'] !== null && $coupon['valid_until'] < $date_sql) {
		$result['message'] = _('This coupon has expired.');
		return $result;
	}

	if ($coupon['usage_limit'] > 0 && $coupon['usage_count'] >= $coupon['usage_limit']) {
		$result['message'] = _('This coupon has reached its usage limit.');
		return $result;
	}

	// Check per-customer limit
	if ($coupon['per_customer_limit'] > 0) {
		$used_sql = "SELECT COUNT(*) AS cnt FROM ".TB_PREF."sales_discount_usage
			WHERE program_id = ".db_escape($coupon['program_id'])."
			  AND coupon_id  = ".db_escape($coupon['id'])."
			  AND debtor_no  = ".db_escape($debtor_no);
		$used_res = db_query($used_sql, 'Cannot check coupon usage');
		$used_row = db_fetch($used_res);
		if ($used_row['cnt'] >= $coupon['per_customer_limit']) {
			$result['message'] = _('You have already used this coupon the maximum number of times.');
			return $result;
		}
	}

	// Check debtor restriction (0 = generic)
	if ($coupon['debtor_no'] > 0 && (int)$coupon['debtor_no'] !== (int)$debtor_no) {
		$result['message'] = _('This coupon is not valid for your account.');
		return $result;
	}

	$result['valid']   = true;
	$result['coupon']  = $coupon;
	$result['program'] = get_discount_program($coupon['program_id']);
	$result['message'] = _('Coupon is valid.');
	return $result;
}

/**
 * Record coupon redemption and increment usage counters.
 * Call this after the transaction is committed.
 *
 * @param string $code       Coupon code
 * @param int    $debtor_no  Customer debtor number
 * @param int    $trans_type Transaction type constant (ST_*)
 * @param int    $trans_no   Transaction number
 * @param double $amount     Discount amount actually applied
 * @return void
 */
function redeem_coupon($code, $debtor_no, $trans_type, $trans_no, $amount) {
	$sql = "SELECT id, program_id FROM ".TB_PREF."sales_coupons WHERE code = ".db_escape($code);
	$res = db_query($sql, 'Cannot get coupon for redemption');
	$coupon = db_fetch($res);
	if (!$coupon)
		return;

	// Increment coupon usage count
	db_query("UPDATE ".TB_PREF."sales_coupons SET usage_count = usage_count + 1 WHERE id = ".db_escape($coupon['id']),
		'Cannot update coupon usage count');

	// Record usage
	record_discount_usage($coupon['program_id'], $coupon['id'], $debtor_no, $trans_type, $trans_no, $amount);
}

// ============================================================================
// AUTOMATIC DISCOUNT CALCULATION
// ============================================================================

/**
 * Calculate automatic discounts applicable to a Cart object.
 * Returns an array of discount entries sorted by priority.
 * Only 'automatic' and 'volume' type programs are evaluated here.
 *
 * @param object $cart Cart instance
 * @return array Array of ['program_id'=>, 'name'=>, 'reward_type'=>, 'reward_value'=>, 'discount_amount'=>, 'stackable'=>]
 */
function calculate_automatic_discounts(&$cart) {
	$discounts = array();

	if (empty(get_company_pref('use_discount_programs')))
		return $discounts;

	$date = $cart->document_date ? $cart->document_date : today();
	$date_sql = date2sql($date);
	$order_amount = $cart->get_items_total();

	// Build items list for category/item checks
	$items = array();
	foreach ($cart->line_items as $line) {
		$items[] = array('stock_id' => $line->stock_id, 'qty' => $line->quantity, 'category_id' => '');
	}

	$programs = get_applicable_programs(
		$cart->customer_id,
		$cart->sales_type,
		$date,
		$order_amount,
		$items
	);

	foreach ($programs as $prog) {
		// Only automatic & volume types are evaluated without coupon entry
		if (!in_array($prog['program_type'], array('automatic', 'volume')))
			continue;

		$discount_amount = _compute_discount_amount($prog, $order_amount);
		if ($discount_amount <= 0)
			continue;

		$discounts[] = array(
			'program_id'      => $prog['id'],
			'name'            => $prog['name'],
			'reward_type'     => $prog['reward_type'],
			'reward_value'    => $prog['reward_value'],
			'discount_amount' => $discount_amount,
			'stackable'       => (bool)$prog['stackable'],
			'coupon_code'     => '',
			'coupon_id'       => 0,
		);
	}

	hook_invoke_all('sales_discount_calculate', $discounts, $cart);

	return $discounts;
}

/**
 * Apply a specific discount program (or coupon) to a Cart.
 * Validates conditions, computes amount, and updates cart->applied_discounts.
 *
 * @param object $cart        Cart instance (passed by reference)
 * @param int    $program_id  Discount program ID
 * @param string $coupon_code Coupon code (empty for automatic programs)
 * @return array ['success'=>bool, 'message'=>string, 'discount_amount'=>double]
 */
function apply_discount_to_cart(&$cart, $program_id, $coupon_code = '') {
	$prog = get_discount_program($program_id);
	if (!$prog)
		return array('success' => false, 'message' => _('Discount program not found.'), 'discount_amount' => 0);

	hook_invoke_all('sales_discount_before_apply', $prog, $cart);

	$order_amount = $cart->get_items_total();

	// Validate coupon if required
	$coupon_row = null;
	$coupon_id  = 0;
	if (!empty($coupon_code)) {
		$validation = validate_coupon($coupon_code, $cart->customer_id, $cart->document_date ? $cart->document_date : today());
		if (!$validation['valid'])
			return array('success' => false, 'message' => $validation['message'], 'discount_amount' => 0);
		$coupon_row = $validation['coupon'];
		$coupon_id  = $coupon_row['id'];
	}

	// Check non-stackable conflict
	if (!$prog['stackable'] && !empty($cart->applied_discounts)) {
		// Check if any non-stackable discount already applied
		foreach ($cart->applied_discounts as $applied) {
			if (!$applied['stackable'])
				return array('success' => false, 'message' => _('A non-stackable discount is already applied.'), 'discount_amount' => 0);
		}
	}

	$discount_amount = _compute_discount_amount($prog, $order_amount);

	if ($discount_amount <= 0)
		return array('success' => false, 'message' => _('Discount conditions not met.'), 'discount_amount' => 0);

	// Add to cart's applied discounts list
	$cart->applied_discounts[] = array(
		'program_id'      => $program_id,
		'name'            => $prog['name'],
		'reward_type'     => $prog['reward_type'],
		'reward_value'    => $prog['reward_value'],
		'discount_amount' => $discount_amount,
		'stackable'       => (bool)$prog['stackable'],
		'coupon_code'     => $coupon_code,
		'coupon_id'       => $coupon_id,
	);

	if (!empty($coupon_code))
		$cart->coupon_code = $coupon_code;

	hook_invoke_all('sales_discount_after_apply', $program_id, $cart);

	return array('success' => true, 'message' => _('Discount applied.'), 'discount_amount' => $discount_amount);
}

// ============================================================================
// DISCOUNT USAGE RECORDING
// ============================================================================

/**
 * Record a discount program usage entry in 0_sales_discount_usage.
 * Also increments the program usage_count.
 *
 * @param int    $program_id  Discount program ID
 * @param int    $coupon_id   Coupon row ID (0 if not a coupon discount)
 * @param int    $debtor_no   Customer debtor number
 * @param int    $trans_type  Transaction type (ST_* constant)
 * @param int    $trans_no    Transaction number
 * @param double $amount      Discount amount applied
 * @return void
 */
function record_discount_usage($program_id, $coupon_id, $debtor_no, $trans_type, $trans_no, $amount) {
	$sql = "INSERT INTO ".TB_PREF."sales_discount_usage
		(program_id, coupon_id, debtor_no, trans_type, trans_no, discount_amount, applied_date)
		VALUES (
			".db_escape($program_id).",
			".db_escape($coupon_id).",
			".db_escape($debtor_no).",
			".db_escape($trans_type).",
			".db_escape($trans_no).",
			".db_escape($amount).",
			NOW()
		)";
	db_query($sql, 'Cannot record discount usage');

	// Increment program usage_count
	db_query("UPDATE ".TB_PREF."sales_discount_programs SET usage_count = usage_count + 1
		WHERE id = ".db_escape($program_id),
		'Cannot update discount program usage count');
}

/**
 * Get the discount usage report between two dates.
 *
 * @param string $date_from  From date (display format)
 * @param string $date_to    To date (display format)
 * @param int    $program_id Filter by program (0 = all)
 * @return resource DB result
 */
function get_discount_usage_report($date_from, $date_to, $program_id = 0) {
	$from_sql = date2sql($date_from);
	$to_sql   = date2sql($date_to);

	$where_prog = ($program_id > 0) ? " AND u.program_id = ".db_escape($program_id) : '';

	$sql = "SELECT u.id, u.program_id, u.coupon_id, u.debtor_no, u.trans_type, u.trans_no,
		u.discount_amount, u.applied_date,
		p.name AS program_name, p.program_type,
		c.code AS coupon_code,
		d.name AS customer_name
	FROM ".TB_PREF."sales_discount_usage u
	LEFT JOIN ".TB_PREF."sales_discount_programs p ON p.id = u.program_id
	LEFT JOIN ".TB_PREF."sales_coupons c ON c.id = u.coupon_id
	LEFT JOIN ".TB_PREF."debtors_master d ON d.debtor_no = u.debtor_no
	WHERE DATE(u.applied_date) >= '$from_sql'
	  AND DATE(u.applied_date) <= '$to_sql'
	$where_prog
	ORDER BY u.applied_date DESC";

	return db_query($sql, 'Cannot get discount usage report');
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Compute the discount amount from a program row given an order total.
 *
 * @param array  $prog         Program row from DB
 * @param double $order_amount Gross order total
 * @return double Computed discount amount (>= 0)
 */
function _compute_discount_amount($prog, $order_amount) {
	switch ($prog['reward_type']) {
		case 'percentage_discount':
			$amount = $order_amount * ($prog['reward_value'] / 100.0);
			if ($prog['reward_max_amount'] > 0 && $amount > $prog['reward_max_amount'])
				$amount = $prog['reward_max_amount'];
			return round($amount, user_price_dec());

		case 'fixed_discount':
			return min($prog['reward_value'], $order_amount);

		case 'free_shipping':
			// Caller handles free_shipping; return 0 here (no monetary amount computed)
			return 0;

		case 'free_product':
			// Free product is handled by adding a line with zero price; return 0 here
			return 0;

		default:
			return 0;
	}
}

/**
 * Get all coupons for a given program.
 *
 * @param int  $program_id Program ID
 * @param bool $active_only When true, return only active coupons
 * @return resource DB result
 */
function get_coupons_for_program($program_id, $active_only = false) {
	$where_active = $active_only ? " AND is_active = 1" : '';
	$sql = "SELECT c.*, d.name AS customer_name
		FROM ".TB_PREF."sales_coupons c
		LEFT JOIN ".TB_PREF."debtors_master d ON d.debtor_no = c.debtor_no
		WHERE c.program_id = ".db_escape($program_id)."
		$where_active
		ORDER BY c.id ASC";
	return db_query($sql, 'Cannot get coupons for program');
}

/**
 * Get the total discount amount from all entries in cart->applied_discounts.
 *
 * @param object $cart Cart instance
 * @return double Sum of all applied discount amounts
 */
function get_cart_total_discount(&$cart) {
	$total = 0;
	if (!empty($cart->applied_discounts)) {
		foreach ($cart->applied_discounts as $d)
			$total += $d['discount_amount'];
	}
	return $total;
}

/**
 * Merge the cart's discount state into an existing custom_data payload.
 *
 * @param mixed  $existing_custom_data Existing JSON string or array payload
 * @param object $cart                 Cart instance
 * @return array Merged custom_data array
 */
function get_discount_custom_data_for_save($existing_custom_data, &$cart) {
	$data = array();

	if (is_array($existing_custom_data))
		$data = $existing_custom_data;
	elseif (!empty($existing_custom_data)) {
		$decoded = json_decode($existing_custom_data, true);
		if (is_array($decoded))
			$data = $decoded;
	}

	$data['sales_discount_state'] = array(
		'coupon_code' => isset($cart->coupon_code) ? (string) $cart->coupon_code : '',
		'applied_discounts' => !empty($cart->applied_discounts) && is_array($cart->applied_discounts)
			? array_values($cart->applied_discounts)
			: array(),
	);

	return $data;
}

/**
 * Build a SQL-safe JSON literal for discount custom_data columns.
 *
 * JSON must bypass db_escape() because that path HTML-encodes quotes,
 * which breaks json_valid() constraints on runtime tables.
 *
 * @param mixed  $existing_custom_data Existing JSON string or array payload
 * @param object $cart                 Cart instance
 * @return string SQL literal including surrounding quotes
 */
function get_discount_custom_data_sql_value($existing_custom_data, &$cart) {
	$json = json_encode(get_discount_custom_data_for_save($existing_custom_data, $cart));

	if (!is_string($json) || $json === '')
		$json = '{}';

	return "'" . db_escape_function($json) . "'";
}

/**
 * Restore cart discount state from a custom_data payload.
 *
 * @param object $cart             Cart instance
 * @param mixed  $raw_custom_data  Existing JSON string or array payload
 * @return void
 */
function restore_cart_discount_state(&$cart, $raw_custom_data) {
	$data = array();

	if (is_array($raw_custom_data))
		$data = $raw_custom_data;
	elseif (!empty($raw_custom_data)) {
		$decoded = json_decode($raw_custom_data, true);
		if (is_array($decoded))
			$data = $decoded;
	}

	if (isset($data['sales_discount_state']) && is_array($data['sales_discount_state']))
		$data = $data['sales_discount_state'];

	$cart->coupon_code = isset($data['coupon_code']) ? (string) $data['coupon_code'] : '';
	$cart->applied_discounts = isset($data['applied_discounts']) && is_array($data['applied_discounts'])
		? array_values($data['applied_discounts'])
		: array();
}

/**
 * Record applied cart discounts for a posted document, skipping duplicates.
 *
 * @param object $cart       Cart instance
 * @param int    $trans_type Transaction type
 * @param int    $trans_no   Transaction number
 * @return void
 */
function record_cart_discount_usage(&$cart, $trans_type, $trans_no) {
	if (empty($cart->applied_discounts) || !$trans_no)
		return;

	foreach ($cart->applied_discounts as $discount_entry) {
		$program_id = isset($discount_entry['program_id']) ? (int) $discount_entry['program_id'] : 0;
		$coupon_id = isset($discount_entry['coupon_id']) ? (int) $discount_entry['coupon_id'] : 0;
		$amount = isset($discount_entry['discount_amount']) ? (double) $discount_entry['discount_amount'] : 0;

		if ($program_id <= 0 || $amount <= 0)
			continue;

		$sql = "SELECT COUNT(*) AS existing_count
			FROM ".TB_PREF."sales_discount_usage
			WHERE program_id = ".db_escape($program_id)."
				AND coupon_id = ".db_escape($coupon_id)."
				AND trans_type = ".db_escape($trans_type)."
				AND trans_no = ".db_escape($trans_no);
		$result = db_query($sql, 'Cannot validate existing discount usage');
		$row = db_fetch($result);

		if (!empty($row['existing_count']))
			continue;

		record_discount_usage($program_id, $coupon_id, $cart->customer_id, $trans_type, $trans_no, $amount);
	}
}
