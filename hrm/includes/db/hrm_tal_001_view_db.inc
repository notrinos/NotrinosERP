<?php
/** HRM-TAL-001 restricted read-only skills/certifications query service. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_catalog_db.inc');

/** @return bool */
function hrm_tal_001_read_only_view_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_read_only_view_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'read_only_view_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS')) return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return array|false */
function hrm_tal_001_read_skill_evidence_for_employee($employee_id, &$error = null)
{
    $error=null;if(!hrm_tal_001_read_only_view_ready($error))return false;
    $employee_id=trim((string)$employee_id);if($employee_id===''||strlen($employee_id)>20)return hrm_tal_001_catalog_fail($error,'employee_scope_invalid');
    $company=hrm_tal_001_runtime_company_id();
    $sql='SELECT e.employee_skill_evidence_id,e.employee_id,e.evidence_type_code,e.proficiency_value,e.evidence_date,e.valid_from,e.valid_to,e.evidence_sha256,'
        .'d.skill_definition_id,d.skill_code,d.skill_name,d.category_code,d.proficiency_scale_code,d.definition_sha256 '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_skill_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_skill_definitions d '
        .'ON d.skill_definition_id=e.skill_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id='.db_escape($employee_id).' ORDER BY e.evidence_date DESC,e.employee_skill_evidence_id DESC';
    $result=db_query($sql);if(!$result)return hrm_tal_001_catalog_fail($error,'skill_evidence_query_failed');$rows=array();while($row=db_fetch_assoc($result))$rows[]=$row;return $rows;
}

/** @return array|false */
function hrm_tal_001_read_certification_evidence_for_employee($employee_id, &$error = null)
{
    $error=null;if(!hrm_tal_001_read_only_view_ready($error))return false;
    $employee_id=trim((string)$employee_id);if($employee_id===''||strlen($employee_id)>20)return hrm_tal_001_catalog_fail($error,'employee_scope_invalid');
    $company=hrm_tal_001_runtime_company_id();
    $sql='SELECT e.employee_certification_evidence_id,e.employee_id,e.issued_on,e.expires_on,e.verification_status_code,e.evidence_sha256,'
        .'d.certification_definition_id,d.certification_code,d.certification_name,d.issuer_name,d.category_code,d.definition_sha256 '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_certification_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_certification_definitions d '
        .'ON d.certification_definition_id=e.certification_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id='.db_escape($employee_id).' ORDER BY e.issued_on DESC,e.employee_certification_evidence_id DESC';
    $result=db_query($sql);if(!$result)return hrm_tal_001_catalog_fail($error,'certification_evidence_query_failed');$rows=array();while($row=db_fetch_assoc($result))$rows[]=$row;return $rows;
}
?>
