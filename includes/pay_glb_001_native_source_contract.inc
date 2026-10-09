<?php
/**
 * PAY-GLB-001 S26: frozen native PAY-CORE/GL/REC read inventory.
 * A shape-valid observation is NOT a trusted as-of source, authorization,
 * reconciled payroll, provider acceptance, or permission to open a route.
 */
require_once __DIR__.'/pay_glb_001_contract.inc';

/** Validate an exact internal native mapping; this does not grant read access. */
function pay_glb_001_native_scope_valid($scope, $entities)
{
    if (!pay_glb_001_context_valid($scope, 'rollup') ||
        !is_array($entities) || count($entities)!==count($scope['entity_codes']) ||
        array_keys($entities)!==range(0,count($entities)-1)) return false;
    $seen=array(); $ids=array();
    foreach ($entities as $entity) {
        if (!pay_glb_001_exact_keys($entity,array('entity_code','legal_entity_id','delivery_model')) ||
            !pay_glb_001_code($entity['entity_code']) ||
            !in_array($entity['entity_code'],$scope['entity_codes'],true) ||
            isset($seen[$entity['entity_code']]) ||
            !is_int($entity['legal_entity_id']) || $entity['legal_entity_id']<=0 ||
            isset($ids[$entity['legal_entity_id']]) ||
            $entity['delivery_model']!=='NATIVE') return false;
        $seen[$entity['entity_code']]=true;
        $ids[$entity['legal_entity_id']]=true;
    }
    return count($seen)===count($scope['entity_codes']);
}

/** Explicit source references pinned to existing accepted SQL CREATE definitions. */
function pay_glb_001_native_source_inventory()
{
    return array(
        'runs'=>array('hrm_pay_core_009_runs','legal_entity_id','run_id','state'),
        'items'=>array('hrm_pay_core_009_items','run_id','legal_entity_id'),
        'journals'=>array('hrm_pay_gl_001_journal_intents','company_id','legal_entity_id','payroll_period_id'),
        'postings'=>array('hrm_pay_gl_001_gl_postings','journal_intent_id','posting_id'),
        'gl_reconciliations'=>array('hrm_pay_gl_001_reconciliations','journal_intent_id','reconciliation_id'),
        'profiles'=>array('hrm_pay_rec_001_profiles','company_id','legal_entity_id','pay_core_run_id','payroll_period_id'),
        'checklists'=>array('hrm_pay_rec_001_checklists','reconciliation_profile_id','close_checklist_id')
    );
}

/** Result never contains individual records, amounts, employee identifiers or evidence payloads. */
function pay_glb_001_native_observation_valid($scope, $entities, $observation)
{
    if (!pay_glb_001_native_scope_valid($scope,$entities) ||
        !pay_glb_001_exact_keys($observation,array('company_id','as_of_utc','state',
            'entities','observation_sha256','production_route_allowed','reconciled_verified')) ||
        $observation['company_id']!==$scope['company_id'] ||
        $observation['as_of_utc']!==$scope['as_of_utc'] ||
        $observation['state']!=='NON_ROUTABLE_NATIVE_COUNTS_ONLY' ||
        $observation['production_route_allowed']!==false ||
        $observation['reconciled_verified']!==false ||
        !pay_glb_001_sha256($observation['observation_sha256']) ||
        !is_array($observation['entities']) ||
        count($observation['entities'])!==count($entities) ||
        array_keys($observation['entities'])!==range(0,count($entities)-1)) return false;
    $expected=array();
    foreach ($entities as $e) $expected[$e['entity_code']]=$e['legal_entity_id'];
    $seen=array();
    foreach ($observation['entities'] as $row) {
        if (!pay_glb_001_exact_keys($row,array('entity_code','legal_entity_id',
            'has_run','has_items','has_journal','has_posting','has_gl_reconciliation',
            'has_rec_profile','has_checklist','authority')) ||
            !pay_glb_001_code($row['entity_code']) ||
            !is_int($row['legal_entity_id']) ||
            !isset($expected[$row['entity_code']]) ||
            $expected[$row['entity_code']]!==$row['legal_entity_id'] ||
            isset($seen[$row['entity_code']]) ||
            $row['authority']!=='UNVERIFIED' ) return false;
        foreach (array('has_run','has_items','has_journal','has_posting',
            'has_gl_reconciliation','has_rec_profile','has_checklist') as $flag)
            if (!is_bool($row[$flag])) return false;
        $seen[$row['entity_code']]=true;
    }
    $copy=$observation; unset($copy['observation_sha256']);
    $json=json_encode($copy,JSON_UNESCAPED_SLASHES);
    return count($seen)===count($expected) && $json!==false &&
        hash_equals(hash('sha256',$json),$observation['observation_sha256']);
}
?>
