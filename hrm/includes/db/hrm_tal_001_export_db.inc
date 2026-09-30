<?php
/** HRM-TAL-001 bounded admin-only exact-employee CSV export service. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_catalog_db.inc');

if (!defined('HRM_TAL_001_EXPORT_MAX_ROWS')) define('HRM_TAL_001_EXPORT_MAX_ROWS', 1000);
if (!defined('HRM_TAL_001_EXPORT_MAX_BYTES')) define('HRM_TAL_001_EXPORT_MAX_BYTES', 2097152);

/** @return bool */
function hrm_tal_001_export_service_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_export_service_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'export_service_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS'))
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return string */
function hrm_tal_001_export_csv_cell($value)
{
    $value = (string)$value;
    if (strlen($value) > 512) $value = substr($value, 0, 512);
    if ($value !== '' && preg_match('/^[\x00-\x20]*[=+\-@]/', $value)) $value = "'".$value;
    return $value;
}

/** @return bool */
function hrm_tal_001_export_write_row($stream, array $row)
{
    foreach ($row as $i=>$value) $row[$i] = hrm_tal_001_export_csv_cell($value);
    return fputcsv($stream, $row) !== false;
}

/** @return array|false */
function hrm_tal_001_export_exact_employee_csv($employee_id, &$error = null)
{
    $error = null;
    if (!hrm_tal_001_export_service_ready($error)) return false;
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || strlen($employee_id) > 20)
        return hrm_tal_001_catalog_fail($error, 'employee_scope_invalid');

    $company = hrm_tal_001_runtime_company_id();
    $actor = hrm_tal_001_runtime_actor_id();
    $limit = HRM_TAL_001_EXPORT_MAX_ROWS + 1;
    $skills = db_query('SELECT e.employee_id,d.skill_code,d.skill_name,d.category_code,e.evidence_type_code,e.proficiency_value,e.evidence_date,e.valid_from,e.valid_to '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_skill_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_skill_definitions d '
        .'ON d.skill_definition_id=e.skill_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id='.db_escape($employee_id)
        .' ORDER BY e.evidence_date DESC,e.employee_skill_evidence_id DESC LIMIT '.$limit);
    if (!$skills) return hrm_tal_001_catalog_fail($error, 'export_skill_query_failed');
    $certs = db_query('SELECT e.employee_id,d.certification_code,d.certification_name,d.issuer_name,d.category_code,e.issued_on,e.expires_on,e.verification_status_code '
        .'FROM '.TB_PREF.'hrm_tal_001_employee_certification_evidence e INNER JOIN '.TB_PREF.'hrm_tal_001_certification_definitions d '
        .'ON d.certification_definition_id=e.certification_definition_id AND d.company_id=e.company_id '
        .'WHERE e.company_id='.(int)$company.' AND e.employee_id='.db_escape($employee_id)
        .' ORDER BY e.issued_on DESC,e.employee_certification_evidence_id DESC LIMIT '.$limit);
    if (!$certs) return hrm_tal_001_catalog_fail($error, 'export_certification_query_failed');

    $stream = fopen('php://temp/maxmemory:2097152', 'w+');
    if (!$stream) return hrm_tal_001_catalog_fail($error, 'export_stream_unavailable');
    $header = array('record_type','employee_id','code','name','issuer','category','evidence_type','proficiency','evidence_date','valid_from','valid_to','issued_on','expires_on','verification_status');
    if (!hrm_tal_001_export_write_row($stream, $header)) { fclose($stream); return hrm_tal_001_catalog_fail($error, 'export_write_failed'); }

    $rows = 0;
    while ($row = db_fetch_assoc($skills)) {
        if (++$rows > HRM_TAL_001_EXPORT_MAX_ROWS) { fclose($stream); return hrm_tal_001_catalog_fail($error, 'export_row_limit_exceeded'); }
        if (!hrm_tal_001_export_write_row($stream, array('skill',$row['employee_id'],$row['skill_code'],$row['skill_name'],'',$row['category_code'],$row['evidence_type_code'],$row['proficiency_value'],$row['evidence_date'],$row['valid_from'],$row['valid_to'],'','',''))) {
            fclose($stream); return hrm_tal_001_catalog_fail($error, 'export_write_failed');
        }
    }
    while ($row = db_fetch_assoc($certs)) {
        if (++$rows > HRM_TAL_001_EXPORT_MAX_ROWS) { fclose($stream); return hrm_tal_001_catalog_fail($error, 'export_row_limit_exceeded'); }
        if (!hrm_tal_001_export_write_row($stream, array('certification',$row['employee_id'],$row['certification_code'],$row['certification_name'],$row['issuer_name'],$row['category_code'],'','','','','',$row['issued_on'],$row['expires_on'],$row['verification_status_code']))) {
            fclose($stream); return hrm_tal_001_catalog_fail($error, 'export_write_failed');
        }
    }
    rewind($stream);
    $content = stream_get_contents($stream, HRM_TAL_001_EXPORT_MAX_BYTES + 1);
    fclose($stream);
    if (!is_string($content) || strlen($content) > HRM_TAL_001_EXPORT_MAX_BYTES)
        return hrm_tal_001_catalog_fail($error, 'export_byte_limit_exceeded');

    $export_sha = hash('sha256', $content);
    $subject_sha = hash('sha256', 'company:'.$company.'|employee:'.$employee_id);
    $sql = 'INSERT INTO '.TB_PREF.'hrm_tal_001_export_audit (company_id,actor_user_id,scope_code,subject_reference_sha256,format_code,row_count,export_sha256,created_at) VALUES ('
        .(int)$company.','.(int)$actor.','.db_escape('admin_exact_employee').','.db_escape($subject_sha).','.db_escape('csv').','.(int)$rows.','.db_escape($export_sha).','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if (!db_query($sql)) return hrm_tal_001_catalog_fail($error, 'export_audit_insert_failed');

    return array(
        'filename'=>'talent-employee-'.substr($subject_sha, 0, 16).'-'.gmdate('Ymd-His').'.csv',
        'content'=>$content,
        'row_count'=>$rows,
        'export_sha256'=>$export_sha,
        'scope_code'=>'admin_exact_employee'
    );
}
?>
