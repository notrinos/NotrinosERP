<?php
/** Attachment-owned ordinary retained presence; protected/shared families remain unknown. */
require_once dirname(dirname(__DIR__)).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(__DIR__)).'/dimensions/includes/dimension_reference_registry.inc';
require_once dirname(dirname(__DIR__)).'/includes/attachment_service.inc';

/** Return only reviewed ordinary attachment types, never caller-provided families. */
function attachment_dimension_reference_sources()
{
    return array('masters'=>array('ST_CUSTOMER','ST_SUPPLIER','ST_ITEM','ST_FIXEDASSET'),
        'documents'=>array('ST_COSTUPDATE','ST_SUPPAYMENT','ST_CUSTPAYMENT','ST_SALESORDER','ST_SALESQUOTE','ST_SALESINVOICE','ST_CUSTCREDIT','ST_CUSTDELIVERY','ST_LOCTRANSFER','ST_INVADJUST','ST_PURCHORDER','ST_SUPPINVOICE','ST_SUPPCREDIT','ST_SUPPRECEIVE','ST_PURCHREQUISITION','ST_PURCHRFQ','ST_WORKORDER','ST_MANUISSUE','ST_MANURECEIVE','ST_DIMENSION'));
}

/** Recheck exact active-company metadata and type columns without cached absence. */
function attachment_dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized() || $table !== 'attachments') return false;
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.'attachments')."'"
        ." AND BINARY COLUMN_NAME IN ('custom_data','type_no')");
    $row = $rows ? db_fetch_row($rows) : false;
    return $row && (int)$row[0] === 2;
}

/**
 * Observe ordinary attachment JSON through the native defensive decoder. Exclude
 * all unsupported/shared families in SQL before transferring metadata. Neither
 * filenames, storage keys, document identities nor payload leave this boundary.
 * Missing owners, malformed data and absence stay unknown; no file is opened.
 * This advisory observation never authorizes deletion or a concurrent writer.
 *
 * @param int|string $id Canonical active-company native Dimension identity
 * @return string used or unknown only
 */
function attachment_observe_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    try {
        if (!attachment_dimension_reference_schema_available('attachments')) return 'unknown';
        $types = array();
        foreach (attachment_dimension_reference_sources() as $group) foreach ($group as $name) {
            if (!defined($name)) continue;
            $types[] = (int)constant($name);
        }
        if (!count($types)) return 'unknown';
        $rows = db_query('SELECT custom_data AS payload FROM '.TB_PREF.'attachments WHERE type_no IN ('.implode(',', $types).')');
        if (!$rows) return 'unknown';
        while ($row = db_fetch($rows)) {
            $payload = attachment_custom_data_array($row['payload']);
            if (dimension_reference_payload_status($payload,(int)$id) === 'used') return 'used';
        }
    } catch (Exception $exception) {
        // Unavailable or malformed owners never prove unused state.
    }
    return 'unknown';
}
