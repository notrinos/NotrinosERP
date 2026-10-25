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
require_once __DIR__.'/../../../dimensions/includes/dimension_reference_writer_fence.inc';

/** Serialize native item defaults, retained intent and all dependent SQL. */
function update_item($stock_id, $description, $long_description, $category_id, $tax_type_id, $units, $mb_flag, $sales_account, $inventory_account, $cogs_account, $adjustment_account, $wip_account, $dimension_id, $dimension2_id, $no_sale, $editable, $no_purchase, $depreciation_method = 'D', $depreciation_rate=100, $depreciation_factor=1, $depreciation_start=null, $fa_class_id=null, $tracking=null) {
	if (dimension_writer_stock_id($stock_id) === false) return false;
	return dimension_ordinary_master_write('stock_master', $stock_id,
		array('dimension_id'=>$dimension_id, 'dimension2_id'=>$dimension2_id), function($next) use ($stock_id, $description, $long_description, $category_id, $tax_type_id, $units, $mb_flag, $sales_account, $inventory_account, $cogs_account, $adjustment_account, $wip_account, $dimension_id, $dimension2_id, $no_sale, $editable, $no_purchase, $depreciation_method, $depreciation_rate, $depreciation_factor, $depreciation_start, $fa_class_id, $tracking) {
		$dimension_id = $next['dimension_id']; $dimension2_id = $next['dimension2_id'];
		$sql = "UPDATE ".TB_PREF."stock_master SET long_description=".db_escape($long_description).",
			description=".db_escape($description).",
			category_id=".db_escape($category_id).",
			sales_account=".db_escape($sales_account).",
			inventory_account=".db_escape($inventory_account).",
			cogs_account=".db_escape($cogs_account).",
			adjustment_account=".db_escape($adjustment_account).",
			wip_account=".db_escape($wip_account).",
			dimension_id=".db_escape($dimension_id).",
			dimension2_id=".db_escape($dimension2_id).",
			tax_type_id=".db_escape($tax_type_id).",
			no_sale=".db_escape($no_sale).",
			no_purchase=".db_escape($no_purchase).",
			editable=".db_escape($editable).",
			depreciation_method=".db_escape($depreciation_method).",
			depreciation_rate=".db_escape($depreciation_rate).",
			depreciation_factor=".db_escape($depreciation_factor).",
			fa_class_id=".db_escape($fa_class_id);

		if ($units != '')
			$sql .= ", units=".db_escape($units);

		if ($mb_flag != '')
			$sql .= ", mb_flag=".db_escape($mb_flag);

		if (isset($depreciation_start))
			$sql .= ", depreciation_start='".date2sql($depreciation_start)."', depreciation_date='".date2sql($depreciation_start)."'";

		if (is_array($tracking))
			$sql .= build_tracking_sql_set($tracking);

		$sql .= " WHERE stock_id=".db_escape($stock_id);

		if (!db_query($sql, 'The item could not be updated')) return false;

		if (!item_codes_entity::update_item_code(-1, $stock_id, $stock_id, $description, $category_id, 1, 0)) return false;
		return true;
	});
}

/** Serialize native item defaults, retained intent and all dependent SQL. */
function add_item($stock_id, $description, $long_description, $category_id, $tax_type_id, $units, $mb_flag,	$sales_account, $inventory_account, $cogs_account, $adjustment_account,	$wip_account, $dimension_id, $dimension2_id, $no_sale, $editable, $no_purchase, $depreciation_method='D', $depreciation_rate=100,  $depreciation_factor=1, $depreciation_start=null, $fa_class_id=null, $tracking=null) {
	if (dimension_writer_stock_id($stock_id) === false) return false;
	return dimension_ordinary_master_write('stock_master', null,
		array('dimension_id'=>$dimension_id, 'dimension2_id'=>$dimension2_id), function($next) use ($stock_id, $description, $long_description, $category_id, $tax_type_id, $units, $mb_flag, $sales_account, $inventory_account, $cogs_account, $adjustment_account, $wip_account, $dimension_id, $dimension2_id, $no_sale, $editable, $no_purchase, $depreciation_method, $depreciation_rate, $depreciation_factor, $depreciation_start, $fa_class_id, $tracking) {
		$dimension_id = $next['dimension_id']; $dimension2_id = $next['dimension2_id'];
		$tracking_cols = '';
		$tracking_vals = '';
		if (is_array($tracking)) {
			$parts = build_tracking_sql_insert($tracking);
			$tracking_cols = $parts['cols'];
			$tracking_vals = $parts['vals'];
		}

		$sql = "INSERT INTO ".TB_PREF."stock_master (stock_id, description, long_description, category_id,
			tax_type_id, units, mb_flag, sales_account, inventory_account, cogs_account,
			adjustment_account, wip_account, dimension_id, dimension2_id, no_sale, no_purchase, editable,
			depreciation_method, depreciation_rate, depreciation_factor"
			.(isset($depreciation_start) ? ", depreciation_start, depreciation_date, fa_class_id" : "")
			.$tracking_cols
			.") VALUES (".db_escape($stock_id).", ".db_escape($description).", ".db_escape($long_description).",
			".db_escape($category_id).", ".db_escape($tax_type_id).", "
			.db_escape($units).", ".db_escape($mb_flag).",
			".db_escape($sales_account).", ".db_escape($inventory_account)
			.", ".db_escape($cogs_account).",".db_escape($adjustment_account)
			.", ".db_escape($wip_account).", "
			.db_escape($dimension_id).", ".db_escape($dimension2_id).","
			.db_escape($no_sale).","
			.db_escape($no_purchase).","
			.db_escape($editable).","
			.db_escape($depreciation_method).",".db_escape($depreciation_rate).",".db_escape($depreciation_factor)
			.(isset($depreciation_start) ? ",'".date2sql($depreciation_start)."','".date2sql($depreciation_start)."',".db_escape($fa_class_id) : "")
			.$tracking_vals
			.")";

		if (!db_query($sql, 'The item could not be added')) return false;

		$sql = "INSERT INTO ".TB_PREF."loc_stock (loc_code, stock_id)
			SELECT loc_code, ".db_escape($stock_id)
			." FROM ".TB_PREF."locations";

		if (!db_query($sql, 'The item locstock could not be added')) return false;

		if (!item_codes_entity::create(array(
			'item_code' => $stock_id,
			'stock_id' => $stock_id,
			'description' => $description,
			'category_id' => $category_id,
			'quantity' => 1,
			'is_foreign' => 0
		))) return false;
		return true;
	});
}

/** Serialize native item defaults, retained intent and all dependent SQL. */
function delete_item($stock_id) {
	if (dimension_writer_stock_id($stock_id) === false) return false;
	return dimension_ordinary_master_write('stock_master', $stock_id,
		array('dimension_id'=>0, 'dimension2_id'=>0), function($next) use ($stock_id) {
		if (!item_dimension_cascade_removable($stock_id)) return false;
		$sql="DELETE FROM ".TB_PREF."stock_master WHERE stock_id=".db_escape($stock_id);
		if (!db_query($sql, 'could not delete stock item')) return false;

		/*and cascade deletes in loc_stock */
		$sql ="DELETE FROM ".TB_PREF."loc_stock WHERE stock_id=".db_escape($stock_id);
		if (!db_query($sql, 'could not delete stock item loc stock')) return false;

		/*and cascade deletes in purch_data */
		$sql ="DELETE FROM ".TB_PREF."purch_data WHERE stock_id=".db_escape($stock_id);
		if (!db_query($sql, 'could not delete stock item purch data')) return false;

		/*and cascade deletes in prices */
		$sql ="DELETE FROM ".TB_PREF."prices WHERE stock_id=".db_escape($stock_id);
		if (!db_query($sql, 'could not delete stock item prices')) return false;

		/*and cascade delete the bill of material if any */
		$sql = "DELETE FROM ".TB_PREF."bom WHERE parent=".db_escape($stock_id);
		if (!db_query($sql, 'could not delete stock item bom')) return false;

		$sql = "DELETE FROM ".TB_PREF."item_codes WHERE item_code=".db_escape($stock_id);
		if (!db_query($sql)) return false;
		return true;
	}, false, true);
}

/**
 * Lock only native deletion targets and deny retained or opaque intent. Missing
 * schema/query state is a failure, never proof of absence. Other domain writers
 * still need the complete B02/B10 protocol; this adds no Dimension deletion grant.
 */
function item_dimension_cascade_removable($stock_id) {
    $targets = array('purch_data'=>'stock_id', 'prices'=>'stock_id',
        'bom'=>'parent', 'item_codes'=>'item_code');
    foreach ($targets as $table=>$key) {
        $rows = db_query('SELECT custom_data FROM '.TB_PREF.$table
            .' WHERE '.$key.'='.db_escape($stock_id).' FOR UPDATE');
        if (!$rows) return false;
        while ($row = db_fetch($rows))
            if (!array_key_exists('custom_data', $row)
                || dimension_master_payload_transition($row['custom_data'], null, true) === false) return false;
    }
    return true;
}

function get_items($fixed_asset = 0) {
	$sql = "SELECT * FROM ".TB_PREF."stock_master WHERE fixed_asset=".db_escape($fixed_asset);
	return db_query($sql, 'items could not be retreived');
}

function item_in_foreign_codes($stock_id) {
	$sqls=  array(
		"SELECT COUNT(*) FROM ".TB_PREF."stock_moves WHERE stock_id=".db_escape($stock_id) =>
		_('Cannot delete this item because there are stock movements that refer to this item.'),
		"SELECT COUNT(*) FROM ".TB_PREF."bom WHERE component=".db_escape($stock_id) =>
		_('Cannot delete this item record because there are bills of material that require this part as a component.'),
		"SELECT COUNT(*) FROM ".TB_PREF."sales_order_details WHERE stk_code=".db_escape($stock_id) =>
		_('Cannot delete this item because there are existing sales order items for it.'),
		"SELECT COUNT(*) FROM ".TB_PREF."purch_order_details WHERE item_code=".db_escape($stock_id) =>
		_('Cannot delete this item because there are existing purchase order items for it.')
	);

	$msg = '';

	foreach($sqls as $sql=>$err) {
		$result = db_query($sql, 'could not query stock usage');
		$myrow = db_fetch_row($result);
		if ($myrow[0] > 0) 
			$msg = $err; break;
	}
	if ($msg == '') {

		$kits = get_where_used($stock_id);
		$num_kits = db_num_rows($kits);
		if ($num_kits) {
			$msg = _('This item cannot be deleted because some code aliases or foreign codes was entered for it, or there are kits defined using this item as component').':<br>';

			while($num_kits--) {
				$kit = db_fetch($kits);
				$msg .= "'".$kit[0]."'";
				if ($num_kits)
					$msg .= ',';
			}

		}
	}
	return $msg;
}

function get_items_search($description, $type) {
	global $SysPrefs;

	$sql = "SELECT COUNT(i.item_code) AS kit, i.item_code, i.description, c.description category
		FROM ".TB_PREF."stock_master s, ".TB_PREF."item_codes i
			LEFT JOIN ".TB_PREF."stock_category c ON i.category_id=c.category_id
		WHERE i.stock_id=s.stock_id
			AND !i.inactive AND !s.inactive
			AND (  i.item_code LIKE " . db_escape("%" . $description. "%") . " OR 
				i.description LIKE " . db_escape("%" . get_post("description"). "%") . " OR 
				c.description LIKE " . db_escape("%" . get_post("description"). "%") . ") ";

	switch ($type) {
		case "sales":
			$sql .= " AND !s.no_sale AND mb_flag <> 'F'";
			break;
		case "manufactured":
			$sql .= " AND mb_flag = 'M'";
			break;
		case "purchasable":
			$sql .= " AND NOT no_purchase AND mb_flag <> 'F' AND i.item_code=i.stock_id";
			break;
		case "costable":
			$sql .= " AND mb_flag <> 'D' AND mb_flag <> 'F' AND  i.item_code=i.stock_id";
			break;
		case "component":
			$parent = $_GET['parent'];
			$sql .= " AND  i.item_code=i.stock_id AND i.stock_id <> '$parent' AND mb_flag <> 'F' ";
			break;
		case "kits":
			$sql .= " AND !i.is_foreign AND i.item_code!=i.stock_id AND mb_flag <> 'F'";
			break;
		case "all":
			$sql .= " AND mb_flag <> 'F' AND i.item_code=i.stock_id";
			break;
	}

	if (isset($SysPrefs->max_rows_in_search))
		$limit = $SysPrefs->max_rows_in_search;
	else
		$limit = 10;

	$sql .= " GROUP BY i.item_code ORDER BY i.description LIMIT 0,".(int)($limit);

	return db_query($sql, 'Failed in retreiving item list.');
}

function get_item_code_id($stock_id) {
	$sql = "SELECT ic.id
		FROM ".TB_PREF."item_codes ic, ".TB_PREF."stock_master sm
		WHERE ic.item_code = ".db_escape($stock_id)." AND ic.item_code = ic.stock_id AND
		ic.stock_id = sm.stock_id";

	$result = db_query($sql, 'could not get item id');
	$row = db_fetch_row($result);

	return is_array($row) ? $row[0] : false;
}

/**
 * Build SQL SET clause fragment for tracking columns in UPDATE.
 *
 * @param array $tracking Associative array of tracking field values
 * @return string SQL fragment starting with ", column=value, ..."
 */
function build_tracking_sql_set($tracking) {
	$valid_columns = array(
		'track_by', 'has_serial_no', 'has_batch_no', 'has_expiry_date',
		'serial_no_prefix', 'batch_no_prefix', 'warranty_days',
		'quality_inspection_required', 'min_order_qty', 'lead_time_days',
		'shelf_life_days', 'item_weight', 'item_volume',
		'storage_category_id', 'abc_class'
	);

	$sql = '';
	foreach ($valid_columns as $col) {
		if (array_key_exists($col, $tracking)) {
			$val = $tracking[$col];
			if ($val === null || $val === '')
				$sql .= ", $col=NULL";
			else
				$sql .= ", $col=".db_escape($val);
		}
	}
	return $sql;
}

/**
 * Build SQL column names and values fragments for tracking columns in INSERT.
 *
 * @param array $tracking Associative array of tracking field values
 * @return array ['cols' => string, 'vals' => string]
 */
function build_tracking_sql_insert($tracking) {
	$valid_columns = array(
		'track_by', 'has_serial_no', 'has_batch_no', 'has_expiry_date',
		'serial_no_prefix', 'batch_no_prefix', 'warranty_days',
		'quality_inspection_required', 'min_order_qty', 'lead_time_days',
		'shelf_life_days', 'item_weight', 'item_volume',
		'storage_category_id', 'abc_class'
	);

	$cols = '';
	$vals = '';
	foreach ($valid_columns as $col) {
		if (array_key_exists($col, $tracking)) {
			$val = $tracking[$col];
			$cols .= ", $col";
			if ($val === null || $val === '')
				$vals .= ", NULL";
			else
				$vals .= ", ".db_escape($val);
		}
	}
	return array('cols' => $cols, 'vals' => $vals);
}
