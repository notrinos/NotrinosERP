<?php
/** Sales-owned saved order/quotation presence observation (DIM-LFC-001B3c2). */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_registry.inc';

/**
 * Observe recognized native intent in retained Sales header/line JSON only.
 * The existing save envelope retains arbitrary custom keys; inspect the entire
 * ordinary decoded array rather than just sales_discount_state. Exclude every
 * unsupported transaction family's payload in SQL, never unserialize embedded
 * carts, and release no document/customer/amount/content. All records remain in
 * scope, including completed/templates, quotes and orphan detail rows.
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
    $types = (int)ST_SALESORDER.','.(int)ST_SALESQUOTE;
    foreach (array('sales_orders','sales_order_details') as $table) {
        $sql = 'SELECT CASE WHEN trans_type IN ('.$types.') THEN custom_data ELSE NULL END AS payload'
            .' FROM '.TB_PREF.$table;
        try {
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
