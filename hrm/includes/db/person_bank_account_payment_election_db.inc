<?php
/**
 * HRM-FND-004 governed Person bank-account payment-election command.
 *
 * This internal writer creates only a pending Person/currency election plus
 * exact bank-version percentage allocations and one central ApprovalWorkflow
 * draft. Final approval promotes only election metadata and closes an optional
 * predecessor election. It does not create payment instructions, post payroll,
 * execute a payment, create bank transactions/GL, or read protected bank values.
 */
$hrm_bank_payment_election_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_bank_payment_election_dependencies = array(
    array('/hrm/includes/hrm_security.inc', 'function', 'hrm_user_can_access_sensitive_field'),
    array('/includes/federation_assurance_consumer.inc', 'function', 'federation_assurance_consumer_current_session_id'),
    array('/includes/approval/approval_workflow.inc', 'class', 'ApprovalWorkflow'),
    array('/includes/approval/db/approval_rules_db.inc', 'function', 'get_approval_workflow_by_type'),
    array('/includes/db/domain_audit_db.inc', 'function', 'append_domain_audit_event'),
);
foreach ($hrm_bank_payment_election_dependencies as $dependency) {
    $available = $dependency[1] === 'class' ? class_exists($dependency[2]) : function_exists($dependency[2]);
    if ($available)
        continue;
    $candidate = $hrm_bank_payment_election_root.$dependency[0];
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_bank_payment_election_root, $hrm_bank_payment_election_dependencies, $dependency, $candidate, $available);

if (!defined('HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS'))
    define('HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS', 'hrm.person_bank_account.payment_election.submit');
if (!defined('HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS'))
    define('HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS', 'hrm.person_bank_account.payment_election.approve');
if (!defined('HRM_BANK_PAYMENT_ELECTION_HASH_CONTRACT'))
    define('HRM_BANK_PAYMENT_ELECTION_HASH_CONTRACT', 'notrinoserp.hrm-fnd-004.person-bank-payment-election');
if (!defined('HRM_BANK_PAYMENT_ELECTION_HASH_VERSION'))
    define('HRM_BANK_PAYMENT_ELECTION_HASH_VERSION', '1');
if (!defined('HRM_BANK_PAYMENT_ELECTION_MAX_ALLOCATIONS'))
    define('HRM_BANK_PAYMENT_ELECTION_MAX_ALLOCATIONS', 16);

function hrm_bank_payment_election_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $integer = (int)$value;
    return $integer > 0 && (string)$integer === $value ? $integer : false;
}

function hrm_bank_payment_election_nonnegative_int($value)
{
    if (is_int($value))
        return $value >= 0 ? $value : false;
    if (!is_string($value) || preg_match('/^(?:0|[1-9][0-9]*)$/D', $value) !== 1)
        return false;
    $integer = (int)$value;
    return $integer >= 0 && (string)$integer === $value ? $integer : false;
}

function hrm_bank_payment_election_currency($value)
{
    return is_string($value) && preg_match('/^[A-Z]{3}$/D', $value) === 1 ? $value : false;
}

function hrm_bank_payment_election_datetime($value)
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
    return array('value'=>$value, 'epoch'=>(int)$date->format('U'));
}

function hrm_bank_payment_election_percentage($value)
{
    if (!is_string($value)
        || preg_match('/^(?:100\.000000|(?:0|[1-9][0-9]?)\.[0-9]{6})$/D', $value) !== 1)
        return false;
    list($whole, $fraction) = explode('.', $value, 2);
    $micros = ((int)$whole * 1000000) + (int)$fraction;
    if ($micros <= 0 || $micros > 100000000)
        return false;
    return array('value'=>$value, 'micros'=>$micros);
}

function hrm_bank_payment_election_normalize_allocations($allocations)
{
    if (!is_array($allocations) || count($allocations) < 1
        || count($allocations) > HRM_BANK_PAYMENT_ELECTION_MAX_ALLOCATIONS)
        return false;
    $rows = array();
    $priorities = array();
    $accounts = array();
    $total = 0;
    foreach ($allocations as $allocation) {
        if (!is_array($allocation))
            return false;
        $keys = array_keys($allocation);
        sort($keys, SORT_STRING);
        if ($keys !== array('allocation_type','allocation_value','bank_account_row_version','person_bank_account_id','priority'))
            return false;
        $bank_id = hrm_bank_payment_election_positive_int($allocation['person_bank_account_id']);
        $bank_version = hrm_bank_payment_election_positive_int($allocation['bank_account_row_version']);
        $priority = hrm_bank_payment_election_positive_int($allocation['priority']);
        $percentage = hrm_bank_payment_election_percentage($allocation['allocation_value']);
        if ($bank_id === false || $bank_version === false || $priority === false
            || $priority > HRM_BANK_PAYMENT_ELECTION_MAX_ALLOCATIONS
            || (string)$allocation['allocation_type'] !== 'percentage' || $percentage === false)
            return false;
        if (isset($priorities[$priority]) || isset($accounts[$bank_id]))
            return false;
        $priorities[$priority] = true;
        $accounts[$bank_id] = true;
        $total += (int)$percentage['micros'];
        $rows[$priority] = array(
            'person_bank_account_id'=>$bank_id,
            'bank_account_row_version'=>$bank_version,
            'allocation_type'=>'percentage',
            'allocation_value'=>$percentage['value'],
            'priority'=>$priority,
        );
    }
    ksort($rows, SORT_NUMERIC);
    $expected = 1;
    $parts = array();
    foreach ($rows as $priority=>$row) {
        if ((int)$priority !== $expected++)
            return false;
        $parts[] = (int)$priority.':'.(int)$row['person_bank_account_id'].':'.(int)$row['bank_account_row_version']
            .':percentage:'.$row['allocation_value'];
    }
    if ($total !== 100000000)
        return false;
    $canonical = implode(';', $parts);
    return array(
        'rows'=>array_values($rows),
        'canonical'=>$canonical,
        'hash'=>hash('sha256', $canonical),
        'total'=>'100.000000',
    );
}

function hrm_bank_payment_election_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

function hrm_bank_payment_election_permission()
{
    return function_exists('hrm_user_can_access_sensitive_field')
        && defined('HRM_FIELD_RESTRICTED_FINANCIAL')
        && defined('HRM_FIELD_ACTION_PAYMENT_ELECTION')
        && hrm_user_can_access_sensitive_field(HRM_FIELD_RESTRICTED_FINANCIAL, HRM_FIELD_ACTION_PAYMENT_ELECTION);
}

function hrm_bank_payment_election_submit_action_hash($company, $person_id, $currency_code,
    $predecessor_election_id, $predecessor_row_version, $effective_from, $allocation_hash)
{
    $person_id = hrm_bank_payment_election_positive_int($person_id);
    $currency_code = hrm_bank_payment_election_currency($currency_code);
    $predecessor_election_id = hrm_bank_payment_election_nonnegative_int($predecessor_election_id);
    $predecessor_row_version = hrm_bank_payment_election_nonnegative_int($predecessor_row_version);
    $effective = hrm_bank_payment_election_datetime($effective_from);
    if ((int)$company < 0 || $person_id === false || $currency_code === false
        || $predecessor_election_id === false || $predecessor_row_version === false
        || (($predecessor_election_id === 0) !== ($predecessor_row_version === 0))
        || $effective === false || !is_string($allocation_hash)
        || preg_match('/^[a-f0-9]{64}$/D', $allocation_hash) !== 1)
        return false;
    return hash('sha256', HRM_BANK_PAYMENT_ELECTION_HASH_CONTRACT
        .'|v='.HRM_BANK_PAYMENT_ELECTION_HASH_VERSION.'|company='.(int)$company
        .'|action='.HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS.'|person_id='.$person_id
        .'|currency='.$currency_code.'|predecessor_payment_election_id='.$predecessor_election_id
        .'|predecessor_row_version='.$predecessor_row_version.'|effective_from='.$effective_from
        .'|allocation_hash='.$allocation_hash);
}

function hrm_bank_payment_election_approval_action_hash($company, $payment_election_id, $election_row_version,
    $approval_draft_id, $currency_code, $effective_from, $allocation_hash)
{
    $payment_election_id = hrm_bank_payment_election_positive_int($payment_election_id);
    $election_row_version = hrm_bank_payment_election_positive_int($election_row_version);
    $approval_draft_id = hrm_bank_payment_election_positive_int($approval_draft_id);
    $currency_code = hrm_bank_payment_election_currency($currency_code);
    $effective = hrm_bank_payment_election_datetime($effective_from);
    if ((int)$company < 0 || $payment_election_id === false || $election_row_version === false
        || $approval_draft_id === false || $currency_code === false || $effective === false
        || !is_string($allocation_hash) || preg_match('/^[a-f0-9]{64}$/D', $allocation_hash) !== 1)
        return false;
    return hash('sha256', HRM_BANK_PAYMENT_ELECTION_HASH_CONTRACT
        .'|v='.HRM_BANK_PAYMENT_ELECTION_HASH_VERSION.'|company='.(int)$company
        .'|action='.HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS.'|payment_election_id='.$payment_election_id
        .'|election_row_version='.$election_row_version.'|approval_draft_id='.$approval_draft_id
        .'|currency='.$currency_code.'|effective_from='.$effective_from.'|allocation_hash='.$allocation_hash);
}

function hrm_bank_payment_election_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_BANK_PAYMENT_ELECTION') || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_HRM_BANK_PAYMENT_ELECTION)
        || !function_exists('get_approval_workflow_by_type') || !function_exists('get_approval_levels'))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_BANK_PAYMENT_ELECTION);
    if (!$workflow || (int)$workflow['is_active'] !== 1 || (int)$workflow['allow_self_approve'] !== 0)
        return false;
    $levels = get_approval_levels((int)$workflow['id'], true);
    if (!$levels)
        return false;
    $count = 0;
    while ($level = db_fetch($levels)) {
        $count++;
        if ((int)$level['is_active'] !== 1 || (int)$level['role_id'] <= 0
            || (int)$level['min_approvers'] < 1 || (float)$level['amount_threshold'] != 0.0)
            return false;
    }
    return $count > 0;
}

function hrm_bank_payment_election_assurance_event_for_update($session_id, $action_class, $action_hash)
{
    if ((int)$session_id <= 0 || !is_string($action_class) || $action_class === ''
        || !function_exists('federation_assurance_consumer_hex64')
        || !federation_assurance_consumer_hex64($action_hash))
        return false;
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,issued_at,expires_at,consumed_at FROM "
        .TB_PREF."user_session_assurance_events WHERE session_id=".(int)$session_id." AND action_class=".db_escape($action_class)
        ." AND action_hash=".db_escape($action_hash)." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE",
        'could not lock HRM bank payment-election assurance event');
    if (!$result)
        return false;
    if (!function_exists('db_num_rows'))
        return false;
    $count = (int)db_num_rows($result);
    if ($count === 0)
        return null;
    if ($count !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row) ? $row : false;
}

function hrm_bank_payment_election_consume_assurance($company, $action_class, $action_hash, $reason_code, $now_epoch)
{
    if (!function_exists('federation_assurance_consumer_current_session_id')
        || !function_exists('federation_assurance_consumer_custody_for_update')
        || !function_exists('federation_assurance_consumer_current_custody_valid')
        || !function_exists('federation_assurance_consumer_restore_level_one_sql')
        || !function_exists('append_domain_audit_event'))
        return false;
    $session_id = federation_assurance_consumer_current_session_id();
    if ($session_id <= 0)
        return false;
    $custody = federation_assurance_consumer_custody_for_update($session_id);
    if (!federation_assurance_consumer_current_custody_valid($custody, $now_epoch, true))
        return false;
    $event = hrm_bank_payment_election_assurance_event_for_update($session_id, $action_class, $action_hash);
    if (!is_array($event) || (int)$event['assurance_level'] !== 2
        || (string)$event['assurance_method'] !== 'oidc_acr_amr'
        || (string)$event['action_class'] !== $action_class
        || !function_exists('session_registry_hash_equals')
        || !session_registry_hash_equals((string)$event['action_hash'], $action_hash)
        || $event['consumed_at'] !== null)
        return false;
    $issued = strtotime((string)$event['issued_at'].' UTC');
    $expires = strtotime((string)$event['expires_at'].' UTC');
    if ($issued === false || $expires === false || !is_int($now_epoch)
        || $now_epoch < $issued || $now_epoch >= $expires)
        return false;
    $consumed_at = gmdate('Y-m-d H:i:s', $now_epoch);
    if (!db_query("UPDATE ".TB_PREF."user_session_assurance_events SET consumed_at=".db_escape($consumed_at)
        ." WHERE assurance_event_id=".(int)$event['assurance_event_id']." AND consumed_at IS NULL AND expires_at>".db_escape($consumed_at),
        'could not consume HRM bank payment-election assurance') || (int)db_num_affected_rows() !== 1)
        return false;
    $restore = federation_assurance_consumer_restore_level_one_sql($custody, $now_epoch);
    if ($restore === false || !db_query("UPDATE ".TB_PREF."user_sessions SET ".$restore." WHERE id=".$session_id
        ." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'",
        'could not reset HRM bank payment-election session assurance') || (int)db_num_affected_rows() !== 1)
        return false;
    if (append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,'entity_type'=>'security_session_assurance',
        'entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>$session_id),
        'action'=>'security.session.assurance.consumed','reason_code'=>$reason_code,
        'before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),
        'restricted_diff'=>array(
            'action_class'=>$action_class,'action_hash'=>$action_hash,'single_use_event_consumed'=>true,
            'session_assurance_reset_to_level_one'=>true,'bank_values_selected'=>false,
            'payment_instruction_binding_changed'=>false,'payment_execution_authority_changed'=>false,
            'payroll_post_authority_changed'=>false,'bank_transaction_authority_changed'=>false,
            'gl_accounting_authority_changed'=>false))) === false)
        return false;
    return (int)$event['assurance_event_id'];
}

function hrm_bank_payment_election_person_for_update($person_id)
{
    $person_id = hrm_bank_payment_election_positive_int($person_id);
    if ($person_id === false)
        return false;
    $result = db_query("SELECT person_id FROM ".TB_PREF."hrm_persons WHERE person_id=".$person_id." LIMIT 2 FOR UPDATE",
        'could not lock HRM bank payment-election Person');
    if (!$result)
        return false;
    if (!function_exists('db_num_rows') || (int)db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row) ? (int)$row['person_id'] : false;
}

function hrm_bank_payment_election_bank_for_update($bank_id)
{
    $bank_id = hrm_bank_payment_election_positive_int($bank_id);
    if ($bank_id === false)
        return false;
    $result = db_query("SELECT person_bank_account_id,person_id,row_version,effective_from,effective_to,source,verification_status,verification_method,approval_status "
        ."FROM ".TB_PREF."hrm_person_bank_accounts WHERE person_bank_account_id=".$bank_id." LIMIT 2 FOR UPDATE",
        'could not lock HRM bank payment-election bank version');
    if (!$result)
        return false;
    if (!function_exists('db_num_rows') || (int)db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row) ? $row : false;
}

function hrm_bank_payment_election_bank_is_eligible($bank, $person_id, $bank_version, $effective_from)
{
    if (!is_array($bank) || (int)$bank['person_id'] !== (int)$person_id
        || (int)$bank['row_version'] !== (int)$bank_version
        || (string)$bank['source'] !== 'governed_supersession'
        || (string)$bank['verification_status'] !== 'verified'
        || trim((string)$bank['verification_method']) === ''
        || (string)$bank['approval_status'] !== 'approved'
        || $bank['effective_from'] === null || strcmp((string)$bank['effective_from'], (string)$effective_from) > 0)
        return false;
    if ($bank['effective_to'] !== null && strcmp((string)$effective_from, (string)$bank['effective_to']) >= 0)
        return false;
    return true;
}

function hrm_bank_payment_election_current_for_update($person_id, $currency_code)
{
    $result = db_query("SELECT payment_election_id,person_id,currency_code,election_status,approval_status,approval_draft_id,row_version,"
        ."supersedes_payment_election_id,effective_from,effective_to,source,created_by FROM ".TB_PREF."hrm_person_bank_payment_elections "
        ."WHERE person_id=".(int)$person_id." AND currency_code=".db_escape($currency_code)
        ." AND election_status='approved' AND approval_status='approved' AND effective_to IS NULL "
        ."ORDER BY payment_election_id DESC LIMIT 2 FOR UPDATE", 'could not lock current HRM bank payment election');
    if (!$result)
        return false;
    if (!function_exists('db_num_rows'))
        return false;
    $count = (int)db_num_rows($result);
    if ($count === 0)
        return null;
    if ($count !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row) ? $row : false;
}

function hrm_bank_payment_election_pending_for_update($person_id, $currency_code)
{
    $result = db_query("SELECT payment_election_id,person_id,currency_code,election_status,approval_status,approval_draft_id,row_version,"
        ."supersedes_payment_election_id,effective_from,effective_to,source,created_by FROM ".TB_PREF."hrm_person_bank_payment_elections "
        ."WHERE person_id=".(int)$person_id." AND currency_code=".db_escape($currency_code)
        ." AND election_status='pending' AND approval_status='unapproved' ORDER BY payment_election_id DESC LIMIT 2 FOR UPDATE",
        'could not lock pending HRM bank payment election');
    if (!$result)
        return false;
    if (!function_exists('db_num_rows'))
        return false;
    $count = (int)db_num_rows($result);
    if ($count === 0)
        return null;
    if ($count !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row) ? $row : false;
}

function hrm_bank_payment_election_by_id_for_update($election_id)
{
    $election_id = hrm_bank_payment_election_positive_int($election_id);
    if ($election_id === false)
        return false;
    $result = db_query("SELECT payment_election_id,person_id,currency_code,election_status,approval_status,approval_draft_id,row_version,"
        ."supersedes_payment_election_id,effective_from,effective_to,source,created_by FROM ".TB_PREF."hrm_person_bank_payment_elections "
        ."WHERE payment_election_id=".$election_id." LIMIT 2 FOR UPDATE", 'could not lock HRM bank payment election');
    if (!$result)
        return false;
    if (!function_exists('db_num_rows') || (int)db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row) ? $row : false;
}

function hrm_bank_payment_election_allocations_for_update($election_id)
{
    $result = db_query("SELECT payment_allocation_id,payment_election_id,person_bank_account_id,bank_account_row_version,"
        ."allocation_type,allocation_value,priority,created_by FROM ".TB_PREF."hrm_person_bank_payment_allocations "
        ."WHERE payment_election_id=".(int)$election_id." ORDER BY priority,payment_allocation_id FOR UPDATE",
        'could not lock HRM bank payment allocations');
    if (!$result)
        return false;
    $raw = array();
    while ($row = db_fetch_assoc($result)) {
        $raw[] = array(
            'person_bank_account_id'=>(string)(int)$row['person_bank_account_id'],
            'bank_account_row_version'=>(string)(int)$row['bank_account_row_version'],
            'allocation_type'=>(string)$row['allocation_type'],
            'allocation_value'=>(string)$row['allocation_value'],
            'priority'=>(string)(int)$row['priority'],
        );
    }
    return hrm_bank_payment_election_normalize_allocations($raw);
}

function hrm_bank_payment_election_decode_draft_data($raw)
{
    if (!is_string($raw) || $raw === '')
        return false;
    $candidates = array($raw, html_entity_decode($raw, ENT_QUOTES, 'UTF-8'), stripslashes($raw));
    foreach ($candidates as $candidate) {
        $decoded = json_decode($candidate, true);
        if (json_last_error() === JSON_ERROR_NONE && is_array($decoded))
            return $decoded;
    }
    return false;
}

function hrm_bank_payment_election_draft_data($draft_id)
{
    $draft_id = hrm_bank_payment_election_positive_int($draft_id);
    if ($draft_id === false)
        return false;
    $result = db_query("SELECT id,trans_type,draft_data,submitted_by,status FROM ".TB_PREF."approval_drafts WHERE id=".$draft_id
        ." LIMIT 2 FOR UPDATE", 'could not lock HRM bank payment-election approval draft');
    if (!$result)
        return false;
    if (!function_exists('db_num_rows') || (int)db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    if (!is_array($row) || !defined('ST_HRM_BANK_PAYMENT_ELECTION')
        || (int)$row['trans_type'] !== (int)ST_HRM_BANK_PAYMENT_ELECTION)
        return false;
    $data = hrm_bank_payment_election_decode_draft_data((string)$row['draft_data']);
    if (!is_array($data))
        return false;
    $row['decoded'] = $data;
    return $row;
}

function hrm_bank_payment_election_draft_matches($draft_data, $election, $allocation_set, $allow_terminal_successor = false)
{
    if (!is_array($draft_data) || !is_array($election) || !is_array($allocation_set))
        return false;
    $required = array(
        'payment_election_id','election_row_version','person_id','currency_code','effective_from',
        'predecessor_payment_election_id','predecessor_row_version','allocation_hash','maker_submit_action_hash'
    );
    foreach ($required as $key)
        if (!array_key_exists($key, $draft_data))
            return false;
    $election_id = hrm_bank_payment_election_positive_int($draft_data['payment_election_id']);
    $row_version = hrm_bank_payment_election_positive_int($draft_data['election_row_version']);
    $person_id = hrm_bank_payment_election_positive_int($draft_data['person_id']);
    $predecessor_id = hrm_bank_payment_election_nonnegative_int($draft_data['predecessor_payment_election_id']);
    $predecessor_version = hrm_bank_payment_election_nonnegative_int($draft_data['predecessor_row_version']);
    $terminal_successor = $allow_terminal_successor === true
        && $row_version === 1
        && (int)$election['row_version'] === $row_version + 1
        && in_array((string)$election['election_status'], array('approved','rejected','cancelled'), true)
        && (string)$election['approval_status'] === (string)$election['election_status'];
    return $election_id !== false && $row_version !== false && $person_id !== false
        && $predecessor_id !== false && $predecessor_version !== false
        && (($predecessor_id === 0) === ($predecessor_version === 0))
        && $election_id === (int)$election['payment_election_id']
        && ($row_version === (int)$election['row_version'] || $terminal_successor)
        && $person_id === (int)$election['person_id']
        && (string)$draft_data['currency_code'] === (string)$election['currency_code']
        && (string)$draft_data['effective_from'] === (string)$election['effective_from']
        && is_string($draft_data['allocation_hash'])
        && (string)$draft_data['allocation_hash'] === (string)$allocation_set['hash']
        && is_string($draft_data['maker_submit_action_hash'])
        && preg_match('/^[a-f0-9]{64}$/D', $draft_data['maker_submit_action_hash']) === 1;
}

function hrm_bank_payment_election_audit($action, $reason_code, $election, $restricted_diff)
{
    if (!function_exists('append_domain_audit_event') || !is_array($election))
        return false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $safe = array_merge(array(
        'bank_values_selected'=>false,
        'bank_values_revealed'=>false,
        'legacy_employee_bank_authority_changed'=>false,
        'payment_instruction_binding_changed'=>false,
        'payment_execution_authority_changed'=>false,
        'payroll_post_authority_changed'=>false,
        'bank_transaction_authority_changed'=>false,
        'gl_accounting_authority_changed'=>false,
        'historical_payroll_accounting_rows_rewritten'=>0,
    ), is_array($restricted_diff) ? $restricted_diff : array());
    return append_domain_audit_event(array(
        'company_id'=>$company,'legal_entity_id'=>0,'entity_type'=>'hrm_person_bank_payment_election',
        'entity_key'=>array('payment_election_id'=>(int)$election['payment_election_id']),
        'action'=>$action,'reason_code'=>$reason_code,
        'before'=>array(),'after'=>array(
            'election_status'=>(string)$election['election_status'],
            'approval_status'=>(string)$election['approval_status'],
            'row_version'=>(int)$election['row_version']),
        'restricted_diff'=>$safe,
    )) !== false;
}

function hrm_bank_payment_election_exact_retry($pending, $actor, $predecessor_id, $predecessor_version,
    $effective_from, $allocation_set, $submit_action_hash)
{
    if (!is_array($pending) || (int)$pending['created_by'] !== (int)$actor
        || (string)$pending['effective_from'] !== (string)$effective_from
        || (string)$pending['source'] !== 'governed_payment_election'
        || (int)$pending['approval_draft_id'] <= 0)
        return false;
    $stored_allocations = hrm_bank_payment_election_allocations_for_update((int)$pending['payment_election_id']);
    $draft = hrm_bank_payment_election_draft_data((int)$pending['approval_draft_id']);
    if (!is_array($stored_allocations) || !is_array($draft) || !isset($draft['decoded'])
        || !hrm_bank_payment_election_draft_matches($draft['decoded'], $pending, $stored_allocations)
        || (string)$stored_allocations['hash'] !== (string)$allocation_set['hash'])
        return false;
    $data = $draft['decoded'];
    if ((int)$data['predecessor_payment_election_id'] !== (int)$predecessor_id
        || (int)$data['predecessor_row_version'] !== (int)$predecessor_version
        || (string)$data['maker_submit_action_hash'] !== (string)$submit_action_hash)
        return false;
    $approval_hash = hrm_bank_payment_election_approval_action_hash(
        function_exists('user_company') ? (int)user_company() : 0,
        (int)$pending['payment_election_id'], (int)$pending['row_version'],
        (int)$pending['approval_draft_id'], (string)$pending['currency_code'],
        (string)$pending['effective_from'], (string)$stored_allocations['hash']
    );
    return $approval_hash === false ? false : array(
        'payment_election_id'=>(int)$pending['payment_election_id'],
        'approval_draft_id'=>(int)$pending['approval_draft_id'],
        'status'=>(string)$pending['election_status'],
        'row_version'=>(int)$pending['row_version'],
        'submit_action_hash'=>$submit_action_hash,
        'approval_action_hash'=>$approval_hash,
        'exact_retry'=>true,
    );
}

/**
 * Submit a future-effective Person/currency percentage election.
 *
 * Allocations are exact bank-id/row-version references and must sum to the
 * exact six-decimal value 100.000000. A current approved election, when one
 * exists, must be supplied as the exact predecessor. Its closure is deferred
 * until final independent approval.
 */
function submit_hrm_person_bank_payment_election($person_id, $currency_code, $predecessor_election_id,
    $predecessor_row_version, $effective_from, $allocations, $now_epoch = null)
{
    $person_id = hrm_bank_payment_election_positive_int($person_id);
    $currency_code = hrm_bank_payment_election_currency($currency_code);
    $predecessor_election_id = hrm_bank_payment_election_nonnegative_int($predecessor_election_id);
    $predecessor_row_version = hrm_bank_payment_election_nonnegative_int($predecessor_row_version);
    $effective = hrm_bank_payment_election_datetime($effective_from);
    $allocation_set = hrm_bank_payment_election_normalize_allocations($allocations);
    $now = $now_epoch === null ? time() : $now_epoch;
    $actor = hrm_bank_payment_election_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if ($person_id === false || $currency_code === false || $predecessor_election_id === false
        || $predecessor_row_version === false || (($predecessor_election_id === 0) !== ($predecessor_row_version === 0))
        || $effective === false || !is_array($allocation_set) || !is_int($now) || $now <= 0
        || (int)$effective['epoch'] < $now || $actor <= 0 || !hrm_bank_payment_election_permission()
        || !hrm_bank_payment_election_workflow_is_maker_checker())
        return false;
    $submit_action_hash = hrm_bank_payment_election_submit_action_hash(
        $company, $person_id, $currency_code, $predecessor_election_id, $predecessor_row_version,
        $effective_from, $allocation_set['hash']
    );
    if ($submit_action_hash === false)
        return false;

    begin_transaction();
    $ok = false;
    $return_value = false;
    do {
        if (hrm_bank_payment_election_person_for_update($person_id) !== $person_id)
            break;
        $pending = hrm_bank_payment_election_pending_for_update($person_id, $currency_code);
        if ($pending === false)
            break;
        if (is_array($pending)) {
            $retry = hrm_bank_payment_election_exact_retry(
                $pending, $actor, $predecessor_election_id, $predecessor_row_version,
                $effective_from, $allocation_set, $submit_action_hash
            );
            if ($retry !== false) {
                $ok = true;
                $return_value = $retry;
            }
            break;
        }

        $current = hrm_bank_payment_election_current_for_update($person_id, $currency_code);
        if ($current === false)
            break;
        if ($predecessor_election_id === 0) {
            if ($current !== null)
                break;
        } else {
            if (!is_array($current)
                || (int)$current['payment_election_id'] !== $predecessor_election_id
                || (int)$current['row_version'] !== $predecessor_row_version
                || (string)$current['source'] !== 'governed_payment_election'
                || $current['effective_from'] === null
                || strcmp((string)$current['effective_from'], $effective_from) > 0)
                break;
        }

        foreach ($allocation_set['rows'] as $allocation) {
            $bank = hrm_bank_payment_election_bank_for_update((int)$allocation['person_bank_account_id']);
            if (!hrm_bank_payment_election_bank_is_eligible(
                $bank, $person_id, (int)$allocation['bank_account_row_version'], $effective_from
            ))
                break 2;
        }

        $maker_assurance_id = hrm_bank_payment_election_consume_assurance(
            $company, HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS, $submit_action_hash,
            'hrm_fnd_004_person_bank_payment_election_submit', $now
        );
        if ($maker_assurance_id === false)
            break;

        if (!db_query("INSERT INTO ".TB_PREF."hrm_person_bank_payment_elections "
            ."(person_id,currency_code,election_status,approval_status,approval_draft_id,row_version,"
            ."supersedes_payment_election_id,effective_from,effective_to,source,created_by) VALUES ("
            .$person_id.",".db_escape($currency_code).",'pending','unapproved',NULL,1,NULL,"
            .db_escape($effective_from).",NULL,'governed_payment_election',".$actor.")",
            'could not create HRM bank payment election'))
            break;
        $election_id = (int)db_insert_id();
        if ($election_id <= 0)
            break;
        foreach ($allocation_set['rows'] as $allocation) {
            if (!db_query("INSERT INTO ".TB_PREF."hrm_person_bank_payment_allocations "
                ."(payment_election_id,person_bank_account_id,bank_account_row_version,allocation_type,allocation_value,priority,created_by) VALUES ("
                .$election_id.",".(int)$allocation['person_bank_account_id'].",".(int)$allocation['bank_account_row_version']
                .",'percentage',".db_escape($allocation['allocation_value']).",".(int)$allocation['priority'].",".$actor.")",
                'could not create HRM bank payment allocation'))
                break 2;
        }

        $draft_data = array(
            'payment_election_id'=>$election_id,
            'election_row_version'=>1,
            'person_id'=>$person_id,
            'currency_code'=>$currency_code,
            'effective_from'=>$effective_from,
            'predecessor_payment_election_id'=>$predecessor_election_id,
            'predecessor_row_version'=>$predecessor_row_version,
            'allocation_hash'=>$allocation_set['hash'],
            'maker_submit_action_hash'=>$submit_action_hash,
        );
        $approval = get_approval_workflow_service();
        $approval_result = $approval->submit(
            ST_HRM_BANK_PAYMENT_ELECTION,
            $draft_data,
            0,
            array('currency'=>$currency_code,'person_id'=>(string)$person_id,
                'summary'=>sprintf(_('Bank payment election #%d'), $election_id))
        );
        if (!is_array($approval_result) || !isset($approval_result['status'],$approval_result['draft_id'])
            || $approval_result['status'] !== 'pending' || (int)$approval_result['draft_id'] <= 0)
            break;
        $draft_id = (int)$approval_result['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_payment_elections SET approval_draft_id=".$draft_id
            ." WHERE payment_election_id=".$election_id." AND approval_draft_id IS NULL AND row_version=1"
            ." AND election_status='pending' AND approval_status='unapproved'",
            'could not bind HRM bank payment election to approval draft') || (int)db_num_affected_rows() !== 1)
            break;

        $election = array(
            'payment_election_id'=>$election_id,'election_status'=>'pending','approval_status'=>'unapproved','row_version'=>1
        );
        $approval_action_hash = hrm_bank_payment_election_approval_action_hash(
            $company, $election_id, 1, $draft_id, $currency_code, $effective_from, $allocation_set['hash']
        );
        if ($approval_action_hash === false
            || !hrm_bank_payment_election_audit(
                'hrm.person_bank_account.payment_election.submitted',
                'hrm_fnd_004_person_bank_payment_election_submit',
                $election,
                array(
                    'approval_draft_id'=>$draft_id,'maker_user_id'=>$actor,'maker_assurance_event_id'=>$maker_assurance_id,
                    'submit_action_hash'=>$submit_action_hash,'checker_approval_action_hash'=>$approval_action_hash,
                    'person_id'=>$person_id,'currency_code'=>$currency_code,
                    'predecessor_payment_election_id'=>$predecessor_election_id,
                    'predecessor_row_version'=>$predecessor_row_version,
                    'allocation_count'=>count($allocation_set['rows']),'allocation_total'=>$allocation_set['total'],
                    'allocation_hash'=>$allocation_set['hash'],'percentage_only'=>true,
                    'predecessor_closed'=>false,'bank_verification_state_changed'=>false,
                    'maker_checker_required'=>true,'central_approval_workflow'=>true)))
            break;
        $ok = true;
        $return_value = array(
            'payment_election_id'=>$election_id,'approval_draft_id'=>$draft_id,'status'=>'pending','row_version'=>1,
            'submit_action_hash'=>$submit_action_hash,'approval_action_hash'=>$approval_action_hash,'exact_retry'=>false,
        );
    } while (false);

    if ($ok)
        commit_transaction();
    else
        cancel_transaction();
    return $ok ? $return_value : false;
}

function hrm_bank_payment_election_from_draft($draft_data, $draft_id)
{
    $draft_id = hrm_bank_payment_election_positive_int($draft_id);
    if ($draft_id === false || !is_array($draft_data))
        return false;
    $election_id = isset($draft_data['payment_election_id'])
        ? hrm_bank_payment_election_positive_int($draft_data['payment_election_id']) : false;
    if ($election_id === false)
        return false;
    $election = hrm_bank_payment_election_by_id_for_update($election_id);
    if (!is_array($election) || (int)$election['approval_draft_id'] !== $draft_id)
        return false;
    $allocations = hrm_bank_payment_election_allocations_for_update($election_id);
    if (!is_array($allocations) || !hrm_bank_payment_election_draft_matches($draft_data, $election, $allocations, true))
        return false;
    return array('election'=>$election,'allocations'=>$allocations);
}

function execute_approved_bank_payment_election($draft_data, $reserved_trans_no, $draft_id)
{
    if (!hrm_bank_payment_election_permission())
        return false;
    $bound = hrm_bank_payment_election_from_draft($draft_data, $draft_id);
    $actor = hrm_bank_payment_election_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if (!is_array($bound) || $actor <= 0)
        return false;
    $election = $bound['election'];
    $allocations = $bound['allocations'];
    if ((string)$election['election_status'] === 'approved' && (string)$election['approval_status'] === 'approved')
        return (int)$election['payment_election_id'];
    if ((string)$election['election_status'] !== 'pending' || (string)$election['approval_status'] !== 'unapproved'
        || (int)$election['row_version'] !== 1 || (int)$election['created_by'] <= 0 || (int)$election['created_by'] === $actor
        || (string)$election['source'] !== 'governed_payment_election')
        return false;

    foreach ($allocations['rows'] as $allocation) {
        $bank = hrm_bank_payment_election_bank_for_update((int)$allocation['person_bank_account_id']);
        if (!hrm_bank_payment_election_bank_is_eligible(
            $bank, (int)$election['person_id'], (int)$allocation['bank_account_row_version'], (string)$election['effective_from']
        ))
            return false;
    }

    $predecessor_id = (int)$draft_data['predecessor_payment_election_id'];
    $predecessor_version = (int)$draft_data['predecessor_row_version'];
    $current = hrm_bank_payment_election_current_for_update((int)$election['person_id'], (string)$election['currency_code']);
    if ($current === false)
        return false;
    if ($predecessor_id === 0) {
        if ($current !== null)
            return false;
    } else {
        if (!is_array($current) || (int)$current['payment_election_id'] !== $predecessor_id
            || (int)$current['row_version'] !== $predecessor_version
            || (string)$current['source'] !== 'governed_payment_election'
            || $current['effective_to'] !== null || $current['effective_from'] === null
            || strcmp((string)$current['effective_from'], (string)$election['effective_from']) > 0)
            return false;
    }

    $approval_action_hash = hrm_bank_payment_election_approval_action_hash(
        $company, (int)$election['payment_election_id'], (int)$election['row_version'], (int)$draft_id,
        (string)$election['currency_code'], (string)$election['effective_from'], (string)$allocations['hash']
    );
    $checker_assurance_id = $approval_action_hash === false ? false : hrm_bank_payment_election_consume_assurance(
        $company, HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS, $approval_action_hash,
        'hrm_fnd_004_person_bank_payment_election_approval', time()
    );
    if ($checker_assurance_id === false)
        return false;

    if ($predecessor_id > 0) {
        if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_payment_elections SET effective_to=".db_escape((string)$election['effective_from'])
            .",row_version=row_version+1 WHERE payment_election_id=".$predecessor_id." AND row_version=".$predecessor_version
            ." AND election_status='approved' AND approval_status='approved' AND effective_to IS NULL",
            'could not close predecessor HRM bank payment election') || (int)db_num_affected_rows() !== 1)
            return false;
    }
    if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_payment_elections SET election_status='approved',approval_status='approved',"
        ."supersedes_payment_election_id=".($predecessor_id > 0 ? (string)$predecessor_id : 'NULL').",row_version=row_version+1 "
        ."WHERE payment_election_id=".(int)$election['payment_election_id']." AND row_version=1 AND election_status='pending' "
        ."AND approval_status='unapproved' AND approval_draft_id=".(int)$draft_id." AND supersedes_payment_election_id IS NULL",
        'could not approve HRM bank payment election') || (int)db_num_affected_rows() !== 1)
        return false;
    $election['election_status'] = 'approved';
    $election['approval_status'] = 'approved';
    $election['row_version'] = 2;
    $election['supersedes_payment_election_id'] = $predecessor_id > 0 ? $predecessor_id : null;
    if (!hrm_bank_payment_election_audit(
        'hrm.person_bank_account.payment_election.approved',
        'hrm_fnd_004_person_bank_payment_election_approval',
        $election,
        array(
            'approval_draft_id'=>(int)$draft_id,'maker_user_id'=>(int)$election['created_by'],'checker_user_id'=>$actor,
            'checker_assurance_event_id'=>$checker_assurance_id,'approval_action_hash'=>$approval_action_hash,
            'maker_checker_separated'=>true,'predecessor_payment_election_id'=>$predecessor_id,
            'predecessor_closed'=>$predecessor_id > 0,'allocation_count'=>count($allocations['rows']),
            'allocation_total'=>$allocations['total'],'allocation_hash'=>$allocations['hash'],
            'bank_verification_state_changed'=>false)))
        return false;
    return (int)$election['payment_election_id'];
}

function reject_bank_payment_election_draft($draft_data, $draft_id, $comments)
{
    if (!hrm_bank_payment_election_permission())
        return false;
    $bound = hrm_bank_payment_election_from_draft($draft_data, $draft_id);
    $actor = hrm_bank_payment_election_actor_id();
    if (!is_array($bound) || $actor <= 0)
        return false;
    $election = $bound['election'];
    if ((string)$election['election_status'] === 'rejected' && (string)$election['approval_status'] === 'rejected')
        return (int)$election['payment_election_id'];
    if ((string)$election['election_status'] !== 'pending' || (string)$election['approval_status'] !== 'unapproved'
        || (int)$election['row_version'] !== 1 || (int)$election['created_by'] === $actor)
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_payment_elections SET election_status='rejected',approval_status='rejected',"
        ."row_version=row_version+1 WHERE payment_election_id=".(int)$election['payment_election_id']
        ." AND row_version=1 AND election_status='pending' AND approval_status='unapproved' AND approval_draft_id=".(int)$draft_id,
        'could not reject HRM bank payment election') || (int)db_num_affected_rows() !== 1)
        return false;
    $election['election_status'] = 'rejected';
    $election['approval_status'] = 'rejected';
    $election['row_version'] = 2;
    return hrm_bank_payment_election_audit(
        'hrm.person_bank_account.payment_election.rejected',
        'hrm_fnd_004_person_bank_payment_election_rejection',
        $election,
        array('approval_draft_id'=>(int)$draft_id,'checker_user_id'=>$actor,
            'maker_checker_separated'=>true,'predecessor_closed'=>false,'comment_text_stored'=>false,
            'allocation_hash'=>$bound['allocations']['hash']))
        ? (int)$election['payment_election_id'] : false;
}

function cancel_bank_payment_election_draft($draft_data, $draft_id, $comments)
{
    if (!hrm_bank_payment_election_permission())
        return false;
    $bound = hrm_bank_payment_election_from_draft($draft_data, $draft_id);
    $actor = hrm_bank_payment_election_actor_id();
    if (!is_array($bound) || $actor <= 0)
        return false;
    $election = $bound['election'];
    if ((string)$election['election_status'] === 'cancelled' && (string)$election['approval_status'] === 'cancelled')
        return (int)$election['payment_election_id'];
    if ((string)$election['election_status'] !== 'pending' || (string)$election['approval_status'] !== 'unapproved'
        || (int)$election['row_version'] !== 1 || (int)$election['created_by'] !== $actor)
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_person_bank_payment_elections SET election_status='cancelled',approval_status='cancelled',"
        ."row_version=row_version+1 WHERE payment_election_id=".(int)$election['payment_election_id']
        ." AND row_version=1 AND election_status='pending' AND approval_status='unapproved' AND approval_draft_id=".(int)$draft_id,
        'could not cancel HRM bank payment election') || (int)db_num_affected_rows() !== 1)
        return false;
    $election['election_status'] = 'cancelled';
    $election['approval_status'] = 'cancelled';
    $election['row_version'] = 2;
    return hrm_bank_payment_election_audit(
        'hrm.person_bank_account.payment_election.cancelled',
        'hrm_fnd_004_person_bank_payment_election_cancellation',
        $election,
        array('approval_draft_id'=>(int)$draft_id,'cancelled_by_maker'=>true,
            'predecessor_closed'=>false,'comment_text_stored'=>false,
            'allocation_hash'=>$bound['allocations']['hash']))
        ? (int)$election['payment_election_id'] : false;
}
