<?php
/**
 * HRM-ANL-001 governed metric/suppression catalog registration.
 *
 * Registration is deliberately two-person. A reviewer first records a
 * hash-only domain-audit review event. A distinct authenticated approver may
 * then append the exact built-in headcount definition and suppression policy.
 * No catalog row can be updated or deleted after insertion.
 */
include_once($path_to_root.'/includes/hrm_anl_001_contract.inc');
include_once($path_to_root.'/includes/hrm_anl_001_headcount_contract.inc');
include_once($path_to_root.'/includes/db/domain_audit_db.inc');

/** @return false */
function hrm_anl_001_catalog_fail(&$error, $code)
{
    $error = $code;
    return false;
}

/** @return string|false */
function hrm_anl_001_runtime_database_version()
{
    if (!function_exists('get_company_pref')) return false;
    $version = get_company_pref('version_id', true);
    return is_string($version) ? $version : false;
}

/** @return int */
function hrm_anl_001_runtime_company_id()
{
    return isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->company)
        ? (int)$_SESSION['wa_current_user']->company : -1;
}

/** @return int */
function hrm_anl_001_runtime_actor_id()
{
    return isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}

/** @return bool */
function hrm_anl_001_runtime_has_report_access()
{
    return function_exists('user_check_access') && user_check_access('SA_HRMREPORTS');
}

/** @return bool */
function hrm_anl_001_runtime_catalog_writer_ready(&$error)
{
    $version = hrm_anl_001_runtime_database_version();
    if (!is_string($version) || !hrm_anl_001_catalog_registration_writer_allowed_for_database_version($version))
        return hrm_anl_001_catalog_fail($error, 'catalog_registration_writer_not_active');
    if (!hrm_anl_001_runtime_has_report_access())
        return hrm_anl_001_catalog_fail($error, 'permission_denied');
    if (hrm_anl_001_runtime_company_id() < 0 || hrm_anl_001_runtime_actor_id() <= 0)
        return hrm_anl_001_catalog_fail($error, 'authenticated_scope_required');
    return true;
}

/** @return string */
function hrm_anl_001_catalog_sql_nullable($value)
{
    return $value === null ? 'NULL' : db_escape($value);
}

/** @return array|false */
function hrm_anl_001_catalog_one($sql)
{
    $result = db_query($sql);
    if (!$result || db_num_rows($result) !== 1) return false;
    return db_fetch_assoc($result);
}

/** @return array */
function hrm_anl_001_catalog_review_payload($valid_from, $expires_at)
{
    $company = hrm_anl_001_runtime_company_id();
    $definition = hrm_anl_001_headcount_metric_definition_for_company($company);
    $policy = hrm_anl_001_headcount_suppression_policy_for_company($company);
    return array(
        'definition_sha256' => $definition === false ? '' : $definition['definition_sha256'],
        'policy_sha256' => $policy === false ? '' : $policy['policy_sha256'],
        'metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY,
        'policy_key' => HRM_ANL_001_HEADCOUNT_POLICY_KEY,
        'valid_from' => (string)$valid_from,
        'expires_at' => (string)$expires_at,
        'minimum_cohort_size' => HRM_ANL_001_HEADCOUNT_MINIMUM_COHORT,
        'export_allowed' => false,
        'drilldown_allowed' => false,
        'employee_identifiers_in_result' => false,
    );
}

/**
 * Record the first-person review for the exact built-in headcount catalog.
 *
 * @param string $valid_from UTC datetime YYYY-MM-DD HH:MM:SS.
 * @param string $expires_at UTC datetime later than valid_from.
 * @param string|null $error
 * @return array|false
 */
function hrm_anl_001_review_headcount_catalog($valid_from, $expires_at, &$error = null)
{
    $error = null;
    if (!hrm_anl_001_runtime_catalog_writer_ready($error)) return false;
    global $transaction_level;
    if (!empty($transaction_level))
        return hrm_anl_001_catalog_fail($error, 'catalog_review_owned_transaction_required');
    if (!hrm_anl_001_headcount_valid_window($valid_from, $expires_at))
        return hrm_anl_001_catalog_fail($error, 'catalog_validity_window_invalid');
    $company = hrm_anl_001_runtime_company_id();
    $definition = hrm_anl_001_headcount_metric_definition_for_company($company);
    $policy = hrm_anl_001_headcount_suppression_policy_for_company($company);
    if ($definition === false || $policy === false
        || !hrm_anl_001_headcount_sha256($definition['definition_sha256'])
        || !hrm_anl_001_headcount_sha256($policy['policy_sha256']))
        return hrm_anl_001_catalog_fail($error, 'catalog_blueprint_invalid');

    $actor = hrm_anl_001_runtime_actor_id();
    $payload = hrm_anl_001_catalog_review_payload($valid_from, $expires_at);
    $event = append_domain_audit_event(array(
        'company_id' => $company,
        'legal_entity_id' => 0,
        'entity_type' => 'hrm-anl-001-catalog',
        'entity_key' => array('metric_key' => HRM_ANL_001_HEADCOUNT_METRIC_KEY, 'definition_sha256' => $definition['definition_sha256']),
        'action' => 'reviewed',
        'reason_code' => 'hrm-anl-001-headcount-catalog-review',
        'restricted_diff' => $payload,
    ));
    if (!is_array($event) || empty($event['event_hash']))
        return hrm_anl_001_catalog_fail($error, 'catalog_review_audit_failed');
    return array(
        'review_event_sha256' => (string)$event['event_hash'],
        'reviewer_user_id' => $actor,
        'definition_sha256' => $definition['definition_sha256'],
        'policy_sha256' => $policy['policy_sha256'],
        'valid_from' => $valid_from,
        'expires_at' => $expires_at,
    );
}

/** @return array|false */
function hrm_anl_001_catalog_review_event($review_event_sha256, &$error = null)
{
    $company = hrm_anl_001_runtime_company_id();
    if (!hrm_anl_001_headcount_sha256($review_event_sha256))
        return hrm_anl_001_catalog_fail($error, 'catalog_review_event_hash_invalid');
    $sql = 'SELECT event_id,actor_user_id,event_hash,restricted_diff FROM '.TB_PREF.'domain_audit_events'
        .' WHERE company_id='.$company.' AND legal_entity_id=0'
        .' AND entity_type='.db_escape('hrm-anl-001-catalog')
        .' AND action='.db_escape('reviewed')
        .' AND reason_code='.db_escape('hrm-anl-001-headcount-catalog-review')
        .' AND event_hash='.db_escape($review_event_sha256).' LIMIT 2';
    $row = hrm_anl_001_catalog_one($sql);
    if ($row === false) return hrm_anl_001_catalog_fail($error, 'catalog_review_event_missing');
    $payload = json_decode((string)$row['restricted_diff'], true);
    if (!is_array($payload)) return hrm_anl_001_catalog_fail($error, 'catalog_review_payload_invalid');
    $definition = hrm_anl_001_headcount_metric_definition_for_company($company);
    $policy = hrm_anl_001_headcount_suppression_policy_for_company($company);
    if ($definition === false || $policy === false
        || !isset($payload['definition_sha256'], $payload['policy_sha256'], $payload['metric_key'], $payload['policy_key'], $payload['valid_from'], $payload['expires_at'])
        || !hash_equals((string)$definition['definition_sha256'], (string)$payload['definition_sha256'])
        || !hash_equals((string)$policy['policy_sha256'], (string)$payload['policy_sha256'])
        || (string)$payload['metric_key'] !== HRM_ANL_001_HEADCOUNT_METRIC_KEY
        || (string)$payload['policy_key'] !== HRM_ANL_001_HEADCOUNT_POLICY_KEY
        || !hrm_anl_001_headcount_valid_window((string)$payload['valid_from'], (string)$payload['expires_at'])
        || !isset($payload['minimum_cohort_size']) || (int)$payload['minimum_cohort_size'] !== HRM_ANL_001_HEADCOUNT_MINIMUM_COHORT
        || !array_key_exists('export_allowed', $payload) || $payload['export_allowed'] !== false
        || !array_key_exists('drilldown_allowed', $payload) || $payload['drilldown_allowed'] !== false
        || !array_key_exists('employee_identifiers_in_result', $payload) || $payload['employee_identifiers_in_result'] !== false)
        return hrm_anl_001_catalog_fail($error, 'catalog_review_payload_mismatch');
    $row['payload'] = $payload;
    return $row;
}

/** @return array|false */
function hrm_anl_001_existing_catalog_pair($company, $definition_sha256, $policy_sha256, &$error = null)
{
    $metric_result = db_query('SELECT * FROM '.TB_PREF.'hrm_anl_001_metric_definitions WHERE company_id='.(int)$company
        .' AND definition_sha256='.db_escape($definition_sha256).' LIMIT 2');
    $policy_result = db_query('SELECT * FROM '.TB_PREF.'hrm_anl_001_suppression_policies WHERE company_id='.(int)$company
        .' AND policy_sha256='.db_escape($policy_sha256).' LIMIT 2');
    if (!$metric_result || !$policy_result) return hrm_anl_001_catalog_fail($error, 'catalog_lookup_failed');
    $metric_count = db_num_rows($metric_result);
    $policy_count = db_num_rows($policy_result);
    if ($metric_count === 0 && $policy_count === 0) return array('metric' => null, 'policy' => null);
    if ($metric_count !== 1 || $policy_count !== 1) return hrm_anl_001_catalog_fail($error, 'catalog_partial_or_duplicate_state');
    return array('metric' => db_fetch_assoc($metric_result), 'policy' => db_fetch_assoc($policy_result));
}

/**
 * Approve and append the exact reviewed headcount definition and suppression policy.
 *
 * The approver must be the currently authenticated user and must differ from the
 * reviewer captured in the immutable domain-audit review event.
 *
 * @param string $review_event_sha256
 * @param string|null $error
 * @return array|false
 */
function hrm_anl_001_approve_headcount_catalog($review_event_sha256, &$error = null)
{
    $error = null;
    if (!hrm_anl_001_runtime_catalog_writer_ready($error)) return false;
    $review = hrm_anl_001_catalog_review_event($review_event_sha256, $error);
    if ($review === false) return false;
    $reviewer = (int)$review['actor_user_id'];
    $approver = hrm_anl_001_runtime_actor_id();
    if ($reviewer <= 0 || $approver <= 0 || $reviewer === $approver)
        return hrm_anl_001_catalog_fail($error, 'catalog_reviewer_approver_separation_required');

    $company = hrm_anl_001_runtime_company_id();
    $definition = hrm_anl_001_headcount_metric_definition_for_company($company);
    $policy = hrm_anl_001_headcount_suppression_policy_for_company($company);
    if ($definition === false || $policy === false) return hrm_anl_001_catalog_fail($error, 'catalog_blueprint_invalid');
    $existing = hrm_anl_001_existing_catalog_pair($company, $definition['definition_sha256'], $policy['policy_sha256'], $error);
    if ($existing === false) return false;
    if ($existing['metric'] !== null && $existing['policy'] !== null) {
        if ((int)$existing['metric']['reviewed_by'] !== $reviewer || (int)$existing['metric']['approved_by'] !== $approver
            || (int)$existing['policy']['reviewed_by'] !== $reviewer || (int)$existing['policy']['approved_by'] !== $approver
            || (string)$existing['metric']['valid_from'] !== (string)$review['payload']['valid_from']
            || (string)$existing['metric']['expires_at'] !== (string)$review['payload']['expires_at']
            || (string)$existing['policy']['valid_from'] !== (string)$review['payload']['valid_from']
            || (string)$existing['policy']['expires_at'] !== (string)$review['payload']['expires_at'])
            return hrm_anl_001_catalog_fail($error, 'catalog_existing_approval_mismatch');
        return array(
            'metric_definition_id' => (int)$existing['metric']['metric_definition_id'],
            'suppression_policy_id' => (int)$existing['policy']['suppression_policy_id'],
            'definition_sha256' => $definition['definition_sha256'],
            'policy_sha256' => $policy['policy_sha256'],
            'reviewer_user_id' => $reviewer,
            'approver_user_id' => $approver,
            'idempotent' => true,
        );
    }

    global $transaction_level;
    if (!empty($transaction_level)) return hrm_anl_001_catalog_fail($error, 'catalog_owned_transaction_required');
    $valid_from = (string)$review['payload']['valid_from'];
    $expires_at = (string)$review['payload']['expires_at'];
    $created_at = gmdate('Y-m-d H:i:s');
    begin_transaction();
    $metric_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_metric_definitions '
        .'(company_id,metric_key,definition_version,source_model_code,value_type_code,unit_code,currency_semantics_code,as_of_semantics_code,aggregation_code,source_contract_sha256,field_policy_sha256,object_policy_sha256,definition_sha256,reviewed_by,approved_by,valid_from,expires_at,created_at) VALUES ('
        .$company.','.db_escape($definition['metric_key']).','.db_escape($definition['definition_version']).','.db_escape($definition['source_model_code']).','
        .db_escape($definition['value_type_code']).','.db_escape($definition['unit_code']).','.db_escape($definition['currency_semantics_code']).','
        .db_escape($definition['as_of_semantics_code']).','.db_escape($definition['aggregation_code']).','.db_escape($definition['source_contract_sha256']).','
        .db_escape($definition['field_policy_sha256']).','.db_escape($definition['object_policy_sha256']).','.db_escape($definition['definition_sha256']).','
        .$reviewer.','.$approver.','.db_escape($valid_from).','.db_escape($expires_at).','.db_escape($created_at).')';
    if (!db_query($metric_sql)) {
        cancel_transaction();
        return hrm_anl_001_catalog_fail($error, 'metric_definition_insert_failed');
    }
    $metric_id = (int)db_insert_id();
    $policy_sql = 'INSERT INTO '.TB_PREF.'hrm_anl_001_suppression_policies '
        .'(company_id,policy_key,policy_version,minimum_cohort_size,suppress_exact_counts,export_allowed,policy_sha256,reviewed_by,approved_by,valid_from,expires_at,created_at) VALUES ('
        .$company.','.db_escape($policy['policy_key']).','.db_escape($policy['policy_version']).','.(int)$policy['minimum_cohort_size'].','.(int)$policy['suppress_exact_counts'].','
        .(int)$policy['export_allowed'].','.db_escape($policy['policy_sha256']).','.$reviewer.','.$approver.','.db_escape($valid_from).','.db_escape($expires_at).','.db_escape($created_at).')';
    if (!db_query($policy_sql)) {
        cancel_transaction();
        return hrm_anl_001_catalog_fail($error, 'suppression_policy_insert_failed');
    }
    $policy_id = (int)db_insert_id();
    commit_transaction();
    return array(
        'metric_definition_id' => $metric_id,
        'suppression_policy_id' => $policy_id,
        'definition_sha256' => $definition['definition_sha256'],
        'policy_sha256' => $policy['policy_sha256'],
        'reviewer_user_id' => $reviewer,
        'approver_user_id' => $approver,
        'idempotent' => false,
    );
}
?>
