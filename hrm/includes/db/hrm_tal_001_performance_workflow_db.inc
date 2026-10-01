<?php
/**
 * HRM-TAL-001 bounded performance workflow and deterministic finalization.
 *
 * Lifecycle is append-only. No legacy appraisal, compensation, payroll,
 * accounting or employment-decision record is updated by this service.
 */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_performance_db.inc');

/** @return bool */
function hrm_tal_001_performance_workflow_ready($minimum_version, &$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || version_compare($version, (string)$minimum_version, '<'))
        return hrm_tal_001_catalog_fail($error, 'performance_workflow_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS'))
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return array|false */
function hrm_tal_001_performance_subject_locked($subject_id, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_tal_001_performance_review_subjects WHERE performance_review_subject_id='.(int)$subject_id.' AND company_id='.(int)$company.' LIMIT 2 FOR UPDATE');
    if (!$result || db_num_rows($result) !== 1)
        return hrm_tal_001_catalog_fail($error, 'performance_subject_missing_or_cross_company');
    return db_fetch_assoc($result);
}

/** @return array */
function hrm_tal_001_performance_allowed_workflow_transitions()
{
    return array('open'=>array('submitted'), 'submitted'=>array('reviewed'), 'reviewed'=>array('finalized'), 'finalized'=>array());
}

/** @return string|false */
function hrm_tal_001_performance_current_state_locked($subject_id, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $result = db_query('SELECT to_state_code FROM '.TB_PREF.'hrm_tal_001_performance_workflow_events WHERE company_id='.(int)$company.' AND performance_review_subject_id='.(int)$subject_id.' ORDER BY performance_workflow_event_id DESC LIMIT 1 FOR UPDATE');
    if (!$result || db_num_rows($result) > 1)
        return hrm_tal_001_catalog_fail($error, 'performance_workflow_state_lookup_failed');
    if (db_num_rows($result) === 0) return 'open';
    $row = db_fetch_assoc($result);
    $state = (string)$row['to_state_code'];
    return isset(hrm_tal_001_performance_allowed_workflow_transitions()[$state]) ? $state : hrm_tal_001_catalog_fail($error, 'performance_workflow_state_invalid');
}

/** @return int|false */
function hrm_tal_001_performance_assessment_count($subject_id, $role, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $where = $role === null ? '' : ' AND assessment_role_code='.db_escape($role);
    $result = db_query('SELECT COUNT(*) n FROM '.TB_PREF.'hrm_tal_001_performance_assessment_evidence WHERE company_id='.(int)$company.' AND performance_review_subject_id='.(int)$subject_id.$where);
    if (!$result || db_num_rows($result) !== 1) return hrm_tal_001_catalog_fail($error, 'performance_assessment_count_failed');
    $row = db_fetch_assoc($result); return (int)$row['n'];
}

/** @return array|false */
function hrm_tal_001_append_performance_workflow_event($subject_id, $from, $to, $reason_sha, $aggregate, $basis_sha, &$error)
{
    $company = hrm_tal_001_runtime_company_id(); $actor = hrm_tal_001_runtime_actor_id();
    $occurred = gmdate('Y-m-d H:i:s');
    $payload = array('company_id'=>$company,'performance_review_subject_id'=>(int)$subject_id,'from_state_code'=>$from,'to_state_code'=>$to,
        'aggregate_rating_value'=>$aggregate,'rating_basis_sha256'=>$basis_sha,'reason_reference_sha256'=>$reason_sha,'actor_user_id'=>$actor,'occurred_at'=>$occurred);
    $sha = hrm_tal_001_payload_sha256($payload);
    $existing = db_query('SELECT performance_workflow_event_id FROM '.TB_PREF.'hrm_tal_001_performance_workflow_events WHERE transition_sha256='.db_escape($sha).' LIMIT 2');
    if (!$existing || db_num_rows($existing) > 1) return hrm_tal_001_catalog_fail($error, 'performance_workflow_replay_lookup_failed');
    if (db_num_rows($existing) === 1) { $row=db_fetch_assoc($existing); return array('performance_workflow_event_id'=>(int)$row['performance_workflow_event_id'],'transition_sha256'=>$sha,'state'=>$to,'idempotent'=>true); }
    $sql='INSERT INTO '.TB_PREF.'hrm_tal_001_performance_workflow_events (company_id,performance_review_subject_id,from_state_code,to_state_code,aggregate_rating_value,rating_basis_sha256,reason_reference_sha256,transition_sha256,actor_user_id,occurred_at,created_at) VALUES ('
        .$company.','.(int)$subject_id.','.db_escape($from).','.db_escape($to).','.($aggregate===null?'NULL':db_escape($aggregate)).','.($basis_sha===null?'NULL':db_escape($basis_sha)).','.db_escape($reason_sha).','.db_escape($sha).','.$actor.','.db_escape($occurred).','.db_escape($occurred).')';
    if(!db_query($sql)) return hrm_tal_001_catalog_fail($error,'performance_workflow_insert_failed');
    return array('performance_workflow_event_id'=>(int)db_insert_id(),'transition_sha256'=>$sha,'state'=>$to,'idempotent'=>false);
}

/**
 * Append one bounded workflow transition. Generic finalization is intentionally
 * excluded here; finalization must use the deterministic scoring function.
 * @return array|false
 */
function hrm_tal_001_record_performance_workflow_transition($subject_id, $target_state, $reason_reference_sha256, &$error = null)
{
    $error=null;
    $version=hrm_tal_001_runtime_database_version();
    if(!is_string($version)||!hrm_tal_001_performance_workflow_writer_allowed_for_database_version($version)||!hrm_tal_001_performance_workflow_ready('1.0.964',$error)) return false;
    $subject_id=(int)$subject_id; $target=hrm_tal_001_token($target_state,24); $reason=hrm_tal_001_sha256($reason_reference_sha256);
    if($subject_id<=0||$target===false||!in_array($target,array('submitted','reviewed'),true)||$reason===false)
        return hrm_tal_001_catalog_fail($error,'performance_workflow_transition_invalid');
    if(!hrm_tal_001_catalog_begin_serialized_transaction($error)) return false;
    $subject=hrm_tal_001_performance_subject_locked($subject_id,$error); if($subject===false){cancel_transaction();return false;}
    $from=hrm_tal_001_performance_current_state_locked($subject_id,$error); if($from===false){cancel_transaction();return false;}
    $allowed=hrm_tal_001_performance_allowed_workflow_transitions();
    if(!isset($allowed[$from])||!in_array($target,$allowed[$from],true)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_workflow_transition_not_allowed');}
    $count=hrm_tal_001_performance_assessment_count($subject_id,$target==='reviewed'?'reviewer':null,$error);
    if($count===false||$count<1){cancel_transaction();return hrm_tal_001_catalog_fail($error,$target==='reviewed'?'performance_reviewer_evidence_required':'performance_assessment_evidence_required');}
    $result=hrm_tal_001_append_performance_workflow_event($subject_id,$from,$target,$reason,null,null,$error);
    if($result===false){cancel_transaction();return false;} commit_transaction(); return $result;
}

/** @return int|false */
function hrm_tal_001_rating_to_units($value)
{
    $value=trim((string)$value);
    if(!preg_match('/^(0|[1-9]\d{0,5})(?:\.(\d{1,4}))?$/D',$value,$m)) return false;
    $frac=isset($m[2])?str_pad($m[2],4,'0',STR_PAD_RIGHT):'0000';
    return ((int)$m[1])*10000+(int)$frac;
}

/** @return string */
function hrm_tal_001_units_to_rating($units)
{
    $units=(int)$units; $whole=intdiv($units,10000); $frac=str_pad((string)($units%10000),4,'0',STR_PAD_LEFT);
    return $whole.'.'.$frac;
}

/**
 * Finalize a reviewed subject using a deterministic mean of manager/reviewer
 * numeric assessment evidence. The score is descriptive evidence only and
 * grants no compensation, payroll or employment-decision authority.
 * @return array|false
 */
function hrm_tal_001_finalize_performance_review($subject_id, $reason_reference_sha256, &$error = null)
{
    $error=null;$version=hrm_tal_001_runtime_database_version();
    if(!is_string($version)||!hrm_tal_001_performance_score_finalization_allowed_for_database_version($version)||!hrm_tal_001_performance_workflow_ready('1.0.965',$error)) return false;
    $subject_id=(int)$subject_id;$reason=hrm_tal_001_sha256($reason_reference_sha256);
    if($subject_id<=0||$reason===false)return hrm_tal_001_catalog_fail($error,'performance_finalization_invalid');
    if(!hrm_tal_001_catalog_begin_serialized_transaction($error))return false;
    $subject=hrm_tal_001_performance_subject_locked($subject_id,$error);if($subject===false){cancel_transaction();return false;}
    $from=hrm_tal_001_performance_current_state_locked($subject_id,$error);if($from===false){cancel_transaction();return false;}
    if($from!=='reviewed'){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_finalization_requires_reviewed_state');}
    $cycle=hrm_tal_001_performance_cycle_by_id((int)$subject['performance_cycle_definition_id'],$error);if($cycle===false){cancel_transaction();return false;}
    if((string)$cycle['rating_scale_code']==='none'){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_finalization_requires_numeric_scale');}
    $company=hrm_tal_001_runtime_company_id();
    $q=db_query('SELECT performance_assessment_evidence_id,assessment_role_code,dimension_code,rating_value,evidence_sha256 FROM '.TB_PREF.'hrm_tal_001_performance_assessment_evidence WHERE company_id='.(int)$company.' AND performance_review_subject_id='.$subject_id." AND assessment_role_code IN ('manager','reviewer') AND rating_value IS NOT NULL ORDER BY performance_assessment_evidence_id ASC FOR UPDATE");
    if(!$q){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_scoring_query_failed');}
    $sum=0;$count=0;$basis=array();$reviewer=false;
    while($row=db_fetch_assoc($q)){$u=hrm_tal_001_rating_to_units($row['rating_value']);if($u===false){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_rating_basis_invalid');}$sum+=$u;++$count;$reviewer=$reviewer||$row['assessment_role_code']==='reviewer';$basis[]=array('id'=>(int)$row['performance_assessment_evidence_id'],'role'=>(string)$row['assessment_role_code'],'dimension'=>(string)$row['dimension_code'],'rating'=>hrm_tal_001_units_to_rating($u),'evidence_sha256'=>(string)$row['evidence_sha256']);}
    if($count<1||!$reviewer){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_reviewer_numeric_evidence_required');}
    $avg=intdiv($sum,$count);$remainder=$sum%$count;if($remainder*2>=$count)++$avg;
    $aggregate=hrm_tal_001_units_to_rating($avg);$basis_sha=hrm_tal_001_payload_sha256(array('subject_id'=>$subject_id,'cycle_definition_sha256'=>(string)$cycle['definition_sha256'],'rating_scale_code'=>(string)$cycle['rating_scale_code'],'evidence'=>$basis));
    $result=hrm_tal_001_append_performance_workflow_event($subject_id,'reviewed','finalized',$reason,$aggregate,$basis_sha,$error);
    if($result===false){cancel_transaction();return false;} $result['aggregate_rating_value']=$aggregate;$result['rating_basis_sha256']=$basis_sha;commit_transaction();return $result;
}
?>
