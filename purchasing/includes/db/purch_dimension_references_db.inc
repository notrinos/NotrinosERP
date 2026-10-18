<?php
/** Purchasing-owned retained source observation (DIM-LFC-001B3c3a). */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_registry.inc';

/**
 * Fixed saved-source owners and their existing legacy HTML-envelope support.
 * Requisition's decoder is strict JSON; RFQ/agreement/template readers allow one
 * envelope decode. PO headers/details have no custom_data in the current schema.
 * This list never accepts caller-selected tables, fields or decoders.
 *
 * @return array Table name => whether its owning reader supports that envelope
 */
function purchasing_dimension_reference_tables()
{
    return array('purch_requisitions'=>false, 'purch_requisition_lines'=>false,
        'purch_rfq'=>true, 'purch_rfq_items'=>true, 'purch_rfq_vendors'=>true,
        'purch_rfq_vendor_lines'=>true, 'purch_agreements'=>true,
        'purch_agreement_lines'=>true, 'purch_order_templates'=>true,
        'purch_order_template_lines'=>true);
}

/**
 * Check exact fixed owner columns without provoking legacy fatal SQL diagnostics.
 * check_table() uses LIKE wildcards; exact information_schema equality preserves
 * company-prefix identity and detects missing columns without querying that table.
 * No caller-selected table/column or cache is accepted. A concurrent deployment
 * DDL/connection failure may still terminate through the existing driver; it
 * cannot become an unused certificate or deletion authorization.
 *
 * @param string $table One of the fixed retained-source/approval owners
 * @return bool All columns used by that owner's observer currently exist
 */
function purchasing_dimension_reference_schema_available($table)
{
    if (!dimension_lifecycle_authorized()) return false;
    $required = array_fill_keys(array_keys(purchasing_dimension_reference_tables()), array('custom_data'));
    $required['approval_drafts'] = array('id','trans_type','draft_data');
    $required['approval_actions'] = array('draft_id','draft_data_snapshot');
    if (!is_string($table) || !isset($required[$table])) return false;
    $columns = array();
    foreach ($required[$table] as $column) $columns[] = db_escape($column);
    $rows = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
        ." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."' AND BINARY COLUMN_NAME IN (".implode(',', $columns).')');
    if (!$rows) return false;
    $row = db_fetch_row($rows);
    return $row && (int)$row[0] === count($required[$table]);
}

/**
 * Observe native intent in all retained ordinary Purchasing custom_data rows.
 * Mirror each owner's outer JSON/envelope semantics without loading document
 * writers or decoding nested strings/serialized objects. Include terminal,
 * inactive and orphan records. Return no source content, actor, amount or ID.
 * A positive reference blocks even if another owner is unavailable. Absence and
 * malformed/opaque state stay unknown: other retained, protected and session
 * authorities and complete writer fencing are unresolved. Never authorize delete.
 *
 * @param int|string $id Canonical company-local native Dimension identity
 * @return string used or unknown only
 */
function purchasing_observe_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    foreach (purchasing_dimension_reference_tables() as $table=>$legacy_envelope) {
        try {
            if (!purchasing_dimension_reference_schema_available($table)) continue;
            $rows = db_query('SELECT custom_data FROM '.TB_PREF.$table);
            if (!$rows) continue;
            while ($row = db_fetch($rows)) {
                $raw = $row['custom_data'];
                if (!is_string($raw)) continue;
                $payload = json_decode($raw, true);
                if (!is_array($payload) && $legacy_envelope)
                    $payload = json_decode(html_entity_decode($raw, ENT_QUOTES, 'UTF-8'), true);
                if (is_array($payload) && dimension_reference_payload_status($payload, (int)$id) === 'used')
                    return 'used';
            }
        } catch (Exception $exception) {
            // Missing owners cannot prove absence; keep looking for positive evidence.
        }
    }
    return 'unknown';
}

/**
 * Observe the Supplier Invoice approval collector's explicit expense-line axes.
 * These names are specific to collect_supp_trans_data/gl_codes, not arbitrary
 * allocation keys. The workflow authorizes and excludes other domains before
 * calling this pure reader. Ordinary header/nested intent uses the fixed shared
 * parser; malformed/opaque lines stay unknown. No payload is returned or altered.
 *
 * @param mixed $payload Decoded ordinary Supplier Invoice draft
 * @param int $id Exact positive native identity
 * @return string used, unused or unknown observation; never delete authority
 */
function purchasing_invoice_approval_dimension_status($payload, $id)
{
    $status = dimension_reference_payload_status($payload, $id);
    if ($status === 'used' || !is_array($payload) || !isset($payload['gl_codes'])) return $status;
    if (!is_array($payload['gl_codes'])) return 'unknown';
    foreach ($payload['gl_codes'] as $line) {
        if (!is_array($line)) { $status = 'unknown'; continue; }
        $axes = array();
        foreach (array('gl_dim'=>'dimension_id','gl_dim2'=>'dimension2_id') as $source=>$target)
            if (array_key_exists($source, $line)) $axes[$target] = $line[$source];
        $observed = dimension_reference_payload_status($axes, $id);
        if ($observed === 'used') return 'used';
        if ($observed === 'unknown') $status = 'unknown';
    }
    return $status;
}
