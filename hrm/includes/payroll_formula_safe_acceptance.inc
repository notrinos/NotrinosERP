<?php
/**********************************************************************
    PAY-RULE-002 configured-company shadow acceptance and publication
    custody. Production payroll routing remains intentionally absent.
***********************************************************************/
include_once(dirname(__FILE__).'/payroll_formula_safe_shadow.inc');
include_once(dirname(__FILE__).'/payroll_formula_exception_review.inc');

if (!defined('HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_ID'))
    define('HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_ID', 'notrinoserp.hrm.payroll.formula_safe_acceptance');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_VERSION', '1.0.0');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_SOURCE_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_SOURCE_VERSION', '1.0.470');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_REVIEW_ID'))
    define('HRM_PAYROLL_FORMULA_SAFE_REVIEW_ID', 'notrinoserp.hrm.payroll.formula_safe_acceptance_review');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_REVIEW_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_REVIEW_VERSION', '1.0.0');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_ID'))
    define('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_ID', 'notrinoserp.hrm.payroll.formula_safe_publication_receipt');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_VERSION', '1.0.0');

if (!defined('HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_ID'))
    define('HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_ID', 'notrinoserp.hrm.payroll.formula_safe_shadow_execution_receipt');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_VERSION', '1.0.0');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_RECOVERY_POLICY'))
    define('HRM_PAYROLL_FORMULA_SAFE_RECOVERY_POLICY', 'forward-correct/reissue new artifacts; never overwrite accepted artifacts; paired pre-upgrade application/database backup restore for source rollback');

/** Return a canonical SHA-256 for a redacted PAY-RULE-002 governance payload. */
function hrm_payroll_formula_safe_acceptance_hash($value)
{
    return hrm_legacy_formula_review_payload_hash($value);
}

/** Return a non-identifying fingerprint for one configured operating-system identity. */
function hrm_payroll_formula_safe_acceptance_fingerprint($identity)
{
    return hrm_legacy_formula_review_identity_fingerprint($identity);
}

/** Return the configured operating-system identity for one PAY-RULE-002 governance role. */
function hrm_payroll_formula_safe_acceptance_expected_os_user($company, $role)
{
    global $db_connections;
    if ($role === 'author')
        return hrm_legacy_formula_review_expected_os_user($company, 'data_owner');
    if ($role === 'reviewer')
        return hrm_legacy_formula_review_expected_os_user($company, 'security_reviewer');
    $constant = $role === 'publisher'
        ? 'PAYROLL_FORMULA_PUBLISHER_OS_USER'
        : ($role === 'shadow_executor' ? 'PAYROLL_FORMULA_SHADOW_EXECUTOR_OS_USER' : '');
    $key = $role === 'publisher'
        ? 'payroll_formula_publisher_os_user'
        : ($role === 'shadow_executor' ? 'payroll_formula_shadow_executor_os_user' : '');
    if ($constant !== '' && defined($constant))
        return (string)constant($constant);
    return $key !== '' && isset($db_connections[$company][$key])
        ? (string)$db_connections[$company][$key] : '';
}

/** Require four configured and distinct operating-system identities for author, reviewer, publisher, and shadow executor. */
function hrm_payroll_formula_safe_acceptance_roles_separated($company)
{
    $seen = array();
    foreach (array('author','reviewer','publisher','shadow_executor') as $role) {
        $identity = hrm_payroll_formula_safe_acceptance_expected_os_user($company, $role);
        if ($identity === '' || strlen($identity) > 128
            || preg_match('/^[A-Za-z0-9_.@\\\\-]+$/D', $identity) !== 1)
            return false;
        $normalized = strtolower($identity);
        if (isset($seen[$normalized]))
            return false;
        $seen[$normalized] = true;
    }
    return true;
}

/** Require the current CLI operating-system identity to match the configured governance role. */
function hrm_payroll_formula_safe_acceptance_authorized_identity($company, $role)
{
    if (PHP_SAPI !== 'cli')
        return false;
    $expected = hrm_payroll_formula_safe_acceptance_expected_os_user($company, $role);
    $actual = domain_audit_key_admin_os_identity();
    if ($expected === '' || $actual === false
        || !domain_audit_hash_equals(strtolower($expected), strtolower($actual)))
        return false;
    return $actual;
}

/** Return the configured redacted fingerprint for one ceremony role. */
function hrm_payroll_formula_safe_acceptance_expected_role_fingerprint($company, $role)
{
    $identity = hrm_payroll_formula_safe_acceptance_expected_os_user($company, $role);
    if ($identity === '' || strlen($identity) > 128
        || preg_match('/^[A-Za-z0-9_.@\\\\-]+$/D', $identity) !== 1)
        return false;
    return hrm_payroll_formula_safe_acceptance_fingerprint($identity);
}

/** Bind a redacted artifact fingerprint to the configured role identity. */
function hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($company, $role, $fingerprint)
{
    $expected = hrm_payroll_formula_safe_acceptance_expected_role_fingerprint($company, $role);
    return $expected !== false && is_string($fingerprint)
        && hash_equals($expected, $fingerprint);
}

/** Compare canonical UTC timestamps without locale/time-zone ambiguity. */
function hrm_payroll_formula_safe_acceptance_utc_not_before($later, $earlier)
{
    return hrm_legacy_formula_review_is_utc_timestamp($later)
        && hrm_legacy_formula_review_is_utc_timestamp($earlier)
        && strcmp($later, $earlier) >= 0;
}

/** Return immutable source/governance hashes that every configured-company acceptance artifact must bind. */
function hrm_payroll_formula_safe_acceptance_bindings()
{
    global $path_to_root;
    $bindings = hrm_payroll_formula_safe_publication_bindings();
    $bindings['source_publication_registry_sha256'] = hash_file(
        'sha256', $path_to_root.'/hrm/config/payroll_formula_safe_publication_registry.json');
    return $bindings;
}

/**
 * Build the exact configured-company source query for formulas that the legacy
 * payroll runtime can currently execute. pay_elements.formula is governance
 * metadata only: salary-component routing selects employee_salary.formula or
 * salary_structure.formula and never pe.formula. The two executable sources
 * are restricted to amount_type=3 plus the same active/effective-date guards
 * used by get_employee_salary_components()/get_salary_components_for_employees().
 *
 * Returning an empty string is intentional for a catalogued but runtime-
 * unreachable source so report scope remains stable without publishing inert
 * metadata as a shadow candidate.
 */
function hrm_payroll_formula_safe_acceptance_reachable_source_sql($scope, &$error=null)
{
    $error = null;
    if ($scope === 'pay_elements.formula')
        return '';

    if ($scope === 'salary_structure.formula') {
        return "SELECT TRIM(ss.formula) AS formula_source, COUNT(*) AS row_count"
            ." FROM ".TB_PREF."salary_structure ss"
            ." JOIN ".TB_PREF."pay_elements pe ON pe.element_id = ss.element_id"
            ." WHERE pe.amount_type = 3"
            ." AND ss.is_active = 1"
            ." AND ss.effective_from <= CURRENT_DATE()"
            ." AND (ss.effective_to IS NULL OR ss.effective_to = '0000-00-00' OR ss.effective_to >= CURRENT_DATE())"
            ." AND TRIM(COALESCE(ss.formula, '')) <> ''"
            ." GROUP BY TRIM(ss.formula) ORDER BY SHA2(TRIM(ss.formula), 256)";
    }

    if ($scope === 'employee_salary.formula') {
        return "SELECT TRIM(es.formula) AS formula_source, COUNT(*) AS row_count"
            ." FROM ".TB_PREF."employee_salary es"
            ." JOIN ".TB_PREF."pay_elements pe ON pe.element_id = es.element_id"
            ." WHERE pe.amount_type = 3"
            ." AND es.is_active = 1"
            ." AND es.effective_from <= CURRENT_DATE()"
            ." AND (es.effective_to IS NULL OR es.effective_to = '0000-00-00' OR es.effective_to >= CURRENT_DATE())"
            ." AND TRIM(COALESCE(es.formula, '')) <> ''"
            ." GROUP BY TRIM(es.formula) ORDER BY SHA2(TRIM(es.formula), 256)";
    }

    $error = 'safe_acceptance_unknown_source_scope';
    return false;
}

/** Read only currently runtime-reachable legacy formula sources from the configured company. */
function hrm_payroll_formula_safe_acceptance_collect_rows(&$error=null)
{
    $error = null;
    $rows = array();
    foreach (hrm_legacy_formula_source_catalog() as $scope=>$definition) {
        $rows[$scope]=array();
        $sql = hrm_payroll_formula_safe_acceptance_reachable_source_sql($scope, $error);
        if ($sql === false)
            return false;
        if ($sql === '')
            continue;
        $result = db_query($sql, 'could not build PAY-RULE-002 configured-company runtime-reachable shadow acceptance evidence');
        if ($result === false) { $error='safe_acceptance_inventory_query_failed'; return false; }
        while ($row=db_fetch_assoc($result)) {
            $count=(int)$row['row_count'];
            if ($count<1 || !is_string($row['formula_source']) || trim($row['formula_source'])==='') {
                $error='safe_acceptance_inventory_row_invalid'; return false;
            }
            $rows[$scope][]=array('formula_source'=>$row['formula_source'],'row_count'=>$count);
        }
    }
    return $rows;
}

/** Classify a bounded configured-company formula inventory into accepted and blocked redacted evidence. */
function hrm_payroll_formula_safe_acceptance_inventory(array $rows_by_scope, array $corpus, &$error=null)
{
    $error=null;
    $validated=hrm_legacy_formula_validate_corpus($corpus,$error);
    if($validated===false)return false;
    $catalog=hrm_legacy_formula_source_catalog();
    if(array_keys($rows_by_scope)!==array_keys($catalog)){$error='safe_acceptance_scope_set_invalid';return false;}
    $accepted=array();$blocked=array();$summary=array();
    foreach($catalog as $scope=>$unused){
        if(!is_array($rows_by_scope[$scope])){$error='safe_acceptance_rows_invalid';return false;}
        $scopeSummary=array('row_count'=>0,'distinct_formula_count'=>0,'accepted_row_count'=>0,'accepted_distinct_count'=>0,'blocked_row_count'=>0,'blocked_distinct_count'=>0,'classification_set_sha256'=>'');
        $parts=array();$seen=array();
        foreach($rows_by_scope[$scope] as $row){
            if(!is_array($row)||!isset($row['formula_source'],$row['row_count'])||!is_string($row['formula_source'])||(int)$row['row_count']<1){$error='safe_acceptance_rows_invalid';return false;}
            $formula=trim($row['formula_source']);$count=(int)$row['row_count'];$hash=hash('sha256',$formula);
            if($formula===''||isset($seen[$hash])){$error='safe_acceptance_duplicate_or_empty_formula';return false;}$seen[$hash]=true;
            $diff=hrm_payroll_formula_safe_differential_classify($formula,$validated['migration_contexts']);
            $scopeSummary['row_count']+=$count;$scopeSummary['distinct_formula_count']++;
            $parts[]=$diff['formula_hash'].':'.$diff['classification'].':'.$diff['reason'].':'.$count;
            if(in_array($diff['classification'],array('exact','translated'),true)){
                $target=$diff['classification']==='translated'?hrm_legacy_formula_translate_for_facade($formula):hrm_legacy_formula_normalize($formula);
                if($target===false){$error='safe_acceptance_target_normalization_failed';return false;}
                $accepted[]=array('source_formula_sha256'=>$diff['formula_hash'],'target_formula_sha256'=>hash('sha256',$target),'source_scope'=>$scope,'classification'=>$diff['classification'],'differential_reason'=>$diff['reason'],'result_set_sha256'=>$diff['result_set_hash'],'row_count'=>$count);
                $scopeSummary['accepted_row_count']+=$count;$scopeSummary['accepted_distinct_count']++;
            }else{
                $blocked[]=array('source_formula_sha256'=>$diff['formula_hash'],'source_scope'=>$scope,'differential_reason'=>$diff['reason'],'row_count'=>$count);
                $scopeSummary['blocked_row_count']+=$count;$scopeSummary['blocked_distinct_count']++;
            }
        }
        sort($parts,SORT_STRING);$scopeSummary['classification_set_sha256']=hash('sha256',implode('|',$parts));$summary[$scope]=$scopeSummary;
    }
    usort($accepted,function($a,$b){return strcmp($a['source_scope'].'|'.$a['source_formula_sha256'],$b['source_scope'].'|'.$b['source_formula_sha256']);});
    usort($blocked,function($a,$b){return strcmp($a['source_scope'].'|'.$a['source_formula_sha256'],$b['source_scope'].'|'.$b['source_formula_sha256']);});
    $payload=array('sources'=>$summary,'accepted'=>$accepted,'blocked'=>$blocked);
    $hash=hrm_payroll_formula_safe_acceptance_hash($payload);if($hash===false){$error='safe_acceptance_inventory_hash_failed';return false;}
    $payload['inventory_sha256']=$hash;return $payload;
}

/** Build a redacted author-owned configured-company shadow acceptance report. */
function hrm_payroll_formula_safe_acceptance_build_report($company,$author_id,$author_os_identity,array $rows_by_scope,array $corpus,$generated_at=null,&$error=null)
{
    $error=null;
    if((int)$company<0||!hrm_payroll_formula_safe_token_is_valid($author_id,100)||!is_string($author_os_identity)||$author_os_identity===''||!hrm_payroll_formula_safe_acceptance_roles_separated((int)$company)){$error='safe_acceptance_identity_invalid';return false;}
    $inventory=hrm_payroll_formula_safe_acceptance_inventory($rows_by_scope,$corpus,$error);if($inventory===false)return false;
    $fp=hrm_payroll_formula_safe_acceptance_fingerprint($author_os_identity);$generated_at=$generated_at===null?gmdate('Y-m-d\TH:i:s\Z'):$generated_at;
    if($fp===false||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches((int)$company,'author',$fp)||!hrm_legacy_formula_review_is_utc_timestamp($generated_at)){$error='safe_acceptance_metadata_invalid';return false;}
    $bindings=hrm_payroll_formula_safe_acceptance_bindings();foreach($bindings as $v)if(!hrm_payroll_formula_safe_hash_is_valid($v)){$error='safe_acceptance_binding_invalid';return false;}
    $report=array('schema_version'=>1,'contract'=>array('id'=>HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_VERSION),'artifact_type'=>'configured_company_shadow_acceptance','data_policy'=>array('classification'=>'restricted-redacted-governance','contains_formula_text'=>false,'contains_payroll_context'=>false,'contains_production_values'=>false),'company_id'=>(int)$company,'source_version'=>HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_SOURCE_VERSION,'generated_at_utc'=>$generated_at,'generated_by_role'=>'payroll-formula-author','generated_by'=>$author_id,'generated_by_os_fingerprint'=>$fp,'bindings'=>$bindings,'inventory'=>$inventory,'acceptance_status'=>count($inventory['blocked'])===0?'ready_for_review':'blocked_unsupported');
    $hash=hrm_payroll_formula_safe_acceptance_hash($report);if($hash===false){$error='safe_acceptance_report_hash_failed';return false;}$report['report_sha256']=$hash;return $report;
}

/** Validate redacted inventory semantics independently of its self-hash. */
function hrm_payroll_formula_safe_acceptance_validate_inventory(array $inv,&$error=null)
{
    $error=null;$top=array('sources','accepted','blocked','inventory_sha256');
    if(!hrm_payroll_formula_safe_has_only_keys($inv,$top)){$error='safe_acceptance_report_inventory_invalid';return false;}
    foreach($top as $field)if(!array_key_exists($field,$inv)){$error='safe_acceptance_report_inventory_invalid';return false;}
    if(!is_array($inv['sources'])||!is_array($inv['accepted'])||!is_array($inv['blocked'])||!hrm_payroll_formula_safe_hash_is_valid($inv['inventory_sha256'])){$error='safe_acceptance_report_inventory_invalid';return false;}
    $catalog=hrm_legacy_formula_source_catalog();if(array_keys($inv['sources'])!==array_keys($catalog)){$error='safe_acceptance_source_summary_invalid';return false;}
    $summary=array();$parts=array();foreach($catalog as $scope=>$unused){$summary[$scope]=array('row_count'=>0,'distinct_formula_count'=>0,'accepted_row_count'=>0,'accepted_distinct_count'=>0,'blocked_row_count'=>0,'blocked_distinct_count'=>0,'classification_set_sha256'=>'');$parts[$scope]=array();}
    $seen=array();$last='';
    foreach($inv['accepted'] as $e){
        $allowed=array('source_formula_sha256','target_formula_sha256','source_scope','classification','differential_reason','result_set_sha256','row_count');
        if(!is_array($e)||!hrm_payroll_formula_safe_has_only_keys($e,$allowed)){ $error='safe_acceptance_entry_invalid';return false;}foreach($allowed as $f)if(!array_key_exists($f,$e)){ $error='safe_acceptance_entry_invalid';return false;}
        if(!hrm_payroll_formula_safe_hash_is_valid($e['source_formula_sha256'])||!hrm_payroll_formula_safe_hash_is_valid($e['target_formula_sha256'])||!hrm_payroll_formula_safe_hash_is_valid($e['result_set_sha256'])||!isset($catalog[$e['source_scope']])||!is_int($e['row_count'])||$e['row_count']<1){$error='safe_acceptance_entry_invalid';return false;}
        if(($e['classification']==='exact'&&$e['differential_reason']!=='equivalent_without_translation')||($e['classification']==='translated'&&$e['differential_reason']!=='decimal_literal_normalized')||!in_array($e['classification'],array('exact','translated'),true)){$error='safe_acceptance_entry_classification_invalid';return false;}
        $key=$e['source_scope'].'|'.$e['source_formula_sha256'];if(isset($seen[$key])||($last!==''&&strcmp($last,$key)>=0)){$error='safe_acceptance_inventory_order_or_duplicate_invalid';return false;}$seen[$key]=true;$last=$key;
        $scope=$e['source_scope'];$count=$e['row_count'];$summary[$scope]['row_count']+=$count;$summary[$scope]['distinct_formula_count']++;$summary[$scope]['accepted_row_count']+=$count;$summary[$scope]['accepted_distinct_count']++;$parts[$scope][]=$e['source_formula_sha256'].':'.$e['classification'].':'.$e['differential_reason'].':'.$count;
    }
    $last='';$blockedReasons=array('legacy_source_unsupported','legacy_execution_error','facade_compile_or_execution_error','semantic_mismatch','translation_unavailable');
    foreach($inv['blocked'] as $e){
        $allowed=array('source_formula_sha256','source_scope','differential_reason','row_count');
        if(!is_array($e)||!hrm_payroll_formula_safe_has_only_keys($e,$allowed)){ $error='safe_acceptance_blocked_entry_invalid';return false;}foreach($allowed as $f)if(!array_key_exists($f,$e)){ $error='safe_acceptance_blocked_entry_invalid';return false;}
        if(!hrm_payroll_formula_safe_hash_is_valid($e['source_formula_sha256'])||!isset($catalog[$e['source_scope']])||!in_array($e['differential_reason'],$blockedReasons,true)||!is_int($e['row_count'])||$e['row_count']<1){$error='safe_acceptance_blocked_entry_invalid';return false;}
        $key=$e['source_scope'].'|'.$e['source_formula_sha256'];if(isset($seen[$key])||($last!==''&&strcmp($last,$key)>=0)){$error='safe_acceptance_inventory_order_or_duplicate_invalid';return false;}$seen[$key]=true;$last=$key;
        $scope=$e['source_scope'];$count=$e['row_count'];$summary[$scope]['row_count']+=$count;$summary[$scope]['distinct_formula_count']++;$summary[$scope]['blocked_row_count']+=$count;$summary[$scope]['blocked_distinct_count']++;$parts[$scope][]=$e['source_formula_sha256'].':unsupported:'.$e['differential_reason'].':'.$count;
    }
    $sourceFields=array('row_count','distinct_formula_count','accepted_row_count','accepted_distinct_count','blocked_row_count','blocked_distinct_count','classification_set_sha256');
    foreach($catalog as $scope=>$unused){
        sort($parts[$scope],SORT_STRING);$summary[$scope]['classification_set_sha256']=hash('sha256',implode('|',$parts[$scope]));$actual=$inv['sources'][$scope];
        if(!is_array($actual)||!hrm_payroll_formula_safe_has_only_keys($actual,$sourceFields)){ $error='safe_acceptance_source_summary_invalid';return false;}foreach($sourceFields as $field)if(!array_key_exists($field,$actual)){$error='safe_acceptance_source_summary_invalid';return false;}
        foreach(array_slice($sourceFields,0,6) as $field)if(!is_int($actual[$field])||$actual[$field]<0){$error='safe_acceptance_source_summary_invalid';return false;}
        if(!hrm_payroll_formula_safe_hash_is_valid($actual['classification_set_sha256'])||$actual!==$summary[$scope]){$error='safe_acceptance_source_summary_mismatch';return false;}
    }
    $payload=$inv;unset($payload['inventory_sha256']);$hash=hrm_payroll_formula_safe_acceptance_hash($payload);if($hash===false||!hash_equals($inv['inventory_sha256'],$hash)){$error='safe_acceptance_inventory_hash_mismatch';return false;}
    return true;
}

/** Validate a configured-company shadow acceptance report and all immutable source bindings. */
function hrm_payroll_formula_safe_acceptance_validate_report(array $report,&$error=null)
{
    $error=null;$fields=array('schema_version','contract','artifact_type','data_policy','company_id','source_version','generated_at_utc','generated_by_role','generated_by','generated_by_os_fingerprint','bindings','inventory','acceptance_status','report_sha256');
    if(!hrm_payroll_formula_safe_has_only_keys($report,$fields)){$error='safe_acceptance_report_schema_invalid';return false;}foreach($fields as $f)if(!array_key_exists($f,$report)){$error='safe_acceptance_report_schema_invalid';return false;}
    if($report['schema_version']!==1||$report['contract']!==array('id'=>HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_VERSION)||$report['artifact_type']!=='configured_company_shadow_acceptance'||$report['source_version']!==HRM_PAYROLL_FORMULA_SAFE_ACCEPTANCE_SOURCE_VERSION||!is_int($report['company_id'])||$report['company_id']<0||$report['generated_by_role']!=='payroll-formula-author'||!hrm_payroll_formula_safe_token_is_valid($report['generated_by'],100)||!hrm_legacy_formula_review_is_utc_timestamp($report['generated_at_utc'])||!in_array($report['acceptance_status'],array('ready_for_review','blocked_unsupported'),true)){$error='safe_acceptance_report_metadata_invalid';return false;}
    if($report['data_policy']!==array('classification'=>'restricted-redacted-governance','contains_formula_text'=>false,'contains_payroll_context'=>false,'contains_production_values'=>false)||preg_match('/^sha256:[a-f0-9]{64}$/D',(string)$report['generated_by_os_fingerprint'])!==1){$error='safe_acceptance_report_data_policy_invalid';return false;}
    if(!hrm_payroll_formula_safe_acceptance_roles_separated($report['company_id'])||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'author',$report['generated_by_os_fingerprint'])){$error='safe_acceptance_role_binding_invalid';return false;}
    $bindings=hrm_payroll_formula_safe_acceptance_bindings();if($report['bindings']!==$bindings){$error='safe_acceptance_report_binding_mismatch';return false;}
    $inv=$report['inventory'];if(!is_array($inv)||!hrm_payroll_formula_safe_acceptance_validate_inventory($inv,$error))return false;
    if((($report['acceptance_status']==='ready_for_review') !== (count($inv['blocked'])===0))){$error='safe_acceptance_status_mismatch';return false;}
    $payload=$report;unset($payload['report_sha256']);$hash=hrm_payroll_formula_safe_acceptance_hash($payload);if($hash===false||!hash_equals($report['report_sha256'],$hash)){$error='safe_acceptance_report_hash_mismatch';return false;}return true;
}

/** Recompute the current redacted inventory and reject a stale configured-company report. */
function hrm_payroll_formula_safe_acceptance_report_is_current(array $report,array $rows_by_scope,array $corpus,&$error=null)
{
    if(!hrm_payroll_formula_safe_acceptance_validate_report($report,$error))return false;
    $current=hrm_payroll_formula_safe_acceptance_inventory($rows_by_scope,$corpus,$error);if($current===false)return false;
    if(!hash_equals($report['inventory']['inventory_sha256'],$current['inventory_sha256'])){$error='safe_acceptance_report_stale';return false;}return true;
}

/** Build an independent checker approval receipt for a current blocker-free acceptance report. */
function hrm_payroll_formula_safe_acceptance_build_review(array $report,$reviewer_id,$reviewer_os_identity,$evidence_id,$reviewed_at,$review_due_on,&$error=null)
{
    $error=null;if(!hrm_payroll_formula_safe_acceptance_validate_report($report,$error))return false;
    if($report['acceptance_status']!=='ready_for_review'){$error='safe_acceptance_blocked';return false;}
    $rfp=hrm_payroll_formula_safe_acceptance_fingerprint($reviewer_os_identity);
    if(!hrm_payroll_formula_safe_token_is_valid($reviewer_id,100)||!hrm_payroll_formula_safe_token_is_valid($evidence_id,120)||$rfp===false||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'reviewer',$rfp)||!hrm_legacy_formula_review_is_utc_timestamp($reviewed_at)||!hrm_payroll_formula_safe_acceptance_utc_not_before($reviewed_at,$report['generated_at_utc'])||!hrm_payroll_formula_safe_iso_date($review_due_on)||substr($reviewed_at,0,10)>$review_due_on||hash_equals($rfp,$report['generated_by_os_fingerprint'])||hash_equals((string)$reviewer_id,(string)$report['generated_by'])){$error='safe_review_metadata_or_separation_invalid';return false;}
    $receipt=array('schema_version'=>1,'contract'=>array('id'=>HRM_PAYROLL_FORMULA_SAFE_REVIEW_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_REVIEW_VERSION),'artifact_type'=>'configured_company_shadow_acceptance_review','company_id'=>$report['company_id'],'report_sha256'=>$report['report_sha256'],'inventory_sha256'=>$report['inventory']['inventory_sha256'],'decision'=>'approved_for_shadow_publication','reviewer_id'=>$reviewer_id,'reviewer_os_fingerprint'=>$rfp,'evidence_id'=>$evidence_id,'reviewed_at_utc'=>$reviewed_at,'review_due_on'=>$review_due_on);
    $hash=hrm_payroll_formula_safe_acceptance_hash($receipt);if($hash===false){$error='safe_review_hash_failed';return false;}$receipt['receipt_sha256']=$hash;return $receipt;
}

/** Validate an independent acceptance-review receipt against its exact author report. */
function hrm_payroll_formula_safe_acceptance_validate_review(array $receipt,array $report,&$error=null)
{
    $error=null;if(!hrm_payroll_formula_safe_acceptance_validate_report($report,$error))return false;
    if($report['acceptance_status']!=='ready_for_review'||count($report['inventory']['blocked'])!==0){$error='safe_acceptance_blocked';return false;}
    $fields=array('schema_version','contract','artifact_type','company_id','report_sha256','inventory_sha256','decision','reviewer_id','reviewer_os_fingerprint','evidence_id','reviewed_at_utc','review_due_on','receipt_sha256');if(!hrm_payroll_formula_safe_has_only_keys($receipt,$fields)){$error='safe_review_schema_invalid';return false;}foreach($fields as $f)if(!array_key_exists($f,$receipt)){$error='safe_review_schema_invalid';return false;}
    if($receipt['schema_version']!==1||$receipt['contract']!==array('id'=>HRM_PAYROLL_FORMULA_SAFE_REVIEW_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_REVIEW_VERSION)||$receipt['artifact_type']!=='configured_company_shadow_acceptance_review'||$receipt['company_id']!==$report['company_id']||!hash_equals($receipt['report_sha256'],$report['report_sha256'])||!hash_equals($receipt['inventory_sha256'],$report['inventory']['inventory_sha256'])||$receipt['decision']!=='approved_for_shadow_publication'||!hrm_payroll_formula_safe_token_is_valid($receipt['reviewer_id'],100)||!hrm_payroll_formula_safe_token_is_valid($receipt['evidence_id'],120)||preg_match('/^sha256:[a-f0-9]{64}$/D',(string)$receipt['reviewer_os_fingerprint'])!==1||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'reviewer',$receipt['reviewer_os_fingerprint'])||!hrm_legacy_formula_review_is_utc_timestamp($receipt['reviewed_at_utc'])||!hrm_payroll_formula_safe_acceptance_utc_not_before($receipt['reviewed_at_utc'],$report['generated_at_utc'])||!hrm_payroll_formula_safe_iso_date($receipt['review_due_on'])||substr($receipt['reviewed_at_utc'],0,10)>$receipt['review_due_on']||hash_equals($receipt['reviewer_os_fingerprint'],$report['generated_by_os_fingerprint'])||hash_equals((string)$receipt['reviewer_id'],(string)$report['generated_by'])){$error='safe_review_metadata_invalid';return false;}
    $payload=$receipt;unset($payload['receipt_sha256']);$hash=hrm_payroll_formula_safe_acceptance_hash($payload);if($hash===false||!hash_equals($receipt['receipt_sha256'],$hash)){$error='safe_review_hash_mismatch';return false;}return true;
}

/** Build a private non-production shadow registry with four-principal separation. */
function hrm_payroll_formula_safe_acceptance_build_registry(array $report,array $review,$publisher_id,$publisher_os_identity,$executor_id,$executor_os_identity,$publication_evidence_id,$published_at,&$error=null)
{
    $error=null;if(!hrm_payroll_formula_safe_acceptance_validate_review($review,$report,$error))return false;
    $pfp=hrm_payroll_formula_safe_acceptance_fingerprint($publisher_os_identity);$efp=hrm_payroll_formula_safe_acceptance_fingerprint($executor_os_identity);
    $ids=array($report['generated_by'],$review['reviewer_id'],$publisher_id,$executor_id);$fps=array($report['generated_by_os_fingerprint'],$review['reviewer_os_fingerprint'],$pfp,$efp);
    if(!hrm_payroll_formula_safe_token_is_valid($publisher_id,100)||!hrm_payroll_formula_safe_token_is_valid($executor_id,100)||!hrm_payroll_formula_safe_token_is_valid($publication_evidence_id,120)||$pfp===false||$efp===false||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'publisher',$pfp)||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'shadow_executor',$efp)||count(array_unique($ids,SORT_STRING))!==4||count(array_unique($fps,SORT_STRING))!==4||!hrm_legacy_formula_review_is_utc_timestamp($published_at)||!hrm_payroll_formula_safe_acceptance_utc_not_before($published_at,$review['reviewed_at_utc'])||substr($published_at,0,10)>$review['review_due_on']){$error='safe_publication_separation_or_metadata_invalid';return false;}
    $bindings=$report['bindings'];$entries=array();foreach($report['inventory']['accepted'] as $candidate){$entries[]=array('source_formula_sha256'=>$candidate['source_formula_sha256'],'target_formula_sha256'=>$candidate['target_formula_sha256'],'source_scope'=>$candidate['source_scope'],'classification'=>$candidate['classification'],'differential_reason'=>$candidate['differential_reason'],'result_set_sha256'=>$candidate['result_set_sha256'],'status'=>'approved_for_shadow','author_id'=>$report['generated_by'],'reviewer_id'=>$review['reviewer_id'],'publisher_id'=>$publisher_id,'shadow_executor_id'=>$executor_id,'approved_on'=>substr($published_at,0,10),'review_due_on'=>$review['review_due_on'],'evidence_id'=>$publication_evidence_id);}
    $registry=array('schema_version'=>1,'contract'=>array('id'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_VERSION),'governance'=>array('environment'=>'non-production-shadow-only','default_deny'=>true,'required_status'=>'approved_for_shadow','author_reviewer_publisher_distinct'=>true,'shadow_executor_distinct'=>true,'production_execution_forbidden'=>true,'formula_text_forbidden'=>true,'payroll_context_forbidden'=>true),'safe_profile_sha256'=>$bindings['safe_profile_sha256'],'pay_rule_001_target_profile_sha256'=>$bindings['pay_rule_001_target_profile_sha256'],'differential_corpus_sha256'=>$bindings['differential_corpus_sha256'],'exception_register_sha256'=>$bindings['exception_register_sha256'],'entries'=>$entries);
    $idx=hrm_payroll_formula_safe_validate_publication_registry($registry,hrm_payroll_formula_safe_publication_bindings(),$error);if($idx===false)return false;
    if(!hrm_payroll_formula_safe_acceptance_validate_registry_bindings($registry,$report,$review,$publisher_id,$executor_id,$publication_evidence_id,$published_at,$error))return false;
    return $registry;
}

/** Bind every private registry entry to the exact author/reviewer/publication inputs. */
function hrm_payroll_formula_safe_acceptance_validate_registry_bindings(array $registry,array $report,array $review,$publisher_id,$executor_id,$evidence_id,$published_at,&$error=null)
{
    $error=null;
    if(!hrm_payroll_formula_safe_acceptance_validate_review($review,$report,$error))return false;
    $index=hrm_payroll_formula_safe_validate_publication_registry($registry,hrm_payroll_formula_safe_publication_bindings(),$error);if($index===false)return false;
    if(!hrm_payroll_formula_safe_token_is_valid($publisher_id,100)||!hrm_payroll_formula_safe_token_is_valid($executor_id,100)||!hrm_payroll_formula_safe_token_is_valid($evidence_id,120)||!hrm_legacy_formula_review_is_utc_timestamp($published_at)||!hrm_payroll_formula_safe_acceptance_utc_not_before($published_at,$review['reviewed_at_utc'])||substr($published_at,0,10)>$review['review_due_on']||count($registry['entries'])!==count($report['inventory']['accepted'])){$error='safe_publication_registry_binding_mismatch';return false;}
    foreach($report['inventory']['accepted'] as $candidate){
        $hash=$candidate['source_formula_sha256'];$scope=$candidate['source_scope'];
        if(!isset($index[$hash][$scope])){$error='safe_publication_registry_binding_mismatch';return false;}
        $entry=$index[$hash][$scope];
        foreach(array('source_formula_sha256','target_formula_sha256','source_scope','classification','differential_reason','result_set_sha256') as $field){
            if(!hash_equals((string)$candidate[$field],(string)$entry[$field])){$error='safe_publication_registry_binding_mismatch';return false;}
        }
        if(!hash_equals((string)$entry['author_id'],(string)$report['generated_by'])||!hash_equals((string)$entry['reviewer_id'],(string)$review['reviewer_id'])||!hash_equals((string)$entry['publisher_id'],(string)$publisher_id)||!hash_equals((string)$entry['shadow_executor_id'],(string)$executor_id)||!hash_equals((string)$entry['evidence_id'],(string)$evidence_id)||$entry['approved_on']!==substr($published_at,0,10)||$entry['review_due_on']!==$review['review_due_on']){$error='safe_publication_registry_binding_mismatch';return false;}
    }
    return true;
}

/** Build an immutable publisher receipt binding report, review, registry payload, and private registry bytes. */
function hrm_payroll_formula_safe_acceptance_build_publication_receipt(array $report,array $review,array $registry,$registry_file_sha256,$publisher_id,$publisher_os_identity,$executor_id,$executor_os_identity,$evidence_id,$published_at,&$error=null)
{
    $error=null;$idx=hrm_payroll_formula_safe_validate_publication_registry($registry,hrm_payroll_formula_safe_publication_bindings(),$error);if($idx===false)return false;
    $pfp=hrm_payroll_formula_safe_acceptance_fingerprint($publisher_os_identity);$efp=hrm_payroll_formula_safe_acceptance_fingerprint($executor_os_identity);$payloadHash=hrm_payroll_formula_safe_acceptance_hash($registry);
    if(!hrm_payroll_formula_safe_acceptance_validate_review($review,$report,$error)||!hrm_payroll_formula_safe_hash_is_valid($registry_file_sha256)||!hrm_payroll_formula_safe_hash_is_valid($payloadHash)||!hrm_payroll_formula_safe_token_is_valid($publisher_id,100)||!hrm_payroll_formula_safe_token_is_valid($executor_id,100)||!hrm_payroll_formula_safe_token_is_valid($evidence_id,120)||$pfp===false||$efp===false||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'publisher',$pfp)||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'shadow_executor',$efp)||!hrm_legacy_formula_review_is_utc_timestamp($published_at)||!hrm_payroll_formula_safe_acceptance_utc_not_before($published_at,$review['reviewed_at_utc'])||substr($published_at,0,10)>$review['review_due_on']){$error='safe_publication_receipt_metadata_invalid';return false;}
    if(!hrm_payroll_formula_safe_acceptance_validate_registry_bindings($registry,$report,$review,$publisher_id,$executor_id,$evidence_id,$published_at,$error))return false;
    $receipt=array('schema_version'=>1,'contract'=>array('id'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_VERSION),'artifact_type'=>'immutable_nonproduction_shadow_publication_receipt','company_id'=>$report['company_id'],'report_sha256'=>$report['report_sha256'],'review_receipt_sha256'=>$review['receipt_sha256'],'registry_payload_sha256'=>$payloadHash,'registry_file_sha256'=>$registry_file_sha256,'entry_count'=>count($registry['entries']),'publisher_id'=>$publisher_id,'publisher_os_fingerprint'=>$pfp,'shadow_executor_id'=>$executor_id,'shadow_executor_os_fingerprint'=>$efp,'evidence_id'=>$evidence_id,'published_at_utc'=>$published_at,'recovery'=>HRM_PAYROLL_FORMULA_SAFE_RECOVERY_POLICY);
    $hash=hrm_payroll_formula_safe_acceptance_hash($receipt);if($hash===false){$error='safe_publication_receipt_hash_failed';return false;}$receipt['receipt_sha256']=$hash;return $receipt;
}

/** Validate a publisher receipt and four-principal separation against exact immutable inputs. */
function hrm_payroll_formula_safe_acceptance_validate_publication_receipt(array $receipt,array $report,array $review,array $registry,$registry_file_sha256,&$error=null)
{
    $error=null;$fields=array('schema_version','contract','artifact_type','company_id','report_sha256','review_receipt_sha256','registry_payload_sha256','registry_file_sha256','entry_count','publisher_id','publisher_os_fingerprint','shadow_executor_id','shadow_executor_os_fingerprint','evidence_id','published_at_utc','recovery','receipt_sha256');if(!hrm_payroll_formula_safe_has_only_keys($receipt,$fields)){$error='safe_publication_receipt_schema_invalid';return false;}foreach($fields as $f)if(!array_key_exists($f,$receipt)){$error='safe_publication_receipt_schema_invalid';return false;}
    if(!hrm_payroll_formula_safe_acceptance_validate_review($review,$report,$error))return false;$idx=hrm_payroll_formula_safe_validate_publication_registry($registry,hrm_payroll_formula_safe_publication_bindings(),$error);if($idx===false)return false;
    $payloadHash=hrm_payroll_formula_safe_acceptance_hash($registry);if($receipt['schema_version']!==1||$receipt['contract']!==array('id'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_RECEIPT_VERSION)||$receipt['artifact_type']!=='immutable_nonproduction_shadow_publication_receipt'||$receipt['company_id']!==$report['company_id']||!hash_equals($receipt['report_sha256'],$report['report_sha256'])||!hash_equals($receipt['review_receipt_sha256'],$review['receipt_sha256'])||!hash_equals($receipt['registry_payload_sha256'],$payloadHash)||!hash_equals($receipt['registry_file_sha256'],$registry_file_sha256)||$receipt['entry_count']!==count($registry['entries'])||!hrm_payroll_formula_safe_token_is_valid($receipt['publisher_id'],100)||!hrm_payroll_formula_safe_token_is_valid($receipt['shadow_executor_id'],100)||!hrm_payroll_formula_safe_token_is_valid($receipt['evidence_id'],120)||preg_match('/^sha256:[a-f0-9]{64}$/D',(string)$receipt['publisher_os_fingerprint'])!==1||preg_match('/^sha256:[a-f0-9]{64}$/D',(string)$receipt['shadow_executor_os_fingerprint'])!==1||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'publisher',$receipt['publisher_os_fingerprint'])||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'shadow_executor',$receipt['shadow_executor_os_fingerprint'])||!hrm_legacy_formula_review_is_utc_timestamp($receipt['published_at_utc'])||!hrm_payroll_formula_safe_acceptance_utc_not_before($receipt['published_at_utc'],$review['reviewed_at_utc'])||substr($receipt['published_at_utc'],0,10)>$review['review_due_on']||$receipt['recovery']!==HRM_PAYROLL_FORMULA_SAFE_RECOVERY_POLICY){$error='safe_publication_receipt_metadata_invalid';return false;}
    $ids=array($report['generated_by'],$review['reviewer_id'],$receipt['publisher_id'],$receipt['shadow_executor_id']);$fps=array($report['generated_by_os_fingerprint'],$review['reviewer_os_fingerprint'],$receipt['publisher_os_fingerprint'],$receipt['shadow_executor_os_fingerprint']);if(count(array_unique($ids,SORT_STRING))!==4||count(array_unique($fps,SORT_STRING))!==4){$error='safe_publication_receipt_separation_invalid';return false;}
    if(!hrm_payroll_formula_safe_acceptance_validate_registry_bindings($registry,$report,$review,$receipt['publisher_id'],$receipt['shadow_executor_id'],$receipt['evidence_id'],$receipt['published_at_utc'],$error))return false;
    $payload=$receipt;unset($payload['receipt_sha256']);$hash=hrm_payroll_formula_safe_acceptance_hash($payload);if($hash===false||!hash_equals($receipt['receipt_sha256'],$hash)){$error='safe_publication_receipt_hash_mismatch';return false;}return true;
}

/** Build a formula lookup map from the current in-memory configured-company rows. */
function hrm_payroll_formula_safe_acceptance_formula_map(array $rows_by_scope, &$error=null)
{
    $error=null;
    $catalog=hrm_legacy_formula_source_catalog();
    if(array_keys($rows_by_scope)!==array_keys($catalog)){$error='safe_execution_scope_set_invalid';return false;}
    $map=array();
    foreach($catalog as $scope=>$unused){
        if(!is_array($rows_by_scope[$scope])){$error='safe_execution_rows_invalid';return false;}
        $map[$scope]=array();
        foreach($rows_by_scope[$scope] as $row){
            if(!is_array($row)||!isset($row['formula_source'],$row['row_count'])||!is_string($row['formula_source'])||(int)$row['row_count']<1){$error='safe_execution_rows_invalid';return false;}
            $formula=hrm_legacy_formula_normalize($row['formula_source']);
            $hash=hrm_legacy_formula_fingerprint($formula);
            if($formula===''||isset($map[$scope][$hash])){$error='safe_execution_formula_map_invalid';return false;}
            $map[$scope][$hash]=$formula;
        }
    }
    return $map;
}

/**
 * Execute every approved distinct configured-company formula against the fixed
 * synthetic typed context under the separately authenticated shadow executor.
 * Formula text, payroll context, and numeric results are never returned in the
 * aggregate receipt.
 */
function hrm_payroll_formula_safe_acceptance_execute(
    array $report,
    array $review,
    array $registry,
    array $publication_receipt,
    $registry_file_sha256,
    $executor_id,
    $executor_os_identity,
    array $rows_by_scope,
    array $corpus,
    $executed_at,
    &$receipt=null,
    &$error=null
) {
    $receipt=null;$error=null;
    if(!hrm_payroll_formula_safe_acceptance_validate_publication_receipt($publication_receipt,$report,$review,$registry,$registry_file_sha256,$error))return false;
    if(!hrm_payroll_formula_safe_acceptance_report_is_current($report,$rows_by_scope,$corpus,$error))return false;
    $executor_fp=hrm_payroll_formula_safe_acceptance_fingerprint($executor_os_identity);
    if(!hrm_payroll_formula_safe_token_is_valid($executor_id,100)||$executor_fp===false
        ||!hrm_payroll_formula_safe_acceptance_role_fingerprint_matches($report['company_id'],'shadow_executor',$executor_fp)
        ||!hash_equals((string)$publication_receipt['shadow_executor_id'],(string)$executor_id)
        ||!hash_equals((string)$publication_receipt['shadow_executor_os_fingerprint'],(string)$executor_fp)
        ||!hrm_legacy_formula_review_is_utc_timestamp($executed_at)
        ||!hrm_payroll_formula_safe_acceptance_utc_not_before($executed_at,$publication_receipt['published_at_utc'])
        ||substr($executed_at,0,10)>$review['review_due_on']){
        $error='safe_execution_identity_or_metadata_invalid';return false;
    }
    $map=hrm_payroll_formula_safe_acceptance_formula_map($rows_by_scope,$error);if($map===false)return false;
    $context=array(
        'BASIC'=>'8000','GROSS'=>'9000','DAYS_WORKED'=>'20','PAYABLE_DAYS'=>'20',
        'WORKING_DAYS'=>'22','DAYS_IN_MONTH'=>'30','LEAVE_DAYS'=>'2','PAID_LEAVE_DAYS'=>'1',
        'ABSENT_DAYS'=>'0','OVERTIME_HOURS'=>'5.5','UNPAID_LEAVE_DAYS'=>'1','HOURLY_RATE'=>'50',
        'PERIOD_START'=>'2026-09-01','PERIOD_END'=>'2026-09-30'
    );
    $parts=array();$execution_items=array();$count=0;$as_of=substr($executed_at,0,10);
    foreach($report['inventory']['accepted'] as $candidate){
        $scope=$candidate['source_scope'];$hash=$candidate['source_formula_sha256'];
        if(!isset($map[$scope][$hash])){$error='safe_execution_formula_source_stale';return false;}
        $shadow_receipt=null;$shadow_error=null;
        $value=hrm_payroll_formula_safe_shadow_evaluate($map[$scope][$hash],$scope,$context,$executor_id,$registry,$corpus,$shadow_receipt,$shadow_error,$as_of);
        if($value===false||!is_array($shadow_receipt)){$error=$shadow_error===null?'safe_execution_shadow_failed':$shadow_error;return false;}
        $shadow_hash=hrm_payroll_formula_safe_acceptance_hash($shadow_receipt);if($shadow_hash===false){$error='safe_execution_shadow_receipt_hash_failed';return false;}
        $parts[]=$scope.':'.$hash.':'.$shadow_hash;$execution_items[]=array('source_scope'=>$scope,'source_formula_sha256'=>$hash,'shadow_receipt'=>$shadow_receipt,'shadow_receipt_sha256'=>$shadow_hash);$count++;
    }
    sort($parts,SORT_STRING);usort($execution_items,function($a,$b){return strcmp($a['source_scope'].'|'.$a['source_formula_sha256'],$b['source_scope'].'|'.$b['source_formula_sha256']);});
    $registry_payload_hash=hrm_payroll_formula_safe_acceptance_hash($registry);
    $receipt=array(
        'schema_version'=>1,
        'contract'=>array('id'=>HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_VERSION),
        'artifact_type'=>'configured_company_nonproduction_shadow_execution_receipt',
        'data_policy'=>array('classification'=>'restricted-redacted-governance','contains_formula_text'=>false,'contains_payroll_context'=>false,'contains_payroll_result'=>false),
        'company_id'=>$report['company_id'],
        'report_sha256'=>$report['report_sha256'],
        'review_receipt_sha256'=>$review['receipt_sha256'],
        'publication_receipt_sha256'=>$publication_receipt['receipt_sha256'],
        'registry_payload_sha256'=>$registry_payload_hash,
        'registry_file_sha256'=>$registry_file_sha256,
        'inventory_sha256'=>$report['inventory']['inventory_sha256'],
        'executor_id'=>$executor_id,
        'executor_os_fingerprint'=>$executor_fp,
        'executed_at_utc'=>$executed_at,
        'executed_distinct_formula_count'=>$count,
        'execution_items'=>$execution_items,
        'execution_set_sha256'=>hash('sha256',implode('|',$parts)),
        'production_execution_authorized'=>false
    );
    $rh=hrm_payroll_formula_safe_acceptance_hash($receipt);if($rh===false){$error='safe_execution_receipt_hash_failed';$receipt=null;return false;}
    $receipt['receipt_sha256']=$rh;return true;
}

/** Validate an immutable redacted shadow execution receipt against its exact publication custody. */
function hrm_payroll_formula_safe_acceptance_validate_execution_receipt(
    array $receipt,
    array $report,
    array $review,
    array $registry,
    array $publication_receipt,
    $registry_file_sha256,
    &$error=null
) {
    $error=null;
    if(!hrm_payroll_formula_safe_acceptance_validate_publication_receipt($publication_receipt,$report,$review,$registry,$registry_file_sha256,$error))return false;
    $fields=array('schema_version','contract','artifact_type','data_policy','company_id','report_sha256','review_receipt_sha256','publication_receipt_sha256','registry_payload_sha256','registry_file_sha256','inventory_sha256','executor_id','executor_os_fingerprint','executed_at_utc','executed_distinct_formula_count','execution_items','execution_set_sha256','production_execution_authorized','receipt_sha256');
    if(!hrm_payroll_formula_safe_has_only_keys($receipt,$fields)){$error='safe_execution_receipt_schema_invalid';return false;}foreach($fields as $f)if(!array_key_exists($f,$receipt)){$error='safe_execution_receipt_schema_invalid';return false;}
    $registry_payload_hash=hrm_payroll_formula_safe_acceptance_hash($registry);
    if($receipt['schema_version']!==1||$receipt['contract']!==array('id'=>HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_EXECUTION_RECEIPT_VERSION)
        ||$receipt['artifact_type']!=='configured_company_nonproduction_shadow_execution_receipt'
        ||$receipt['data_policy']!==array('classification'=>'restricted-redacted-governance','contains_formula_text'=>false,'contains_payroll_context'=>false,'contains_payroll_result'=>false)
        ||$receipt['company_id']!==$report['company_id']||!hash_equals($receipt['report_sha256'],$report['report_sha256'])
        ||!hash_equals($receipt['review_receipt_sha256'],$review['receipt_sha256'])||!hash_equals($receipt['publication_receipt_sha256'],$publication_receipt['receipt_sha256'])
        ||!hash_equals($receipt['registry_payload_sha256'],$registry_payload_hash)||!hash_equals($receipt['registry_file_sha256'],$registry_file_sha256)
        ||!hash_equals($receipt['inventory_sha256'],$report['inventory']['inventory_sha256'])||!hash_equals((string)$receipt['executor_id'],(string)$publication_receipt['shadow_executor_id'])
        ||!hash_equals((string)$receipt['executor_os_fingerprint'],(string)$publication_receipt['shadow_executor_os_fingerprint'])
        ||!hrm_legacy_formula_review_is_utc_timestamp($receipt['executed_at_utc'])||!hrm_payroll_formula_safe_acceptance_utc_not_before($receipt['executed_at_utc'],$publication_receipt['published_at_utc'])||substr($receipt['executed_at_utc'],0,10)>$review['review_due_on']||!is_int($receipt['executed_distinct_formula_count'])
        ||$receipt['executed_distinct_formula_count']!==count($report['inventory']['accepted'])||!is_array($receipt['execution_items'])||!hrm_payroll_formula_safe_hash_is_valid($receipt['execution_set_sha256'])
        ||$receipt['production_execution_authorized']!==false||!hrm_payroll_formula_safe_hash_is_valid($receipt['receipt_sha256'])){
        $error='safe_execution_receipt_metadata_invalid';return false;
    }
    $expected=array();foreach($report['inventory']['accepted'] as $candidate)$expected[$candidate['source_scope'].'|'.$candidate['source_formula_sha256']]=true;
    $seen=array();$parts=array();$last='';foreach($receipt['execution_items'] as $item){
        $itemFields=array('source_scope','source_formula_sha256','shadow_receipt','shadow_receipt_sha256');if(!is_array($item)||!hrm_payroll_formula_safe_has_only_keys($item,$itemFields)){ $error='safe_execution_item_invalid';return false;}foreach($itemFields as $field)if(!array_key_exists($field,$item)){ $error='safe_execution_item_invalid';return false;}
        if(!isset(hrm_legacy_formula_source_catalog()[$item['source_scope']])||!hrm_payroll_formula_safe_hash_is_valid($item['source_formula_sha256'])||!is_array($item['shadow_receipt'])||!hrm_payroll_formula_safe_hash_is_valid($item['shadow_receipt_sha256'])){$error='safe_execution_item_invalid';return false;}
        $key=$item['source_scope'].'|'.$item['source_formula_sha256'];if(!isset($expected[$key])||isset($seen[$key])||($last!==''&&strcmp($last,$key)>=0)){$error='safe_execution_item_set_mismatch';return false;}
        $shadowFields=array('contract','mode','source_scope','source_formula_sha256','target_formula_sha256','classification','differential_reason','result_set_sha256','safe_profile_sha256','evidence_id','executor_fingerprint','executed_on','result_type','result_unit');$shadow=$item['shadow_receipt'];
        if(!hrm_payroll_formula_safe_has_only_keys($shadow,$shadowFields)){ $error='safe_execution_shadow_receipt_invalid';return false;}foreach($shadowFields as $field)if(!array_key_exists($field,$shadow)){ $error='safe_execution_shadow_receipt_invalid';return false;}
        $candidate=null;foreach($report['inventory']['accepted'] as $accepted)if($accepted['source_scope']===$item['source_scope']&&hash_equals($accepted['source_formula_sha256'],$item['source_formula_sha256'])){$candidate=$accepted;break;}
        $registryIndex=hrm_payroll_formula_safe_validate_publication_registry($registry,hrm_payroll_formula_safe_publication_bindings(),$registryError);if($registryIndex===false){$error=$registryError;return false;}$entry=$registryIndex[$item['source_formula_sha256']][$item['source_scope']];$bindings=hrm_payroll_formula_safe_publication_bindings();
        if($candidate===null||$shadow['contract']!==array('id'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_VERSION)||$shadow['mode']!=='non-production-shadow'||$shadow['source_scope']!==$item['source_scope']||!hash_equals($shadow['source_formula_sha256'],$item['source_formula_sha256'])||!hash_equals($shadow['target_formula_sha256'],$candidate['target_formula_sha256'])||$shadow['classification']!==$candidate['classification']||$shadow['differential_reason']!==$candidate['differential_reason']||!hash_equals($shadow['result_set_sha256'],$candidate['result_set_sha256'])||!hash_equals($shadow['safe_profile_sha256'],$bindings['safe_profile_sha256'])||!hash_equals((string)$shadow['evidence_id'],(string)$entry['evidence_id'])||!hash_equals($shadow['executor_fingerprint'],hash('sha256','pay-rule-002-shadow:'.$receipt['executor_id']))||$shadow['executed_on']!==substr($receipt['executed_at_utc'],0,10)||!in_array($shadow['result_type'],array('decimal','integer','date'),true)||!in_array($shadow['result_unit'],array('currency','scalar','day','hour'),true)){$error='safe_execution_shadow_receipt_binding_mismatch';return false;}
        $shadowHash=hrm_payroll_formula_safe_acceptance_hash($shadow);if($shadowHash===false||!hash_equals($item['shadow_receipt_sha256'],$shadowHash)){$error='safe_execution_shadow_receipt_hash_mismatch';return false;}
        $seen[$key]=true;$last=$key;$parts[]=$item['source_scope'].':'.$item['source_formula_sha256'].':'.$item['shadow_receipt_sha256'];
    }
    if(count($seen)!==count($expected)||count($receipt['execution_items'])!==$receipt['executed_distinct_formula_count']){$error='safe_execution_item_set_mismatch';return false;}
    sort($parts,SORT_STRING);$setHash=hash('sha256',implode('|',$parts));if(!hash_equals($receipt['execution_set_sha256'],$setHash)){$error='safe_execution_set_hash_mismatch';return false;}
    $payload=$receipt;unset($payload['receipt_sha256']);$hash=hrm_payroll_formula_safe_acceptance_hash($payload);if($hash===false||!hash_equals($receipt['receipt_sha256'],$hash)){$error='safe_execution_receipt_hash_mismatch';return false;}
    return true;
}

