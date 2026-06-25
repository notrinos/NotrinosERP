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
 * Entity class for crm_contracts.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Complex JOIN queries (get_crm_contracts with customer name join) and renewal logic
 * remain in crm_contracts_db.inc as non-CRUD helpers.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class crm_contracts_entity extends Entity {
	function __construct() {
		parent::__construct(
			'crm_contracts',
			array('contract_ref', 'customer_id', 'lead_id', 'title', 'description',
				'contract_value', 'start_date', 'end_date', 'renewal_alert_days',
				'status', 'signed_by', 'signed_date', 'created_by', 'created_date', 'id'),
			'id'
		);
	}
}
