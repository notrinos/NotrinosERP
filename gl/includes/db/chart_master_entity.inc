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
 * Entity class for chart_master (GL accounts / chart of accounts).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Non-CRUD helpers (is_account_balancesheet, gl_account_in_*, is_subledger_account, etc.)
 * remain standalone in gl_db_accounts.inc.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class chart_master_entity extends Entity {
	function __construct() {
		parent::__construct(
			'chart_master',
			array('account_code', 'account_code2', 'account_name', 'account_type', 'inactive', 'custom_data'),
			'account_code'
		);
	}

	/**
	 * Fetch all GL accounts with joined account type name, matching legacy get_gl_accounts().
	 *
	 * @param string|null $from  Lower bound account_code (inclusive)
	 * @param string|null $to    Upper bound account_code (inclusive)
	 * @param string|null $type  Account type filter
	 * @return array
	 */
	public static function all_with_types($from=null, $to=null, $type=null) {
		$sql = "SELECT coa.*, act_type.name AS AccountTypeName
			FROM " . TB_PREF . "chart_master coa
			LEFT JOIN " . TB_PREF . "chart_types act_type ON coa.account_type = act_type.id
			WHERE 1=1";
		if ($from !== null)
			$sql .= " AND coa.account_code >= " . db_escape($from);
		if ($to !== null)
			$sql .= " AND coa.account_code <= " . db_escape($to);
		if ($type !== null)
			$sql .= " AND coa.account_type = " . db_escape($type);
		$sql .= " ORDER BY account_code";

		$result = db_query($sql, 'could not get gl accounts');
		$rows = array();
		while ($row = db_fetch($result)) {
			$rows[] = $row;
		}
		return $rows;
	}
}
