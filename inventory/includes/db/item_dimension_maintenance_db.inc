<?php
/** Fixed item/alias maintenance commands using the existing ordinary reference fence. */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/** Read native stock identity and all retained coordinates under the caller's transaction. */
function item_dimension_stock_metadata_row($stock_id)
{
    if(dimension_writer_stock_id($stock_id)===false)return false;
    $rows=db_query('SELECT stock_id,dimension_id,dimension2_id,custom_data,inactive,BINARY stock_id=BINARY '.db_escape($stock_id).' AS exact_native_key FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($stock_id).' FOR UPDATE');
    return $rows && db_num_rows($rows)===1?db_fetch_assoc($rows):false;
}

/** Complete the existing generic stock Entity path against its admitted native row. */
function item_dimension_stock_entity_write($operation,$key,$next,$persist,$new_key=null)
{
    $context=dimension_financial_reference_context();$identity=$operation==='insert'?$new_key:$key;
    if(!$context || !in_array($operation,array('insert','update','delete'),true)
        || dimension_writer_stock_id($identity)===false || !is_array($next) || !is_callable($persist))return false;
    return dimension_ordinary_master_write('stock_master',$key,$next,function($admitted)use($operation,$identity,$persist,$context){
        $before=$operation==='insert'?null:item_dimension_stock_metadata_row($identity);
        if($operation!=='insert' && !$before)return false;
        if($operation==='insert' && !db_query('SELECT custom_data FROM '.TB_PREF.'stock_master LIMIT 0'))return false;
        $saved=call_user_func($persist,$admitted);if(!$saved || dimension_financial_reference_context()!==$context)return false;
        if($operation==='delete'){
            $rows=db_query('SELECT custom_data FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($identity).' FOR UPDATE');
            return $rows && db_num_rows($rows)===0?$saved:false;
        }
        $after=item_dimension_stock_metadata_row($identity);if(!$after)return false;
        if($before?$before['stock_id']!==$after['stock_id']:(int)$after['exact_native_key']!==1)return false;
        $expected=array_key_exists('custom_data',$admitted)?$admitted['custom_data']:($before?$before['custom_data']:'{}');
        if($after['custom_data']!==$expected)return false;
        foreach(array('dimension_id','dimension2_id') as $field){
            $expected=array_key_exists($field,$admitted)?$admitted[$field]:($before?$before[$field]:0);
            if((int)$after[$field]!==dimension_writer_reference_id($expected))return false;
        }
        return $saved;
    },$operation==='update',$operation==='delete');
}

/** Admit every actual kit/alias member before native bulk SQL, then verify completion. */
function item_code_dimension_group_write($item_code,$next,$persist,$remove=false)
{
    $context=dimension_financial_reference_context();
    if(!$context || !is_string($item_code) || !is_array($next) || !is_callable($persist)
        || array_diff(array_keys($next),array('description','category_id','inactive')))return false;
    try{
        if(!begin_transaction())return false;
        if(!db_query('SELECT custom_data FROM '.TB_PREF.'item_codes LIMIT 0')){cancel_transaction();return false;}
        // Preserve the native text predicate; locked numeric row IDs establish custody.
        $where='item_code='.db_escape($item_code);
        $rows=db_query('SELECT * FROM '.TB_PREF.'item_codes WHERE '.$where.' ORDER BY id FOR UPDATE');
        if(!$rows){cancel_transaction();return false;}$before=array();
        while($row=db_fetch_assoc($rows)){$before[$row['id']]=$row;}
        if(!dimension_master_metadata_lock_rows($before)){cancel_transaction();return false;}
        foreach($before as $row)if(!dimension_ordinary_master_write('item_codes',$row['id'],$next,function($admitted){return true;},true,$remove)){cancel_transaction();return false;}
        $saved=call_user_func($persist);
        if(!$saved || dimension_financial_reference_context()!==$context){cancel_transaction();return false;}
        $rows=db_query('SELECT * FROM '.TB_PREF.'item_codes WHERE '.$where.' ORDER BY id FOR UPDATE');
        if(!$rows || db_num_rows($rows)!==($remove?0:count($before))){cancel_transaction();return false;}
        while($after=db_fetch_assoc($rows)){
            if(!isset($before[$after['id']])){cancel_transaction();return false;}$old=$before[$after['id']];
            foreach(array('id','item_code','stock_id','is_foreign','quantity','custom_data') as $field)
                if(!array_key_exists($field,$old) || !array_key_exists($field,$after) || $old[$field]!==$after[$field]){cancel_transaction();return false;}
        }
        if(dimension_financial_reference_context()!==$context || !commit_transaction()){cancel_transaction();return false;}
        return $saved;
    }catch(Exception $e){cancel_transaction();return false;}catch(Throwable $e){cancel_transaction();return false;}
}

/** Native stock status cannot change either retained scalar slot or raw metadata. */
function item_dimension_stock_status_write($stock_id,$status)
{
    $context=dimension_financial_reference_context();
    if(!$context || dimension_writer_stock_id($stock_id)===false)return false;
    return dimension_ordinary_master_write('stock_master',$stock_id,array(),function($admitted)use($stock_id,$status,$context){
        $before=item_dimension_stock_metadata_row($stock_id);if(!$before)return false;
        if(!db_query('UPDATE '.TB_PREF.'stock_master SET inactive='.(int)$status.' WHERE stock_id='.db_escape($stock_id)))return false;
        $after=item_dimension_stock_metadata_row($stock_id);
        if(!$after || dimension_financial_reference_context()!==$context)return false;
        foreach(array('stock_id','dimension_id','dimension2_id','custom_data') as $field)
            if(!array_key_exists($field,$before) || !array_key_exists($field,$after) || $before[$field]!==$after[$field])return false;
        return (int)$after['inactive']===(int)$status;
    },true);
}
