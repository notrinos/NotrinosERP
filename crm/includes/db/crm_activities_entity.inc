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
 * Entity class for crm_activities.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Complex JOIN queries (get_crm_activities with type/user joins) remain in crm_activities_db.inc
 * as non-CRUD helpers.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class crm_activities_entity extends Entity {
	function __construct() {
		parent::__construct(
			'crm_activities',
			array('entity_type', 'entity_id', 'activity_type_id',
				'summary', 'description', 'date_scheduled', 'date_completed',
				'assigned_to', 'created_by', 'status', 'outcome',
				'duration_minutes', 'next_activity_id', 'id', 'priority'),
			'id'
		);
	}
}
