<?php
/** GL-owned ordinary retained metadata presence; never unused/delete authority. */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_registry.inc';

/** Return only source-verified ordinary raw JSON owners, excluding transaction/payroll payloads. */
function gl_dimension_reference_sources()
{
    return array('chart_master','budget_trans','bank_accounts','chart_types','chart_class');
}

/** Recheck exact active-prefix JSON storage; deny before SQL or escaping. */
function gl_dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized() || !is_string($table)
        || !in_array($table,gl_dimension_reference_sources(),true)) return false;
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
        ." AND BINARY COLUMN_NAME=BINARY 'custom_data'");
    if (!$rows) return false;
    $row = db_fetch_row($rows);
    return $row && (int)$row[0] === 1;
}

/**
 * Observe retained ordinary native intent on chart/bank accounts, chart hierarchy
 * and budgets, including inactive/orphan masters and zero or historical budgets.
 * Source helpers expose raw custom_data, so only strict JSON is recognized;
 * no HTML/serialized/nested-string decoder is invented.
 * No account identity, bank detail, class, content or amount leaves this boundary. Absence
 * remains unknown across all other retained/domain/session/deployment owners.
 *
 * @param int|string $id Canonical positive active-company native identity
 * @return string used or unknown only
 */
function gl_observe_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    foreach (gl_dimension_reference_sources() as $table) {
        try {
            if (!gl_dimension_reference_schema_available($table)) continue;
            $rows = db_query('SELECT custom_data AS payload FROM '.TB_PREF.$table);
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $payload = is_string($row['payload']) ? json_decode($row['payload'],true) : null;
                if (is_array($payload) && dimension_reference_payload_status($payload,(int)$id) === 'used') return 'used';
            }
        } catch (Exception $exception) {
            // No unavailable retained owner can attest absence.
        }
    }
    return gl_observe_transaction_dimension_references($id);
}

/**
 * Return fixed ordinary transaction owners and their supported type constants.
 * Shared journal/banking types may contain payroll; they require their own owner.
 * No caller can supply a table, transaction family or payroll exclusion attestation.
 */
function gl_dimension_transaction_reference_sources()
{
    return array('gl_trans'=>array('ST_SALESINVOICE','ST_CUSTCREDIT','ST_CUSTPAYMENT',
        'ST_CUSTDELIVERY','ST_SUPPINVOICE','ST_SUPPCREDIT','ST_SUPPAYMENT','ST_SUPPRECEIVE'),
        'bank_trans'=>array('ST_CUSTPAYMENT','ST_SUPPAYMENT'),
        'audit_trail'=>array('ST_SALESINVOICE','ST_CUSTCREDIT','ST_CUSTPAYMENT','ST_CUSTDELIVERY',
            'ST_SUPPINVOICE','ST_SUPPCREDIT','ST_SUPPAYMENT','ST_SUPPRECEIVE','ST_DIMENSION'));
}

/** Recheck both exact active-company columns before querying an optional owner. */
function gl_dimension_transaction_reference_schema_available($table)
{
    $sources = gl_dimension_transaction_reference_sources();
    if (!dimension_lifecycle_authorized() || !is_string($table) || !isset($sources[$table])) return false;
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
        ." AND (BINARY COLUMN_NAME=BINARY 'custom_data' OR BINARY COLUMN_NAME=BINARY 'type')");
    if (!$rows) return false;
    $row = db_fetch_row($rows);
    return $row && (int)$row[0] === 2;
}

/**
 * Observe ordinary retained transaction JSON with unsupported families excluded
 * in SQL before payload loading. Never join current masters, dates, amounts or
 * void records: zeroed/closed/orphan history still owns its retained identity.
 * Only a minimized used/unknown status leaves this GL-owned boundary. This is
 * not an absence certificate, payroll reader, posting validator or writer fence.
 *
 * @param int|string $id Exact positive active-company native identity
 * @return string used or unknown only
 */
function gl_observe_transaction_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    foreach (gl_dimension_transaction_reference_sources() as $table=>$names) {
        $types = array();
        foreach ($names as $name) {
            if (!defined($name)) continue 2;
            $types[] = (int)constant($name);
        }
        try {
            if (!gl_dimension_transaction_reference_schema_available($table)) continue;
            $rows = db_query('SELECT custom_data AS payload FROM '.TB_PREF.$table
                .' WHERE type IN ('.implode(',',$types).')');
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $payload = is_string($row['payload']) ? json_decode($row['payload'],true) : null;
                if (is_array($payload) && dimension_reference_payload_status($payload,(int)$id) === 'used') return 'used';
            }
        } catch (Exception $exception) {
            // An unavailable ordinary or protected owner can never attest absence.
        }
    }
    return 'unknown';
}
