<?php
/** HRM-TAL-001 session-derived ESS self-only skills/certifications read service. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_catalog_db.inc');
include_once($path_to_root.'/hrm/includes/db/ess_self_scope_db.inc');

/** @return array|false */
function hrm_tal_001_self_service_scope(&$error)
{
    $error = null;
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_self_service_view_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'self_service_view_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRM_ESS_VIEW_SELF'))
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    $scope = get_hrm_ess_current_self_scope();
    if (!is_array($scope) || empty($scope['employee_id']) || strlen((string)$scope['employee_id']) > 20)
        return hrm_tal_001_catalog_fail($error, 'self_scope_unavailable');
    return $scope;
}

/** @return array|false */
function hrm_tal_001_read_skill_evidence_for_self(&$error = null)
{
    $scope = hrm_tal_001_self_service_scope($error); if ($scope === false) return false;
    $company = hrm_tal_001_runtime_company_id(); $employee = (string)$scope['employee_id'];
    $sql = 'SELECT e.evidence_type_code,e.proficiency_value,e.evidence_date,e.valid_from,e.valid_to,'
        .'d.skill_code,d.skill_name,d.category_code,d.proficiency_scale_code '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_skill_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_skill_definitions d '
        .'ON d.skill_definition_id=e.skill_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id='.db_escape($employee).' ORDER BY e.evidence_date DESC,e.employee_skill_evidence_id DESC';
    $result = db_query($sql); if (!$result) return hrm_tal_001_catalog_fail($error, 'self_skill_query_failed');
    $rows = array(); while ($row = db_fetch_assoc($result)) $rows[] = $row; return $rows;
}

/** @return array|false */
function hrm_tal_001_read_certification_evidence_for_self(&$error = null)
{
    $scope = hrm_tal_001_self_service_scope($error); if ($scope === false) return false;
    $company = hrm_tal_001_runtime_company_id(); $employee = (string)$scope['employee_id'];
    $sql = 'SELECT e.issued_on,e.expires_on,e.verification_status_code,'
        .'d.certification_code,d.certification_name,d.issuer_name,d.category_code '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_certification_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_certification_definitions d '
        .'ON d.certification_definition_id=e.certification_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id='.db_escape($employee).' ORDER BY e.issued_on DESC,e.employee_certification_evidence_id DESC';
    $result = db_query($sql); if (!$result) return hrm_tal_001_catalog_fail($error, 'self_certification_query_failed');
    $rows = array(); while ($row = db_fetch_assoc($result)) $rows[] = $row; return $rows;
}
?>
