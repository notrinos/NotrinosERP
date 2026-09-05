<?php
/**
 * HRM-FND-004 approved payment-election to future payment-instruction binding resolver.
 *
 * This is a read-only custody boundary. It returns only approved election/allocation
 * identifiers, row versions, allocation metadata, and hashes. It deliberately does
 * not read or return bank plaintext, ciphertext, lookup tokens, key material, legacy
 * employee bank columns, payroll amounts, payment instructions, bank transactions,
 * GL rows, or accounting data. Future payment orchestration remains responsible for
 * authorization, result/obligation binding, trusted decryption, persistence, and all
 * external payment side effects.
 */

if (!defined('HRM_BANK_PAYMENT_INSTRUCTION_BINDING_CONTRACT'))
    define('HRM_BANK_PAYMENT_INSTRUCTION_BINDING_CONTRACT', 'notrinoserp.hrm-fnd-004.payment-instruction-binding');
if (!defined('HRM_BANK_PAYMENT_INSTRUCTION_BINDING_VERSION'))
    define('HRM_BANK_PAYMENT_INSTRUCTION_BINDING_VERSION', '1');
if (!defined('HRM_BANK_PAYMENT_INSTRUCTION_BINDING_MAX_ALLOCATIONS'))
    define('HRM_BANK_PAYMENT_INSTRUCTION_BINDING_MAX_ALLOCATIONS', 16);

function hrm_bank_payment_instruction_binding_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $integer = (int)$value;
    return $integer > 0 && (string)$integer === $value ? $integer : false;
}

function hrm_bank_payment_instruction_binding_currency($value)
{
    return is_string($value) && preg_match('/^[A-Z]{3}$/D', $value) === 1 ? $value : false;
}

function hrm_bank_payment_instruction_binding_datetime($value)
{
    if (!is_string($value) || strlen($value) !== 19
        || preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$/D', $value) !== 1)
        return false;
    $zone = new DateTimeZone('UTC');
    $date = DateTime::createFromFormat('!Y-m-d H:i:s', $value, $zone);
    $errors = DateTime::getLastErrors();
    if (!$date || (is_array($errors) && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $date->format('Y-m-d H:i:s') !== $value)
        return false;
    return $value;
}

function hrm_bank_payment_instruction_binding_percentage($value)
{
    if (!is_string($value)
        || preg_match('/^(?:100\.000000|(?:0|[1-9][0-9]?)\.[0-9]{6})$/D', $value) !== 1)
        return false;
    list($whole, $fraction) = explode('.', $value, 2);
    $micros = ((int)$whole * 1000000) + (int)$fraction;
    return $micros > 0 && $micros <= 100000000 ? $micros : false;
}

/**
 * Build exact read-only SQL for one approved Person/currency election at a UTC value time.
 * The projection intentionally excludes protected bank material and legacy employee bank fields.
 */
function get_hrm_bank_payment_instruction_binding_election_sql($person_id, $currency_code, $value_at_utc)
{
    $person_id = hrm_bank_payment_instruction_binding_positive_int($person_id);
    $currency_code = hrm_bank_payment_instruction_binding_currency($currency_code);
    $value_at_utc = hrm_bank_payment_instruction_binding_datetime($value_at_utc);
    if ($person_id === false || $currency_code === false || $value_at_utc === false || !defined('TB_PREF'))
        return false;

    return "SELECT e.payment_election_id,e.person_id,e.currency_code,e.approval_draft_id,e.row_version,"
        ."e.effective_from,e.effective_to,e.source,d.status approval_draft_status,d.trans_type approval_trans_type "
        ."FROM ".TB_PREF."hrm_person_bank_payment_elections e "
        ."INNER JOIN ".TB_PREF."approval_drafts d ON d.id=e.approval_draft_id "
        ."WHERE e.person_id=".$person_id." AND e.currency_code=".db_escape($currency_code)
        ." AND e.election_status='approved' AND e.approval_status='approved' "
        ."AND e.source='governed_payment_election' AND e.effective_from IS NOT NULL "
        ."AND e.effective_from<=".db_escape($value_at_utc)
        ." AND (e.effective_to IS NULL OR e.effective_to>".db_escape($value_at_utc).") "
        ."ORDER BY e.payment_election_id DESC LIMIT 2";
}

/**
 * Build the exact allocation/bank-version custody projection for one election.
 * No bank value, mask, ciphertext, token, key, verification evidence, or legacy field is selected.
 */
function get_hrm_bank_payment_instruction_binding_allocations_sql($payment_election_id, $person_id, $value_at_utc)
{
    $payment_election_id = hrm_bank_payment_instruction_binding_positive_int($payment_election_id);
    $person_id = hrm_bank_payment_instruction_binding_positive_int($person_id);
    $value_at_utc = hrm_bank_payment_instruction_binding_datetime($value_at_utc);
    if ($payment_election_id === false || $person_id === false || $value_at_utc === false || !defined('TB_PREF'))
        return false;

    return "SELECT a.payment_allocation_id,a.payment_election_id,a.person_bank_account_id,a.bank_account_row_version,"
        ."a.allocation_type,a.allocation_value,a.priority,b.person_id bank_person_id,b.row_version current_bank_row_version,"
        ."b.effective_from bank_effective_from,b.effective_to bank_effective_to,b.source bank_source,"
        ."b.verification_status bank_verification_status,b.approval_status bank_approval_status "
        ."FROM ".TB_PREF."hrm_person_bank_payment_allocations a "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_bank_account_id=a.person_bank_account_id "
        ."WHERE a.payment_election_id=".$payment_election_id." AND b.person_id=".$person_id
        ." AND b.row_version=a.bank_account_row_version AND b.source='governed_supersession' "
        ."AND b.verification_status='verified' AND b.approval_status='approved' AND b.verification_method<>'' "
        ."AND b.effective_from IS NOT NULL AND b.effective_from<=".db_escape($value_at_utc)
        ." AND (b.effective_to IS NULL OR b.effective_to>".db_escape($value_at_utc).") "
        ."ORDER BY a.priority,a.payment_allocation_id";
}

function hrm_bank_payment_instruction_binding_expected_approval_type()
{
    return defined('ST_HRM_BANK_PAYMENT_ELECTION') ? (int)ST_HRM_BANK_PAYMENT_ELECTION : 89;
}

function hrm_bank_payment_instruction_binding_expected_approval_status()
{
    return defined('APPROVAL_STATUS_APPROVED') ? (int)APPROVAL_STATUS_APPROVED : 1;
}

/**
 * Resolve a deterministic, read-only approved payment-election binding for a future payment builder.
 *
 * @return array|false Exact custody contract, or false for invalid/ambiguous/stale/ineligible state.
 */
function get_hrm_person_bank_payment_instruction_binding($person_id, $currency_code, $value_at_utc)
{
    $person_id = hrm_bank_payment_instruction_binding_positive_int($person_id);
    $currency_code = hrm_bank_payment_instruction_binding_currency($currency_code);
    $value_at_utc = hrm_bank_payment_instruction_binding_datetime($value_at_utc);
    if ($person_id === false || $currency_code === false || $value_at_utc === false
        || !function_exists('db_query') || !function_exists('db_fetch_assoc') || !function_exists('db_num_rows'))
        return false;

    $sql = get_hrm_bank_payment_instruction_binding_election_sql($person_id, $currency_code, $value_at_utc);
    if ($sql === false)
        return false;
    $result = db_query($sql, 'could not resolve approved HRM bank payment-election binding');
    if (!$result || (int)db_num_rows($result) !== 1)
        return false;
    $election = db_fetch_assoc($result);
    if (!is_array($election)
        || (int)$election['person_id'] !== $person_id
        || (string)$election['currency_code'] !== $currency_code
        || (string)$election['source'] !== 'governed_payment_election'
        || (int)$election['approval_draft_id'] <= 0
        || (int)$election['row_version'] <= 0
        || (int)$election['approval_draft_status'] !== hrm_bank_payment_instruction_binding_expected_approval_status()
        || (int)$election['approval_trans_type'] !== hrm_bank_payment_instruction_binding_expected_approval_type())
        return false;

    $allocation_sql = get_hrm_bank_payment_instruction_binding_allocations_sql(
        (int)$election['payment_election_id'], $person_id, $value_at_utc
    );
    if ($allocation_sql === false)
        return false;
    $alloc_result = db_query($allocation_sql, 'could not resolve HRM bank payment-election allocation binding');
    if (!$alloc_result)
        return false;

    $rows = array();
    $priorities = array();
    $accounts = array();
    $total_micros = 0;
    $canonical_parts = array();
    while ($row = db_fetch_assoc($alloc_result)) {
        if (!is_array($row) || count($rows) >= HRM_BANK_PAYMENT_INSTRUCTION_BINDING_MAX_ALLOCATIONS)
            return false;
        $allocation_id = hrm_bank_payment_instruction_binding_positive_int((string)$row['payment_allocation_id']);
        $bank_id = hrm_bank_payment_instruction_binding_positive_int((string)$row['person_bank_account_id']);
        $bank_version = hrm_bank_payment_instruction_binding_positive_int((string)$row['bank_account_row_version']);
        $priority = hrm_bank_payment_instruction_binding_positive_int((string)$row['priority']);
        $percentage = hrm_bank_payment_instruction_binding_percentage((string)$row['allocation_value']);
        if ($allocation_id === false || $bank_id === false || $bank_version === false || $priority === false
            || $priority > HRM_BANK_PAYMENT_INSTRUCTION_BINDING_MAX_ALLOCATIONS || $percentage === false
            || (string)$row['allocation_type'] !== 'percentage'
            || (int)$row['payment_election_id'] !== (int)$election['payment_election_id']
            || (int)$row['bank_person_id'] !== $person_id
            || (int)$row['current_bank_row_version'] !== $bank_version
            || (string)$row['bank_source'] !== 'governed_supersession'
            || (string)$row['bank_verification_status'] !== 'verified'
            || (string)$row['bank_approval_status'] !== 'approved'
            || isset($priorities[$priority]) || isset($accounts[$bank_id]))
            return false;
        $priorities[$priority] = true;
        $accounts[$bank_id] = true;
        $total_micros += $percentage;
        $rows[$priority] = array(
            'payment_allocation_id'=>$allocation_id,
            'person_bank_account_id'=>$bank_id,
            'bank_account_row_version'=>$bank_version,
            'allocation_type'=>'percentage',
            'allocation_value'=>(string)$row['allocation_value'],
            'priority'=>$priority,
        );
    }
    if (count($rows) < 1 || count($rows) > HRM_BANK_PAYMENT_INSTRUCTION_BINDING_MAX_ALLOCATIONS
        || $total_micros !== 100000000)
        return false;
    ksort($rows, SORT_NUMERIC);
    $expected_priority = 1;
    foreach ($rows as $priority=>$row) {
        if ((int)$priority !== $expected_priority++)
            return false;
        $canonical_parts[] = (int)$priority.':'.(int)$row['person_bank_account_id'].':'
            .(int)$row['bank_account_row_version'].':percentage:'.(string)$row['allocation_value'];
    }
    $canonical = implode(';', $canonical_parts);

    return array(
        'contract'=>HRM_BANK_PAYMENT_INSTRUCTION_BINDING_CONTRACT,
        'version'=>HRM_BANK_PAYMENT_INSTRUCTION_BINDING_VERSION,
        'person_id'=>$person_id,
        'currency_code'=>$currency_code,
        'value_at_utc'=>$value_at_utc,
        'payment_election_id'=>(int)$election['payment_election_id'],
        'payment_election_row_version'=>(int)$election['row_version'],
        'approval_draft_id'=>(int)$election['approval_draft_id'],
        'effective_from'=>(string)$election['effective_from'],
        'effective_to'=>$election['effective_to'] === null ? null : (string)$election['effective_to'],
        'allocation_count'=>count($rows),
        'allocation_total'=>'100.000000',
        'allocation_hash'=>hash('sha256', $canonical),
        'allocations'=>array_values($rows),
    );
}
