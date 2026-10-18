<?php
/** Inventory-owned retained JSON presence, never an unused/delete certificate. */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_registry.inc';

/**
 * Fixed ordinary Inventory sources, with source-owned document discriminators.
 * Raw master/QC/operation readers expose custom_data. Receipt, delivery and
 * adjustment constants come from their actual QC/WMS call sites. NULL supports
 * standalone records; other source families never release their JSON to PHP.
 * This list adds no classification fields or support to a business writer.
 *
 * @return array Table => discriminator and ordinary source constant names
 */
function inventory_dimension_reference_sources()
{
    return array('stock_master'=>array(null, array()),
        'quality_inspections'=>array('trans_type', array('ST_SUPPRECEIVE')),
        'wh_operations'=>array('source_doc_type', array('ST_SUPPRECEIVE','ST_CUSTDELIVERY','ST_INVADJUST')));
}

/**
 * Preflight exact active-prefix JSON/discriminator columns without fatal reads.
 * Recheck on every call; scalar stock-master availability remains independent.
 * No caller-supplied tables, columns, connection or availability cache is used.
 *
 * @param string $table Fixed Inventory source
 * @return bool Every exact column required by this reader is present
 */
function inventory_dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized()) return false;
    $sources = inventory_dimension_reference_sources();
    if (!is_string($table) || !isset($sources[$table])) return false;
    $columns = array(db_escape('custom_data'));
    if ($sources[$table][0] !== null) $columns[] = db_escape($sources[$table][0]);
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
        .' AND BINARY COLUMN_NAME IN ('.implode(',', $columns).')');
    if (!$rows) return false;
    $row = db_fetch_row($rows);
    return $row && (int)$row[0] === count($columns);
}

/**
 * Observe positive native identity in retained ordinary Inventory JSON only.
 * Include inactive, terminal and orphan sources; do not join current masters,
 * filter statuses, decode nested strings/serialized carts, or return content.
 * Keep looking for positive presence when another fixed owner is unavailable.
 * Protected, unsupported, empty and malformed data and global absence remain
 * unknown. No deletion or concurrent writer-fence authority is added.
 *
 * @param int|string $id Canonical positive company-local native identity
 * @return string used or unknown only
 */
function inventory_observe_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    foreach (inventory_dimension_reference_sources() as $table=>$source) {
        try {
            if (!inventory_dimension_reference_schema_available($table)) continue;
            $payload_column = 'custom_data';
            if ($source[0] !== null) {
                $allowed = $source[0].' IS NULL';
                $types = array();
                foreach ($source[1] as $type) if (defined($type)) $types[] = (int)constant($type);
                if (count($types)) $allowed .= ' OR '.$source[0].' IN ('.implode(',', $types).')';
                $payload_column = 'CASE WHEN '.$allowed.' THEN custom_data ELSE NULL END';
            }
            $rows = db_query('SELECT '.$payload_column.' AS payload FROM '.TB_PREF.$table);
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $payload = is_string($row['payload']) ? json_decode($row['payload'], true) : null;
                if (is_array($payload) && dimension_reference_payload_status($payload,(int)$id) === 'used') return 'used';
            }
        } catch (Exception $exception) {
            // An unavailable owner cannot prove absence; inspect positive siblings.
        }
    }
    return 'unknown';
}
