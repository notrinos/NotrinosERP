<?php
/** S25: deployment-pinned mapping validation. No collector, network or legal certification. */
require_once __DIR__.'/pay_glb_001_server_trust.inc';

/** Bind the exact validated aggregate and mapping revision into a V2 signed digest. */
function pay_glb_001_server_mapping_digest($batch, $manifest, $mapping_pin)
{
    if (!is_array($batch) || !is_array($manifest) ||
        !isset($batch['collection_sha256'],$manifest['manifest_sha256']) ||
        !pay_glb_001_sha256($batch['collection_sha256']) ||
        !pay_glb_001_sha256($manifest['manifest_sha256']) || !pay_glb_001_sha256($mapping_pin)) return false;
    return hash('sha256',json_encode(array('domain'=>'PAY-GLB-001-MAPPED-BATCH-V1',
        'collection_sha256'=>$batch['collection_sha256'],'manifest_sha256'=>$manifest['manifest_sha256'],
        'mapping_store_sha256'=>$mapping_pin),JSON_UNESCAPED_SLASHES));
}

/**
 * Resolve complete current entity/country/source/destination contracts from the
 * operator store, then verify a one-use signed batch proof against current trust.
 * APPROVED_FOR_INTERNAL_READ is deployment state, never jurisdictional approval.
 * Re-run this boundary for every later collector/read; returned data is not a grant.
 */
function pay_glb_001_resolve_server_mappings($entity_codes, $batch, $manifest, $proof)
{
    $access=pay_glb_001_resolve_server_access($entity_codes);
    $store=$access===false ? false : pay_glb_001_read_admission_store('mapping');
    if ($access===false || $store===false || !is_array($batch) || !isset($batch['as_of_utc'])) return false;
    $scope=$access['scope']; $now=$scope['as_of_utc'];
    if (!pay_glb_001_iso_utc($batch['as_of_utc']) || $batch['as_of_utc']>$now ||
        strtotime($now)-strtotime($batch['as_of_utc'])>60) return false;
    $scope['as_of_utc']=$batch['as_of_utc'];
    if (!pay_glb_001_source_manifest_valid($scope,$batch,$manifest)) return false;
    $entities=array(); $sources=array(); $resolved=array(); $ids=array();
    foreach ($access['entities'] as $entity) $entities[$entity['entity_code']]=$entity;
    foreach ($manifest['sources'] as $source) $sources[$source['entity_code']]=$source;
    foreach ($store['records'] as $mapping) {
        if (!pay_glb_001_exact_keys($mapping,array('record_id','entity_code','legal_entity_id','country_code',
            'delivery_model','source_class','source_contract_sha256','mapping_version','mapping_version_sha256',
            'destination','destination_company_id','destination_region','effective_from_utc','expires_at_utc',
            'status','legal_review_evidence_sha256')) || !pay_glb_001_code($mapping['record_id']) ||
            isset($ids[$mapping['record_id']]) || !pay_glb_001_code($mapping['entity_code'])) return false;
        $ids[$mapping['record_id']]=true;
        if (!isset($entities[$mapping['entity_code']])) continue;
        $entity=$entities[$mapping['entity_code']]; $source=$sources[$mapping['entity_code']];
        if (isset($resolved[$mapping['entity_code']]) || isset($store['revoked_ids'][$mapping['record_id']]) ||
            !is_int($mapping['legal_entity_id']) || $mapping['legal_entity_id']!==$entity['legal_entity_id'] ||
            $mapping['country_code']!==$entity['country_code'] || $mapping['country_code']!==$source['country_code'] ||
            !in_array($mapping['delivery_model'],pay_glb_001_models(),true) ||
            $mapping['delivery_model']!==$source['delivery_model'] || $mapping['source_class']!==$source['source_class'] ||
            !pay_glb_001_sha256($mapping['source_contract_sha256']) ||
            $mapping['source_contract_sha256']!==$source['source_contract_sha256'] ||
            !is_int($mapping['mapping_version']) || $mapping['mapping_version']<=0 ||
            !pay_glb_001_sha256($mapping['mapping_version_sha256']) ||
            $mapping['mapping_version_sha256']!==$source['mapping_version_sha256'] ||
            $mapping['destination']!=='PAY_GLB_001_MINIMIZED_INTERNAL_VIEW' ||
            $mapping['destination_company_id']!==$scope['company_id'] ||
            $mapping['destination_region']!==$store['execution_region'] ||
            !pay_glb_001_iso_utc($mapping['effective_from_utc']) || !pay_glb_001_iso_utc($mapping['expires_at_utc']) ||
            $mapping['effective_from_utc']>$scope['as_of_utc'] || $mapping['expires_at_utc']<=$now ||
            $mapping['expires_at_utc']>$store['expires_at_utc'] ||
            $mapping['status']!=='APPROVED_FOR_INTERNAL_READ' ||
            !pay_glb_001_sha256($mapping['legal_review_evidence_sha256'])) {
            pay_glb_001_audit_admission('mapping',false,$store['store_sha256']); return false;
        }
        $resolved[$mapping['entity_code']]=array('entity_code'=>$mapping['entity_code'],
            'legal_entity_id'=>$entity['legal_entity_id'],'country_code'=>$mapping['country_code'],
            'delivery_model'=>$mapping['delivery_model'],'source_class'=>$mapping['source_class'],
            'mapping_version'=>$mapping['mapping_version'],'mapping_version_sha256'=>$mapping['mapping_version_sha256']);
    }
    if (count($resolved)!==count($scope['entity_codes'])) {
        pay_glb_001_audit_admission('mapping',false,$store['store_sha256']); return false;
    }
    $digest=pay_glb_001_server_mapping_digest($batch,$manifest,$store['store_sha256']);
    if ($digest===false || pay_glb_001_verify_server_proof($entity_codes,$digest,$proof)===false) return false;
    $current=pay_glb_001_read_admission_store('mapping');
    if ($current===false || $current['store_sha256']!==$store['store_sha256'] ||
        !pay_glb_001_audit_admission('mapping',true,$store['store_sha256'])) return false;
    ksort($resolved,SORT_STRING);
    return array('mapping_contract_valid'=>true,'mapping_revision'=>$store['revision'],
        'mapping_store_sha256'=>$store['store_sha256'],'entities'=>array_values($resolved),
        'legal_approval_verified'=>false,'live_collection_verified'=>false,
        'production_route_allowed'=>false,'close_ready'=>false);
}
