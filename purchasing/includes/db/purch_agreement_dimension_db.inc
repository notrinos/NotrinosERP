<?php
/** Purchase-agreement reference custody shared by native and generic writers. */
require_once __DIR__.'/../../../dimensions/includes/dimension_reference_writer_fence.inc';

/** Bind maintenance to the authenticated actor and active company prefix. */
function purch_agreement_dimension_context()
{
    global $db_connections;
    if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])) return false;
    $user = $_SESSION['wa_current_user'];
    if (!isset($user->user, $user->cur_con) || !dimension_writer_reference_id($user->user)
        || !isset($db_connections[$user->cur_con]['tbpref'])
        || !is_string($db_connections[$user->cur_con]['tbpref'])
        || !method_exists($user, 'can_access') || !$user->can_access('SA_PURCHAGREEMENT')) return false;
    return array($user->user, $user->cur_con, $db_connections[$user->cur_con]['tbpref']);
}

/**
 * Serialize draft maintenance parent-first, then retain scalar/JSON references
 * through the existing ordinary fence. A child lookup is only a parent hint;
 * its identity is rechecked after the parent lock. No workflow status, approval
 * decision, fulfillment quantity or generated identity is authored here.
 */
function purch_agreement_dimension_write($owner, $key, $next, $persist, $remove = false)
{
    $context = purch_agreement_dimension_context();
    if (!$context || !in_array($owner, array('purch_agreements','purch_agreement_lines'), true)
        || !is_array($next) || !is_callable($persist)) return false;
    $identity = $key === null ? null : dimension_writer_reference_id($key);
    if ($identity === false || $identity === 0) return false;
    $header = $owner === 'purch_agreements';
    try {
        if (!begin_transaction()) return false;
        $draft_rows = db_query('SELECT draft_data,status FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_PURCHAGREEMENT.' ORDER BY id FOR UPDATE');
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
            $rows = db_query('SELECT * FROM '.TB_PREF.'purch_agreements WHERE id='.$parent.' FOR UPDATE');
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
            $rows = db_query('SELECT order_no FROM '.TB_PREF.'purch_orders WHERE agreement_id='.$parent.' ORDER BY order_no FOR UPDATE');
            if (!$rows || db_fetch($rows)) { cancel_transaction(); return false; }
            $rows = db_query('SELECT * FROM '.TB_PREF.'purch_agreement_lines WHERE agreement_id='.$parent.' ORDER BY id FOR UPDATE');
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
                if (!dimension_ordinary_master_write('purch_agreement_lines', $line['id'], array(),
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
                if (!$result || purch_agreement_dimension_context() !== $context) return false;
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
        if (!$result || purch_agreement_dimension_context() !== $context || !commit_transaction()) {
            cancel_transaction(); return false;
        }
        if ($identity === null && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/** Route generic/import models through the same validated native draft commands. */
function purch_agreement_dimension_entity_write($owner, $operation, $key, $next, $validate)
{
    if (!is_array($next) || !is_callable($validate)
        || !in_array($operation, array('insert','update','delete'), true)) return false;
    if (!function_exists('add_purch_agreement')) require_once __DIR__.'/purch_agreement_db.inc';
    $header = $owner === 'purch_agreements';
    if (!$header && $owner !== 'purch_agreement_lines') return false;
    $defaults = $header ? array('agreement_type'=>'blanket_order','supplier_id'=>0,'date_start'=>null,'date_end'=>null,
        'currency'=>'','payment_terms'=>0,'delivery_location'=>'','auto_renew'=>0,'renewal_period_months'=>12,
        'terms_and_conditions'=>'','notes'=>'','rfq_id'=>0,'dimension_id'=>0,'dimension2_id'=>0,
        'reference'=>'','buyer_id'=>0,'custom_data'=>null)
        : array('agreement_id'=>0,'stock_id'=>'','committed_qty'=>0,'unit_price'=>0,'discount_percent'=>0,
            'description'=>'','min_qty_per_order'=>0,'price_valid_until'=>null,'custom_data'=>null);
    return purch_agreement_dimension_write($owner, $operation === 'insert' ? null : $key, $next,
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
            // Native db_escape encodes authored strings. Omitted stored text
            // already has that encoding and must survive a partial update.
            foreach ($defaults as $field=>$value)
                if ($field !== 'custom_data' && !array_key_exists($field, $next) && isset($old[$field]) && is_string($data[$field]))
                    $data[$field] = htmlspecialchars_decode($data[$field], ENT_QUOTES);
            if ($operation === 'delete') return $header ? delete_purch_agreement($key) : delete_agreement_line($key);
            $args = array();
            foreach ($defaults as $field=>$value) $args[] = $data[$field];
            if ($header) {
                if ($operation === 'update') array_unshift($args, $key);
                return call_user_func_array($operation === 'insert' ? 'add_purch_agreement' : 'update_purch_agreement', $args);
            }
            if ($operation === 'update') $args[0] = $key;
            return call_user_func_array($operation === 'insert' ? 'add_agreement_line' : 'update_agreement_line', $args);
        }, $operation === 'delete');
}
