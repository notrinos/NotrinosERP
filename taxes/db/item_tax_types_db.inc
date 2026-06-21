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
 * Item Tax Types helper functions.
 *
 * Simple CRUD (get/add/update/delete) is provided by the item_tax_types_entity class.
 * This file retains only cross-table helper functions
 * (get_item_tax_type_for_item, get_item_tax_type_exemptions).
 */

function get_item_tax_type_for_item($stock_id) {
	$sql = "SELECT item_tax_type.*
		FROM ".TB_PREF."item_tax_types item_tax_type,"
			.TB_PREF."stock_master item
		WHERE item.stock_id=".db_escape($stock_id)."
		AND item_tax_type.id=item.tax_type_id";
	
	$result = db_query($sql, 'could not get item tax type');
	
	return db_fetch($result);	
}

function get_item_tax_type_exemptions($id) {
	$sql = "SELECT * FROM ".TB_PREF."item_tax_type_exemptions WHERE item_tax_type_id=".db_escape($id);
	
	return db_query($sql, 'could not get item tax type exemptions');
}
