<?php
/**
 * PAY-RULE-003 immutable decision/rate artifact custody.
 *
 * This source is deliberately not included by payroll_engine.inc.  It stores
 * typed artifacts only for draft/unverified/simulation custody and exposes no
 * production resolver or publication state.
 */
include_once(__DIR__.'/decision_rate_plan.inc');

if (!defined('HRM_PAY_RULE_003_ARTIFACT_STORE_VERSION'))
    define('HRM_PAY_RULE_003_ARTIFACT_STORE_VERSION', '1.0.479');
if (!defined('HRM_PAY_RULE_003_PRODUCTION_PUBLICATION_ALLOWED'))
    define('HRM_PAY_RULE_003_PRODUCTION_PUBLICATION_ALLOWED', false);

function hrm_pay_rule_003_canonical_json($value)
{
    $json = json_encode(hrm_payroll_decision_table_canonicalize($value), JSON_UNESCAPED_SLASHES);
    return is_string($json) ? $json : false;
}

function hrm_pay_rule_003_valid_custody_status($status)
{
    return in_array($status, array('manual_draft', 'unverified_legacy', 'approved_for_simulation'), true);
}

function hrm_pay_rule_003_valid_source_class($source)
{
    return in_array($source, array('manual', 'legacy_tax_brackets', 'legacy_statutory_deductions', 'legacy_mixed'), true);
}

function hrm_pay_rule_003_prepare_binding($artifact, $custody_status, $source_class, $evidence, &$error)
{
    $error = null;
    if (!is_array($artifact) || !isset($artifact['artifact_type'], $artifact['table_id'], $artifact['version'])) {
        $error = 'artifact_shape_invalid';
        return false;
    }
    if (!hrm_pay_rule_003_valid_custody_status($custody_status)) {
        $error = 'custody_status_forbidden';
        return false;
    }
    if (!hrm_pay_rule_003_valid_source_class($source_class)) {
        $error = 'source_class_invalid';
        return false;
    }
    if (!is_array($evidence)) {
        $error = 'evidence_invalid';
        return false;
    }
    if ($custody_status === 'manual_draft' && $source_class !== 'manual') {
        $error = 'manual_draft_source_mismatch';
        return false;
    }
    if ($custody_status === 'unverified_legacy' && $source_class === 'manual') {
        $error = 'unverified_legacy_source_mismatch';
        return false;
    }

    $artifact_type = (string)$artifact['artifact_type'];
    $contract = '';
    $plan = false;
    if ($artifact_type === 'decision_table') {
        $plan = hrm_payroll_decision_table_compile($artifact, $error);
        $contract = HRM_PAY_RULE_003_DECISION_CONTRACT_VERSION;
    } elseif ($artifact_type === 'rate_table') {
        $plan = hrm_payroll_rate_table_compile($artifact, $error);
        $contract = HRM_PAY_RULE_003_RATE_CONTRACT_VERSION;
    } else {
        $error = 'artifact_type_invalid';
        return false;
    }
    if ($plan === false)
        return false;

    $artifact_json = hrm_pay_rule_003_canonical_json($artifact);
    $evidence_json = hrm_pay_rule_003_canonical_json($evidence);
    if ($artifact_json === false || $evidence_json === false
        || strlen($artifact_json) > 1048576 || strlen($evidence_json) > 65535) {
        $error = 'artifact_or_evidence_too_large';
        return false;
    }

    return array(
        'artifact_key'=>$artifact_type.':'.$artifact['table_id'],
        'artifact_type'=>$artifact_type,
        'artifact_semver'=>(string)$artifact['version'],
        'custody_status'=>$custody_status,
        'source_class'=>$source_class,
        'artifact_json'=>$artifact_json,
        'artifact_sha256'=>$plan['artifact_hash'],
        'plan_sha256'=>$plan['plan_hash'],
        'validator_contract'=>$contract,
        'plan_contract'=>HRM_PAY_RULE_003_PLAN_VERSION,
        'evidence_json'=>$evidence_json
    );
}

function hrm_pay_rule_003_store_user_can($permission)
{
    return function_exists('user_check_access') && user_check_access($permission);
}

function hrm_pay_rule_003_store_actor_id()
{
    if (function_exists('hrm_person_worker_actor_id'))
        return (int)hrm_person_worker_actor_id();
    if (function_exists('user_id'))
        return (int)user_id();
    return 0;
}

function hrm_pay_rule_003_store_row_matches($row, $binding, $actor_id)
{
    foreach (array('artifact_semver','custody_status','source_class','artifact_json','artifact_sha256',
                   'plan_sha256','validator_contract','plan_contract','evidence_json') as $key) {
        if (!array_key_exists($key, $row) || (string)$row[$key] !== (string)$binding[$key])
            return false;
    }
    return (int)$row['created_by'] === (int)$actor_id;
}

/** Validate one in-memory, already locked version chain for a single artifact. */
function hrm_pay_rule_003_validate_version_chain_rows($rows, &$leaf_id, &$max_version)
{
    $leaf_id = null;
    $max_version = 0;
    if (!is_array($rows))
        return false;
    if (count($rows) === 0)
        return true;

    $by_version = array();
    $successor = array();
    foreach ($rows as $id=>$row) {
        $id = (int)$id;
        $version_no = isset($row['version_no']) ? (int)$row['version_no'] : 0;
        if ($id <= 0 || $version_no <= 0 || isset($by_version[$version_no]))
            return false;
        $by_version[$version_no] = $id;
        $max_version = max($max_version, $version_no);
    }
    if (count($by_version) !== $max_version)
        return false;
    ksort($by_version, SORT_NUMERIC);

    $previous_id = null;
    foreach ($by_version as $version_no=>$id) {
        $row = $rows[$id];
        $predecessor = $row['predecessor_id'] === null ? null : (int)$row['predecessor_id'];
        if ((int)$version_no === 1) {
            if ($predecessor !== null)
                return false;
        } else {
            if ($predecessor !== $previous_id || !isset($rows[$predecessor]) || isset($successor[$predecessor]))
                return false;
            $successor[$predecessor] = $id;
        }
        $previous_id = $id;
    }
    $leaf_id = $previous_id;
    return true;
}

/**
 * Append one immutable artifact version.
 *
 * Exact retries are verification-only. A successor must extend the current
 * leaf exactly; forks, gaps, cross-artifact predecessors and semantic-version
 * reuse with different evidence fail closed.
 */
function hrm_pay_rule_003_append_artifact_version($artifact, $custody_status, $source_class, $evidence,
    $predecessor_id=null, $actor_id=null, $permission='SA_HRM_MANAGE_PAY_RULE_ARTIFACT', $manage_transaction=true)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');

    if ($actor_id === null)
        $actor_id = hrm_pay_rule_003_store_actor_id();
    $error = null;
    $binding = hrm_pay_rule_003_prepare_binding($artifact, $custody_status, $source_class, $evidence, $error);
    if ($binding === false || $actor_id <= 0 || !hrm_pay_rule_003_store_user_can($permission))
        return false;

    if ($permission === 'SA_HRM_MANAGE_PAY_RULE_ARTIFACT') {
        if ($custody_status === 'approved_for_simulation')
            return false;
    } elseif ($permission === 'SA_HRM_EXECUTE_PAY_RULE_IMPORT') {
        if ($custody_status !== 'approved_for_simulation' || $source_class === 'manual')
            return false;
    } else {
        return false;
    }
    if ($predecessor_id !== null && (!is_int($predecessor_id) || $predecessor_id <= 0))
        return false;

    if ($manage_transaction)
        begin_transaction();
    $ok = true;
    $artifact_id = 0;
    $return_id = false;
    $created_identity = false;

    $key = db_escape($binding['artifact_key']);
    $result = db_query('SELECT artifact_id,artifact_type FROM '.TB_PREF.'hrm_pay_rule_artifacts '
        .'WHERE artifact_key='.$key.' LIMIT 2 FOR UPDATE');
    if (!$result || db_num_rows($result) > 1)
        $ok = false;
    if ($ok && db_num_rows($result) === 1) {
        $identity = db_fetch_assoc($result);
        $artifact_id = (int)$identity['artifact_id'];
        if ((string)$identity['artifact_type'] !== $binding['artifact_type'])
            $ok = false;
    } elseif ($ok && $predecessor_id !== null) {
        $ok = false;
    } elseif ($ok) {
        $ok = db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_artifacts (artifact_key,artifact_type,created_by) VALUES ('
            .$key.','.db_escape($binding['artifact_type']).','.(int)$actor_id.')') !== false;
        if ($ok) {
            $artifact_id = (int)db_insert_id();
            $created_identity = $artifact_id > 0;
            if (!$created_identity)
                $ok = false;
        }
    }

    $rows = array();
    $leaf_id = null;
    $max_version = 0;
    if ($ok) {
        $result = db_query('SELECT artifact_version_id,version_no,predecessor_id,artifact_semver,custody_status,source_class,'
            .'artifact_json,artifact_sha256,plan_sha256,validator_contract,plan_contract,evidence_json,created_by '
            .'FROM '.TB_PREF.'hrm_pay_rule_artifact_versions WHERE artifact_id='.(int)$artifact_id
            .' ORDER BY version_no,artifact_version_id FOR UPDATE');
        if (!$result) {
            $ok = false;
        } else {
            while ($row = db_fetch_assoc($result))
                $rows[(int)$row['artifact_version_id']] = $row;
            if (!hrm_pay_rule_003_validate_version_chain_rows($rows, $leaf_id, $max_version))
                $ok = false;
        }
    }

    if ($ok && $predecessor_id !== null) {
        if (!isset($rows[$predecessor_id]) || $leaf_id !== $predecessor_id) {
            /* Exact retry may name the predecessor of the already-created leaf. */
            $candidate = false;
            foreach ($rows as $id=>$row) {
                if ($row['predecessor_id'] !== null && (int)$row['predecessor_id'] === $predecessor_id)
                    $candidate = $row;
            }
            if ($candidate && $leaf_id === (int)$candidate['artifact_version_id']
                && hrm_pay_rule_003_store_row_matches($candidate, $binding, $actor_id))
                $return_id = (int)$candidate['artifact_version_id'];
            else
                $ok = false;
        }
    } elseif ($ok && count($rows) > 0) {
        if ($leaf_id !== null && hrm_pay_rule_003_store_row_matches($rows[$leaf_id], $binding, $actor_id))
            $return_id = $leaf_id;
        else
            $ok = false;
    } elseif ($ok && !$created_identity) {
        $ok = false;
    }

    if ($ok && $return_id === false) {
        $version_no = $max_version + 1;
        $sql = 'INSERT INTO '.TB_PREF.'hrm_pay_rule_artifact_versions '
            .'(artifact_id,version_no,predecessor_id,artifact_semver,custody_status,source_class,artifact_json,'
            .'artifact_sha256,plan_sha256,validator_contract,plan_contract,evidence_json,created_by) VALUES ('
            .(int)$artifact_id.','.$version_no.','
            .($predecessor_id === null ? 'NULL' : (int)$predecessor_id).','
            .db_escape($binding['artifact_semver']).','.db_escape($binding['custody_status']).','
            .db_escape($binding['source_class']).','.db_escape($binding['artifact_json']).','
            .db_escape($binding['artifact_sha256']).','.db_escape($binding['plan_sha256']).','
            .db_escape($binding['validator_contract']).','.db_escape($binding['plan_contract']).','
            .db_escape($binding['evidence_json']).','.(int)$actor_id.')';
        $ok = db_query($sql, 'could not append PAY-RULE-003 artifact version') !== false;
        if ($ok) {
            $return_id = (int)db_insert_id();
            if ($return_id <= 0)
                $ok = false;
        }
        if ($ok) {
            $audit = append_domain_audit_event(array(
                'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
                'legal_entity_id'=>0,
                'entity_type'=>'pay_rule_003_artifact',
                'entity_key'=>array('artifact_id'=>$artifact_id,'artifact_version_id'=>$return_id,'version_no'=>$version_no),
                'action'=>'pay.rule_003.artifact_version_appended',
                'reason_code'=>'pay_rule_003_artifact_store',
                'before'=>$predecessor_id === null ? null : array('predecessor_id'=>(int)$predecessor_id),
                'after'=>array('custody_status'=>$custody_status,'source_class'=>$source_class),
                'restricted_diff'=>array(
                    'artifact_sha256'=>$binding['artifact_sha256'],
                    'plan_sha256'=>$binding['plan_sha256'],
                    'validator_contract'=>$binding['validator_contract'],
                    'plan_contract'=>$binding['plan_contract'],
                    'production_publication_allowed'=>false
                )
            ));
            if ($audit === false)
                $ok = false;
        }
    }

    if ($manage_transaction) {
        if ($ok) commit_transaction(); else cancel_transaction();
    }
    return $ok ? $return_id : false;
}

/** Internal verified reader. Caller must already have authorized the operation. */
function hrm_pay_rule_003_read_artifact_version_internal($artifact_version_id, &$error)
{
    $error = null;
    if (!is_int($artifact_version_id) || $artifact_version_id <= 0) {
        $error = 'artifact_id_invalid';
        return false;
    }
    $result = db_query('SELECT a.artifact_key,a.artifact_type,v.* FROM '.TB_PREF.'hrm_pay_rule_artifact_versions v '
        .'JOIN '.TB_PREF.'hrm_pay_rule_artifacts a ON a.artifact_id=v.artifact_id '
        .'WHERE v.artifact_version_id='.(int)$artifact_version_id.' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1) {
        $error = 'artifact_not_found_or_ambiguous';
        return false;
    }
    $row = db_fetch_assoc($result);
    $artifact = json_decode($row['artifact_json'], true);
    $evidence = json_decode($row['evidence_json'], true);
    $binding = hrm_pay_rule_003_prepare_binding($artifact, $row['custody_status'], $row['source_class'], $evidence, $error);
    if ($binding === false)
        return false;
    foreach (array('artifact_key','artifact_type','artifact_semver','artifact_json','artifact_sha256',
                   'plan_sha256','validator_contract','plan_contract','evidence_json') as $key) {
        if ((string)$row[$key] !== (string)$binding[$key]) {
            $error = 'artifact_binding_drift';
            return false;
        }
    }
    return array(
        'artifact_version_id'=>(int)$row['artifact_version_id'],
        'artifact_id'=>(int)$row['artifact_id'],
        'version_no'=>(int)$row['version_no'],
        'predecessor_id'=>$row['predecessor_id'] === null ? null : (int)$row['predecessor_id'],
        'custody_status'=>$row['custody_status'],
        'source_class'=>$row['source_class'],
        'artifact'=>$artifact,
        'evidence'=>$evidence,
        'artifact_sha256'=>$row['artifact_sha256'],
        'plan_sha256'=>$row['plan_sha256']
    );
}

function hrm_pay_rule_003_read_artifact_version($artifact_version_id, &$error)
{
    if (!hrm_pay_rule_003_store_user_can('SA_HRM_VIEW_PAY_RULE_ARTIFACT')) {
        $error = 'unauthorized';
        return false;
    }
    return hrm_pay_rule_003_read_artifact_version_internal($artifact_version_id, $error);
}
