<?php
/** HRM-TAL-001 bounded admin-only exact-employee performance CSV export service. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_export_db.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_performance_db.inc');

if (!defined('HRM_TAL_001_PERFORMANCE_EXPORT_MAX_ROWS')) define('HRM_TAL_001_PERFORMANCE_EXPORT_MAX_ROWS', 1000);
if (!defined('HRM_TAL_001_PERFORMANCE_EXPORT_MAX_BYTES')) define('HRM_TAL_001_PERFORMANCE_EXPORT_MAX_BYTES', 2097152);

/** @return bool */
function hrm_tal_001_performance_export_service_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_performance_export_service_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'performance_export_service_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS'))
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return array|false */
function hrm_tal_001_export_exact_employee_performance_csv($employee_id, &$error = null)
{
    $error = null;
    if (!hrm_tal_001_performance_export_service_ready($error)) return false;
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || strlen($employee_id) > 20)
        return hrm_tal_001_catalog_fail($error, 'employee_scope_invalid');

    $company = hrm_tal_001_runtime_company_id();
    $actor = hrm_tal_001_runtime_actor_id();
    $limit = HRM_TAL_001_PERFORMANCE_EXPORT_MAX_ROWS + 1;
    $assessments = db_query('SELECT s.employee_id,c.cycle_code,c.cycle_name,c.purpose_code,s.review_type_code,s.review_window_start,s.review_window_end,'
        .'a.assessment_role_code,a.dimension_code,a.rating_value,a.evidence_date '
        .'FROM '.TB_PREF.'hrm_tal_001_performance_assessment_evidence a '
        .'INNER JOIN '.TB_PREF.'hrm_tal_001_performance_review_subjects s ON s.performance_review_subject_id=a.performance_review_subject_id AND s.company_id=a.company_id '
        .'INNER JOIN '.TB_PREF.'hrm_tal_001_performance_cycle_definitions c ON c.performance_cycle_definition_id=s.performance_cycle_definition_id AND c.company_id=s.company_id '
        .'WHERE s.company_id='.(int)$company.' AND s.employee_id='.db_escape($employee_id)
        .' ORDER BY s.review_window_start DESC,a.evidence_date DESC,a.performance_assessment_evidence_id DESC LIMIT '.$limit);
    if (!$assessments) return hrm_tal_001_catalog_fail($error, 'performance_export_assessment_query_failed');
    $workflow = db_query('SELECT s.employee_id,c.cycle_code,c.cycle_name,c.purpose_code,s.review_type_code,s.review_window_start,s.review_window_end,'
        .'w.from_state_code,w.to_state_code,w.aggregate_rating_value,w.occurred_at '
        .'FROM '.TB_PREF.'hrm_tal_001_performance_workflow_events w '
        .'INNER JOIN '.TB_PREF.'hrm_tal_001_performance_review_subjects s ON s.performance_review_subject_id=w.performance_review_subject_id AND s.company_id=w.company_id '
        .'INNER JOIN '.TB_PREF.'hrm_tal_001_performance_cycle_definitions c ON c.performance_cycle_definition_id=s.performance_cycle_definition_id AND c.company_id=s.company_id '
        .'WHERE s.company_id='.(int)$company.' AND s.employee_id='.db_escape($employee_id)
        .' ORDER BY s.review_window_start DESC,w.performance_workflow_event_id DESC LIMIT '.$limit);
    if (!$workflow) return hrm_tal_001_catalog_fail($error, 'performance_export_workflow_query_failed');

    $stream = fopen('php://temp/maxmemory:2097152', 'w+');
    if (!$stream) return hrm_tal_001_catalog_fail($error, 'performance_export_stream_unavailable');
    $header = array('record_type','employee_id','cycle_code','cycle_name','purpose','review_type','review_window_start','review_window_end','assessment_role','dimension','rating','evidence_date','from_state','to_state','aggregate_rating','occurred_at');
    if (!hrm_tal_001_export_write_row($stream, $header)) { fclose($stream); return hrm_tal_001_catalog_fail($error, 'performance_export_write_failed'); }

    $rows = 0;
    while ($row = db_fetch_assoc($assessments)) {
        if (++$rows > HRM_TAL_001_PERFORMANCE_EXPORT_MAX_ROWS) { fclose($stream); return hrm_tal_001_catalog_fail($error, 'performance_export_row_limit_exceeded'); }
        if (!hrm_tal_001_export_write_row($stream, array('assessment',$row['employee_id'],$row['cycle_code'],$row['cycle_name'],$row['purpose_code'],$row['review_type_code'],$row['review_window_start'],$row['review_window_end'],$row['assessment_role_code'],$row['dimension_code'],$row['rating_value'],$row['evidence_date'],'','','',''))) {
            fclose($stream); return hrm_tal_001_catalog_fail($error, 'performance_export_write_failed');
        }
    }
    while ($row = db_fetch_assoc($workflow)) {
        if (++$rows > HRM_TAL_001_PERFORMANCE_EXPORT_MAX_ROWS) { fclose($stream); return hrm_tal_001_catalog_fail($error, 'performance_export_row_limit_exceeded'); }
        if (!hrm_tal_001_export_write_row($stream, array('workflow',$row['employee_id'],$row['cycle_code'],$row['cycle_name'],$row['purpose_code'],$row['review_type_code'],$row['review_window_start'],$row['review_window_end'],'','','','',$row['from_state_code'],$row['to_state_code'],$row['aggregate_rating_value'],$row['occurred_at']))) {
            fclose($stream); return hrm_tal_001_catalog_fail($error, 'performance_export_write_failed');
        }
    }
    rewind($stream);
    $content = stream_get_contents($stream, HRM_TAL_001_PERFORMANCE_EXPORT_MAX_BYTES + 1);
    fclose($stream);
    if (!is_string($content) || strlen($content) > HRM_TAL_001_PERFORMANCE_EXPORT_MAX_BYTES)
        return hrm_tal_001_catalog_fail($error, 'performance_export_byte_limit_exceeded');

    $export_sha = hash('sha256', $content);
    $subject_sha = hash('sha256', 'company:'.$company.'|performance-employee:'.$employee_id);
    $sql = 'INSERT INTO '.TB_PREF.'hrm_tal_001_export_audit (company_id,actor_user_id,scope_code,subject_reference_sha256,format_code,row_count,export_sha256,created_at) VALUES ('
        .(int)$company.','.(int)$actor.','.db_escape('admin_performance_exact_employee').','.db_escape($subject_sha).','.db_escape('csv').','.(int)$rows.','.db_escape($export_sha).','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if (!db_query($sql)) return hrm_tal_001_catalog_fail($error, 'performance_export_audit_insert_failed');

    return array(
        'filename'=>'performance-employee-'.substr($subject_sha, 0, 16).'-'.gmdate('Ymd-His').'.csv',
        'content'=>$content,
        'row_count'=>$rows,
        'export_sha256'=>$export_sha,
        'scope_code'=>'admin_performance_exact_employee'
    );
}
?>
