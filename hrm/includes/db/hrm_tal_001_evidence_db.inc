<?php
/** HRM-TAL-001 immutable employee skill/certification evidence writer. */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_catalog_db.inc');

/** @return bool */
function hrm_tal_001_evidence_writer_ready(&$error)
{
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_evidence_writer_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'evidence_writer_not_active');
    return hrm_tal_001_catalog_writer_ready($error);
}

/** @return bool */
function hrm_tal_001_employee_exists($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '') return false;
    $result = db_query('SELECT employee_id FROM '.TB_PREF.'employees WHERE employee_id='.db_escape($employee_id).' LIMIT 2');
    return $result && db_num_rows($result) === 1;
}

/** @return string|null|false */
function hrm_tal_001_decimal_or_null($value)
{
    if ($value === null || $value === '') return null;
    $value = trim((string)$value);
    if (!preg_match('/^-?(?:0|[1-9]\\d{0,5})(?:\\.\\d{1,4})?$/D', $value)) return false;
    $negative = isset($value[0]) && $value[0] === '-';
    if ($negative) $value = substr($value, 1);
    $parts = explode('.', $value, 2);
    $whole = ltrim($parts[0], '0'); if ($whole === '') $whole = '0';
    $fraction = isset($parts[1]) ? rtrim($parts[1], '0') : '';
    $out = $whole.($fraction === '' ? '' : '.'.$fraction);
    return $negative && $out !== '0' ? '-'.$out : $out;
}

/** @return int */
function hrm_tal_001_decimal_compare($a, $b)
{
    $fa = (float)$a; $fb = (float)$b;
    if ($fa < $fb) return -1;
    if ($fa > $fb) return 1;
    return 0;
}

/** @return array|false */
function hrm_tal_001_skill_definition_for_evidence($id, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_tal_001_skill_definitions WHERE skill_definition_id='.(int)$id.' AND company_id='.(int)$company.' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1) return hrm_tal_001_catalog_fail($error, 'skill_definition_missing_or_cross_company');
    return db_fetch_assoc($result);
}

/** @return array|false */
function hrm_tal_001_certification_definition_for_evidence($id, &$error)
{
    $company = hrm_tal_001_runtime_company_id();
    $result = db_query('SELECT * FROM '.TB_PREF.'hrm_tal_001_certification_definitions WHERE certification_definition_id='.(int)$id.' AND company_id='.(int)$company.' LIMIT 2');
    if (!$result || db_num_rows($result) !== 1) return hrm_tal_001_catalog_fail($error, 'certification_definition_missing_or_cross_company');
    return db_fetch_assoc($result);
}

/** @return array|false */
function hrm_tal_001_record_skill_evidence(array $x, &$error = null)
{
    $error = null;
    if (!hrm_tal_001_evidence_writer_ready($error)) return false;
    foreach (array('skill_definition_id','employee_id','evidence_type_code','source_reference_sha256','evidence_date','valid_from') as $key)
        if (!array_key_exists($key, $x)) return hrm_tal_001_catalog_fail($error, 'skill_evidence_field_missing');
    $definition_id = (int)$x['skill_definition_id'];
    $employee = trim((string)$x['employee_id']);
    $type = hrm_tal_001_token($x['evidence_type_code'], 32);
    $source = hrm_tal_001_sha256($x['source_reference_sha256']);
    $date = hrm_tal_001_date($x['evidence_date']);
    $from = hrm_tal_001_date($x['valid_from']);
    $to = hrm_tal_001_optional_date(isset($x['valid_to']) ? $x['valid_to'] : null);
    $proficiency = hrm_tal_001_decimal_or_null(isset($x['proficiency_value']) ? $x['proficiency_value'] : null);
    if ($definition_id <= 0 || $employee === '' || strlen($employee) > 20 || $type === false
        || !in_array($type, array('assessment','credential','manager_attestation','employee_declaration'), true)
        || $source === false || $date === false || $from === false || ($to !== null && ($to === false || $to < $from)) || $proficiency === false
        || !hrm_tal_001_employee_exists($employee))
        return hrm_tal_001_catalog_fail($error, 'skill_evidence_invalid');
    $definition = hrm_tal_001_skill_definition_for_evidence($definition_id, $error); if ($definition === false) return false;
    if ($date < $definition['effective_from'] || (!empty($definition['effective_to']) && $date > $definition['effective_to'])
        || $from < $definition['effective_from'] || (!empty($definition['effective_to']) && $from > $definition['effective_to'])
        || ($to !== null && (!empty($definition['effective_to']) && $to > $definition['effective_to'])))
        return hrm_tal_001_catalog_fail($error, 'skill_evidence_outside_definition_window');
    $scale = (string)$definition['proficiency_scale_code'];
    if (($scale === 'none' && $proficiency !== null)
        || ($scale === 'level_1_5' && ($proficiency === null || hrm_tal_001_decimal_compare($proficiency, '1') < 0 || hrm_tal_001_decimal_compare($proficiency, '5') > 0))
        || ($scale === 'percent_0_100' && ($proficiency === null || hrm_tal_001_decimal_compare($proficiency, '0') < 0 || hrm_tal_001_decimal_compare($proficiency, '100') > 0)))
        return hrm_tal_001_catalog_fail($error, 'skill_proficiency_invalid_for_scale');
    $company = hrm_tal_001_runtime_company_id(); $actor = hrm_tal_001_runtime_actor_id();
    $payload = array('company_id'=>$company,'skill_definition_id'=>$definition_id,'definition_sha256'=>(string)$definition['definition_sha256'],
        'employee_id'=>$employee,'evidence_type_code'=>$type,'source_reference_sha256'=>$source,'proficiency_value'=>$proficiency,
        'evidence_date'=>$date,'valid_from'=>$from,'valid_to'=>$to);
    $sha = hrm_tal_001_payload_sha256($payload);
    $existing = db_query('SELECT employee_skill_evidence_id,evidence_sha256 FROM '.TB_PREF.'hrm_tal_001_employee_skill_evidence WHERE evidence_sha256='.db_escape($sha).' LIMIT 2');
    if (!$existing || db_num_rows($existing) > 1) return hrm_tal_001_catalog_fail($error, 'skill_evidence_lookup_failed');
    if (db_num_rows($existing) === 1) { $row=db_fetch_assoc($existing); return array('employee_skill_evidence_id'=>(int)$row['employee_skill_evidence_id'],'evidence_sha256'=>$sha,'idempotent'=>true); }
    $sql='INSERT INTO '.TB_PREF.'hrm_tal_001_employee_skill_evidence (company_id,skill_definition_id,employee_id,evidence_type_code,source_reference_sha256,proficiency_value,evidence_date,valid_from,valid_to,evidence_sha256,recorded_by_user_id,created_at) VALUES ('
        .$company.','.$definition_id.','.db_escape($employee).','.db_escape($type).','.db_escape($source).','.($proficiency===null?'NULL':db_escape($proficiency)).','.db_escape($date).','.db_escape($from).','.($to===null?'NULL':db_escape($to)).','.db_escape($sha).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if (!db_query($sql)) return hrm_tal_001_catalog_fail($error, 'skill_evidence_insert_failed');
    return array('employee_skill_evidence_id'=>(int)db_insert_id(),'evidence_sha256'=>$sha,'idempotent'=>false);
}

/** @return array|false */
function hrm_tal_001_record_certification_evidence(array $x, &$error = null)
{
    $error = null;
    if (!hrm_tal_001_evidence_writer_ready($error)) return false;
    foreach (array('certification_definition_id','employee_id','credential_reference_sha256','issued_on','verification_status_code','verification_evidence_sha256') as $key)
        if (!array_key_exists($key, $x)) return hrm_tal_001_catalog_fail($error, 'certification_evidence_field_missing');
    $definition_id=(int)$x['certification_definition_id']; $employee=trim((string)$x['employee_id']);
    $credential=hrm_tal_001_sha256($x['credential_reference_sha256']); $issued=hrm_tal_001_date($x['issued_on']);
    $expires=hrm_tal_001_optional_date(isset($x['expires_on'])?$x['expires_on']:null); $status=hrm_tal_001_token($x['verification_status_code'],32);
    $verification=hrm_tal_001_sha256($x['verification_evidence_sha256']);
    if ($definition_id<=0||$employee===''||strlen($employee)>20||$credential===false||$issued===false||($expires!==null&&($expires===false||$expires<$issued))
        ||$status===false||!in_array($status,array('unverified','verified','rejected'),true)||$verification===false||!hrm_tal_001_employee_exists($employee))
        return hrm_tal_001_catalog_fail($error,'certification_evidence_invalid');
    $definition=hrm_tal_001_certification_definition_for_evidence($definition_id,$error); if($definition===false)return false;
    if($issued<$definition['effective_from']||(!empty($definition['effective_to'])&&$issued>$definition['effective_to']))return hrm_tal_001_catalog_fail($error,'certification_evidence_outside_definition_window');
    $policy=(string)$definition['expiry_policy_code'];
    if($policy==='none'&&$expires!==null)return hrm_tal_001_catalog_fail($error,'certification_expiry_not_allowed');
    if($policy==='explicit_on_evidence'&&$expires===null)return hrm_tal_001_catalog_fail($error,'certification_expiry_required');
    if($policy==='fixed_days'){
        $expected=date('Y-m-d',strtotime($issued.' +'.(int)$definition['default_validity_days'].' days'));
        if($expires!==$expected)return hrm_tal_001_catalog_fail($error,'certification_fixed_expiry_mismatch');
    }
    $company=hrm_tal_001_runtime_company_id();$actor=hrm_tal_001_runtime_actor_id();
    $payload=array('company_id'=>$company,'certification_definition_id'=>$definition_id,'definition_sha256'=>(string)$definition['definition_sha256'],
        'employee_id'=>$employee,'credential_reference_sha256'=>$credential,'issued_on'=>$issued,'expires_on'=>$expires,
        'verification_status_code'=>$status,'verification_evidence_sha256'=>$verification);
    $sha=hrm_tal_001_payload_sha256($payload);
    $existing=db_query('SELECT employee_certification_evidence_id,evidence_sha256 FROM '.TB_PREF.'hrm_tal_001_employee_certification_evidence WHERE evidence_sha256='.db_escape($sha).' LIMIT 2');
    if(!$existing||db_num_rows($existing)>1)return hrm_tal_001_catalog_fail($error,'certification_evidence_lookup_failed');
    if(db_num_rows($existing)===1){$row=db_fetch_assoc($existing);return array('employee_certification_evidence_id'=>(int)$row['employee_certification_evidence_id'],'evidence_sha256'=>$sha,'idempotent'=>true);}
    $sql='INSERT INTO '.TB_PREF.'hrm_tal_001_employee_certification_evidence (company_id,certification_definition_id,employee_id,credential_reference_sha256,issued_on,expires_on,verification_status_code,verification_evidence_sha256,evidence_sha256,recorded_by_user_id,created_at) VALUES ('
        .$company.','.$definition_id.','.db_escape($employee).','.db_escape($credential).','.db_escape($issued).','.($expires===null?'NULL':db_escape($expires)).','.db_escape($status).','.db_escape($verification).','.db_escape($sha).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';
    if(!db_query($sql))return hrm_tal_001_catalog_fail($error,'certification_evidence_insert_failed');
    return array('employee_certification_evidence_id'=>(int)db_insert_id(),'evidence_sha256'=>$sha,'idempotent'=>false);
}
?>
