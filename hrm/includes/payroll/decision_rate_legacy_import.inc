<?php
/** PAY-RULE-003 read-only legacy inventory and quarantined snapshot custody. */
include_once(__DIR__.'/decision_rate_artifact_store.inc');

if (!defined('HRM_PAY_RULE_003_LEGACY_IMPORT_VERSION'))
    define('HRM_PAY_RULE_003_LEGACY_IMPORT_VERSION', '1.0.480');

function hrm_pay_rule_003_legacy_requirements($kind)
{
    if ($kind === 'tax_brackets')
        return array('jurisdiction','currency','period_basis','rounding','authority_source_ids','rate_set_revision','qualified_review');
    if ($kind === 'statutory_deductions')
        return array('jurisdiction','currency_or_unit','liability_semantics','period_basis','rounding','authority_source_ids','ordering','qualified_review');
    return false;
}

function hrm_pay_rule_003_legacy_source_definition($kind)
{
    if ($kind === 'tax_brackets')
        return array('id'=>'bracket_id','columns'=>'bracket_id,bracket_name,from_amount,to_amount,rate,fixed_amount,effective_from,effective_to');
    if ($kind === 'statutory_deductions')
        return array('id'=>'statutory_id','columns'=>'statutory_id,statutory_name,statutory_code,employee_rate,employer_rate,employee_fixed,employer_fixed,ceiling_amount,floor_amount,calculation_base,employee_account,employer_account,effective_from,effective_to,inactive');
    return false;
}

function hrm_pay_rule_003_legacy_snapshot($kind, $row, &$error)
{
    $error = null;
    $requirements = hrm_pay_rule_003_legacy_requirements($kind);
    $definition = hrm_pay_rule_003_legacy_source_definition($kind);
    if ($requirements === false || $definition === false || !is_array($row)) {
        $error = 'legacy_source_invalid';
        return false;
    }
    $idfield = $definition['id'];
    if (!isset($row[$idfield]) || preg_match('/^[1-9][0-9]*$/D', (string)$row[$idfield]) !== 1) {
        $error = 'legacy_source_id_invalid';
        return false;
    }

    $snapshot = array();
    foreach ($row as $key=>$value) {
        if (is_float($value)) {
            $error = 'legacy_binary_float_not_accepted';
            return false;
        }
        if (!is_scalar($value) && $value !== null) {
            $error = 'legacy_value_invalid';
            return false;
        }
        $snapshot[(string)$key] = $value === null ? null : (string)$value;
    }
    ksort($snapshot, SORT_STRING);
    $json = hrm_pay_rule_003_canonical_json($snapshot);
    if ($json === false) {
        $error = 'legacy_snapshot_encode_failed';
        return false;
    }
    return array(
        'source_kind'=>$kind,
        'source_row_id'=>(int)$row[$idfield],
        'source_snapshot'=>$snapshot,
        'source_snapshot_json'=>$json,
        'source_snapshot_sha256'=>hash('sha256', 'pay-rule-003-legacy-snapshot-v1|'.$kind.'|'.$json),
        'diagnostics'=>array(
            'status'=>'quarantined_unverified',
            'missing_required_metadata'=>$requirements,
            'automatic_certification'=>false,
            'production_eligible'=>false
        )
    );
}

function hrm_pay_rule_003_load_legacy_source_row($kind, $source_row_id, $for_update=false)
{
    $definition = hrm_pay_rule_003_legacy_source_definition($kind);
    if ($definition === false || !is_int($source_row_id) || $source_row_id <= 0)
        return false;
    $sql = 'SELECT '.$definition['columns'].' FROM '.TB_PREF.$kind
        .' WHERE '.$definition['id'].'='.(int)$source_row_id.' LIMIT 2'.($for_update ? ' FOR UPDATE' : '');
    $result = db_query($sql, 'could not read PAY-RULE-003 legacy source');
    if (!$result || db_num_rows($result) !== 1)
        return false;
    return db_fetch_assoc($result);
}

/** Re-read one legacy row and prove it still matches the quarantined candidate. */
function hrm_pay_rule_003_reconcile_legacy_candidate($candidate, $for_update, &$error)
{
    $error = null;
    if (!is_array($candidate)
        || !isset($candidate['source_kind'],$candidate['source_row_id'],$candidate['source_snapshot_json'],$candidate['source_snapshot_sha256'])
        || !in_array($candidate['source_kind'], array('tax_brackets','statutory_deductions'), true)
        || !is_numeric($candidate['source_row_id']) || (int)$candidate['source_row_id'] <= 0
        || preg_match('/^[a-f0-9]{64}$/D', (string)$candidate['source_snapshot_sha256']) !== 1) {
        $error = 'candidate_source_binding_invalid';
        return false;
    }
    $row = hrm_pay_rule_003_load_legacy_source_row($candidate['source_kind'], (int)$candidate['source_row_id'], (bool)$for_update);
    if ($row === false) {
        $error = 'legacy_source_missing_or_ambiguous';
        return false;
    }
    $current = hrm_pay_rule_003_legacy_snapshot($candidate['source_kind'], $row, $error);
    if ($current === false)
        return false;
    if (!hash_equals((string)$candidate['source_snapshot_sha256'], (string)$current['source_snapshot_sha256'])
        || (string)$candidate['source_snapshot_json'] !== (string)$current['source_snapshot_json']) {
        $error = 'legacy_source_snapshot_stale';
        return false;
    }
    return $current;
}

function hrm_pay_rule_003_inventory_legacy_source($kind)
{
    if (!hrm_pay_rule_003_store_user_can('SA_HRM_MANAGE_PAY_RULE_ARTIFACT'))
        return false;
    $definition = hrm_pay_rule_003_legacy_source_definition($kind);
    if ($definition === false)
        return false;
    $result = db_query('SELECT '.$definition['columns'].' FROM '.TB_PREF.$kind.' ORDER BY '.$definition['id'],
        'could not inventory PAY-RULE-003 legacy source');
    if (!$result)
        return false;
    $out = array();
    while ($row = db_fetch_assoc($result)) {
        $error = null;
        $snapshot = hrm_pay_rule_003_legacy_snapshot($kind, $row, $error);
        if ($snapshot === false)
            return false;
        $out[] = $snapshot;
    }
    return $out;
}

function hrm_pay_rule_003_persist_legacy_candidate($snapshot)
{
    global $path_to_root;
    if (!function_exists('append_domain_audit_event'))
        include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!is_array($snapshot)
        || !isset($snapshot['source_kind'],$snapshot['source_row_id'],$snapshot['source_snapshot_json'],
                  $snapshot['source_snapshot_sha256'],$snapshot['diagnostics'])
        || !hrm_pay_rule_003_store_user_can('SA_HRM_MANAGE_PAY_RULE_ARTIFACT'))
        return false;

    $actor = hrm_pay_rule_003_store_actor_id();
    if ($actor <= 0)
        return false;
    $diagnostics_json = hrm_pay_rule_003_canonical_json($snapshot['diagnostics']);
    if ($diagnostics_json === false)
        return false;

    begin_transaction();
    $ok = true;
    $error = null;
    $current = hrm_pay_rule_003_reconcile_legacy_candidate($snapshot, true, $error);
    if ($current === false)
        $ok = false;
    if ($ok) {
        $expected_diag = hrm_pay_rule_003_canonical_json($current['diagnostics']);
        if ($expected_diag === false || $expected_diag !== $diagnostics_json)
            $ok = false;
    }

    $candidate_id = false;
    if ($ok) {
        $result = db_query('SELECT candidate_id,source_snapshot_json,diagnostics_json,candidate_status,created_by '
            .'FROM '.TB_PREF.'hrm_pay_rule_import_candidates WHERE source_kind='.db_escape($snapshot['source_kind'])
            .' AND source_row_id='.(int)$snapshot['source_row_id']
            .' AND source_snapshot_sha256='.db_escape($snapshot['source_snapshot_sha256']).' LIMIT 2 FOR UPDATE');
        $ok = $result && db_num_rows($result) <= 1;
        if ($ok && db_num_rows($result) === 1) {
            $row = db_fetch_assoc($result);
            $ok = (string)$row['source_snapshot_json'] === (string)$snapshot['source_snapshot_json']
                && (string)$row['diagnostics_json'] === $diagnostics_json
                && (string)$row['candidate_status'] === 'quarantined_unverified'
                && (int)$row['created_by'] === $actor;
            if ($ok)
                $candidate_id = (int)$row['candidate_id'];
        } elseif ($ok) {
            $ok = db_query('INSERT INTO '.TB_PREF.'hrm_pay_rule_import_candidates '
                .'(source_kind,source_row_id,source_snapshot_json,source_snapshot_sha256,diagnostics_json,candidate_status,created_by) VALUES ('
                .db_escape($snapshot['source_kind']).','.(int)$snapshot['source_row_id'].','
                .db_escape($snapshot['source_snapshot_json']).','.db_escape($snapshot['source_snapshot_sha256']).','
                .db_escape($diagnostics_json).",'quarantined_unverified',".(int)$actor.')') !== false;
            if ($ok) {
                $candidate_id = (int)db_insert_id();
                if ($candidate_id <= 0)
                    $ok = false;
            }
            if ($ok) {
                $audit = append_domain_audit_event(array(
                    'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
                    'legal_entity_id'=>0,
                    'entity_type'=>'pay_rule_003_import_candidate',
                    'entity_key'=>array('candidate_id'=>$candidate_id,'source_kind'=>$snapshot['source_kind'],
                                        'source_row_id'=>(int)$snapshot['source_row_id']),
                    'action'=>'pay.rule_003.legacy_snapshot_quarantined',
                    'reason_code'=>'pay_rule_003_legacy_import',
                    'before'=>null,
                    'after'=>array('candidate_status'=>'quarantined_unverified'),
                    'restricted_diff'=>array(
                        'source_snapshot_sha256'=>$snapshot['source_snapshot_sha256'],
                        'source_reconciled_at_insert'=>true,
                        'automatic_certification'=>false,
                        'production_eligible'=>false
                    )
                ));
                if ($audit === false)
                    $ok = false;
            }
        }
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $candidate_id : false;
}
