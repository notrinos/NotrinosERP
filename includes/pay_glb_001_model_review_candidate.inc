<?php
/**
 * PAY-GLB-001 S21: legal-entity, delivery-model and source provenance
 * cross-check for a future reviewed mapping ledger. This pure candidate
 * contract cannot certify real source authority, country law or residency;
 * no production route, SQL writer or provider IO is added.
 */
require_once __DIR__.'/pay_glb_001_preflight.inc';

/** Cross-check model reviews and non-authorizing predecessor candidate reports. */
function pay_glb_001_model_review_candidate($scope, $entities, $batch, $manifest,
    $reviews, $acl_candidate, $trust_candidate)
{
    if (!pay_glb_001_legal_entity_rows_valid($scope,$entities) ||
        !pay_glb_001_source_manifest_valid($scope,$batch,$manifest) ||
        !pay_glb_001_exact_keys($acl_candidate, array('candidate_consistent',
            'durable_ledger_verified','revocation_completeness_verified',
            'authorize_request','company_id','entity_count')) ||
        !pay_glb_001_exact_keys($trust_candidate, array('candidate_consistent',
            'server_trust_store_verified','registry_revocation_verified',
            'authorize_request','company_id','issuer_count')) ||
        $acl_candidate['candidate_consistent'] !== true ||
        $acl_candidate['durable_ledger_verified'] !== false ||
        $acl_candidate['revocation_completeness_verified'] !== false ||
        $trust_candidate['candidate_consistent'] !== true ||
        $trust_candidate['server_trust_store_verified'] !== false ||
        $trust_candidate['registry_revocation_verified'] !== false ||
        $acl_candidate['authorize_request'] !== false ||
        $trust_candidate['authorize_request'] !== false ||
        $acl_candidate['company_id'] !== $scope['company_id'] ||
        $trust_candidate['company_id'] !== $scope['company_id'] ||
        $acl_candidate['entity_count'] !== count($scope['entity_codes']) ||
        $trust_candidate['issuer_count'] !== 3 ||
        !is_array($reviews) || count($reviews) !== count($scope['entity_codes']) ||
        array_keys($reviews) !== range(0,count($reviews)-1)) return false;
    $entities_by_code=array(); $sources_by_code=array(); $rows_by_code=array();
    foreach ($entities as $entity) $entities_by_code[$entity['entity_code']]=$entity;
    foreach ($manifest['sources'] as $source) $sources_by_code[$source['entity_code']]=$source;
    foreach ($batch['rollup_rows'] as $row) $rows_by_code[$row['entity_code']]=$row;
    $seen=array();
    foreach ($reviews as $review) {
        if (!pay_glb_001_exact_keys($review,array('company_id','legal_entity_id',
                'entity_code','country_code','delivery_model','source_class',
                'mapping_version_sha256','residency_policy_sha256','reviewer_user_id',
                'approver_user_id','reviewed_at_utc','approved_at_utc',
                'expires_at_utc','status')) ||
            !is_int($review['company_id']) || $review['company_id'] !== $scope['company_id'] ||
            !pay_glb_001_code($review['entity_code']) ||
            !isset($entities_by_code[$review['entity_code']],
                $sources_by_code[$review['entity_code']],$rows_by_code[$review['entity_code']]) ||
            isset($seen[$review['entity_code']])) return false;
        $code=$review['entity_code']; $entity=$entities_by_code[$code];
        $source=$sources_by_code[$code]; $row=$rows_by_code[$code];
        if (!is_int($review['legal_entity_id']) ||
            $review['legal_entity_id'] !== $entity['legal_entity_id'] ||
            $review['country_code'] !== $entity['country_code'] ||
            $review['country_code'] !== $source['country_code'] ||
            $review['delivery_model'] !== $source['delivery_model'] ||
            $review['delivery_model'] !== $row['delivery_model'] ||
            $review['source_class'] !== $source['source_class'] ||
            $review['mapping_version_sha256'] !== $source['mapping_version_sha256'] ||
            !pay_glb_001_sha256($review['residency_policy_sha256']) ||
            !is_int($review['reviewer_user_id']) || !is_int($review['approver_user_id']) ||
            $review['reviewer_user_id'] <= 0 || $review['approver_user_id'] <= 0 ||
            $review['reviewer_user_id'] === $review['approver_user_id'] ||
            !pay_glb_001_iso_utc($review['reviewed_at_utc']) ||
            !pay_glb_001_iso_utc($review['approved_at_utc']) ||
            !pay_glb_001_iso_utc($review['expires_at_utc']) ||
            $review['reviewed_at_utc'] > $review['approved_at_utc'] ||
            $review['approved_at_utc'] > $scope['as_of_utc'] ||
            $review['expires_at_utc'] <= $scope['as_of_utc'] ||
            $review['status'] !== 'CANDIDATE_REVIEWED_NOT_ADMITTED') return false;
        $seen[$code]=true;
    }
    return count($seen) === count($scope['entity_codes']) ?
        array('candidate_consistent'=>true,'legal_approval_verified'=>false,
            'residency_authority_verified'=>false,'production_route_allowed'=>false,
            'close_ready'=>false,'company_id'=>$scope['company_id'],
            'entity_count'=>count($seen)) : false;
}
?>
