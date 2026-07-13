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
 * Entity class for tax groups.
 *
 * Multi-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * ::create(), ::modify(), and ::remove() override the base to handle tax_group_items
 * child table in transactions.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class tax_group_entity extends Entity {
	function __construct() {
		parent::__construct(
			'tax_groups',
			array('name', 'id', 'inactive'),
			'id'
		);
	}

	/**
	 * Add tax group items (child records).
	 *
	 * @param int   $id            Tax group ID
	 * @param array $items         Array of tax type IDs
	 * @param array $tax_shippings Array of shipping flags
	 */
	private function _add_items($id, $items, $tax_shippings) {
		for ($i = 0; $i < count($items); $i++) {
			$sql = "INSERT INTO ".TB_PREF."tax_group_items (tax_group_id, tax_type_id, tax_shipping)
				VALUES (".db_escape($id).",  ".db_escape($items[$i]).", " . $tax_shippings[$i] .")";
			db_query($sql, 'could not add item tax group item');
		}
	}

	/**
	 * Delete all tax group items for a given tax group.
	 *
	 * @param int $id Tax group ID
	 */
	private function _delete_items($id) {
		$sql = "DELETE FROM ".TB_PREF."tax_group_items WHERE tax_group_id=".db_escape($id);
		db_query($sql, 'could not delete item tax group items');
	}

	/**
	 * Create a tax group with its child items in a transaction.
	 *
	 * @param array $data Must contain 'name', 'taxes' (array of tax_type_ids),
	 *                    and 'tax_shippings' (array of shipping flags)
	 * @return bool True on success
	 */
	public static function create($data) {
		$e = new static();
		begin_transaction();
		$result = $e->insert(array('name' => $data['name']));
		if ($result && !empty($data['taxes'])) {
			$id = db_insert_id();
			$e->_add_items($id, $data['taxes'], $data['tax_shippings']);
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}

	/**
	 * Update a tax group and its child items in a transaction.
	 *
	 * @param mixed $key  Tax group ID
	 * @param array $data Must contain 'name', 'taxes', and 'tax_shippings'
	 * @return bool True on success
	 */
	public static function modify($key, $data) {
		$e = new static();
		begin_transaction();
		$result = $e->update($key, array('name' => $data['name']));
		if ($result && isset($data['taxes'])) {
			$e->_delete_items($key);
			$e->_add_items($key, $data['taxes'], $data['tax_shippings']);
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}

	/**
	 * Delete a tax group and its child items in a transaction.
	 *
	 * @param mixed $key Tax group ID
	 * @return bool True on success
	 */
	public static function remove($key) {
		$e = new static();
		begin_transaction();
		$result = $e->delete($key);
		if ($result) {
			$e->_delete_items($key);
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}
}
