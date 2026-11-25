<?php
/**
 * The one copy of the check that an employee already has a pending lifecycle command.
 *
 * Each fixed lifecycle command (confirmation, probation schedule, promotion, reinstatement, salary revision,
 * separation, suspension, training assignment, transfer) refuses to start while another command for the same worker is
 * pending. require_once this file from each calling file; never copy the check under a command name
 * (tests/php/test_hrm_lifecycle_other_pending.php refuses copies).
 */

/**
 * Tell whether the worker has a pending lifecycle command, locking the row that answers it.
 *
 * Runs inside the caller's transaction: SELECT ... FOR UPDATE keeps a second command from slipping in before
 * the caller writes its own.
 *
 * @param int $worker_id
 * @return bool True when a pending command exists.
 */
function hrm_lifecycle_other_pending_exists($worker_id)
{
    $result = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id="
        .(int)$worker_id." AND command_status='pending' ORDER BY lifecycle_command_id LIMIT 1 FOR UPDATE",
        'could not inspect pending Employee lifecycle command');
    return $result && db_fetch_assoc($result) ? true : false;
}
