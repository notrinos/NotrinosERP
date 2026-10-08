<?php
/** PAY-CON-001 dependency and authority readiness guards. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_con_001_contract.inc');

function pay_con_001_fail_patch($patch, $message)
{
    return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT, $message);
}

function pay_con_001_predecessor_files()
{
    return array(
        'tests/fixtures/payroll/legacy_v1/manifest_v1.0.542.json' => '929ef868f55d014514747edd3c83ed8caffc1e473a2118e1d32b64c490adbdd0',
        'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/EXECUTION_REPORT_PAY_REC_001_CLOSURE_ACCEPTANCE_2026-10-08.md' => '013f599de785c00bd70249c6b9d7eeb9d4072641e8616dc1bdc43d1adb601967',
        'Testting-Planning-Migration/HRM_GLOBAL_PAYROLL_IMPROVEMENT/IMPLEMENTATION_BACKLOG_PAY_REC_001_CLOSURE_ACCEPTANCE_2026-10-08.md' => '1624ec77c50fd4f1c6d5c32069e04219aa7eefc9f0508cd40ae4b25c59b1a22a',
        'includes/pay_api_001_contract.inc' => '2ee401889ff95195744b0c204edea07b11ec67f4e5329ec60f365fb08d518b02'
    );
}

function pay_con_001_require_predecessor($patch)
{
    global $path_to_root;
    foreach (pay_con_001_predecessor_files() as $rel => $sha) {
        $file = $path_to_root.'/'.$rel;
        if (!is_file($file) || !hash_equals($sha, hash_file('sha256', $file)))
            return pay_con_001_fail_patch($patch, 'PAY-CON-001 immutable predecessor custody mismatch: '.$rel.'.');
    }
    return true;
}

function pay_con_001_require_dependencies($patch)
{
    global $path_to_root;
    $path = $path_to_root.'/tests/fixtures/payroll/legacy_v1/manifest_v1.0.542.json';
    $manifest = json_decode((string)@file_get_contents($path), true);
    if (!is_array($manifest))
        return pay_con_001_fail_patch($patch, 'PAY-CON-001 predecessor manifest is unreadable.');

    $api = isset($manifest['pay_api_001_evidence']) ? $manifest['pay_api_001_evidence'] : array();
    $rec = isset($manifest['pay_rec_001_closure_acceptance_evidence']) ? $manifest['pay_rec_001_closure_acceptance_evidence'] : array();
    if ((isset($api['status']) ? $api['status'] : '') !== 'CLOSED / LOCAL ACCEPTANCE PASS'
        || (isset($api['database_version']) ? $api['database_version'] : '') !== '1.0.905')
        return pay_con_001_fail_patch($patch, 'PAY-CON-001 requires accepted PAY-API-001 closure custody.');
    if ((isset($rec['status']) ? $rec['status'] : '') !== 'CLOSED / DONE / LOCAL ACCEPTANCE PASS'
        || (isset($rec['database_version']) ? $rec['database_version'] : '') !== '1.0.1060'
        || (isset($rec['mandatory_local_gate_result']) ? $rec['mandatory_local_gate_result'] : '') !== 'PASS'
        || (isset($rec['pay_con_001_dependency_blocked_by_pay_rec']) ? $rec['pay_con_001_dependency_blocked_by_pay_rec'] : true) !== false)
        return pay_con_001_fail_patch($patch, 'PAY-CON-001 requires accepted PAY-REC-001 closure and explicit dependency release.');
    return true;
}

function pay_con_001_require_contract($patch)
{
    $authority = pay_con_001_authority_contract();
    $required_true = array(
        'pay_api_closed_required','pay_rec_closed_required','named_provider_due_diligence_required_before_writer',
        'data_residency_review_required_before_writer','delivery_model_disclosure_required','staged_shadow_exchange_required',
        'schema_validation_required','mapping_version_pinning_required','idempotency_required','reconciliation_required',
        'scoped_secret_reference_required_before_transport','immutable_provider_profile_custody','immutable_mapping_version_custody'
    );
    foreach ($required_true as $name)
        if (!isset($authority[$name]) || $authority[$name] !== true)
            return pay_con_001_fail_patch($patch, 'PAY-CON-001 required contract capability missing: '.$name.'.');

    $required_false = array(
        'provider_profile_writer_allowed','mapping_version_writer_allowed','shadow_export_runtime_allowed','shadow_import_runtime_allowed',
        'external_http_route_allowed','provider_adapter_runtime_allowed','outbound_network_authority_allowed','inbound_network_authority_allowed',
        'cleartext_secret_storage_allowed','connector_secret_custody_allowed','provider_payroll_execution_authority_allowed',
        'payroll_result_authority_allowed','payroll_state_mutation_allowed','gl_posting_authority_allowed','payment_execution_authority_allowed',
        'filing_submission_authority_allowed','approval_bypass_allowed','audit_bypass_allowed','cross_company_authority_allowed',
        'historical_payroll_accounting_filing_mutation_allowed','automatic_role_grant_allowed','production_provider_exchange_allowed'
    );
    foreach ($required_false as $name)
        if (!array_key_exists($name, $authority) || $authority[$name] !== false)
            return pay_con_001_fail_patch($patch, 'PAY-CON-001 authority ceiling violated: '.$name.'.');
    return true;
}
?>
