<?php
/**
 * PAY-RULE-004 controlled Rule/Rule-Version/regulatory-evidence custody.
 *
 * This module is intentionally not loaded by payroll_engine.inc. It creates
 * governance evidence only; publication never means production activation.
 */
include_once(__DIR__.'/rule_lifecycle_contract.inc');
include_once(__DIR__.'/decision_rate_artifact_store.inc');

if (!defined('HRM_PAY_RULE_004_DRAFT_STORE_VERSION'))
    define('HRM_PAY_RULE_004_DRAFT_STORE_VERSION', '1.0.489');

function hrm_pay_rule_004_canonical_json($value)
{
    if (is_array($value)) {
        $assoc = array_keys($value) !== range(0, count($value) - 1);
        if ($assoc) {
            ksort($value, SORT_STRING);
            foreach ($value as $k=>$v) $value[$k] = hrm_pay_rule_004_canonicalize($v);
        } else {
            foreach ($value as $k=>$v) $value[$k] = hrm_pay_rule_004_canonicalize($v);
        }
    }
    return json_encode($value, JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);
}

function hrm_pay_rule_004_canonicalize($value)
{
    if (!is_array($value)) return $value;
    $assoc = array_keys($value) !== range(0, count($value) - 1);
    if ($assoc) ksort($value, SORT_STRING);
    foreach ($value as $k=>$v) $value[$k] = hrm_pay_rule_004_canonicalize($v);
    return $value;
}

function hrm_pay_rule_004_digest($parts)
{
    $json = hrm_pay_rule_004_canonical_json($parts);
    return is_string($json) ? hash('sha256', $json) : false;
}

function hrm_pay_rule_004_store_user_can($permission)
{
    return function_exists('user_check_access') && user_check_access($permission);
}

function hrm_pay_rule_004_store_actor_id()
{
    if (function_exists('hrm_person_worker_actor_id')) return (int)hrm_person_worker_actor_id();
    if (function_exists('user_id')) return (int)user_id();
    return 0;
}

function hrm_pay_rule_004_validate_version_chain_rows($rows, &$leaf_id, &$max_version)
{
    $leaf_id = null; $max_version = 0;
    if (!is_array($rows)) return false;
    if (!$rows) return true;
    $by_version=array(); $successor=array();
    foreach ($rows as $id=>$row) {
        $id=(int)$id; $n=isset($row['version_no'])?(int)$row['version_no']:0;
        if ($id<=0 || $n<=0 || isset($by_version[$n])) return false;
        $by_version[$n]=$id; $max_version=max($max_version,$n);
    }
    if (count($by_version)!==$max_version) return false;
    ksort($by_version,SORT_NUMERIC); $previous=null;
    foreach($by_version as $n=>$id) {
        $pred=$rows[$id]['predecessor_version_id']===null?null:(int)$rows[$id]['predecessor_version_id'];
        if ((int)$n===1) { if ($pred!==null) return false; }
        else {
            if ($pred!==$previous || !isset($rows[$pred]) || isset($successor[$pred])) return false;
            $successor[$pred]=$id;
        }
        $previous=$id;
    }
    $leaf_id=$previous; return true;
}

function hrm_pay_rule_004_prepare_draft_binding($rule, $artifact_row, &$error=null)
{
    $error=null;
    $validated=hrm_pay_rule_004_validate_rule_version_envelope($rule,$error);
    if ($validated===false) return false;
    if (!is_array($artifact_row)
        || !isset($artifact_row['artifact_version_id'],$artifact_row['artifact'],$artifact_row['artifact_sha256'],$artifact_row['plan_sha256'],$artifact_row['custody_status'])) {
        $error='artifact_binding_missing'; return false;
    }
    if ((int)$artifact_row['artifact_version_id']!==$validated['artifact_version_id']
        || (string)$artifact_row['artifact_sha256']!==$validated['artifact_sha256']
        || (string)$artifact_row['plan_sha256']!==$validated['plan_sha256']) {
        $error='artifact_binding_mismatch'; return false;
    }
    if (!isset($artifact_row['artifact']['artifact_type'])
        || (string)$artifact_row['artifact']['artifact_type']!==$validated['rule_type']) {
        $error='artifact_rule_type_mismatch'; return false;
    }
    if ((string)$artifact_row['custody_status']==='unverified_legacy') {
        $error='unverified_legacy_artifact_forbidden'; return false;
    }
    $e=$validated['regulatory_evidence'];
    $evidence_sha=hrm_pay_rule_004_digest(array(
        'rule_key'=>$validated['rule_key'],'version_label'=>$validated['version_label'],
        'artifact_version_id'=>$validated['artifact_version_id'],'artifact_sha256'=>$validated['artifact_sha256'],
        'plan_sha256'=>$validated['plan_sha256'],'jurisdiction_token'=>$e['jurisdiction_token'],
        'legal_reference'=>$e['legal_reference'],'source_evidence_ref'=>$e['source_evidence_ref'],
        'source_sha256'=>$e['source_sha256'],'effective_from'=>$e['effective_from'],
        'effective_to'=>$e['effective_to'],'captured_at'=>$e['captured_at'],'author_id'=>$validated['author_id']));
    if ($evidence_sha===false) { $error='evidence_hash_failed'; return false; }
    return $validated+array('evidence_sha256'=>$evidence_sha);
}

function hrm_pay_rule_004_version_row_matches($row,$binding,$predecessor_id,$version_no)
{
    if (!is_array($row)) return false;
    $pred=$row['predecessor_version_id']===null?null:(int)$row['predecessor_version_id'];
    return (int)$row['version_no']===(int)$version_no
        && $pred===$predecessor_id
        && (string)$row['version_label']===(string)$binding['version_label']
        && (int)$row['artifact_version_id']===(int)$binding['artifact_version_id']
        && (string)$row['artifact_sha256']===(string)$binding['artifact_sha256']
        && (string)$row['plan_sha256']===(string)$binding['plan_sha256']
        && (string)$row['effective_from']===(string)$binding['regulatory_evidence']['effective_from']
        && (string)$row['effective_to']===(string)$binding['regulatory_evidence']['effective_to']
        && (int)$row['author_id']===(int)$binding['author_id'];
}

function hrm_pay_rule_004_evidence_row_matches($row,$binding)
{
    if (!is_array($row)) return false; $e=$binding['regulatory_evidence'];
    return (string)$row['jurisdiction_token']===$e['jurisdiction_token']
        && (string)$row['legal_reference']===$e['legal_reference']
        && (string)$row['source_evidence_ref']===$e['source_evidence_ref']
        && (string)$row['source_sha256']===$e['source_sha256']
        && (string)$row['effective_from']===$e['effective_from']
        && (string)$row['effective_to']===$e['effective_to']
        && (string)$row['captured_at']===$e['captured_at']
        && (int)$row['created_by']===(int)$binding['author_id']
        && (string)$row['evidence_sha256']===(string)$binding['evidence_sha256'];
}

/**
 * Append a controlled draft Rule Version and its regulatory evidence.
 * Exact retries verify the accepted immutable rows and return their id.
 */
function hrm_pay_rule_004_create_draft_version($rule,$predecessor_version_id=null,$actor_id=null,$manage_transaction=true)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event')) include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if ($actor_id===null) $actor_id=hrm_pay_rule_004_store_actor_id();
    if ($actor_id<=0 || !hrm_pay_rule_004_store_user_can('SA_HRM_AUTHOR_PAY_RULE')) return false;
    if (!is_array($rule)) return false;
    $rule['author_id']=(int)$actor_id;
    if ($predecessor_version_id!==null && (!is_int($predecessor_version_id)||$predecessor_version_id<=0)) return false;
    $artifact_error=null;
    $artifact=hrm_pay_rule_003_read_artifact_version_internal((int)($rule['artifact_version_id']??0),$artifact_error);
    $error=null; $binding=hrm_pay_rule_004_prepare_draft_binding($rule,$artifact,$error);
    if ($binding===false) return false;
    if ($manage_transaction) begin_transaction();
    $ok=true; $rule_id=0; $return_id=false; $created_rule=false;
    $q=db_query('SELECT rule_id,rule_type,created_by FROM '.TB_PREF.'hrm_pay_rule_rules WHERE rule_key='.
        db_escape($binding['rule_key']).' LIMIT 2 FOR UPDATE');
    if (!$q || db_num_rows($q)>1) $ok=false;
    if ($ok && db_num_rows($q)===1) {
        $identity=db_fetch_assoc($q); $rule_id=(int)$identity['rule_id'];
        if ((string)$identity['rule_type']!==$binding['rule_type']) $ok=false;
    } elseif ($ok && $predecessor_version_id!==null) $ok=false;
    elseif ($ok) {
        $ok=db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_rules (rule_key,rule_type,created_by) VALUES ('.
            db_escape($binding['rule_key']).','.db_escape($binding['rule_type']).','.(int)$actor_id.')',
            'could not create PAY-RULE-004 stable Rule identity')!==false;
        if ($ok) { $rule_id=(int)db_insert_id(); $created_rule=$rule_id>0; if(!$created_rule)$ok=false; }
    }
    $rows=array();$leaf=null;$max=0;
    if ($ok) {
        $q=db_query('SELECT rule_version_id,version_no,predecessor_version_id,version_label,artifact_version_id,artifact_sha256,plan_sha256,effective_from,effective_to,author_id FROM '.TB_PREF.'hrm_pay_rule_rule_versions WHERE rule_id='.(int)$rule_id.' ORDER BY version_no,rule_version_id FOR UPDATE');
        if(!$q)$ok=false; else {while($row=db_fetch_assoc($q))$rows[(int)$row['rule_version_id']]=$row;if(!hrm_pay_rule_004_validate_version_chain_rows($rows,$leaf,$max))$ok=false;}
    }
    $candidate_id=false;$candidate_version=0;
    if($ok && $rows){
        foreach($rows as $id=>$row){
            $pred=$row['predecessor_version_id']===null?null:(int)$row['predecessor_version_id'];
            if($pred===$predecessor_version_id && hrm_pay_rule_004_version_row_matches($row,$binding,$predecessor_version_id,(int)$row['version_no'])){
                $candidate_id=(int)$id;$candidate_version=(int)$row['version_no'];break;
            }
        }
        if($candidate_id!==false)$return_id=$candidate_id;
    }
    if($ok && $return_id===false && $predecessor_version_id!==null){
        if(!isset($rows[$predecessor_version_id]) || $leaf!==$predecessor_version_id)$ok=false;
        else $candidate_version=$max+1;
    } elseif($ok && $return_id===false && $rows){
        $ok=false;
    } elseif($ok && $return_id===false){
        $candidate_version=1;if(!$created_rule)$ok=false;
    }
    if($ok && $return_id===false){
        $e=$binding['regulatory_evidence'];
        $ok=db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_rule_versions (rule_id,version_no,predecessor_version_id,version_label,artifact_version_id,artifact_sha256,plan_sha256,effective_from,effective_to,author_id) VALUES ('.
            (int)$rule_id.','.(int)$candidate_version.','.($predecessor_version_id===null?'NULL':(int)$predecessor_version_id).','.
            db_escape($binding['version_label']).','.(int)$binding['artifact_version_id'].','.db_escape($binding['artifact_sha256']).','.db_escape($binding['plan_sha256']).','.
            db_escape($e['effective_from']).','.db_escape($e['effective_to']).','.(int)$actor_id.')','could not append PAY-RULE-004 Rule Version')!==false;
        if($ok){$return_id=(int)db_insert_id();if($return_id<=0)$ok=false;}
        if($ok)$ok=db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_regulatory_evidence (rule_version_id,jurisdiction_token,legal_reference,source_evidence_ref,source_sha256,effective_from,effective_to,captured_at,created_by,evidence_sha256) VALUES ('.
            (int)$return_id.','.db_escape($e['jurisdiction_token']).','.db_escape($e['legal_reference']).','.db_escape($e['source_evidence_ref']).','.db_escape($e['source_sha256']).','.
            db_escape($e['effective_from']).','.db_escape($e['effective_to']).','.db_escape(str_replace('T',' ',rtrim($e['captured_at'],'Z'))).','.(int)$actor_id.','.db_escape($binding['evidence_sha256']).')',
            'could not register PAY-RULE-004 regulatory evidence')!==false;
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
            'entity_type'=>'pay_rule_004_rule_version','entity_key'=>array('rule_id'=>$rule_id,'rule_version_id'=>$return_id,'version_no'=>$candidate_version),
            'action'=>'pay.rule_004.draft_created','reason_code'=>'pay_rule_004_draft_writer','before'=>$predecessor_version_id===null?null:array('predecessor_version_id'=>$predecessor_version_id),
            'after'=>array('state'=>'draft','rule_key'=>$binding['rule_key'],'version_label'=>$binding['version_label']),
            'restricted_diff'=>array('artifact_version_id'=>$binding['artifact_version_id'],'artifact_sha256'=>$binding['artifact_sha256'],'plan_sha256'=>$binding['plan_sha256'],'regulatory_evidence_sha256'=>$binding['evidence_sha256'],'production_activation_allowed'=>false)));
            if($audit===false)$ok=false;}
    } elseif($ok && $return_id!==false) {
        $q=db_query('SELECT jurisdiction_token,legal_reference,source_evidence_ref,source_sha256,effective_from,effective_to,DATE_FORMAT(captured_at,\'%Y-%m-%dT%H:%i:%sZ\') captured_at,created_by,evidence_sha256 FROM '.TB_PREF.'hrm_pay_rule_regulatory_evidence WHERE rule_version_id='.(int)$return_id.' LIMIT 2');
        if(!$q||db_num_rows($q)!==1||!hrm_pay_rule_004_evidence_row_matches(db_fetch_assoc($q),$binding))$ok=false;
    }
    if($manage_transaction){if($ok)commit_transaction();else cancel_transaction();}
    return $ok?$return_id:false;
}

function hrm_pay_rule_004_read_version_internal($rule_version_id,$for_update=false,&$error=null)
{
    $error=null;if(!is_int($rule_version_id)||$rule_version_id<=0){$error='rule_version_id_invalid';return false;}
    $sql='SELECT r.rule_key,r.rule_type,v.*,e.regulatory_evidence_id,e.jurisdiction_token,e.legal_reference,e.source_evidence_ref,e.source_sha256,e.effective_from evidence_effective_from,e.effective_to evidence_effective_to,DATE_FORMAT(e.captured_at,\'%Y-%m-%dT%H:%i:%sZ\') captured_at,e.created_by evidence_created_by,e.evidence_sha256 FROM '.TB_PREF.'hrm_pay_rule_rule_versions v JOIN '.TB_PREF.'hrm_pay_rule_rules r ON r.rule_id=v.rule_id JOIN '.TB_PREF.'hrm_pay_rule_regulatory_evidence e ON e.rule_version_id=v.rule_version_id WHERE v.rule_version_id='.(int)$rule_version_id.' LIMIT 2'.($for_update?' FOR UPDATE':'');
    $q=db_query($sql);if(!$q||db_num_rows($q)!==1){$error='rule_version_not_found_or_ambiguous';return false;}return db_fetch_assoc($q);
}
