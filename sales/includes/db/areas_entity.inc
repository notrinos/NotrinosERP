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
 * Entity class for areas.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * PK: area_code (int) — auto-increment.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class areas_entity extends Entity {
	function __construct() {
		parent::__construct(
			'areas',
			array('area_code', 'description', 'inactive', 'custom_data'),
			'area_code'
		);
	}

	/**
	 * Get the descriptive name for an area.
	 * Replaces legacy get_area_name().
	 *
	 * @param int $id
	 * @return string|null
	 */
	public static function get_name($id) {
		$row = self::find($id);
		return $row ? $row['description'] : null;
	}
}
