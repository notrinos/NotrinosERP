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

/** @return array|false */
function hrm_tal_001_read_performance_for_self(&$error = null)
{
    $error = null;
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_performance_self_service_view_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'performance_self_service_not_active');
    $scope = hrm_tal_001_self_service_scope($error);
    if ($scope === false) return false;
    $company = hrm_tal_001_runtime_company_id();
    $employee = (string)$scope['employee_id'];
    $sql = 'SELECT s.review_type_code,s.review_window_start,s.review_window_end,'
        .'c.cycle_code,c.cycle_name,c.purpose_code,c.period_start,c.period_end,c.rating_scale_code,'
        .'a.assessment_role_code,a.dimension_code,a.rating_value,a.evidence_date '
        .'FROM '.TB_PREF.'hrm_tal_001_performance_review_subjects s INNER JOIN '.TB_PREF.'hrm_tal_001_performance_cycle_definitions c '
        .'ON c.performance_cycle_definition_id=s.performance_cycle_definition_id AND c.company_id=s.company_id '
        .'LEFT JOIN '.TB_PREF.'hrm_tal_001_performance_assessment_evidence a ON a.performance_review_subject_id=s.performance_review_subject_id AND a.company_id=s.company_id '
        .'WHERE s.company_id='.(int)$company.' AND s.employee_id='.db_escape($employee)
        .' ORDER BY s.review_window_start DESC,s.performance_review_subject_id DESC,a.evidence_date DESC,a.performance_assessment_evidence_id DESC';
    $result = db_query($sql);
    if (!$result) return hrm_tal_001_catalog_fail($error, 'performance_self_query_failed');
    $rows = array(); while ($row = db_fetch_assoc($result)) $rows[] = $row; return $rows;
}
?>
