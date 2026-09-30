<?php
/**
 * PAY-AI-001 deterministic assistive runtime custody and bounded HRM adapter.
 *
 * This file permits INSERT-only request/output/review evidence for the local
 * deterministic adapter. It does not expose provider/network execution and it
 * never stores raw prompts, generated prose, employee identity, bank data or
 * tax data.
 */
include_once($path_to_root.'/includes/pay_ai_001_contract.inc');
include_once($path_to_root.'/includes/pay_ai_001_grounding_contract.inc');
include_once($path_to_root.'/includes/pay_ai_001_baseline_contract.inc');
include_once($path_to_root.'/includes/pay_ai_001_operational_contract.inc');
include_once($path_to_root.'/includes/pay_ai_001_runtime_adapter.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_anl_001_headcount_db.inc');

/** @return false */
function pay_ai_001_runtime_fail(&$error, $code)
{
    $error = $code;
    return false;
}

/** @return array|false */
function pay_ai_001_runtime_one($sql)
{
    $result = db_query($sql);
    if (!$result || db_num_rows($result) !== 1) return false;
    return db_fetch_assoc($result);
}

/** @return array|null|false */
function pay_ai_001_runtime_optional_one($sql)
{
    $result = db_query($sql);
    if (!$result) return false;
    $count = db_num_rows($result);
    if ($count === 0) return null;
    if ($count !== 1) return false;
    return db_fetch_assoc($result);
}

/** @return int */
function pay_ai_001_runtime_company_id()
{
    return function_exists('hrm_anl_001_runtime_company_id') ? hrm_anl_001_runtime_company_id() : -1;
}

/** @return int */
function pay_ai_001_runtime_actor_id()
{
    return function_exists('hrm_anl_001_runtime_actor_id') ? hrm_anl_001_runtime_actor_id() : 0;
}

/** @return string|false */
function pay_ai_001_runtime_database_version()
{
    return function_exists('hrm_anl_001_runtime_database_version') ? hrm_anl_001_runtime_database_version() : false;
}

/** @return bool */
function pay_ai_001_runtime_has_access()
{
    return function_exists('user_check_access') && user_check_access('SA_HRMREPORTS');
}

/** @return bool */
function pay_ai_001_runtime_custody_ready(&$error)
{
    $version = pay_ai_001_runtime_database_version();
    if (!is_string($version) || !pay_ai_001_review_workflow_required_for_database_version($version))
        return pay_ai_001_runtime_fail($error, 'ai_review_workflow_not_active');
    if (!pay_ai_001_operational_runtime_allowed_for_database_version($version))
        return pay_ai_001_runtime_fail($error, pay_ai_001_company_opted_out() ? 'ai_company_opted_out' : 'ai_runtime_globally_disabled');
    if (!pay_ai_001_runtime_has_access())
        return pay_ai_001_runtime_fail($error, 'permission_denied');
    if (pay_ai_001_runtime_company_id() < 0 || pay_ai_001_runtime_actor_id() <= 0)
        return pay_ai_001_runtime_fail($error, 'authenticated_scope_required');
    if (pay_ai_001_external_provider_execution_allowed())
        return pay_ai_001_runtime_fail($error, 'external_provider_boundary_invalid');
    return true;
}

/** @param array $basis @return string|false */
function pay_ai_001_runtime_hash(array $basis)
{
    return pay_ai_001_grounding_hash($basis);
}

/**
 * Persist immutable request/output hashes for one deterministic result.
 *
 * @param string $capability
 * @param array $fact
 * @param array $result
 * @param string|null $error
 * @return array|false
 */
function pay_ai_001_record_deterministic_output($capability, array $fact, array $result, &$error = null)
{
    $error = null;
    if (!pay_ai_001_runtime_custody_ready($error)) return false;
    if (!in_array((string)$capability, pay_ai_001_supported_capabilities(), true)
        || !isset($result['generated_content_sha256'],$result['grounding_refs_sha256'],$result['baseline_sha256'])
        || !preg_match('/^[a-f0-9]{64}$/D', (string)$result['generated_content_sha256'])
        || !preg_match('/^[a-f0-9]{64}$/D', (string)$result['grounding_refs_sha256'])
        || !preg_match('/^[a-f0-9]{64}$/D', (string)$result['baseline_sha256']))
        return pay_ai_001_runtime_fail($error, 'ai_output_evidence_invalid');

    $company = pay_ai_001_runtime_company_id();
    $actor = pay_ai_001_runtime_actor_id();
    $metric_key = isset($fact['metric_key']) ? (string)$fact['metric_key'] : '';
    $object_scope_sha256 = pay_ai_001_runtime_hash(array('company_id'=>$company,'metric_key'=>$metric_key));
    $field_scope_sha256 = pay_ai_001_runtime_hash(array('fields'=>array('metric_key','current_value','baseline_value','status_code')));
    $minimization_sha256 = pay_ai_001_data_minimization_policy_sha256();
    $provider_policy_sha256 = pay_ai_001_provider_policy_sha256();
    if ($object_scope_sha256 === false || $field_scope_sha256 === false)
        return pay_ai_001_runtime_fail($error, 'ai_scope_hash_failed');

    $request_basis = array(
        'company_id'=>$company,
        'capability_code'=>(string)$capability,
        'purpose_code'=>'hrm_anl_headcount_compare',
        'actor_user_id'=>$actor,
        'authorized_input_refs_sha256'=>(string)$result['grounding_refs_sha256'],
        'object_scope_sha256'=>$object_scope_sha256,
        'field_scope_sha256'=>$field_scope_sha256,
        'data_minimization_sha256'=>$minimization_sha256,
        'provider_policy_sha256'=>$provider_policy_sha256,
        'opt_out_checked'=>1,
    );
    $request_sha256 = pay_ai_001_runtime_hash($request_basis);
    if ($request_sha256 === false) return pay_ai_001_runtime_fail($error, 'ai_request_hash_failed');

    global $transaction_level;
    if (!empty($transaction_level)) return pay_ai_001_runtime_fail($error, 'ai_custody_owned_transaction_required');
    begin_transaction();
    $request = pay_ai_001_runtime_optional_one('SELECT * FROM '.TB_PREF.'hrm_pay_ai_001_requests WHERE company_id='.(int)$company.' AND request_sha256='.db_escape($request_sha256).' LIMIT 2');
    $now = gmdate('Y-m-d H:i:s');
    if ($request === false) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_request_lookup_failed'); }
    if ($request === null) {
        $sql = 'INSERT INTO '.TB_PREF.'hrm_pay_ai_001_requests '
            .'(company_id,capability_code,purpose_code,actor_user_id,authorized_input_refs_sha256,object_scope_sha256,field_scope_sha256,data_minimization_sha256,provider_policy_sha256,opt_out_checked,request_sha256,requested_at,created_at) VALUES ('
            .(int)$company.','.db_escape($capability).','.db_escape('hrm_anl_headcount_compare').','.(int)$actor.','.db_escape($result['grounding_refs_sha256']).','
            .db_escape($object_scope_sha256).','.db_escape($field_scope_sha256).','.db_escape($minimization_sha256).','.db_escape($provider_policy_sha256).',1,'.db_escape($request_sha256).','.db_escape($now).','.db_escape($now).')';
        if (!db_query($sql)) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_request_insert_failed'); }
        $request_id = (int)db_insert_id();
    } else {
        $request_id = (int)$request['ai_request_id'];
    }

    $output_basis = array(
        'company_id'=>$company,
        'ai_request_id'=>$request_id,
        'provider_code'=>'local_deterministic',
        'model_version'=>'deterministic-aggregate-delta-v1',
        'generated_content_sha256'=>(string)$result['generated_content_sha256'],
        'grounding_refs_sha256'=>(string)$result['grounding_refs_sha256'],
        'confidence_evidence_sha256'=>(string)$result['baseline_sha256'],
        'abstained'=>false,
        'provider_failure'=>false,
    );
    $output_sha256 = pay_ai_001_runtime_hash($output_basis);
    if ($output_sha256 === false) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_output_hash_failed'); }
    $output = pay_ai_001_runtime_optional_one('SELECT * FROM '.TB_PREF.'hrm_pay_ai_001_outputs WHERE company_id='.(int)$company.' AND output_sha256='.db_escape($output_sha256).' LIMIT 2');
    if ($output === false) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_output_lookup_failed'); }
    if ($output === null) {
        $sql = 'INSERT INTO '.TB_PREF.'hrm_pay_ai_001_outputs '
            .'(company_id,ai_request_id,provider_code,model_version,generated_content_sha256,grounding_refs_sha256,confidence_evidence_sha256,abstained,provider_failure,output_sha256,generated_at,created_at) VALUES ('
            .(int)$company.','.(int)$request_id.','.db_escape('local_deterministic').','.db_escape('deterministic-aggregate-delta-v1').','.db_escape($result['generated_content_sha256']).','
            .db_escape($result['grounding_refs_sha256']).','.db_escape($result['baseline_sha256']).',0,0,'.db_escape($output_sha256).','.db_escape($now).','.db_escape($now).')';
        if (!db_query($sql)) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_output_insert_failed'); }
        $output_id = (int)db_insert_id();
        $idempotent = false;
    } else {
        $output_id = (int)$output['ai_output_id'];
        $idempotent = true;
    }
    commit_transaction();
    return array('ai_request_id'=>$request_id,'ai_output_id'=>$output_id,'request_sha256'=>$request_sha256,'output_sha256'=>$output_sha256,'idempotent'=>$idempotent);
}

/**
 * Run the accepted total-headcount comparison through deterministic PAY-AI.
 * Both inputs come from the governed HRM-ANL-001 runtime under the same actor.
 *
 * @return array|false
 */
function pay_ai_001_run_headcount_assistance($capability, $current_as_of_utc, $baseline_as_of_utc, &$error = null)
{
    $error = null;
    if (!in_array((string)$capability, array('anomaly_ranking','variance_explanation'), true))
        return pay_ai_001_runtime_fail($error, 'assistant_capability_not_exposed_on_screen');
    if ((string)$current_as_of_utc === (string)$baseline_as_of_utc)
        return pay_ai_001_runtime_fail($error, 'assistant_dates_must_differ');
    if (!pay_ai_001_runtime_custody_ready($error)) return false;

    $current = hrm_anl_001_execute_headcount_metric($current_as_of_utc, HRM_ANL_001_HEADCOUNT_PURPOSE, false, false, $error);
    if ($current === false) return false;
    $baseline = hrm_anl_001_execute_headcount_metric($baseline_as_of_utc, HRM_ANL_001_HEADCOUNT_PURPOSE, false, false, $error);
    if ($baseline === false) return false;

    $suppressed = !empty($current['suppressed']) || !empty($baseline['suppressed']);
    $reference_sha256 = pay_ai_001_runtime_hash(array('current_result_sha256'=>$current['result_sha256'],'baseline_result_sha256'=>$baseline['result_sha256']));
    if ($reference_sha256 === false) return pay_ai_001_runtime_fail($error, 'assistant_reference_hash_failed');
    $fact = array(
        'reference_type'=>'hrm_anl_aggregate',
        'reference_sha256'=>$reference_sha256,
        'metric_key'=>'workforce.active_headcount.total',
        'value_kind'=>'count',
        'current_value'=>$suppressed ? null : (int)$current['value'],
        'baseline_value'=>$suppressed ? null : (int)$baseline['value'],
        'status_code'=>$suppressed ? 'suppressed' : 'available',
        'authorized'=>true,
        'opted_out'=>false,
    );
    $version = pay_ai_001_runtime_database_version();
    $result = pay_ai_001_execute_deterministic_assistance($version, $capability, array($fact), $error);
    if ($result === false) return false;
    $custody = pay_ai_001_record_deterministic_output($capability, $fact, $result, $error);
    if ($custody === false) return false;
    return array('result'=>$result,'custody'=>$custody,'current'=>$current,'baseline'=>$baseline);
}

/** @return array */
function pay_ai_001_review_dispositions()
{
    return array('accepted','rejected','needs_follow_up');
}

/** @return array */
function pay_ai_001_review_reason_codes()
{
    return array('confirmed','false_positive','needs_data_review','not_actionable');
}

/**
 * Append one human review. Raw review prose is intentionally unsupported;
 * only bounded disposition/reason codes are hashed into immutable evidence.
 *
 * @return array|false
 */
function pay_ai_001_record_output_review($ai_output_id, $disposition_code, $reason_code, &$error = null)
{
    $error = null;
    if (!pay_ai_001_runtime_custody_ready($error)) return false;
    $output_id = filter_var($ai_output_id, FILTER_VALIDATE_INT, array('options'=>array('min_range'=>1)));
    if ($output_id === false
        || !in_array((string)$disposition_code, pay_ai_001_review_dispositions(), true)
        || !in_array((string)$reason_code, pay_ai_001_review_reason_codes(), true))
        return pay_ai_001_runtime_fail($error, 'ai_review_input_invalid');

    $company = pay_ai_001_runtime_company_id();
    $actor = pay_ai_001_runtime_actor_id();
    global $transaction_level;
    if (!empty($transaction_level)) return pay_ai_001_runtime_fail($error, 'ai_review_owned_transaction_required');
    begin_transaction();
    $output = pay_ai_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_ai_001_outputs WHERE company_id='.(int)$company.' AND ai_output_id='.(int)$output_id.' LIMIT 2 FOR UPDATE');
    if ($output === false) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_output_not_found'); }
    $existing = db_query('SELECT ai_review_id FROM '.TB_PREF.'hrm_pay_ai_001_reviews WHERE company_id='.(int)$company.' AND ai_output_id='.(int)$output_id.' LIMIT 2');
    if (!$existing) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_review_lookup_failed'); }
    if (db_num_rows($existing) !== 0) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_output_already_reviewed'); }
    $reason_sha256 = pay_ai_001_runtime_hash(array('reason_code'=>(string)$reason_code));
    $review_basis = array('company_id'=>$company,'ai_output_id'=>(int)$output_id,'reviewer_user_id'=>$actor,'disposition_code'=>(string)$disposition_code,'reason_sha256'=>$reason_sha256,'false_positive'=>$reason_code==='false_positive','appeal_requested'=>false,'output_sha256'=>(string)$output['output_sha256']);
    $review_sha256 = pay_ai_001_runtime_hash($review_basis);
    if ($reason_sha256 === false || $review_sha256 === false) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_review_hash_failed'); }
    $now = gmdate('Y-m-d H:i:s');
    $sql = 'INSERT INTO '.TB_PREF.'hrm_pay_ai_001_reviews (company_id,ai_output_id,reviewer_user_id,disposition_code,reason_sha256,false_positive,appeal_requested,review_sha256,reviewed_at,created_at) VALUES ('
        .(int)$company.','.(int)$output_id.','.(int)$actor.','.db_escape($disposition_code).','.db_escape($reason_sha256).','.($reason_code==='false_positive'?1:0).',0,'.db_escape($review_sha256).','.db_escape($now).','.db_escape($now).')';
    if (!db_query($sql)) { cancel_transaction(); return pay_ai_001_runtime_fail($error, 'ai_review_insert_failed'); }
    $review_id = (int)db_insert_id();
    commit_transaction();
    return array('ai_review_id'=>$review_id,'review_sha256'=>$review_sha256,'disposition_code'=>(string)$disposition_code,'reason_code'=>(string)$reason_code);
}
?>
