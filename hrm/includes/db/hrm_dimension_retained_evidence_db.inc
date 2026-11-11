<?php
/**
 * DIM-LFC-001 B04: HRM-owned minimized native Dimension-reference authority for
 * retained and protected HRM evidence.
 *
 * Three HRM evidence families can hold, or could be mistaken for holding, a
 * native Dimension coordinate outside the six scalar HRM owners:
 *  - hrm_lifecycle_hire_payloads: the encrypted pre-Worker Hire envelope. A
 *    staged/pending envelope carries the requested Cost Center (axis one) until
 *    the checker completes, rejects or cancels it, which purges the ciphertext.
 *  - hrm_worker_profile_versions: immutable encrypted Worker profile versions,
 *    every one of which retains a copy of employees.custom_data.
 *  - HRM approval families in approval_drafts / approval_actions. No reviewed
 *    HRM producer stores a Dimension coordinate in a draft (Hire drafts bind the
 *    command id and hashes only; Rehire copies the locked predecessor Assignment).
 *    Any Dimension-like key in an HRM draft is unexpected and stays unknown.
 *
 * Each decoder authenticates the envelope under the company's own key custody,
 * reads only the coordinate it needs and returns one state string: used, unused
 * or unknown. Plaintext, person data, salary, keys and identifiers never leave
 * these functions, and any failure (missing key, tampering, unknown shape or
 * state) is unknown, never unused. Payroll families (ST_PAYROLL_PERIOD and the
 * PAY-GL stores) belong to B05 and are not handled here.
 */
if (!function_exists('dimension_writer_reference_id'))
    require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_writer_fence.inc';
if (!function_exists('dimension_master_payload_references'))
    require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_master_payload_guard.inc';

/**
 * Load the HRM custody sources only when an HRM evidence row must be opened.
 *
 * @return bool True when the decrypting helpers are available
 */
function hrm_dimension_retained_load_sources()
{
    global $path_to_root;
    $root = dirname(dirname(dirname(__DIR__)));
    if (!isset($path_to_root) || $path_to_root === '' || realpath($path_to_root) === false) $path_to_root = $root;
    if (!function_exists('hrm_lifecycle_hire_payload_decrypt'))
        include_once $root.'/hrm/includes/db/lifecycle_hire_payload_db.inc';
    if (!function_exists('hrm_compatibility_profile_open'))
        include_once $root.'/hrm/includes/db/employee_compatibility_profile_db.inc';
    return function_exists('hrm_lifecycle_hire_payload_decrypt') && function_exists('hrm_compatibility_profile_open')
        && function_exists('hrm_person_sensitive_key') && function_exists('domain_audit_hash_equals');
}

/**
 * Is this array key a native Dimension coordinate name? Same vocabulary as the
 * B10 live-schema drift check, plus the ordinary default and GL draft names.
 *
 * @param mixed $key Array key
 * @return bool
 */
function hrm_dimension_retained_coordinate_key($key)
{
    return is_string($key) && (bool)preg_match('/(^|_)(dimension[0-9]*|dim[0-9]*)(_id)?$|cost_cent|^dflt_dim[0-9]*$|^gl_dim[0-9]*$/i', $key);
}

/**
 * Is this legacy custom_data key a native or independent Dimension coordinate in
 * the shared retained-JSON projection? Such a key must never be re-homed into a
 * Typed Attribute value: the coordinate would leave the projection that protects
 * it. Free-text keys such as "cost_center.note" are not coordinates.
 *
 * @param mixed $key Legacy custom_data key
 * @return bool
 */
function hrm_dimension_legacy_key_is_coordinate($key)
{
    return is_string($key) && in_array($key, array('dimension_id', 'dimension', 'dimension2_id', 'dimension2',
        'cost_center_id', 'legacy_cost_center_id', 'legacy_dimension2_id', 'core_dimension_id', 'core_dimension2_id',
        'axis_value_key', 'core_slot_hint'), true);
}

/**
 * The HRM approval families this authority decodes. ST_PAYROLL_PERIOD is a
 * payroll family (B05) and is deliberately absent.
 *
 * @return int[] Transaction type identities
 */
function hrm_dimension_approval_family_types()
{
    $types = array();
    foreach (array('ST_LEAVE_REQUEST','ST_OVERTIME_REQUEST','ST_LOAN_REQUEST','ST_EMPLOYEE_APPRAISAL',
        'ST_HRM_IDENTIFIER_VERIFICATION','ST_HRM_BANK_ACCOUNT_VERIFICATION','ST_HRM_BANK_PAYMENT_ELECTION',
        'ST_HRM_LIFECYCLE_TRANSFER','ST_HRM_LIFECYCLE_SEPARATION','ST_HRM_LIFECYCLE_SALARY_REVISION',
        'ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT','ST_HRM_LIFECYCLE_CONFIRMATION','ST_HRM_LIFECYCLE_PROBATION_SCHEDULE',
        'ST_HRM_LIFECYCLE_PROMOTION','ST_HRM_LIFECYCLE_SUSPENSION','ST_HRM_LIFECYCLE_REINSTATEMENT',
        'ST_HRM_LIFECYCLE_HIRE','ST_HRM_LIFECYCLE_REHIRE','ST_HRM_COMPENSATION_AWARD') as $type)
        if (defined($type)) $types[] = (int)constant($type);
    return $types;
}

/**
 * Classify one decoded HRM approval draft or action snapshot. HRM drafts carry
 * no Dimension coordinate, so a payload without any coordinate-like key is
 * unused; one with such a key, or one that is not a bounded array, is unknown.
 *
 * @param mixed $payload Decoded draft payload
 * @return string unused or unknown
 */
function hrm_dimension_approval_payload_status($payload)
{
    if (!is_array($payload)) return 'unknown';
    $stack = array(array($payload, 0));
    while ($stack) {
        list($node, $depth) = array_pop($stack);
        if ($depth > 64) return 'unknown';
        foreach ($node as $key => $value) {
            if (hrm_dimension_retained_coordinate_key($key)) return 'unknown';
            if (is_array($value)) $stack[] = array($value, $depth + 1);
            elseif (is_object($value)) return 'unknown';
        }
    }
    return 'unused';
}

/**
 * Classify one Hire command/payload pair for one Dimension. A purged tombstone
 * holds no coordinate. A staged envelope of a staged or pending command is
 * authenticated with its own key generation, request hash and ciphertext hash,
 * canonicalized exactly as the executor does, and only its Cost Center is read.
 *
 * @param array $row Joined command and payload columns
 * @param int   $id  Exact positive Dimension identity
 * @param int   $company Active company
 * @return string used, unused or unknown
 */
function hrm_dimension_hire_payload_status($row, $id, $company)
{
    if (!is_array($row) || !isset($row['payload_state'])) return 'unknown';
    if ((string)$row['payload_state'] === 'purged')
        return ($row['payload_ciphertext'] === null || $row['payload_ciphertext'] === '') ? 'unused' : 'unknown';
    if ((string)$row['payload_state'] !== 'staged' || !in_array((string)$row['command_status'], array('staged', 'pending'), true)
        || !is_string($row['payload_ciphertext']) || $row['payload_ciphertext'] === '') return 'unknown';
    if (!domain_audit_hash_equals((string)$row['command_request_hash'], (string)$row['request_hash'])
        || !domain_audit_hash_equals(hash('sha256', $row['payload_ciphertext']), (string)$row['ciphertext_sha256'])) return 'unknown';
    $key = hrm_hire_payload_key((string)$row['key_id'], $company, (int)$row['key_generation']);
    $context = hrm_lifecycle_hire_payload_context($company, $row['hire_command_id'], (string)$row['command_request_hash'],
        (int)$row['envelope_version']);
    $json = $key === false || $context === false ? false : hrm_lifecycle_hire_payload_decrypt($row['payload_ciphertext'], $context, $key);
    $decoded = is_string($json) ? json_decode($json, true) : null;
    $canonical = is_array($decoded) ? hrm_lifecycle_hire_payload_canonicalize($decoded) : false;
    $reencoded = $canonical === false ? false : hrm_lifecycle_hire_payload_canonical_json($canonical);
    $expected = $reencoded === false ? false
        : hrm_lifecycle_hire_payload_request_hash($company, substr((string)$row['effective_at'], 0, 10), $reencoded);
    if ($reencoded === false || $reencoded !== $json || $expected === false
        || !domain_audit_hash_equals($expected, (string)$row['command_request_hash'])) return 'unknown';
    // The Hire schema has exactly one native coordinate; everything else is person data and stays here.
    return (int)$canonical['cost_center_id'] === $id ? 'used' : 'unused';
}

/**
 * Classify one immutable Worker profile version for one Dimension through the
 * same canonical custom_data projection the Employees owner uses.
 *
 * @param array $row Profile version row
 * @param int   $id  Exact positive Dimension identity
 * @return string used, unused or unknown
 */
function hrm_dimension_profile_version_status($row, $id)
{
    $values = hrm_compatibility_profile_open($row);
    if (!is_array($values) || !array_key_exists('custom_data', $values)) return 'unknown';
    if ($values['custom_data'] === null || $values['custom_data'] === '') return 'unused';
    $references = dimension_master_payload_references($values['custom_data']);
    if ($references === false) return 'unknown';
    foreach ($references as $reference) if ((int)$reference[0] === $id) return 'used';
    return 'unused';
}

/**
 * Locked current-read classification of the two HRM retained owners, inside the
 * caller's open repeatable-read transaction (B10 recheck). Shared row and gap
 * locks keep a new staged Hire or profile version from appearing after an
 * "unused" answer. Only owner names and states are returned.
 *
 * @param int   $id      Exact positive Dimension identity
 * @param array $columns Live columns per table from dimension_reference_recheck_columns()
 * @return array owner => used|unused|unknown
 */
function hrm_dimension_retained_observe($id, $columns)
{
    $result = array('hrm_lifecycle_hire_payloads'=>'unknown', 'hrm_worker_profile_versions'=>'unknown');
    if (empty($GLOBALS['transaction_level']) || !is_array($columns) || !hrm_dimension_retained_load_sources()) return $result;
    $company = function_exists('user_company') ? user_company() : false;
    if (!is_int($company) && !(is_string($company) && preg_match('/^(?:0|[1-9][0-9]*)$/D', $company))) return $result;
    $company = (int)$company;
    $id = (int)$id;
    $expected = array(
        'hrm_lifecycle_hire_commands'=>array('hire_command_id','command_status','effective_at','request_hash'),
        'hrm_lifecycle_hire_payloads'=>array('hire_command_id','payload_state','envelope_version','payload_ciphertext',
            'ciphertext_sha256','request_hash','key_id','key_generation'),
        'hrm_worker_profile_versions'=>array('profile_version_id','worker_id','version_no','predecessor_id','effective_from','key_id','payload_ciphertext'));
    $schema = array();
    foreach ($expected as $table => $needed) {
        $live = isset($columns[$table]) ? $columns[$table] : array();
        $schema[$table] = (bool)$live;
        foreach ($needed as $column) if (!isset($live[$column])) $schema[$table] = false;
        // A new plain Dimension-like column would be an unregistered coordinate.
        foreach (array_keys($live) as $column) if (hrm_dimension_retained_coordinate_key($column)) $schema[$table] = false;
    }
    try {
        if ($schema['hrm_lifecycle_hire_commands'] && $schema['hrm_lifecycle_hire_payloads']) {
            $rows = db_query('SELECT p.hire_command_id,p.payload_state,p.envelope_version,p.payload_ciphertext,p.ciphertext_sha256,'
                .'p.request_hash,p.key_id,p.key_generation,c.command_status,c.effective_at,c.request_hash AS command_request_hash'
                .' FROM '.TB_PREF.'hrm_lifecycle_hire_payloads p LEFT JOIN '.TB_PREF.'hrm_lifecycle_hire_commands c'
                .' ON c.hire_command_id=p.hire_command_id ORDER BY p.hire_command_id LOCK IN SHARE MODE');
            if ($rows) {
                $state = 'unused';
                while ($row = db_fetch_assoc($rows)) {
                    $observed = $row['command_status'] === null && (string)$row['payload_state'] !== 'purged' ? 'unknown'
                        : hrm_dimension_hire_payload_status($row, $id, $company);
                    if ($observed === 'used') { $state = 'used'; break; }
                    if ($observed !== 'unused') $state = 'unknown';
                }
                $result['hrm_lifecycle_hire_payloads'] = $state;
            }
            if (db_error_no()) return $result;
        }
        if ($schema['hrm_worker_profile_versions']) {
            $rows = db_query('SELECT worker_id,version_no,predecessor_id,effective_from,key_id,payload_ciphertext FROM '
                .TB_PREF.'hrm_worker_profile_versions ORDER BY profile_version_id LOCK IN SHARE MODE');
            if ($rows) {
                $state = 'unused';
                while ($row = db_fetch_assoc($rows)) {
                    $observed = hrm_dimension_profile_version_status($row, $id);
                    if ($observed === 'used') { $state = 'used'; break; }
                    if ($observed !== 'unused') $state = 'unknown';
                }
                $result['hrm_worker_profile_versions'] = $state;
            }
        }
    } catch (Exception $exception) {
        return array('hrm_lifecycle_hire_payloads'=>'unknown', 'hrm_worker_profile_versions'=>'unknown');
    }
    return $result;
}
