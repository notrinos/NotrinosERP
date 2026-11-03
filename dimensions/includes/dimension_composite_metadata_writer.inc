<?php
/** Retained metadata custody for the two native composite inventory catalogs. */
require_once __DIR__.'/dimension_financial_reference_writer.inc';

/** Fixed canonical primary columns; BOM's numeric id is not unique. */
function dimension_composite_metadata_columns($owner)
{
    if ($owner === 'bom') return array('parent','component','workcentre_added','loc_code');
    if ($owner === 'purch_data') return array('supplier_id','stock_id');
    return false;
}

/** Preserve native text identities and normalize only actual integer key fields. */
function dimension_composite_metadata_key($owner,$values)
{
    $columns=dimension_composite_metadata_columns($owner);$key=array();
    if (!$columns || !is_array($values)) return false;
    foreach ($columns as $column) {
        if (!array_key_exists($column,$values)) return false;
        $value=$values[$column];
        if ($column==='supplier_id' || $column==='workcentre_added') $value=dimension_writer_reference_id($value);
        elseif (!is_string($value)) return false;
        if ($value===false) return false;
        $key[$column]=$value;
    }
    return $key;
}

/** Construct predicates solely from the fixed native key declaration. */
function dimension_composite_metadata_predicate($owner,$values)
{
    $key=dimension_composite_metadata_key($owner,$values);if ($key===false) return false;
    $parts=array();foreach ($key as $column=>$value) $parts[]='`'.$column.'`='.db_escape($value);
    return implode(' AND ',$parts);
}

/** Lock the actual canonical row, never treating an unavailable source as absence. */
function dimension_composite_metadata_rows($owner,$key)
{
    $where=dimension_composite_metadata_predicate($owner,$key);if ($where===false) return false;
    return db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE '.$where.' FOR UPDATE');
}

/** Admit and verify one native create/change/remove, including explicit BOM relocation. */
function dimension_composite_metadata_write($owner,$key,$next,$persist,$remove=false,$final_key=null)
{
    $context=dimension_financial_reference_context();$columns=dimension_composite_metadata_columns($owner);
    if (!$context || !$columns || !is_array($next) || !is_callable($persist)) return false;
    if ($key!==null && ($key=dimension_composite_metadata_key($owner,$key))===false) return false;
    $final=$final_key!==null?$final_key:($key===null?$next:$key);
    if (($final=dimension_composite_metadata_key($owner,$final))===false) return false;
    try {
        if (!begin_transaction()) return false;
        if (!db_query('SELECT custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) {cancel_transaction();return false;}
        $before=null;
        if ($key!==null) {
            $rows=dimension_composite_metadata_rows($owner,$key);
            if (!$rows || db_num_rows($rows)!==1) {cancel_transaction();return false;}
            $before=db_fetch_assoc($rows);
        }
        $targets=dimension_composite_metadata_rows($owner,$final);
        if (!$targets || db_num_rows($targets)>1) {cancel_transaction();return false;}
        $target=db_num_rows($targets)?db_fetch_assoc($targets):false;
        if ($target && (!$before || dimension_composite_metadata_key($owner,$target)!==dimension_composite_metadata_key($owner,$before))) {cancel_transaction();return false;}
        $insert_id=0;
        $result=dimension_ordinary_master_write($owner,$key,$next,function($admitted)use($owner,$key,$final,$before,$persist,$remove,$context,&$insert_id){
            $result=call_user_func($persist,$admitted);
            if (!$result || dimension_financial_reference_context()!==$context) return false;
            if ($key===null && $owner==='bom') $insert_id=db_insert_id();
            $rows=dimension_composite_metadata_rows($owner,$final);
            if (!$rows) return false;
            if ($remove) return db_num_rows($rows)===0?$result:false;
            if (db_num_rows($rows)!==1) return false;
            $after=db_fetch_assoc($rows);
            $expected=array_key_exists('custom_data',$admitted)?$admitted['custom_data']:($before?$before['custom_data']:'{}');
            if (!array_key_exists('custom_data',$after) || $after['custom_data']!==$expected) return false;
            $expected_key=$before && $final===$key?dimension_composite_metadata_key($owner,$before):$final;
            if (dimension_composite_metadata_key($owner,$after)!==$expected_key) return false;
            if ($owner==='bom' && $before && $after['id']!==$before['id']) return false;
            if ($before && $final!==$key) {
                $old=dimension_composite_metadata_rows($owner,$key);if (!$old) return false;
                if (db_num_rows($old)) {
                    $old_row=db_fetch_assoc($old);
                    if (db_num_rows($old)!==1 || dimension_composite_metadata_key($owner,$old_row)!==dimension_composite_metadata_key($owner,$after)) return false;
                }
            }
            return $result;
        },true,$remove);
        if (!$result || dimension_financial_reference_context()!==$context || !commit_transaction()) {cancel_transaction();return false;}
        if ($key===null && $owner==='bom' && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $e) {cancel_transaction();return false;} catch (Throwable $e) {cancel_transaction();return false;}
}
