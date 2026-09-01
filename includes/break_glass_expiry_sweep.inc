<?php
/**
 * PAY-SEC-004 scoped local break-glass expiry sweep command foundation.
 *
 * Internal operator-invoked cleanup only. It terminalizes already-expired active
 * security_recovery leases under existing SA_USERS authority, preserving the
 * accepted durable-session-revoke-before-lease-terminal ordering. It adds no
 * scheduler/background writer, role/security-area grant, provider bypass, or
 * payroll/payment/accounting authority.
 */

if (!defined('BREAK_GLASS_EXPIRY_SWEEP_SCOPE'))
    define('BREAK_GLASS_EXPIRY_SWEEP_SCOPE', 'security_recovery');
if (!defined('BREAK_GLASS_EXPIRY_SWEEP_MAX_BATCH'))
    define('BREAK_GLASS_EXPIRY_SWEEP_MAX_BATCH', 100);

function break_glass_expiry_sweep_load_dependencies()
{
    global $path_to_root;
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    $file = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'break_glass_activation.inc';
    if (!is_file($file))
        return false;
    require_once($file);
    if (!function_exists('break_glass_activation_load_dependencies')
        || !break_glass_activation_load_dependencies())
        return false;
    return function_exists('break_glass_activation_active_lease_for_update')
        && function_exists('break_glass_activation_terminalize_locked_lease')
        && function_exists('break_glass_designation_current_actor_id')
        && function_exists('break_glass_designation_company_matches')
        && function_exists('break_glass_designation_row_for_update')
        && function_exists('session_registry_hash')
        && function_exists('append_domain_audit_event');
}

/** Existing same-company SA_USERS authority only; no service identity is invented. */
function break_glass_expiry_sweep_authorized($company)
{
    $actor_id = function_exists('break_glass_designation_current_actor_id')
        ? break_glass_designation_current_actor_id() : 0;
    return $actor_id > 0
        && function_exists('break_glass_designation_company_matches')
        && break_glass_designation_company_matches((int)$company)
        && function_exists('user_check_access')
        && user_check_access('SA_USERS');
}

function break_glass_expiry_sweep_batch_size($limit)
{
    $limit = (int)$limit;
    return $limit >= 1 && $limit <= BREAK_GLASS_EXPIRY_SWEEP_MAX_BATCH ? $limit : false;
}

/**
 * Read a deterministic candidate page without locking. Each candidate is then
 * revalidated under the accepted designation -> active-lease lock order.
 */
function break_glass_expiry_sweep_candidates($now_epoch, $limit)
{
    if (!defined('TB_PREF') || !function_exists('db_query') || !function_exists('db_fetch_assoc')
        || !is_int($now_epoch) || $now_epoch <= 0 || break_glass_expiry_sweep_batch_size($limit) === false)
        return false;
    $now = gmdate('Y-m-d H:i:s', $now_epoch);
    $sql = "SELECT break_glass_activation_lease_id,break_glass_designation_id,user_id,expires_at "
        ."FROM ".TB_PREF."user_break_glass_activation_leases WHERE active_slot=1 AND terminalized_at IS NULL "
        ."AND scope_code=".db_escape(BREAK_GLASS_EXPIRY_SWEEP_SCOPE)." AND expires_at<=".db_escape($now)
        ." ORDER BY expires_at,break_glass_activation_lease_id LIMIT ".(int)$limit;
    $result = db_query($sql, 'could not read PAY-SEC-004 expired break-glass lease candidates');
    if (!$result)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        if (!is_array($row))
            return false;
        $rows[] = $row;
    }
    return $rows;
}

/**
 * Terminalize a bounded batch of already-expired leases. Per-lease PAY-AUD is
 * emitted by the accepted terminal command inside the same transaction.
 */
function break_glass_expiry_sweep($company, $limit=25, $now_epoch=null)
{
    $company = (int)$company;
    $now = $now_epoch === null ? time() : $now_epoch;
    $limit = break_glass_expiry_sweep_batch_size($limit);
    if ($limit === false || !is_int($now) || $now <= 0 || !break_glass_expiry_sweep_load_dependencies()
        || !break_glass_expiry_sweep_authorized($company)
        || !function_exists('begin_transaction') || !function_exists('commit_transaction')
        || !function_exists('cancel_transaction'))
        return false;
    $actor_id = break_glass_designation_current_actor_id();
    $candidates = break_glass_expiry_sweep_candidates($now, $limit);
    if (!is_array($candidates))
        return false;
    $processed = 0;
    $skipped = 0;
    foreach ($candidates as $candidate) {
        $candidate_lease_id = isset($candidate['break_glass_activation_lease_id'])
            ? (int)$candidate['break_glass_activation_lease_id'] : 0;
        $user_id = isset($candidate['user_id']) ? (int)$candidate['user_id'] : 0;
        if ($candidate_lease_id <= 0 || $user_id <= 0)
            return false;
        begin_transaction();
        $committed = false;
        do {
            $designation = break_glass_designation_row_for_update($user_id);
            if (!is_array($designation))
                break;
            $lease = break_glass_activation_active_lease_for_update((int)$designation['break_glass_designation_id']);
            if ($lease === false)
                break;
            if ($lease === null || (int)$lease['break_glass_activation_lease_id'] !== $candidate_lease_id) {
                commit_transaction();
                $committed = true;
                $skipped++;
                break;
            }
            $expires = strtotime((string)$lease['expires_at'].' UTC');
            if ($expires === false)
                break;
            if ($now < $expires) {
                commit_transaction();
                $committed = true;
                $skipped++;
                break;
            }
            if (!break_glass_activation_terminalize_locked_lease(
                $company, $lease, $actor_id, 'lease_expired', $now))
                break;
            commit_transaction();
            $committed = true;
            $processed++;
        } while (false);
        if (!$committed) {
            cancel_transaction();
            return false;
        }
    }
    return array(
        'processed'=>$processed,
        'skipped'=>$skipped,
        'candidate_count'=>count($candidates),
        'batch_limit'=>$limit,
        'remaining_possible'=>count($candidates)===$limit,
        'terminal_reason'=>'lease_expired',
        'role_granted'=>false,
        'security_area_granted'=>false,
        'background_writer_added'=>false
    );
}
