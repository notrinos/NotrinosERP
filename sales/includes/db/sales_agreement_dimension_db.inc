<?php
/** Sales-agreement reference custody shared by native and generic writers. */
require_once __DIR__.'/../../../dimensions/includes/dimension_reference_writer_fence.inc';
require_once __DIR__.'/../../../dimensions/includes/dimension_financial_reference_writer.inc';

/** Bind maintenance to the authenticated actor and active company prefix. */
function sales_agreement_dimension_context()
{
    global $db_connections;
    if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])) return false;
    $user = $_SESSION['wa_current_user'];
    if (!isset($user->user, $user->cur_con) || !dimension_writer_reference_id($user->user)
        || !isset($db_connections[$user->cur_con]['tbpref'])
        || !is_string($db_connections[$user->cur_con]['tbpref'])
        || !method_exists($user, 'can_access') || !$user->can_access('SA_SALESAGREEMENT')) return false;
    return array($user->user, $user->cur_con, $db_connections[$user->cur_con]['tbpref']);
}

/**
 * Serialize draft maintenance parent-first, then retain scalar/JSON references
 * through the existing ordinary fence. A child lookup is only a parent hint;
 * its identity is rechecked after the parent lock. No workflow status, approval
 * decision, fulfillment quantity or generated identity is authored here.
 */
function sales_agreement_dimension_write($owner, $key, $next, $persist, $remove = false)
{
    $context = sales_agreement_dimension_context();
    if (!$context || !in_array($owner, array('sales_agreements','sales_agreement_lines'), true)
        || !is_array($next) || !is_callable($persist)) return false;
    $identity = $key === null ? null : dimension_writer_reference_id($key);
    if ($identity === false || $identity === 0) return false;
    $header = $owner === 'sales_agreements';
    try {
        if (!begin_transaction()) return false;
        foreach(array('sales_agreements','sales_agreement_lines') as $store)
            if(!db_query('SELECT custom_data FROM '.TB_PREF.$store.' LIMIT 0')){cancel_transaction();return false;}
        $draft_rows = db_query('SELECT draft_data,status FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_SALESAGREEMENT.' ORDER BY id FOR UPDATE');
        if (!$draft_rows) { cancel_transaction(); return false; }
        $drafts = array();
        while ($draft = db_fetch($draft_rows)) $drafts[] = $draft;
        $parent = $header ? $identity : (isset($next['agreement_id']) ? dimension_writer_reference_id($next['agreement_id']) : false);
        if (!$header && $identity !== null) {
            $rows = db_query('SELECT agreement_id FROM '.TB_PREF.$owner.' WHERE id='.$identity);
            $hint = $rows ? db_fetch($rows) : false;
            $parent = $hint ? dimension_writer_reference_id($hint['agreement_id']) : false;
        }
        if (!$header && !$parent) { cancel_transaction(); return false; }
        if ($parent !== null) {
            $rows = db_query('SELECT * FROM '.TB_PREF.'sales_agreements WHERE id='.$parent.' FOR UPDATE');
            $current = $rows ? db_fetch($rows) : false;
            if (!$current || $current['status'] !== 'draft') { cancel_transaction(); return false; }
            // Pending/approved commands retain the current source. Terminal
            // rejected/cancelled snapshots allow edits but never source erasure.
            foreach ($drafts as $draft) {
                if (!function_exists('get_approval_workflow_service')) { cancel_transaction(); return false; }
                $data = get_approval_workflow_service()->decodeStoredDraftData($draft['draft_data']);
                if (!is_array($data) || !isset($data['agreement_id'])
                    || !dimension_writer_reference_id($data['agreement_id'])
                    || ((int)$data['agreement_id'] === $parent
                        && ($remove || !in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)))) {
                    cancel_transaction(); return false;
                }
            }
            $rows = db_query('SELECT order_no FROM '.TB_PREF.'sales_orders WHERE agreement_id='.$parent.' AND trans_type='.ST_SALESORDER.' ORDER BY order_no FOR UPDATE');
            if (!$rows || db_fetch($rows)) { cancel_transaction(); return false; }
            $rows = db_query('SELECT * FROM '.TB_PREF.'sales_agreement_lines WHERE agreement_id='.$parent.' ORDER BY id FOR UPDATE');
            if (!$rows) { cancel_transaction(); return false; }
            $found = false; $children = array();
            while ($line = db_fetch($rows)) {
                $children[] = $line;
                if (!$header && (int)$line['id'] === $identity) $found = $line;
            }
            if (!$header && $identity !== null && !$found) { cancel_transaction(); return false; }
            if (!$header && isset($next['agreement_id']) && dimension_writer_reference_id($next['agreement_id']) !== $parent) {
                cancel_transaction(); return false;
            }
            if ($header && $remove) foreach ($children as $line) {
                if (!dimension_ordinary_master_write('sales_agreement_lines', $line['id'], array(),
                    function($admitted) { return true; }, true, true)) { cancel_transaction(); return false; }
            }
        }
        $result = dimension_ordinary_master_write($owner, $identity, $next,
            function($admitted) use ($persist, $context, $owner, $identity, $header, $remove, $parent) {
                $before = null;
                if ($identity !== null) {
                    $rows = db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE id='.$identity.' FOR UPDATE');
                    $before = $rows ? db_fetch($rows) : false;
                    if (!$before) return false;
                }
                $result = call_user_func($persist, $admitted);
                if (!$result || sales_agreement_dimension_context() !== $context) return false;
                $id = $identity === null ? dimension_writer_reference_id($result) : $identity;
                if (!$id) return false;
                $rows = db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE id='.$id.' FOR UPDATE');
                if (!$rows) return false;
                $after = db_fetch($rows);
                if ($remove) return $after === false ? $result : false;
                if (!$after || ($header ? $after['status'] !== 'draft' : (int)$after['agreement_id'] !== $parent)) return false;
                $expected = $admitted + ($before ? $before : array());
                foreach ($header ? array('dimension_id','dimension2_id','custom_data') : array('custom_data') as $field) {
                    $value = array_key_exists($field, $expected) ? $expected[$field] : ($field === 'custom_data' ? null : 0);
                    if ($value === null ? $after[$field] !== null : (string)$after[$field] !== (string)$value) return false;
                }
                if ($identity === null && !db_preserve_insert_id($id)) return false;
                return $result;
            }, true, $remove);
        $insert_id = $result && $identity === null ? db_insert_id() : 0;
        if (!$result || sales_agreement_dimension_context() !== $context || !commit_transaction()) {
            cancel_transaction(); return false;
        }
        if ($identity === null && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/** Route generic/import models through the same validated native draft commands. */
function sales_agreement_dimension_entity_write($owner, $operation, $key, $next, $validate)
{
    if (!is_array($next) || !is_callable($validate)
        || !in_array($operation, array('insert','update','delete'), true)) return false;
    if (!function_exists('add_sales_agreement')) require_once __DIR__.'/sales_agreement_db.inc';
    $header = $owner === 'sales_agreements';
    if (!$header && $owner !== 'sales_agreement_lines') return false;
    $defaults = $header ? array('agreement_type'=>'blanket_order','debtor_no'=>0,'branch_code'=>0,'date_start'=>null,'date_end'=>null,
        'currency'=>'','payment_terms'=>0,'auto_renew'=>0,'renewal_period_months'=>12,
        'terms_and_conditions'=>'','notes'=>'','dimension_id'=>0,'dimension2_id'=>0,
        'reference'=>'','salesman_id'=>0,'custom_data'=>null)
        : array('agreement_id'=>0,'stock_id'=>'','committed_qty'=>0,'unit_price'=>0,'discount_percent'=>0,
            'description'=>'','min_qty_per_order'=>0,'price_valid_until'=>null,'custom_data'=>null);
    return sales_agreement_dimension_write($owner, $operation === 'insert' ? null : $key, $next,
        function($admitted) use ($owner, $operation, $key, $next, $validate, $header, $defaults) {
            $old = array();
            if ($operation !== 'insert') {
                $rows = db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE id='.(int)$key.' FOR UPDATE');
                $old = $rows ? db_fetch($rows) : false;
                if (!$old) return false;
            }
            foreach ($next as $field=>$value) {
                if (array_key_exists($field, $defaults) && !(!$header && $field === 'agreement_id' && $operation !== 'insert')) continue;
                if (!array_key_exists($field, $old) || is_array($value) || is_object($value)
                    || ($value === null ? $old[$field] !== null : $old[$field] === null || (string)$value !== (string)$old[$field])) return false;
            }
            if (!$validate()) return false;
            $data = $next + $old + $defaults;
            // db_escape already decodes/re-encodes native stored text once;
            // another decode here would damage retained literal entity text.
            if ($operation === 'delete') return $header ? delete_sales_agreement($key) : delete_sales_agreement_line($key);
            $args = array();
            foreach ($defaults as $field=>$value) $args[] = $data[$field];
            if ($header) {
                if ($operation === 'update') array_unshift($args, $key);
                return call_user_func_array($operation === 'insert' ? 'add_sales_agreement' : 'update_sales_agreement', $args);
            }
            if ($operation === 'update') $args[0] = $key;
            return call_user_func_array($operation === 'insert' ? 'add_sales_agreement_line' : 'update_sales_agreement_line', $args);
        }, $operation === 'delete');
}

/** Encode authored JSON without changing retained stored bytes or hiding encoding failure. */
function sales_agreement_dimension_payload($value)
{
    if($value===null || is_string($value))return $value;
    if(!is_array($value))return false;
    return json_encode($value);
}

/** Quote raw retained metadata through the native driver, never HTML encoding. */
function sales_agreement_dimension_payload_sql($value)
{
    return $value===null ? 'NULL' : "'".db_escape_function($value)."'";
}

/**
 * Retain source references while native order/delivery/invoice commands refresh derived counters.
 * Their native permissions remain authoritative; cache maintenance grants no lifecycle decision.
 */
function sales_agreement_dimension_maintenance($identity,$persist)
{
    $id=dimension_writer_reference_id($identity);$context=dimension_financial_reference_context();
    if(!$id || !$context || !is_callable($persist))dimension_financial_reference_failure();
    try{
        if(!begin_transaction())dimension_financial_reference_failure();
        if(!db_query('SELECT custom_data FROM '.TB_PREF.'sales_agreement_lines LIMIT 0'))dimension_financial_reference_failure();
        $headers=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'sales_agreements WHERE id='.$id.' FOR UPDATE');
        $lines=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'sales_agreement_lines WHERE agreement_id='.$id.' ORDER BY id FOR UPDATE');
        if(!$headers || count($headers)!==1 || $lines===false)dimension_financial_reference_failure();
        $masters=array();
        foreach(array_merge($headers,$lines) as $row){
            if(!array_key_exists('custom_data',$row))dimension_financial_reference_failure();
            $references=dimension_master_payload_references($row['custom_data']);
            if(is_array($references))foreach($references as $reference)$masters[$reference[0]]=$reference[0];
        }
        foreach(array('dimension_id','dimension2_id') as $field){
            if(!array_key_exists($field,$headers[0]) || ($ref=dimension_writer_reference_id($headers[0][$field]))===false)dimension_financial_reference_failure();
            if($ref)$masters[$ref]=$ref;
        }
        sort($masters,SORT_NUMERIC);
        foreach($masters as $master)if(sales_agreement_lifecycle_rows('SELECT id FROM '.TB_PREF.'dimensions WHERE id='.$master.' FOR UPDATE')===false)dimension_financial_reference_failure();
        foreach($lines as $line)if(!dimension_ordinary_master_write('sales_agreement_lines',$line['id'],array(),function(){return true;},true))dimension_financial_reference_failure();
        $result=dimension_ordinary_master_write('sales_agreements',$id,array(),function($admitted)use($id,$headers,$lines,$persist,$context){
            if(!call_user_func($persist) || dimension_financial_reference_context()!==$context)return false;
            $after_headers=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'sales_agreements WHERE id='.$id.' FOR UPDATE');
            $after_lines=sales_agreement_lifecycle_rows('SELECT * FROM '.TB_PREF.'sales_agreement_lines WHERE agreement_id='.$id.' ORDER BY id FOR UPDATE');
            if(!$after_headers || count($after_headers)!==1 || $after_lines===false)return false;
            foreach(array('total_committed','total_ordered','total_delivered','total_invoiced') as $field){unset($headers[0][$field],$after_headers[0][$field]);}
            foreach($lines as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($lines[$index][$field]);
            foreach($after_lines as $index=>$line)foreach(array('ordered_qty','delivered_qty','invoiced_qty') as $field)unset($after_lines[$index][$field]);
            return sales_agreement_lifecycle_canonical(array($headers,$lines))===sales_agreement_lifecycle_canonical(array($after_headers,$after_lines));
        },true);
        if(!$result || dimension_financial_reference_context()!==$context || !commit_transaction())dimension_financial_reference_failure();
        return true;
    }catch(DimensionFinancialReferenceWriteException $exception){throw $exception;
    }catch(Exception $exception){dimension_financial_reference_failure();
    }catch(Throwable $exception){dimension_financial_reference_failure();}
}
