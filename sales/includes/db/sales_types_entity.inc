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
 * Entity class for sales types.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Note: delete_sales_type() also deletes from prices table. The static ::remove() is a simple
 * single-table delete — use the existing delete_sales_type() function for full multi-table cleanup.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class sales_types_entity extends Entity {
	function __construct() {
		parent::__construct(
			'sales_types',
			array('sales_type', 'tax_included', 'factor', 'id', 'inactive'),
			'id'
		);
	}
}
