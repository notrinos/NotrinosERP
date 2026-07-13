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
 * Entity class for purch_orders (purchase orders).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * ::all() returns a plain PHP array (foreach-ready).
 * ::all_db_resource() returns a DB resource for streaming/backward-compat with while(db_fetch()).
 *
 * SIMPLE MODE — this entity wraps ONLY the purch_orders header table. The complex
 * add_po() / update_po() / delete_po() / read_po_header() / get_supplier_details_to_order()
 * functions that interact with purch_order_details, stock_master, supp_trans, and the
 * Ref system stay as-is. This entity provides find() and all() for admin listing/display,
 * plus friendly helpers like ::reference() and ::supplier_name().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class purch_orders_entity extends Entity {
	function __construct() {
		parent::__construct(
			'purch_orders',
			array(
				'supplier_id', 'comments', 'ord_date', 'reference',
				'supp_reference', 'into_stock_location', 'delivery_address',
				'total', 'prep_amount', 'alloc', 'tax_included',
				'custom_data', 'drop_ship', 'drop_ship_so_no',
				'agreement_id', 'requisition_id', 'rfq_id',
				'order_no'
			),
			'order_no'
		);
	}

	/**
	 * Fetch a PO header with joined supplier name.
	 *
	 * @param int $order_no Purchase order number
	 * @return array|null Associative array with supp_name added, or null
	 */
	public static function find_with_supplier($order_no) {
		$sql = "SELECT po.*, supplier.supp_name
			FROM " . TB_PREF . "purch_orders po
			INNER JOIN " . TB_PREF . "suppliers supplier ON po.supplier_id = supplier.supplier_id
			WHERE po.order_no = " . db_escape((int)$order_no);
		$result = db_query($sql, 'The order cannot be retrieved');
		return db_fetch($result);
	}

	/**
	 * Fetch all PO headers with joined supplier name.
	 *
	 * @param string|null $where  Additional WHERE clause (e.g., 'supplier_id=5')
	 * @param string|null $order_by  ORDER BY clause (e.g., 'ord_date DESC')
	 * @return array
	 */
	public static function all_with_supplier($where=null, $order_by=null) {
		$sql = "SELECT po.*, supplier.supp_name
			FROM " . TB_PREF . "purch_orders po
			INNER JOIN " . TB_PREF . "suppliers supplier ON po.supplier_id = supplier.supplier_id";
		if ($where)
			$sql .= " WHERE " . $where;
		$sql .= " ORDER BY " . ($order_by ? $order_by : "po.ord_date DESC");
		$result = db_query($sql, 'The orders could not be retrieved');
		$rows = array();
		while ($row = db_fetch($result)) {
			$rows[] = $row;
		}
		return $rows;
	}

}
