<?php
/** PAY-RULE-003 validation/review/execution workflow for simulation-only legacy import. */
include_once(__DIR__.'/decision_rate_legacy_import.inc');

if (!defined('HRM_PAY_RULE_003_IMPORT_VALIDATION_VERSION'))
    define('HRM_PAY_RULE_003_IMPORT_VALIDATION_VERSION', '1.0.481');
if (!defined('HRM_PAY_RULE_003_IMPORT_REVIEW_VERSION'))
    define('HRM_PAY_RULE_003_IMPORT_REVIEW_VERSION', '1.0.482');
if (!defined('HRM_PAY_RULE_003_IMPORT_EXECUTION_VERSION'))
    define('HRM_PAY_RULE_003_IMPORT_EXECUTION_VERSION', '1.0.483');
if (!defined('HRM_PAY_RULE_003_PUBLICATION_BOUNDARY_VERSION'))
    define('HRM_PAY_RULE_003_PUBLICATION_BOUNDARY_VERSION', '1.0.484');
if (!defined('HRM_PAY_RULE_003_COMPLETION_VERSION'))
    define('HRM_PAY_RULE_003_COMPLETION_VERSION', '1.0.485');

function hrm_pay_rule_003_import_evidence_id_valid($id)
{
    return is_string($id) && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]{2,127}$/D', $id) === 1;
}

function hrm_pay_rule_003_candidate_manifest($candidates, &$error)
{
    $error = null;
    if (!is_array($candidates) || !hrm_payroll_decision_table_is_list($candidates)
        || count($candidates) < 1 || count($candidates) > 500) {
        $error = 'candidate_set_invalid';
        return false;
    }
    $manifest = array();
    $maker = null;
    $ids = array();
    foreach ($candidates as $candidate) {
        if (!is_array($candidate)
            || !isset($candidate['candidate_id'],$candidate['source_kind'],$candidate['source_row_id'],
                      $candidate['source_snapshot_sha256'],$candidate['candidate_status'],$candidate['created_by'])
            || (string)$candidate['candidate_status'] !== 'quarantined_unverified'
            || !in_array($candidate['source_kind'], array('tax_brackets','statutory_deductions'), true)
            || !is_numeric($candidate['candidate_id']) || (int)$candidate['candidate_id'] <= 0
            || !is_numeric($candidate['source_row_id']) || (int)$candidate['source_row_id'] <= 0
            || preg_match('/^[a-f0-9]{64}$/D', (string)$candidate['source_snapshot_sha256']) !== 1) {
            $error = 'candidate_invalid';
            return false;
        }
        $candidate_id = (int)$candidate['candidate_id'];
        if (isset($ids[$candidate_id])) {
            $error = 'candidate_duplicate';
            return false;
        }
        $ids[$candidate_id] = true;
        if ($maker === null)
            $maker = (int)$candidate['created_by'];
        if ($maker <= 0 || $maker !== (int)$candidate['created_by']) {
            $error = 'candidate_maker_mismatch';
            return false;
        }
        $manifest[] = array(
            'candidate_id'=>$candidate_id,
            'source_kind'=>$candidate['source_kind'],
            'source_row_id'=>(int)$candidate['source_row_id'],
            'source_snapshot_sha256'=>$candidate['source_snapshot_sha256']
        );
    }
    usort($manifest, function($a,$b) {
        if ($a['candidate_id'] === $b['candidate_id'])
            return strcmp($a['source_kind'], $b['source_kind']);
        return $a['candidate_id'] < $b['candidate_id'] ? -1 : 1;
    });
    $json = hrm_pay_rule_003_canonical_json($manifest);
    if ($json === false) {
        $error = 'candidate_manifest_encode_failed';
        return false;
    }
    return array(
        'manifest'=>$manifest,
        'manifest_json'=>$json,
        'candidate_set_sha256'=>hash('sha256', 'pay-rule-003-candidate-set-v1|'.$json),
        'maker_id'=>$maker
    );
}

function hrm_pay_rule_003_validate_import_package($candidates, $artifact, $evidence_id, &$error)
{
    if (!hrm_pay_rule_003_import_evidence_id_valid($evidence_id)) {
        $error = 'evidence_id_invalid';
        return false;
    }
    $manifest = hrm_pay_rule_003_candidate_manifest($candidates, $error);
    if ($manifest === false)
        return false;

    $kinds = array();
    foreach ($manifest['manifest'] as $entry)
        $kinds[$entry['source_kind']] = true;
    $source_class = count($kinds) > 1
        ? 'legacy_mixed'
        : (isset($kinds['tax_brackets']) ? 'legacy_tax_brackets' : 'legacy_statutory_deductions');

    $binding = hrm_pay_rule_003_prepare_binding($artifact, 'approved_for_simulation', $source_class, array(
        'evidence_id'=>$evidence_id,
        'candidate_set_sha256'=>$manifest['candidate_set_sha256'],
        'approval_scope'=>'simulation_only'
    ), $error);
    if ($binding === false)
        return false;

    return array_merge($manifest, array(
        'artifact'=>$artifact,
        'artifact_json'=>$binding['artifact_json'],
        'artifact_sha256'=>$binding['artifact_sha256'],
        'plan_sha256'=>$binding['plan_sha256'],
        'source_class'=>$source_class,
        'evidence_id'=>$evidence_id
    ));
}

function hrm_pay_rule_003_load_candidates_for_update($ids, &$error)
{
    $error = null;
    if (!is_array($ids) || !hrm_payroll_decision_table_is_list($ids)
        || count($ids) < 1 || count($ids) > 500) {
        $error = 'candidate_ids_invalid';
        return false;
    }
    $normalized = array();
    foreach ($ids as $id) {
        if (!is_int($id) || $id <= 0 || isset($normalized[$id])) {
            $error = 'candidate_ids_invalid';
            return false;
        }
        $normalized[$id] = true;
    }
    $result = db_query('SELECT candidate_id,source_kind,source_row_id,source_snapshot_json,source_snapshot_sha256,'
        .'diagnostics_json,candidate_status,created_by FROM '.TB_PREF.'hrm_pay_rule_import_candidates '
        .'WHERE candidate_id IN ('.implode(',', array_keys($normalized)).') ORDER BY candidate_id FOR UPDATE');
    if (!$result) {
        $error = 'candidate_load_failed';
        return false;
    }
    $rows = array();
    while ($row = db_fetch_assoc($result))
        $rows[] = $row;
    if (count($rows) !== count($normalized)) {
        $error = 'candidate_missing';
        return false;
    }
    return $rows;
}

/** Reconcile every frozen candidate against the current locked legacy source. */
function hrm_pay_rule_003_reconcile_candidates($candidates, &$error)
{
    $error = null;
    foreach ($candidates as $candidate) {
        if (hrm_pay_rule_003_reconcile_legacy_candidate($candidate, true, $error) === false)
            return false;
    }
    return true;
}

function hrm_pay_rule_003_review_import($candidate_ids, $artifact, $evidence_id, $decision)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!hrm_pay_rule_003_store_user_can('SA_HRM_REVIEW_PAY_RULE_IMPORT')
        || !in_array($decision, array('approved_for_simulation','rejected'), true))
        return false;
    $reviewer = hrm_pay_rule_003_store_actor_id();
    if ($reviewer <= 0)
        return false;

    begin_transaction();
    $error = null;
    $rows = hrm_pay_rule_003_load_candidates_for_update($candidate_ids, $error);
    $ok = $rows !== false;
    if ($ok && !hrm_pay_rule_003_reconcile_candidates($rows, $error))
        $ok = false;
    $package = $ok ? hrm_pay_rule_003_validate_import_package($rows, $artifact, $evidence_id, $error) : false;
    if ($package === false)
        $ok = false;
    if ($ok && $reviewer === (int)$package['maker_id'])
        $ok = false;

    $review_id = false;
    $review_sha256 = false;
    if ($ok) {
        $review_base = array(
            'candidate_set_sha256'=>$package['candidate_set_sha256'],
            'artifact_sha256'=>$package['artifact_sha256'],
            'plan_sha256'=>$package['plan_sha256'],
            'source_class'=>$package['source_class'],
            'evidence_id'=>$evidence_id,
            'review_status'=>$decision,
            'maker_id'=>(int)$package['maker_id'],
            'reviewer_id'=>$reviewer
        );
        $json = hrm_pay_rule_003_canonical_json($review_base);
        $review_sha256 = $json === false ? false : hash('sha256', 'pay-rule-003-import-review-v1|'.$json);
        if ($review_sha256 === false)
            $ok = false;
    }

    if ($ok) {
        $result = db_query('SELECT review_id,candidate_manifest_json,candidate_set_sha256,artifact_sha256,plan_sha256,'
            .'source_class,artifact_json,evidence_id,review_status,maker_id,reviewer_id FROM '
            .TB_PREF.'hrm_pay_rule_import_reviews WHERE review_sha256='.db_escape($review_sha256).' LIMIT 2 FOR UPDATE');
        $ok = $result && db_num_rows($result) <= 1;
        if ($ok && db_num_rows($result) === 1) {
            $row = db_fetch_assoc($result);
            $ok = (string)$row['candidate_manifest_json'] === $package['manifest_json']
                && (string)$row['candidate_set_sha256'] === $package['candidate_set_sha256']
                && (string)$row['artifact_sha256'] === $package['artifact_sha256']
                && (string)$row['plan_sha256'] === $package['plan_sha256']
                && (string)$row['source_class'] === $package['source_class']
                && (string)$row['artifact_json'] === $package['artifact_json']
                && (string)$row['evidence_id'] === $evidence_id
                && (string)$row['review_status'] === $decision
                && (int)$row['maker_id'] === (int)$package['maker_id']
                && (int)$row['reviewer_id'] === $reviewer;
            if ($ok)
                $review_id = (int)$row['review_id'];
        } elseif ($ok) {
            $ok = db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_import_reviews '
                .'(candidate_manifest_json,candidate_set_sha256,artifact_sha256,plan_sha256,source_class,artifact_json,evidence_id,'
                .'review_status,maker_id,reviewer_id,review_sha256) VALUES ('
                .db_escape($package['manifest_json']).','.db_escape($package['candidate_set_sha256']).','
                .db_escape($package['artifact_sha256']).','.db_escape($package['plan_sha256']).','
                .db_escape($package['source_class']).','.db_escape($package['artifact_json']).','
                .db_escape($evidence_id).','.db_escape($decision).','.(int)$package['maker_id'].','.$reviewer.','
                .db_escape($review_sha256).')') !== false;
            if ($ok) {
                $review_id = (int)db_insert_id();
                if ($review_id <= 0)
                    $ok = false;
            }
            if ($ok) {
                $audit = append_domain_audit_event(array(
                    'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
                    'legal_entity_id'=>0,
                    'entity_type'=>'pay_rule_003_import_review',
                    'entity_key'=>array('review_id'=>$review_id),
                    'action'=>'pay.rule_003.import_review_recorded',
                    'reason_code'=>'pay_rule_003_import_review',
                    'before'=>null,
                    'after'=>array('review_status'=>$decision,'simulation_only'=>true),
                    'restricted_diff'=>array(
                        'candidate_set_sha256'=>$package['candidate_set_sha256'],
                        'artifact_sha256'=>$package['artifact_sha256'],
                        'plan_sha256'=>$package['plan_sha256'],
                        'review_sha256'=>$review_sha256,
                        'legacy_source_reconciled'=>true,
                        'production_publication_allowed'=>false
                    )
                ));
                if ($audit === false)
                    $ok = false;
            }
        }
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $review_id : false;
}

function hrm_pay_rule_003_execute_import($review_id, $predecessor_id=null)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!hrm_pay_rule_003_store_user_can('SA_HRM_EXECUTE_PAY_RULE_IMPORT')
        || !is_int($review_id) || $review_id <= 0)
        return false;
    $executor = hrm_pay_rule_003_store_actor_id();
    if ($executor <= 0)
        return false;

    begin_transaction();
    $ok = true;
    $receipt_id = false;
    $existing_version_id = null;
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_pay_rule_import_reviews WHERE review_id='.$review_id.' LIMIT 2 FOR UPDATE');
    if (!$result || db_num_rows($result) !== 1)
        $ok = false;
    if ($ok) {
        $review = db_fetch_assoc($result);
        if ((string)$review['review_status'] !== 'approved_for_simulation'
            || $executor === (int)$review['maker_id'] || $executor === (int)$review['reviewer_id'])
            $ok = false;
    }

    if ($ok) {
        $result = db_query('SELECT * FROM '.TB_PREF.'hrm_pay_rule_import_receipts WHERE review_id='.$review_id.' LIMIT 2 FOR UPDATE');
        if (!$result || db_num_rows($result) > 1) {
            $ok = false;
        } elseif (db_num_rows($result) === 1) {
            $receipt = db_fetch_assoc($result);
            if ((int)$receipt['executor_id'] !== $executor || (string)$receipt['reconciliation_status'] !== 'verified'
                || (string)$receipt['candidate_set_sha256'] !== (string)$review['candidate_set_sha256']
                || (string)$receipt['artifact_sha256'] !== (string)$review['artifact_sha256']
                || (string)$receipt['plan_sha256'] !== (string)$review['plan_sha256']
                || (string)$receipt['review_sha256'] !== (string)$review['review_sha256']) {
                $ok = false;
            } else {
                $receipt_base = array(
                    'review_id'=>$review_id,
                    'artifact_version_id'=>(int)$receipt['artifact_version_id'],
                    'candidate_set_sha256'=>$receipt['candidate_set_sha256'],
                    'artifact_sha256'=>$receipt['artifact_sha256'],
                    'plan_sha256'=>$receipt['plan_sha256'],
                    'review_sha256'=>$receipt['review_sha256'],
                    'executor_id'=>$executor,
                    'reconciliation_status'=>'verified'
                );
                $json = hrm_pay_rule_003_canonical_json($receipt_base);
                $expected = $json === false ? false : hash('sha256', 'pay-rule-003-import-receipt-v1|'.$json);
                if ($expected === false || !hash_equals((string)$receipt['receipt_sha256'], $expected)) {
                    $ok = false;
                } else {
                    $receipt_id = (int)$receipt['receipt_id'];
                    $existing_version_id = (int)$receipt['artifact_version_id'];
                }
            }
        }
    }

    if ($ok && $receipt_id !== false) {
        $error = null;
        $artifact_row = hrm_pay_rule_003_read_artifact_version_internal($existing_version_id, $error);
        if ($artifact_row === false || $artifact_row['custody_status'] !== 'approved_for_simulation'
            || $artifact_row['artifact_sha256'] !== (string)$review['artifact_sha256']
            || $artifact_row['plan_sha256'] !== (string)$review['plan_sha256'])
            $ok = false;
        if ($ok) commit_transaction(); else cancel_transaction();
        return $ok ? $receipt_id : false;
    }

    if ($ok) {
        $manifest = json_decode($review['candidate_manifest_json'], true);
        $artifact = json_decode($review['artifact_json'], true);
        if (!is_array($manifest) || !is_array($artifact))
            $ok = false;
    }
    if ($ok) {
        $candidate_ids = array();
        foreach ($manifest as $entry)
            $candidate_ids[] = (int)$entry['candidate_id'];
        $error = null;
        $rows = hrm_pay_rule_003_load_candidates_for_update($candidate_ids, $error);
        if ($rows === false || !hrm_pay_rule_003_reconcile_candidates($rows, $error)) {
            $ok = false;
        } else {
            $package = hrm_pay_rule_003_validate_import_package($rows, $artifact, $review['evidence_id'], $error);
            if ($package === false
                || $package['candidate_set_sha256'] !== (string)$review['candidate_set_sha256']
                || $package['artifact_sha256'] !== (string)$review['artifact_sha256']
                || $package['plan_sha256'] !== (string)$review['plan_sha256']
                || $package['source_class'] !== (string)$review['source_class']
                || $package['manifest_json'] !== (string)$review['candidate_manifest_json'])
                $ok = false;
        }
    }

    if ($ok) {
        $artifact_version_id = hrm_pay_rule_003_append_artifact_version(
            $artifact,
            'approved_for_simulation',
            $package['source_class'],
            array(
                'evidence_id'=>$review['evidence_id'],
                'candidate_set_sha256'=>$package['candidate_set_sha256'],
                'review_id'=>$review_id,
                'review_sha256'=>$review['review_sha256'],
                'approval_scope'=>'simulation_only'
            ),
            $predecessor_id,
            $executor,
            'SA_HRM_EXECUTE_PAY_RULE_IMPORT',
            false
        );
        if ($artifact_version_id === false)
            $ok = false;
    }

    if ($ok) {
        $receipt_base = array(
            'review_id'=>$review_id,
            'artifact_version_id'=>(int)$artifact_version_id,
            'candidate_set_sha256'=>$package['candidate_set_sha256'],
            'artifact_sha256'=>$package['artifact_sha256'],
            'plan_sha256'=>$package['plan_sha256'],
            'review_sha256'=>$review['review_sha256'],
            'executor_id'=>$executor,
            'reconciliation_status'=>'verified'
        );
        $json = hrm_pay_rule_003_canonical_json($receipt_base);
        $receipt_sha256 = $json === false ? false : hash('sha256', 'pay-rule-003-import-receipt-v1|'.$json);
        if ($receipt_sha256 === false) {
            $ok = false;
        } else {
            $ok = db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_import_receipts '
                .'(review_id,artifact_version_id,candidate_set_sha256,artifact_sha256,plan_sha256,review_sha256,executor_id,receipt_sha256,reconciliation_status) VALUES ('
                .$review_id.','.(int)$artifact_version_id.','.db_escape($package['candidate_set_sha256']).','
                .db_escape($package['artifact_sha256']).','.db_escape($package['plan_sha256']).','
                .db_escape($review['review_sha256']).','.$executor.','.db_escape($receipt_sha256).",'verified')") !== false;
            if ($ok) {
                $receipt_id = (int)db_insert_id();
                if ($receipt_id <= 0)
                    $ok = false;
            }
        }
        if ($ok) {
            $audit = append_domain_audit_event(array(
                'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
                'legal_entity_id'=>0,
                'entity_type'=>'pay_rule_003_import_receipt',
                'entity_key'=>array('receipt_id'=>$receipt_id,'review_id'=>$review_id,
                                    'artifact_version_id'=>(int)$artifact_version_id),
                'action'=>'pay.rule_003.import_executed_for_simulation',
                'reason_code'=>'pay_rule_003_import_execution',
                'before'=>null,
                'after'=>array('reconciliation_status'=>'verified','simulation_only'=>true),
                'restricted_diff'=>array(
                    'receipt_sha256'=>$receipt_sha256,
                    'candidate_set_sha256'=>$package['candidate_set_sha256'],
                    'artifact_sha256'=>$package['artifact_sha256'],
                    'plan_sha256'=>$package['plan_sha256'],
                    'legacy_source_reconciled_at_execution'=>true,
                    'production_publication_allowed'=>false
                )
            ));
            if ($audit === false)
                $ok = false;
        }
    }

    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $receipt_id : false;
}

function hrm_pay_rule_003_import_recovery_policy()
{
    return array(
        'mutation_of_accepted_evidence'=>false,
        'retry_exact_execution'=>'verification_only',
        'stale_source_snapshot'=>'new_candidate_and_new_immutable_review',
        'stale_or_rejected_package'=>'new_immutable_review',
        'production_cutover_authorized'=>false,
        'selective_downgrade_authorized'=>false
    );
}
