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
 * Entity class for sales points (POS).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class sales_points_entity extends Entity {
	function __construct() {
		parent::__construct(
			'sales_pos',
			array('pos_name', 'pos_location', 'pos_account', 'cash_sale', 'credit_sale', 'id', 'inactive'),
			'id'
		);
	}

	/**
	 * Fetch all sales points with location and bank account names (JOINed).
	 * Returns a DB resource for backward-compat with while(db_fetch()).
	 *
	 * @param bool $show_inactive Whether to include inactive records
	 * @return mixed DB result set
	 */
	public static function all_with_details($show_inactive=false) {
		$sql = "SELECT pos.*, loc.location_name, acc.bank_account_name FROM "
			.TB_PREF."sales_pos as pos
			LEFT JOIN ".TB_PREF."locations as loc on pos.pos_location=loc.loc_code
			LEFT JOIN ".TB_PREF."bank_accounts as acc on pos.pos_account=acc.id";
		if (!$show_inactive) $sql .= " WHERE !pos.inactive";
		return db_query($sql, 'could not get all POS definitions');
	}

	/**
	 * Fetch a single sales point with location and bank account names (JOINed).
	 *
	 * @param mixed $id Primary key value
	 * @return array|null Associative array or null if not found
	 */
	public static function find_with_details($id) {
		$sql = "SELECT pos.*, loc.location_name, acc.bank_account_name FROM "
			.TB_PREF."sales_pos as pos
			LEFT JOIN ".TB_PREF."locations as loc on pos.pos_location=loc.loc_code
			LEFT JOIN ".TB_PREF."bank_accounts as acc on pos.pos_account=acc.id
			WHERE pos.id=".db_escape($id);
		$result = db_query($sql, 'could not get POS definition');
		return db_fetch($result);
	}
}
