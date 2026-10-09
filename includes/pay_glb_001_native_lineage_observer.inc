<?php
/**
 * PAY-GLB-001 S30: rollback-only, consistent-snapshot selected-run diagnostics.
 * No entity grants, live provider exchange, source writes or source replay.
 * GL journals have only a period/group/result-set hash, not an authenticated
 * direct PAY-CORE-009 run foreign key. Result equivalence stays UNVERIFIED.
 */
require_once __DIR__.'/pay_glb_001_native_lineage_contract.inc';

/** Fetch exactly one row of an expected fixed aggregate/metadata shape. */
function pay_glb_001_lineage_one($db,$sql,$keys)
{
    $result=$db->query($sql);
    if (!is_object($result) || !method_exists($result,'fetch_assoc')) return false;
    $row=$result->fetch_assoc();$extra=$result->fetch_assoc();
    if (method_exists($result,'free')) $result->free();
    if (!is_array($row) || array_keys($row)!==$keys || ($extra!==null && $extra!==false)) return false;
    return $row;
}

/** Accept only bounded nonnegative MySQL COUNT/SUM decimal fields. */
function pay_glb_001_lineage_count($value)
{
    return is_string($value) && preg_match('/^(0|[1-9][0-9]{0,14})$/D',$value)===1;
}

/** Execute fixed SQL inside one rollback-only InnoDB snapshot, or deny. */
function pay_glb_001_native_lineage_observe($db,$prefix,$scope,$entity,$selection)
{
    $queries=pay_glb_001_lineage_queries($prefix,$scope,$entity,$selection);
    if ($queries===false || !is_object($db) || !method_exists($db,'query')) return false;
    $asOf=strtotime($scope['as_of_utc']);
    if ($asOf===false || abs(time()-$asOf)>60) return false;
    $started=false;$good=true;$rows=array();
    try {
        if ($db->query('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')!==true) return false;
        if ($db->query('START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY')!==true) return false;
        $started=true;
        foreach ($queries as $key=>$sql) {
            $keys=$key==='run'?array('state','state_version','state_sha256'):
                ($key==='items'?array('total_count','incomplete_count'):array('c'));
            $row=pay_glb_001_lineage_one($db,$sql,$keys);
            if ($row===false) {$good=false;break;}
            if ($key==='run') {
                if (!in_array($row['state'],array('queued','running','finalizing','completed',
                    'failed','quarantined','cancelled'),true) ||
                    !pay_glb_001_lineage_count($row['state_version']) ||
                    $row['state_version']==='0' || !pay_glb_001_sha256($row['state_sha256'])) {
                    $good=false;break;
                }
            } else {
                foreach ($row as $count) if (!pay_glb_001_lineage_count($count)) {$good=false;break;}
                if (!$good) break;
            }
            $rows[$key]=$row;
        }
    } catch (Throwable $error) {$good=false;}
    finally {
        if ($started) {
            try {if ($db->query('ROLLBACK')!==true) $good=false;}
            catch (Throwable $error) {$good=false;}
        }
    }
    if (!$good || !$started || count($rows)!==count($queries)) return false;
    $out=array('company_id'=>$scope['company_id'],'legal_entity_id'=>$entity['legal_entity_id'],
        'run_id'=>$selection['run_id'],'payroll_period_id'=>$selection['payroll_period_id'],
        'entity_code'=>$entity['entity_code'],'as_of_utc'=>$scope['as_of_utc'],
        'status'=>'UNVERIFIED_SAME_RUN_LINEAGE',
        'run_completed'=>$rows['run']['state']==='completed',
        'all_items_completed'=>$rows['items']['total_count']!=='0' && $rows['items']['incomplete_count']==='0',
        'journal_present'=>$rows['journal']['c']!=='0',
        'posting_present'=>$rows['postings']['c']!=='0',
        'gl_reconciliation_present'=>$rows['gl_reconciliations']['c']!=='0',
        'rec_profile_present'=>$rows['profiles']['c']!=='0',
        'checklist_present'=>$rows['checklists']['c']!=='0',
        'currency_mismatch_present'=>$rows['currency_mismatches']['c']!=='0',
        'same_run_result_hash_verified'=>false,'source_freshness_verified'=>false,
        'production_route_allowed'=>false,'reconciled_verified'=>false);
    $json=json_encode($out,JSON_UNESCAPED_SLASHES);
    if ($json===false) return false;
    $out['lineage_sha256']=hash('sha256',$json);
    return pay_glb_001_lineage_diagnostic_valid($scope,$entity,$selection,$out)?$out:false;
}
?>
