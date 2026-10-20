<?php
/** Inventory-owned saved movement/workflow metadata; presence never authorizes deletion. */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_registry.inc';

/** Return fixed retained owners and source-verified ordinary transaction discriminators. */
function inventory_retained_dimension_reference_sources()
{
    return array('stock_moves'=>array('type',array('ST_INVADJUST','ST_LOCTRANSFER',
        'ST_SUPPRECEIVE','ST_SUPPCREDIT','ST_CUSTDELIVERY','ST_CUSTCREDIT',
        'ST_WORKORDER','ST_MANUISSUE','ST_MANURECEIVE')),
        'wh_material_requests'=>array(null,array()),
        'wh_material_request_lines'=>array(null,array()),
        'wh_transfer_orders'=>array(null,array()),
        'wh_transfer_order_lines'=>array(null,array()),
        'wh_cycle_count_plans'=>array(null,array()),
        'wh_cycle_counts'=>array(null,array()),
        'wh_scrap'=>array(null,array()),
        'wh_return_orders'=>array(null,array()),
        'wh_return_order_lines'=>array(null,array()),
        'wh_consignment_stock'=>array(null,array()),
        'warranty_claims'=>array(null,array()),
        'recall_campaigns'=>array(null,array()),
        'wh_operation_lines'=>array(null,array()),
        'serial_numbers'=>array(null,array()),
        'stock_batches'=>array(null,array()),
        'stock_category'=>array(null,array()),
        'locations'=>array(null,array()));
}

/** Recheck exact active-prefix columns; no caller-supplied schema or cached absence. */
function inventory_retained_dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized()) return false;
    $sources = inventory_retained_dimension_reference_sources();
    if (!is_string($table) || !isset($sources[$table])) return false;
    $columns = array(db_escape('custom_data'));
    if ($sources[$table][0] !== null) $columns[] = db_escape($sources[$table][0]);
    if ($table === 'wh_operation_lines') $columns[] = db_escape('op_id');
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
        .' AND BINARY COLUMN_NAME IN ('.implode(',',$columns).')');
    if (!$rows) return false;
    $row = db_fetch_row($rows);
    if (!$row || (int)$row[0] !== count($columns)) return false;
    if ($table !== 'wh_operation_lines') return true;
    // Parent discrimination is mandatory before any line payload can be loaded.
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.'wh_operations')."'"
        ." AND BINARY COLUMN_NAME IN ('op_id','source_doc_type')");
    $row = $rows ? db_fetch_row($rows) : false;
    return $row && (int)$row[0] === 2;
}

/**
 * Observe strict raw JSON retained by fixed ordinary Inventory owners. SQL
 * excludes unsupported/protected families before payloads reach PHP. Include
 * zero/negative quantities and historical/orphan records without master joins.
 * Operation lines require a present ordinary parent; orphan/unsupported parent
 * lines remain unknown without loading their payload. Other fixed standalone
 * workflow stores retain orphan rows. No stock, actor or payload leaves this boundary. Missing
 * owners and absence remain unknown, including after legacy source deletion.
 * This adds no classification writer, approval, costing or retention authority.
 *
 * @param int|string $id Canonical positive active-company native identity
 * @return string used or unknown only
 */
function inventory_observe_retained_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    foreach (inventory_retained_dimension_reference_sources() as $table=>$source) {
        try {
            if (!inventory_retained_dimension_reference_schema_available($table)) continue;
            $sql = 'SELECT custom_data AS payload FROM '.TB_PREF.$table;
            if ($table === 'wh_operation_lines') {
                $types = array();
                foreach (array('ST_SUPPRECEIVE','ST_CUSTDELIVERY','ST_INVADJUST') as $name) {
                    if (!defined($name)) continue 2;
                    $types[] = (int)constant($name);
                }
                $sql = 'SELECT line.custom_data AS payload FROM '.TB_PREF.'wh_operation_lines line'
                    .' INNER JOIN '.TB_PREF.'wh_operations op ON op.op_id=line.op_id'
                    .' WHERE op.source_doc_type IS NULL OR op.source_doc_type IN ('.implode(',',$types).')';
            } elseif ($source[0] !== null) {
                $types = array();
                foreach ($source[1] as $name) {
                    if (!defined($name)) continue 2;
                    $types[] = (int)constant($name);
                }
                $sql .= ' WHERE '.$source[0].' IN ('.implode(',',$types).')';
            }
            $rows = db_query($sql);
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $payload = is_string($row['payload']) ? json_decode($row['payload'],true) : null;
                if (is_array($payload) && dimension_reference_payload_status($payload,(int)$id) === 'used') return 'used';
            }
        } catch (Exception $exception) {
            // An unavailable owner never certifies absence; inspect positive siblings.
        }
    }
    return 'unknown';
}
