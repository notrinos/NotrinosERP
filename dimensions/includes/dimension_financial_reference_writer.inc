<?php
/** Ordinary financial reference custody; business authorization remains with native owners. */
require_once __DIR__.'/dimension_reference_writer_fence.inc';

/** A denied ledger leg must unwind callers which historically ignored numeric/void returns. */
class DimensionFinancialReferenceWriteException extends RuntimeException {}

/** Bind an ordinary command to the current company, actor and actual connection. */
function dimension_financial_reference_context()
{
    global $db, $db_connections;
    $user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
    if (!is_object($user) || !isset($user->cur_con,$db_connections[$user->cur_con]) || !$db) return false;
    return array($user->cur_con,$db_connections[$user->cur_con],isset($user->user)?$user->user:null,
        isset($user->access)?$user->access:null,is_object($db)?spl_object_hash($db):$db);
}

/** Stop the native command after the existing transaction manager releases its resources. */
function dimension_financial_reference_failure()
{
    cancel_transaction();
    throw new DimensionFinancialReferenceWriteException(_('Financial classification could not be verified. No transaction changes were saved.'));
}

/** Fixed intrinsic row identity; customer/supplier headers include their actual party key. */
function dimension_financial_reference_predicate($owner,$key)
{
    $keys = array('gl_trans'=>'counter','bank_trans'=>'id','supp_invoice_items'=>'id','debtor_trans_details'=>'id','stock_moves'=>'trans_id');
    if (in_array($owner,array('debtor_trans','supp_trans'),true)) {
        $party=$owner==='debtor_trans'?'debtor_no':'supplier_id';
        $legacy=$owner==='debtor_trans' && is_array($key) && count($key)===2;
        if(!is_array($key) || !isset($key['type'],$key['trans_no']) || count($key)!==($legacy?2:3)
            || ($type=dimension_writer_reference_id($key['type']))===false || !($id=dimension_writer_reference_id($key['trans_no'])))return false;
        if(!$legacy && (!isset($key[$party]) || !($party_id=dimension_writer_reference_id($key[$party]))))return false;
        return 'type='.$type.' AND trans_no='.$id.($legacy?'':' AND '.$party.'='.$party_id);
    }
    if (!isset($keys[$owner]) || !($id=dimension_writer_reference_id($key))) return false;
    return $keys[$owner].'='.$id;
}

/** Read one current exact owner; a legacy two-field customer lookup must be unambiguous. */
function dimension_financial_reference_row($owner,$key)
{
    $where=dimension_financial_reference_predicate($owner,$key);
    if ($where===false) return false;
    $rows=db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE '.$where.' FOR UPDATE');
    return $rows && db_num_rows($rows)===1 ? db_fetch_assoc($rows) : false;
}

/**
 * Keep native SQL, return values and allocation/audit ownership at the caller.
 * Admission and final reread retain both scalar axes and raw custom_data.
 * The callback returns its inserted identity or a true update/remove outcome.
 */
function dimension_financial_reference_write($owner,$key,$next,$persist,$remove=false,$historical=array())
{
    $context=dimension_financial_reference_context();
    if (!$context || !is_array($next) || !is_callable($persist)
        || !in_array($owner,array('gl_trans','bank_trans','debtor_trans','supp_invoice_items','supp_trans','debtor_trans_details','stock_moves'),true)) dimension_financial_reference_failure();
    try {
        if (!begin_transaction()) dimension_financial_reference_failure();
        $before=$key===null ? null : dimension_financial_reference_row($owner,$key);
        if ($key!==null && !$before) dimension_financial_reference_failure();
        if($owner==='debtor_trans' && $key!==null && count($key)===2)$key['debtor_no']=$before['debtor_no'];
        if ($remove && (!empty($before['dimension_id']) || !empty($before['dimension2_id']))) dimension_financial_reference_failure();
        $result=dimension_ordinary_master_write($owner,$key,$next,
            function($admitted) use ($owner,$key,$before,$persist,$context,$remove) {
                $result=call_user_func($persist,$admitted);
                if (!$result || dimension_financial_reference_context()!==$context) return false;
                $identity=$key===null ? $result : $key;
                $after=dimension_financial_reference_row($owner,$identity);
                if ($remove) return $after===false ? $result : false;
                if (!$after) return false;
                $expected=$admitted+($before?$before:array('dimension_id'=>0,'dimension2_id'=>0,'custom_data'=>'{}'));
                foreach (in_array($owner,array('supp_trans','debtor_trans_details','stock_moves'),true)?array('custom_data'):array('dimension_id','dimension2_id','custom_data') as $field) {
                    $value=array_key_exists($field,$expected)?$expected[$field]:($field==='custom_data'?'{}':0);
                    if ($value===null ? $after[$field]!==null : (string)$after[$field] !== (string)$value) return false;
                }
                if ($key===null && !in_array($owner,array('debtor_trans','supp_trans'),true) && !db_preserve_insert_id($result)) return false;
                return $result;
            },true,$remove,$owner==='gl_trans' && $key===null ? $historical : array());
        if (!$result || dimension_financial_reference_context()!==$context || !commit_transaction()) dimension_financial_reference_failure();
        if ($key===null && !in_array($owner,array('debtor_trans','supp_trans'),true) && !db_preserve_insert_id($result)) dimension_financial_reference_failure();
        return $result;
    } catch (DimensionFinancialReferenceWriteException $exception) { throw $exception;
    } catch (Exception $exception) { dimension_financial_reference_failure();
    } catch (Throwable $exception) { dimension_financial_reference_failure(); }
}

/** Lock all current rows and references before a native group void/remove. */
function dimension_financial_reference_group($owner,$type,$number,$persist,$remove=false,$complete=null)
{
    $context=dimension_financial_reference_context();
    $columns=array('gl_trans'=>array('type','type_no','counter'),'bank_trans'=>array('type','trans_no','id'),
        'supp_invoice_items'=>array('supp_trans_type','supp_trans_no','id'),'debtor_trans'=>array('type','trans_no','trans_no'),'supp_trans'=>array('type','trans_no','trans_no'),
'debtor_trans_details'=>array('debtor_trans_type','debtor_trans_no','id'),'stock_moves'=>array('type','trans_no','trans_id'));
    if (!$context || !isset($columns[$owner]) || ($type=dimension_writer_reference_id($type))===false
        || !($number=dimension_writer_reference_id($number)) || !is_callable($persist) || ($complete!==null && !is_callable($complete))) dimension_financial_reference_failure();
    try {
        if (!begin_transaction()) dimension_financial_reference_failure();
        $column=$columns[$owner];$predicate=$column[0].'='.$type.' AND '.$column[1].'='.$number;
        $scalars=!in_array($owner,array('supp_trans','debtor_trans_details','stock_moves'),true);
        if (!db_query('SELECT '.($scalars?'dimension_id,dimension2_id,':'').'custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) dimension_financial_reference_failure();
        $rows=db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE '.$predicate.' ORDER BY '.$column[2].' FOR UPDATE');
        if (!$rows) dimension_financial_reference_failure();
        $source=array();$masters=array();
        while ($row=db_fetch_assoc($rows)) {
            $source[]=$row;
            foreach($scalars?array('dimension_id','dimension2_id'):array() as $field) {
                if (!array_key_exists($field,$row) || ($id=dimension_writer_reference_id($row[$field]))===false) dimension_financial_reference_failure();
                if($id)$masters[$id]=$id;
            }
            if(!array_key_exists('custom_data',$row))dimension_financial_reference_failure();
            $references=dimension_master_payload_references($row['custom_data']);
            if(is_array($references))foreach($references as $reference)$masters[$reference[0]]=$reference[0];
        }
        sort($masters,SORT_NUMERIC);
        foreach($masters as $id)if(!db_query('SELECT id FROM '.TB_PREF.'dimensions WHERE id='.$id.' FOR UPDATE'))dimension_financial_reference_failure();
        // Admit every sibling under the existing fence before the native command can clear any row.
        foreach($source as $row){
            $key=in_array($owner,array('debtor_trans','supp_trans'),true)?array('type'=>$type,'trans_no'=>$number):$row[$column[2]];
            if($owner==='supp_trans')$key['supplier_id']=$row['supplier_id'];
            if($owner==='debtor_trans')$key['debtor_no']=$row['debtor_no'];
            if(!dimension_ordinary_master_write($owner,$key,array(),function($admitted){return true;},true,$remove))dimension_financial_reference_failure();
        }
        foreach($source as $row)if(!call_user_func($persist,$row))dimension_financial_reference_failure();
        if($complete!==null && !call_user_func($complete))dimension_financial_reference_failure();
        // Later callbacks and the native completion leg must not rewrite an earlier retained row.
        $expected=array();$fields=$scalars?array('dimension_id','dimension2_id','custom_data'):array('custom_data');
        if(!$remove)foreach($source as $row){$coordinate=$row[$column[2]].($owner==='supp_trans'?'/'.$row['supplier_id']:($owner==='debtor_trans'?'/'.$row['debtor_no']:''));$expected[$coordinate]=array_intersect_key($row,array_flip($fields));}
        $final=db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE '.$predicate.' ORDER BY '.$column[2].' FOR UPDATE');if(!$final)dimension_financial_reference_failure();
        while($row=db_fetch_assoc($final)){$coordinate=$row[$column[2]].($owner==='supp_trans'?'/'.$row['supplier_id']:($owner==='debtor_trans'?'/'.$row['debtor_no']:''));
            if(!isset($expected[$coordinate]) || $expected[$coordinate]!==array_intersect_key($row,array_flip($fields)))dimension_financial_reference_failure();unset($expected[$coordinate]);}
        if($expected || dimension_financial_reference_context()!==$context || !commit_transaction())dimension_financial_reference_failure();
        return true;
    } catch(DimensionFinancialReferenceWriteException $exception){throw $exception;
    } catch(Exception $exception){dimension_financial_reference_failure();
    } catch(Throwable $exception){dimension_financial_reference_failure();}
}
