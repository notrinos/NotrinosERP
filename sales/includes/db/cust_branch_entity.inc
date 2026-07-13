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
 * Entity class for cust_branch.
 *
 * Composite PK (branch_code, debtor_no) entity with PK-agnostic CRUD.
 * Static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class cust_branch_entity extends Entity {
	function __construct() {
		parent::__construct(
			'cust_branch',
			array(
				'branch_code', 'debtor_no', 'br_name', 'branch_ref', 'br_address',
				'area', 'salesman', 'default_location', 'tax_group_id',
				'sales_account', 'sales_discount_account', 'receivables_account',
				'payment_discount_account', 'default_ship_via', 'br_post_address',
				'group_no', 'notes', 'bank_account', 'inactive'
			),
			array('branch_code', 'debtor_no')
		);
	}

	/**
	 * Override remove() to also delete entity contacts before the branch record.
	 */
	public static function remove($key) {
		if (isset($key['branch_code'])) {
			delete_entity_contacts('cust_branch', $key['branch_code']);
		}
		return parent::remove($key);
	}
}
