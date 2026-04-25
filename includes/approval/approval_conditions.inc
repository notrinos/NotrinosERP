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

/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Conditional Rules Engine

	Provides field-based conditional rules for approval routing.

	Conditions are stored as JSON in a new `conditions` column on
	approval_levels. When evaluating whether a level applies, the
	engine checks the conditions against the draft data fields.

	Supported operators: ==, !=, >, <, >=, <=, contains, not_contains,
	in, not_in, starts_with, ends_with, between, is_empty, is_not_empty

	PHP 5.6+ compatible.
***********************************************************************/

/**
 * Evaluate all conditions for a specific approval level against draft data.
 *
 * Conditions format (JSON array stored in approval_levels.conditions):
 * [
 *   {"field": "amount", "operator": ">", "value": "5000"},
 *   {"field": "person_id", "operator": "in", "value": "1,2,3"},
 *   {"field": "loc_code", "operator": "==", "value": "LOC01"}
 * ]
 *
 * All conditions must be true (AND logic) for the level to apply.
 *
 * @param string $conditions_json JSON-encoded conditions array
 * @param array  $draft_data      Draft data (decoded JSON from approval_drafts)
 * @param array  $context         Additional context fields (amount, trans_type, loc_code, etc.)
 * @return bool                    True if all conditions are met (or no conditions defined)
 */
function evaluate_approval_conditions($conditions_json, $draft_data, $context = array())
{
	if (empty($conditions_json)) {
		return true;
	}

	$decoded_conditions = decode_approval_conditions_json($conditions_json);
	$conditions = normalize_approval_conditions_array($decoded_conditions);

	if (!is_array($conditions) || empty($conditions)) {
		return true;
	}

	foreach ($conditions as $condition) {
		if (!isset($condition['field']) || !isset($condition['operator'])) {
			continue;
		}

		$field = $condition['field'];
		$operator = $condition['operator'];
		$expected = isset($condition['value']) ? $condition['value'] : '';

		// Resolve the field value from context first, then draft_data
		$actual = resolve_condition_field_value($field, $draft_data, $context);

		if (!evaluate_single_condition($actual, $operator, $expected)) {
			return false;
		}
	}

	return true;
}

/**
 * Normalize decoded conditions into a list of condition objects.
 *
 * Supported input shapes:
 * - List of condition objects: [{"field":"amount","operator":">","value":"100"}]
 * - Single condition object: {"field":"amount","operator":">","value":"100"}
 *
 * @param mixed $decoded_conditions Decoded JSON value
 * @return array|null               Normalized condition list or null for invalid shape
 */
function normalize_approval_conditions_array($decoded_conditions)
{
	if (!is_array($decoded_conditions)) {
		return null;
	}

	// Single condition object support.
	if (isset($decoded_conditions['field']) || isset($decoded_conditions['operator'])) {
		return array($decoded_conditions);
	}

	return $decoded_conditions;
}

/**
 * Resolve a field value from the draft data or context.
 *
 * Supports dot notation for nested fields: "line_items.0.stock_id"
 * Also checks top-level context fields: amount, trans_type, currency, etc.
 *
 * @param string $field      Field name (supports dot notation)
 * @param array  $draft_data Draft data array
 * @param array  $context    Additional context
 * @return mixed              Resolved field value or null
 */
function resolve_condition_field_value($field, $draft_data, $context)
{
	// Check context fields first (these take precedence)
	$context_fields = array('amount', 'trans_type', 'currency', 'loc_code',
		'person_type', 'person_id', 'reference', 'summary');

	if (in_array($field, $context_fields) && isset($context[$field])) {
		return $context[$field];
	}

	// Check draft data with dot notation support
	return resolve_dot_notation($field, $draft_data);
}

/**
 * Decode conditions JSON with fallback handling for escaped payloads.
 *
 * @param string $conditions_json Raw JSON from form input
 * @return array|null             Decoded value or null when invalid
 */
function decode_approval_conditions_json($conditions_json)
{
	$conditions_json = trim((string)$conditions_json);
	if ($conditions_json === '') {
		return array();
	}

	$decoded = json_decode($conditions_json, true);
	if (json_last_error() === JSON_ERROR_NONE) {
		return $decoded;
	}

	$unescaped = stripslashes($conditions_json);
	if ($unescaped !== $conditions_json) {
		$decoded = json_decode($unescaped, true);
		if (json_last_error() === JSON_ERROR_NONE) {
			return $decoded;
		}
	}

	$entity_decoded = html_entity_decode($conditions_json, ENT_QUOTES, 'UTF-8');
	if ($entity_decoded !== $conditions_json) {
		$decoded = json_decode($entity_decoded, true);
		if (json_last_error() === JSON_ERROR_NONE) {
			return $decoded;
		}
	}

	return null;
}

/**
 * Normalize conditions JSON into canonical array JSON for storage.
 *
 * @param string $conditions_json Raw JSON from form input
 * @return string|null            Canonical JSON or null when invalid
 */
function normalize_approval_conditions_json($conditions_json)
{
	$decoded_conditions = decode_approval_conditions_json($conditions_json);
	if ($decoded_conditions === null) {
		return null;
	}

	$conditions = normalize_approval_conditions_array($decoded_conditions);
	if (!is_array($conditions)) {
		return null;
	}

	return json_encode($conditions);
}

/**
 * Resolve a dot-notation path in a nested array.
 *
 * Example: "header.customer_id" resolves $data['header']['customer_id']
 *
 * @param string $path Dot-separated field path
 * @param array  $data Data array to search
 * @return mixed        Resolved value or null
 */
function resolve_dot_notation($path, $data)
{
	if (!is_array($data)) {
		return null;
	}

	// Simple case: direct key
	if (isset($data[$path])) {
		return $data[$path];
	}

	// Dot notation
	$parts = explode('.', $path);
	$current = $data;

	foreach ($parts as $part) {
		if (is_array($current) && isset($current[$part])) {
			$current = $current[$part];
		} else {
			return null;
		}
	}

	return $current;
}

/**
 * Evaluate a single condition.
 *
 * @param mixed  $actual   Actual value from draft
 * @param string $operator Comparison operator
 * @param string $expected Expected value from condition rule
 * @return bool             True if condition is satisfied
 */
function evaluate_single_condition($actual, $operator, $expected)
{
	switch ($operator) {
		case '==':
		case 'equals':
			return ((string)$actual === (string)$expected);

			$decoded_conditions = decode_approval_conditions_json($conditions_json);
		case 'not_equals':
			return ((string)$actual !== (string)$expected);

		case '>':
		case 'greater_than':
			return ((float)$actual > (float)$expected);

		case '<':
		case 'less_than':
			return ((float)$actual < (float)$expected);

		case '>=':
		case 'greater_or_equal':
			return ((float)$actual >= (float)$expected);

		case '<=':
		case 'less_or_equal':
			return ((float)$actual <= (float)$expected);

		case 'contains':
			return (strpos((string)$actual, (string)$expected) !== false);

		case 'not_contains':
			return (strpos((string)$actual, (string)$expected) === false);

		case 'starts_with':
			return (strpos((string)$actual, (string)$expected) === 0);

		case 'ends_with':
			$expected_str = (string)$expected;
			$actual_str = (string)$actual;
			if (strlen($expected_str) === 0) {
				return true;
			}
			return (substr($actual_str, -strlen($expected_str)) === $expected_str);

		case 'in':
			$values = array_map('trim', explode(',', (string)$expected));
			return in_array((string)$actual, $values);

		case 'not_in':
			$values = array_map('trim', explode(',', (string)$expected));
			return !in_array((string)$actual, $values);

		case 'between':
			$range = array_map('trim', explode(',', (string)$expected));
			if (count($range) !== 2) {
				return false;
			}
			$value = (float)$actual;
			return ($value >= (float)$range[0] && $value <= (float)$range[1]);

		case 'is_empty':
			return (empty($actual) || $actual === '' || $actual === null);

		case 'is_not_empty':
			return (!empty($actual) && $actual !== '' && $actual !== null);

		default:
			return true;
	}
}

/**
 * Validate a conditions JSON string.
 *
 * @param string $conditions_json JSON string to validate
 * @return array ['valid' => bool, 'errors' => array]
 */
function validate_approval_conditions($conditions_json)
{
	$errors = array();

	if (empty($conditions_json)) {
		return array('valid' => true, 'errors' => $errors);
	}

	$decoded_conditions = decode_approval_conditions_json($conditions_json);

	if ($decoded_conditions === null) {
		$errors[] = _('Invalid JSON format.');
		return array('valid' => false, 'errors' => $errors);
	}

	$conditions = normalize_approval_conditions_array($decoded_conditions);

	if (!is_array($conditions)) {
		$errors[] = _('Conditions must be a JSON object or array.');
		return array('valid' => false, 'errors' => $errors);
	}

	$valid_operators = array(
		'==', '!=', '>', '<', '>=', '<=',
		'equals', 'not_equals', 'greater_than', 'less_than',
		'greater_or_equal', 'less_or_equal',
		'contains', 'not_contains', 'starts_with', 'ends_with',
		'in', 'not_in', 'between', 'is_empty', 'is_not_empty'
	);

	foreach ($conditions as $index => $condition) {
		if (!is_array($condition)) {
			$errors[] = sprintf(_('Condition #%d must be an object.'), $index + 1);
			continue;
		}

		if (empty($condition['field'])) {
			$errors[] = sprintf(_('Condition #%d is missing the "field" property.'), $index + 1);
		}

		if (empty($condition['operator'])) {
			$errors[] = sprintf(_('Condition #%d is missing the "operator" property.'), $index + 1);
		} elseif (!in_array($condition['operator'], $valid_operators)) {
			$errors[] = sprintf(_('Condition #%d has an invalid operator: "%s".'), $index + 1, $condition['operator']);
		}
	}

	return array('valid' => empty($errors), 'errors' => $errors);
}

/**
 * Get a human-readable description of a condition.
 *
 * @param array $condition Single condition array
 * @return string           Human-readable description
 */
function format_condition_description($condition)
{
	$field = isset($condition['field']) ? $condition['field'] : '?';
	$operator = isset($condition['operator']) ? $condition['operator'] : '?';
	$value = isset($condition['value']) ? $condition['value'] : '';

	$operator_labels = array(
		'==' => '=', '!=' => '!=', '>' => '>', '<' => '<',
		'>=' => '>=', '<=' => '<=',
		'equals' => '=', 'not_equals' => '!=',
		'greater_than' => '>', 'less_than' => '<',
		'greater_or_equal' => '>=', 'less_or_equal' => '<=',
		'contains' => _('contains'), 'not_contains' => _('not contains'),
		'starts_with' => _('starts with'), 'ends_with' => _('ends with'),
		'in' => _('in'), 'not_in' => _('not in'),
		'between' => _('between'),
		'is_empty' => _('is empty'), 'is_not_empty' => _('is not empty'),
	);

	$op_label = isset($operator_labels[$operator]) ? $operator_labels[$operator] : $operator;

	if ($operator === 'is_empty' || $operator === 'is_not_empty') {
		return sprintf('%s %s', $field, $op_label);
	}

	return sprintf('%s %s %s', $field, $op_label, $value);
}

/**
 * Format all conditions for display.
 *
 * @param string $conditions_json JSON-encoded conditions
 * @return string                  Human-readable conditions summary
 */
function format_conditions_summary($conditions_json)
{
	if (empty($conditions_json)) {
		return _('No conditions');
	}

	$decoded_conditions = decode_approval_conditions_json($conditions_json);
	$conditions = normalize_approval_conditions_array($decoded_conditions);

	if (!is_array($conditions) || empty($conditions)) {
		return _('No conditions');
	}

	$descriptions = array();
	foreach ($conditions as $condition) {
		$descriptions[] = format_condition_description($condition);
	}

	return implode(' ' . _('AND') . ' ', $descriptions);
}

/**
 * Get the list of available condition operators for the UI.
 *
 * @return array Associative array of operator => label
 */
function get_condition_operators()
{
	return array(
		'=='           => _('Equals (==)'),
		'!='           => _('Not Equals (!=)'),
		'>'            => _('Greater Than (>)'),
		'<'            => _('Less Than (<)'),
		'>='           => _('Greater or Equal (>=)'),
		'<='           => _('Less or Equal (<=)'),
		'contains'     => _('Contains'),
		'not_contains' => _('Not Contains'),
		'starts_with'  => _('Starts With'),
		'ends_with'    => _('Ends With'),
		'in'           => _('In (comma-separated)'),
		'not_in'       => _('Not In (comma-separated)'),
		'between'      => _('Between (min,max)'),
		'is_empty'     => _('Is Empty'),
		'is_not_empty' => _('Is Not Empty'),
	);
}

/**
 * Get common field names that can be used in conditions.
 *
 * @return array Associative array of field_name => label
 */
function get_common_condition_fields()
{
	return array(
		'amount'     => _('Transaction Amount'),
		'currency'   => _('Currency Code'),
		'loc_code'   => _('Location Code'),
		'person_id'  => _('Person ID (Customer/Supplier)'),
		'reference'  => _('Reference'),
		'date_'      => _('Transaction Date'),
		'bank_account' => _('Bank Account'),
		'pay_type'   => _('Payment Type'),
		'stock_id'   => _('Stock Item ID'),
		'location'   => _('Warehouse Location'),
		'category'   => _('Item Category'),
		'department' => _('Department'),
	);
}
