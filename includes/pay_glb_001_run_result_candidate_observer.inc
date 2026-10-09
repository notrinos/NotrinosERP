<?php
/**
 * PAY-GLB-001 S36: isolated, rollback-only candidate observation. The SQL
 * compares source hashes but cannot prove an approved run/result-set binding.
 * No current-user/session/tenant access authority is granted by this include.
 */
require_once __DIR__.'/pay_glb_001_run_result_candidate_contract.inc';

/** Observe aggregate discrepancies in a fresh read-only snapshot only. */
function pay_glb_001_run_result_candidate_observe($db,$prefix,$scope,$entity,$selection)
{
    $sql=pay_glb_001_run_result_candidate_query($prefix,$scope,$entity,$selection);
    if ($sql===false || !is_object($db) || !method_exists($db,'query')) return false;
    $asOf=strtotime($scope['as_of_utc']);
    if ($asOf===false || abs(time()-$asOf)>60) return false;
    $started=false;$good=true;$row=false;
    try {
        // Never start a new transaction over the caller's uncommitted writes.
        $state=$db->query('SELECT @@in_transaction AS transaction_active');
        if (!is_object($state) || !method_exists($state,'fetch_assoc')) return false;
        $current=$state->fetch_assoc();$extra=$state->fetch_assoc();
        if (method_exists($state,'free')) $state->free();
        if (!is_array($current) || array_keys($current)!==array('transaction_active') ||
            $current['transaction_active']!=='0' || ($extra!==null && $extra!==false)) return false;
        if ($db->query('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')!==true) return false;
        if ($db->query('START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY')!==true) return false;
        $started=true;
        $result=$db->query($sql);
        if (!is_object($result) || !method_exists($result,'fetch_assoc')) $good=false;
        else {
            $row=$result->fetch_assoc();$extra=$result->fetch_assoc();
            if (method_exists($result,'free')) $result->free();
            if (!is_array($row) || array_keys($row)!==array('item_count',
                'incomplete_count','unresolved_count','superseded_count') ||
                ($extra!==null && $extra!==false)) $good=false;
            else foreach($row as $v) if (!is_string($v) ||
                preg_match('/^(0|[1-9][0-9]{0,14})$/D',$v)!==1)
                {$good=false;break;}
        }
    } catch (Throwable $error) {$good=false;}
    finally {
        if ($started) try {if ($db->query('ROLLBACK')!==true) $good=false;}
            catch (Throwable $error) {$good=false;}
    }
    if (!$good || !$started || !is_array($row)) return false;
    $total=(float)$row['item_count'];
    foreach(array('incomplete_count','unresolved_count','superseded_count') as $k)
        if ((float)$row[$k]>$total) return false;
    $out=array('company_id'=>$scope['company_id'],
        'legal_entity_id'=>$entity['legal_entity_id'],
        'entity_code'=>$entity['entity_code'],
        'run_id'=>$selection['run_id'],
        'payroll_period_id'=>$selection['payroll_period_id'],
        'as_of_utc'=>$scope['as_of_utc'],
        'status'=>'UNVERIFIED_RESULT_HASH_CANDIDATE',
        'items_present'=>$row['item_count']!=='0',
        'incomplete_items_present'=>$row['incomplete_count']!=='0',
        'unresolved_results_present'=>$row['unresolved_count']!=='0',
        'superseded_results_present'=>$row['superseded_count']!=='0',
        'same_run_result_hash_verified'=>false,
        'current_result_set_verified'=>false,
        'approval_binding_verified'=>false,
        'production_route_allowed'=>false,'close_ready'=>false);
    $json=json_encode($out,JSON_UNESCAPED_SLASHES);
    if ($json===false) return false;
    $out['candidate_sha256']=hash('sha256',$json);
    return pay_glb_001_run_result_candidate_valid($scope,$entity,$selection,$out)?$out:false;
}
?>
