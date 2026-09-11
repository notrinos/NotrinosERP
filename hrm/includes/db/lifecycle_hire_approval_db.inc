<?php
/**
 * HRM-FND-005 Employee Hire central maker/checker approval, atomic execution,
 * and staged-payload key rotation/recovery foundation.
 *
 * This source deliberately exposes no browser/import route. Approval JSON holds
 * only redacted command/hash/version metadata. Confidential onboarding data is
 * decrypted only while the terminal approval transaction owns the command,
 * payload, employee-code uniqueness lock and aggregate creation boundary.
 */

include_once($path_to_root.'/hrm/includes/db/lifecycle_hire_payload_db.inc');
if (!function_exists('add_employee'))
    include_once($path_to_root.'/hrm/includes/db/employee_db.inc');

if (!defined('HRM_LIFECYCLE_HIRE_APPROVAL_REASON'))
    define('HRM_LIFECYCLE_HIRE_APPROVAL_REASON', 'hrm_fnd_005_employee_hire_approval_execution_foundation');
if (!defined('HRM_LIFECYCLE_HIRE_APPROVAL_DRAFT_VERSION'))
    define('HRM_LIFECYCLE_HIRE_APPROVAL_DRAFT_VERSION', 1);

/** @return bool */
function hrm_lifecycle_hire_approval_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_LIFECYCLE_HIRE') || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_HRM_LIFECYCLE_HIRE)
        || !function_exists('get_approval_workflow_by_type') || !function_exists('get_approval_levels'))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_LIFECYCLE_HIRE);
    if (!$workflow || (int)$workflow['is_active'] !== 1 || (int)$workflow['allow_self_approve'] !== 0)
        return false;
    $levels = get_approval_levels((int)$workflow['id'], true);
    if (!$levels)
        return false;
    $count = 0;
    while ($level = db_fetch($levels)) {
        $count++;
        if ((int)$level['is_active'] !== 1 || (int)$level['role_id'] <= 0
            || (int)$level['min_approvers'] < 1 || (float)$level['amount_threshold'] != 0.0
            || (isset($level['conditions']) && trim((string)$level['conditions']) !== ''))
            return false;
    }
    return $count > 0;
}

/** @return array|false */
function hrm_lifecycle_hire_approval_draft_data($loaded, $command_row_version)
{
    if (!is_array($loaded) || !isset($loaded['command'],$loaded['payload_meta'],$loaded['payload'])
        || !is_array($loaded['command']) || !is_array($loaded['payload_meta']) || !is_array($loaded['payload']))
        return false;
    $command = $loaded['command'];
    $meta = $loaded['payload_meta'];
    $payload = $loaded['payload'];
    $command_row_version = hrm_lifecycle_hire_payload_positive_int($command_row_version);
    if ($command_row_version === false || (int)$command['hire_command_id'] <= 0
        || preg_match('/^[a-f0-9]{64}$/D', (string)$command['request_hash']) !== 1
        || preg_match('/^[a-f0-9]{64}$/D', (string)$meta['ciphertext_sha256']) !== 1
        || trim((string)$payload['employee_id']) === '')
        return false;
    return array(
        'draft_contract_version'=>HRM_LIFECYCLE_HIRE_APPROVAL_DRAFT_VERSION,
        'hire_command_id'=>(int)$command['hire_command_id'],
        'command_row_version'=>(int)$command_row_version,
        'payload_row_version'=>(int)$meta['row_version'],
        'request_hash'=>(string)$command['request_hash'],
        'ciphertext_sha256'=>(string)$meta['ciphertext_sha256'],
        'key_id'=>(string)$meta['key_id'],
        'key_generation'=>(int)$meta['key_generation'],
        'effective_date'=>substr((string)$command['effective_at'], 0, 10),
        'employee_code_hash'=>hash('sha256', (string)$payload['employee_id']),
        'confidential_payload_in_draft'=>false,
    );
}

/** @return bool */
function hrm_lifecycle_hire_draft_matches($draft_data, $loaded, $expected_command_row_version)
{
    $expected = hrm_lifecycle_hire_approval_draft_data($loaded, $expected_command_row_version);
    if ($expected === false || !is_array($draft_data))
        return false;
    $expected_keys = array_keys($expected);
    $actual_keys = array_keys($draft_data);
    sort($expected_keys); sort($actual_keys);
    if ($expected_keys !== $actual_keys)
        return false;
    foreach ($expected as $key=>$value) {
        if (is_bool($value)) {
            if (!is_bool($draft_data[$key]) || $draft_data[$key] !== $value) return false;
        } elseif (is_int($value)) {
            if ((int)$draft_data[$key] !== $value || !is_numeric($draft_data[$key])) return false;
        } elseif (!function_exists('domain_audit_hash_equals')
            || !domain_audit_hash_equals((string)$draft_data[$key], (string)$value)) {
            return false;
        }
    }
    return true;
}

/** @return array|false */
function hrm_lifecycle_hire_approval_draft_for_update($draft_id)
{
    $draft_id = hrm_lifecycle_hire_payload_positive_int($draft_id);
    if ($draft_id === false)
        return false;
    $result = db_query("SELECT id,trans_type,draft_data,status,submitted_by FROM ".TB_PREF."approval_drafts "
        ."WHERE id=".$draft_id." LIMIT 1 FOR UPDATE", 'could not lock Employee Hire approval draft');
    return $result ? db_fetch_assoc($result) : false;
}

/** @return bool */
function hrm_lifecycle_hire_employee_code_available_for_execution($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || strlen($employee_id) > 20)
        return false;
    $result = db_query("SELECT employee_number FROM ".TB_PREF."employees WHERE employee_id=".db_escape($employee_id)
        ." LIMIT 1 FOR UPDATE", 'could not lock Employee Hire employee-code uniqueness range');
    return $result && db_fetch_assoc($result) ? false : (bool)$result;
}

/** @return bool */
function hrm_lifecycle_hire_approval_audit($action, $command, $payload_meta, $restricted_diff=array())
{
    if (!function_exists('append_domain_audit_event') || !is_array($command) || !is_array($payload_meta))
        return false;
    $safe = array_merge(array(
        'fixed_command_type'=>'employee_hire','central_maker_checker'=>true,'approval_registered'=>true,
        'approval_draft_contains_confidential_payload'=>false,'payload_ciphertext_audited'=>false,
        'browser_route_added'=>false,'csv_import_route_added'=>false,'same_code_rehire_released'=>false,
        'payroll_rows_mutated'=>0,'payment_rows_mutated'=>0,'bank_transaction_rows_mutated'=>0,
        'gl_accounting_rows_mutated'=>0,
    ), is_array($restricted_diff) ? $restricted_diff : array());
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_lifecycle_hire_command','entity_key'=>array('hire_command_id'=>(int)$command['hire_command_id']),
        'action'=>(string)$action,'reason_code'=>HRM_LIFECYCLE_HIRE_APPROVAL_REASON,
        'before'=>array(),'after'=>array('command_status'=>(string)$command['command_status'],
            'command_row_version'=>(int)$command['row_version'],'payload_state'=>(string)$payload_meta['payload_state'],
            'payload_row_version'=>(int)$payload_meta['row_version'],'request_hash'=>(string)$command['request_hash'],
            'ciphertext_sha256'=>(string)$payload_meta['ciphertext_sha256'],'key_id'=>(string)$payload_meta['key_id'],
            'key_generation'=>(int)$payload_meta['key_generation']),
        'restricted_diff'=>$safe,
    )) !== false;
}

/**
 * Bind one staged command to central approval. The approval draft contains only
 * redacted custody metadata; the encrypted onboarding payload remains solely in
 * hrm_lifecycle_hire_payloads.
 *
 * @return array|false
 */
function submit_staged_hrm_lifecycle_employee_hire_for_approval($command_id)
{
    $command_id = hrm_lifecycle_hire_payload_positive_int($command_id);
    $actor = hrm_lifecycle_hire_payload_actor_id();
    if ($command_id === false || $actor <= 0 || !hrm_lifecycle_hire_payload_permission()
        || !hrm_lifecycle_hire_approval_workflow_is_maker_checker()
        || !function_exists('get_approval_workflow_service'))
        return false;

    $ok = false; $return_value = false;
    begin_transaction();
    do {
        $command = hrm_lifecycle_hire_command_for_update($command_id);
        if (!$command || (int)$command['requested_by'] !== $actor)
            break;
        if ((string)$command['command_status'] === 'pending' && (int)$command['row_version'] === 2
            && (int)$command['approval_draft_id'] > 0) {
            $loaded = load_hrm_lifecycle_employee_hire_payload_for_update($command_id, array('pending'));
            $draft = $loaded ? hrm_lifecycle_hire_approval_draft_for_update($command['approval_draft_id']) : false;
            $decoded = $draft ? json_decode(html_entity_decode((string)$draft['draft_data'], ENT_QUOTES, 'UTF-8'), true) : null;
            if (!$loaded || !$draft || (int)$draft['trans_type'] !== (int)ST_HRM_LIFECYCLE_HIRE
                || (int)$draft['submitted_by'] !== $actor || (int)$draft['status'] !== (int)APPROVAL_STATUS_PENDING
                || !hrm_lifecycle_hire_draft_matches($decoded, $loaded, 2))
                break;
            $ok = true;
            $return_value = array('hire_command_id'=>$command_id,'approval_draft_id'=>(int)$command['approval_draft_id'],
                'status'=>'pending','row_version'=>2,'request_hash'=>(string)$command['request_hash'],'exact_retry'=>true);
            break;
        }
        if ((string)$command['command_status'] !== 'staged' || (int)$command['row_version'] !== 1
            || (int)$command['approval_draft_id'] !== 0)
            break;
        $loaded = load_hrm_lifecycle_employee_hire_payload_for_update($command_id, array('staged'));
        if (!$loaded || strcmp(gmdate('Y-m-d H:i:s'), (string)$loaded['payload_meta']['purge_after']) > 0
            || substr((string)$command['effective_at'],0,10) !== (string)$loaded['payload']['hire_date']
            || !hrm_lifecycle_hire_employee_code_available_for_execution($loaded['payload']['employee_id']))
            break;
        $draft_data = hrm_lifecycle_hire_approval_draft_data($loaded, 2);
        if ($draft_data === false)
            break;
        $approval = get_approval_workflow_service();
        $result = $approval->submit(ST_HRM_LIFECYCLE_HIRE, $draft_data, 0,
            array('summary'=>sprintf(_('Employee Hire lifecycle command #%d'), $command_id)));
        if (!is_array($result) || !isset($result['status'],$result['draft_id'])
            || (string)$result['status'] !== 'pending' || (int)$result['draft_id'] <= 0)
            break;
        $draft_id = (int)$result['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_hire_commands SET command_status='pending',approval_draft_id=".$draft_id
            .",row_version=2 WHERE hire_command_id=".$command_id." AND command_status='staged' AND row_version=1"
            ." AND approval_draft_id IS NULL", 'could not bind Employee Hire command to approval draft')
            || (int)db_num_affected_rows() !== 1)
            break;
        $command['command_status']='pending'; $command['approval_draft_id']=$draft_id; $command['row_version']=2;
        if (!hrm_lifecycle_hire_approval_audit('hrm.lifecycle.employee_hire.submitted', $command, $loaded['payload_meta'], array(
            'approval_draft_id'=>$draft_id,'maker_user_id'=>$actor,'maker_checker_separated'=>true,
            'employee_code_hash'=>(string)$draft_data['employee_code_hash'],'exact_request_hash_bound'=>true,
            'command_payload_locked_before_submit'=>true,'employee_code_uniqueness_revalidated'=>true)))
            break;
        $ok = true;
        $return_value = array('hire_command_id'=>$command_id,'approval_draft_id'=>$draft_id,'status'=>'pending',
            'row_version'=>2,'request_hash'=>(string)$command['request_hash'],'exact_retry'=>false);
    } while (false);
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $return_value : false;
}

/** @return int|false */
function execute_approved_hrm_lifecycle_hire($draft_data, $reserved_trans_no, $draft_id)
{
    $actor = hrm_lifecycle_hire_payload_actor_id();
    if ($actor <= 0 || !hrm_lifecycle_hire_payload_permission() || !is_array($draft_data)
        || !isset($draft_data['hire_command_id']))
        return false;
    $command_id = hrm_lifecycle_hire_payload_positive_int($draft_data['hire_command_id']);
    $draft_id = hrm_lifecycle_hire_payload_positive_int($draft_id);
    if ($command_id === false || $draft_id === false)
        return false;
    $loaded = load_hrm_lifecycle_employee_hire_payload_for_update($command_id, array('pending'));
    if (!$loaded)
        return false;
    $command = $loaded['command']; $meta = $loaded['payload_meta']; $payload = $loaded['payload'];
    if ((int)$command['approval_draft_id'] !== $draft_id || (int)$command['requested_by'] <= 0
        || (int)$command['requested_by'] === $actor)
        return false;
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 2
        || !hrm_lifecycle_hire_draft_matches($draft_data, $loaded, 2)
        || strcmp(gmdate('Y-m-d H:i:s'), (string)$meta['purge_after']) > 0
        || substr((string)$command['effective_at'],0,10) !== (string)$payload['hire_date'])
        return false;
    if (!hrm_lifecycle_hire_employee_code_available_for_execution($payload['employee_id']))
        return false;

    $employee_number = add_employee($payload);
    if ((int)$employee_number <= 0)
        return false;
    $binding = function_exists('get_hrm_person_worker_binding')
        ? get_hrm_person_worker_binding((int)$employee_number, true) : false;
    $employment = function_exists('get_hrm_legacy_current_employment')
        ? get_hrm_legacy_current_employment((int)$employee_number, true) : false;
    $assignment = function_exists('get_hrm_legacy_current_assignment')
        ? get_hrm_legacy_current_assignment((int)$employee_number, true) : false;
    if (!$binding || !$employment || !$assignment || !empty($binding['retired_at'])
        || (int)$binding['worker_id'] <= 0 || (int)$employment['worker_id'] !== (int)$binding['worker_id']
        || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
        || (int)$assignment['legacy_employee_number'] !== (int)$employee_number
        || (int)$employment['legacy_current'] !== 1 || (int)$assignment['legacy_current'] !== 1)
        return false;

    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_hire_commands SET command_status='completed',row_version=3,"
        ."completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE hire_command_id=".$command_id
        ." AND command_status='pending' AND row_version=2 AND approval_draft_id=".$draft_id,
        'could not complete Employee Hire lifecycle command') || (int)db_num_affected_rows() !== 1)
        return false;
    $now = gmdate('Y-m-d H:i:s');
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_hire_payloads SET payload_state='purged',payload_ciphertext=NULL,"
        ."purged_at=".db_escape($now).",row_version=".((int)$meta['row_version']+1)
        ." WHERE hire_command_id=".$command_id." AND payload_state='staged' AND row_version=".(int)$meta['row_version'],
        'could not purge completed Employee Hire payload') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status']='completed'; $command['row_version']=3; $command['completed_by']=$actor;
    $meta['payload_state']='purged'; $meta['payload_ciphertext']=null; $meta['row_version']=(int)$meta['row_version']+1;
    if (!hrm_lifecycle_hire_approval_audit('hrm.lifecycle.employee_hire.completed', $command, $meta, array(
        'approval_draft_id'=>$draft_id,'maker_user_id'=>(int)$command['requested_by'],'checker_user_id'=>$actor,
        'maker_checker_separated'=>true,'employee_number'=>(int)$employee_number,'worker_id'=>(int)$binding['worker_id'],
        'employment_id'=>(int)$employment['employment_id'],'assignment_id'=>(int)$assignment['assignment_id'],
        'employee_rows_mutated'=>1,'person_worker_aggregate_created'=>true,'employment_rows_mutated'=>1,
        'contract_assignment_aggregate_created'=>true,'ciphertext_destroyed'=>true,'historical_rows_rewritten'=>0)))
        return false;
    return $command_id;
}

/** @return int|false */
function hrm_lifecycle_hire_terminalize($draft_data, $draft_id, $status, $maker_only)
{
    if (!in_array($status, array('rejected','cancelled'), true) || !hrm_lifecycle_hire_payload_permission())
        return false;
    $actor = hrm_lifecycle_hire_payload_actor_id();
    $command_id = is_array($draft_data) && isset($draft_data['hire_command_id'])
        ? hrm_lifecycle_hire_payload_positive_int($draft_data['hire_command_id']) : false;
    $draft_id = hrm_lifecycle_hire_payload_positive_int($draft_id);
    if ($actor <= 0 || $command_id === false || $draft_id === false)
        return false;
    $loaded = load_hrm_lifecycle_employee_hire_payload_for_update($command_id, array('pending'));
    if (!$loaded) return false;
    $command=$loaded['command']; $meta=$loaded['payload_meta'];
    if ((int)$command['approval_draft_id'] !== $draft_id)
        return false;
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 2
        || !hrm_lifecycle_hire_draft_matches($draft_data, $loaded, 2)
        || ($maker_only && (int)$command['requested_by'] !== $actor)
        || (!$maker_only && (int)$command['requested_by'] === $actor))
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_hire_commands SET command_status=".db_escape($status)
        .",row_version=3,completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE hire_command_id=".$command_id
        ." AND command_status='pending' AND row_version=2 AND approval_draft_id=".$draft_id,
        'could not terminalize Employee Hire lifecycle command') || (int)db_num_affected_rows() !== 1)
        return false;
    $now=gmdate('Y-m-d H:i:s');
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_hire_payloads SET payload_state='purged',payload_ciphertext=NULL,"
        ."purged_at=".db_escape($now).",row_version=".((int)$meta['row_version']+1)
        ." WHERE hire_command_id=".$command_id." AND payload_state='staged' AND row_version=".(int)$meta['row_version'],
        'could not purge terminal Employee Hire payload') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status']=$status; $command['row_version']=3; $meta['payload_state']='purged'; $meta['row_version']=(int)$meta['row_version']+1;
    $action=$status==='rejected'?'hrm.lifecycle.employee_hire.rejected':'hrm.lifecycle.employee_hire.cancelled';
    return hrm_lifecycle_hire_approval_audit($action,$command,$meta,array(
        'approval_draft_id'=>$draft_id,'actor_user_id'=>$actor,'maker_only'=>$maker_only,
        'maker_checker_separated'=>$maker_only?false:true,'employee_rows_mutated'=>0,
        'person_worker_aggregate_created'=>false,'ciphertext_destroyed'=>true)) ? $command_id : false;
}

function reject_hrm_lifecycle_hire_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_hire_terminalize($draft_data, $draft_id, 'rejected', false);
}

function cancel_hrm_lifecycle_hire_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_hire_terminalize($draft_data, $draft_id, 'cancelled', true);
}

/** @return bool */
function hrm_hire_payload_key_rotation_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_SETUPCOMPANY');
}

/** @return bool */
function hrm_hire_payload_rotation_lock($company)
{
    $name='notrinoserp:hrm-hire-key:'.(int)$company;
    $result=db_query("SELECT GET_LOCK(".db_escape($name).",10) acquired", 'could not acquire Employee Hire key rotation lock');
    $row=$result?db_fetch_assoc($result):false;
    return $row && (int)$row['acquired']===1;
}

/** @return void */
function hrm_hire_payload_rotation_unlock($company)
{
    $name='notrinoserp:hrm-hire-key:'.(int)$company;
    @db_query("SELECT RELEASE_LOCK(".db_escape($name).")", 'could not release Employee Hire key rotation lock');
}

/** @return array|false */
function hrm_hire_payload_rotation_raw_manifest($company)
{
    $paths=hrm_hire_payload_keyring_paths($company);
    if ($paths===false || !is_file($paths['manifest']) || is_link($paths['manifest'])) return false;
    $manifest=json_decode((string)@file_get_contents($paths['manifest']),true);
    if (!is_array($manifest) || !isset($manifest['schema_version'],$manifest['company_id'],$manifest['active_key_id'],$manifest['generation'],$manifest['keys'])
        || $manifest['schema_version']!=='notrinoserp.hrm-hire-payload-keyring.v1' || (int)$manifest['company_id']!==(int)$company
        || (int)$manifest['generation']<1 || !is_array($manifest['keys']) || count($manifest['keys'])!==(int)$manifest['generation']) return false;
    $last=false; $generation=0;
    foreach($manifest['keys'] as $entry){
        if(!is_array($entry)||!isset($entry['key_id'],$entry['generation'],$entry['filename'],$entry['activated_at'],$entry['retired_at'])
            ||(int)$entry['generation']!==++$generation||preg_match('/^[a-f0-9]{16}$/D',(string)$entry['key_id'])!==1
            ||(string)$entry['filename']!=='hire-payload-v1-'.$entry['key_id'].'.key'
            ||hrm_hire_payload_load_key_file($paths['keys'].DIRECTORY_SEPARATOR.$entry['filename'],$entry['key_id'])===false) return false;
        $last=$entry;
    }
    return $last && (string)$last['key_id']===(string)$manifest['active_key_id'] && (int)$last['generation']===(int)$manifest['generation'] ? $manifest : false;
}

/** @return array|false */
function hrm_hire_payload_rotation_pending($company)
{
    $paths=hrm_hire_payload_keyring_paths($company);
    if($paths===false||!is_file($paths['pending'])||is_link($paths['pending']))return false;
    $pending=json_decode((string)@file_get_contents($paths['pending']),true);
    $keys=is_array($pending)?array_keys($pending):array(); $expected=array('schema_version','company_id','previous_key_id','new_key_id','generation','filename','occurred_at','correlation_id','reason_code','actor_user_id');
    sort($keys); sort($expected);
    if(!is_array($pending)||$keys!==$expected||$pending['schema_version']!=='notrinoserp.hrm-hire-payload-key-rotation-pending.v1'
        ||(int)$pending['company_id']!==(int)$company||preg_match('/^[a-f0-9]{16}$/D',(string)$pending['previous_key_id'])!==1
        ||preg_match('/^[a-f0-9]{16}$/D',(string)$pending['new_key_id'])!==1||(int)$pending['generation']<2
        ||$pending['filename']!=='hire-payload-v1-'.$pending['new_key_id'].'.key'||preg_match('/^[a-f0-9]{32}$/D',(string)$pending['correlation_id'])!==1
        ||!function_exists('domain_audit_valid_token')||!domain_audit_valid_token((string)$pending['reason_code'])||(int)$pending['actor_user_id']<=0)return false;
    return $pending;
}

/** @return array|false */
function hrm_hire_payload_manifest_after_rotation($manifest,$pending)
{
    if(!is_array($manifest)||!is_array($pending)||(int)$pending['generation']!==(int)$manifest['generation']+1
        ||(string)$pending['previous_key_id']!==(string)$manifest['active_key_id'])return false;
    $next=$manifest; $count=count($next['keys']); if($count<1)return false;
    $next['keys'][$count-1]['retired_at']=(string)$pending['occurred_at'];
    $next['keys'][]=array('key_id'=>(string)$pending['new_key_id'],'generation'=>(int)$pending['generation'],
        'filename'=>(string)$pending['filename'],'activated_at'=>(string)$pending['occurred_at'],'retired_at'=>'');
    $next['active_key_id']=(string)$pending['new_key_id']; $next['generation']=(int)$pending['generation'];
    return $next;
}

/** @return int|false|null */
function hrm_hire_payload_rotation_audit_count($pending)
{
    if(!is_array($pending))return false;
    $result=db_query("SELECT COUNT(*) c FROM ".TB_PREF."domain_audit_events WHERE entity_type='hrm_lifecycle_hire_payload_key'"
        ." AND action='hrm.lifecycle.employee_hire.payload_key_rotated' AND correlation_id=".db_escape($pending['correlation_id']),
        'could not inspect Employee Hire payload key rotation audit');
    $row=$result?db_fetch_assoc($result):false;
    return $row?(int)$row['c']:false;
}

/** @return bool */
function hrm_hire_payload_append_rotation_audit($pending)
{
    $count=hrm_hire_payload_rotation_audit_count($pending);
    if($count===1)return true; if($count!==0)return false;
    return append_domain_audit_event(array('company_id'=>(int)$pending['company_id'],'legal_entity_id'=>0,
        'entity_type'=>'hrm_lifecycle_hire_payload_key','entity_key'=>array('key_id'=>(string)$pending['new_key_id']),
        'action'=>'hrm.lifecycle.employee_hire.payload_key_rotated','reason_code'=>(string)$pending['reason_code'],
        'correlation_id'=>(string)$pending['correlation_id'],'occurred_at'=>(string)$pending['occurred_at'],
        'restricted_diff'=>array('previous_key_id'=>(string)$pending['previous_key_id'],'new_key_id'=>(string)$pending['new_key_id'],
            'generation'=>(int)$pending['generation'],'old_key_retained_for_staged_payload_recovery'=>true,
            'payload_rows_reencrypted'=>0,'database_key_material_rows_added'=>0,'browser_route_added'=>false)))!==false;
}

/**
 * Rotate the active staging key without re-encrypting existing payloads. Old
 * generations remain decryptable until no retained envelope references them.
 * A private pending journal makes every crash state explicitly recoverable.
 *
 * @return array|false
 */
function rotate_hrm_hire_payload_key($company, $reason_code)
{
    $company=(int)$company; $actor=hrm_lifecycle_hire_payload_actor_id();
    if($company<0||$actor<=0||!hrm_hire_payload_key_rotation_permission()||!domain_audit_valid_token((string)$reason_code)
        ||!hrm_hire_payload_rotation_lock($company))return false;
    $result=false;
    do{
        $paths=hrm_hire_payload_keyring_paths($company);
        if($paths===false||is_file($paths['pending']))break;
        $manifest=hrm_hire_payload_keyring($company,false); if($manifest===false)break;
        if(!function_exists('openssl_random_pseudo_bytes'))break;
        $strong=false; $key=openssl_random_pseudo_bytes(32,$strong);
        if(!is_string($key)||strlen($key)!==32||!$strong)break;
        $key_id=substr(hash('sha256',$key),0,16); if($key_id===$manifest['active_key_id'])break;
        $filename='hire-payload-v1-'.$key_id.'.key'; $key_path=$paths['keys'].DIRECTORY_SEPARATOR.$filename;
        if(!domain_audit_write_key_file($key_path,$key))break;
        $correlation=domain_audit_correlation_id();
        $pending=array('schema_version'=>'notrinoserp.hrm-hire-payload-key-rotation-pending.v1','company_id'=>$company,
            'previous_key_id'=>(string)$manifest['active_key_id'],'new_key_id'=>$key_id,'generation'=>(int)$manifest['generation']+1,
            'filename'=>$filename,'occurred_at'=>gmdate('Y-m-d H:i:s'),'correlation_id'=>$correlation,
            'reason_code'=>(string)$reason_code,'actor_user_id'=>$actor);
        if($correlation===false||!domain_audit_write_private_json($paths['pending'],$pending,false)){@unlink($key_path);break;}
        $next=hrm_hire_payload_manifest_after_rotation($manifest,$pending);
        if($next===false||!domain_audit_write_private_json($paths['manifest'],$next,true))break;
        if(!hrm_hire_payload_append_rotation_audit($pending)||!@unlink($paths['pending']))break;
        $result=array('company_id'=>$company,'generation'=>(int)$next['generation'],'previous_key_id'=>(string)$pending['previous_key_id'],
            'active_key_id'=>(string)$pending['new_key_id'],'correlation_id'=>(string)$pending['correlation_id']);
    }while(false);
    hrm_hire_payload_rotation_unlock($company);
    return $result;
}

/** @return array|false */
function recover_hrm_hire_payload_key_rotation($company)
{
    $company=(int)$company;
    if($company<0||hrm_lifecycle_hire_payload_actor_id()<=0||!hrm_hire_payload_key_rotation_permission()
        ||!hrm_hire_payload_rotation_lock($company))return false;
    $result=false;
    do{
        $paths=hrm_hire_payload_keyring_paths($company); $pending=hrm_hire_payload_rotation_pending($company);
        $manifest=hrm_hire_payload_rotation_raw_manifest($company);
        if($paths===false||$pending===false||$manifest===false)break;
        $new_key_path=$paths['keys'].DIRECTORY_SEPARATOR.$pending['filename'];
        if((int)$manifest['generation']===(int)$pending['generation']-1
            &&(string)$manifest['active_key_id']===(string)$pending['previous_key_id']){
            if(hrm_hire_payload_rotation_audit_count($pending)!==0)break;
            if(file_exists($new_key_path)&&!@unlink($new_key_path))break;
            if(!@unlink($paths['pending']))break;
            $result=array('ok'=>true,'recovery'=>'aborted_unactivated','generation'=>(int)$manifest['generation'],
                'active_key_id'=>(string)$manifest['active_key_id']);
            break;
        }
        if((int)$manifest['generation']!==(int)$pending['generation']
            ||(string)$manifest['active_key_id']!==(string)$pending['new_key_id']
            ||hrm_hire_payload_load_key_file($new_key_path,$pending['new_key_id'])===false)break;
        if(!hrm_hire_payload_append_rotation_audit($pending)||!@unlink($paths['pending']))break;
        $result=array('ok'=>true,'recovery'=>'finalized_activated','generation'=>(int)$manifest['generation'],
            'active_key_id'=>(string)$manifest['active_key_id']);
    }while(false);
    hrm_hire_payload_rotation_unlock($company);
    return $result;
}
