<?php
/** HRM-ESS-001 fail-closed current-session self-scope resolver. */
if (!function_exists('hrm_ess_identity_link_actor_id'))
    include_once($path_to_root.'/hrm/includes/db/ess_identity_link_db.inc');

/**
 * Resolve self scope exclusively from the authenticated account's approved
 * immutable ESS link. Caller input can never select another account.
 */
function get_hrm_ess_current_self_scope()
{
    $actor = hrm_ess_identity_link_actor_id();
    if ($actor <= 0 || !hrm_ess_identity_link_tables_ready())
        return false;
    $sql = 'SELECT l.link_id,l.user_id,l.worker_id,w.person_id,m.mapping_id,m.employee_number,m.employee_id '
        .'FROM '.TB_PREF.'hrm_ess_identity_links l '
        .'INNER JOIN '.TB_PREF.'users u ON u.id=l.user_id AND u.inactive=0 '
        .'INNER JOIN '.TB_PREF."hrm_workers w ON w.worker_id=l.worker_id AND w.worker_status='active' "
        .'INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.worker_id=l.worker_id AND m.retired_at IS NULL '
        .'WHERE l.user_id='.$actor.' ORDER BY l.link_id';
    $r = db_query($sql, 'could not resolve ESS self scope');
    if (!$r || db_num_rows($r) !== 1)
        return false;
    $row = db_fetch_assoc($r);
    if (!$row || (int)$row['user_id'] !== $actor || (int)$row['link_id'] <= 0
        || (int)$row['worker_id'] <= 0 || (int)$row['mapping_id'] <= 0)
        return false;
    return array('link_id'=>(int)$row['link_id'],'user_id'=>$actor,
        'worker_id'=>(int)$row['worker_id'],'person_id'=>(int)$row['person_id'],
        'mapping_id'=>(int)$row['mapping_id'],'employee_number'=>(int)$row['employee_number'],
        'employee_id'=>(string)$row['employee_id']);
}

function hrm_ess_current_user_owns_worker($worker_id)
{
    $scope = get_hrm_ess_current_self_scope();
    return is_array($scope) && (int)$worker_id > 0 && (int)$scope['worker_id'] === (int)$worker_id;
}

function hrm_ess_current_user_owns_employee_number($employee_number)
{
    $scope = get_hrm_ess_current_self_scope();
    return is_array($scope) && (int)$employee_number > 0
        && (int)$scope['employee_number'] === (int)$employee_number;
}
