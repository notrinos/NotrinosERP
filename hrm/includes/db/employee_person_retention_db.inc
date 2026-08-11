<?php
/**
 * HRM-FND-001 retired Person-shadow retention/anonymization executor.
 *
 * This is a deliberately narrow records-governance path. Legacy employee rows,
 * payroll/accounting history, PAY-AUD evidence, Person/Worker/link identifiers,
 * and compatibility mappings are never deleted or renumbered here.
 */

if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');
if (!function_exists('domain_audit_governance_state'))
    include_once($path_to_root.'/includes/db/domain_audit_governance.inc');

/** @return bool */
function hrm_person_retention_tables_ready()
{
    foreach (array(
        'employees', 'hrm_persons', 'hrm_person_names', 'hrm_person_contacts',
        'hrm_person_identifiers', 'hrm_person_addresses',
        'hrm_person_emergency_contacts', 'hrm_workers',
        'hrm_employee_worker_map', 'hrm_person_canonical_link_workers',
        'domain_audit_events', 'domain_audit_chain_state'
    ) as $table) {
        if (!hrm_person_worker_table_exists($table))
            return false;
    }
    return true;
}

/**
 * Normalize a public integer identifier without accepting lossy casts.
 *
 * @param mixed $value
 * @param bool $allow_zero
 * @return int|false
 */
function hrm_person_retention_exact_id($value, $allow_zero = false)
{
    if (is_int($value)) {
        $parsed = $value;
    } elseif (is_string($value)
        && preg_match('/^(0|[1-9][0-9]*)$/D', $value) === 1
    ) {
        $parsed = (int)$value;
        if ((string)$parsed !== $value)
            return false;
    } else {
        return false;
    }
    if ($parsed < 0 || (!$allow_zero && $parsed === 0))
        return false;
    return $parsed;
}

/**
 * Require the already-approved records-governance OS identity and exact active
 * company. No browser permission or new role grant is introduced.
 *
 * @param int $company
 * @return string|false Authorized OS identity.
 */
function hrm_person_retention_authorized($company)
{
    $company = hrm_person_retention_exact_id($company, true);
    if ($company === false || !hrm_person_worker_company_scope_is_current($company)
        || !function_exists('domain_audit_records_admin_authorized'))
        return false;
    return domain_audit_records_admin_authorized($company);
}

/**
 * Read and lock every compatibility mapping that has ever belonged to a Person.
 * Canonical-link snapshots retain alias ownership after a Worker is rebound.
 *
 * @param int $person_id
 * @param bool $for_update
 * @return array|false
 */
function hrm_person_retention_mappings($person_id, $for_update = false)
{
    $person_id = hrm_person_retention_exact_id($person_id);
    if ($person_id === false)
        return false;
    $sql = "SELECT DISTINCT m.mapping_id,m.employee_number,m.employee_id,m.retired_at,"
        ."w.worker_id,w.person_id current_person_id,w.worker_status,w.active_to "
        ."FROM ".TB_PREF."hrm_employee_worker_map m "
        ."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."LEFT JOIN ".TB_PREF."hrm_person_canonical_link_workers clw "
        ."ON clw.worker_id=w.worker_id AND clw.mapping_id=m.mapping_id "
        ."WHERE w.person_id=".$person_id." OR clw.previous_person_id=".$person_id
        ." ORDER BY m.mapping_id";
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read HRM Person retention mappings');
    if (!$result)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($result))
        $rows[] = $row;
    return $rows;
}

/**
 * Return a fail-closed retirement candidate and retention anchor.
 *
 * Eligibility requires at least one historical mapping, every mapping retired,
 * and zero surviving legacy employee rows. This keeps all current legacy
 * selectors unchanged by construction.
 *
 * @param int $person_id
 * @param bool $for_update
 * @return array|false
 */
function hrm_person_retention_candidate($person_id, $for_update = false)
{
    $person_id = hrm_person_retention_exact_id($person_id);
    if ($person_id === false || !hrm_person_retention_tables_ready())
        return false;
    $sql = "SELECT person_id,person_status,created_at FROM ".TB_PREF."hrm_persons"
        ." WHERE person_id=".$person_id." LIMIT 1".($for_update ? ' FOR UPDATE' : '');
    $result = db_query($sql, 'could not lock HRM Person retention candidate');
    $person = $result ? db_fetch_assoc($result) : false;
    if (!$person)
        return false;
    if ((string)$person['person_status'] === 'anonymized')
        return array('person_id' => $person_id, 'person_status' => 'anonymized',
            'retention_anchor' => null, 'mapping_count' => 0, 'worker_count' => 0);
    if (!in_array((string)$person['person_status'], array('active', 'inactive'), true))
        return false;

    $mappings = hrm_person_retention_mappings($person_id, $for_update);
    if ($mappings === false || count($mappings) === 0)
        return false;
    $anchor = '';
    $employees = array();
    $workers = array();
    foreach ($mappings as $row) {
        $retired_at = isset($row['retired_at']) ? (string)$row['retired_at'] : '';
        if ($retired_at === '' || hrm_person_worker_normalize_as_of($retired_at) === false)
            return false;
        if ($retired_at > $anchor)
            $anchor = $retired_at;
        $employees[(int)$row['employee_number']] = true;
        $workers[(int)$row['worker_id']] = true;
    }
    if ($anchor === '' || empty($employees))
        return false;

    $employee_ids = array_keys($employees);
    sort($employee_ids, SORT_NUMERIC);
    $result = db_query(
        "SELECT COUNT(*) row_count FROM ".TB_PREF."employees WHERE employee_number IN ("
        .implode(',', $employee_ids).")",
        'could not verify retired HRM Person legacy cohort'
    );
    $count = $result ? db_fetch_assoc($result) : false;
    if (!$count || (int)$count['row_count'] !== 0)
        return false;

    return array(
        'person_id' => $person_id,
        'person_status' => (string)$person['person_status'],
        'retention_anchor' => $anchor,
        'mapping_count' => count($mappings),
        'worker_count' => count($workers),
    );
}

/**
 * Generate one unlinkable replacement lookup token without retaining source PII.
 *
 * @param string $class
 * @param int $row_id
 * @return string|false
 */
function hrm_person_retention_lookup_token($class, $row_id)
{
    if (!function_exists('openssl_random_pseudo_bytes'))
        return false;
    $strong = false;
    $bytes = openssl_random_pseudo_bytes(32, $strong);
    if ($bytes === false || strlen($bytes) !== 32 || !$strong)
        return false;
    return hash('sha256', 'hrm-person-retention-v1'."\0".(string)$class."\0".(int)$row_id."\0".$bytes);
}

/**
 * Return exact row counts for only the shadow attributes this slice anonymizes.
 *
 * @param int $person_id
 * @return array|false
 */
function hrm_person_retention_shadow_counts($person_id)
{
    $person_id = hrm_person_retention_exact_id($person_id);
    if ($person_id === false)
        return false;
    $counts = array();
    foreach (array(
        'name_versions' => 'hrm_person_names',
        'contact_versions' => 'hrm_person_contacts',
        'identifier_versions' => 'hrm_person_identifiers',
        'address_versions' => 'hrm_person_addresses',
        'emergency_versions' => 'hrm_person_emergency_contacts',
    ) as $label => $table) {
        $result = db_query("SELECT COUNT(*) row_count FROM ".TB_PREF.$table
            ." WHERE person_id=".$person_id,
            'could not count HRM Person retention shadow rows');
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row)
            return false;
        $counts[$label] = (int)$row['row_count'];
    }
    return $counts;
}

/**
 * Apply direct-identifier anonymization only. Structural/evidence identifiers,
 * effective dates, mapping IDs, Worker IDs, Person IDs and audit history stay.
 *
 * @param int $person_id
 * @return array|false Counts of anonymized rows.
 */
function hrm_person_retention_anonymize_shadow_rows($person_id)
{
    $person_id = hrm_person_retention_exact_id($person_id);
    if ($person_id === false)
        return false;
    $counts = hrm_person_retention_shadow_counts($person_id);
    if ($counts === false)
        return false;

    if (!db_query("UPDATE ".TB_PREF."hrm_person_names SET first_name='Anonymized',"
        ."middle_name='',last_name='Person',verification_status='redacted'"
        ." WHERE person_id=".$person_id,
        'could not anonymize HRM Person name shadow'))
        return false;

    foreach (array(
        'contact' => array('table' => 'hrm_person_contacts', 'id' => 'contact_id'),
        'identifier' => array('table' => 'hrm_person_identifiers', 'id' => 'identifier_id'),
    ) as $class => $meta) {
        $result = db_query("SELECT ".$meta['id']." row_id FROM ".TB_PREF.$meta['table']
            ." WHERE person_id=".$person_id." ORDER BY ".$meta['id']." FOR UPDATE",
            'could not lock HRM Person encrypted shadow rows');
        if (!$result)
            return false;
        while ($row = db_fetch_assoc($result)) {
            $token = hrm_person_retention_lookup_token($class, (int)$row['row_id']);
            if ($token === false)
                return false;
            $extra = $class === 'identifier'
                ? ",issuing_jurisdiction='',valid_to=NULL" : '';
            if (!db_query("UPDATE ".TB_PREF.$meta['table']
                ." SET value_ciphertext='',masked_value='[redacted]',lookup_token=".db_escape($token)
                .",key_id='purged-v1',verification_status='redacted'".$extra
                ." WHERE ".$meta['id']."=".(int)$row['row_id']." AND person_id=".$person_id,
                'could not purge HRM Person encrypted shadow value'))
                return false;
        }
    }

    if (!db_query("UPDATE ".TB_PREF."hrm_person_addresses SET address_line='',city='',region='',"
        ."country='',postal_code='',is_residence=0,is_tax_residence=0,verification_status='redacted'"
        ." WHERE person_id=".$person_id,
        'could not anonymize HRM Person address shadow'))
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_person_emergency_contacts SET contact_name='',relationship='',"
        ."phone='',is_primary=0,consent_status='redacted',verification_status='redacted'"
        ." WHERE person_id=".$person_id,
        'could not anonymize HRM Person emergency-contact shadow'))
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_persons SET person_status='anonymized'"
        ." WHERE person_id=".$person_id." AND person_status IN ('active','inactive')",
        'could not mark HRM Person anonymized'))
        return false;
    return $counts;
}

/**
 * Verify every PAY-AUD governance chain known for the company and derive one
 * conservative hold/retention floor while Person has no legal-entity relation.
 * Any active hold in any governance scope blocks deletion/anonymization.
 *
 * @param int $company
 * @param string $as_of
 * @return array|false
 */
function hrm_person_retention_company_governance_state($company, $as_of)
{
    $company = hrm_person_retention_exact_id($company, true);
    if ($company === false || !function_exists('domain_audit_valid_utc_timestamp')
        || !domain_audit_valid_utc_timestamp((string)$as_of))
        return false;

    $result = db_query(
        "SELECT DISTINCT legal_entity_id FROM ".TB_PREF."domain_audit_events"
        ." WHERE company_id=".$company." AND entity_type='domain_audit_governance'"
        ." ORDER BY legal_entity_id",
        'could not enumerate PAY-AUD governance scopes for HRM Person retention'
    );
    if (!$result)
        return false;
    $entities = array(0 => true);
    while ($row = db_fetch_assoc($result)) {
        $entity = hrm_person_retention_exact_id((string)$row['legal_entity_id'], true);
        if ($entity === false)
            return false;
        $entities[$entity] = true;
    }
    ksort($entities, SORT_NUMERIC);

    $active_hold_count = 0;
    $minimum_retention_days = 0;
    foreach (array_keys($entities) as $legal_entity) {
        $state = domain_audit_governance_state($company, $legal_entity, (string)$as_of);
        if (!is_array($state) || empty($state['ok']))
            return false;
        $active_hold_count += isset($state['active_hold_count'])
            ? (int)$state['active_hold_count'] : 0;
        if (!empty($state['policy']) && isset($state['policy']['retention_days'])) {
            $days = (int)$state['policy']['retention_days'];
            if ($days < 1 || $days > 36500)
                return false;
            if ($days > $minimum_retention_days)
                $minimum_retention_days = $days;
        }
    }
    return array(
        'ok' => true,
        'governance_scope_count' => count($entities),
        'active_hold_count' => $active_hold_count,
        'minimum_retention_days' => $minimum_retention_days,
    );
}

/**
 * Execute one serialized, auditable anonymization of an already-retired Person.
 *
 * The caller must supply a non-sensitive reference to an already-approved
 * employer/country retention decision and its exact retained-until date; core
 * does not infer jurisdictional retention. Every active PAY-AUD legal hold in
 * the company blocks execution. Existing PAY-AUD retention policies, where
 * present, are only an additional conservative minimum floor. SERIALIZABLE
 * isolation orders governance reads, eligibility, mutation and audit evidence.
 *
 * @param int|string $company
 * @param int|string $person_id
 * @param string $approved_retained_until
 * @param string $policy_ref
 * @param string $reason_code
 * @return array
 */
function execute_hrm_person_retention_anonymization(
    $company,
    $person_id,
    $approved_retained_until,
    $policy_ref,
    $reason_code
) {
    $actor = hrm_person_retention_authorized($company);
    $company = hrm_person_retention_exact_id($company, true);
    $person_id = hrm_person_retention_exact_id($person_id);
    $approved_retained_until = trim((string)$approved_retained_until);
    $policy_ref = trim((string)$policy_ref);
    $reason_code = trim((string)$reason_code);
    if ($company === false || $person_id === false || $actor === false
        || !function_exists('domain_audit_valid_token')
        || !function_exists('domain_audit_valid_date')
        || !domain_audit_valid_date($approved_retained_until)
        || !domain_audit_valid_token($policy_ref)
        || !domain_audit_valid_token($reason_code))
        return array('ok' => false, 'code' => 'authorization_or_input_rejected');
    if (!hrm_person_retention_tables_ready())
        return array('ok' => false, 'code' => 'retention_predecessor_missing');

    if (!db_query('SET TRANSACTION ISOLATION LEVEL SERIALIZABLE',
        'could not serialize HRM Person retention transaction'))
        return array('ok' => false, 'code' => 'serialization_setup_failed');
    begin_transaction();

    $candidate = hrm_person_retention_candidate($person_id, true);
    if ($candidate === false) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'person_not_retention_eligible');
    }
    if ($candidate['person_status'] === 'anonymized') {
        commit_transaction();
        return array('ok' => true, 'code' => 'already_anonymized', 'person_id' => $person_id);
    }

    $now_result = db_query('SELECT UTC_TIMESTAMP() utc_now',
        'could not read HRM Person retention UTC timestamp');
    $now_row = $now_result ? db_fetch_assoc($now_result) : false;
    $now = $now_row && isset($now_row['utc_now'])
        ? hrm_person_worker_normalize_as_of((string)$now_row['utc_now']) : false;
    if ($now === false) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'retention_clock_failed');
    }

    $governance = hrm_person_retention_company_governance_state($company, $now);
    if (!is_array($governance) || empty($governance['ok'])) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'retention_governance_invalid');
    }
    if (!empty($governance['active_hold_count'])) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'legal_hold_active');
    }

    $retention_anchor_date = substr((string)$candidate['retention_anchor'], 0, 10);
    if ($approved_retained_until < $retention_anchor_date) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'approved_schedule_invalid');
    }
    $governance_floor = $retention_anchor_date;
    $minimum_days = isset($governance['minimum_retention_days'])
        ? (int)$governance['minimum_retention_days'] : 0;
    if ($minimum_days > 0) {
        $governance_floor = domain_audit_minimum_retained_until(
            (string)$candidate['retention_anchor'], $minimum_days
        );
        if ($governance_floor === false || $approved_retained_until < $governance_floor) {
            cancel_transaction();
            return array('ok' => false, 'code' => 'approved_schedule_below_governance_floor',
                'minimum_retained_until' => $governance_floor);
        }
    }
    if (substr($now, 0, 10) < $approved_retained_until) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'retention_period_not_elapsed',
            'retained_until' => $approved_retained_until);
    }

    $counts = hrm_person_retention_anonymize_shadow_rows($person_id);
    if ($counts === false) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'shadow_anonymization_failed');
    }
    $audit = append_domain_audit_event(array(
        'company_id' => $company,
        'legal_entity_id' => 0,
        'entity_type' => 'hrm_person_retention',
        'entity_key' => array('person_id' => $person_id),
        'action' => 'hrm.person.retention_anonymized',
        'reason_code' => $reason_code,
        'security_context' => array(
            'actor_user_id' => 0,
            'company_id' => $company,
            'role_id' => 0,
            'role_set' => array(),
            'auth_method' => 'records_governance_cli',
            'os_actor' => (string)$actor,
        ),
        'before' => null,
        'after' => array('person_status' => 'anonymized'),
        'restricted_diff' => array(
            'retention_anchor' => (string)$candidate['retention_anchor'],
            'approved_retained_until' => $approved_retained_until,
            'policy_ref' => $policy_ref,
            'governance_minimum_retained_until' => (string)$governance_floor,
            'governance_minimum_retention_days' => $minimum_days,
            'governance_scope_count' => (int)$governance['governance_scope_count'],
            'mapping_count' => (int)$candidate['mapping_count'],
            'worker_count' => (int)$candidate['worker_count'],
            'shadow_row_counts' => $counts,
            'active_hold_count' => 0,
            'restricted_values_stored' => false,
        ),
    ));
    if (!is_array($audit)) {
        cancel_transaction();
        return array('ok' => false, 'code' => 'retention_audit_failed');
    }
    commit_transaction();
    return array(
        'ok' => true,
        'code' => 'person_shadow_anonymized',
        'person_id' => $person_id,
        'retention_anchor' => (string)$candidate['retention_anchor'],
        'retained_until' => $approved_retained_until,
        'policy_ref' => $policy_ref,
        'governance_minimum_retained_until' => (string)$governance_floor,
        'shadow_row_counts' => $counts,
        'chain_sequence' => (int)$audit['chain_sequence'],
        'correlation_id' => (string)$audit['correlation_id'],
    );
}
