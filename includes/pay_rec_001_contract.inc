<?php
/** PAY-REC-001 unified payroll-to-GL-bank-filing reconciliation architecture contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_rec_001_filing_completion_policy_contract.inc');
if (!defined('PAY_REC_001_CONTRACT_VERSION')) define('PAY_REC_001_CONTRACT_VERSION','1.0');
if (!defined('PAY_REC_001_REVIEW_PERMISSION')) define('PAY_REC_001_REVIEW_PERMISSION','SA_HRM_REVIEW_PAY_REC');
if (!defined('PAY_REC_001_APPROVE_PERMISSION')) define('PAY_REC_001_APPROVE_PERMISSION','SA_HRM_APPROVE_PAY_REC');
if (!defined('PAY_REC_001_CLOSE_PERMISSION')) define('PAY_REC_001_CLOSE_PERMISSION','SA_HRM_CLOSE_PAY_REC');
if (!defined('PAY_REC_001_RECOVER_PERMISSION')) define('PAY_REC_001_RECOVER_PERMISSION','SA_HRM_RECOVER_PAY_REC');
if (!defined('PAY_REC_001_VIEW_PERMISSION')) define('PAY_REC_001_VIEW_PERMISSION','SA_HRM_VIEW_PAY_REC');

function pay_rec_001_authority_contract()
{
    return array(
        'contract_version'=>PAY_REC_001_CONTRACT_VERSION,
        'evidence_sets'=>array('payroll','gl','payment','filing'),
        'exact_membership_required'=>true,
        'independent_recomputation_required'=>true,
        'versioned_tolerance_policy_required'=>true,
        'implicit_tolerance_allowed'=>false,
        'review_permission'=>PAY_REC_001_REVIEW_PERMISSION,
        'approve_permission'=>PAY_REC_001_APPROVE_PERMISSION,
        'close_permission'=>PAY_REC_001_CLOSE_PERMISSION,
        'recover_permission'=>PAY_REC_001_RECOVER_PERMISSION,
        'view_permission'=>PAY_REC_001_VIEW_PERMISSION,
        'reviewer_approver_separation_required'=>true,
        'maker_reviewer_separation_required'=>true,
        'immutable_signed_hash_required'=>true,
        'current_evidence_only'=>true,
        'stale_evidence_close_denied'=>true,
        'partial_payment_close_denied'=>true,
        'returned_payment_close_denied'=>true,
        'filing_correction_pending_close_denied'=>true,
        'unresolved_reversal_close_denied'=>true,
        'concurrent_or_replayed_close_denied'=>true,
        'historical_close_rewrite_allowed'=>false,
        'production_close_runtime_enabled'=>false,
        'development_close_state_mutation_enabled'=>false,
        'schema_writer_enabled'=>false,
        'automatic_role_grant_allowed'=>false,
        'gl_posting_authority_allowed'=>false,
        'payment_authority_allowed'=>false,
        'filing_submission_authority_allowed'=>false,
        'payroll_calculation_authority_allowed'=>false
    );
}
function pay_rec_001_contract_sha256(){return pay_rec_001_hash(pay_rec_001_authority_contract());}

/** Recompute a PAY-REC filing-completion receipt before using it for CloseReady. */
function pay_rec_001_validate_filing_completion_receipt(array $receipt,&$error=null)
{
    $error=null;
    $keys=array(
        'policy_sha256','close_mode','terminal_state','obligation_sha256','source_set_sha256',
        'generated_payload_sha256','generated_validation_receipt_sha256','local_validator_result',
        'repository_xsd_validation','statutory_acceptance_claimed','production_close_allowed',
        'completion_receipt_sha256'
    );
    $actual_keys=array_keys($receipt);sort($actual_keys,SORT_STRING);$expected_keys=$keys;sort($expected_keys,SORT_STRING);
    if($actual_keys!==$expected_keys)return pay_rec_001_fail($error,'filing_completion_receipt_incomplete');
    $policy=pay_rec_001_filing_completion_policy();
    if(!pay_rec_001_sha($receipt['policy_sha256'])||$receipt['policy_sha256']!==pay_rec_001_filing_completion_policy_sha256()
        ||$receipt['close_mode']!==$policy['development_close_mode']||$receipt['terminal_state']!==$policy['development_terminal_state']
        ||!pay_rec_001_sha($receipt['obligation_sha256'])||!pay_rec_001_sha($receipt['source_set_sha256'])
        ||!pay_rec_001_sha($receipt['generated_payload_sha256'])||!pay_rec_001_sha($receipt['generated_validation_receipt_sha256'])
        ||$receipt['local_validator_result']!=='PASS'||$receipt['repository_xsd_validation']!=='PASS'
        ||$receipt['statutory_acceptance_claimed']!==false||$receipt['production_close_allowed']!==false
        ||!pay_rec_001_sha($receipt['completion_receipt_sha256']))
        return pay_rec_001_fail($error,'filing_completion_receipt_invalid');
    $body=$receipt;unset($body['completion_receipt_sha256']);$expected_sha=pay_rec_001_hash($body);
    if($expected_sha===false||!hash_equals($expected_sha,(string)$receipt['completion_receipt_sha256']))return pay_rec_001_fail($error,'filing_completion_receipt_hash_mismatch');
    return $receipt;
}

/** Pure development-readiness evaluator. It does not mutate or close a payroll run. */
function pay_rec_001_evaluate_development_close_readiness(array $sets,array $filing_completion,array $policy,&$error=null)
{
    $error=null;$required=array('payroll','gl','payment','filing');
    foreach($required as $name){if(!isset($sets[$name])||!is_array($sets[$name]))return pay_rec_001_fail($error,'reconciliation_evidence_set_missing');$set=$sets[$name];foreach(array('evidence_sha256','status','current') as $k)if(!array_key_exists($k,$set))return pay_rec_001_fail($error,'reconciliation_evidence_set_incomplete');if(!pay_rec_001_sha($set['evidence_sha256'])||$set['status']!=='matched'||$set['current']!==true)return pay_rec_001_fail($error,'reconciliation_evidence_not_close_ready');}
    $completion_error=null;if(pay_rec_001_validate_filing_completion_receipt($filing_completion,$completion_error)===false)return pay_rec_001_fail($error,$completion_error);
    foreach(array('policy_version','tolerance_sha256','rounding_sha256') as $k)if(!array_key_exists($k,$policy))return pay_rec_001_fail($error,'reconciliation_policy_incomplete');
    if(pay_rec_001_token($policy['policy_version'],32)===false||!pay_rec_001_sha($policy['tolerance_sha256'])||!pay_rec_001_sha($policy['rounding_sha256']))return pay_rec_001_fail($error,'reconciliation_policy_invalid');
    $receipt=array('contract_sha256'=>pay_rec_001_contract_sha256(),'mode'=>'LOCAL_DEVELOPMENT_RECONCILIATION','payroll_evidence_sha256'=>$sets['payroll']['evidence_sha256'],'gl_evidence_sha256'=>$sets['gl']['evidence_sha256'],'payment_evidence_sha256'=>$sets['payment']['evidence_sha256'],'filing_evidence_sha256'=>$sets['filing']['evidence_sha256'],'filing_completion_receipt_sha256'=>$filing_completion['completion_receipt_sha256'],'policy_version'=>$policy['policy_version'],'tolerance_sha256'=>$policy['tolerance_sha256'],'rounding_sha256'=>$policy['rounding_sha256'],'close_ready'=>true,'state_mutation_authorized'=>false,'production_close_authorized'=>false);
    $receipt['readiness_sha256']=pay_rec_001_hash($receipt);return $receipt;
}
?>
