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
 * Add a new sales pricelist.
 * 
 * @param string $name Pricelist name
 * @param string $currency Currency code (empty = all currencies)
 * @param string $date_start Start date (format: YYYY-MM-DD)
 * @param string $date_end End date (format: YYYY-MM-DD)
 * @param int $priority Priority level (lower = higher priority)
 * @param string $applicable_to Applicability scope: 'all', 'sales_type', 'customer', 'customer_group', 'branch'
 * @param int $applicable_id Foreign key ID based on applicable_to
 * @param string $description Optional description
 * @param bool $is_default Mark as default pricelist
 * @param array $custom_data Optional custom data as JSON
 * @return int The ID of the newly created pricelist
 */
function add_sales_pricelist($name, $currency = '', $date_start = null, $date_end = null, $priority = 10, 
	$applicable_to = 'all', $applicable_id = 0, $description = '', $is_default = false, $custom_data = array()) {
	
	global $SysPrefs;
	
	begin_transaction();
	hook_invoke_all('sales_pricelist_before_save', $pricelist_data);
	
	$date_start_sql = ($date_start) ? date2sql($date_start) : 'NULL';
	$date_end_sql = ($date_end) ? date2sql($date_end) : 'NULL';
	
	$sql = "INSERT INTO ".TB_PREF."sales_pricelists 
		(name, description, currency, is_default, date_start, date_end, priority, 
		 applicable_to, applicable_id, custom_data, inactive)
		VALUES (
			".db_escape($name).",
			".db_escape($description).",
			".db_escape($currency).",
			".($is_default ? 1 : 0).",
			".$date_start_sql.",
			".$date_end_sql.",
			".db_escape($priority).",
			".db_escape($applicable_to).",
			".db_escape($applicable_id).",
			".db_escape(json_encode($custom_data)).",
			0
		)";
	
	db_query($sql, 'Sales pricelist cannot be added');
	$pricelist_id = db_insert_id();
	
	hook_invoke_all('sales_pricelist_after_save', $pricelist_id);
	commit_transaction();
	
	return $pricelist_id;
}

/**
 * Update an existing sales pricelist.
 * 
 * @param int $id Pricelist ID
 * @param array $update_data Associative array of fields to update
 * @return bool True on success
 */
function update_sales_pricelist($id, $update_data) {
	begin_transaction();
	hook_invoke_all('sales_pricelist_before_save', $update_data);
	
	$set_clause = array();
	foreach ($update_data as $field => $value) {
		if (in_array($field, array('name', 'description', 'currency', 'applicable_to'))) {
			$set_clause[] = "$field = ".db_escape($value);
		} else if (in_array($field, array('is_default', 'priority', 'applicable_id', 'inactive'))) {
			$set_clause[] = "$field = ".db_escape($value);
		} else if ($field == 'date_start' || $field == 'date_end') {
			$set_clause[] = "$field = ".($value ? date2sql($value) : 'NULL');
		} else if ($field == 'custom_data') {
			$set_clause[] = "$field = ".db_escape(json_encode($value));
		}
	}
	
	if (empty($set_clause)) {
		commit_transaction();
		return true;
	}
	
	$sql = "UPDATE ".TB_PREF."sales_pricelists SET ".implode(", ", $set_clause)." WHERE id = ".db_escape($id);
	db_query($sql, 'Sales pricelist cannot be updated');
	
	hook_invoke_all('sales_pricelist_after_save', $id);
	commit_transaction();
	
	return true;
}

/**
 * Delete a sales pricelist and all its rules.
 * 
 * @param int $id Pricelist ID
 * @return bool True on success
 */
function delete_sales_pricelist($id) {
	begin_transaction();
	
	// Delete all rules for this pricelist
	$sql = "DELETE FROM ".TB_PREF."sales_pricelist_rules WHERE pricelist_id = ".db_escape($id);
	db_query($sql, 'Pricelist rules cannot be deleted');
	
	// Delete pricelist itself
	$sql = "DELETE FROM ".TB_PREF."sales_pricelists WHERE id = ".db_escape($id);
	db_query($sql, 'Pricelist cannot be deleted');
	
	commit_transaction();
	return true;
}

/**
 * Get a specific sales pricelist by ID.
 * 
 * @param int $id Pricelist ID
 * @return array|null Pricelist data or null if not found
 */
function get_sales_pricelist($id) {
	$sql = "SELECT * FROM ".TB_PREF."sales_pricelists WHERE id = ".db_escape($id);
	$result = db_query($sql, 'Pricelist cannot be retrieved');
	
	if (db_num_rows($result) > 0) {
		$data = db_fetch_assoc($result);
		if (!empty($data['custom_data'])) {
			$data['custom_data'] = json_decode($data['custom_data'], true);
		}
		return $data;
	}
	return null;
}

/**
 * Get all sales pricelists with optional filtering.
 * 
 * @param bool $inactive Include inactive pricelists
 * @param string $applicable_to Filter by applicability type
 * @param int $applicable_id Filter by applicable ID
 * @return array Array of pricelist records
 */
function get_sales_pricelists($inactive = false, $applicable_to = '', $applicable_id = 0) {
	$sql = "SELECT * FROM ".TB_PREF."sales_pricelists WHERE 1=1";
	
	if (!$inactive) {
		$sql .= " AND inactive = 0";
	}
	
	if (!empty($applicable_to)) {
		$sql .= " AND applicable_to = ".db_escape($applicable_to);
	}
	
	if ($applicable_id > 0) {
		$sql .= " AND applicable_id = ".db_escape($applicable_id);
	}
	
	$sql .= " ORDER BY priority ASC, id ASC";
	
	$result = db_query($sql, 'Pricelists cannot be retrieved');
	$pricelists = array();
	
	while ($row = db_fetch_assoc($result)) {
		if (!empty($row['custom_data'])) {
			$row['custom_data'] = json_decode($row['custom_data'], true);
		}
		$pricelists[] = $row;
	}
	
	return $pricelists;
}

/**
 * Get applicable pricelists for a given context (customer, branch, sales_type, currency).
 * Returns pricelists ordered by priority.
 * 
 * @param int $customer_id Customer ID (debtor_no)
 * @param int $branch_code Branch code
 * @param int $sales_type Sales type ID
 * @param string $currency Currency code
 * @param string $date Current date (YYYY-MM-DD format)
 * @return array Array of applicable pricelists
 */
function get_applicable_pricelists($customer_id = 0, $branch_code = 0, $sales_type = 0, $currency = '', $date = null) {
	if (!$date) {
		$date = date('Y-m-d');
	}
	
	$sql_date = date2sql($date);
	
	$sql = "SELECT * FROM ".TB_PREF."sales_pricelists 
		WHERE inactive = 0
		AND (date_start IS NULL OR DATE(date_start) <= '$sql_date')
		AND (date_end IS NULL OR DATE(date_end) >= '$sql_date')
		AND (currency = '' OR currency = ".db_escape($currency).")
		AND (
			applicable_to = 'all'
			OR (applicable_to = 'customer' AND applicable_id = ".db_escape($customer_id).")
			OR (applicable_to = 'sales_type' AND applicable_id = ".db_escape($sales_type).")
			OR (applicable_to = 'branch' AND applicable_id = ".db_escape($branch_code).")
		)
		ORDER BY priority ASC, id ASC";
	
	$result = db_query($sql, 'Applicable pricelists cannot be retrieved');
	$pricelists = array();
	
	while ($row = db_fetch_assoc($result)) {
		if (!empty($row['custom_data'])) {
			$row['custom_data'] = json_decode($row['custom_data'], true);
		}
		$pricelists[] = $row;
	}
	
	return $pricelists;
}

/**
 * Add a rule to a pricelist.
 * 
 * @param int $pricelist_id Pricelist ID
 * @param string $stock_id Item code (empty = all items)
 * @param int $category_id Stock category ID (0 = all categories)
 * @param double $min_quantity Minimum quantity for this rule to apply
 * @param string $computation_type 'fixed', 'percentage', or 'formula'
 * @param double $fixed_price Fixed price (for fixed computation type)
 * @param double $percentage Percentage discount/markup (for percentage type)
 * @param string $base_price_type 'list_price', 'cost', or 'other_pricelist'
 * @param int $base_pricelist_id Base pricelist ID (for other_pricelist type)
 * @param double $surcharge Surcharge to add after computation
 * @param double $rounding Rounding value
 * @param int $priority Priority level
 * @param string $date_start Rule start date
 * @param string $date_end Rule end date
 * @param array $custom_data Custom data
 * @return int Rule ID
 */
function add_pricelist_rule($pricelist_id, $stock_id = '', $category_id = 0, $min_quantity = 0, 
	$computation_type = 'fixed', $fixed_price = 0, $percentage = 0, $base_price_type = 'list_price', 
	$base_pricelist_id = 0, $surcharge = 0, $rounding = 0.01, $priority = 10, 
	$date_start = null, $date_end = null, $custom_data = array()) {
	
	begin_transaction();
	
	$date_start_sql = ($date_start) ? date2sql($date_start) : 'NULL';
	$date_end_sql = ($date_end) ? date2sql($date_end) : 'NULL';
	
	$sql = "INSERT INTO ".TB_PREF."sales_pricelist_rules 
		(pricelist_id, stock_id, stock_category_id, min_quantity, computation_type, 
		 fixed_price, percentage, base_price_type, base_pricelist_id, surcharge, 
		 rounding, priority, date_start, date_end, custom_data, inactive)
		VALUES (
			".db_escape($pricelist_id).",
			".db_escape($stock_id).",
			".db_escape($category_id).",
			".db_escape($min_quantity).",
			".db_escape($computation_type).",
			".db_escape($fixed_price).",
			".db_escape($percentage).",
			".db_escape($base_price_type).",
			".db_escape($base_pricelist_id).",
			".db_escape($surcharge).",
			".db_escape($rounding).",
			".db_escape($priority).",
			".$date_start_sql.",
			".$date_end_sql.",
			".db_escape(json_encode($custom_data)).",
			0
		)";
	
	db_query($sql, 'Pricelist rule cannot be added');
	$rule_id = db_insert_id();
	
	commit_transaction();
	return $rule_id;
}

/**
 * Update a pricelist rule.
 * 
 * @param int $id Rule ID
 * @param array $update_data Associative array of fields to update
 * @return bool True on success
 */
function update_pricelist_rule($id, $update_data) {
	begin_transaction();
	
	$set_clause = array();
	foreach ($update_data as $field => $value) {
		if (in_array($field, array('stock_id', 'computation_type', 'base_price_type'))) {
			$set_clause[] = "$field = ".db_escape($value);
		} else if (in_array($field, array('stock_category_id', 'priority', 'base_pricelist_id', 'inactive'))) {
			$set_clause[] = "$field = ".db_escape($value);
		} else if (in_array($field, array('min_quantity', 'fixed_price', 'percentage', 'surcharge', 'rounding'))) {
			$set_clause[] = "$field = ".db_escape($value);
		} else if ($field == 'date_start' || $field == 'date_end') {
			$set_clause[] = "$field = ".($value ? date2sql($value) : 'NULL');
		} else if ($field == 'custom_data') {
			$set_clause[] = "$field = ".db_escape(json_encode($value));
		}
	}
	
	if (empty($set_clause)) {
		commit_transaction();
		return true;
	}
	
	$sql = "UPDATE ".TB_PREF."sales_pricelist_rules SET ".implode(", ", $set_clause)." WHERE id = ".db_escape($id);
	db_query($sql, 'Pricelist rule cannot be updated');
	
	commit_transaction();
	return true;
}

/**
 * Delete a pricelist rule.
 * 
 * @param int $id Rule ID
 * @return bool True on success
 */
function delete_pricelist_rule($id) {
	begin_transaction();
	
	$sql = "DELETE FROM ".TB_PREF."sales_pricelist_rules WHERE id = ".db_escape($id);
	db_query($sql, 'Pricelist rule cannot be deleted');
	
	commit_transaction();
	return true;
}

/**
 * Get all rules for a specific pricelist.
 * 
 * @param int $pricelist_id Pricelist ID
 * @param bool $inactive Include inactive rules
 * @return array Array of rule records
 */
function get_pricelist_rules($pricelist_id, $inactive = false) {
	$sql = "SELECT * FROM ".TB_PREF."sales_pricelist_rules WHERE pricelist_id = ".db_escape($pricelist_id);
	
	if (!$inactive) {
		$sql .= " AND inactive = 0";
	}
	
	$sql .= " ORDER BY priority ASC, min_quantity ASC, id ASC";
	
	$result = db_query($sql, 'Pricelist rules cannot be retrieved');
	$rules = array();
	
	while ($row = db_fetch_assoc($result)) {
		if (!empty($row['custom_data'])) {
			$row['custom_data'] = json_decode($row['custom_data'], true);
		}
		$rules[] = $row;
	}
	
	return $rules;
}

/**
 * Compute the best price for an item using applicable pricelists and rules.
 * This is the core pricing engine function.
 * 
 * @param string $stock_id Item code
 * @param double $quantity Quantity ordered
 * @param int $customer_id Customer ID (debtor_no)
 * @param int $branch_code Branch code
 * @param int $sales_type Sales type ID
 * @param string $currency Currency code
 * @param string $date Current date (YYYY-MM-DD)
 * @return array Result with keys: 'price', 'base_price', 'discount_percent', 'applied_pricelist_id', 'rule_id'
 */
function compute_pricelist_price($stock_id, $quantity, $customer_id = 0, $branch_code = 0, 
	$sales_type = 0, $currency = '', $date = null) {
	
	if (!$date) {
		$date = date('Y-m-d');
	}
	
	// Initialize result with default (list price)
	$result = array(
		'price' => 0,
		'base_price' => 0,
		'discount_percent' => 0,
		'applied_pricelist_id' => 0,
		'rule_id' => 0,
		'source' => 'standard'  // 'pricelist' or 'standard'
	);
	
	// Get standard price first (fallback)
	$standard_price = get_price($stock_id, $quantity, $sales_type, $currency, $customer_id);
	$result['base_price'] = $standard_price;
	$result['price'] = $standard_price;
	
	// Check if advanced pricelists are enabled
	if (!get_company_pref('use_advanced_pricelists')) {
		return $result;
	}
	
	// Get applicable pricelists
	$pricelists = get_applicable_pricelists($customer_id, $branch_code, $sales_type, $currency, $date);
	
	if (empty($pricelists)) {
		return $result;
	}
	
	// Find the best matching rule
	$best_price = $standard_price;
	$best_rule = null;
	$best_pricelist = null;
	
	foreach ($pricelists as $pricelist) {
		$rules = get_pricelist_rules($pricelist['id'], false);
		
		foreach ($rules as $rule) {
			// Check if rule applies to this item
			if (!empty($rule['stock_id']) && $rule['stock_id'] != $stock_id) {
				continue;
			}
			
			// Check if quantity matches rule
			if ($quantity < $rule['min_quantity']) {
				continue;
			}
			
			// Check if rule date range is valid
			if (!is_null($rule['date_start'])) {
				if (strtotime($date) < strtotime($rule['date_start'])) {
					continue;
				}
			}
			if (!is_null($rule['date_end'])) {
				if (strtotime($date) > strtotime($rule['date_end'])) {
					continue;
				}
			}
			
			// Calculate price based on rule
			$computed_price = $standard_price;
			
			// Determine base price
			if ($rule['base_price_type'] == 'cost') {
				$computed_price = get_standard_cost($stock_id);
			} else if ($rule['base_price_type'] == 'other_pricelist' && $rule['base_pricelist_id'] > 0) {
				// Recursively get price from base pricelist
				$base_result = compute_pricelist_price_from_pricelist($stock_id, $quantity, $rule['base_pricelist_id']);
				$computed_price = $base_result['price'];
			}
			// else: use list_price (standard_price)
			
			// Apply computation
			if ($rule['computation_type'] == 'fixed') {
				$computed_price = $rule['fixed_price'];
			} else if ($rule['computation_type'] == 'percentage') {
				$computed_price = $computed_price * (1 - $rule['percentage'] / 100);
			} else if ($rule['computation_type'] == 'formula') {
				// Could be extended for custom formula support
				$computed_price = $computed_price;
			}
			
			// Add surcharge
			$computed_price += $rule['surcharge'];
			
			// Apply rounding
			if ($rule['rounding'] > 0) {
				$computed_price = round($computed_price / $rule['rounding']) * $rule['rounding'];
			}
			
			// Keep best price (lowest)
			if ($computed_price < $best_price) {
				$best_price = $computed_price;
				$best_rule = $rule;
				$best_pricelist = $pricelist;
			}
		}
	}
	
	// If a better price was found, update result
	if ($best_rule && $best_pricelist) {
		$result['price'] = $best_price;
		$result['applied_pricelist_id'] = $best_pricelist['id'];
		$result['rule_id'] = $best_rule['id'];
		$result['source'] = 'pricelist';
		$result['discount_percent'] = round(($standard_price - $best_price) / $standard_price * 100, 2);
	}
	
	// Fire hook to allow extensions to override/adjust price
	hook_invoke_all('sales_price_compute', $result, $stock_id, $customer_id, array(
		'quantity' => $quantity,
		'branch_code' => $branch_code,
		'sales_type' => $sales_type,
		'currency' => $currency,
		'date' => $date
	));
	
	hook_invoke_all('sales_price_override', $result['price'], $stock_id, $customer_id, array(
		'quantity' => $quantity,
		'branch_code' => $branch_code,
		'sales_type' => $sales_type,
		'currency' => $currency,
		'date' => $date
	));
	
	return $result;
}

/**
 * Helper function: Compute price from a specific pricelist ID.
 * Used when a rule references another pricelist as its base.
 * 
 * @param string $stock_id Item code
 * @param double $quantity Quantity
 * @param int $pricelist_id Pricelist ID to use as base
 * @return array Price result array
 */
function compute_pricelist_price_from_pricelist($stock_id, $quantity, $pricelist_id) {
	// Get rules for this specific pricelist
	$rules = get_pricelist_rules($pricelist_id, false);
	$best_price = get_price($stock_id, $quantity);
	
	foreach ($rules as $rule) {
		if (!empty($rule['stock_id']) && $rule['stock_id'] != $stock_id) {
			continue;
		}
		if ($quantity < $rule['min_quantity']) {
			continue;
		}
		
		$computed_price = get_price($stock_id, $quantity);
		
		if ($rule['computation_type'] == 'fixed') {
			$computed_price = $rule['fixed_price'];
		} else if ($rule['computation_type'] == 'percentage') {
			$computed_price = $computed_price * (1 - $rule['percentage'] / 100);
		}
		
		$computed_price += $rule['surcharge'];
		
		if ($rule['rounding'] > 0) {
			$computed_price = round($computed_price / $rule['rounding']) * $rule['rounding'];
		}
		
		if ($computed_price < $best_price) {
			$best_price = $computed_price;
		}
	}
	
	return array(
		'price' => $best_price,
		'source' => 'pricelist',
		'pricelist_id' => $pricelist_id
	);
}
