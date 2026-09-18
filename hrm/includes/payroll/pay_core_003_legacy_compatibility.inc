<?php
/**
 * Non-authoritative legacy compatibility adapters for PAY-CORE-003.
 * They build explicit candidates only after a hash-bound approved mapping exists.
 */
include_once(dirname(__FILE__).'/pay_core_003_governance.inc');
include_once(dirname(__FILE__).'/pay_core_003_legacy_mapping_review.inc');
if (!defined('HRM_PAY_CORE_003_COMPATIBILITY_PRODUCTION_AUTHORITY')) define('HRM_PAY_CORE_003_COMPATIBILITY_PRODUCTION_AUTHORITY',false);
function hrm_pay_core_003_legacy_mapping_candidate($source_kind,$legacy_row,$approved_resolution)
{
    $source_sha=hrm_pay_core_003_legacy_source_sha256($source_kind,$legacy_row);
    if ($source_sha===false || !hrm_pay_core_003_validate_mapping_resolution($source_kind,$approved_resolution)) return false;
    $candidate=array('source_kind'=>$source_kind,'source_sha256'=>$source_sha,'target_kind'=>$approved_resolution['target_kind'],'target_payload_sha256'=>hrm_pay_core_003_hash($approved_resolution),'authoritative'=>false);
    if ($approved_resolution['target_kind']==='approved_input') {
        if ($source_kind!=='employee_salary' || trim((string)$legacy_row['formula'])!=='' || !hrm_pay_core_003_validate_typed_value($approved_resolution['value_type'],$legacy_row['amount'])) return false;
        $candidate['typed_value_source']='legacy_amount';
        $candidate['typed_value']=$legacy_row['amount'];
    }
    return $candidate;
}
function hrm_pay_core_003_compatibility_evidence_projection($candidate)
{
    if (!is_array($candidate) || empty($candidate['source_sha256']) || empty($candidate['target_payload_sha256'])) return false;
    return array('source_sha256'=>$candidate['source_sha256'],'target_payload_sha256'=>$candidate['target_payload_sha256'],'target_kind'=>$candidate['target_kind'],'authoritative'=>false);
}
