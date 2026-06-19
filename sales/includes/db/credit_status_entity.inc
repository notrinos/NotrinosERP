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
 * Entity class for credit status.
 *
 * Extends the Entity base class to provide PK-agnostic CRUD with built-in validation.
 * Wrapper functions in credit_status_db.inc delegate to this class.
 */

include_once($path_to_root . '/includes/db/class.entity.inc');

class credit_status_entity extends Entity {

	function __construct() {
		// Validators
		$this->set_validator('reason_description:iu:required', _("The credit status description cannot be empty."));

		// Constructor
		parent::__construct('credit_status', 
			array('reason_description', 'dissallow_invoices', 'id', 'inactive'),
			'id');
	}
}
