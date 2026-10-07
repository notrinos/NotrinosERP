<?php
/** PAY-REC-001 immutable reconciliation/profile/checklist custody contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_rec_001_contract.inc');
if (!defined('PAY_REC_001_CUSTODY_CONTRACT_VERSION')) define('PAY_REC_001_CUSTODY_CONTRACT_VERSION','1.0');
if (!defined('PAY_REC_001_PREPARE_PERMISSION')) define('PAY_REC_001_PREPARE_PERMISSION','SA_HRM_PREPARE_PAY_REC');

/** Return the bounded zero-seed PAY-REC custody foundation contract. */
function pay_rec_001_custody_contract()
{
    return array(
        'contract_version'=>PAY_REC_001_CUSTODY_CONTRACT_VERSION,
        'custody_status'=>'ZERO_SEED_IMMUTABLE_FOUNDATION_NO_RUNTIME_WRITER',
        'tables'=>array('hrm_pay_rec_001_profiles','hrm_pay_rec_001_evidence_sets','hrm_pay_rec_001_checklists'),
        'evidence_kinds'=>array('payroll','gl','payment','filing'),
        'prepare_permission'=>PAY_REC_001_PREPARE_PERMISSION,
        'review_permission'=>PAY_REC_001_REVIEW_PERMISSION,
        'approve_permission'=>PAY_REC_001_APPROVE_PERMISSION,
        'close_permission'=>PAY_REC_001_CLOSE_PERMISSION,
        'recover_permission'=>PAY_REC_001_RECOVER_PERMISSION,
        'view_permission'=>PAY_REC_001_VIEW_PERMISSION,
        'exact_membership_hash_required'=>true,
        'independent_recomputation_required'=>true,
        'filing_completion_receipt_hash_required'=>true,
        'reviewer_approver_separation_required'=>true,
        'automatic_role_grant_allowed'=>false,
        'runtime_writer_enabled'=>false,
        'review_runtime_enabled'=>false,
        'approval_runtime_enabled'=>false,
        'close_state_mutation_enabled'=>false,
        'production_close_allowed'=>false,
        'gl_posting_authority_allowed'=>false,
        'payment_execution_authority_allowed'=>false,
        'filing_submission_authority_allowed'=>false,
        'network_authority_allowed'=>false,
        'payroll_calculation_authority_allowed'=>false,
        'historical_payroll_accounting_filing_mutation_allowed'=>false
    );
}

/** Return the canonical SHA-256 for the PAY-REC custody foundation contract. */
function pay_rec_001_custody_contract_sha256()
{
    return pay_rec_001_hash(pay_rec_001_custody_contract());
}
?>