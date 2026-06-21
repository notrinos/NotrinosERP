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
 * Tax Groups helper functions.
 *
 * Simple CRUD (get/add/update/delete) is provided by the tax_group_entity class.
 * This file retains only cross-table helper functions (get_tax_group_rates,
 * get_tax_group_items_as_array, get_shipping_tax_as_array).
 */

//
//	Return all tax types with rate value updated according to tax group selected
//	Call the function without arg to find shipment group taxes.
//
function get_tax_group_rates($group_id=null, $tax_shipping=false) {
	global $SysPrefs;

	$sql = 
	"SELECT t.id as tax_type_id,"
		.($SysPrefs->suppress_tax_rates() == 1 ? "t.name as tax_type_name,"
			: "CONCAT(t.name, ' (', t.rate, '%)') as tax_type_name,")
		."t.sales_gl_code,
		  t.purchasing_gl_code,
		  IF(g.tax_type_id, t.rate, NULL) as rate,
		  g.tax_shipping
		FROM ".TB_PREF."tax_types t 
		  LEFT JOIN ".TB_PREF."tax_group_items g ON t.id=g.tax_type_id
	AND g.tax_group_id=". ($group_id ? db_escape($group_id) : "(SELECT MIN(id) FROM ".TB_PREF."tax_groups)")		  
	. " WHERE !t.inactive";
	if ($tax_shipping)
		$sql .= " AND g.tax_shipping=1";

	return db_query($sql, 'cannot get tax types as array');
}

function get_tax_group_items_as_array($id) {
	$ret_tax_array = array();
	
	$tax_group_items = get_tax_group_rates($id);
	
	while ($tax_group_item = db_fetch($tax_group_items)) {
		$tax_group_item['Value'] = 0;
		$ret_tax_array[$tax_group_item['tax_type_id']] = $tax_group_item;

	}
	
	return $ret_tax_array;
}

function get_shipping_tax_as_array($id=null) {
	$ret_tax_array = array();

	$tax_group_items = get_tax_group_rates($id, true);

	while ($tax_group_item = db_fetch($tax_group_items)) {
		$tax_group_item['Value'] = 0;
		$ret_tax_array[$tax_group_item['tax_type_id']] = $tax_group_item;
	}
	
	return $ret_tax_array;
}
