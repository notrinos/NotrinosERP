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
 * Entity class for crm_appointments.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Complex JOIN queries (get_crm_appointments, get_upcoming_appointments with
 * type/lead/customer joins) remain in crm_appointments_db.inc as non-CRUD helpers.
 * Appointment type CRUD has been migrated to crm_appointment_types_entity.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class crm_appointments_entity extends Entity {
	function __construct() {
		parent::__construct(
			'crm_appointments',
			array('title', 'appointment_type_id', 'lead_id', 'customer_id', 'contact_person_id',
				'date_time', 'duration_minutes', 'location', 'video_link',
				'attendees_json', 'notes', 'status', 'reminder_sent', 'created_by', 'id'),
			'id'
		);
	}
}
