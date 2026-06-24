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
 * Entity class for document_types (HRM).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class doc_types_entity extends Entity {
	function __construct() {
		parent::__construct(
			'document_types',
			array('type_name', 'notify_before', 'is_required', 'inactive', 'doc_type_id'),
			'doc_type_id'
		);
	}
}
