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
 * Entity class for customer/supplier groups (shared `groups` table).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class sales_group_entity extends Entity {
	function __construct() {
		// Validators
		$this->set_validator('description:iu:required', _("The group description cannot be empty."));

		// Constructor
		parent::__construct(
			'groups',
			array('description', 'id', 'inactive'),
			'id'
		);
	}

	/**
	 * Get group name by ID. Returns empty string if not found.
	 *
	 * @param int $group_no
	 * @return string
	 */
	public static function name($group_no) {
		$row = static::find((int)$group_no);
		return $row ? $row['description'] : '';
	}
}
