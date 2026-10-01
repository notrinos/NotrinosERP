<?php
/** HRM-TAL-001 minimized read-only current manager/team talent view. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_manager_scope_db.inc');

/** @return array|false */
function hrm_tal_001_manager_team_view_subjects($as_of, &$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_manager_team_view_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'manager_team_view_not_active');
    return hrm_tal_001_manager_scope_subjects($as_of, $error);
}

/** @return array */
function hrm_tal_001_manager_subject_index(array $subjects)
{
    $index = array();
    foreach ($subjects as $subject)
        $index[(string)$subject['employee_id']] = $subject;
    return $index;
}

/** @return string */
function hrm_tal_001_manager_employee_in_list(array $subjects)
{
    $ids = array();
    foreach ($subjects as $subject)
        $ids[] = db_escape((string)$subject['employee_id']);
    return implode(',', $ids);
}

/** @return array|false */
function hrm_tal_001_read_skill_evidence_for_manager_team($as_of, &$error = null)
{
    $error = null;
    $subjects = hrm_tal_001_manager_team_view_subjects($as_of, $error);
    if ($subjects === false) return false;
    if (!$subjects) return array();
    $subject_index = hrm_tal_001_manager_subject_index($subjects);
    $company = hrm_tal_001_runtime_company_id();
    $sql = 'SELECT e.employee_id,e.evidence_type_code,e.proficiency_value,e.evidence_date,e.valid_from,e.valid_to,'
        .'d.skill_code,d.skill_name,d.category_code,d.proficiency_scale_code '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_skill_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_skill_definitions d '
        .'ON d.skill_definition_id=e.skill_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id IN ('.hrm_tal_001_manager_employee_in_list($subjects).') '
        .'ORDER BY e.employee_id,e.evidence_date DESC,e.employee_skill_evidence_id DESC';
    $result = db_query($sql);
    if (!$result) return hrm_tal_001_catalog_fail($error, 'manager_skill_query_failed');
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $employee = (string)$row['employee_id'];
        if (!isset($subject_index[$employee]))
            return hrm_tal_001_catalog_fail($error, 'manager_skill_scope_projection_mismatch');
        $row['scope_origin'] = (string)$subject_index[$employee]['scope_origin'];
        $rows[] = $row;
    }
    return $rows;
}

/** @return array|false */
function hrm_tal_001_read_certification_evidence_for_manager_team($as_of, &$error = null)
{
    $error = null;
    $subjects = hrm_tal_001_manager_team_view_subjects($as_of, $error);
    if ($subjects === false) return false;
    if (!$subjects) return array();
    $subject_index = hrm_tal_001_manager_subject_index($subjects);
    $company = hrm_tal_001_runtime_company_id();
    $sql = 'SELECT e.employee_id,e.issued_on,e.expires_on,e.verification_status_code,'
        .'d.certification_code,d.certification_name,d.issuer_name,d.category_code '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_certification_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_certification_definitions d '
        .'ON d.certification_definition_id=e.certification_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id IN ('.hrm_tal_001_manager_employee_in_list($subjects).') '
        .'ORDER BY e.employee_id,e.issued_on DESC,e.employee_certification_evidence_id DESC';
    $result = db_query($sql);
    if (!$result) return hrm_tal_001_catalog_fail($error, 'manager_certification_query_failed');
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $employee = (string)$row['employee_id'];
        if (!isset($subject_index[$employee]))
            return hrm_tal_001_catalog_fail($error, 'manager_certification_scope_projection_mismatch');
        $row['scope_origin'] = (string)$subject_index[$employee]['scope_origin'];
        $rows[] = $row;
    }
    return $rows;
}

/** @return array|false */
function hrm_tal_001_read_performance_for_manager_team($as_of, &$error = null)
{
    $error = null;
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_performance_manager_team_view_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'performance_manager_team_not_active');
    $subjects = hrm_tal_001_manager_scope_subjects($as_of, $error);
    if ($subjects === false) return false;
    if (!$subjects) return array();
    $subject_index = hrm_tal_001_manager_subject_index($subjects);
    $company = hrm_tal_001_runtime_company_id();
    $sql = 'SELECT s.employee_id,s.review_type_code,s.review_window_start,s.review_window_end,'
        .'c.cycle_code,c.cycle_name,c.purpose_code,c.period_start,c.period_end,c.rating_scale_code,'
        .'a.assessment_role_code,a.dimension_code,a.rating_value,a.evidence_date '
        .'FROM '.TB_PREF.'hrm_tal_001_performance_review_subjects s INNER JOIN '.TB_PREF.'hrm_tal_001_performance_cycle_definitions c '
        .'ON c.performance_cycle_definition_id=s.performance_cycle_definition_id AND c.company_id=s.company_id '
        .'LEFT JOIN '.TB_PREF.'hrm_tal_001_performance_assessment_evidence a ON a.performance_review_subject_id=s.performance_review_subject_id AND a.company_id=s.company_id '
        .'WHERE s.company_id='.(int)$company.' AND s.employee_id IN ('.hrm_tal_001_manager_employee_in_list($subjects).') '
        .'ORDER BY s.employee_id,s.review_window_start DESC,s.performance_review_subject_id DESC,a.evidence_date DESC,a.performance_assessment_evidence_id DESC';
    $result = db_query($sql);
    if (!$result) return hrm_tal_001_catalog_fail($error, 'performance_manager_query_failed');
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $employee = (string)$row['employee_id'];
        if (!isset($subject_index[$employee])) return hrm_tal_001_catalog_fail($error, 'manager_performance_scope_projection_mismatch');
        $row['scope_origin'] = (string)$subject_index[$employee]['scope_origin'];
        $rows[] = $row;
    }
    return $rows;
}
?>
