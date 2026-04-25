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

include_once($path_to_root . '/inventory/warehouse/includes/replenishment_engine.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_replenishment_db.inc');
include_once($path_to_root . '/purchasing/includes/db/vendor_pricelist_db.inc');

/**
 * Resolve the effective supplier id for one replenishment suggestion.
 *
 * @param array $suggestion
 * @return int
 */
function resolve_reorder_suggestion_supplier_id($suggestion)
{
	$supplier_id = 0;

	if (isset($suggestion['preferred_supplier']) && (int)$suggestion['preferred_supplier'] > 0)
		$supplier_id = (int)$suggestion['preferred_supplier'];

	if ($supplier_id <= 0) {
		$best_price = get_best_vendor_price(
			$suggestion['stock_id'],
			(float)$suggestion['suggested_qty'],
			'',
			date2sql(Today())
		);
		if (!empty($best_price['supplier_id']))
			$supplier_id = (int)$best_price['supplier_id'];
	}

	return $supplier_id;
}

/**
 * Get replenishment rules with purchasing-focused fields.
 *
 * @param string $stock_id
 * @param string $location
 * @param int    $supplier_id
 * @param int    $show_inactive
 * @return resource
 */
function get_purchasing_reorder_rules($stock_id = '', $location = '', $supplier_id = 0, $show_inactive = 0)
{
	$sql = "SELECT r.*,\n\	\tlocation.location_name AS warehouse_name,\n\	\tsupplier.supp_name AS supplier_name,\n\	\tCOALESCE(NULLIF(r.preferred_supplier_id, 0), r.preferred_supplier, 0) AS effective_supplier_id\n\	FROM " . TB_PREF . "wh_replenishment_rules r\n\	LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = r.warehouse_loc_code\n\	LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = COALESCE(NULLIF(r.preferred_supplier_id, 0), r.preferred_supplier, 0)\n\	WHERE 1=1";

	if ($stock_id !== '' && $stock_id !== ALL_TEXT)
		$sql .= " AND r.stock_id = " . db_escape($stock_id);

	if ($location !== '' && $location !== ALL_TEXT)
		$sql .= " AND r.warehouse_loc_code = " . db_escape($location);

	if ((int)$supplier_id > 0)
		$sql .= " AND COALESCE(NULLIF(r.preferred_supplier_id, 0), r.preferred_supplier, 0) = " . db_escape((int)$supplier_id);

	if (!(int)$show_inactive)
		$sql .= " AND r.active = 1";

	$sql .= " ORDER BY r.rule_name";
	return db_query($sql, 'could not retrieve purchasing reorder rules');
}

/**
 * Update the preferred supplier and purchasing automation flags on one rule.
 *
 * @param int $rule_id
 * @param int $supplier_id
 * @param int $auto_create_rfq
 * @param int $auto_create_po
 * @param int $lead_time_days
 * @return bool
 */
function set_preferred_supplier_for_rule($rule_id, $supplier_id, $auto_create_rfq = 0, $auto_create_po = 0, $lead_time_days = 0)
{
	$sql = "UPDATE " . TB_PREF . "wh_replenishment_rules SET\n\	\tpreferred_supplier_id = " . db_escape((int)$supplier_id) . ",\n\	\tpreferred_supplier = " . db_escape((int)$supplier_id) . ",\n\	\tauto_create_rfq = " . db_escape((int)$auto_create_rfq) . ",\n\	\tauto_create_po = " . db_escape((int)$auto_create_po) . ",\n\	\tlead_time_days = " . db_escape((int)$lead_time_days) . "\n\	WHERE rule_id = " . (int)$rule_id;

	db_query($sql, 'could not update purchasing reorder rule overlay');
	return true;
}

/**
 * Group current reorder suggestions by supplier for procurement batching.
 *
 * @param string $location
 * @return array
 */
function get_items_needing_reorder_by_supplier($location = '')
{
	$suggestions = evaluate_replenishment_rules($location !== '' ? $location : null, null);
	$grouped = array();

	foreach ($suggestions as $suggestion) {
		if ((float)$suggestion['suggested_qty'] <= 0)
			continue;

		$supplier_id = resolve_reorder_suggestion_supplier_id($suggestion);
		if ($supplier_id <= 0)
			continue;

		if (!isset($grouped[$supplier_id])) {
			$grouped[$supplier_id] = array(
				'supplier_id' => $supplier_id,
				'supplier_name' => get_supplier_name($supplier_id),
				'items' => array(),
				'total_lines' => 0,
				'total_estimated' => 0,
			);
		}

		$price_context = get_best_vendor_price(
			$suggestion['stock_id'],
			(float)$suggestion['suggested_qty'],
			'',
			date2sql(Today()),
			$supplier_id
		);

		$estimated_price = isset($price_context['price']) ? (float)$price_context['price'] : 0;
		$line_estimated_total = (float)$suggestion['suggested_qty'] * $estimated_price;

		$suggestion['resolved_supplier_id'] = $supplier_id;
		$suggestion['estimated_unit_price'] = $estimated_price;
		$suggestion['estimated_total'] = $line_estimated_total;
		$grouped[$supplier_id]['items'][] = $suggestion;
		$grouped[$supplier_id]['total_lines']++;
		$grouped[$supplier_id]['total_estimated'] += $line_estimated_total;
	}

	return $grouped;
}
