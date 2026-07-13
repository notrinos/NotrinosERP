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
 * Entity class for item tax types.
 *
 * Multi-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * ::create(), ::modify(), and ::remove() override the base to handle
 * item_tax_type_exemptions child table in transactions.
 *
 * ::is_inactive($id) — checks whether a given item tax type is inactive.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class item_tax_types_entity extends Entity {
	function __construct() {
		parent::__construct(
			'item_tax_types',
			array('name', 'exempt', 'id', 'inactive'),
			'id'
		);
	}

	/**
	 * Add exemption records for an item tax type.
	 *
	 * @param int   $id         Item tax type ID
	 * @param array $exemptions Array of tax type IDs to exempt from
	 */
	private function _add_exemptions($id, $exemptions) {
		for ($i = 0; $i < count($exemptions); $i++) {
			$sql = "INSERT INTO ".TB_PREF."item_tax_type_exemptions (item_tax_type_id, tax_type_id)
				VALUES (".db_escape($id).",  ".db_escape($exemptions[$i]).")";
			db_query($sql, 'could not add item tax type exemptions');
		}
	}

	/**
	 * Delete all exemption records for an item tax type.
	 *
	 * @param int $id Item tax type ID
	 */
	private function _delete_exemptions($id) {
		$sql = "DELETE FROM ".TB_PREF."item_tax_type_exemptions WHERE item_tax_type_id=".db_escape($id);
		db_query($sql, 'could not delete item tax type exemptions');
	}

	/**
	 * Create an item tax type with its exemptions in a transaction.
	 *
	 * @param array $data Must contain 'name', 'exempt', and 'exempt_from' (array of tax_type_ids)
	 * @return bool True on success
	 */
	public static function create($data) {
		$e = new static();
		begin_transaction();
		$result = $e->insert(array('name' => $data['name'], 'exempt' => $data['exempt']));
		if ($result && !empty($data['exempt_from'])) {
			$id = db_insert_id();
			$e->_add_exemptions($id, $data['exempt_from']);
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}

	/**
	 * Update an item tax type and its exemptions in a transaction.
	 *
	 * @param mixed $key  Item tax type ID
	 * @param array $data Must contain 'name', 'exempt', and 'exempt_from'
	 * @return bool True on success
	 */
	public static function modify($key, $data) {
		$e = new static();
		begin_transaction();
		$result = $e->update($key, array('name' => $data['name'], 'exempt' => $data['exempt']));
		if ($result && isset($data['exempt_from'])) {
			$e->_delete_exemptions($key);
			$e->_add_exemptions($key, $data['exempt_from']);
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}

	/**
	 * Delete an item tax type and its exemptions in a transaction.
	 *
	 * @param mixed $key Item tax type ID
	 * @return bool True on success
	 */
	public static function remove($key) {
		$e = new static();
		begin_transaction();
		$result = $e->delete($key);
		if ($result) {
			$e->_delete_exemptions($key);
		}
		if ($result)
			commit_transaction();
		else
			rollback_transaction();
		return $result;
	}

	/**
	 * Check whether a given item tax type is inactive.
	 *
	 * @param mixed $id Item tax type ID
	 * @return bool True if inactive
	 */
	public static function is_inactive($id) {
		$row = static::find($id);
		return $row ? (bool)$row['inactive'] : false;
	}
}
