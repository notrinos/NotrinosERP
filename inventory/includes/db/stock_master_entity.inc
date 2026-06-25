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
 * Entity class for stock_master (inventory items).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Note: add_item() and delete_item() in items_db.inc handle multi-table operations
 * (loc_stock, purch_data, prices, bom, item_codes). These stay standalone.
 * This entity provides simple single-table CRUD for the stock_master table.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class stock_master_entity extends Entity {
	function __construct() {
		parent::__construct(
			'stock_master',
			array(
				'stock_id', 'category_id', 'tax_type_id', 'description', 'long_description',
				'units', 'mb_flag', 'sales_account', 'cogs_account', 'inventory_account',
				'adjustment_account', 'wip_account', 'dimension_id', 'dimension2_id',
				'purchase_cost', 'material_cost', 'labour_cost', 'overhead_cost',
				'inactive', 'no_sale', 'no_purchase', 'editable',
				'depreciation_method', 'depreciation_rate', 'depreciation_factor',
				'depreciation_start', 'depreciation_date', 'fa_class_id', 'custom_data',
				'track_by', 'has_serial_no', 'has_batch_no', 'has_expiry_date',
				'serial_no_prefix', 'batch_no_prefix', 'warranty_days',
				'quality_inspection_required', 'min_order_qty', 'lead_time_days',
				'shelf_life_days', 'item_weight', 'item_volume',
				'putaway_rule_id', 'removal_strategy', 'default_bin_id',
				'storage_category_id', 'abc_class', 'drop_ship_eligible', 'cross_dock_eligible'
			),
			'stock_id'
		);
	}

	/**
	 * Find item with joined tax type name, matching legacy get_item() behavior.
	 *
	 * @param mixed $key Stock ID (varchar PK)
	 * @return array|null
	 */
	public static function find($key=null) {
		if ($key === null) return parent::find();
		$sql = "SELECT item.*, taxtype.name AS tax_type_name
			FROM " . TB_PREF . "stock_master item
			LEFT JOIN " . TB_PREF . "item_tax_types taxtype ON taxtype.id = item.tax_type_id
			WHERE stock_id = " . db_escape($key);
		$result = db_query($sql, 'an item could not be retreived');
		return db_fetch_assoc($result);
	}
}
