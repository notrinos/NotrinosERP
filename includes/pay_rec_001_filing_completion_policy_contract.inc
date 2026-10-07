<?php
/** PAY-REC-001 filing completion policy for reconciliation close-readiness. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_rec_001_filing_monetary_lineage_contract.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_generated_validation_contract.inc');
if (!defined('PAY_REC_001_FILING_COMPLETION_POLICY_CONTRACT_VERSION'))
    define('PAY_REC_001_FILING_COMPLETION_POLICY_CONTRACT_VERSION','1.0');

function pay_rec_001_filing_completion_policy()
{
    return array(
        'contract_version'=>PAY_REC_001_FILING_COMPLETION_POLICY_CONTRACT_VERSION,
        'development_close_mode'=>'LOCAL_DEVELOPMENT_RECONCILIATION',
        'development_terminal_state'=>'LOCAL_VALIDATION_ACCEPTED_FOR_RECONCILIATION',
        'accepted_pay_fil_source_version'=>'1.0.1045',
        'accepted_pay_fil_custody_version'=>'1.0.535',
        'required_pay_fil_status'=>'CLOSED / LOCAL ACCEPTANCE PASS',
        'required_development_gate'=>'OFFICIAL_LOCAL_VALIDATOR_COMPATIBILITY',
        'required_local_validator_result'=>'PASS',
        'required_repository_xsd_validation'=>'PASS',
        'hash_bound_generated_payload_required'=>true,
        'hash_bound_validation_receipt_required'=>true,
        'submission_acknowledgement_required_for_development_close'=>false,
        'production_statutory_acceptance_claimed'=>false,
        'production_close_allowed'=>false,
        'portal_submission_authority_allowed'=>false,
        'network_authority_allowed'=>false,
        'historical_filing_mutation_allowed'=>false,
        'policy_scope'=>'development_reconciliation_only_not_statutory_acceptance'
    );
}
function pay_rec_001_filing_completion_policy_sha256(){return pay_rec_001_hash(pay_rec_001_filing_completion_policy());}

function pay_rec_001_validate_filing_completion_evidence(array $context,array $evidence,&$error=null)
{
    $error=null;$policy=pay_rec_001_filing_completion_policy();
    foreach(array('close_mode','obligation_sha256','source_set_sha256') as $k)if(!array_key_exists($k,$context))return pay_rec_001_fail($error,'filing_completion_context_incomplete');
    if($context['close_mode']!==$policy['development_close_mode']||!pay_rec_001_sha($context['obligation_sha256'])||!pay_rec_001_sha($context['source_set_sha256']))return pay_rec_001_fail($error,'filing_completion_context_invalid');
    $keys=array('status','source_version','database_version','source_custody_version','generated_payload_sha256','generated_validation_receipt_sha256','repository_xsd_validation','local_validator','local_validator_version','local_validator_result','production_transmission_required_for_closure','production_transmission_allowed','portal_submission_allowed');
    foreach($keys as $k)if(!array_key_exists($k,$evidence))return pay_rec_001_fail($error,'filing_completion_evidence_incomplete');
    if($evidence['status']!==$policy['required_pay_fil_status']||$evidence['source_version']!==$policy['accepted_pay_fil_source_version']||$evidence['database_version']!==$policy['accepted_pay_fil_source_version']||$evidence['source_custody_version']!==$policy['accepted_pay_fil_custody_version']
        ||!pay_rec_001_sha($evidence['generated_payload_sha256'])||!pay_rec_001_sha($evidence['generated_validation_receipt_sha256'])||$evidence['repository_xsd_validation']!=='PASS'
        ||!is_string($evidence['local_validator'])||trim($evidence['local_validator'])===''||!is_string($evidence['local_validator_version'])||trim($evidence['local_validator_version'])===''||$evidence['local_validator_result']!=='PASS'
        ||$evidence['production_transmission_required_for_closure']!==false||$evidence['production_transmission_allowed']!==false||$evidence['portal_submission_allowed']!==false)
        return pay_rec_001_fail($error,'filing_completion_evidence_invalid');
    $receipt=array('policy_sha256'=>pay_rec_001_filing_completion_policy_sha256(),'close_mode'=>$context['close_mode'],'terminal_state'=>$policy['development_terminal_state'],'obligation_sha256'=>$context['obligation_sha256'],'source_set_sha256'=>$context['source_set_sha256'],'generated_payload_sha256'=>$evidence['generated_payload_sha256'],'generated_validation_receipt_sha256'=>$evidence['generated_validation_receipt_sha256'],'local_validator_result'=>'PASS','repository_xsd_validation'=>'PASS','statutory_acceptance_claimed'=>false,'production_close_allowed'=>false);
    $receipt['completion_receipt_sha256']=pay_rec_001_hash($receipt);return $receipt;
}
?>
