<?php
/**
 * Internal FND-006 compatibility projector. Existing pages retain their field
 * and object gates. This service exposes no public full-profile read endpoint.
 * Legacy identifiers and bank columns are pre-governance compatibility facts;
 * governed successors remain exclusively under their FND-004 command/election.
 */
include_once(dirname(__FILE__).'/employee_compatibility_profile_db.inc');
include_once(dirname(__FILE__).'/employee_employment_db.inc');
include_once(dirname(__FILE__).'/employee_contract_db.inc');
include_once(dirname(__FILE__).'/employee_assignment_db.inc');
include_once(dirname(__FILE__).'/employee_person_bank_db.inc');
include_once(dirname(__FILE__).'/employee_person_private_profile_db.inc');

/** Fetch a singular result, rejecting ambiguous identity/version evidence. */
function hrm_employee_projection_one($sql, $optional = false)
{
    $result = db_query($sql, 'could not resolve employee compatibility source');
    if (!$result || db_num_rows($result) > 1) return false;
    if (db_num_rows($result) === 0) return $optional ? array() : false;
    return db_fetch_assoc($result);
}

/** Select a bounded current/as-of interval with half-open timestamp semantics. */
function hrm_employee_projection_interval($as_of)
{
    return $as_of === null ? 'effective_to IS NULL'
        : '(effective_from IS NULL OR effective_from<='.db_escape($as_of).') AND '
            .'(effective_to IS NULL OR effective_to>'.db_escape($as_of).')';
}

/** Resolve protected compatibility fields internally, without widening reveal or payment authority. */
function hrm_employee_projection_private($person_id, $as_of = null)
{
    $values = array();
    foreach (array('contact'=>hrm_person_contact_field_map(), 'identifier'=>hrm_person_identifier_field_map()) as $class=>$fields) {
        $table = $class === 'contact' ? 'hrm_person_contacts' : 'hrm_person_identifiers';
        $id = $class === 'contact' ? 'contact_id' : 'identifier_id';
        foreach ($fields as $type=>$field) {
            $where = 'person_id='.(int)$person_id.' AND '.$class.'_type='.db_escape($type);
            $row = hrm_employee_projection_one('SELECT * FROM '.TB_PREF.$table.' WHERE '.$where
                .' AND '.hrm_employee_projection_interval($as_of).' LIMIT 2', true);
            if ($row === false) return false;
            // Governed identifier supersession does not authorize copying new plaintext
            // back to the old field. Resolve the retained original compatibility lineage.
            if ($row && $class === 'identifier' && $row['source'] === 'governed_supersession') {
                $row = hrm_employee_projection_one('SELECT * FROM '.TB_PREF.$table.' WHERE '.$where
                    ." AND source<>'governed_supersession' ORDER BY ".$id.' DESC LIMIT 1');
                if ($row === false) return false;
            }
            $value = null;
            if ($row) {
                $key = hrm_person_sensitive_key($row['key_id']);
                $value = $key === false ? false : hrm_person_sensitive_decrypt($row['value_ciphertext'],
                    hrm_person_sensitive_context($person_id, $class, $type), $key);
                if ($value === false) return false;
            }
            $values[$field] = $value;
            if ($type === 'passport') $values['passport_expiry'] = $row ? $row['valid_to'] : null;
        }
    }
    foreach (array('hrm_person_addresses'=>array('address'=>'address_line','city'=>'city','state'=>'region','country'=>'country'),
        'hrm_person_emergency_contacts'=>array('emergency_name'=>'contact_name','emergency_relation'=>'relationship','emergency_phone'=>'phone')) as $table=>$fields) {
        $selector = $table === 'hrm_person_addresses' ? "address_type='residential'" : 'is_primary=1';
        $row = hrm_employee_projection_one('SELECT * FROM '.TB_PREF.$table.' WHERE person_id='.(int)$person_id
            .' AND '.$selector.' AND '.hrm_employee_projection_interval($as_of).' LIMIT 2', true);
        if ($row === false) return false;
        foreach ($fields as $legacy=>$normalized) $values[$legacy] = $row ? $row[$normalized] : null;
    }
    $bank = hrm_employee_projection_one('SELECT * FROM '.TB_PREF.'hrm_person_bank_accounts WHERE person_id='.(int)$person_id
        .' AND '.hrm_employee_projection_interval($as_of).' LIMIT 2', true);
    if ($bank === false) return false;
    if ($bank && $bank['source'] === 'governed_supersession') {
        $bank = hrm_employee_projection_one('SELECT * FROM '.TB_PREF.'hrm_person_bank_accounts WHERE person_id='.(int)$person_id
            ." AND source<>'governed_supersession' ORDER BY person_bank_account_id DESC LIMIT 1");
        if ($bank === false) return false;
    }
    $payload = array();
    if ($bank) {
        $key = hrm_person_sensitive_key($bank['key_id']);
        $plain = $key === false ? false : hrm_person_sensitive_decrypt($bank['details_ciphertext'], hrm_person_bank_context($person_id), $key);
        if ($plain === false) return false;
        $payload = json_decode($plain, true);
        if (!is_array($payload) || hrm_person_bank_payload_json($payload) !== $plain) return false;
    }
    foreach (array('bank_name','bank_branch','bank_account','bank_routing') as $field)
        $values[$field] = $bank ? $payload[$field] : null;
    return $values;
}

/**
 * Build current compatibility facts from authoritative owners only.
 * Current means accepted legacy-current episode/Assignment, not future scheduling.
 * Historical projection is deliberately a separate least-data API below.
 */
function hrm_employee_projection_build($employee_number)
{
    if (hrm_person_worker_current_company_id() === false || (int)$employee_number <= 0) return false;
    $binding = get_hrm_person_worker_binding((int)$employee_number);
    if (!$binding || $binding['retired_at'] !== null) return false;
    $link = hrm_employee_projection_one('SELECT clw.link_worker_id,clw.previous_person_id,clw.canonical_person_id,'
        .'cl.alias_person_id,cl.canonical_person_id AS accepted_person_id FROM '.TB_PREF.'hrm_employee_worker_map m'
        .' LEFT JOIN '.TB_PREF.'hrm_person_canonical_link_workers clw ON clw.worker_id=m.worker_id AND clw.mapping_id=m.mapping_id'
        .' LEFT JOIN '.TB_PREF.'hrm_person_canonical_links cl ON cl.link_id=clw.link_id'
        .' WHERE m.employee_number='.(int)$employee_number.' LIMIT 2');
    if (!$link || ($link['link_worker_id'] !== null && ((int)$link['previous_person_id'] !== (int)$link['alias_person_id']
        || (int)$link['canonical_person_id'] !== (int)$link['accepted_person_id']
        || (int)$link['canonical_person_id'] !== (int)$binding['person_id']))) return false;
    $name = hrm_employee_projection_one('SELECT first_name,middle_name,last_name FROM '.TB_PREF.'hrm_person_names WHERE person_id='
        .(int)$binding['person_id'].' AND effective_to IS NULL LIMIT 2');
    $employment = get_hrm_legacy_current_employment((int)$employee_number);
    $contract = get_hrm_legacy_current_contract((int)$employee_number);
    $assignment = get_hrm_legacy_current_assignment((int)$employee_number);
    $profile = hrm_compatibility_profile_current((int)$binding['worker_id']);
    $retained = $profile ? hrm_compatibility_profile_open($profile) : false;
    if (!$name || !$employment || !$contract || !$assignment || $retained === false
        || (int)$employment['worker_id'] !== (int)$binding['worker_id']
        || (int)$contract['employment_id'] !== (int)$employment['employment_id']
        || (int)$assignment['employment_id'] !== (int)$employment['employment_id']) return false;
    $first_hire = $employment['start_date'];
    if ((int)$employment['employment_sequence'] > 1) {
        $receipt = get_hrm_lifecycle_rehire_employment_evidence($employment);
        if (!$receipt) return false;
        $first_hire = $receipt['first_hire_date'];
    }
    $private = hrm_employee_projection_private((int)$binding['person_id']);
    if ($private === false) return false;
    $row = array_merge($retained, $name, $private, array(
        'employee_number'=>(int)$employee_number,'employee_id'=>$binding['employee_id'],
        'hire_date'=>$first_hire,'released_date'=>$employment['end_date'],
        'inactive'=>in_array($employment['employment_status'], array('terminated','inactive'), true) ? 1 : 0,
        'employment_type'=>$contract['legacy_employment_type']));
    foreach (array('department_id','position_id','grade_id','shift_id','reporting_to','cost_center_id','dimension2_id') as $field)
        $row[$field] = $assignment['legacy_'.$field];
    return $row;
}

/** Compare values without treating SQL numeric strings or empty optional text as drift. */
function hrm_employee_projection_difference($stored, $expected)
{
    if (!is_array($stored) || !is_array($expected)) return false;
    $fields = array();
    foreach ($expected as $field=>$value) {
        if (!array_key_exists($field, $stored)) { $fields[] = $field; continue; }
        if (($value === null || $value === '') && ($stored[$field] === null || $stored[$field] === '')) continue;
        if ((string)$stored[$field] !== (string)$value) $fields[] = $field;
    }
    sort($fields); return $fields;
}

/** Record only field names and numeric subject custody; no low-entropy PII hashes. */
function hrm_employee_projection_audit($employee_number, $action, $fields, $mode)
{
    return append_domain_audit_event(array('company_id'=>hrm_person_worker_current_company_id(),
        'legal_entity_id'=>0,'entity_type'=>'hrm_employee_projection','entity_key'=>array('employee_number'=>(int)$employee_number),
        'action'=>'hrm.employee_projection.'.$action,'reason_code'=>'hrm_fnd_006_projection',
        'before'=>null,'after'=>array('mode'=>$mode),
        'restricted_diff'=>array('fields'=>array_values($fields),'values_logged'=>false))) !== false;
}

/**
 * Compare or rebuild one existing row under Person/Worker custody and policy lock.
 * A missing row is not silently recreated: identity allocation and deletion have
 * their own authorized commands. Rebuild never invokes any legacy synchronizer.
 */
function hrm_employee_projection_reconcile($employee_number, $rebuild = false)
{
    if (!function_exists('user_check_access') || !user_check_access('SA_HRSETTINGS')
        || !user_check_access('SA_EMPLOYEE')) return false;
    begin_transaction();
    $policy = hrm_employee_projection_policy(true);
    $binding = $policy ? hrm_employment_lock_binding_for_interval_policy((int)$employee_number) : false;
    $stored = $binding ? hrm_employee_projection_one('SELECT * FROM '.TB_PREF.'employees WHERE employee_number='.(int)$employee_number.' FOR UPDATE') : false;
    $expected = $stored ? hrm_employee_projection_build((int)$employee_number) : false;
    $fields = $expected ? hrm_employee_projection_difference($stored, $expected) : false;
    $ok = $fields !== false;
    if ($ok && $rebuild && $fields) $ok = hrm_employee_projection_store($stored, $expected);
    if ($ok) $ok = hrm_employee_projection_audit((int)$employee_number,
        $rebuild ? 'rebuilt' : ($fields ? 'divergence' : 'parity'), $fields, $policy['mode']);
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? array('employee_number'=>(int)$employee_number,'fields'=>$fields,'rebuilt'=>(bool)$rebuild) : false;
}

/** Internal sole compatibility UPDATE; caller holds normalized and legacy locks. */
function hrm_employee_projection_store($stored, $expected)
{
    $fields = hrm_employee_projection_difference($stored, $expected);
    if ($fields === false || !isset($expected['employee_number'], $expected['employee_id'])
        || (int)$stored['employee_number'] !== (int)$expected['employee_number']
        || (string)$stored['employee_id'] !== (string)$expected['employee_id']) return false;
    if (!$fields) return true;
    $sets = array();
    foreach ($fields as $field) {
        if (!preg_match('/^[a-z_]+$/D', $field) || in_array($field, array('employee_number','employee_id'), true)) return false;
        $sets[] = $field.'='.($expected[$field] === null ? 'NULL' : db_escape($expected[$field]));
    }
    return db_query('UPDATE '.TB_PREF.'employees SET '.implode(',', $sets)
        .' WHERE employee_number='.(int)$expected['employee_number'].' AND employee_id='.db_escape($expected['employee_id']),
        'could not materialize employee compatibility projection') !== false;
}

/** Authorized least-data as-of identity and Assignment view; never overwrites current state. */
function get_hrm_employee_compatibility_as_of($company, $employee_number, $as_of)
{
    if (!hrm_person_worker_company_scope_is_current($company) || !hrm_person_worker_user_can_read()) return false;
    $as_of = hrm_person_worker_normalize_as_of($as_of);
    if ($as_of === false) return false;
    $identity = get_hrm_person_worker_as_of_for_company($company, (int)$employee_number, $as_of);
    if (!$identity) return false;
    $date = db_escape(substr($as_of, 0, 10));
    $row = hrm_employee_projection_one('SELECT h.employment_id,a.assignment_id,a.legacy_department_id AS department_id,'
        .'a.legacy_position_id AS position_id,a.legacy_grade_id AS grade_id FROM '.TB_PREF.'hrm_employments h'
        .' INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=h.employment_id'
        .' WHERE h.worker_id='.(int)$identity['worker_id'].' AND h.legacy_employee_number='.(int)$employee_number
        .' AND a.legacy_employee_number='.(int)$employee_number.' AND a.primary_flag=1'
        .' AND h.start_date<='.$date.' AND (h.end_date IS NULL OR h.end_date>='.$date.')'
        .' AND a.effective_from<='.$date.' AND (a.effective_to IS NULL OR a.effective_to>='.$date.') LIMIT 2');
    if (!$row) return false; // Unknown starts and gaps are explicit unavailable states.
    return array_merge($row, array('employee_number'=>(int)$employee_number,'employee_id'=>$identity['employee_id'],
        'first_name'=>$identity['first_name'],'middle_name'=>$identity['middle_name'],'last_name'=>$identity['last_name']));
}
