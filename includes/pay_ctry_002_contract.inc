<?php
/**
 * PAY-CTRY-002 lifecycle/activation contract foundation.
 *
 * PAY-CTRY-001 owns site trust and immutable verified pack artifacts.
 * PAY-CTRY-002 adds governed lifecycle evidence but does not make installation
 * equivalent to company/legal-entity/payroll activation.
 */
if (!defined('PAY_CTRY_002_CONTRACT_VERSION')) define('PAY_CTRY_002_CONTRACT_VERSION', '1.0.0');
if (!defined('PAY_CTRY_002_BATCH_DB_VERSION')) define('PAY_CTRY_002_BATCH_DB_VERSION', '1.0.779');

if (!defined('PAY_CTRY_002_MAKER_PERMISSION')) define('PAY_CTRY_002_MAKER_PERMISSION', 'SA_HRM_MANAGE_PAY_CTRY_ACTIVATION');
if (!defined('PAY_CTRY_002_CHECKER_PERMISSION')) define('PAY_CTRY_002_CHECKER_PERMISSION', 'SA_HRM_REVIEW_PAY_CTRY_ACTIVATION');
if (!defined('PAY_CTRY_002_EXECUTOR_PERMISSION')) define('PAY_CTRY_002_EXECUTOR_PERMISSION', 'SA_HRM_EXECUTE_PAY_CTRY_ACTIVATION');
if (!defined('PAY_CTRY_002_RECOVERY_PERMISSION')) define('PAY_CTRY_002_RECOVERY_PERMISSION', 'SA_HRM_RECOVER_PAY_CTRY_ACTIVATION');

function pay_ctry_002_site_operation_types()
{
    return array('install_artifact','upgrade_artifact','rollback_artifact','restore_reverify','trust_recheck','hard_revocation_propagation');
}
function pay_ctry_002_site_operation_events()
{
    return array('requested','verified','applied','failed','recovered');
}
function pay_ctry_002_activation_actions()
{
    return array('activate','upgrade','rollback','deactivate','suspend','resume','hard_revoke');
}
function pay_ctry_002_scope_types()
{
    return array('company','legal_entity','payroll_scope');
}
function pay_ctry_002_contract_invariants()
{
    return array(
        'installation_implies_activation'=>false,
        'activation_inferred_from_installation'=>false,
        'site_trust_admin_may_activate_company'=>false,
        'installer_may_self_approve_activation'=>false,
        'maker_may_execute_own_activation'=>false,
        'checker_may_execute_approved_activation'=>false,
        'historical_activation_evidence_mutable'=>false,
        'hard_revocation_allows_new_execution'=>false,
        'historical_payroll_rewrite_allowed'=>false,
        'automatic_pack_activation_allowed'=>false,
        'activation_requires_exact_pack_root'=>true,
        'activation_requires_impact_evidence'=>true,
        'activation_requires_independent_approval'=>true,
        'emergency_suspension_preserves_evidence'=>true
    );
}
?>
