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
 * Entity class for payment terms.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class payment_terms_entity extends Entity {
	function __construct() {
		parent::__construct(
			'payment_terms',
			array('terms', 'days_before_due', 'day_in_following_month', 'terms_indicator', 'inactive'),
			'terms_indicator'
		);
	}

	/**
	 * Fetch a single payment term with computed cash_sale column,
	 * matching the legacy get_payment_terms() behavior.
	 *
	 * @param mixed $key Primary key value
	 * @return array|null
	 */
	public static function find($key=null) {
		if ($key === null) return parent::find();
		$sql = "SELECT *, (days_before_due=0) AND (day_in_following_month=0) AS cash_sale
			FROM ".TB_PREF."payment_terms
			WHERE terms_indicator=".db_escape($key);
		$result = db_query($sql, 'could not get payment term');
		return db_fetch($result);
	}
}
