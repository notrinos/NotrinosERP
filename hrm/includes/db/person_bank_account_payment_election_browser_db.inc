<?php
/** HRM-FND-004 Person bank payment-election browser read/intent helpers. */
$hrm_bank_payment_election_browser_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_bank_payment_election_browser_command = $hrm_bank_payment_election_browser_root.'/hrm/includes/db/person_bank_account_payment_election_db.inc';
if (is_file($hrm_bank_payment_election_browser_command)) include_once($hrm_bank_payment_election_browser_command);
unset($hrm_bank_payment_election_browser_root, $hrm_bank_payment_election_browser_command);

function hrm_bank_payment_election_browser_permission()
{
    return function_exists('hrm_bank_payment_election_permission') && hrm_bank_payment_election_permission();
}

function hrm_bank_payment_election_browser_employee_ref($value)
{
    if (!is_scalar($value)) return false;
    $value = trim((string)$value);
    return $value !== '' && strlen($value) <= 64 && preg_match('/^[A-Za-z0-9_.:@-]+$/D', $value) === 1 ? $value : false;
}

/** Resolve exactly one current Person for the selected employee/Worker containment. */
function hrm_bank_payment_election_browser_person($employee_ref)
{
    if (!hrm_bank_payment_election_browser_permission() || !defined('TB_PREF')) return false;
    $employee_ref = hrm_bank_payment_election_browser_employee_ref($employee_ref);
    if ($employee_ref === false) return false;
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $employee = is_numeric($employee_ref) ? (int)$employee_ref : $employee_ref;
    $sql = "SELECT DISTINCT w.person_id FROM ".TB_PREF."hrm_employee_worker_map m "
        ."INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_persons p ON p.person_id=w.person_id "
        ."WHERE m.".$column."=".db_escape($employee)." AND m.retired_at IS NULL ORDER BY w.person_id LIMIT 2";
    $result = db_query($sql, 'could not resolve Person for bank payment election browser route');
    if (!$result) return false;
    $rows = array(); while ($row = db_fetch_assoc($result)) $rows[] = $row;
    return count($rows) === 1 && (int)$rows[0]['person_id'] > 0 ? (int)$rows[0]['person_id'] : false;
}

/** List exact verified/approved same-Person bank versions with masked metadata only. */
function get_hrm_bank_payment_election_browser_bank_candidates_masked($employee_ref, $effective_from)
{
    $person_id = hrm_bank_payment_election_browser_person($employee_ref);
    $effective = function_exists('hrm_bank_payment_election_datetime') ? hrm_bank_payment_election_datetime($effective_from) : false;
    if ($person_id === false || $effective === false || !defined('TB_PREF')) return false;
    $sql = "SELECT person_bank_account_id,row_version,masked_account,masked_routing,payment_method,verification_method,effective_from,effective_to "
        ."FROM ".TB_PREF."hrm_person_bank_accounts WHERE person_id=".$person_id
        ." AND source='governed_supersession' AND verification_status='verified' AND verification_method<>'' "
        ."AND approval_status='approved' AND effective_from IS NOT NULL AND effective_from<=".db_escape($effective_from)
        ." AND (effective_to IS NULL OR effective_to>".db_escape($effective_from).") ORDER BY person_bank_account_id";
    $result = db_query($sql, 'could not list masked banks eligible for payment election');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = array(
            'person_bank_account_id'=>(int)$row['person_bank_account_id'], 'bank_account_row_version'=>(int)$row['row_version'],
            'masked_account'=>(string)$row['masked_account'], 'masked_routing'=>(string)$row['masked_routing'],
            'payment_method'=>(int)$row['payment_method'], 'verification_method'=>(string)$row['verification_method'],
            'effective_from'=>(string)$row['effective_from'], 'effective_to'=>$row['effective_to'] === null ? null : (string)$row['effective_to'],
        );
    }
    return $rows;
}

function hrm_bank_payment_election_browser_current($person_id, $currency_code)
{
    $person_id = function_exists('hrm_bank_payment_election_positive_int') ? hrm_bank_payment_election_positive_int($person_id) : false;
    $currency_code = function_exists('hrm_bank_payment_election_currency') ? hrm_bank_payment_election_currency($currency_code) : false;
    if ($person_id === false || $currency_code === false || !defined('TB_PREF')) return false;
    $result = db_query("SELECT payment_election_id,row_version,effective_from FROM ".TB_PREF."hrm_person_bank_payment_elections "
        ."WHERE person_id=".$person_id." AND currency_code=".db_escape($currency_code)
        ." AND election_status='approved' AND approval_status='approved' AND effective_to IS NULL ORDER BY payment_election_id DESC LIMIT 2",
        'could not read current Person bank payment election');
    if (!$result) return false;
    $rows = array(); while ($row = db_fetch_assoc($result)) $rows[] = $row;
    if (count($rows) === 0) return null;
    if (count($rows) !== 1) return false;
    return array('payment_election_id'=>(int)$rows[0]['payment_election_id'],'row_version'=>(int)$rows[0]['row_version'],
        'effective_from'=>(string)$rows[0]['effective_from']);
}

/** Prepare the browser's deliberately minimal one-bank 100% election action. */
function hrm_bank_payment_election_browser_prepare_submit($employee_ref, $currency_code, $effective_from, $bank_id, $bank_row_version)
{
    $person_id = hrm_bank_payment_election_browser_person($employee_ref);
    $currency_code = function_exists('hrm_bank_payment_election_currency') ? hrm_bank_payment_election_currency($currency_code) : false;
    $effective = function_exists('hrm_bank_payment_election_datetime') ? hrm_bank_payment_election_datetime($effective_from) : false;
    $bank_id = function_exists('hrm_bank_payment_election_positive_int') ? hrm_bank_payment_election_positive_int($bank_id) : false;
    $bank_row_version = function_exists('hrm_bank_payment_election_positive_int') ? hrm_bank_payment_election_positive_int($bank_row_version) : false;
    if ($person_id === false || $currency_code === false || $effective === false || $bank_id === false || $bank_row_version === false
        || (int)$effective['epoch'] < time()) return false;
    $eligible = get_hrm_bank_payment_election_browser_bank_candidates_masked($employee_ref, $effective_from);
    if (!is_array($eligible)) return false;
    $selected = false;
    foreach ($eligible as $row) if ((int)$row['person_bank_account_id'] === $bank_id && (int)$row['bank_account_row_version'] === $bank_row_version) {
        if ($selected !== false) return false; $selected = $row;
    }
    if (!is_array($selected)) return false;
    $current = hrm_bank_payment_election_browser_current($person_id, $currency_code);
    if ($current === false) return false;
    if (is_array($current) && strcmp((string)$current['effective_from'], (string)$effective_from) > 0) return false;
    $predecessor_id = is_array($current) ? (int)$current['payment_election_id'] : 0;
    $predecessor_version = is_array($current) ? (int)$current['row_version'] : 0;
    $allocations = array(array('person_bank_account_id'=>(string)$bank_id,'bank_account_row_version'=>(string)$bank_row_version,
        'allocation_type'=>'percentage','allocation_value'=>'100.000000','priority'=>'1'));
    $allocation_set = hrm_bank_payment_election_normalize_allocations($allocations);
    if (!is_array($allocation_set)) return false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $action_hash = hrm_bank_payment_election_submit_action_hash($company, $person_id, $currency_code,
        $predecessor_id, $predecessor_version, $effective_from, (string)$allocation_set['hash']);
    if (!is_string($action_hash) || preg_match('/^[a-f0-9]{64}$/D',$action_hash)!==1) return false;
    return array('company'=>$company,'employee_ref'=>(string)$employee_ref,'person_id'=>$person_id,'currency_code'=>$currency_code,
        'effective_from'=>$effective_from,'predecessor_payment_election_id'=>$predecessor_id,'predecessor_row_version'=>$predecessor_version,
        'person_bank_account_id'=>$bank_id,'bank_account_row_version'=>$bank_row_version,'masked_account'=>(string)$selected['masked_account'],
        'masked_routing'=>(string)$selected['masked_routing'],'allocation_hash'=>(string)$allocation_set['hash'],'allocations'=>$allocations,
        'action_class'=>HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS,'action_hash'=>$action_hash,
        'object_id'=>$person_id,'object_row_version'=>0);
}

function hrm_bank_payment_election_browser_read_allocations($election_id)
{
    $election_id = function_exists('hrm_bank_payment_election_positive_int') ? hrm_bank_payment_election_positive_int($election_id) : false;
    if ($election_id === false || !defined('TB_PREF')) return false;
    $result = db_query("SELECT person_bank_account_id,bank_account_row_version,allocation_type,allocation_value,priority FROM "
        .TB_PREF."hrm_person_bank_payment_allocations WHERE payment_election_id=".$election_id." ORDER BY priority,payment_allocation_id",
        'could not read Person bank payment-election allocations');
    if (!$result) return false;
    $rows=array(); while($row=db_fetch_assoc($result)) $rows[]=array(
        'person_bank_account_id'=>(string)(int)$row['person_bank_account_id'],'bank_account_row_version'=>(string)(int)$row['bank_account_row_version'],
        'allocation_type'=>(string)$row['allocation_type'],'allocation_value'=>(string)$row['allocation_value'],'priority'=>(string)(int)$row['priority']);
    return hrm_bank_payment_election_normalize_allocations($rows);
}

/**
 * Probe maker recovery before issuing fresh assurance.
 *
 * Return values are intentionally tri-state:
 *   - null: there is no pending Person/currency election; fresh assurance may start;
 *   - array exact_retry=true: the accepted command can return the existing draft idempotently;
 *   - array exact_retry=false: a different pending intent exists and must block a new submission;
 *   - false: custody could not be proven and the route must fail closed.
 */
function hrm_bank_payment_election_browser_retry_probe($prepared)
{
    if (!hrm_bank_payment_election_browser_permission() || !defined('TB_PREF') || !is_array($prepared)) return false;
    foreach (array('person_id','currency_code','effective_from','predecessor_payment_election_id','predecessor_row_version',
        'allocation_hash','action_hash') as $key) if (!array_key_exists($key,$prepared)) return false;
    $person_id=hrm_bank_payment_election_positive_int($prepared['person_id']);
    $currency_code=hrm_bank_payment_election_currency($prepared['currency_code']);
    $actor=function_exists('hrm_bank_payment_election_actor_id')?hrm_bank_payment_election_actor_id():0;
    if($person_id===false||$currency_code===false||$actor<=0||!is_string($prepared['allocation_hash'])
        ||preg_match('/^[a-f0-9]{64}$/D',(string)$prepared['allocation_hash'])!==1
        ||!is_string($prepared['action_hash'])||preg_match('/^[a-f0-9]{64}$/D',(string)$prepared['action_hash'])!==1)return false;
    $result=db_query("SELECT payment_election_id,person_id,currency_code,election_status,approval_status,approval_draft_id,row_version,"
        ."supersedes_payment_election_id,effective_from,effective_to,source,created_by FROM ".TB_PREF."hrm_person_bank_payment_elections "
        ."WHERE person_id=".$person_id." AND currency_code=".db_escape($currency_code)
        ." AND election_status='pending' AND approval_status='unapproved' ORDER BY payment_election_id DESC LIMIT 2",
        'could not probe pending HRM bank payment election before assurance');
    if(!$result)return false;$rows=array();while($row=db_fetch_assoc($result))$rows[]=$row;
    if(count($rows)===0)return null;
    if(count($rows)!==1)return false;
    $pending=$rows[0];
    $blocked=array('exact_retry'=>false,'payment_election_id'=>(int)$pending['payment_election_id'],
        'approval_draft_id'=>(int)$pending['approval_draft_id']);
    if((int)$pending['created_by']!==$actor||(string)$pending['effective_from']!==(string)$prepared['effective_from']
        ||(string)$pending['source']!=='governed_payment_election'||(int)$pending['approval_draft_id']<=0)return $blocked;
    $allocations=hrm_bank_payment_election_browser_read_allocations((int)$pending['payment_election_id']);
    if(!is_array($allocations)||(string)$allocations['hash']!==(string)$prepared['allocation_hash'])return $blocked;
    $draft_id=(int)$pending['approval_draft_id'];
    $draft_result=db_query("SELECT id,trans_type,draft_data,submitted_by,status FROM ".TB_PREF."approval_drafts WHERE id=".$draft_id." LIMIT 2",
        'could not probe HRM bank payment-election approval draft before assurance');
    if(!$draft_result)return false;$draft_rows=array();while($row=db_fetch_assoc($draft_result))$draft_rows[]=$row;
    if(count($draft_rows)!==1)return false;$draft=$draft_rows[0];
    if(!defined('ST_HRM_BANK_PAYMENT_ELECTION')||(int)$draft['trans_type']!==(int)ST_HRM_BANK_PAYMENT_ELECTION
        ||(int)$draft['submitted_by']!==$actor)return $blocked;
    $decoded=hrm_bank_payment_election_decode_draft_data((string)$draft['draft_data']);
    if(!is_array($decoded)||!hrm_bank_payment_election_draft_matches($decoded,$pending,$allocations)
        ||(int)($decoded['predecessor_payment_election_id']??-1)!==(int)$prepared['predecessor_payment_election_id']
        ||(int)($decoded['predecessor_row_version']??-1)!==(int)$prepared['predecessor_row_version']
        ||!isset($decoded['maker_submit_action_hash'])
        ||!hash_equals((string)$prepared['action_hash'],(string)$decoded['maker_submit_action_hash']))return $blocked;
    $approval_hash=hrm_bank_payment_election_approval_action_hash(function_exists('user_company')?(int)user_company():0,
        (int)$pending['payment_election_id'],(int)$pending['row_version'],$draft_id,(string)$pending['currency_code'],
        (string)$pending['effective_from'],(string)$allocations['hash']);
    if($approval_hash===false)return false;
    return array('payment_election_id'=>(int)$pending['payment_election_id'],'approval_draft_id'=>$draft_id,
        'status'=>(string)$pending['election_status'],'row_version'=>(int)$pending['row_version'],
        'submit_action_hash'=>(string)$prepared['action_hash'],'approval_action_hash'=>$approval_hash,'exact_retry'=>true);
}

/** Resolve one exact pending election that the current independent checker may act on. */
function hrm_bank_payment_election_browser_checker_candidate($employee_ref, $payment_election_id, $row_version)
{
    if (!hrm_bank_payment_election_browser_permission() || !defined('TB_PREF') || !function_exists('get_approval_workflow_service')
        || !function_exists('get_approval_draft')) return false;
    $person_id = hrm_bank_payment_election_browser_person($employee_ref);
    $payment_election_id = hrm_bank_payment_election_positive_int($payment_election_id);
    $row_version = hrm_bank_payment_election_positive_int($row_version);
    $actor = function_exists('hrm_bank_payment_election_actor_id') ? hrm_bank_payment_election_actor_id() : 0;
    if ($person_id===false||$payment_election_id===false||$row_version===false||$actor<=0) return false;
    $result=db_query("SELECT payment_election_id,person_id,currency_code,election_status,approval_status,approval_draft_id,row_version,effective_from,created_by,source "
        ."FROM ".TB_PREF."hrm_person_bank_payment_elections WHERE payment_election_id=".$payment_election_id
        ." AND person_id=".$person_id." AND row_version=".$row_version." AND election_status='pending' AND approval_status='unapproved' "
        ."AND source='governed_payment_election' AND created_by>0 AND created_by<>".$actor." LIMIT 2",
        'could not resolve pending Person bank payment election for checker');
    if(!$result)return false;$rows=array();while($row=db_fetch_assoc($result))$rows[]=$row;if(count($rows)!==1)return false;$election=$rows[0];
    $draft_id=(int)$election['approval_draft_id']; if($draft_id<=0)return false;
    $draft=get_approval_draft($draft_id); if(!is_array($draft)||!defined('ST_HRM_BANK_PAYMENT_ELECTION')
        ||(int)$draft['trans_type']!==(int)ST_HRM_BANK_PAYMENT_ELECTION||(int)$draft['status']!==APPROVAL_STATUS_PENDING
        ||(int)$draft['submitted_by']!==(int)$election['created_by']||(int)$draft['allow_self_approve']!==0)return false;
    $approval=get_approval_workflow_service(); if(!is_object($approval)||!$approval->canActOnDraft($draft_id))return false;
    $current_level=(int)$draft['current_level'];
    if(!function_exists('get_max_approval_level')||!function_exists('get_min_approvers_for_level')||!function_exists('count_approval_actions_at_level'))return false;
    $max_level=(int)get_max_approval_level((int)$draft['workflow_id']);
    $min_required=(int)get_min_approvers_for_level((int)$draft['workflow_id'],$current_level);
    $approved_count=(int)count_approval_actions_at_level($draft_id,$current_level);
    if($current_level<=0||$max_level<=0||$current_level!==$max_level||$min_required<1||$approved_count+1<$min_required)return false;
    $allocations=hrm_bank_payment_election_browser_read_allocations($payment_election_id); if(!is_array($allocations))return false;
    $draft_data=hrm_bank_payment_election_decode_draft_data((string)$draft['draft_data']);
    if(!hrm_bank_payment_election_draft_matches($draft_data,$election,$allocations,false))return false;
    $company=function_exists('user_company')?(int)user_company():0;
    $action_hash=hrm_bank_payment_election_approval_action_hash($company,$payment_election_id,$row_version,$draft_id,
        (string)$election['currency_code'],(string)$election['effective_from'],(string)$allocations['hash']);
    if(!is_string($action_hash)||preg_match('/^[a-f0-9]{64}$/D',$action_hash)!==1)return false;
    return array('company'=>$company,'employee_ref'=>(string)$employee_ref,'person_id'=>$person_id,'payment_election_id'=>$payment_election_id,
        'row_version'=>$row_version,'approval_draft_id'=>$draft_id,'currency_code'=>(string)$election['currency_code'],
        'effective_from'=>(string)$election['effective_from'],'maker_user_id'=>(int)$election['created_by'],
        'allocation_hash'=>(string)$allocations['hash'],'allocation_count'=>count($allocations['rows']),
        'action_class'=>HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS,'action_hash'=>$action_hash,
        'object_id'=>$payment_election_id,'object_row_version'=>$row_version);
}

function get_hrm_bank_payment_election_browser_checker_candidates($employee_ref)
{
    $person_id=hrm_bank_payment_election_browser_person($employee_ref); if($person_id===false||!defined('TB_PREF'))return false;
    $result=db_query("SELECT payment_election_id,row_version FROM ".TB_PREF."hrm_person_bank_payment_elections WHERE person_id=".$person_id
        ." AND election_status='pending' AND approval_status='unapproved' AND source='governed_payment_election' ORDER BY payment_election_id",
        'could not list pending Person bank payment elections'); if(!$result)return false;
    $rows=array(); while($row=db_fetch_assoc($result)){ $candidate=hrm_bank_payment_election_browser_checker_candidate($employee_ref,(int)$row['payment_election_id'],(int)$row['row_version']); if(is_array($candidate))$rows[]=$candidate; }
    return $rows;
}

/** Revalidate generic OIDC step-up context without accepting browser-controlled return or secret material. */
function hrm_bank_payment_election_browser_step_up_context($employee_ref, $action_class, $object_id, $object_row_version, $action_hash)
{
    if (!hrm_bank_payment_election_browser_permission() || !is_string($action_hash) || preg_match('/^[a-f0-9]{64}$/D',$action_hash)!==1) return false;
    if ((string)$action_class === HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS) {
        $person_id=hrm_bank_payment_election_browser_person($employee_ref);
        $object_id=hrm_bank_payment_election_positive_int($object_id);
        if($person_id===false||$object_id===false||$person_id!==$object_id||(int)$object_row_version!==0)return false;
        return array('employee_ref'=>(string)$employee_ref,'object_id'=>$object_id,'object_row_version'=>0,
            'action_class'=>HRM_BANK_PAYMENT_ELECTION_SUBMIT_ACTION_CLASS,'action_hash'=>$action_hash);
    }
    if ((string)$action_class === HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS) {
        $candidate=hrm_bank_payment_election_browser_checker_candidate($employee_ref,$object_id,$object_row_version);
        if(!is_array($candidate)||!hash_equals((string)$candidate['action_hash'],$action_hash))return false;
        return array('employee_ref'=>(string)$employee_ref,'object_id'=>(int)$candidate['payment_election_id'],
            'object_row_version'=>(int)$candidate['row_version'],'action_class'=>HRM_BANK_PAYMENT_ELECTION_APPROVE_ACTION_CLASS,
            'action_hash'=>(string)$candidate['action_hash']);
    }
    return false;
}
