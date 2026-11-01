<?php
/**
 * Lock only family draft identities in the owning company transaction.
 * Existing native commands already serialize each complete review family.
 * Material precedes Transfer wherever their custody meets. No payload is read,
 * decoded or endorsed here; native validators retain permission and source checks.
 */
function lock_warehouse_review_family($trans_type)
{
    if (empty($GLOBALS['transaction_level'])) return false;
    return db_query('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.(int)$trans_type
        .' ORDER BY id FOR UPDATE') !== false;
}
