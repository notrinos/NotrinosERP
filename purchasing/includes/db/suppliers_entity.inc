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
 * Entity class for suppliers.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * ::remove() overrides the base to also delete entity contacts (multi-table transaction).
 * ::name($supplier_id) — returns the supplier name string (convenience getter).
 * ::currency($supplier_id) — returns the supplier's currency code.
 *
 * The full supplier_details (Balance, Due, Overdue...) query is kept as
 * get_supplier_details() in suppliers_db.inc — it's a multi-table JOIN, not simple CRUD.
 * The get_suppliers_search() and get_supplier_accounts() helpers also stay standalone.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');
require_once($path_to_root . '/dimensions/includes/dimension_reference_writer_fence.inc');

class suppliers_entity extends Entity {
	function __construct() {
		parent::__construct(
			'suppliers',
			array(
				'supp_name', 'supp_ref', 'address', 'supp_address',
				'gst_no', 'contact', 'supp_account_no', 'website',
				'bank_account', 'curr_code', 'payment_terms', 'tax_included',
				'dimension_id', 'dimension2_id', 'tax_group_id', 'credit_limit',
				'purchase_account', 'payable_account', 'payment_discount_account',
				'notes', 'inactive', 'custom_data',
				'vendor_tier', 'vendor_category', 'overall_score',
				'quality_score', 'delivery_score', 'price_score', 'service_score',
				'last_evaluation_date', 'next_evaluation_date', 'evaluation_frequency_months',
				'certifications', 'approved_categories',
				'lead_time_average', 'on_time_delivery_pct', 'defect_rate_pct',
				'payment_reliability_score',
				'supplier_id'
			),
			'supplier_id'
		);
	}

	/**
	 * Override remove() to also delete entity contacts before the supplier record.
	 *
	 * @param int $supplier_id Supplier primary key
	 * @return bool
	 */
	public static function remove($key) {
		if (!dimension_writer_reference_id($key)) return false;
		return dimension_ordinary_master_write('suppliers', $key,
			array('dimension_id'=>0, 'dimension2_id'=>0), function() use ($key) {
				delete_entity_contacts('supplier', (int)$key);
				return parent::remove($key);
			});
	}

	/** Serialize direct/static inserts with the native master-reference protocol. */
	function insert($data) {
		if (!is_array($data) || (array_key_exists('supplier_id', $data)
			&& dimension_writer_reference_id($data['supplier_id']) === false)) return false;
		$data += array('dimension_id'=>0, 'dimension2_id'=>0);
		return dimension_ordinary_master_write('suppliers', null, $data, function($next) {
			return parent::insert($next);
		});
	}

	/** Retain omitted coordinates under lock; primary-key reassignment is forbidden. */
	function update($key, $data) {
		if (!dimension_writer_reference_id($key) || !is_array($data) || (array_key_exists('supplier_id', $data)
			&& (dimension_writer_reference_id($key) === false
				|| dimension_writer_reference_id($data['supplier_id']) !== dimension_writer_reference_id($key)))) return false;
		return dimension_ordinary_master_write('suppliers', $key, $data, function($next) use ($key) {
			return parent::update($key, $next);
		}, true);
	}

	/** Serialize instance deletion; static remove also owns contact cleanup. */
	function delete($key) {
		if (!dimension_writer_reference_id($key)) return false;
		return dimension_ordinary_master_write('suppliers', $key,
			array('dimension_id'=>0, 'dimension2_id'=>0), function() use ($key) {
				return parent::delete($key);
			});
	}

	/**
	 * Get the supplier's display name.
	 *
	 * @param int $supplier_id Supplier primary key
	 * @return string|null Supplier name or null if not found
	 */
	public static function name($supplier_id) {
		$sql = "SELECT supp_name FROM " . TB_PREF . "suppliers WHERE supplier_id=" . db_escape((int)$supplier_id);
		$result = db_query($sql, 'could not get supplier name');
		$row = db_fetch_row($result);
		return $row ? $row[0] : null;
	}

	/**
	 * Get the supplier's currency code.
	 * Falls back to company currency if supplier not found.
	 *
	 * @param int $supplier_id Supplier primary key
	 * @return string Currency code
	 */
	public static function currency($supplier_id) {
		$sql = "SELECT curr_code FROM " . TB_PREF . "suppliers WHERE supplier_id = " . db_escape((int)$supplier_id);
		$result = db_query($sql, "Retreive currency of supplier $supplier_id");
		$myrow = db_fetch_row($result);
		return $myrow ? $myrow[0] : get_company_currency();
	}
}
