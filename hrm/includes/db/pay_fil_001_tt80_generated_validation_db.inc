<?php
/** Source-only PAY-FIL-001 generated-payload validation/readiness helpers. No writer or egress authority. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_render_model_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_generated_validation_contract.inc');

/** Recheck one in-memory generation result against the frozen generated-validation contract. */
function pay_fil_001_tt80_prepare_generated_validation_candidate($filing_source_set_id,array $context,&$error=null)
{
    $generated=pay_fil_001_generate_tt80_local_validation_xml($filing_source_set_id,$context,$error);
    if($generated===false)return false;
    return pay_fil_001_tt80_validate_generated_candidate($generated,$error);
}

/** Source guard for the validation candidate path; migrations may only enable read-only/local-validation behavior. */
function pay_fil_001_tt80_generated_validation_require_runtime_source($patch)
{
    global $path_to_root;
    if(!pay_fil_001_tt80_runtime_generation_require_activation_source($patch))return false;
    $contract=@file_get_contents($path_to_root.'/includes/pay_fil_001_tt80_generated_validation_contract.inc');
    $runtime=@file_get_contents($path_to_root.'/hrm/includes/db/pay_fil_001_tt80_generated_validation_db.inc');
    foreach(array('HASH_BOUND_GENERATED_PAYLOAD_OBSERVATION','raw_payload_persistence_allowed','production_acceptance_claimed','pay_fil_001_tt80_validate_generated_candidate','pay_fil_001_tt80_generated_validation_receipt') as $token)
        if(!is_string($contract)||strpos($contract,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 generated-validation contract source incomplete: '.$token.'.');
    $start=is_string($runtime)?strpos($runtime,'function pay_fil_001_tt80_prepare_generated_validation_candidate'):false;
    $cut=is_string($runtime)?strpos($runtime,'function pay_fil_001_tt80_generated_validation_require_runtime_source'):false;
    $body=($start===false||$cut===false||$cut<=$start)?false:substr($runtime,$start,$cut-$start);
    foreach(array('pay_fil_001_generate_tt80_local_validation_xml','pay_fil_001_tt80_validate_generated_candidate') as $token)
        if(!is_string($body)||strpos($body,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 generated-validation runtime source incomplete: '.$token.'.');
    foreach(array('INSERT INTO','UPDATE ','DELETE FROM','begin_transaction(','commit_transaction(','rollback_transaction(','file_put_contents(','curl_','fsockopen(','stream_socket_client(','openssl_sign(') as $token)
        if(stripos((string)$body,$token)!==false||stripos((string)$contract,$token)!==false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 generated-validation source acquired forbidden authority: '.$token.'.');
    return true;
}

/** Closure remains pending until the exact generated-payload local mandatory gate is actually executed. */
function pay_fil_001_tt80_closure_readiness_profile()
{
    return array(
        'milestone'=>'PAY-FIL-001',
        'source_readiness'=>'READY_FOR_LOCAL_MANDATORY_GATE',
        'local_mandatory_gate_required'=>true,
        'development_acceptance_requires_generated_payload_xsd_and_local_validator_pass'=>true,
        'production_transmission_required_for_development_closure'=>false,
        'production_acceptance'=>'PRODUCTION-ENVIRONMENT-PENDING / OUTSIDE CURRENT DEVELOPMENT PLAN',
        'artifact_publication_allowed'=>false,'production_generation_allowed'=>false,'production_signing_allowed'=>false,
        'production_credentials_allowed'=>false,'network_authority_allowed'=>false,'production_transmission_allowed'=>false,
        'portal_submission_allowed'=>false,'historical_mutation_allowed'=>false
    );
}

function pay_fil_001_tt80_closure_require_source($patch)
{
    global $path_to_root;
    if(!pay_fil_001_tt80_generated_validation_require_runtime_source($patch))return false;
    $runner='tests/php/run_pay_fil_001_tt80_generated_validation_closure_mandatory_blockers_browser.php';
    $test='tests/php/test_pay_fil_001_tt80_generated_payload_local_compatibility.php';
    foreach(array($runner,$test) as $relative)if(!is_file($path_to_root.'/'.$relative))return pay_fil_001_fail_patch($patch,'PAY-FIL-001 closure readiness source missing: '.$relative.'.');
    $raw=@file_get_contents($path_to_root.'/'.$runner);
    foreach(array('pay-fil-001-tt80-generated-validation-closure-v2-20261007','1.0.1045','1.0.535','PAY-FIL-001 CLOSED / LOCAL ACCEPTANCE PASS','PAY-CON-001 remains blocked') as $token)
        if(!is_string($raw)||strpos($raw,$token)===false)return pay_fil_001_fail_patch($patch,'PAY-FIL-001 closure runner source incomplete: '.$token.'.');
    return true;
}
?>
