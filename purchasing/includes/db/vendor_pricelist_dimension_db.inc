<?php
/** Retained vendor-price custody using the existing ordinary writer protocol. */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/** Admit every currency row before native last-purchase bulk SQL and verify the whole group. */
function vendor_pricelist_dimension_group_write($supplier_id,$stock_id,$persist)
{
    $context=dimension_financial_reference_context();$supplier=dimension_writer_reference_id($supplier_id);
    if (!$context || $supplier===false || !is_string($stock_id) || !is_callable($persist)) return false;
    try {
        if (!begin_transaction()) return false;
        if (!db_query('SELECT custom_data FROM '.TB_PREF.'vendor_pricelists LIMIT 0')) {cancel_transaction();return false;}
        $where='supplier_id='.db_escape($supplier).' AND stock_id='.db_escape($stock_id);
        $rows=db_query('SELECT * FROM '.TB_PREF.'vendor_pricelists WHERE '.$where.' ORDER BY id FOR UPDATE');
        if (!$rows) {cancel_transaction();return false;}$before=array();
        while ($row=db_fetch_assoc($rows)) $before[$row['id']]=$row;
        if (!dimension_master_metadata_lock_rows($before)) {cancel_transaction();return false;}
        foreach ($before as $row)
            if (!dimension_ordinary_master_write('vendor_pricelists',$row['id'],array(),function($admitted){return true;},true)) {cancel_transaction();return false;}
        $saved=call_user_func($persist,$before);
        if (!$saved || dimension_financial_reference_context()!==$context) {cancel_transaction();return false;}
        $rows=db_query('SELECT * FROM '.TB_PREF.'vendor_pricelists WHERE '.$where.' ORDER BY id FOR UPDATE');
        if (!$rows || db_num_rows($rows)!==count($before)) {cancel_transaction();return false;}
        while ($after=db_fetch_assoc($rows)) {
            if (!isset($before[$after['id']])) {cancel_transaction();return false;}
            foreach ($before[$after['id']] as $field=>$value)
                if (!in_array($field,array('last_purchase_date','last_purchase_price'),true)
                    && (!array_key_exists($field,$after) || $after[$field]!==$value)) {cancel_transaction();return false;}
        }
        if (dimension_financial_reference_context()!==$context || !commit_transaction()) {cancel_transaction();return false;}
        return $saved;
    } catch (Exception $e) {cancel_transaction();return false;} catch (Throwable $e) {cancel_transaction();return false;}
}
