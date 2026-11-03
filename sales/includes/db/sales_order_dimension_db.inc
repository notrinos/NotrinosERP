<?php
/** Native order/quotation retained-reference custody, including derived writers. */
require_once __DIR__.'/../../../dimensions/includes/dimension_financial_reference_writer.inc';
require_once __DIR__.'/sales_order_source_db.inc';
require_once __DIR__.'/../../../includes/approval/approval_number.inc';

/** Read the complete current order group using its native composite identity. */
function sales_order_dimension_rows($table, $number, $type)
{
    if (!in_array($table, array('sales_orders','sales_order_details'), true)) return false;
    $rows = db_query('SELECT * FROM '.TB_PREF.$table.' WHERE order_no='.$number
        .' AND trans_type='.$type.' ORDER BY '.($table === 'sales_orders' ? 'order_no' : 'id').' FOR UPDATE');
    if (!$rows) return false;
    $result = array();
    while ($row = db_fetch_assoc($rows)) $result[] = $row;
    return $result;
}

/**
 * Preserve the native whole-command transaction and failure signal. Admission
 * covers every current child before replacement/cascade and checks exact final
 * metadata after hooks. Callers still own business authorization and approval.
 * The native callback receives the locked header's merged discount JSON.
 */
function sales_order_dimension_command($number, $type, $operation, $persist, $order = null)
{
    $context = dimension_financial_reference_context();
    $number = dimension_writer_reference_id($number);
    $type = dimension_writer_reference_id($type);
    if (!$context || $number === false || !in_array($type, array(ST_SALESORDER,ST_SALESQUOTE), true)
        || !in_array($operation, array('add','update','delete','maintain'), true)
        || !is_callable($persist) || ($operation !== 'add' && !$number)) dimension_financial_reference_failure();
    try {
        if (!begin_transaction()) dimension_financial_reference_failure();
        // A missing retained store is unknown even for an empty/new document.
        foreach (array('sales_orders','sales_order_details') as $table)
            if (!db_query('SELECT custom_data FROM '.TB_PREF.$table.' LIMIT 0')) dimension_financial_reference_failure();
        $source_state=sales_order_source_admission($number,$type,$operation,$order);
        $headers = $operation === 'add' ? array() : sales_order_dimension_rows('sales_orders',$number,$type);
        $lines = $operation === 'add' ? array() : sales_order_dimension_rows('sales_order_details',$number,$type);
        if ($headers === false || $lines === false || ($operation !== 'add' && count($headers) !== 1)) dimension_financial_reference_failure();
        if($operation!=='add' && array_key_exists('id',$source_state) && (int)$headers[0]['agreement_id']!==$source_state['id'])dimension_financial_reference_failure();
        $payload = $headers ? $headers[0]['custom_data'] : null;
        $inheritance = $operation === 'add' && function_exists('agreement_sales_order_native_metadata')
            ? agreement_sales_order_native_metadata($order) : array();
        if ($inheritance === false) dimension_financial_reference_failure();
        if (!$inheritance && isset($source_state['inheritance'])) $inheritance=$source_state['inheritance'];
        $line_payloads = $inheritance ? $inheritance['line_payloads'] : array();
        if ($inheritance && empty($inheritance['preserve_header'])) $payload = $inheritance['header_payload'];
        elseif ($operation==='add') {
            $d1=dimension_writer_reference_id(isset($order->dimension_id)?$order->dimension_id:0);$d2=dimension_writer_reference_id(isset($order->dimension2_id)?$order->dimension2_id:0);
            if($d1===false || $d2===false)dimension_financial_reference_failure();
            $payload=json_encode(array('dimension_id'=>$d1,'dimension2_id'=>$d2));
        }
        if ($operation === 'add' || $operation === 'update') {
            if (!is_object($order) || !is_array($order->line_items)) dimension_financial_reference_failure();
            if ($operation === 'update' && ((string)current($order->trans_no) !== (string)$headers[0]['version'])) dimension_financial_reference_failure();
            $payload = json_encode(get_discount_custom_data_for_save($payload, $order));
            if (!is_string($payload)) dimension_financial_reference_failure();
        }
        $kept = array();
        if ($operation === 'update') foreach ($order->line_items as $line) {
            $id = dimension_writer_reference_id($line->id);
            if ($id === false || ($id && isset($kept[$id]))) dimension_financial_reference_failure();
            if ($id) $kept[$id] = $id;
        }
        $masters = array();
        $new_lines = array();
        foreach ($line_payloads as $raw) $new_lines[] = array('custom_data'=>$raw);
        foreach (array_merge($headers,$lines,$new_lines,array(array('custom_data'=>$payload))) as $row) {
            if (!array_key_exists('custom_data',$row)) dimension_financial_reference_failure();
            $refs = dimension_master_payload_references($row['custom_data']);
            if (is_array($refs)) foreach ($refs as $reference) $masters[$reference[0]] = $reference[0];
        }
        sort($masters,SORT_NUMERIC);
        foreach ($masters as $master)
            if (!db_query('SELECT id FROM '.TB_PREF.'dimensions WHERE id='.$master.' FOR UPDATE')) dimension_financial_reference_failure();
        foreach($line_payloads as $index=>$raw) if($operation==='add' || empty($order->line_items[$index]->id)) {
            $transition=dimension_master_payload_transition(null,$raw);if($transition===false)dimension_financial_reference_failure();
            foreach($transition['new'] as $ref){$rows=db_query('SELECT type_,closed FROM '.TB_PREF.'dimensions WHERE id='.$ref[0].' FOR UPDATE');$master=$rows?db_fetch_assoc($rows):false;if(!$master || (int)$master['closed'] || (int)$master['type_']!==$ref[1])dimension_financial_reference_failure();}
        }
        $expected_lines = array();
        foreach ($lines as $line) {
            $remove = $operation === 'delete' || ($operation === 'update' && !isset($kept[$line['id']]));
            if (!dimension_ordinary_master_write('sales_order_details',$line['id'],array(),function(){return true;},true,$remove)) dimension_financial_reference_failure();
            if (!$remove) $expected_lines[$line['id']] = $line['custom_data'];
            unset($kept[$line['id']]);
        }
        // A caller-supplied line ID cannot steal another order/quotation's row.
        if ($kept) dimension_financial_reference_failure();
        $key = $operation === 'add' ? null : array('order_no'=>$number,'trans_type'=>$type);
        $next = $operation === 'delete' ? array() : array('custom_data'=>$payload);
        $result = dimension_ordinary_master_write('sales_orders',$key,$next,
            function($admitted) use ($persist,$payload,$operation,$number,$type,$expected_lines,$context,$line_payloads,$inheritance,$order,$source_state) {
                $result = call_user_func($persist,$payload,$line_payloads,$inheritance);
                if (!$result || dimension_financial_reference_context() !== $context) return false;
                $id = $operation === 'add' ? dimension_writer_reference_id($result) : $number;
                if (!$id) return false;
                $after = sales_order_dimension_rows('sales_orders',$id,$type);
                $children = sales_order_dimension_rows('sales_order_details',$id,$type);
                if ($after === false || $children === false) return false;
                if ($operation === 'delete') return !$after && !$children && verify_sales_order_source_admission($source_state,$id,$order) ? $result : false;
                if (count($after) !== 1 || $after[0]['custom_data'] !== $payload) return false;
                $new_expected = array();
                if ($line_payloads) foreach ($line_payloads as $index=>$raw) {
                    if (!isset($order->line_items[$index]) || !($line_id=dimension_writer_reference_id($order->line_items[$index]->id))
                        || isset($new_expected[$line_id])) return false;
                    if (!array_key_exists($line_id,$expected_lines)) $new_expected[$line_id] = $raw;
                }
                foreach ($children as $line) {
                    if (!array_key_exists('custom_data',$line)) return false;
                    if (array_key_exists($line['id'],$expected_lines)) {
                        if ($line['custom_data'] !== $expected_lines[$line['id']]) return false;
                        unset($expected_lines[$line['id']]);
                    } elseif ($inheritance || $line_payloads) {
                        if (!array_key_exists($line['id'],$new_expected) || $line['custom_data'] !== $new_expected[$line['id']]) return false;
                        unset($new_expected[$line['id']]);
                    } elseif ($operation === 'maintain' || $line['custom_data'] !== '{}') return false;
                }
                return !$expected_lines && !$new_expected && verify_sales_order_source_admission($source_state,$id,$order) ? $result : false;
            },true,$operation === 'delete');
        if (!$result || dimension_financial_reference_context() !== $context || !commit_transaction()) dimension_financial_reference_failure();
        return $result;
    } catch (DimensionFinancialReferenceWriteException $exception) { throw $exception;
    } catch (Exception $exception) { dimension_financial_reference_failure();
    } catch (Throwable $exception) { dimension_financial_reference_failure(); }
}

/** Resolve only a hint, then recheck a child under its actual parent lock. */
function sales_order_dimension_line_command($line_id, $persist)
{
    $id = dimension_writer_reference_id($line_id);
    if (!$id || !is_callable($persist)) dimension_financial_reference_failure();
    $rows = db_query('SELECT order_no,trans_type FROM '.TB_PREF.'sales_order_details WHERE id='.$id);
    $hint = $rows ? db_fetch_assoc($rows) : false;
    if (!$hint) dimension_financial_reference_failure();
    return sales_order_dimension_command($hint['order_no'],$hint['trans_type'],'maintain',
        function() use ($id,$hint,$persist) {
            $rows = db_query('SELECT order_no,trans_type FROM '.TB_PREF.'sales_order_details WHERE id='.$id.' FOR UPDATE');
            $current = $rows ? db_fetch_assoc($rows) : false;
            if ($current !== $hint) return false;
            return call_user_func($persist);
        });
}

/** Preserve every actual order/ledger target of the native allocation clear. */
function sales_order_dimension_allocation_clear($type, $number, $person, $persist)
{
    $context = dimension_financial_reference_context();
    $type = dimension_writer_reference_id($type);
    $number = dimension_writer_reference_id($number);
    if (!$context || $type === false || !$number || !is_callable($persist)) dimension_financial_reference_failure();
    try {
        if (!begin_transaction()) dimension_financial_reference_failure();
        $where = '((trans_type_from='.$type.' AND trans_no_from='.$number.') OR (trans_type_to='.$type.' AND trans_no_to='.$number.'))';
        if ($person) $where .= ' AND person_id='.db_escape($person);
        $rows = db_query('SELECT * FROM '.TB_PREF.'cust_allocations WHERE '.$where.' ORDER BY id FOR UPDATE');
        if (!$rows) dimension_financial_reference_failure();
        $targets = array();
        while ($row = db_fetch_assoc($rows)) {
            foreach (array('from','to') as $side) {
                $target_type = dimension_writer_reference_id($row['trans_type_'.$side]);
                $target_no = dimension_writer_reference_id($row['trans_no_'.$side]);
                if ($target_type === false || !$target_no) dimension_financial_reference_failure();
                $targets['debtor_trans/'.$target_type.'/'.$target_no.'/'.$row['person_id']] = array('debtor_trans',$target_type,$target_no,$row['person_id']);
            }
            // Native clear updates the order target only, never the order source.
            $targets['sales_orders/'.$row['trans_type_to'].'/'.$row['trans_no_to'].'/'.$row['person_id']] = array('sales_orders',(int)$row['trans_type_to'],(int)$row['trans_no_to'],$row['person_id']);
        }
        ksort($targets,SORT_STRING);
        $command = $persist;
        foreach (array_reverse($targets) as $target) {
            list($table,$target_type,$target_no,$customer) = $target;
            if ($table === 'sales_orders' && !in_array($target_type,array(ST_SALESORDER,ST_SALESQUOTE),true)) continue;
            // An absent LEFT JOIN target is not written by the native SQL.
            $rows = db_query('SELECT '.($table === 'sales_orders' ? 'order_no' : 'trans_no').' FROM '.TB_PREF.$table
                .' WHERE '.($table === 'sales_orders' ? 'trans_type' : 'type').'='.$target_type
                .' AND '.($table === 'sales_orders' ? 'order_no' : 'trans_no').'='.$target_no.' AND debtor_no='.db_escape($customer).' FOR UPDATE');
            if (!$rows) dimension_financial_reference_failure();
            if (!db_fetch_assoc($rows)) continue;
            $previous = $command;
            $command = function() use ($table,$target_type,$target_no,$customer,$previous) {
                return $table === 'sales_orders'
                    ? sales_order_dimension_command($target_no,$target_type,'maintain',$previous)
                    : dimension_financial_reference_write('debtor_trans',array('type'=>$target_type,'trans_no'=>$target_no,'debtor_no'=>$customer),array(),$previous);
            };
        }
        $result = call_user_func($command);
        if (!$result || dimension_financial_reference_context() !== $context || !commit_transaction()) dimension_financial_reference_failure();
        return $result;
    } catch (DimensionFinancialReferenceWriteException $exception) { throw $exception;
    } catch (Exception $exception) { dimension_financial_reference_failure();
    } catch (Throwable $exception) { dimension_financial_reference_failure(); }
}
