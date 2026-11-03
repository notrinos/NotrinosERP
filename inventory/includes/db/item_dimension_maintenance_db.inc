<?php
/** Fixed item/alias maintenance commands using the existing ordinary reference fence. */
require_once __DIR__.'/../../../dimensions/includes/dimension_master_metadata_writer.inc';

/** Admit every actual kit/alias member before native bulk SQL, then verify completion. */
function item_code_dimension_group_write($item_code,$next,$persist,$remove=false)
{
    $context=dimension_financial_reference_context();
    if(!$context || dimension_writer_stock_id($item_code)===false || !is_array($next) || !is_callable($persist)
        || array_diff(array_keys($next),array('description','category_id','inactive')))return false;
    try{
        if(!begin_transaction())return false;
        if(!db_query('SELECT custom_data FROM '.TB_PREF.'item_codes LIMIT 0')){cancel_transaction();return false;}
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
        $where='stock_id='.db_escape($stock_id);$columns='stock_id,dimension_id,dimension2_id,custom_data,inactive';
        $rows=db_query('SELECT '.$columns.' FROM '.TB_PREF.'stock_master WHERE '.$where.' FOR UPDATE');
        if(!$rows || db_num_rows($rows)!==1)return false;$before=db_fetch_assoc($rows);
        if(!db_query('UPDATE '.TB_PREF.'stock_master SET inactive='.(int)$status.' WHERE '.$where))return false;
        $rows=db_query('SELECT '.$columns.' FROM '.TB_PREF.'stock_master WHERE '.$where.' FOR UPDATE');
        if(!$rows || db_num_rows($rows)!==1 || dimension_financial_reference_context()!==$context)return false;$after=db_fetch_assoc($rows);
        foreach(array('stock_id','dimension_id','dimension2_id','custom_data') as $field)
            if(!array_key_exists($field,$before) || !array_key_exists($field,$after) || $before[$field]!==$after[$field])return false;
        return (int)$after['inactive']===(int)$status;
    },true);
}
