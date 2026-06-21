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
 * Tax Types helper functions.
 *
 * Simple CRUD (get/add/update/delete) is provided by the tax_types_entity class.
 * This file retains only cross-table helper functions (is_tax_gl_unique, is_tax_account).
 */

/*
	Check if gl_code is used by more than 2 tax types,
	or check if the two gl codes are not used by any other 
	than selected tax type.
	Necessary for pre-2.2 installations.
*/
function is_tax_gl_unique($gl_code, $gl_code2=-1, $selected_id=-1) {

	$purch_code = $gl_code2== -1 ? $gl_code : $gl_code2;

	$sql = "SELECT count(*) FROM "
			.TB_PREF."tax_types	
		WHERE (sales_gl_code=" .db_escape($gl_code)
			." OR purchasing_gl_code=" .db_escape($purch_code). ")";

	if ($selected_id != -1)
		$sql .= " AND id!=".db_escape($selected_id);

	$res = db_query($sql, 'could not query gl account uniqueness');
	$row = db_fetch($res);

	return $gl_code2 == -1 ? ($row[0] <= 1) : ($row[0] == 0);
}

function is_tax_account($account_code) {
	$sql = "SELECT id FROM ".TB_PREF."tax_types WHERE 
		sales_gl_code=".db_escape($account_code)." OR purchasing_gl_code=".db_escape($account_code);
	$result = db_query($sql, 'checking account is tax account');
	if (db_num_rows($result) > 0) {
		$acct = db_fetch($result);
		return $acct['id'];
	}
	
	return false;
}
