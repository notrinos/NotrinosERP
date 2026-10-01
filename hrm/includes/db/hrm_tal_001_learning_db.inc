<?php
/** HRM-TAL-001 bounded learning runtime over dedicated immutable TAL custody. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_evidence_db.inc');

/** @return bool */
function hrm_tal_001_learning_runtime_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_learning_runtime_service_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'learning_runtime_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS'))
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return bool */
function hrm_tal_001_learning_begin_transaction(&$error)
{
    global $transaction_level;
    if (!empty($transaction_level)) return hrm_tal_001_catalog_fail($error, 'learning_owned_transaction_required');
    begin_transaction();
    $r=db_query("SELECT value FROM ".TB_PREF."sys_prefs WHERE name='version_id' AND category='system' LIMIT 2 FOR UPDATE");
    $row=$r?db_fetch_assoc($r):false;
    if (!$row || !isset($row['value']) || !hrm_tal_001_learning_runtime_service_allowed_for_database_version((string)$row['value'])) {
        cancel_transaction(); return hrm_tal_001_catalog_fail($error, 'learning_runtime_not_active');
    }
    return true;
}

/** @return array|false */
function hrm_tal_001_register_learning_definition(array $x, &$error=null)
{
    $error=null; if(!hrm_tal_001_learning_runtime_ready($error))return false;
    foreach(array('learning_code','learning_name','delivery_mode_code','effective_from') as $k)if(!array_key_exists($k,$x))return hrm_tal_001_catalog_fail($error,'learning_definition_field_missing');
    $code=hrm_tal_001_token($x['learning_code'],64); $name=trim((string)$x['learning_name']); $mode=hrm_tal_001_token($x['delivery_mode_code'],32);
    $provider=isset($x['provider_name'])?trim((string)$x['provider_name']):''; $duration=isset($x['duration_minutes'])?(int)$x['duration_minutes']:0;
    $from=hrm_tal_001_date($x['effective_from']); $to=isset($x['effective_to'])&&trim((string)$x['effective_to'])!==''?hrm_tal_001_date($x['effective_to']):null;
    if($code===false||$mode===false||$name===''||strlen($name)>140||strlen($provider)>140||$duration<0||$duration>525600||$from===false||($to===false)||($to!==null&&strcmp($to,$from)<0))return hrm_tal_001_catalog_fail($error,'learning_definition_invalid');
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();
    $hash=hash('sha256',implode('|',array($company,$code,$name,$mode,$provider,$duration,$from,$to===null?'':$to)));
    if(!hrm_tal_001_learning_begin_transaction($error))return false;
    $same=db_query("SELECT learning_definition_id FROM ".TB_PREF."hrm_tal_001_learning_definitions WHERE company_id=".(int)$company." AND definition_sha256=".db_escape($hash)." LIMIT 2");
    if($same&&db_num_rows($same)===1){$row=db_fetch_assoc($same);commit_transaction();return array('learning_definition_id'=>(int)$row['learning_definition_id'],'replayed'=>true,'definition_sha256'=>$hash);}
    $overlap=db_query("SELECT learning_definition_id FROM ".TB_PREF."hrm_tal_001_learning_definitions WHERE company_id=".(int)$company." AND learning_code=".db_escape($code)." AND effective_from<=".db_escape($to===null?'9999-12-31':$to)." AND (effective_to IS NULL OR effective_to>=".db_escape($from).") LIMIT 1");
    if($overlap&&db_num_rows($overlap)>0){cancel_transaction();return hrm_tal_001_catalog_fail($error,'learning_effective_window_overlaps');}
    $ok=db_query("INSERT INTO ".TB_PREF."hrm_tal_001_learning_definitions (company_id,learning_code,learning_name,delivery_mode_code,provider_name,duration_minutes,effective_from,effective_to,definition_sha256,created_by_user_id,created_at) VALUES (".(int)$company.",".db_escape($code).",".db_escape($name).",".db_escape($mode).",".db_escape($provider).",".(int)$duration.",".db_escape($from).",".($to===null?'NULL':db_escape($to)).",".db_escape($hash).",".(int)$actor.",UTC_TIMESTAMP())");
    if(!$ok){cancel_transaction();return hrm_tal_001_catalog_fail($error,'learning_definition_insert_failed');}
    $id=(int)db_insert_id();commit_transaction();return array('learning_definition_id'=>$id,'replayed'=>false,'definition_sha256'=>$hash);
}

/** @return array|false */
function hrm_tal_001_assign_learning_to_employee(array $x, &$error=null)
{
    $error=null;if(!hrm_tal_001_learning_runtime_ready($error))return false;
    foreach(array('learning_definition_id','employee_id','assigned_on','assignment_status_code','source_reference_sha256') as $k)if(!array_key_exists($k,$x))return hrm_tal_001_catalog_fail($error,'learning_assignment_field_missing');
    $id=(int)$x['learning_definition_id'];$employee=trim((string)$x['employee_id']);$assigned=hrm_tal_001_date($x['assigned_on']);$due=isset($x['due_on'])&&trim((string)$x['due_on'])!==''?hrm_tal_001_date($x['due_on']):null;$status=hrm_tal_001_token($x['assignment_status_code'],24);$ref=strtolower(trim((string)$x['source_reference_sha256']));
    if($id<=0||$employee===''||strlen($employee)>20||$assigned===false||$due===false||($due!==null&&strcmp($due,$assigned)<0)||!in_array($status,array('assigned','in_progress','waived'),true)||!preg_match('/^[0-9a-f]{64}$/D',$ref)||!hrm_tal_001_employee_exists($employee))return hrm_tal_001_catalog_fail($error,'learning_assignment_invalid');
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();
    $def=db_query("SELECT learning_definition_id,effective_from,effective_to FROM ".TB_PREF."hrm_tal_001_learning_definitions WHERE learning_definition_id=".$id." AND company_id=".(int)$company." LIMIT 2");$d=$def?db_fetch_assoc($def):false;
    if(!$d)return hrm_tal_001_catalog_fail($error,'learning_definition_missing_or_cross_company');
    if(strcmp($assigned,(string)$d['effective_from'])<0||(!empty($d['effective_to'])&&strcmp($assigned,(string)$d['effective_to'])>0))return hrm_tal_001_catalog_fail($error,'learning_assignment_outside_definition_window');
    $hash=hash('sha256',implode('|',array($company,$id,$employee,$assigned,$due===null?'':$due,$status,$ref)));
    if(!hrm_tal_001_learning_begin_transaction($error))return false;
    $same=db_query("SELECT learning_assignment_id FROM ".TB_PREF."hrm_tal_001_learning_assignments WHERE company_id=".(int)$company." AND assignment_sha256=".db_escape($hash)." LIMIT 2");
    if($same&&db_num_rows($same)===1){$row=db_fetch_assoc($same);commit_transaction();return array('learning_assignment_id'=>(int)$row['learning_assignment_id'],'replayed'=>true,'assignment_sha256'=>$hash);}
    $ok=db_query("INSERT INTO ".TB_PREF."hrm_tal_001_learning_assignments (company_id,learning_definition_id,employee_id,assigned_on,due_on,assignment_status_code,source_reference_sha256,assignment_sha256,assigned_by_user_id,created_at) VALUES (".(int)$company.",".$id.",".db_escape($employee).",".db_escape($assigned).",".($due===null?'NULL':db_escape($due)).",".db_escape($status).",".db_escape($ref).",".db_escape($hash).",".(int)$actor.",UTC_TIMESTAMP())");
    if(!$ok){cancel_transaction();return hrm_tal_001_catalog_fail($error,'learning_assignment_insert_failed');}$new=(int)db_insert_id();commit_transaction();return array('learning_assignment_id'=>$new,'replayed'=>false,'assignment_sha256'=>$hash);
}

/** @return array|false */
function hrm_tal_001_record_learning_completion(array $x, &$error=null)
{
    $error=null;if(!hrm_tal_001_learning_runtime_ready($error))return false;
    foreach(array('learning_assignment_id','completion_status_code','completed_on','evidence_reference_sha256') as $k)if(!array_key_exists($k,$x))return hrm_tal_001_catalog_fail($error,'learning_completion_field_missing');
    $assignment=(int)$x['learning_assignment_id'];$status=hrm_tal_001_token($x['completion_status_code'],24);$completed=hrm_tal_001_date($x['completed_on']);$score=isset($x['score_value'])?hrm_tal_001_decimal_or_null($x['score_value']):null;$ref=strtolower(trim((string)$x['evidence_reference_sha256']));
    if($assignment<=0||!in_array($status,array('completed','failed','waived'),true)||$completed===false||$score===false||($score!==null&&((float)$score<0||(float)$score>100))||!preg_match('/^[0-9a-f]{64}$/D',$ref))return hrm_tal_001_catalog_fail($error,'learning_completion_invalid');
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();$q=db_query("SELECT learning_assignment_id,assigned_on FROM ".TB_PREF."hrm_tal_001_learning_assignments WHERE learning_assignment_id=".$assignment." AND company_id=".(int)$company." LIMIT 2");$a=$q?db_fetch_assoc($q):false;
    if(!$a)return hrm_tal_001_catalog_fail($error,'learning_assignment_missing_or_cross_company');if(strcmp($completed,(string)$a['assigned_on'])<0)return hrm_tal_001_catalog_fail($error,'learning_completion_before_assignment');
    $hash=hash('sha256',implode('|',array($company,$assignment,$status,$completed,$score===null?'':$score,$ref)));
    if(!hrm_tal_001_learning_begin_transaction($error))return false;$same=db_query("SELECT learning_completion_evidence_id FROM ".TB_PREF."hrm_tal_001_learning_completion_evidence WHERE company_id=".(int)$company." AND evidence_sha256=".db_escape($hash)." LIMIT 2");
    if($same&&db_num_rows($same)===1){$row=db_fetch_assoc($same);commit_transaction();return array('learning_completion_evidence_id'=>(int)$row['learning_completion_evidence_id'],'replayed'=>true,'evidence_sha256'=>$hash);}
    $ok=db_query("INSERT INTO ".TB_PREF."hrm_tal_001_learning_completion_evidence (company_id,learning_assignment_id,completion_status_code,completed_on,score_value,evidence_reference_sha256,evidence_sha256,recorded_by_user_id,created_at) VALUES (".(int)$company.",".$assignment.",".db_escape($status).",".db_escape($completed).",".($score===null?'NULL':db_escape($score)).",".db_escape($ref).",".db_escape($hash).",".(int)$actor.",UTC_TIMESTAMP())");
    if(!$ok){cancel_transaction();return hrm_tal_001_catalog_fail($error,'learning_completion_insert_failed');}$new=(int)db_insert_id();commit_transaction();return array('learning_completion_evidence_id'=>$new,'replayed'=>false,'evidence_sha256'=>$hash);
}

/** @return array|false */
function hrm_tal_001_read_learning_for_employee($employee_id, &$error=null)
{
    $error=null;if(!hrm_tal_001_learning_runtime_ready($error))return false;$employee_id=trim((string)$employee_id);if($employee_id===''||strlen($employee_id)>20)return hrm_tal_001_catalog_fail($error,'employee_scope_invalid');
    $company=hrm_tal_001_runtime_company_id();$q=db_query("SELECT a.learning_assignment_id,d.learning_code,d.learning_name,d.delivery_mode_code,d.provider_name,d.duration_minutes,a.assigned_on,a.due_on,a.assignment_status_code,c.completion_status_code,c.completed_on,c.score_value FROM ".TB_PREF."hrm_tal_001_learning_assignments a INNER JOIN ".TB_PREF."hrm_tal_001_learning_definitions d ON d.learning_definition_id=a.learning_definition_id AND d.company_id=a.company_id LEFT JOIN ".TB_PREF."hrm_tal_001_learning_completion_evidence c ON c.learning_assignment_id=a.learning_assignment_id AND c.company_id=a.company_id WHERE a.company_id=".(int)$company." AND a.employee_id=".db_escape($employee_id)." ORDER BY a.assigned_on DESC,a.learning_assignment_id DESC,c.completed_on DESC,c.learning_completion_evidence_id DESC");
    if(!$q)return hrm_tal_001_catalog_fail($error,'learning_read_failed');$rows=array();while($row=db_fetch_assoc($q))$rows[]=$row;return $rows;
}

/* Dedicated TAL learning custody only: legacy training mutation is not authorized. */
?>
