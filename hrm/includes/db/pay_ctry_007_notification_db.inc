<?php
/**
 * PAY-CTRY-007 governed scoped audience, recipient, acknowledgement and lifecycle writer.
 *
 * This writer is intentionally internal-only; external delivery remains denied. It does not send external notifications,
 * activate country packs, execute payroll, post accounting, submit filings or expose
 * employee facts in regulatory notice payloads.
 */
$pay_ctry_007_runtime_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
if (!function_exists('pay_ctry_007_validate_audience')) {
    include_once($pay_ctry_007_runtime_root.'/includes/pay_ctry_007_contract.inc');
}
if (!function_exists('pay_ctry_007_schema_groups')) {
    include_once($pay_ctry_007_runtime_root.'/hrm/includes/db/pay_ctry_007_schema_db.inc');
}
if (!function_exists('append_domain_audit_event')) {
    include_once($pay_ctry_007_runtime_root.'/includes/db/domain_audit_db.inc');
}
unset($pay_ctry_007_runtime_root);

function pay_ctry_007_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}

function pay_ctry_007_company_id()
{
    return function_exists('user_company') ? (int)user_company() : -1;
}

function pay_ctry_007_has_access($permission)
{
    return function_exists('user_check_access') && user_check_access($permission);
}

function pay_ctry_007_insert_id()
{
    return function_exists('db_insert_id') ? (int)db_insert_id() : 0;
}

function pay_ctry_007_one($sql)
{
    $result = db_query($sql);
    if (!$result || db_num_rows($result) !== 1) {
        return false;
    }
    return db_fetch_assoc($result);
}

function pay_ctry_007_iso_now($epoch)
{
    return gmdate('Y-m-d\TH:i:s\Z', (int)$epoch);
}

function pay_ctry_007_sql_now($epoch)
{
    return gmdate('Y-m-d H:i:s', (int)$epoch);
}

function pay_ctry_007_recipient_tables_ready()
{
    if (!function_exists('hrm_migration_table_exists')) {
        return false;
    }
    foreach (array(
        'pay_ctry_update_notices',
        'pay_ctry_update_notice_applicability',
        'pay_ctry_update_notice_audiences',
        'pay_ctry_update_notice_recipient_events',
        'pay_ctry_update_notice_acknowledgements',
        'pay_ctry_update_notice_lifecycle_events'
    ) as $table) {
        if (!hrm_migration_table_exists($table)) {
            return false;
        }
    }
    return true;
}

function pay_ctry_007_scope_for_update($notice_applicability_id)
{
    $company_id = pay_ctry_007_company_id();
    if ($company_id < 0) {
        return false;
    }
    return pay_ctry_007_one(
        'SELECT a.*,n.notice_sha256,n.notice_key,n.severity,n.deadline_at'
        .' FROM '.TB_PREF.'pay_ctry_update_notice_applicability a'
        .' INNER JOIN '.TB_PREF.'pay_ctry_update_notices n ON n.update_notice_id=a.update_notice_id'
        .' WHERE a.notice_applicability_id='.(int)$notice_applicability_id
        .' AND a.company_id='.$company_id.' LIMIT 2 FOR UPDATE'
    );
}

function pay_ctry_007_audience_for_update($notice_audience_id)
{
    $company_id = pay_ctry_007_company_id();
    if ($company_id < 0) {
        return false;
    }
    return pay_ctry_007_one(
        'SELECT u.*,a.company_id,a.applicability_sha256,n.notice_sha256'
        .' FROM '.TB_PREF.'pay_ctry_update_notice_audiences u'
        .' INNER JOIN '.TB_PREF.'pay_ctry_update_notice_applicability a ON a.notice_applicability_id=u.notice_applicability_id'
        .' INNER JOIN '.TB_PREF.'pay_ctry_update_notices n ON n.update_notice_id=u.update_notice_id'
        .' WHERE u.notice_audience_id='.(int)$notice_audience_id
        .' AND a.company_id='.$company_id.' LIMIT 2 FOR UPDATE'
    );
}

function pay_ctry_007_recipient_user_active($user_id)
{
    $row = pay_ctry_007_one('SELECT id,inactive FROM '.TB_PREF.'users WHERE id='.(int)$user_id.' LIMIT 2');
    return is_array($row) && (int)$row['inactive'] === 0;
}

function pay_ctry_007_audit(array $event, &$error)
{
    if (!function_exists('append_domain_audit_event') || !append_domain_audit_event($event)) {
        $error = 'recipient_lifecycle_audit_failed';
        return false;
    }
    return true;
}

/**
 * Create one immutable scoped audience and its initial recipient assignments.
 * Audience identity excludes execution timestamps, so a retry after commit is a replay.
 */
function pay_ctry_007_create_notice_audience($notice_applicability_id, $audience_type, array $recipient_user_ids, $now_epoch = null, &$error = null)
{
    $error = null;
    $actor_id = pay_ctry_007_actor_id();
    $now_epoch = $now_epoch === null ? time() : (int)$now_epoch;
    if ($actor_id <= 0 || !pay_ctry_007_has_access(PAY_CTRY_007_MANAGE_PERMISSION)
        || !in_array($audience_type, pay_ctry_007_audience_types(), true)) {
        $error = 'audience_unauthorized_or_invalid';
        return false;
    }
    if (!pay_ctry_007_recipient_tables_ready()) {
        $error = 'recipient_schema_not_ready';
        return false;
    }
    if (!empty($GLOBALS['transaction_level'])) {
        $error = 'recipient_lifecycle_requires_owned_transaction';
        return false;
    }

    $recipient_ids = array();
    foreach ($recipient_user_ids as $user_id) {
        $user_id = (int)$user_id;
        if ($user_id <= 0) {
            $error = 'recipient_user_invalid';
            return false;
        }
        $recipient_ids[$user_id] = $user_id;
    }
    if (!$recipient_ids || count($recipient_ids) > 200) {
        $error = 'recipient_set_invalid';
        return false;
    }
    ksort($recipient_ids);
    $recipient_ids = array_values($recipient_ids);

    begin_transaction();
    $ok = false;
    $result = false;
    do {
        $scope = pay_ctry_007_scope_for_update($notice_applicability_id);
        if (!is_array($scope)) {
            $error = 'notice_applicability_scope_denied';
            break;
        }
        foreach ($recipient_ids as $user_id) {
            if (!pay_ctry_007_recipient_user_active($user_id)) {
                $error = 'recipient_user_unavailable';
                break 2;
            }
        }

        $recipient_set_sha256 = pay_ctry_007_hash($recipient_ids);
        $scope_sha256 = pay_ctry_007_hash(array(
            'company_id'=>(int)$scope['company_id'],
            'scope_type'=>(string)$scope['scope_type'],
            'legal_entity_id'=>$scope['legal_entity_id'] === null ? null : (int)$scope['legal_entity_id'],
            'payroll_scope_key'=>$scope['payroll_scope_key'] === null ? null : (string)$scope['payroll_scope_key'],
            'applicability_sha256'=>(string)$scope['applicability_sha256']
        ));
        $audience_key = 'ctry7.audience.'.substr(pay_ctry_007_hash(array(
            'notice'=>(string)$scope['notice_sha256'],
            'app'=>(string)$scope['applicability_sha256'],
            'type'=>$audience_type,
            'scope'=>$scope_sha256,
            'recipients'=>$recipient_set_sha256
        )), 0, 48);

        /* Idempotency is resolved before timestamp-dependent custody is rebuilt. */
        $existing = pay_ctry_007_one(
            'SELECT notice_audience_id,audience_sha256 FROM '.TB_PREF.'pay_ctry_update_notice_audiences'
            .' WHERE audience_key='.db_escape($audience_key).' LIMIT 2 FOR UPDATE'
        );
        if (is_array($existing)) {
            $ok = true;
            $result = array(
                'notice_audience_id'=>(int)$existing['notice_audience_id'],
                'audience_sha256'=>(string)$existing['audience_sha256'],
                'recipient_count'=>count($recipient_ids),
                'replayed'=>true
            );
            break;
        }

        $selected_at = pay_ctry_007_iso_now($now_epoch);
        $sql_at = pay_ctry_007_sql_now($now_epoch);
        $audience = array(
            'audience_key'=>$audience_key,
            'update_notice_sha256'=>(string)$scope['notice_sha256'],
            'applicability_sha256'=>(string)$scope['applicability_sha256'],
            'audience_type'=>$audience_type,
            'audience_scope_sha256'=>$scope_sha256,
            'recipient_set_sha256'=>$recipient_set_sha256,
            'selected_by'=>$actor_id,
            'selected_at'=>$selected_at,
            'audience_sha256'=>''
        );
        $audience['audience_sha256'] = pay_ctry_007_audience_sha256($audience);
        if (!pay_ctry_007_validate_audience($audience, $error)) {
            break;
        }

        $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_update_notice_audiences'
            .' (update_notice_id,notice_applicability_id,audience_key,audience_type,audience_scope_sha256,recipient_set_sha256,audience_sha256,selected_by,selected_at,created_at) VALUES ('
            .(int)$scope['update_notice_id'].','.(int)$scope['notice_applicability_id'].','
            .db_escape($audience_key).','.db_escape($audience_type).','.db_escape($scope_sha256).','
            .db_escape($recipient_set_sha256).','.db_escape($audience['audience_sha256']).','.$actor_id.','
            .db_escape($sql_at).','.db_escape($sql_at).')';
        if (!db_query($sql, 'could not append PAY-CTRY-007 audience')) {
            $error = 'audience_insert_failed';
            break;
        }
        $audience_id = pay_ctry_007_insert_id();
        if ($audience_id <= 0) {
            $error = 'audience_identity_missing';
            break;
        }

        foreach ($recipient_ids as $user_id) {
            $event_key = 'ctry7.recipient.'.substr(pay_ctry_007_hash(array(
                'audience'=>$audience['audience_sha256'], 'user'=>$user_id, 'command'=>'initial_assignment'
            )), 0, 48);
            $event = array(
                'recipient_event_key'=>$event_key,
                'audience_sha256'=>$audience['audience_sha256'],
                'update_notice_sha256'=>(string)$scope['notice_sha256'],
                'applicability_sha256'=>(string)$scope['applicability_sha256'],
                'recipient_user_id'=>$user_id,
                'event_no'=>1,
                'predecessor_recipient_event_sha256'=>null,
                'event_type'=>'assigned',
                'reason_code'=>'initial_assignment',
                'occurred_at'=>$selected_at,
                'actor_id'=>$actor_id,
                'recipient_event_sha256'=>''
            );
            $event['recipient_event_sha256'] = pay_ctry_007_recipient_event_sha256($event);
            if (!pay_ctry_007_validate_recipient_event($event, $error)) {
                break 2;
            }
            $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_update_notice_recipient_events'
                .' (notice_audience_id,update_notice_id,notice_applicability_id,recipient_user_id,recipient_event_key,event_no,predecessor_recipient_event_id,predecessor_recipient_event_sha256,event_type,reason_code,recipient_event_sha256,acted_by,occurred_at,created_at) VALUES ('
                .$audience_id.','.(int)$scope['update_notice_id'].','.(int)$scope['notice_applicability_id'].','.$user_id.','
                .db_escape($event_key).',1,NULL,NULL,\'assigned\',\'initial_assignment\','
                .db_escape($event['recipient_event_sha256']).','.$actor_id.','.db_escape($sql_at).','.db_escape($sql_at).')';
            if (!db_query($sql, 'could not append PAY-CTRY-007 recipient assignment')) {
                $error = 'recipient_assignment_insert_failed';
                break 2;
            }
        }

        if (!pay_ctry_007_audit(array(
            'company_id'=>(int)$scope['company_id'],
            'legal_entity_id'=>$scope['legal_entity_id'] === null ? 0 : (int)$scope['legal_entity_id'],
            'entity_type'=>'pay_ctry_update_notice_audience',
            'entity_key'=>array('notice_audience_id'=>$audience_id),
            'action'=>'pay.ctry.update.audience_assigned',
            'reason_code'=>'governed_assignment',
            'before'=>array(),
            'after'=>array('audience_sha256'=>$audience['audience_sha256']),
            'restricted_diff'=>array(
                'recipient_count'=>count($recipient_ids),
                'recipient_set_sha256'=>$recipient_set_sha256,
                'employee_fact_payload'=>false,
                'external_delivery'=>false
            )
        ), $error)) {
            break;
        }

        $ok = true;
        $result = array(
            'notice_audience_id'=>$audience_id,
            'audience_sha256'=>$audience['audience_sha256'],
            'recipient_count'=>count($recipient_ids),
            'replayed'=>false
        );
    } while (false);

    if ($ok) {
        commit_transaction();
    } else {
        cancel_transaction();
    }
    return $ok ? $result : false;
}

/**
 * Append an explicit assignment/withdrawal transition.
 * $command_key is caller-supplied stable idempotency identity for the logical command.
 */
function pay_ctry_007_append_recipient_event($notice_audience_id, $recipient_user_id, $event_type, $reason_code, $command_key, $now_epoch = null, &$error = null)
{
    $error = null;
    $actor_id = pay_ctry_007_actor_id();
    $recipient_user_id = (int)$recipient_user_id;
    $now_epoch = $now_epoch === null ? time() : (int)$now_epoch;
    if ($actor_id <= 0 || !pay_ctry_007_has_access(PAY_CTRY_007_MANAGE_PERMISSION)
        || $recipient_user_id <= 0 || !in_array($event_type, pay_ctry_007_recipient_event_types(), true)
        || !pay_ctry_007_reason($reason_code) || !pay_ctry_007_token($command_key, 64)) {
        $error = 'recipient_event_unauthorized_or_invalid';
        return false;
    }
    if (!pay_ctry_007_recipient_tables_ready()) {
        $error = 'recipient_schema_not_ready';
        return false;
    }
    if (!empty($GLOBALS['transaction_level'])) {
        $error = 'recipient_lifecycle_requires_owned_transaction';
        return false;
    }

    begin_transaction();
    $ok = false;
    $result = false;
    do {
        $audience = pay_ctry_007_audience_for_update($notice_audience_id);
        if (!is_array($audience) || !pay_ctry_007_recipient_user_active($recipient_user_id)) {
            $error = 'recipient_event_scope_denied';
            break;
        }

        $event_key = 'ctry7.recipient.'.substr(pay_ctry_007_hash(array(
            'audience'=>(string)$audience['audience_sha256'],
            'user'=>$recipient_user_id,
            'command'=>$command_key
        )), 0, 48);
        $existing = pay_ctry_007_one(
            'SELECT recipient_event_id,notice_audience_id,recipient_user_id,event_type,reason_code,recipient_event_sha256'
            .' FROM '.TB_PREF.'pay_ctry_update_notice_recipient_events'
            .' WHERE recipient_event_key='.db_escape($event_key).' LIMIT 2 FOR UPDATE'
        );
        if (is_array($existing)) {
            if ((int)$existing['notice_audience_id'] !== (int)$notice_audience_id
                || (int)$existing['recipient_user_id'] !== $recipient_user_id
                || (string)$existing['event_type'] !== $event_type
                || (string)$existing['reason_code'] !== $reason_code) {
                $error = 'recipient_event_replay_mismatch';
                break;
            }
            $ok = true;
            $result = array(
                'recipient_event_id'=>(int)$existing['recipient_event_id'],
                'recipient_event_sha256'=>(string)$existing['recipient_event_sha256'],
                'replayed'=>true
            );
            break;
        }

        $latest = pay_ctry_007_one(
            'SELECT recipient_event_id,event_no,event_type,recipient_event_sha256'
            .' FROM '.TB_PREF.'pay_ctry_update_notice_recipient_events'
            .' WHERE notice_audience_id='.(int)$notice_audience_id.' AND recipient_user_id='.$recipient_user_id
            .' ORDER BY event_no DESC LIMIT 1 FOR UPDATE'
        );
        if (!is_array($latest)) {
            $error = 'recipient_event_predecessor_missing';
            break;
        }
        if (($event_type === 'withdrawn' && (string)$latest['event_type'] !== 'assigned')
            || ($event_type === 'assigned' && (string)$latest['event_type'] !== 'withdrawn')) {
            $error = 'recipient_event_transition_invalid';
            break;
        }

        $event_no = (int)$latest['event_no'] + 1;
        $occurred_at = pay_ctry_007_iso_now($now_epoch);
        $sql_at = pay_ctry_007_sql_now($now_epoch);
        $event = array(
            'recipient_event_key'=>$event_key,
            'audience_sha256'=>(string)$audience['audience_sha256'],
            'update_notice_sha256'=>(string)$audience['notice_sha256'],
            'applicability_sha256'=>(string)$audience['applicability_sha256'],
            'recipient_user_id'=>$recipient_user_id,
            'event_no'=>$event_no,
            'predecessor_recipient_event_sha256'=>(string)$latest['recipient_event_sha256'],
            'event_type'=>$event_type,
            'reason_code'=>$reason_code,
            'occurred_at'=>$occurred_at,
            'actor_id'=>$actor_id,
            'recipient_event_sha256'=>''
        );
        $event['recipient_event_sha256'] = pay_ctry_007_recipient_event_sha256($event);
        if (!pay_ctry_007_validate_recipient_event($event, $error)) {
            break;
        }

        $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_update_notice_recipient_events'
            .' (notice_audience_id,update_notice_id,notice_applicability_id,recipient_user_id,recipient_event_key,event_no,predecessor_recipient_event_id,predecessor_recipient_event_sha256,event_type,reason_code,recipient_event_sha256,acted_by,occurred_at,created_at) VALUES ('
            .(int)$notice_audience_id.','.(int)$audience['update_notice_id'].','.(int)$audience['notice_applicability_id'].','.$recipient_user_id.','
            .db_escape($event_key).','.$event_no.','.(int)$latest['recipient_event_id'].','.db_escape((string)$latest['recipient_event_sha256']).','
            .db_escape($event_type).','.db_escape($reason_code).','.db_escape($event['recipient_event_sha256']).','.$actor_id.','
            .db_escape($sql_at).','.db_escape($sql_at).')';
        if (!db_query($sql, 'could not append PAY-CTRY-007 recipient event')) {
            $error = 'recipient_event_insert_failed';
            break;
        }
        $event_id = pay_ctry_007_insert_id();

        if (!pay_ctry_007_audit(array(
            'company_id'=>(int)$audience['company_id'],
            'entity_type'=>'pay_ctry_update_notice_recipient',
            'entity_key'=>array('recipient_event_id'=>$event_id),
            'action'=>'pay.ctry.update.recipient_'.$event_type,
            'reason_code'=>$reason_code,
            'before'=>array('recipient_event_sha256'=>(string)$latest['recipient_event_sha256']),
            'after'=>array('recipient_event_sha256'=>$event['recipient_event_sha256']),
            'restricted_diff'=>array('recipient_user_id'=>$recipient_user_id, 'external_delivery'=>false)
        ), $error)) {
            break;
        }

        $ok = true;
        $result = array(
            'recipient_event_id'=>$event_id,
            'recipient_event_sha256'=>$event['recipient_event_sha256'],
            'replayed'=>false
        );
    } while (false);

    if ($ok) {
        commit_transaction();
    } else {
        cancel_transaction();
    }
    return $ok ? $result : false;
}

/** Self-acknowledge the currently assigned recipient event. */
function pay_ctry_007_acknowledge_notice($recipient_event_id, $now_epoch = null, &$error = null)
{
    $error = null;
    $actor_id = pay_ctry_007_actor_id();
    $recipient_event_id = (int)$recipient_event_id;
    $now_epoch = $now_epoch === null ? time() : (int)$now_epoch;
    if ($actor_id <= 0 || $recipient_event_id <= 0 || !pay_ctry_007_has_access(PAY_CTRY_007_ACK_PERMISSION)) {
        $error = 'acknowledgement_unauthorized_or_invalid';
        return false;
    }
    if (!pay_ctry_007_recipient_tables_ready()) {
        $error = 'recipient_schema_not_ready';
        return false;
    }
    if (!empty($GLOBALS['transaction_level'])) {
        $error = 'recipient_lifecycle_requires_owned_transaction';
        return false;
    }

    begin_transaction();
    $ok = false;
    $result = false;
    do {
        $company_id = pay_ctry_007_company_id();
        $recipient = pay_ctry_007_one(
            'SELECT r.*,a.company_id,a.applicability_sha256,n.notice_sha256'
            .' FROM '.TB_PREF.'pay_ctry_update_notice_recipient_events r'
            .' INNER JOIN '.TB_PREF.'pay_ctry_update_notice_applicability a ON a.notice_applicability_id=r.notice_applicability_id'
            .' INNER JOIN '.TB_PREF.'pay_ctry_update_notices n ON n.update_notice_id=r.update_notice_id'
            .' WHERE r.recipient_event_id='.$recipient_event_id.' AND a.company_id='.(int)$company_id.' LIMIT 2 FOR UPDATE'
        );
        if (!is_array($recipient) || (int)$recipient['recipient_user_id'] !== $actor_id
            || (string)$recipient['event_type'] !== 'assigned') {
            $error = 'acknowledgement_self_scope_denied';
            break;
        }

        /* A committed self-ack remains an idempotent replay even if assignment later changes. */
        $existing = pay_ctry_007_one(
            'SELECT notice_acknowledgement_id,acknowledgement_sha256'
            .' FROM '.TB_PREF.'pay_ctry_update_notice_acknowledgements'
            .' WHERE recipient_event_id='.$recipient_event_id.' LIMIT 2 FOR UPDATE'
        );
        if (is_array($existing)) {
            $ok = true;
            $result = array(
                'notice_acknowledgement_id'=>(int)$existing['notice_acknowledgement_id'],
                'acknowledgement_sha256'=>(string)$existing['acknowledgement_sha256'],
                'replayed'=>true
            );
            break;
        }

        $latest = pay_ctry_007_one(
            'SELECT recipient_event_id,event_type FROM '.TB_PREF.'pay_ctry_update_notice_recipient_events'
            .' WHERE notice_audience_id='.(int)$recipient['notice_audience_id'].' AND recipient_user_id='.$actor_id
            .' ORDER BY event_no DESC LIMIT 1 FOR UPDATE'
        );
        if (!is_array($latest) || (int)$latest['recipient_event_id'] !== $recipient_event_id
            || (string)$latest['event_type'] !== 'assigned') {
            $error = 'acknowledgement_assignment_not_current';
            break;
        }

        $acknowledged_at = pay_ctry_007_iso_now($now_epoch);
        $sql_at = pay_ctry_007_sql_now($now_epoch);
        $ack = array(
            'recipient_event_sha256'=>(string)$recipient['recipient_event_sha256'],
            'update_notice_sha256'=>(string)$recipient['notice_sha256'],
            'applicability_sha256'=>(string)$recipient['applicability_sha256'],
            'recipient_user_id'=>$actor_id,
            'acknowledgement_type'=>'acknowledged',
            'acknowledged_at'=>$acknowledged_at,
            'actor_id'=>$actor_id,
            'acknowledgement_sha256'=>''
        );
        $ack['acknowledgement_sha256'] = pay_ctry_007_ack_sha256($ack);
        if (!pay_ctry_007_validate_acknowledgement($ack, $error)) {
            break;
        }

        $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_update_notice_acknowledgements'
            .' (recipient_event_id,update_notice_id,notice_applicability_id,recipient_user_id,acknowledgement_type,acknowledged_at,acknowledgement_sha256,actor_id,created_at) VALUES ('
            .$recipient_event_id.','.(int)$recipient['update_notice_id'].','.(int)$recipient['notice_applicability_id'].','.$actor_id.','
            .'\'acknowledged\','.db_escape($sql_at).','.db_escape($ack['acknowledgement_sha256']).','.$actor_id.','.db_escape($sql_at).')';
        if (!db_query($sql, 'could not append PAY-CTRY-007 acknowledgement')) {
            $error = 'acknowledgement_insert_failed';
            break;
        }
        $ack_id = pay_ctry_007_insert_id();

        if (!pay_ctry_007_audit(array(
            'company_id'=>(int)$recipient['company_id'],
            'entity_type'=>'pay_ctry_update_notice_acknowledgement',
            'entity_key'=>array('notice_acknowledgement_id'=>$ack_id),
            'action'=>'pay.ctry.update.acknowledged',
            'reason_code'=>'self_acknowledgement',
            'before'=>array(),
            'after'=>array('acknowledgement_sha256'=>$ack['acknowledgement_sha256']),
            'restricted_diff'=>array('self_acknowledgement'=>true, 'employee_fact_payload'=>false, 'external_delivery'=>false)
        ), $error)) {
            break;
        }

        $ok = true;
        $result = array(
            'notice_acknowledgement_id'=>$ack_id,
            'acknowledgement_sha256'=>$ack['acknowledgement_sha256'],
            'replayed'=>false
        );
    } while (false);

    if ($ok) {
        commit_transaction();
    } else {
        cancel_transaction();
    }
    return $ok ? $result : false;
}

/** Append a reminder, escalation or scoped resolution event without sending anything externally. */
function pay_ctry_007_append_notice_lifecycle_event($notice_applicability_id, $event_type, $recipient_event_id, $reason_code, $evidence_sha256, $due_at = null, $now_epoch = null, &$error = null)
{
    $error = null;
    $actor_id = pay_ctry_007_actor_id();
    $now_epoch = $now_epoch === null ? time() : (int)$now_epoch;
    $recipient_event_id = $recipient_event_id === null ? null : (int)$recipient_event_id;
    $required_permission = $event_type === 'resolved' ? PAY_CTRY_007_RESOLVE_PERMISSION : PAY_CTRY_007_ESCALATE_PERMISSION;
    if ($actor_id <= 0 || !pay_ctry_007_has_access($required_permission)
        || !in_array($event_type, pay_ctry_007_lifecycle_event_types(), true)
        || !pay_ctry_007_reason($reason_code) || !pay_ctry_007_sha($evidence_sha256)) {
        $error = 'notice_lifecycle_unauthorized_or_invalid';
        return false;
    }
    if (!pay_ctry_007_recipient_tables_ready()) {
        $error = 'recipient_schema_not_ready';
        return false;
    }
    if (!empty($GLOBALS['transaction_level'])) {
        $error = 'recipient_lifecycle_requires_owned_transaction';
        return false;
    }

    begin_transaction();
    $ok = false;
    $result = false;
    do {
        $scope = pay_ctry_007_scope_for_update($notice_applicability_id);
        if (!is_array($scope)) {
            $error = 'notice_lifecycle_scope_denied';
            break;
        }

        $recipient = false;
        $recipient_sha256 = null;
        if ($event_type === 'resolved') {
            if ($recipient_event_id !== null || $due_at !== null) {
                $error = 'resolution_scope_invalid';
                break;
            }
        } else {
            if ($recipient_event_id === null || !is_string($due_at) || pay_ctry_007_time($due_at) === false) {
                $error = 'reminder_escalation_scope_invalid';
                break;
            }
            $recipient = pay_ctry_007_one(
                'SELECT * FROM '.TB_PREF.'pay_ctry_update_notice_recipient_events'
                .' WHERE recipient_event_id='.$recipient_event_id
                .' AND update_notice_id='.(int)$scope['update_notice_id']
                .' AND notice_applicability_id='.(int)$scope['notice_applicability_id'].' LIMIT 2 FOR UPDATE'
            );
            if (!is_array($recipient) || (string)$recipient['event_type'] !== 'assigned') {
                $error = 'recipient_event_scope_invalid';
                break;
            }
            $recipient_sha256 = (string)$recipient['recipient_event_sha256'];
        }

        $lifecycle_key = 'ctry7.lifecycle.'.substr(pay_ctry_007_hash(array(
            'notice'=>(string)$scope['notice_sha256'],
            'app'=>(string)$scope['applicability_sha256'],
            'recipient'=>$recipient_sha256,
            'type'=>$event_type,
            'due'=>$due_at,
            'reason'=>$reason_code,
            'evidence'=>$evidence_sha256
        )), 0, 48);

        /* Replay is checked before mutable current-assignment state and occurrence time. */
        $existing = pay_ctry_007_one(
            'SELECT notice_lifecycle_event_id,lifecycle_sha256'
            .' FROM '.TB_PREF.'pay_ctry_update_notice_lifecycle_events'
            .' WHERE lifecycle_event_key='.db_escape($lifecycle_key).' LIMIT 2 FOR UPDATE'
        );
        if (is_array($existing)) {
            $ok = true;
            $result = array(
                'notice_lifecycle_event_id'=>(int)$existing['notice_lifecycle_event_id'],
                'lifecycle_sha256'=>(string)$existing['lifecycle_sha256'],
                'replayed'=>true
            );
            break;
        }

        if (is_array($recipient)) {
            $latest = pay_ctry_007_one(
                'SELECT recipient_event_id,event_type FROM '.TB_PREF.'pay_ctry_update_notice_recipient_events'
                .' WHERE notice_audience_id='.(int)$recipient['notice_audience_id']
                .' AND recipient_user_id='.(int)$recipient['recipient_user_id']
                .' ORDER BY event_no DESC LIMIT 1 FOR UPDATE'
            );
            if (!is_array($latest) || (int)$latest['recipient_event_id'] !== $recipient_event_id
                || (string)$latest['event_type'] !== 'assigned') {
                $error = 'recipient_event_not_current';
                break;
            }
        }

        $occurred_at = pay_ctry_007_iso_now($now_epoch);
        $event = array(
            'lifecycle_event_key'=>$lifecycle_key,
            'update_notice_sha256'=>(string)$scope['notice_sha256'],
            'applicability_sha256'=>(string)$scope['applicability_sha256'],
            'recipient_event_sha256'=>$recipient_sha256,
            'event_type'=>$event_type,
            'due_at'=>$due_at,
            'reason_code'=>$reason_code,
            'evidence_sha256'=>$evidence_sha256,
            'actor_id'=>$actor_id,
            'occurred_at'=>$occurred_at,
            'lifecycle_sha256'=>''
        );
        $event['lifecycle_sha256'] = pay_ctry_007_lifecycle_sha256($event);
        if (!pay_ctry_007_validate_lifecycle_event($event, $error)) {
            break;
        }

        $sql_at = pay_ctry_007_sql_now($now_epoch);
        $due_sql = $due_at === null ? 'NULL' : db_escape(gmdate('Y-m-d H:i:s', pay_ctry_007_time($due_at)));
        $sql = 'INSERT INTO '.TB_PREF.'pay_ctry_update_notice_lifecycle_events'
            .' (update_notice_id,notice_applicability_id,recipient_event_id,lifecycle_event_key,event_type,due_at,reason_code,evidence_sha256,lifecycle_sha256,actor_id,occurred_at,created_at) VALUES ('
            .(int)$scope['update_notice_id'].','.(int)$scope['notice_applicability_id'].','
            .($recipient_event_id === null ? 'NULL' : $recipient_event_id).','.db_escape($lifecycle_key).','.db_escape($event_type).','.$due_sql.','
            .db_escape($reason_code).','.db_escape($evidence_sha256).','.db_escape($event['lifecycle_sha256']).','.$actor_id.','
            .db_escape($sql_at).','.db_escape($sql_at).')';
        if (!db_query($sql, 'could not append PAY-CTRY-007 notice lifecycle event')) {
            $error = 'notice_lifecycle_insert_failed';
            break;
        }
        $lifecycle_id = pay_ctry_007_insert_id();

        if (!pay_ctry_007_audit(array(
            'company_id'=>(int)$scope['company_id'],
            'legal_entity_id'=>$scope['legal_entity_id'] === null ? 0 : (int)$scope['legal_entity_id'],
            'entity_type'=>'pay_ctry_update_notice_lifecycle',
            'entity_key'=>array('notice_lifecycle_event_id'=>$lifecycle_id),
            'action'=>'pay.ctry.update.'.$event_type,
            'reason_code'=>$reason_code,
            'before'=>array(),
            'after'=>array('lifecycle_sha256'=>$event['lifecycle_sha256']),
            'restricted_diff'=>array(
                'recipient_scoped'=>$recipient_event_id !== null,
                'employee_fact_payload'=>false,
                'external_delivery'=>false,
                'automatic_pack_activation'=>false
            )
        ), $error)) {
            break;
        }

        $ok = true;
        $result = array(
            'notice_lifecycle_event_id'=>$lifecycle_id,
            'lifecycle_sha256'=>$event['lifecycle_sha256'],
            'replayed'=>false
        );
    } while (false);

    if ($ok) {
        commit_transaction();
    } else {
        cancel_transaction();
    }
    return $ok ? $result : false;
}
?>
