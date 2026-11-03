<?php
/** Fixed ordinary master metadata custody using the existing writer fence and transaction manager. */
require_once __DIR__.'/dimension_financial_reference_writer.inc';

/** Only these native key/metadata contracts are admitted here; no caller-selected schema. */
function dimension_master_metadata_sources(){return array('areas'=>'area_code','tax_types'=>'id','chart_types'=>'id','chart_class'=>'cid','chart_master'=>'account_code','sales_pos'=>'id','sales_types'=>'id','salesman'=>'salesman_code','prices'=>'id','item_codes'=>'id');}

/** Read one exact current native key without case-folding or numeric truncation. */
function dimension_master_metadata_row($owner,$id)
{
    $sources=dimension_master_metadata_sources();if(!isset($sources[$owner]) || ($id=dimension_writer_master_id($owner,$id))===false || $id===0)return false;
    $rows=db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE '.$sources[$owner].'='.db_escape($id).' FOR UPDATE');
    if(!$rows || db_num_rows($rows)!==1)return false;$row=db_fetch_assoc($rows);
    return array_key_exists('custom_data',$row) && (string)$row[$sources[$owner]]===(string)$id?$row:false;
}

/** Hold all known retained masters before a native hierarchy command can update siblings. */
function dimension_master_metadata_lock_rows($rows)
{
    $locks=array();foreach($rows as $row){if(!array_key_exists('custom_data',$row))return false;$refs=dimension_master_payload_references($row['custom_data']);if(is_array($refs))foreach($refs as $ref)$locks[$ref[0]]=$ref[0];}
    sort($locks,SORT_NUMERIC);foreach($locks as $id)if(!db_query('SELECT id FROM '.TB_PREF.'dimensions WHERE id='.$id.' FOR UPDATE'))return false;return true;
}

/** Preserve native SQL/return values while checking raw metadata and the actual final identity. */
function dimension_master_metadata_write($owner,$key,$next,$persist,$remove=false,$final_key=null)
{
    $sources=dimension_master_metadata_sources();$context=dimension_financial_reference_context();
    if(!$context || !isset($sources[$owner]) || !is_array($next) || !is_callable($persist))return false;
    if($key!==null && (($key=dimension_writer_master_id($owner,$key))===false || $key===0))return false;
    if($final_key!==null && (($final_key=dimension_writer_master_id($owner,$final_key))===false || $final_key===0))return false;
    try{
        if(!begin_transaction())return false;
        if(!db_query('SELECT custom_data FROM '.TB_PREF.$owner.' LIMIT 0')){cancel_transaction();return false;}
        $before=$key===null?null:dimension_master_metadata_row($owner,$key);if($key!==null && !$before){cancel_transaction();return false;}
        $insert_id=0;$result=dimension_ordinary_master_write($owner,$key,$next,function($admitted)use($owner,$sources,$key,$before,$persist,$remove,$final_key,$context,&$insert_id){
            $result=call_user_func($persist,$admitted);if(!$result || dimension_financial_reference_context()!==$context)return false;
            $identity=$final_key!==null?$final_key:($key!==null?$key:(array_key_exists($sources[$owner],$admitted)?$admitted[$sources[$owner]]:db_insert_id()));
            if($key===null)$insert_id=$identity;$after=dimension_master_metadata_row($owner,$identity);
            if($remove){$rows=db_query('SELECT '.$sources[$owner].',custom_data FROM '.TB_PREF.$owner.' WHERE '.$sources[$owner].'='.db_escape($identity).' FOR UPDATE');return $rows && db_num_rows($rows)===0?$result:false;}
            $expected=array_key_exists('custom_data',$admitted)?$admitted['custom_data']:($before?$before['custom_data']:'{}');
            if(!$after || $after['custom_data']!==$expected)return false;
            if($key!==null && $final_key!==null && (string)$key!==(string)$final_key && dimension_master_metadata_row($owner,$key)!==false)return false;
            if($key===null && !in_array($owner,array('chart_master','chart_types','chart_class'),true) && !db_preserve_insert_id($insert_id))return false;
            return $result;
        },true,$remove);
        if(!$result || dimension_financial_reference_context()!==$context || !commit_transaction()){cancel_transaction();return false;}
        if($key===null && !in_array($owner,array('chart_master','chart_types','chart_class'),true) && !db_preserve_insert_id($insert_id))return false;
        return $result;
    }catch(Exception $e){cancel_transaction();return false;}catch(Throwable $e){cancel_transaction();return false;}
}
