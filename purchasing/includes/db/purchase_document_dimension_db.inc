<?php
/** PO/GRN retained metadata joins the existing lifecycle protocol, without granting document authority. */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/** Fixed physical document owners; generic Entity CRUD cannot authorize their native commands. */
function purchase_document_dimension_sources()
{
    return array('purch_orders'=>'order_no','purch_order_details'=>'po_detail_item','grn_batch'=>'id','grn_items'=>'id');
}

/** Keep native SQL/results and unwind financial callers that historically ignored write returns. */
function purchase_document_dimension_write($owner,$key,$persist,$remove=false,$final_key=null)
{
    $sources=purchase_document_dimension_sources();
    if (!isset($sources[$owner]) || !is_callable($persist)) dimension_financial_reference_failure();
    $result=dimension_master_metadata_write($owner,$key,array(),function($admitted)use($persist){return call_user_func($persist);},$remove,$final_key);
    if (!$result) dimension_financial_reference_failure();
    return $result;
}

/** Canonical native id sets, including an explicit empty available source. */
function purchase_document_dimension_ids($ids)
{
    if (!is_array($ids)) dimension_financial_reference_failure();$result=array();
    foreach ($ids as $id) {
        $id=dimension_writer_reference_id($id);if (!$id) dimension_financial_reference_failure();
        $result[$id]=$id;
    }
    sort($result,SORT_NUMERIC);return $result;
}

/** Capture actual rows before native hooks/SQL, with parent-first row locks and a sorted master union. */
function purchase_document_dimension_snapshot($identities)
{
    $sources=purchase_document_dimension_sources();$context=dimension_financial_reference_context();
    if (!$context || empty($GLOBALS['transaction_level']) || !is_array($identities) || array_diff(array_keys($identities),array_keys($sources))) dimension_financial_reference_failure();
    $snapshot=array('context'=>$context,'rows'=>array());$all=array();
    foreach ($sources as $owner=>$primary) {
        if (!array_key_exists($owner,$identities)) continue;
        if (!db_query('SELECT '.$primary.',custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) dimension_financial_reference_failure();
        $ids=purchase_document_dimension_ids($identities[$owner]);$snapshot['rows'][$owner]=array();
        if (!$ids) continue;
        $rows=db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE '.$primary.' IN ('.implode(',',$ids).') ORDER BY '.$primary.' FOR UPDATE');
        if (!$rows || db_num_rows($rows)!==count($ids)) dimension_financial_reference_failure();
        while ($row=db_fetch_assoc($rows)) {
            if (!array_key_exists('custom_data',$row)) dimension_financial_reference_failure();
            $snapshot['rows'][$owner][(int)$row[$primary]]=$row['custom_data'];$all[]=$row;
        }
    }
    if (!dimension_master_metadata_lock_rows($all)) dimension_financial_reference_failure();
    foreach ($snapshot['rows'] as $owner=>$rows) foreach ($rows as $id=>$raw)
        if (!dimension_ordinary_master_write($owner,$id,array(),function($admitted){return true;},true)) dimension_financial_reference_failure();
    return $snapshot;
}

/** Verify original metadata, explicitly created native defaults and proven-unused removals at completion. */
function purchase_document_dimension_verify($snapshot,$created=array(),$removed=array())
{
    $sources=purchase_document_dimension_sources();
    if (empty($GLOBALS['transaction_level']) || !is_array($snapshot) || !isset($snapshot['context'],$snapshot['rows']) || dimension_financial_reference_context()!==$snapshot['context']
        || !is_array($created) || !is_array($removed) || array_diff(array_keys($created),array_keys($sources)) || array_diff(array_keys($removed),array_keys($sources))) dimension_financial_reference_failure();
    foreach ($sources as $owner=>$primary) {
        if (!array_key_exists($owner,$snapshot['rows']) && !array_key_exists($owner,$created) && !array_key_exists($owner,$removed)) continue;
        $expected=isset($snapshot['rows'][$owner])?$snapshot['rows'][$owner]:array();$ids=array_keys($expected);
        foreach (purchase_document_dimension_ids(isset($removed[$owner])?$removed[$owner]:array()) as $id) {
            if (!array_key_exists($id,$expected) || dimension_master_payload_transition($expected[$id],null,true)===false) dimension_financial_reference_failure();
            unset($expected[$id]);
        }
        foreach (purchase_document_dimension_ids(isset($created[$owner])?$created[$owner]:array()) as $id) {
            if (array_key_exists($id,$expected)) dimension_financial_reference_failure();
            $expected[$id]='{}';$ids[]=$id;
        }
        if (!db_query('SELECT '.$primary.',custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) dimension_financial_reference_failure();
        if (!$ids) continue;$ids=array_unique($ids);sort($ids,SORT_NUMERIC);
        $rows=db_query('SELECT '.$primary.',custom_data FROM '.TB_PREF.$owner.' WHERE '.$primary.' IN ('.implode(',',$ids).') ORDER BY '.$primary.' FOR UPDATE');
        if (!$rows || db_num_rows($rows)!==count($expected)) dimension_financial_reference_failure();
        while ($row=db_fetch_assoc($rows)) {
            $id=(int)$row[$primary];
            if (!array_key_exists($id,$expected) || $row['custom_data']!==$expected[$id]) dimension_financial_reference_failure();
            unset($expected[$id]);
        }
        if ($expected) dimension_financial_reference_failure();
    }
    if (dimension_financial_reference_context()!==$snapshot['context']) dimension_financial_reference_failure();
    return true;
}

/** Retain an actual current PO aggregate across native edit/remove hooks and later child SQL. */
function purchase_order_dimension_snapshot($order_no)
{
    $order_no=dimension_writer_reference_id($order_no);if (!$order_no || empty($GLOBALS['transaction_level'])) dimension_financial_reference_failure();
    $header=db_query('SELECT order_no FROM '.TB_PREF.'purch_orders WHERE order_no='.$order_no.' FOR UPDATE');
    if(!$header || db_num_rows($header)!==1)dimension_financial_reference_failure();
    $rows=db_query('SELECT po_detail_item FROM '.TB_PREF.'purch_order_details WHERE order_no='.$order_no.' ORDER BY po_detail_item FOR UPDATE');
    if (!$rows) dimension_financial_reference_failure();$ids=array();while($row=db_fetch_assoc($rows))$ids[]=$row['po_detail_item'];
    $snapshot=purchase_document_dimension_snapshot(array('purch_orders'=>array($order_no),'purch_order_details'=>$ids));
    $snapshot['order_no']=$order_no;return $snapshot;
}

/** Preserve exact native PO line membership while admitting only explicit fresh/unused deltas. */
function purchase_order_dimension_verify($snapshot,$created=array(),$removed=array(),$remove_header=false)
{
    $created=purchase_document_dimension_ids($created);$removed=purchase_document_dimension_ids($removed);
    purchase_document_dimension_verify($snapshot,array('purch_order_details'=>$created),array('purch_order_details'=>$removed,'purch_orders'=>$remove_header?array($snapshot['order_no']):array()));
    $expected=array_fill_keys(array_keys($snapshot['rows']['purch_order_details']),true);
    foreach($removed as $id)unset($expected[$id]);foreach($created as $id)$expected[$id]=true;
    $rows=db_query('SELECT po_detail_item FROM '.TB_PREF.'purch_order_details WHERE order_no='.(int)$snapshot['order_no'].' ORDER BY po_detail_item FOR UPDATE');
    if (!$rows || db_num_rows($rows)!==count($expected)) dimension_financial_reference_failure();
    while($row=db_fetch_assoc($rows)){if(!isset($expected[$row['po_detail_item']]))dimension_financial_reference_failure();unset($expected[$row['po_detail_item']]);}
    if($expected)dimension_financial_reference_failure();return true;
}

/** Admit and verify every physical item selected by the native GRN/stock text predicate. */
function purchase_grn_item_dimension_group($grn_no,$stock_id,$persist)
{
    $grn_no=dimension_writer_reference_id($grn_no);
    if (!$grn_no || ($stock_id!==null && !is_string($stock_id)) || !is_callable($persist)) dimension_financial_reference_failure();
    try {
        if (!begin_transaction()) dimension_financial_reference_failure();
        $where='grn_batch_id='.$grn_no.($stock_id===null?'':' AND item_code='.db_escape($stock_id));
        $rows=db_query('SELECT id FROM '.TB_PREF.'grn_items WHERE '.$where.' ORDER BY id FOR UPDATE');if(!$rows)dimension_financial_reference_failure();
        $ids=array();while($row=db_fetch_assoc($rows))$ids[]=$row['id'];
        $snapshot=purchase_document_dimension_snapshot(array('grn_items'=>$ids));$saved=call_user_func($persist);
        if(!$saved)dimension_financial_reference_failure();purchase_document_dimension_verify($snapshot);
        $rows=db_query('SELECT id FROM '.TB_PREF.'grn_items WHERE '.$where.' ORDER BY id FOR UPDATE');if(!$rows || db_num_rows($rows)!==count($ids))dimension_financial_reference_failure();
        $expected=array_fill_keys($ids,true);while($row=db_fetch_assoc($rows)){if(!isset($expected[$row['id']]))dimension_financial_reference_failure();unset($expected[$row['id']]);}
        if($expected || !commit_transaction())dimension_financial_reference_failure();return $saved;
    }catch(Exception $e){cancel_transaction();throw $e;}catch(Throwable $e){cancel_transaction();throw $e;}
}

/**
 * Run native PO/GRN maintenance SQL that never owns metadata.
 * Every named row is locked first and must keep its exact payload; a failed result or
 * hook/trigger metadata change restores the whole caller transaction.
 */
function purchase_document_dimension_guard($identities,$persist)
{
    if (!is_callable($persist) || !begin_transaction()) dimension_financial_reference_failure();
    try {
        $snapshot=purchase_document_dimension_snapshot($identities);
        $result=call_user_func($persist);
        if ($result===false) dimension_financial_reference_failure();
        purchase_document_dimension_verify($snapshot);
        if (!commit_transaction()) dimension_financial_reference_failure();
        return $result;
    }catch(Exception $e){cancel_transaction();throw $e;}catch(Throwable $e){cancel_transaction();throw $e;}
}

/** Current locked PO line and GRN item identities for one receipt batch, in native key order. */
function purchase_grn_batch_dimension_identities($grn_batch)
{
    $grn_batch=dimension_writer_reference_id($grn_batch);
    if (!$grn_batch || empty($GLOBALS['transaction_level'])) dimension_financial_reference_failure();
    $rows=db_query('SELECT id,po_detail_item FROM '.TB_PREF.'grn_items WHERE grn_batch_id='.$grn_batch.' ORDER BY id FOR UPDATE');
    if (!$rows) dimension_financial_reference_failure();
    $identities=array('purch_order_details'=>array(),'grn_batch'=>array($grn_batch),'grn_items'=>array());
    while ($row=db_fetch_assoc($rows)) {
        $identities['grn_items'][]=$row['id'];
        $identities['purch_order_details'][$row['po_detail_item']]=$row['po_detail_item'];
    }
    $identities['purch_order_details']=array_values($identities['purch_order_details']);
    return $identities;
}
