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
 * Entity class for item_units (Units of Measure).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove(), ::upsert().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * PK: abbr (varchar) — natural key.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class item_units_entity extends Entity {
	function __construct() {
		parent::__construct(
			'item_units',
			array('abbr', 'name', 'decimals', 'inactive'),
			'abbr'
		);
	}

	/**
	 * Upsert — insert if not exists, update if exists.
	 * Matches legacy write_item_unit() behavior.
	 *
	 * @param string $key    PK value (abbr), empty for insert
	 * @param array  $data   Assoc array with 'abbr', 'name', 'decimals'
	 * @return bool
	 */
	public static function upsert($key, $data) {
		$e = new static();
		if ($key !== '' && $key !== null && $e->get($key)) {
			return $e->update($key, $data);
		} else {
			return $e->insert($data);
		}
	}
}
