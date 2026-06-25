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
 * Entity class for crm_sales_stages.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class crm_sales_stages_entity extends Entity {
	function __construct() {
		parent::__construct(
			'crm_sales_stages',
			array('name', 'sequence', 'probability', 'description', 'active', 'id'),
			'id'
		);
	}
}
