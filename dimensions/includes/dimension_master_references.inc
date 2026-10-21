<?php
/** Fixed ordinary master metadata presence; never deletion or writer authority. */
require_once __DIR__.'/dimension_lifecycle_db.inc';
require_once __DIR__.'/dimension_reference_registry.inc';

/** Return only source-reviewed master stores and ordinary discriminator constants. */
function dimension_master_reference_sources()
{
    return array(
        'areas'=>array(null, array()),
        'tax_types'=>array(null, array()),
        'reflines'=>array('trans_type', array('ST_COSTUPDATE','ST_SUPPAYMENT','ST_CUSTPAYMENT','ST_SALESORDER','ST_SALESQUOTE','ST_SALESINVOICE','ST_CUSTCREDIT','ST_CUSTDELIVERY','ST_LOCTRANSFER','ST_INVADJUST','ST_PURCHORDER','ST_SUPPINVOICE','ST_SUPPCREDIT','ST_SUPPRECEIVE','ST_PURCHREQUISITION','ST_PURCHRFQ','ST_WORKORDER','ST_MANUISSUE','ST_MANURECEIVE','ST_DIMENSION')),
        'tags'=>array('type', array('TAG_ACCOUNT','TAG_DIMENSION')),
        'dimensions'=>array(null, array()));
}

/** Check exact active-company columns without trusting a cache or caller table. */
function dimension_master_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized()) return false;
    $sources = dimension_master_reference_sources();
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
 * Observe raw native intent in fixed ordinary masters, including inactive and
 * orphan history. Shared reference/tag families are excluded in SQL before
 * payload transfer. Native getters expose raw JSON: no HTML, serialized object
 * or nested-string decoder is inferred. Return only positive presence; missing,
 * malformed and absent state cannot certify unused or authorize deletion.
 *
 * @param int|string $id Canonical active-company Dimension identity
 * @return string used or unknown only
 */
function dimension_master_references_observe($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    foreach (dimension_master_reference_sources() as $table=>$source) {
        $where = '';
        if ($source[0] !== null) {
            $types = array();
            foreach ($source[1] as $constant) {
                if (!defined($constant)) continue 2;
                $types[] = (int)constant($constant);
            }
            if (!count($types)) continue;
            $where = ' WHERE '.$source[0].' IN ('.implode(',', $types).')';
        }
        try {
            if (!dimension_master_reference_schema_available($table)) continue;
            $rows = db_query('SELECT custom_data AS payload FROM '.TB_PREF.$table.$where);
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $payload = is_string($row['payload']) ? json_decode($row['payload'], true) : null;
                if (is_array($payload) && dimension_reference_payload_status($payload, (int)$id) === 'used') return 'used';
            }
        } catch (Exception $exception) {
            // Unavailable siblings never disprove a positive retained reference.
        }
    }
    return 'unknown';
}
