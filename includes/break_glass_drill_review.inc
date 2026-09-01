<?php
/**
 * PAY-SEC-004 break-glass recovery-drill alert/reviewer and provider-restoration evidence.
 *
 * Internal SA_USERS commands only. They reuse append-only PAY-AUD as the durable
 * review record and deliberately do not reuse approval_notifications (draft-bound),
 * add an incident table, deliver notifications, mutate provider state, or widen
 * payroll/payment/accounting authority.
 */

if (!defined('BREAK_GLASS_DRILL_SCOPE'))
    define('BREAK_GLASS_DRILL_SCOPE', 'security_recovery');
if (!defined('BREAK_GLASS_DRILL_REASON'))
    define('BREAK_GLASS_DRILL_REASON', 'recovery_drill');
if (!defined('BREAK_GLASS_DRILL_AUDIT_SOURCE'))
    define('BREAK_GLASS_DRILL_AUDIT_SOURCE', 'security_break_glass_activation_lease');

function break_glass_drill_review_load_dependencies()
{
    global $path_to_root, $security_areas;
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    $activation = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'break_glass_activation.inc';
    if (!function_exists('break_glass_activation_load_dependencies')) {
        if (!is_file($activation))
            return false;
        require_once($activation);
    }
    if (!function_exists('break_glass_activation_load_dependencies')
        || !break_glass_activation_load_dependencies())
        return false;
    if (!isset($security_areas['SA_USERS'])) {
        $access = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'access_levels.inc';
        if (!is_file($access))
            return false;
        require_once($access);
    }
    return isset($security_areas['SA_USERS'][0])
        && function_exists('break_glass_designation_current_actor_id')
        && function_exists('break_glass_designation_company_matches')
        && function_exists('break_glass_designation_row_for_update')
        && function_exists('break_glass_activation_active_lease_for_update')
        && function_exists('append_domain_audit_event');
}

/** Existing same-company SA_USERS authority only. */
function break_glass_drill_review_authorized($company)
{
    $actor_id = function_exists('break_glass_designation_current_actor_id')
        ? break_glass_designation_current_actor_id() : 0;
    return $actor_id > 0
        && function_exists('break_glass_designation_company_matches')
        && break_glass_designation_company_matches((int)$company)
        && function_exists('user_check_access')
        && user_check_access('SA_USERS');
}

/** Validate that a stored role still contains the SA_USERS area. */
function break_glass_drill_role_has_sa_users($areas)
{
    global $security_areas;
    if (!isset($security_areas['SA_USERS'][0]) || !is_string($areas))
        return false;
    $required = (int)$security_areas['SA_USERS'][0];
    foreach (explode(';', $areas) as $area)
        if ($area !== '' && (int)$area === $required)
            return true;
    return false;
}

/** Lock and verify a distinct active reviewer with SA_USERS authority. */
function break_glass_drill_reviewer_for_update($reviewer_user_id)
{
    $reviewer_user_id = (int)$reviewer_user_id;
    if ($reviewer_user_id <= 0 || !defined('TB_PREF'))
        return false;
    $result = db_query(
        "SELECT u.id user_id,u.role_id,u.inactive user_inactive,r.inactive role_inactive,r.areas role_areas "
        ."FROM ".TB_PREF."users u INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE u.id=".$reviewer_user_id." LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 break-glass drill reviewer'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    $extra = db_fetch_assoc($result);
    if (!is_array($row) || is_array($extra)
        || (int)$row['user_inactive'] !== 0 || (int)$row['role_inactive'] !== 0
        || !break_glass_drill_role_has_sa_users((string)$row['role_areas']))
        return false;
    return $row;
}

/** Read the minimum lease identity needed to establish the accepted lock order. */
function break_glass_drill_lease_snapshot($lease_id)
{
    $lease_id = (int)$lease_id;
    if ($lease_id <= 0 || !defined('TB_PREF'))
        return false;
    $result = db_query(
        "SELECT break_glass_activation_lease_id,break_glass_designation_id,user_id FROM "
        .TB_PREF."user_break_glass_activation_leases WHERE break_glass_activation_lease_id=".$lease_id." LIMIT 2",
        'could not read PAY-SEC-004 break-glass drill lease identity'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    $extra = db_fetch_assoc($result);
    return is_array($row) && !is_array($extra) ? $row : false;
}

/** Lock the exact drill lease after its designation is already locked. */
function break_glass_drill_lease_for_update($lease_id)
{
    $lease_id = (int)$lease_id;
    if ($lease_id <= 0 || !defined('TB_PREF'))
        return false;
    $result = db_query(
        "SELECT l.break_glass_activation_lease_id,l.break_glass_designation_id,l.user_id,l.scope_code,"
        ."l.required_auth_method,l.reason_code,l.activated_at,l.expires_at,l.session_id,l.active_slot,"
        ."l.terminalized_at,l.terminalized_by,l.terminal_reason,l.row_version,s.revoked_at session_revoked_at "
        ."FROM ".TB_PREF."user_break_glass_activation_leases l LEFT JOIN ".TB_PREF."user_sessions s ON s.id=l.session_id "
        ."WHERE l.break_glass_activation_lease_id=".$lease_id." LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 break-glass drill lease'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    $extra = db_fetch_assoc($result);
    return is_array($row) && !is_array($extra) ? $row : false;
}

/** Return the three append-only drill review events keyed by legacy source/id. */
function break_glass_drill_review_events($lease_id)
{
    $lease_id = (int)$lease_id;
    if ($lease_id <= 0 || !defined('TB_PREF'))
        return false;
    $actions = array(
        'security.break_glass.drill.alert_raised',
        'security.break_glass.drill.reviewer_acknowledged',
        'security.break_glass.drill.provider_restored'
    );
    $quoted = array();
    foreach ($actions as $action)
        $quoted[] = db_escape($action);
    $result = db_query(
        "SELECT event_id,actor_user_id,action,reason_code,restricted_diff FROM ".TB_PREF."domain_audit_events "
        ."WHERE legacy_source=".db_escape(BREAK_GLASS_DRILL_AUDIT_SOURCE)." AND legacy_id=".$lease_id
        ." AND action IN (".implode(',', $quoted).") ORDER BY event_id",
        'could not read PAY-SEC-004 break-glass drill review evidence'
    );
    if (!$result)
        return false;
    $events = array();
    while ($row = db_fetch_assoc($result)) {
        if (!is_array($row) || !in_array((string)$row['action'], $actions, true)
            || isset($events[(string)$row['action']]))
            return false;
        $diff = json_decode((string)$row['restricted_diff'], true);
        if (!is_array($diff) || !isset($diff['break_glass_activation_lease_id'])
            || (int)$diff['break_glass_activation_lease_id'] !== $lease_id)
            return false;
        $row['restricted_diff_decoded'] = $diff;
        $events[(string)$row['action']] = $row;
    }
    if ((isset($events['security.break_glass.drill.reviewer_acknowledged'])
            || isset($events['security.break_glass.drill.provider_restored']))
        && !isset($events['security.break_glass.drill.alert_raised']))
        return false;
    if (isset($events['security.break_glass.drill.provider_restored'])
        && !isset($events['security.break_glass.drill.reviewer_acknowledged']))
        return false;
    return $events;
}

function break_glass_drill_review_append($company, $lease, $action, $reason_code,
    $reviewer_user_id, $actor_id, $now_epoch)
{
    if (!is_array($lease) || !is_int($now_epoch) || $now_epoch <= 0)
        return false;
    $lease_id = (int)$lease['break_glass_activation_lease_id'];
    $designation_id = (int)$lease['break_glass_designation_id'];
    $user_id = (int)$lease['user_id'];
    if ($lease_id <= 0 || $designation_id <= 0 || $user_id <= 0)
        return false;
    $state = $action === 'security.break_glass.drill.alert_raised' ? 'alert_raised'
        : ($action === 'security.break_glass.drill.reviewer_acknowledged' ? 'reviewer_acknowledged' : 'provider_restored');
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_break_glass_drill_review',
        'entity_key'=>array('break_glass_activation_lease_id'=>$lease_id,'review_state'=>$state),
        'action'=>$action,'reason_code'=>$reason_code,
        'legacy_source'=>BREAK_GLASS_DRILL_AUDIT_SOURCE,'legacy_id'=>$lease_id,
        'occurred_at'=>gmdate('Y-m-d H:i:s', $now_epoch),
        'before'=>null,'after'=>array('review_state'=>$state),
        'restricted_diff'=>array(
            'break_glass_activation_lease_id'=>$lease_id,
            'break_glass_designation_id'=>$designation_id,
            'user_id'=>$user_id,
            'reviewer_user_id'=>(int)$reviewer_user_id,
            'actor_id'=>(int)$actor_id,
            'fixed_scope'=>BREAK_GLASS_DRILL_SCOPE,
            'activation_reason'=>BREAK_GLASS_DRILL_REASON,
            'append_only_pay_aud'=>true,
            'approval_notification_queue_reused'=>false,
            'notification_delivery_claimed'=>false,
            'parallel_incident_table_added'=>false,
            'provider_state_mutated'=>false,
            'role_granted'=>false,'security_area_granted'=>false,
            'payroll_post_authority_changed'=>false,'payment_authority_changed'=>false,
            'accounting_authority_changed'=>false,'historical_rows_rewritten'=>false
        )
    )) !== false;
}

/** Raise one bounded PAY-AUD drill alert and assign an independent SA_USERS reviewer. */
function break_glass_drill_raise_alert($company, $lease_id, $reviewer_user_id, $now_epoch=null)
{
    $company = (int)$company;
    $lease_id = (int)$lease_id;
    $reviewer_user_id = (int)$reviewer_user_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($lease_id <= 0 || $reviewer_user_id <= 0 || !is_int($now) || $now <= 0
        || !break_glass_drill_review_load_dependencies() || !break_glass_drill_review_authorized($company)
        || !function_exists('begin_transaction') || !function_exists('commit_transaction')
        || !function_exists('cancel_transaction'))
        return false;
    $actor_id = break_glass_designation_current_actor_id();
    $snapshot = break_glass_drill_lease_snapshot($lease_id);
    if (!is_array($snapshot))
        return false;
    begin_transaction();
    $committed = false;
    do {
        $designation = break_glass_designation_row_for_update((int)$snapshot['user_id']);
        if (!is_array($designation) || (int)$designation['break_glass_designation_id'] !== (int)$snapshot['break_glass_designation_id'])
            break;
        $lease = break_glass_activation_active_lease_for_update((int)$designation['break_glass_designation_id']);
        if (!is_array($lease) || (int)$lease['break_glass_activation_lease_id'] !== $lease_id
            || (string)$lease['scope_code'] !== BREAK_GLASS_DRILL_SCOPE
            || (string)$lease['required_auth_method'] !== BREAK_GLASS_ACTIVATION_AUTH_METHOD
            || (string)$lease['reason_code'] !== BREAK_GLASS_DRILL_REASON
            || (int)$lease['active_slot'] !== 1 || $lease['terminalized_at'] !== null)
            break;
        $activated = strtotime((string)$lease['activated_at'].' UTC');
        $expires = strtotime((string)$lease['expires_at'].' UTC');
        if ($activated === false || $expires === false || $now < $activated || $now >= $expires)
            break;
        $reviewer = break_glass_drill_reviewer_for_update($reviewer_user_id);
        if (!is_array($reviewer) || $reviewer_user_id === (int)$lease['user_id'] || $reviewer_user_id === $actor_id)
            break;
        $events = break_glass_drill_review_events($lease_id);
        if (!is_array($events))
            break;
        $alert_action = 'security.break_glass.drill.alert_raised';
        if (isset($events[$alert_action])) {
            $diff = $events[$alert_action]['restricted_diff_decoded'];
            if ((int)$diff['reviewer_user_id'] !== $reviewer_user_id || (int)$diff['actor_id'] !== $actor_id)
                break;
            commit_transaction();
            $committed = true;
            break;
        }
        if (!break_glass_drill_review_append($company,$lease,$alert_action,
            'recovery_drill_alert',$reviewer_user_id,$actor_id,$now))
            break;
        commit_transaction();
        $committed = true;
    } while (false);
    if (!$committed) {
        cancel_transaction();
        return false;
    }
    return true;
}

/** Acknowledge the assigned drill alert; only the assigned reviewer may do so. */
function break_glass_drill_acknowledge($company, $lease_id, $now_epoch=null)
{
    $company = (int)$company;
    $lease_id = (int)$lease_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($lease_id <= 0 || !is_int($now) || $now <= 0 || !break_glass_drill_review_load_dependencies()
        || !break_glass_drill_review_authorized($company) || !function_exists('begin_transaction')
        || !function_exists('commit_transaction') || !function_exists('cancel_transaction'))
        return false;
    $actor_id = break_glass_designation_current_actor_id();
    $snapshot = break_glass_drill_lease_snapshot($lease_id);
    if (!is_array($snapshot))
        return false;
    begin_transaction();
    $committed = false;
    do {
        $designation = break_glass_designation_row_for_update((int)$snapshot['user_id']);
        if (!is_array($designation) || (int)$designation['break_glass_designation_id'] !== (int)$snapshot['break_glass_designation_id'])
            break;
        $lease = break_glass_drill_lease_for_update($lease_id);
        if (!is_array($lease) || (int)$lease['break_glass_designation_id'] !== (int)$designation['break_glass_designation_id']
            || (string)$lease['scope_code'] !== BREAK_GLASS_DRILL_SCOPE
            || (string)$lease['reason_code'] !== BREAK_GLASS_DRILL_REASON)
            break;
        $events = break_glass_drill_review_events($lease_id);
        $alert_action = 'security.break_glass.drill.alert_raised';
        $ack_action = 'security.break_glass.drill.reviewer_acknowledged';
        if (!is_array($events) || !isset($events[$alert_action]))
            break;
        $reviewer_user_id = (int)$events[$alert_action]['restricted_diff_decoded']['reviewer_user_id'];
        if ($reviewer_user_id !== $actor_id || !is_array(break_glass_drill_reviewer_for_update($reviewer_user_id)))
            break;
        if (isset($events[$ack_action])) {
            commit_transaction();
            $committed = true;
            break;
        }
        if (!break_glass_drill_review_append($company,$lease,$ack_action,
            'recovery_drill_reviewed',$reviewer_user_id,$actor_id,$now))
            break;
        commit_transaction();
        $committed = true;
    } while (false);
    if (!$committed) {
        cancel_transaction();
        return false;
    }
    return true;
}

/**
 * Attest normal provider restoration after explicit return-to-normal. This is
 * evidence only: provider/configuration/link rows are never mutated here.
 */
function break_glass_drill_attest_provider_restored($company, $lease_id, $now_epoch=null)
{
    $company = (int)$company;
    $lease_id = (int)$lease_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($lease_id <= 0 || !is_int($now) || $now <= 0 || !break_glass_drill_review_load_dependencies()
        || !break_glass_drill_review_authorized($company) || !function_exists('begin_transaction')
        || !function_exists('commit_transaction') || !function_exists('cancel_transaction'))
        return false;
    $actor_id = break_glass_designation_current_actor_id();
    $snapshot = break_glass_drill_lease_snapshot($lease_id);
    if (!is_array($snapshot))
        return false;
    begin_transaction();
    $committed = false;
    do {
        $designation = break_glass_designation_row_for_update((int)$snapshot['user_id']);
        if (!is_array($designation) || (int)$designation['break_glass_designation_id'] !== (int)$snapshot['break_glass_designation_id'])
            break;
        $lease = break_glass_drill_lease_for_update($lease_id);
        if (!is_array($lease) || (int)$lease['break_glass_designation_id'] !== (int)$designation['break_glass_designation_id']
            || (string)$lease['scope_code'] !== BREAK_GLASS_DRILL_SCOPE
            || (string)$lease['reason_code'] !== BREAK_GLASS_DRILL_REASON
            || $lease['active_slot'] !== null || $lease['terminalized_at'] === null
            || (string)$lease['terminal_reason'] !== 'return_to_normal'
            || ($lease['session_id'] !== null && $lease['session_revoked_at'] === null))
            break;
        $events = break_glass_drill_review_events($lease_id);
        $alert_action = 'security.break_glass.drill.alert_raised';
        $ack_action = 'security.break_glass.drill.reviewer_acknowledged';
        $restored_action = 'security.break_glass.drill.provider_restored';
        if (!is_array($events) || !isset($events[$alert_action]) || !isset($events[$ack_action]))
            break;
        $reviewer_user_id = (int)$events[$alert_action]['restricted_diff_decoded']['reviewer_user_id'];
        if ($reviewer_user_id !== $actor_id || !is_array(break_glass_drill_reviewer_for_update($reviewer_user_id)))
            break;
        if (isset($events[$restored_action])) {
            commit_transaction();
            $committed = true;
            break;
        }
        if (!break_glass_drill_review_append($company,$lease,$restored_action,
            'normal_provider_restored',$reviewer_user_id,$actor_id,$now))
            break;
        commit_transaction();
        $committed = true;
    } while (false);
    if (!$committed) {
        cancel_transaction();
        return false;
    }
    return true;
}
