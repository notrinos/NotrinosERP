<?php
/**
 * HRM-TAL-001 bounded talent governance foundation contract.
 *
 * The first batch covers skills and certifications only. It deliberately adds
 * no recruitment, training, appraisal, payroll or employment-decision writer.
 */
if (!defined('HRM_TAL_001_CONTRACT_ID'))
    define('HRM_TAL_001_CONTRACT_ID', 'notrinoserp.hrm.talent.foundation.v1');
if (!defined('HRM_TAL_001_CONTRACT_VERSION'))
    define('HRM_TAL_001_CONTRACT_VERSION', '1.0.0');

/** @return array */
function hrm_tal_001_foundation_capabilities()
{
    return array('skills', 'certifications');
}

/** @return array */
function hrm_tal_001_governance_contract()
{
    return array(
        'contract_id' => HRM_TAL_001_CONTRACT_ID,
        'contract_version' => HRM_TAL_001_CONTRACT_VERSION,
        'optional_module' => true,
        'effective_history_required' => true,
        'object_security_required' => true,
        'purpose_limitation_required' => true,
        'data_minimization_required' => true,
        'skills_foundation_enabled' => true,
        'certifications_foundation_enabled' => true,
        'performance_runtime_enabled' => false,
        'learning_runtime_enabled' => false,
        'confidential_case_runtime_enabled' => false,
        'recruitment_mutation_allowed' => false,
        'training_mutation_allowed' => false,
        'appraisal_mutation_allowed' => false,
        'payroll_mutation_allowed' => false,
        'payroll_calculation_authority_allowed' => false,
        'compensation_mutation_allowed' => false,
        'employment_decision_authority_allowed' => false,
        'autonomous_employment_decision_allowed' => false,
        'cross_company_authority_allowed' => false,
        'raw_identity_document_storage_allowed' => false,
        'salary_bank_tax_detail_storage_allowed' => false,
        'writer_runtime_allowed' => false,
        'browser_ui_allowed' => false,
    );
}

/** @param string $database_version @return bool */
function hrm_tal_001_contract_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.937', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_skill_definition_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.938', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_skill_evidence_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.939', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_certification_definition_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.940', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_certification_evidence_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.941', '>=');
}


/** @param string $database_version @return bool */
function hrm_tal_001_catalog_writer_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.942', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_evidence_writer_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.943', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_read_only_view_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.944', '>=');
}

/** @return bool */
function hrm_tal_001_writer_runtime_allowed()
{
    return false;
}

/** @return bool */
function hrm_tal_001_browser_ui_allowed()
{
    return false;
}
?>
