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
 * Entity class for tax types.
 *
 * Multi-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * For JOIN queries with chart_master account names, use:
 *   ::find_with_accounts($id)  — returns single record with SalesAccountName, PurchasingAccountName
 *   ::all_with_accounts($where) — returns DB resource with JOINed account names
 *
 * ::remove() overrides the base to also delete from item_tax_type_exemptions (multi-table transaction).
 * ::rate($id) — returns the tax rate for a given tax type id (simple field getter).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class tax_types_entity extends Entity {
	function __construct() {
		parent::__construct(
			'tax_types',
			array('name', 'sales_gl_code', 'purchasing_gl_code', 'rate', 'id', 'inactive', 'custom_data'),
			'id'
		);
	}

	/**
	 * Fetch a single tax type WITH account names (JOIN chart_master).
	 * Returns assoc array with SalesAccountName and PurchasingAccountName.
	 *
	 * @param mixed $id Tax type primary key
	 * @return array|null Associative array or null if not found
	 */
	public static function find_with_accounts($id) {
		$sql = "SELECT tax_type.*,
				Chart1.account_name AS SalesAccountName,
				Chart2.account_name AS PurchasingAccountName
			FROM ".TB_PREF."tax_types tax_type LEFT JOIN "
				.TB_PREF."chart_master AS Chart1 ON tax_type.sales_gl_code = Chart1.account_code LEFT JOIN "
				.TB_PREF."chart_master AS Chart2 ON tax_type.purchasing_gl_code = Chart2.account_code 
			WHERE tax_type.id=".db_escape($id);

		$result = db_query($sql, 'could not get tax type');
		return db_fetch($result);
	}

	/**
	 * Fetch all tax types WITH account names (JOIN chart_master).
	 * Returns a DB resource — use while(db_fetch()) to iterate.
	 *
	 * @param bool $all If true, include inactive; if false, exclude inactive
	 * @return mixed DB result set
	 */
	public static function all_with_accounts($all=false) {
		$sql = "SELECT tax_type.*,
				Chart1.account_name AS SalesAccountName,
				Chart2.account_name AS PurchasingAccountName
			FROM ".TB_PREF."tax_types tax_type LEFT JOIN "
				.TB_PREF."chart_master AS Chart1 ON tax_type.sales_gl_code = Chart1.account_code LEFT JOIN "
				.TB_PREF."chart_master AS Chart2 ON tax_type.purchasing_gl_code = Chart2.account_code";

		if (!$all)
			$sql .= " WHERE !tax_type.inactive";
		return db_query($sql, 'could not get all tax types');
	}

	/**
	 * Fetch all active tax types (simple query, no JOINs).
	 * Returns a DB resource.
	 *
	 * @return mixed DB result set
	 */
	public static function all_simple() {
		$e = new static();
		return $e->get_all('!inactive');
	}

	/**
	 * Get the tax rate for a given tax type id.
	 *
	 * @param mixed $id Tax type primary key
	 * @return float Tax rate
	 */
	public static function rate($id) {
		$row = static::find($id);
		return $row ? (float)$row['rate'] : 0;
	}

	/**
	 * Delete a tax type and its associated item_tax_type_exemptions.
	 *
	 * @param mixed $key Primary key value (id)
	 * @return bool True on success
	 */
	public static function remove($key) {
		$e = new static();
		begin_transaction();
		$result = $e->delete($key);
		if ($result) {
			// Also delete any item tax exemptions associated with this type
			$sql = "DELETE FROM ".TB_PREF."item_tax_type_exemptions WHERE tax_type_id=".db_escape($key);
			db_query($sql, 'could not delete item tax type exemptions');
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}
}
