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
 * Entity class for currencies.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * ::remove() overrides the base to also delete from exchange_rates (multi-table transaction).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class currencies_entity extends Entity {
	function __construct() {
		parent::__construct(
			'currencies',
			array('currency', 'curr_abrev', 'curr_symbol', 'country', 'hundreds_name', 'auto_update', 'inactive'),
			'curr_abrev'
		);
	}

	/**
	 * Delete a currency AND its exchange rates in a single transaction.
	 * Overrides Entity::remove() for multi-table support.
	 *
	 * @param string $key Currency abbreviation (primary key)
	 * @return bool
	 */
	public static function remove($key) {
		$e = new static();
		begin_transaction();
		$result = $e->delete($key);
		if ($result) {
			$sql = 'DELETE FROM '.TB_PREF.'exchange_rates WHERE curr_code='.db_escape($key);
			db_query($sql, 'could not delete exchange rates for currency '.$key);
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}
}
