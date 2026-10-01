<?php
/**
 * HRM-TAL-001 bounded performance writer and exact-employee read services.
 *
 * These services append only to the immutable performance custody tables.
 * They do not update/delete performance evidence, mutate legacy appraisal,
 * payroll or compensation data, or make employment decisions.
 */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_catalog_db.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_evidence_db.inc');

/** @return bool */
function hrm_tal_001_performance_admin_ready($minimum_version, &$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || version_compare($version, (string)$minimum_version, '<'))
        return hrm_tal_001_catalog_fail($error, 'performance_service_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS'))
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return bool */
function hrm_tal_001_performance_cycle_writer_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_performance_cycle_writer_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'performance_cycle_writer_not_active');
    return hrm_tal_001_performance_admin_ready('1.0.957', $error);
}

/** @return bool */
function hrm_tal_001_performance_evidence_writer_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_performance_evidence_writer_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'performance_evidence_writer_not_active');
    return hrm_tal_001_performance_admin_ready('1.0.958', $error);
}

/** @return bool */
function hrm_tal_001_performance_read_only_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_performance_read_only_view_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'performance_read_only_not_active');
    return hrm_tal_001_performance_admin_ready('1.0.959', $error);
}

/** @return array|false */
function hrm_tal_001_performance_cycle_by_id($id, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_tal_001_performance_cycle_definitions WHERE performance_cycle_definition_id='.(int)$id.' AND company_id='.(int)$company.' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1)
        return hrm_tal_001_catalog_fail($error, 'performance_cycle_missing_or_cross_company');
    return db_fetch_assoc($result);
}

/** @return array|false */
function hrm_tal_001_performance_subject_by_id($id, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_tal_001_performance_review_subjects WHERE performance_review_subject_id='.(int)$id.' AND company_id='.(int)$company.' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1)
        return hrm_tal_001_catalog_fail($error, 'performance_subject_missing_or_cross_company');
    return db_fetch_assoc($result);
}

/** @return bool */
function hrm_tal_001_performance_cycle_window_available($code, $from, $to, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $end = $to === null ? '9999-12-31' : $to;
    $result = db_query('SELECT COUNT(*) n FROM '.TB_PREF.'hrm_tal_001_performance_cycle_definitions WHERE company_id='.(int)$company
        .' AND cycle_code='.db_escape($code).' AND effective_from<='.db_escape($end)
        .' AND COALESCE(effective_to,\'9999-12-31\')>='.db_escape($from));
    if (!$result || db_num_rows($result) !== 1) return hrm_tal_001_catalog_fail($error, 'performance_cycle_overlap_check_failed');
    $row = db_fetch_assoc($result);
    if ((int)$row['n'] !== 0) return hrm_tal_001_catalog_fail($error, 'performance_cycle_effective_window_overlaps');
    return true;
}

/**
 * Append one effective-dated performance cycle definition.
 * @param array $x
 * @param string|null $error
 * @return array|false
 */
function hrm_tal_001_register_performance_cycle_definition(array $x, &$error = null)
{
    $error = null;
    if (!hrm_tal_001_performance_cycle_writer_ready($error)) return false;
    foreach (array('cycle_code','cycle_name','purpose_code','period_start','period_end','rating_scale_code','effective_from') as $key)
        if (!array_key_exists($key, $x)) return hrm_tal_001_catalog_fail($error, 'performance_cycle_field_missing');
    $code = hrm_tal_001_token($x['cycle_code'], 64);
    $name = trim((string)$x['cycle_name']);
    $purpose = hrm_tal_001_token($x['purpose_code'], 32);
    $period_start = hrm_tal_001_date($x['period_start']);
    $period_end = hrm_tal_001_date($x['period_end']);
    $scale = hrm_tal_001_token($x['rating_scale_code'], 32);
    $from = hrm_tal_001_date($x['effective_from']);
    $to = hrm_tal_001_optional_date(isset($x['effective_to']) ? $x['effective_to'] : null);
    if ($code === false || $name === '' || strlen($name) > 140 || $purpose === false
        || !in_array($purpose, array('development','performance_review','goal_review'), true)
        || $period_start === false || $period_end === false || $period_end < $period_start
        || $scale === false || !in_array($scale, array('none','level_1_5','percent_0_100'), true)
        || $from === false || ($to !== null && ($to === false || $to < $from)))
        return hrm_tal_001_catalog_fail($error, 'performance_cycle_invalid');
    $company = hrm_tal_001_runtime_company_id(); $actor = hrm_tal_001_runtime_actor_id();
    $payload = array('company_id'=>$company,'cycle_code'=>$code,'cycle_name'=>$name,'purpose_code'=>$purpose,
        'period_start'=>$period_start,'period_end'=>$period_end,'rating_scale_code'=>$scale,'effective_from'=>$from,'effective_to'=>$to);
    $sha = hrm_tal_001_payload_sha256($payload);
    if (!hrm_tal_001_catalog_begin_serialized_transaction($error)) return false;
    $existing = db_query('SELECT performance_cycle_definition_id,definition_sha256 FROM '.TB_PREF.'hrm_tal_001_performance_cycle_definitions WHERE company_id='.(int)$company
        .' AND cycle_code='.db_escape($code).' AND effective_from='.db_escape($from).' LIMIT 2');
    if (!$existing || db_num_rows($existing) > 1) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'performance_cycle_lookup_failed'); }
    if (db_num_rows($existing) === 1) {
        $row = db_fetch_assoc($existing);
        if (!hash_equals((string)$row['definition_sha256'], $sha)) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'performance_cycle_replay_mismatch'); }
        commit_transaction();
        return array('performance_cycle_definition_id'=>(int)$row['performance_cycle_definition_id'],'definition_sha256'=>$sha,'idempotent'=>true);
    }
    if (!hrm_tal_001_performance_cycle_window_available($code, $from, $to, $error)) { cancel_transaction(); return false; }
    $sql='INSERT INTO '.TB_PREF.'hrm_tal_001_performance_cycle_definitions (company_id,cycle_code,cycle_name,purpose_code,period_start,period_end,rating_scale_code,effective_from,effective_to,definition_sha256,created_by_user_id,created_at) VALUES ('
        .$company.','.db_escape($code).','.db_escape($name).','.db_escape($purpose).','.db_escape($period_start).','.db_escape($period_end).','.db_escape($scale).','.db_escape($from).','
        .($to===null?'NULL':db_escape($to)).','.db_escape($sha).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if (!db_query($sql)) { cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'performance_cycle_insert_failed'); }
    $id=(int)db_insert_id(); commit_transaction();
    return array('performance_cycle_definition_id'=>$id,'definition_sha256'=>$sha,'idempotent'=>false);
}

/**
 * Append one exact employee review subject bound to an accepted cycle.
 * @param array $x
 * @param string|null $error
 * @return array|false
 */
function hrm_tal_001_register_performance_review_subject(array $x, &$error = null)
{
    $error=null;if(!hrm_tal_001_performance_evidence_writer_ready($error))return false;
    foreach(array('performance_cycle_definition_id','employee_id','review_type_code','review_window_start','review_window_end','subject_reference_sha256') as $key)
        if(!array_key_exists($key,$x))return hrm_tal_001_catalog_fail($error,'performance_subject_field_missing');
    $cycle_id=(int)$x['performance_cycle_definition_id'];$employee=trim((string)$x['employee_id']);$type=hrm_tal_001_token($x['review_type_code'],32);
    $start=hrm_tal_001_date($x['review_window_start']);$end=hrm_tal_001_date($x['review_window_end']);$reference=hrm_tal_001_sha256($x['subject_reference_sha256']);
    if($cycle_id<=0||$employee===''||strlen($employee)>20||$type===false||!in_array($type,array('annual','midyear','probation','project'),true)
        ||$start===false||$end===false||$end<$start||$reference===false||!hrm_tal_001_employee_exists($employee))
        return hrm_tal_001_catalog_fail($error,'performance_subject_invalid');
    $cycle=hrm_tal_001_performance_cycle_by_id($cycle_id,$error);if($cycle===false)return false;
    if($start<(string)$cycle['period_start']||$end>(string)$cycle['period_end'])return hrm_tal_001_catalog_fail($error,'performance_subject_outside_cycle_period');
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();
    $payload=array('company_id'=>$company,'performance_cycle_definition_id'=>$cycle_id,'definition_sha256'=>(string)$cycle['definition_sha256'],
        'employee_id'=>$employee,'review_type_code'=>$type,'review_window_start'=>$start,'review_window_end'=>$end,'subject_reference_sha256'=>$reference);
    $sha=hrm_tal_001_payload_sha256($payload);
    if(!hrm_tal_001_catalog_begin_serialized_transaction($error))return false;
    $existing=db_query('SELECT performance_review_subject_id,subject_sha256 FROM '.TB_PREF.'hrm_tal_001_performance_review_subjects WHERE company_id='.(int)$company
        .' AND performance_cycle_definition_id='.$cycle_id.' AND employee_id='.db_escape($employee).' AND review_type_code='.db_escape($type).' LIMIT 2');
    if(!$existing||db_num_rows($existing)>1){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_subject_lookup_failed');}
    if(db_num_rows($existing)===1){$row=db_fetch_assoc($existing);if(!hash_equals((string)$row['subject_sha256'],$sha)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_subject_replay_mismatch');}commit_transaction();return array('performance_review_subject_id'=>(int)$row['performance_review_subject_id'],'subject_sha256'=>$sha,'idempotent'=>true);}
    $sql='INSERT INTO '.TB_PREF.'hrm_tal_001_performance_review_subjects (company_id,performance_cycle_definition_id,employee_id,review_type_code,review_window_start,review_window_end,subject_reference_sha256,subject_sha256,created_by_user_id,created_at) VALUES ('
        .$company.','.$cycle_id.','.db_escape($employee).','.db_escape($type).','.db_escape($start).','.db_escape($end).','.db_escape($reference).','.db_escape($sha).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if(!db_query($sql)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_subject_insert_failed');}
    $id=(int)db_insert_id();commit_transaction();
    return array('performance_review_subject_id'=>$id,'subject_sha256'=>$sha,'idempotent'=>false);
}

/**
 * Append minimized assessment evidence. Raw comment text is never accepted.
 * @param array $x
 * @param string|null $error
 * @return array|false
 */
function hrm_tal_001_record_performance_assessment_evidence(array $x, &$error = null)
{
    $error=null;if(!hrm_tal_001_performance_evidence_writer_ready($error))return false;
    foreach(array('performance_review_subject_id','assessment_role_code','dimension_code','evidence_date') as $key)
        if(!array_key_exists($key,$x))return hrm_tal_001_catalog_fail($error,'performance_assessment_field_missing');
    if(array_key_exists('comment_text',$x)||array_key_exists('raw_comment',$x))return hrm_tal_001_catalog_fail($error,'performance_raw_comment_denied');
    $subject_id=(int)$x['performance_review_subject_id'];$role=hrm_tal_001_token($x['assessment_role_code'],32);$dimension=hrm_tal_001_token($x['dimension_code'],64);
    $rating=hrm_tal_001_decimal_or_null(isset($x['rating_value'])?$x['rating_value']:null);$comment=isset($x['comment_reference_sha256'])&&$x['comment_reference_sha256']!==''?hrm_tal_001_sha256($x['comment_reference_sha256']):null;$date=hrm_tal_001_date($x['evidence_date']);
    if($subject_id<=0||$role===false||!in_array($role,array('self','manager','reviewer'),true)||$dimension===false||$rating===false||$comment===false||$date===false)
        return hrm_tal_001_catalog_fail($error,'performance_assessment_invalid');
    $subject=hrm_tal_001_performance_subject_by_id($subject_id,$error);if($subject===false)return false;
    if($date<(string)$subject['review_window_start']||$date>(string)$subject['review_window_end'])return hrm_tal_001_catalog_fail($error,'performance_assessment_outside_review_window');
    $cycle=hrm_tal_001_performance_cycle_by_id((int)$subject['performance_cycle_definition_id'],$error);if($cycle===false)return false;
    $scale=(string)$cycle['rating_scale_code'];
    if(($scale==='none'&&$rating!==null)||($scale==='level_1_5'&&($rating===null||hrm_tal_001_decimal_compare($rating,'1')<0||hrm_tal_001_decimal_compare($rating,'5')>0))
        ||($scale==='percent_0_100'&&($rating===null||hrm_tal_001_decimal_compare($rating,'0')<0||hrm_tal_001_decimal_compare($rating,'100')>0)))
        return hrm_tal_001_catalog_fail($error,'performance_rating_invalid_for_scale');
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();
    $payload=array('company_id'=>$company,'performance_review_subject_id'=>$subject_id,'subject_sha256'=>(string)$subject['subject_sha256'],
        'assessment_role_code'=>$role,'dimension_code'=>$dimension,'rating_value'=>$rating,'comment_reference_sha256'=>$comment,'evidence_date'=>$date);
    $sha=hrm_tal_001_payload_sha256($payload);
    if(!hrm_tal_001_catalog_begin_serialized_transaction($error))return false;
    $existing=db_query('SELECT performance_assessment_evidence_id,evidence_sha256 FROM '.TB_PREF.'hrm_tal_001_performance_assessment_evidence WHERE company_id='.(int)$company.' AND evidence_sha256='.db_escape($sha).' LIMIT 2');
    if(!$existing||db_num_rows($existing)>1){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_assessment_lookup_failed');}
    if(db_num_rows($existing)===1){$row=db_fetch_assoc($existing);commit_transaction();return array('performance_assessment_evidence_id'=>(int)$row['performance_assessment_evidence_id'],'evidence_sha256'=>$sha,'idempotent'=>true);}
    $sql='INSERT INTO '.TB_PREF.'hrm_tal_001_performance_assessment_evidence (company_id,performance_review_subject_id,assessment_role_code,dimension_code,rating_value,comment_reference_sha256,evidence_date,evidence_sha256,recorded_by_user_id,created_at) VALUES ('
        .$company.','.$subject_id.','.db_escape($role).','.db_escape($dimension).','.($rating===null?'NULL':db_escape($rating)).','.($comment===null?'NULL':db_escape($comment)).','.db_escape($date).','.db_escape($sha).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if(!db_query($sql)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'performance_assessment_insert_failed');}
    $id=(int)db_insert_id();commit_transaction();
    return array('performance_assessment_evidence_id'=>$id,'evidence_sha256'=>$sha,'idempotent'=>false);
}

/**
 * Read minimized performance records for one exact employee.
 * Sensitive source/comment reference hashes and actor identifiers are excluded.
 * @return array|false
 */
function hrm_tal_001_read_performance_for_employee($employee_id, &$error = null)
{
    $error=null;if(!hrm_tal_001_performance_read_only_ready($error))return false;
    $employee_id=trim((string)$employee_id);if($employee_id===''||strlen($employee_id)>20)return hrm_tal_001_catalog_fail($error,'employee_scope_invalid');
    $company=hrm_tal_001_runtime_company_id();
    $sql='SELECT s.performance_review_subject_id,s.employee_id,s.review_type_code,s.review_window_start,s.review_window_end,s.subject_sha256,'
        .'c.performance_cycle_definition_id,c.cycle_code,c.cycle_name,c.purpose_code,c.period_start,c.period_end,c.rating_scale_code,c.definition_sha256,'
        .'a.performance_assessment_evidence_id,a.assessment_role_code,a.dimension_code,a.rating_value,a.evidence_date,a.evidence_sha256 '
        .'FROM '.TB_PREF.'hrm_tal_001_performance_review_subjects s INNER JOIN '.TB_PREF.'hrm_tal_001_performance_cycle_definitions c '
        .'ON c.performance_cycle_definition_id=s.performance_cycle_definition_id AND c.company_id=s.company_id '
        .'LEFT JOIN '.TB_PREF.'hrm_tal_001_performance_assessment_evidence a ON a.performance_review_subject_id=s.performance_review_subject_id AND a.company_id=s.company_id '
        .'WHERE s.company_id='.(int)$company.' AND s.employee_id='.db_escape($employee_id)
        .' ORDER BY s.review_window_start DESC,s.performance_review_subject_id DESC,a.evidence_date DESC,a.performance_assessment_evidence_id DESC';
    $result=db_query($sql);if(!$result)return hrm_tal_001_catalog_fail($error,'performance_read_query_failed');$rows=array();while($row=db_fetch_assoc($result))$rows[]=$row;return $rows;
}
?>
