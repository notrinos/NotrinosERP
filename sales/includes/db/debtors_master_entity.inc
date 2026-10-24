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
 * Entity class for debtors_master (customers).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * ::remove() overrides the base to also delete entity contacts (multi-table transaction).
 * ::name($debtor_no) — returns the customer name string (convenience getter).
 *
 * The full customer_details (Balance, Due, Overdue1, Overdue2) query is kept as
 * get_customer_details() in customers_db.inc — it's a multi-table JOIN, not simple CRUD.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');
require_once($path_to_root . '/dimensions/includes/dimension_reference_writer_fence.inc');

class debtors_master_entity extends Entity {
	function __construct() {
		parent::__construct(
			'debtors_master',
			array(
				'name', 'debtor_ref', 'address', 'tax_id',
				'curr_code', 'sales_type', 'dimension_id', 'dimension2_id',
				'credit_status', 'payment_terms', 'discount',
				'pymt_discount', 'credit_limit', 'notes', 'inactive',
				'custom_data', 'credit_insurance_limit', 'credit_review_date',
				'credit_risk_score', 'payment_behavior_score', 'customer_tier',
				'preferred_communication', 'industry', 'company_size',
				'debtor_no'
			),
			'debtor_no'
		);
	}

	/**
	 * Override remove() to also delete entity contacts and their branches before
	 * deleting the customer record itself. This matches the legacy delete_customer().
	 *
	 * @param int $debtor_no Customer primary key
	 * @return bool
	 */
	public static function remove($key) {
		if (!dimension_writer_reference_id($key)) return false;
		return dimension_ordinary_master_write('debtors_master', $key,
			array('dimension_id'=>0, 'dimension2_id'=>0), function() use ($key) {
				delete_entity_contacts('customer', (int)$key);
				return parent::remove($key);
			});
	}

	/** Serialize direct/static inserts with the native master-reference protocol. */
	function insert($data) {
		if (!is_array($data) || (array_key_exists('debtor_no', $data)
			&& dimension_writer_reference_id($data['debtor_no']) === false)) return false;
		$data += array('dimension_id'=>0, 'dimension2_id'=>0);
		return dimension_ordinary_master_write('debtors_master', null, $data, function($next) {
			return parent::insert($next);
		});
	}

	/** Retain omitted coordinates under lock; primary-key reassignment is forbidden. */
	function update($key, $data) {
		if (!dimension_writer_reference_id($key) || !is_array($data) || (array_key_exists('debtor_no', $data)
			&& (dimension_writer_reference_id($key) === false
				|| dimension_writer_reference_id($data['debtor_no']) !== dimension_writer_reference_id($key)))) return false;
		return dimension_ordinary_master_write('debtors_master', $key, $data, function($next) use ($key) {
			return parent::update($key, $next);
		}, true);
	}

	/** Serialize instance deletion; static remove also owns contact cleanup. */
	function delete($key) {
		if (!dimension_writer_reference_id($key)) return false;
		return dimension_ordinary_master_write('debtors_master', $key,
			array('dimension_id'=>0, 'dimension2_id'=>0), function() use ($key) {
				return parent::delete($key);
			});
	}

	/**
	 * Get the customer's display name.
	 *
	 * @param int $debtor_no Customer primary key
	 * @return string|null Customer name or null if not found
	 */
	public static function name($debtor_no) {
		$sql = "SELECT name FROM " . TB_PREF . "debtors_master WHERE debtor_no=" . db_escape((int)$debtor_no);
		$result = db_query($sql, 'could not get customer name');
		$row = db_fetch_row($result);
		return $row ? $row[0] : null;
	}
}
