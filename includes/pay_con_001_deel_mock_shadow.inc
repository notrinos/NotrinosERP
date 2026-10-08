<?php
/**
 * Non-delivering synthetic PAY-CON-001 Deel shadow planning.
 * No payroll computation, record writer, external call or accepted provider mapping.
 */
require_once(dirname(__FILE__).'/pay_con_001_deel_mock_contract.inc');

/** Validate synthetic identifiers and produce a deterministic inert adjustment draft. */
function pay_con_001_deel_mock_shadow_plan($sample)
{
    if (!is_array($sample)) return false;
    $allowed=array('company_id','source_kind','worker','contract_id','category_id','amount','adjustment_date','mapping_version');
    foreach($sample as $k=>$v) if(!in_array($k,$allowed,true))return false;
    if (count($sample)!==count($allowed)
        || ($sample['company_id']??null)!==0
        || ($sample['source_kind']??'')!=='SYNTHETIC_ADJUSTMENT'
        || ($sample['mapping_version']??'')!=='mock-v1'
        || !is_string($sample['worker']??null) || preg_match('/^FAKE-[0-9]{3,6}$/D',$sample['worker'])!==1
        || !is_string($sample['contract_id']??null) || preg_match('/^synthetic-contract-[0-9]{3,6}$/D',$sample['contract_id'])!==1
        || !is_string($sample['category_id']??null) || preg_match('/^synthetic-category-[0-9]{3,6}$/D',$sample['category_id'])!==1
        || !is_string($sample['amount']??null) || preg_match('/^-?[0-9]{1,9}\.[0-9]{2}$/D',$sample['amount'])!==1
        || !is_string($sample['adjustment_date']??null) || preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D',$sample['adjustment_date'])!==1) return false;
    $dt=DateTimeImmutable::createFromFormat('!Y-m-d',$sample['adjustment_date']);
    if(!$dt || $dt->format('Y-m-d')!==$sample['adjustment_date'])return false;
    // Stable canonical order and hash. Test artefacts are never live idempotency keys.
    $draft=array(
        'contract_id'=>$sample['contract_id'],
        'title'=>'Synthetic test adjustment',
        'amount'=>$sample['amount'],
        'adjustment_category_id'=>$sample['category_id'],
        'date_of_adjustment'=>$sample['adjustment_date']
    );
    $json=json_encode($draft, JSON_UNESCAPED_SLASHES);
    if(!is_string($json))return false;
    $hash=hash('sha256','PAY-CON-001:DEEL:MOCK:v1:company=0:worker='.$sample['worker'].':'.$json);
    return array(
        'provider'=>'DEEL',
        'mode'=>'OFFLINE_SHADOW_DRAFT',
        'company_id'=>0,
        'mapping_version'=>'mock-v1',
        'documented_method'=>'POST',
        'documented_path'=>'/rest/adjustments',
        'documented_version'=>'2026-01-01',
        'draft_payload'=>$draft,
        'shadow_digest_sha256'=>$hash,
        'mock_idempotency_marker'=>'MOCK-'.substr($hash,0,32),
        'synthetic_worker'=>$sample['worker'],
        'authoritative'=>false,
        'network_allowed'=>false,
        'database_write_allowed'=>false,
        'payroll_execution_allowed'=>false
    );
}

/** Verify the complete synthetic draft and echo without importing payroll results. */
function pay_con_001_deel_mock_shadow_reconcile($plan,$observed)
{
    if(!is_array($plan)||!is_array($observed)
        || ($plan['mode']??'')!=='OFFLINE_SHADOW_DRAFT'
        || ($plan['network_allowed']??true)!==false
        || ($plan['database_write_allowed']??true)!==false
        || ($plan['authoritative']??true)!==false
        || ($plan['company_id']??null)!==0
        || ($plan['provider']??'')!=='DEEL'
        || ($observed['fixture_kind']??'')!=='SYNTHETIC_SHADOW_ECHO'
        || ($observed['company_id']??null)!==0
        || ($observed['provider']??'')!=='DEEL'
        || !is_string($observed['shadow_digest_sha256']??null)
        || !is_array($plan['draft_payload']??null)
        || count($observed)!==4)return false;
    $draft=$plan['draft_payload'];
    $canonical=pay_con_001_deel_mock_shadow_plan(array(
        'company_id'=>0,'source_kind'=>'SYNTHETIC_ADJUSTMENT',
        'worker'=>$plan['synthetic_worker']??null,
        'contract_id'=>$draft['contract_id']??null,
        'category_id'=>$draft['adjustment_category_id']??null,
        'amount'=>$draft['amount']??null,
        'adjustment_date'=>$draft['date_of_adjustment']??null,
        'mapping_version'=>$plan['mapping_version']??null
    ));
    if($canonical===false || $plan!==$canonical)return false;
    if(!hash_equals($plan['shadow_digest_sha256'],$observed['shadow_digest_sha256']))return false;
    // A matching synthetic echo means only that the MOCK draft was replayable.
    return array(
        'status'=>'MOCK_RECONCILED_NOT_IMPORTED',
        'shadow_digest_sha256'=>$plan['shadow_digest_sha256'],
        'payroll_result_accepted'=>false,
        'external_exchange_occurred'=>false
    );
}
?>
