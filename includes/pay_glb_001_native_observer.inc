<?php
/**
 * PAY-GLB-001 S27: INTERNAL ONLY. Read-only, single consistent InnoDB snapshot of
 * narrowly selected source PRESENCE. Does not implement a production collector:
 * no authentication, reconciliation proof, operational delivery, as-of history,
 * custody writer, route or live/legal authority is inferred from these counts.
 * Database handle must be supplied by an independently authorized server caller.
 */
require_once __DIR__.'/pay_glb_001_native_source_contract.inc';

/** Build only fixed presence reads using validated numeric identifiers and prefix. */
function pay_glb_001_native_queries($prefix, $company_id, $entity_id)
{
    if (!is_string($prefix) || preg_match('/^[0-9]{1,5}_$/D',$prefix)!==1 ||
        !is_int($company_id) || $company_id<0 ||
        !is_int($entity_id) || $entity_id<=0) return false;
    $p=$prefix; $c=(string)$company_id; $e=(string)$entity_id;
    $r='`'.$p.'hrm_pay_core_009_runs`';
    $i='`'.$p.'hrm_pay_core_009_items`';
    $j='`'.$p.'hrm_pay_gl_001_journal_intents`';
    $g='`'.$p.'hrm_pay_gl_001_gl_postings`';
    $gr='`'.$p.'hrm_pay_gl_001_reconciliations`';
    $pr='`'.$p.'hrm_pay_rec_001_profiles`';
    $cl='`'.$p.'hrm_pay_rec_001_checklists`';
    return array(
        'has_run'=>"SELECT COUNT(*) AS c FROM $r r WHERE r.legal_entity_id=$e",
        'has_items'=>"SELECT COUNT(*) AS c FROM $i i INNER JOIN $r r ON r.run_id=i.run_id".
            " AND r.legal_entity_id=i.legal_entity_id WHERE r.legal_entity_id=$e",
        'has_journal'=>"SELECT COUNT(*) AS c FROM $j j WHERE j.company_id=$c AND j.legal_entity_id=$e",
        'has_posting'=>"SELECT COUNT(*) AS c FROM $g g INNER JOIN $j j".
            " ON j.journal_intent_id=g.journal_intent_id WHERE j.company_id=$c AND j.legal_entity_id=$e",
        'has_gl_reconciliation'=>"SELECT COUNT(*) AS c FROM $gr gr INNER JOIN $j j".
            " ON j.journal_intent_id=gr.journal_intent_id WHERE j.company_id=$c AND j.legal_entity_id=$e",
        'has_rec_profile'=>"SELECT COUNT(*) AS c FROM $pr p INNER JOIN $r r".
            " ON r.run_id=p.pay_core_run_id AND r.legal_entity_id=p.legal_entity_id".
            " AND r.payroll_period_id=p.payroll_period_id".
            " WHERE p.company_id=$c AND p.legal_entity_id=$e",
        'has_checklist'=>"SELECT COUNT(*) AS c FROM $cl cl INNER JOIN $pr p".
            " ON p.reconciliation_profile_id=cl.reconciliation_profile_id".
            " INNER JOIN $r r ON r.run_id=p.pay_core_run_id".
            " AND r.legal_entity_id=p.legal_entity_id AND r.payroll_period_id=p.payroll_period_id".
            " WHERE p.company_id=$c AND p.legal_entity_id=$e"
    );
}

/** Accept exactly one bounded aggregate column and return only its presence. */
function pay_glb_001_native_count_present($db, $sql)
{
    $result=$db->query($sql);
    if (!is_object($result) || !method_exists($result,'fetch_assoc')) return null;
    $first=$result->fetch_assoc();
    $second=$result->fetch_assoc();
    if (method_exists($result,'free')) $result->free();
    if (!is_array($first) || array_keys($first)!==array('c') ||
        $second!==null && $second!==false ||
        !is_string($first['c']) || !preg_match('/^(0|[1-9][0-9]{0,15})$/D',$first['c'])) return null;
    return $first['c']!=='0';
}

/**
 * Roll back even a successful read transaction: this never commits or writes.
 * A SQL failure/timeout/partial read/rollback failure is an all-or-nothing denial.
 * Not reachable from any application route. Do not forward its observations to UI.
 */
function pay_glb_001_native_observe($db, $prefix, $scope, $entities)
{
    if (!pay_glb_001_native_scope_valid($scope,$entities) ||
        !is_object($db) || !method_exists($db,'query') ||
        !pay_glb_001_native_queries($prefix,$scope['company_id'],$entities[0]['legal_entity_id']))
        return false;
    $as_of=strtotime($scope['as_of_utc']);
    if ($as_of===false || abs(time()-$as_of)>60) return false;
    $started=false; $good=true; $rows=array();
    try {
        if ($db->query('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')!==true)
            return false;
        if ($db->query('START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY')!==true)
            return false;
        $started=true;
        foreach ($entities as $entity) {
            $queries=pay_glb_001_native_queries($prefix,$scope['company_id'],$entity['legal_entity_id']);
            if ($queries===false) { $good=false; break; }
            $row=array('entity_code'=>$entity['entity_code'],
                'legal_entity_id'=>$entity['legal_entity_id']);
            foreach ($queries as $key=>$query) {
                $value=pay_glb_001_native_count_present($db,$query);
                if ($value===null) { $good=false; break; }
                $row[$key]=$value;
            }
            if (!$good) break;
            $row['authority']='UNVERIFIED';
            $rows[]=$row;
        }
    } catch (Throwable $error) {
        $good=false;
    } finally {
        // ROLLBACK success is required; uncommitted/uncertain transaction denies.
        if ($started) {
            try { if ($db->query('ROLLBACK')!==true) $good=false; }
            catch (Throwable $error) { $good=false; }
        }
    }
    if (!$good || !$started || count($rows)!==count($entities)) return false;
    $result=array('company_id'=>$scope['company_id'],
        'as_of_utc'=>$scope['as_of_utc'],
        'state'=>'NON_ROUTABLE_NATIVE_COUNTS_ONLY','entities'=>$rows,
        'production_route_allowed'=>false,'reconciled_verified'=>false);
    $json=json_encode($result,JSON_UNESCAPED_SLASHES);
    if ($json===false) return false;
    $result['observation_sha256']=hash('sha256',$json);
    return pay_glb_001_native_observation_valid($scope,$entities,$result) ? $result : false;
}
?>
