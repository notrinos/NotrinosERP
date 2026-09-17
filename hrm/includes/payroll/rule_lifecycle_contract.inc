<?php
/**
 * PAY-RULE-004 pure rule lifecycle/regulatory evidence contract.
 *
 * No database, session, file, network, payroll routing or publication side
 * effects are permitted in this module.
 */
if (!defined('HRM_PAY_RULE_004_LIFECYCLE_CONTRACT_VERSION'))
    define('HRM_PAY_RULE_004_LIFECYCLE_CONTRACT_VERSION', '1.0.487');
if (!defined('HRM_PAY_RULE_004_PRODUCTION_ACTIVATION_ALLOWED'))
    define('HRM_PAY_RULE_004_PRODUCTION_ACTIVATION_ALLOWED', false);

function hrm_pay_rule_004_ascii_token($value, $max=96)
{
    if (!is_string($value)) return false;
    $value = trim($value);
    if ($value === '' || strlen($value) > $max) return false;
    return preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]*$/D', $value) ? $value : false;
}

function hrm_pay_rule_004_text($value, $max=512)
{
    if (!is_string($value)) return false;
    $value = trim($value);
    if ($value === '' || strlen($value) > $max || preg_match('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/', $value))
        return false;
    return $value;
}

function hrm_pay_rule_004_date($value)
{
    if (!is_string($value) || !preg_match('/^(\d{4})-(\d{2})-(\d{2})$/D', $value, $m)) return false;
    return checkdate((int)$m[2], (int)$m[3], (int)$m[1]) ? $value : false;
}

function hrm_pay_rule_004_utc($value)
{
    if (!is_string($value) || !preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/D', $value)) return false;
    $t = strtotime($value);
    return $t !== false && gmdate('Y-m-d\TH:i:s\Z', $t) === $value ? $value : false;
}

function hrm_pay_rule_004_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) ? $value : false;
}

function hrm_pay_rule_004_validate_regulatory_evidence($evidence, &$error=null)
{
    $error = null;
    if (!is_array($evidence)) { $error='evidence_not_array'; return false; }
    $required = array('jurisdiction_token','legal_reference','source_evidence_ref','source_sha256','effective_from','effective_to','captured_at');
    foreach ($required as $key) if (!array_key_exists($key, $evidence)) { $error='evidence_missing_'.$key; return false; }
    $jurisdiction = hrm_pay_rule_004_ascii_token($evidence['jurisdiction_token'], 96);
    $legal = hrm_pay_rule_004_text($evidence['legal_reference'], 512);
    $source_ref = hrm_pay_rule_004_text($evidence['source_evidence_ref'], 256);
    $source_hash = hrm_pay_rule_004_sha256($evidence['source_sha256']);
    $from = hrm_pay_rule_004_date($evidence['effective_from']);
    $to = hrm_pay_rule_004_date($evidence['effective_to']);
    $captured = hrm_pay_rule_004_utc($evidence['captured_at']);
    if ($jurisdiction === false) { $error='jurisdiction_invalid'; return false; }
    if ($legal === false) { $error='legal_reference_invalid'; return false; }
    if ($source_ref === false) { $error='source_evidence_ref_invalid'; return false; }
    if ($source_hash === false) { $error='source_sha256_invalid'; return false; }
    if ($from === false || $to === false || strcmp($from, $to) > 0) { $error='effective_interval_invalid'; return false; }
    if ($captured === false) { $error='captured_at_invalid'; return false; }
    return array(
        'jurisdiction_token'=>$jurisdiction,
        'legal_reference'=>$legal,
        'source_evidence_ref'=>$source_ref,
        'source_sha256'=>$source_hash,
        'effective_from'=>$from,
        'effective_to'=>$to,
        'captured_at'=>$captured
    );
}

function hrm_pay_rule_004_validate_impact_preview($preview, $artifact_sha256, $plan_sha256, &$error=null)
{
    $error = null;
    if (!is_array($preview)) { $error='preview_not_array'; return false; }
    foreach (array('artifact_sha256','plan_sha256','preview_sha256','scope_token','generated_at') as $key)
        if (!array_key_exists($key, $preview)) { $error='preview_missing_'.$key; return false; }
    if (hrm_pay_rule_004_sha256($artifact_sha256) === false || hrm_pay_rule_004_sha256($plan_sha256) === false
        || $preview['artifact_sha256'] !== $artifact_sha256 || $preview['plan_sha256'] !== $plan_sha256) {
        $error='preview_artifact_binding_mismatch'; return false;
    }
    if (hrm_pay_rule_004_sha256($preview['preview_sha256']) === false) { $error='preview_sha256_invalid'; return false; }
    if (hrm_pay_rule_004_ascii_token($preview['scope_token'], 96) === false) { $error='preview_scope_invalid'; return false; }
    if (hrm_pay_rule_004_utc($preview['generated_at']) === false) { $error='preview_generated_at_invalid'; return false; }
    return true;
}

function hrm_pay_rule_004_validate_rule_version_envelope($rule, &$error=null)
{
    $error = null;
    if (!is_array($rule)) { $error='rule_not_array'; return false; }
    foreach (array('rule_key','rule_type','version_label','artifact_version_id','artifact_sha256','plan_sha256','author_id','regulatory_evidence') as $key)
        if (!array_key_exists($key, $rule)) { $error='rule_missing_'.$key; return false; }
    $key = hrm_pay_rule_004_ascii_token($rule['rule_key'], 160);
    $type = hrm_pay_rule_004_ascii_token($rule['rule_type'], 32);
    $label = hrm_pay_rule_004_ascii_token($rule['version_label'], 64);
    if ($key === false || !in_array($type, array('decision_table','rate_table'), true) || $label === false) { $error='rule_identity_invalid'; return false; }
    if (!is_int($rule['artifact_version_id']) || $rule['artifact_version_id'] <= 0 || !is_int($rule['author_id']) || $rule['author_id'] <= 0) { $error='rule_actor_or_artifact_invalid'; return false; }
    if (hrm_pay_rule_004_sha256($rule['artifact_sha256']) === false || hrm_pay_rule_004_sha256($rule['plan_sha256']) === false) { $error='rule_hash_invalid'; return false; }
    $evidence = hrm_pay_rule_004_validate_regulatory_evidence($rule['regulatory_evidence'], $error);
    if ($evidence === false) return false;
    return array(
        'rule_key'=>$key,'rule_type'=>$type,'version_label'=>$label,
        'artifact_version_id'=>$rule['artifact_version_id'],'artifact_sha256'=>$rule['artifact_sha256'],
        'plan_sha256'=>$rule['plan_sha256'],'author_id'=>$rule['author_id'],'regulatory_evidence'=>$evidence
    );
}

function hrm_pay_rule_004_transition_allowed($from, $to)
{
    $map = array(
        'draft'=>array('published'),
        'published'=>array('withdrawn','superseded'),
        'withdrawn'=>array(),
        'superseded'=>array()
    );
    return isset($map[$from]) && in_array($to, $map[$from], true);
}

function hrm_pay_rule_004_validate_publication_roles($author_id, $reviewer_id, $publisher_id, &$error=null)
{
    $error = null;
    foreach (array($author_id,$reviewer_id,$publisher_id) as $id)
        if (!is_int($id) || $id <= 0) { $error='publication_actor_invalid'; return false; }
    if ($author_id === $reviewer_id || $author_id === $publisher_id || $reviewer_id === $publisher_id) {
        $error='publication_sod_violation'; return false;
    }
    return true;
}

function hrm_pay_rule_004_effective_intervals_overlap($a_from, $a_to, $b_from, $b_to)
{
    foreach (array($a_from,$a_to,$b_from,$b_to) as $d) if (hrm_pay_rule_004_date($d) === false) return null;
    if (strcmp($a_from,$a_to)>0 || strcmp($b_from,$b_to)>0) return null;
    return strcmp($a_from,$b_to) <= 0 && strcmp($b_from,$a_to) <= 0;
}
