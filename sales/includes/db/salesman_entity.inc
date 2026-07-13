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
 * Entity class for salesman.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * PK: salesman_code (int) — auto-increment.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class salesman_entity extends Entity {
	function __construct() {
		parent::__construct(
			'salesman',
			array('salesman_code', 'salesman_name', 'salesman_phone', 'salesman_fax',
				'salesman_email', 'provision', 'break_pt', 'provision2', 'inactive', 'custom_data'),
			'salesman_code'
		);
	}

	/**
	 * Get the name for a salesman by ID.
	 * Replaces legacy get_salesman_name().
	 *
	 * @param int $id
	 * @return string|null
	 */
	public static function get_name($id) {
		$row = self::find($id);
		return $row ? $row['salesman_name'] : null;
	}
}
