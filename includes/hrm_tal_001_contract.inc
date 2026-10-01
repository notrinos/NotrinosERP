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
        'performance_foundation_enabled' => true,
        'performance_runtime_enabled' => false,
        'performance_cycle_writer_source_available' => true,
        'performance_evidence_writer_source_available' => true,
        'performance_read_only_source_available' => true,
        'performance_browser_ui_enabled' => true,
        'performance_manager_view_enabled' => true,
        'performance_self_service_view_enabled' => true,
        'performance_workflow_source_available' => true,
        'performance_deterministic_scoring_source_available' => true,
        'performance_export_enabled' => true,
        'performance_compensation_linkage_allowed' => false,
        'learning_foundation_enabled' => true,
        'learning_runtime_enabled' => true,
        'learning_writer_source_available' => true,
        'learning_read_only_source_available' => true,
        'learning_browser_ui_enabled' => false,
        'learning_self_service_view_enabled' => false,
        'learning_manager_view_enabled' => false,
        'learning_export_enabled' => false,
        'learning_legacy_training_bridge_allowed' => false,
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

/** @param string $database_version @return bool */
function hrm_tal_001_governed_admin_ui_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.945', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_browser_navigation_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.946', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_self_service_view_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.947', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_manager_scope_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.948', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_manager_team_view_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.949', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_manager_browser_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.950', '>=');
}


/** @param string $database_version @return bool */
function hrm_tal_001_export_audit_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.951', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_export_service_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.952', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_export_browser_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.953', '>=');
}


/** @param string $database_version @return bool */
function hrm_tal_001_performance_cycle_definition_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.954', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_review_subject_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.955', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_assessment_evidence_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.956', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_cycle_writer_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.957', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_evidence_writer_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.958', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_read_only_view_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.959', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_admin_browser_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.960', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_self_service_view_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.961', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_manager_team_view_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.962', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_workflow_event_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.963', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_workflow_writer_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.964', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_score_finalization_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.965', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_export_contract_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.966', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_export_service_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.967', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_performance_export_browser_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.968', '>=');
}


/** @param string $database_version @return bool */
function hrm_tal_001_learning_authority_contract_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.969', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_learning_definition_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.970', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_learning_assignment_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.971', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_learning_completion_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.972', '>=');
}

/** @param string $database_version @return bool */
function hrm_tal_001_learning_runtime_service_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.973', '>=');
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
