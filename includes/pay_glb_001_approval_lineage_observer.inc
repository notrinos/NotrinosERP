<?php
/**
 * PAY-GLB-001 S33: rollback-only approval/journal mismatch diagnostics.
 * No immutable capture, certified lineage, signoff, production access or route.
 * The selected run is metadata-only: there is no join from a journal to the
 * PAY-CORE-009 run, so even all matching observations remain UNVERIFIED.
 */
require_once __DIR__.'/pay_glb_001_approval_lineage_contract.inc';

/** Observe aggregate discrepancies on an idle connection and roll back only our snapshot. */
function pay_glb_001_approval_lineage_observe($db,$prefix,$scope,$entity,$selection)
{
    $sql=pay_glb_001_approval_lineage_query($prefix,$scope,$entity,$selection);
    if ($sql===false || !is_object($db) || !method_exists($db,'query')) return false;
    $asOf=strtotime($scope['as_of_utc']);
    if ($asOf===false || abs(time()-$asOf)>60) return false;
    $started=false;$good=true;$row=false;
    try {
        // START TRANSACTION would implicitly commit an existing caller transaction.
        // Fail before changing isolation or taking ownership of that transaction.
        $state=$db->query('SELECT @@in_transaction AS transaction_active');
        if (!is_object($state) || !method_exists($state,'fetch_assoc')) return false;
        $active=$state->fetch_assoc();$extraState=$state->fetch_assoc();
        if (method_exists($state,'free')) $state->free();
        if (!is_array($active) || array_keys($active)!==array('transaction_active') ||
            $active['transaction_active']!=='0' ||
            ($extraState!==null && $extraState!==false)) return false;
        if ($db->query('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')!==true) return false;
        if ($db->query('START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY')!==true) return false;
        $started=true;
        $result=$db->query($sql);
        if (!is_object($result) || !method_exists($result,'fetch_assoc')) {
            $good=false;
        } else {
            $row=$result->fetch_assoc();
            $extra=$result->fetch_assoc();
            if (method_exists($result,'free')) $result->free();
            $expected=array('journal_count','unbound_count',
                'distinct_result_sets','distinct_approvals');
            if (!is_array($row) || array_keys($row)!==$expected ||
                ($extra!==null && $extra!==false)) $good=false;
            else foreach($row as $value)
                if (!is_string($value) ||
                    preg_match('/^(0|[1-9][0-9]{0,14})$/D',$value)!==1)
                    {$good=false;break;}
        }
    } catch (Throwable $error) {$good=false;}
    finally {
        if ($started) {
            try {if ($db->query('ROLLBACK')!==true) $good=false;}
            catch (Throwable $error) {$good=false;}
        }
    }
    if (!$good || !$started || !is_array($row)) return false;
    // These columns describe one aggregate. Impossible relationships are corrupt
    // results, not meaningful discrepancy observations, even with valid decimals.
    $total=(float)$row['journal_count'];
    if ((float)$row['unbound_count']>$total ||
        (float)$row['distinct_result_sets']>$total ||
        (float)$row['distinct_approvals']>$total ||
        ($total>0 && ($row['distinct_result_sets']==='0' || $row['distinct_approvals']==='0')))
        return false;
    $out=array(
        'company_id'=>$scope['company_id'],
        'legal_entity_id'=>$entity['legal_entity_id'],
        'run_id'=>$selection['run_id'],
        'payroll_period_id'=>$selection['payroll_period_id'],
        'entity_code'=>$entity['entity_code'],
        'as_of_utc'=>$scope['as_of_utc'],
        'status'=>'UNVERIFIED_APPROVAL_RUN_LINK',
        'journal_present'=>$row['journal_count']!=='0',
        'unbound_approval_present'=>$row['unbound_count']!=='0',
        'multiple_result_sets'=>(float)$row['distinct_result_sets']>1,
        'multiple_approvals'=>(float)$row['distinct_approvals']>1,
        'same_run_result_hash_verified'=>false,
        'reconciled_verified'=>false,
        'production_route_allowed'=>false,
        'close_ready'=>false
    );
    $json=json_encode($out,JSON_UNESCAPED_SLASHES);
    if ($json===false) return false;
    $out['diagnostic_sha256']=hash('sha256',$json);
    return pay_glb_001_approval_lineage_diagnostic_valid(
        $scope,$entity,$selection,$out)?$out:false;
}
?>
