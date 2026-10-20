<?php
/** Sales-owned saved order/quotation presence observation (DIM-LFC-001B3c2). */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_registry.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_schema.inc';

/**
 * Fixed retained Sales JSON owners and their transaction-family discriminators.
 * Templates/agreements/customer and commercial-policy masters expose custom_data;
 * recognizing ordinary JSON here does not add classification to their writers.
 * No caller can select a table, discriminator, or supported transaction family.
 *
 * @return array Table => discriminator column and source constant names
 */
function sales_dimension_reference_sources()
{
    $orders = array('trans_type', array('ST_SALESORDER','ST_SALESQUOTE'));
    $transactions = array('ST_SALESINVOICE','ST_CUSTDELIVERY','ST_CUSTCREDIT','ST_CUSTPAYMENT');
    return array('sales_orders'=>$orders, 'sales_order_details'=>$orders,
        'debtors_master'=>array(null, array()),
        'sales_quotation_templates'=>array(null, array()),
        'sales_quotation_template_lines'=>array(null, array()),
        'sales_agreements'=>array(null, array()),
        'sales_agreement_lines'=>array(null, array()),
        'sales_types'=>array(null, array()),
        'sales_pricelists'=>array(null, array()),
        'sales_pricelist_rules'=>array(null, array()),
        'sales_discount_programs'=>array(null, array()),
        'debtor_trans'=>array('type', $transactions),
        'debtor_trans_details'=>array('debtor_trans_type', $transactions));
}

/**
 * Preflight only the columns needed by this fixed Sales JSON owner.
 * Keep optional JSON availability independent of scalar reference dependencies.
 * Exact binary table/column comparison rejects prefix/case lookalikes; recheck
 * each invocation rather than caching across companies or schema recovery.
 *
 * @param string $table One of the fixed Sales sources
 * @return bool Whether this reader's exact columns are currently present
 */
function sales_dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized()) return false;
    $sources = sales_dimension_reference_sources();
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
 * Observe recognized native intent in fixed retained Sales header/line/master JSON.
 * The existing save envelope retains arbitrary custom keys; inspect the entire
 * ordinary decoded array rather than just sales_discount_state. Exclude every
 * unsupported transaction family's payload in SQL, never unserialize embedded
 * carts, and release no document/customer/amount/content. All records remain in
 * scope, including completed/templates, quotes and orphan detail rows. Ordinary
 * debtor JSON is independently preflighted and SQL-filtered by its source type;
 * this never changes scalar header availability or admits a protected family.
 * No positive match means unknown: extension-defined encodings, other retained
 * sources, audit and sessions have not been attested by this bounded owner.
 *
 * @param int|string $id Exact company-local native identity
 * @return string used or unknown; never a deletion/writer-fence authority
 */
function sales_observe_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)
        || !defined('ST_SALESORDER') || !defined('ST_SALESQUOTE')) return 'unknown';
    foreach (sales_dimension_reference_sources() as $table=>$source) {
        $payload_column = 'custom_data';
        if ($source[0] !== null) {
            $types = array();
            foreach ($source[1] as $type) if (defined($type)) $types[] = (int)constant($type);
            if (!count($types)) continue;
            $payload_column = 'CASE WHEN '.$source[0].' IN ('.implode(',', $types).') THEN custom_data ELSE NULL END';
        }
        $sql = 'SELECT '.$payload_column.' AS payload FROM '.TB_PREF.$table;
        try {
            if (!sales_dimension_reference_schema_available($table)) continue;
            $rows = db_query($sql);
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $payload = is_string($row['payload']) ? json_decode($row['payload'], true) : null;
                if (is_array($payload) && dimension_reference_payload_status($payload, (int)$id) === 'used') return 'used';
            }
        } catch (Exception $exception) {
            // Still inspect the other fixed owner for positive presence.
        }
    }
    return 'unknown';
}
