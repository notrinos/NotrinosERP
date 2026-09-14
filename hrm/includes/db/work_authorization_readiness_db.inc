<?php
/**
 * HRM-FND-007 Work Authorization readiness evaluator foundation.
 *
 * This is a read-only internal decision service. It is deliberately not wired
 * to Assignment, payroll, browser, import, export, or notification routes.
 * It never treats nationality, passport, a document type, or verification by
 * itself as legal/work eligibility.
 */
if (!function_exists('get_hrm_work_authorization_policy_as_of'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_policy_db.inc');
if (!function_exists('hrm_person_sensitive_decrypt'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');

/**
 * Decrypt and canonicalize one Work Authorization detail envelope.
 *
 * @param array $row
 * @return array|false
 */
function hrm_work_authorization_readiness_open_details($row)
{
    if (!is_array($row) || empty($row['worker_id']) || empty($row['version_no'])
        || empty($row['key_id']) || !isset($row['details_ciphertext']))
        return false;
    $key = hrm_person_sensitive_key((string)$row['key_id']);
    $context = hrm_work_authorization_crypto_context(
        (int)$row['worker_id'], (int)$row['version_no'],
        $row['predecessor_id'] === null ? null : (int)$row['predecessor_id']
    );
    if ($key === false || $context === false)
        return false;
    $plain = hrm_person_sensitive_decrypt($row['details_ciphertext'], $context, $key);
    $details = is_string($plain) ? json_decode($plain, true) : null;
    return is_array($details) ? hrm_work_authorization_canonical_details($details) : false;
}

/**
 * Return whether the linked evidence document still belongs to the Worker and
 * remains outside quarantine/migration-conflict custody.
 *
 * @param int $worker_id
 * @param int|null $document_id
 * @return bool
 */
function hrm_work_authorization_readiness_evidence_ok($worker_id, $document_id)
{
    $worker_id = (int)$worker_id;
    $document_id = $document_id === null ? 0 : (int)$document_id;
    if ($worker_id <= 0 || $document_id <= 0)
        return false;
    $r = db_query('SELECT d.doc_id,d.content_state FROM '.TB_PREF.'employee_documents d '
        .'INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.employee_id=d.employee_id '
        .'WHERE m.worker_id='.$worker_id.' AND m.retired_at IS NULL '
        .'AND d.doc_id='.$document_id.' ORDER BY m.mapping_id DESC LIMIT 2',
        'could not validate Work Authorization readiness evidence');
    if (!$r || db_num_rows($r) !== 1)
        return false;
    $row = db_fetch_assoc($r);
    return !in_array((string)$row['content_state'], array('quarantined','migration_conflict'), true);
}

/**
 * Evaluate one already-resolved current authorization leaf against one approved
 * policy. Any free-form occupation/location/employer restriction requires
 * qualified manual review and therefore cannot auto-pass this foundation.
 *
 * @param array $policy
 * @param array $authorization
 * @param array $details
 * @param string $as_of
 * @param bool $evidence_ok
 * @return array
 */
function hrm_work_authorization_evaluate_candidate($policy, $authorization, $details, $as_of, $evidence_ok)
{
    $deny = function ($decision) {
        return array('ready'=>false, 'decision'=>$decision, 'expiry_attention_required'=>false);
    };
    if (!is_array($policy) || !is_array($authorization) || !is_array($details)
        || !hrm_work_authorization_validate_date($as_of, false))
        return $deny('deny_invalid_custody');

    if (!isset($policy['policy_status']) || (string)$policy['policy_status'] !== 'approved'
        || !hrm_work_authorization_policy_validate_review_ref(
            isset($policy['qualified_review_ref']) ? $policy['qualified_review_ref'] : null)
        || empty($policy['approved_at']) || empty($policy['approved_by']))
        return $deny('deny_approved_policy_provenance_unavailable');

    $warning_days = isset($policy['expiry_warning_days']) ? (int)$policy['expiry_warning_days'] : -1;
    if ($warning_days < 0 || $warning_days > 365)
        return $deny('deny_invalid_policy_custody');

    $policy_types = isset($policy['authorization_types']) && is_array($policy['authorization_types'])
        ? $policy['authorization_types']
        : hrm_work_authorization_policy_canonical_types(
            isset($policy['authorization_types_json']) ? $policy['authorization_types_json'] : null);
    if ($policy_types === false || empty($policy['issuing_jurisdiction'])
        || empty($authorization['issuing_jurisdiction'])
        || (string)$policy['issuing_jurisdiction'] !== (string)$authorization['issuing_jurisdiction'])
        return $deny('deny_policy_mismatch');
    if (!in_array((string)$authorization['authorization_type'], $policy_types, true))
        return $deny('deny_authorization_type_not_approved');
    if ((string)$authorization['authorization_status'] !== 'active')
        return $deny('deny_authorization_not_active');
    if ((string)$authorization['verification_status'] !== 'verified')
        return $deny('deny_authorization_not_verified');
    if (!$evidence_ok)
        return $deny('deny_governed_evidence_unavailable');
    if (!hrm_work_authorization_validate_date(
            isset($authorization['valid_from']) ? $authorization['valid_from'] : null, false)
        || !hrm_work_authorization_validate_date(
            isset($authorization['valid_to']) ? $authorization['valid_to'] : null, true)
        || (!empty($authorization['valid_to'])
            && $authorization['valid_to'] < $authorization['valid_from']))
        return $deny('deny_invalid_authorization_dates');
    if ($as_of < $authorization['valid_from'])
        return $deny('deny_not_yet_valid');
    if (!empty($authorization['valid_to']) && $as_of > $authorization['valid_to'])
        return $deny('deny_expired');

    foreach (array('occupation_restriction','location_restriction','employer_restriction') as $field)
        if (!empty($details[$field]))
            return $deny('manual_review_restrictions_present');

    $expiry_attention = false;
    if (!empty($authorization['valid_to']) && $warning_days > 0) {
        $start = new DateTime($as_of.' 00:00:00');
        $end = new DateTime($authorization['valid_to'].' 00:00:00');
        $days = (int)$start->diff($end)->format('%r%a');
        $expiry_attention = $days >= 0 && $days <= $warning_days;
    }

    return array(
        'ready'=>true,
        'decision'=>'ready_under_approved_policy',
        'expiry_attention_required'=>$expiry_attention
    );
}

/**
 * Resolve a minimal Worker readiness decision under one approved policy pack.
 *
 * The result exposes only decision/evidence identifiers and expiry metadata;
 * decrypted sponsor/restriction text and permit identifiers are never returned.
 *
 * @param int $worker_id
 * @param string $jurisdiction
 * @param string $as_of SQL date
 * @return array
 */
function get_hrm_work_authorization_readiness_as_of($worker_id, $jurisdiction, $as_of)
{
    $worker_id = (int)$worker_id;
    if ($worker_id <= 0 || !hrm_work_authorization_validate_jurisdiction($jurisdiction)
        || !hrm_work_authorization_validate_date($as_of, false))
        return array('ready'=>false,'decision'=>'deny_invalid_request');

    $policy = get_hrm_work_authorization_policy_as_of($jurisdiction, $as_of);
    if (!is_array($policy))
        return array('ready'=>false,'decision'=>'deny_approved_policy_unavailable');

    $sql = 'SELECT a.authorization_id,a.worker_id,a.version_no,a.predecessor_id,'
        .'a.authorization_type,a.issuing_jurisdiction,a.authorization_status,'
        .'a.valid_from,a.valid_to,a.key_id,a.details_ciphertext,a.verification_status,'
        .'a.verified_at,a.verified_by,a.employee_document_id '
        .'FROM '.TB_PREF.'hrm_work_authorizations a '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorizations s '
        .'ON s.predecessor_id=a.authorization_id '
        .'WHERE a.worker_id='.$worker_id.' AND s.authorization_id IS NULL '
        .'AND a.issuing_jurisdiction='.db_escape($jurisdiction)
        .' ORDER BY a.authorization_type,a.authorization_id';
    $r = db_query($sql, 'could not resolve current Work Authorization readiness leaves');
    if (!$r)
        return array('ready'=>false,'decision'=>'deny_authorization_custody_unavailable');

    $seen = array(); $ready = array(); $denials = array();
    while ($row = db_fetch_assoc($r)) {
        $chain = (string)$row['authorization_type'].'|'.(string)$row['issuing_jurisdiction'];
        if (isset($seen[$chain]))
            return array('ready'=>false,'decision'=>'deny_ambiguous_current_authorization');
        $seen[$chain] = true;
        if (!in_array((string)$row['authorization_type'], $policy['authorization_types'], true))
            continue;

        $details = hrm_work_authorization_readiness_open_details($row);
        if ($details === false) {
            $denials[] = 'deny_sensitive_custody_unavailable';
            continue;
        }
        $evidence_ok = hrm_work_authorization_readiness_evidence_ok(
            $worker_id, $row['employee_document_id'] === null ? null : (int)$row['employee_document_id']);
        $decision = hrm_work_authorization_evaluate_candidate($policy, $row, $details, $as_of, $evidence_ok);
        if (!empty($decision['ready'])) {
            $ready[] = array(
                'authorization_id'=>(int)$row['authorization_id'],
                'valid_to'=>$row['valid_to'] === null ? null : (string)$row['valid_to'],
                'expiry_attention_required'=>!empty($decision['expiry_attention_required'])
            );
        } else {
            $denials[] = isset($decision['decision']) ? (string)$decision['decision'] : 'deny_invalid_custody';
        }
    }

    if (count($ready) < 1)
        return array(
            'ready'=>false,
            'decision'=>count($denials) ? $denials[0] : 'deny_no_approved_authorization',
            'policy_version_id'=>(int)$policy['policy_version_id'],
            'policy_code'=>(string)$policy['policy_code']
        );

    usort($ready, function ($a, $b) {
        return (int)$a['authorization_id'] - (int)$b['authorization_id'];
    });
    $chosen = $ready[0];
    return array(
        'ready'=>true,
        'decision'=>'ready_under_approved_policy',
        'policy_version_id'=>(int)$policy['policy_version_id'],
        'policy_code'=>(string)$policy['policy_code'],
        'authorization_id'=>(int)$chosen['authorization_id'],
        'valid_to'=>$chosen['valid_to'],
        'expiry_attention_required'=>!empty($chosen['expiry_attention_required'])
    );
}
