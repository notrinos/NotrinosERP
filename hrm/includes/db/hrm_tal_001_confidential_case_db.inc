<?php
/** HRM-TAL-001 bounded confidential-case runtime over minimized immutable custody. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_evidence_db.inc');

/** @return bool */
function hrm_tal_001_confidential_case_writer_ready(&$error)
{
    $version=hrm_tal_001_runtime_database_version();
    if(!is_string($version)||!hrm_tal_001_confidential_case_writer_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error,'confidential_case_writer_not_active');
    if(!function_exists('user_check_access')||!user_check_access('SA_HRSETTINGS'))
        return hrm_tal_001_catalog_fail($error,'permission_denied');
    if(hrm_tal_001_runtime_company_id()<0||hrm_tal_001_runtime_actor_id()<=0)
        return hrm_tal_001_catalog_fail($error,'authenticated_scope_required');
    return true;
}

/** @return bool */
function hrm_tal_001_confidential_case_read_ready(&$error)
{
    $version=hrm_tal_001_runtime_database_version();
    if(!is_string($version)||!hrm_tal_001_confidential_case_read_only_view_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error,'confidential_case_read_not_active');
    if(!function_exists('user_check_access')||!user_check_access('SA_HRSETTINGS'))
        return hrm_tal_001_catalog_fail($error,'permission_denied');
    if(hrm_tal_001_runtime_company_id()<0||hrm_tal_001_runtime_actor_id()<=0)
        return hrm_tal_001_catalog_fail($error,'authenticated_scope_required');
    return true;
}

/** @return bool */
function hrm_tal_001_confidential_case_begin_transaction(&$error)
{
    global $transaction_level;
    if(!empty($transaction_level))return hrm_tal_001_catalog_fail($error,'confidential_case_owned_transaction_required');
    begin_transaction();
    $r=db_query("SELECT value FROM ".TB_PREF."sys_prefs WHERE name='version_id' AND category='system' LIMIT 2 FOR UPDATE");
    $row=$r?db_fetch_assoc($r):false;
    if(!$row||!isset($row['value'])||!hrm_tal_001_confidential_case_writer_allowed_for_database_version((string)$row['value'])){
        cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_writer_not_active');
    }
    return true;
}

/** @param mixed $value @return int|false */
function hrm_tal_001_confidential_case_positive_id($value)
{
    if(is_int($value))return $value>0?$value:false;
    if(!is_string($value)||!preg_match('/^[0-9]+$/D',$value))return false;
    $digits=ltrim($value,'0');if($digits==='')return false;
    $maximum=(string)PHP_INT_MAX;
    if(strlen($digits)>strlen($maximum)||(strlen($digits)===strlen($maximum)&&strcmp($digits,$maximum)>0))return false;
    return (int)$digits;
}

/** @return bool */
function hrm_tal_001_confidential_case_user_exists($user_id)
{
    $user_id=hrm_tal_001_confidential_case_positive_id($user_id);if($user_id===false)return false;
    $r=db_query('SELECT id,inactive FROM '.TB_PREF.'users WHERE id='.$user_id.' LIMIT 2');
    if(!$r||db_num_rows($r)!==1)return false;
    $row=db_fetch_assoc($r);return (int)$row['inactive']===0;
}

/** @return array|false */
function hrm_tal_001_confidential_case_row($case_id,&$error,$lock=false)
{
    $company=hrm_tal_001_runtime_company_id();$case_id=hrm_tal_001_confidential_case_positive_id($case_id);
    if($case_id===false)return hrm_tal_001_catalog_fail($error,'confidential_case_id_invalid');
    $sql='SELECT confidential_case_id,company_id,subject_employee_id,case_type_code,purpose_code,opened_on,retention_until,sensitivity_code FROM '.TB_PREF.'hrm_tal_001_confidential_cases WHERE confidential_case_id='.$case_id.' AND company_id='.(int)$company.' LIMIT 2'.($lock?' FOR UPDATE':'');
    $r=db_query($sql);if(!$r||db_num_rows($r)!==1)return hrm_tal_001_catalog_fail($error,'confidential_case_missing_or_cross_company');
    return db_fetch_assoc($r);
}

/** @return bool */
function hrm_tal_001_confidential_case_raw_payload_denied(array $x)
{
    foreach(array('title','case_title','description','notes','narrative','details','content','attachment','attachment_id','document_id','file','path','salary','bank_account','tax_id','payroll_value') as $key)
        if(array_key_exists($key,$x))return true;
    return false;
}

/** @return array|false */
function hrm_tal_001_open_confidential_case(array $x,&$error=null)
{
    $error=null;if(!hrm_tal_001_confidential_case_writer_ready($error))return false;
    foreach(array('subject_employee_id','case_type_code','purpose_code','opened_on','retention_until','sensitivity_code','title_reference_sha256','case_reference_sha256') as $k)
        if(!array_key_exists($k,$x))return hrm_tal_001_catalog_fail($error,'confidential_case_field_missing');
    if(hrm_tal_001_confidential_case_raw_payload_denied($x))return hrm_tal_001_catalog_fail($error,'confidential_case_raw_payload_denied');
    $employee=trim((string)$x['subject_employee_id']);$type=hrm_tal_001_token($x['case_type_code'],32);$purpose=hrm_tal_001_token($x['purpose_code'],48);
    $opened=hrm_tal_001_date($x['opened_on']);$retention=hrm_tal_001_date($x['retention_until']);$sensitivity=hrm_tal_001_token($x['sensitivity_code'],24);
    $title_ref=hrm_tal_001_sha256($x['title_reference_sha256']);$case_ref=hrm_tal_001_sha256($x['case_reference_sha256']);
    if($employee===''||strlen($employee)>20||$type===false||!in_array($type,array('employee_relations','grievance','investigation','accommodation','ethics','other_restricted'),true)
        ||$purpose===false||!in_array($purpose,array('case_management','investigation','employee_relations','legal_hold'),true)
        ||$opened===false||$retention===false||strcmp($retention,$opened)<0||$sensitivity===false||!in_array($sensitivity,array('restricted','highly_restricted'),true)
        ||$title_ref===false||$case_ref===false||!hrm_tal_001_employee_exists($employee))
        return hrm_tal_001_catalog_fail($error,'confidential_case_invalid');
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();
    if(!hrm_tal_001_confidential_case_begin_transaction($error))return false;
    $same=db_query('SELECT confidential_case_id,company_id,subject_employee_id,case_type_code,purpose_code,opened_on,retention_until,sensitivity_code,title_reference_sha256 FROM '.TB_PREF.'hrm_tal_001_confidential_cases WHERE company_id='.(int)$company.' AND case_reference_sha256='.db_escape($case_ref).' LIMIT 2');
    if(!$same||db_num_rows($same)>1){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_lookup_failed');}
    if(db_num_rows($same)===1){$row=db_fetch_assoc($same);$match=(int)$row['company_id']===$company&&(string)$row['subject_employee_id']===$employee&&(string)$row['case_type_code']===$type&&(string)$row['purpose_code']===$purpose&&(string)$row['opened_on']===$opened&&(string)$row['retention_until']===$retention&&(string)$row['sensitivity_code']===$sensitivity&&hash_equals((string)$row['title_reference_sha256'],$title_ref);if(!$match){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_reference_conflict');}$id=(int)$row['confidential_case_id'];commit_transaction();return array('confidential_case_id'=>$id,'case_reference_sha256'=>$case_ref,'idempotent'=>true);}
    $sql='INSERT INTO '.TB_PREF.'hrm_tal_001_confidential_cases (company_id,subject_employee_id,case_type_code,purpose_code,opened_on,retention_until,sensitivity_code,title_reference_sha256,case_reference_sha256,created_by_user_id,created_at) VALUES ('.(int)$company.','.db_escape($employee).','.db_escape($type).','.db_escape($purpose).','.db_escape($opened).','.db_escape($retention).','.db_escape($sensitivity).','.db_escape($title_ref).','.db_escape($case_ref).','.(int)$actor.',UTC_TIMESTAMP())';
    if(!db_query($sql)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_insert_failed');}
    $id=(int)db_insert_id();$owner_ref=hash('sha256','hrm-tal-001|case-owner|'.$company.'|'.$id.'|'.$actor.'|'.$purpose.'|'.$opened.'|'.$retention);
    $g='INSERT INTO '.TB_PREF.'hrm_tal_001_confidential_case_access_grants (company_id,confidential_case_id,grantee_user_id,access_role_code,purpose_code,valid_from,valid_to,grant_reference_sha256,granted_by_user_id,created_at) VALUES ('.(int)$company.','.$id.','.(int)$actor.','.db_escape('case_owner').','.db_escape($purpose).','.db_escape($opened).','.db_escape($retention).','.db_escape($owner_ref).','.(int)$actor.',UTC_TIMESTAMP())';
    if(!db_query($g)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_owner_grant_insert_failed');}
    commit_transaction();return array('confidential_case_id'=>$id,'case_reference_sha256'=>$case_ref,'idempotent'=>false);
}

/** @return array|false */
function hrm_tal_001_grant_confidential_case_access(array $x,&$error=null)
{
    $error=null;if(!hrm_tal_001_confidential_case_writer_ready($error))return false;
    foreach(array('confidential_case_id','grantee_user_id','access_role_code','purpose_code','valid_from','grant_reference_sha256') as $k)
        if(!array_key_exists($k,$x))return hrm_tal_001_catalog_fail($error,'confidential_case_access_field_missing');
    if(hrm_tal_001_confidential_case_raw_payload_denied($x))return hrm_tal_001_catalog_fail($error,'confidential_case_raw_payload_denied');
    $case_id=hrm_tal_001_confidential_case_positive_id($x['confidential_case_id']);$grantee=hrm_tal_001_confidential_case_positive_id($x['grantee_user_id']);$role=hrm_tal_001_token($x['access_role_code'],24);$purpose=hrm_tal_001_token($x['purpose_code'],48);
    $from=hrm_tal_001_date($x['valid_from']);$to=hrm_tal_001_optional_date(isset($x['valid_to'])?$x['valid_to']:null);$ref=hrm_tal_001_sha256($x['grant_reference_sha256']);
    if($case_id===false||$grantee===false||$role===false||!in_array($role,array('case_owner','case_reviewer','case_reader'),true)||$purpose===false||$from===false||($to!==null&&($to===false||strcmp($to,$from)<0))||$ref===false||!hrm_tal_001_confidential_case_user_exists($grantee))
        return hrm_tal_001_catalog_fail($error,'confidential_case_access_invalid');
    if(!hrm_tal_001_confidential_case_begin_transaction($error))return false;
    $case=hrm_tal_001_confidential_case_row($case_id,$error,true);if($case===false){cancel_transaction();return false;}
    if((string)$case['purpose_code']!==$purpose||strcmp($from,(string)$case['opened_on'])<0||strcmp($from,(string)$case['retention_until'])>0||($to!==null&&strcmp($to,(string)$case['retention_until'])>0)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_access_outside_case_scope');}
    $same=db_query('SELECT confidential_case_access_grant_id,company_id,confidential_case_id,grantee_user_id,access_role_code,purpose_code,valid_from,valid_to FROM '.TB_PREF.'hrm_tal_001_confidential_case_access_grants WHERE company_id='.(int)hrm_tal_001_runtime_company_id().' AND grant_reference_sha256='.db_escape($ref).' LIMIT 2');
    if(!$same||db_num_rows($same)>1){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_access_lookup_failed');}
    if(db_num_rows($same)===1){$row=db_fetch_assoc($same);$match=(int)$row['company_id']===hrm_tal_001_runtime_company_id()&&(int)$row['confidential_case_id']===$case_id&&(int)$row['grantee_user_id']===$grantee&&(string)$row['access_role_code']===$role&&(string)$row['purpose_code']===$purpose&&(string)$row['valid_from']===$from&&((string)$row['valid_to']===($to===null?'':$to));if(!$match){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_access_reference_conflict');}$id=(int)$row['confidential_case_access_grant_id'];commit_transaction();return array('confidential_case_access_grant_id'=>$id,'grant_reference_sha256'=>$ref,'idempotent'=>true);}
    $actor=hrm_tal_001_runtime_actor_id();$sql='INSERT INTO '.TB_PREF.'hrm_tal_001_confidential_case_access_grants (company_id,confidential_case_id,grantee_user_id,access_role_code,purpose_code,valid_from,valid_to,grant_reference_sha256,granted_by_user_id,created_at) VALUES ('.(int)hrm_tal_001_runtime_company_id().','.$case_id.','.$grantee.','.db_escape($role).','.db_escape($purpose).','.db_escape($from).','.($to===null?'NULL':db_escape($to)).','.db_escape($ref).','.(int)$actor.',UTC_TIMESTAMP())';
    if(!db_query($sql)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_access_insert_failed');}
    $id=(int)db_insert_id();commit_transaction();return array('confidential_case_access_grant_id'=>$id,'grant_reference_sha256'=>$ref,'idempotent'=>false);
}

/** @return array|false */
function hrm_tal_001_revoke_confidential_case_access(array $x,&$error=null)
{
    $error=null;if(!hrm_tal_001_confidential_case_writer_ready($error))return false;
    foreach(array('confidential_case_id','grantee_user_id','effective_on','revocation_reference_sha256') as $k)
        if(!array_key_exists($k,$x))return hrm_tal_001_catalog_fail($error,'confidential_case_revocation_field_missing');
    if(hrm_tal_001_confidential_case_raw_payload_denied($x))return hrm_tal_001_catalog_fail($error,'confidential_case_raw_payload_denied');
    $case_id=hrm_tal_001_confidential_case_positive_id($x['confidential_case_id']);$grantee=hrm_tal_001_confidential_case_positive_id($x['grantee_user_id']);$effective=hrm_tal_001_date($x['effective_on']);$ref=hrm_tal_001_sha256($x['revocation_reference_sha256']);
    if($case_id===false||$grantee===false||$effective===false||$ref===false)return hrm_tal_001_catalog_fail($error,'confidential_case_revocation_invalid');
    if(!hrm_tal_001_confidential_case_begin_transaction($error))return false;
    $case=hrm_tal_001_confidential_case_row($case_id,$error,true);if($case===false){cancel_transaction();return false;}
    if(strcmp($effective,(string)$case['opened_on'])<0||strcmp($effective,(string)$case['retention_until'])>0){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_revocation_outside_case_scope');}
    $same=db_query('SELECT confidential_case_access_grant_id,company_id,confidential_case_id,grantee_user_id,access_role_code,purpose_code,valid_from FROM '.TB_PREF.'hrm_tal_001_confidential_case_access_grants WHERE company_id='.(int)hrm_tal_001_runtime_company_id().' AND grant_reference_sha256='.db_escape($ref).' LIMIT 2');
    if(!$same||db_num_rows($same)>1){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_revocation_lookup_failed');}
    if(db_num_rows($same)===1){$row=db_fetch_assoc($same);$match=(int)$row['company_id']===hrm_tal_001_runtime_company_id()&&(int)$row['confidential_case_id']===$case_id&&(int)$row['grantee_user_id']===$grantee&&(string)$row['access_role_code']==='revoked'&&(string)$row['purpose_code']===(string)$case['purpose_code']&&(string)$row['valid_from']===$effective;if(!$match){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_revocation_reference_conflict');}$id=(int)$row['confidential_case_access_grant_id'];commit_transaction();return array('confidential_case_access_grant_id'=>$id,'grant_reference_sha256'=>$ref,'idempotent'=>true);}
    $actor=hrm_tal_001_runtime_actor_id();$sql='INSERT INTO '.TB_PREF.'hrm_tal_001_confidential_case_access_grants (company_id,confidential_case_id,grantee_user_id,access_role_code,purpose_code,valid_from,valid_to,grant_reference_sha256,granted_by_user_id,created_at) VALUES ('.(int)hrm_tal_001_runtime_company_id().','.$case_id.','.$grantee.','.db_escape('revoked').','.db_escape((string)$case['purpose_code']).','.db_escape($effective).','.db_escape((string)$case['retention_until']).','.db_escape($ref).','.(int)$actor.',UTC_TIMESTAMP())';
    if(!db_query($sql)){cancel_transaction();return hrm_tal_001_catalog_fail($error,'confidential_case_revocation_insert_failed');}
    $id=(int)db_insert_id();commit_transaction();return array('confidential_case_access_grant_id'=>$id,'grant_reference_sha256'=>$ref,'idempotent'=>false);
}

/** @return array|false */
function hrm_tal_001_read_confidential_case($case_id,&$error=null)
{
    $error=null;if(!hrm_tal_001_confidential_case_read_ready($error))return false;
    $case_id=hrm_tal_001_confidential_case_positive_id($case_id);
    if($case_id===false)return hrm_tal_001_catalog_fail($error,'confidential_case_id_invalid');
    $case=hrm_tal_001_confidential_case_row($case_id,$error,false);if($case===false)return false;
    $today=gmdate('Y-m-d');if(strcmp($today,(string)$case['retention_until'])>0)return hrm_tal_001_catalog_fail($error,'confidential_case_retention_elapsed');
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();
    $sql='SELECT confidential_case_access_grant_id,access_role_code,purpose_code,valid_from,valid_to FROM '.TB_PREF.'hrm_tal_001_confidential_case_access_grants WHERE company_id='.(int)$company.' AND confidential_case_id='.(int)$case_id.' AND grantee_user_id='.(int)$actor.' AND valid_from<='.db_escape($today).' AND (valid_to IS NULL OR valid_to>='.db_escape($today).') ORDER BY valid_from DESC,confidential_case_access_grant_id DESC LIMIT 2';
    $r=db_query($sql);if(!$r||db_num_rows($r)<1)return hrm_tal_001_catalog_fail($error,'confidential_case_object_access_denied');
    $grant=db_fetch_assoc($r);$role=(string)$grant['access_role_code'];if($role==='revoked'||!in_array($role,array('case_owner','case_reviewer','case_reader'),true)||(string)$grant['purpose_code']!==(string)$case['purpose_code'])return hrm_tal_001_catalog_fail($error,'confidential_case_object_access_denied');
    return array('confidential_case_id'=>(int)$case['confidential_case_id'],'subject_employee_id'=>(string)$case['subject_employee_id'],'case_type_code'=>(string)$case['case_type_code'],'purpose_code'=>(string)$case['purpose_code'],'opened_on'=>(string)$case['opened_on'],'retention_until'=>(string)$case['retention_until'],'sensitivity_code'=>(string)$case['sensitivity_code'],'access_role_code'=>$role);
}
?>
