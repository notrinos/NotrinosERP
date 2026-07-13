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
 * Entity class for item_codes.
 *
 * PK-agnostic CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * Special: add_item_code() and update_item_code() have complex non-standard signatures
 * (conditional WHERE, foreign flag, etc.). The static methods provide clean data-array APIs
 * while wrapper functions preserve backward compatibility during migration.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class item_codes_entity extends Entity {
	function __construct() {
		parent::__construct(
			'item_codes',
			array('id', 'item_code', 'stock_id', 'description', 'category_id', 'quantity', 'is_foreign', 'inactive'),
			'id'
		);
	}

	/**
	 * Update an item code.
	 *
	 * When $id == -1, the record is looked up by (item_code, stock_id) composite
	 * instead of the PK. This supports the items table editor where the item code
	 * PK is not known at call time.
	 *
	 * @param int    $id          Primary key, or -1 to look up by composite key
	 * @param string $item_code   EAN/UPC code
	 * @param string $stock_id    Stock item ID
	 * @param string $description Description
	 * @param int    $category    Category ID
	 * @param float  $qty         Quantity
	 * @param int    $foreign     Is foreign code flag (1=foreign, 0=kit)
	 * @return bool
	 */
	public static function update_item_code($id, $item_code, $stock_id, $description, $category, $qty, $foreign = 0) {
		if ($id == -1) {
			// Unknown PK — look up existing record by (item_code, stock_id)
			$existing = (new static())->get_all(
				"item_code = " . db_escape($item_code) .
				" AND stock_id = " . db_escape($stock_id)
			);
			$row = db_fetch($existing);
			if (!$row) {
				// Not found — insert instead
				return static::create(array(
					'item_code' => $item_code,
					'stock_id' => $stock_id,
					'description' => $description,
					'category_id' => $category,
					'quantity' => $qty,
					'is_foreign' => $foreign
				));
			}
			$id = $row['id'];
		}

		return static::modify($id, array(
			'item_code' => $item_code,
			'stock_id' => $stock_id,
			'description' => $description,
			'category_id' => $category,
			'quantity' => $qty,
			'is_foreign' => $foreign
		));
	}
}
