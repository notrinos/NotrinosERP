<?php
/** HRM-ESS-001 assignment-scoped manager and dated delegation resolver. */
if (!function_exists('get_hrm_ess_current_self_scope'))
    include_once($path_to_root.'/hrm/includes/db/ess_self_scope_db.inc');

function hrm_ess_manager_scope_has_area($area)
{
    return function_exists('user_check_access') && user_check_access($area);
}

function hrm_ess_manager_scope_normalize_date($as_of)
{
    $as_of = trim((string)$as_of);
    if (preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $as_of, $m) !== 1)
        return false;
    if (!checkdate((int)$m[2], (int)$m[3], (int)$m[1]))
        return false;
    return sprintf('%04d-%02d-%02d', (int)$m[1], (int)$m[2], (int)$m[3]);
}

function hrm_ess_manager_scope_tables_ready($with_delegation = false)
{
    $tables = array('hrm_workers','hrm_employments','hrm_assignments','hrm_ess_identity_links');
    if ($with_delegation)
        $tables[] = 'hrm_ess_manager_delegations';
    foreach ($tables as $table) {
        $r = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function($table)."'");
        if (!$r || db_num_rows($r) !== 1)
            return false;
    }
    return true;
}

/** Return exact Assignment ids owned by one Worker at one date. */
function hrm_ess_manager_assignment_ids_as_of($worker_id, $as_of)
{
    $worker_id = (int)$worker_id;
    $as_of = hrm_ess_manager_scope_normalize_date($as_of);
    if ($worker_id <= 0 || $as_of === false || !hrm_ess_manager_scope_tables_ready(false))
        return false;
    $d = db_escape($as_of);
    $sql = 'SELECT a.assignment_id,a.employment_id,h.legal_entity_id FROM '.TB_PREF.'hrm_employments h '
        .'INNER JOIN '.TB_PREF.'hrm_assignments a ON a.employment_id=h.employment_id '
        .'WHERE h.worker_id='.$worker_id.' '
        .'AND (h.start_date IS NULL OR h.start_date<='.$d.') AND (h.end_date IS NULL OR h.end_date>='.$d.') '
        .'AND (a.effective_from IS NULL OR a.effective_from<='.$d.') AND (a.effective_to IS NULL OR a.effective_to>='.$d.') '
        .'ORDER BY a.assignment_id';
    $r = db_query($sql, 'could not resolve manager Assignment scope');
    if (!$r)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($r)) {
        $assignment_id = (int)$row['assignment_id'];
        $employment_id = (int)$row['employment_id'];
        $legal_entity_id = (int)$row['legal_entity_id'];
        if ($assignment_id <= 0 || $employment_id <= 0 || $legal_entity_id <= 0)
            return false;
        $rows[$assignment_id] = array('assignment_id'=>$assignment_id,
            'employment_id'=>$employment_id,'legal_entity_id'=>$legal_entity_id);
    }
    return $rows;
}

/** Resolve only direct reports bound to the supplied manager Worker at the same as-of date. */
function hrm_ess_direct_team_scope_for_worker($manager_worker_id, $as_of)
{
    $manager_worker_id = (int)$manager_worker_id;
    $as_of = hrm_ess_manager_scope_normalize_date($as_of);
    if ($manager_worker_id <= 0 || $as_of === false)
        return false;
    $manager_assignments = hrm_ess_manager_assignment_ids_as_of($manager_worker_id, $as_of);
    if ($manager_assignments === false)
        return false;
    if (!$manager_assignments)
        return array();
    $ids = array_keys($manager_assignments);
    $ids = array_map('intval', $ids);
    $d = db_escape($as_of);
    $sql = 'SELECT h.worker_id,h.employment_id,h.legal_entity_id,a.assignment_id,a.manager_assignment_id,a.legacy_employee_number '
        .'FROM '.TB_PREF.'hrm_assignments a INNER JOIN '.TB_PREF.'hrm_employments h ON h.employment_id=a.employment_id '
        .'WHERE a.manager_assignment_id IN ('.implode(',', $ids).') '
        .'AND (h.start_date IS NULL OR h.start_date<='.$d.') AND (h.end_date IS NULL OR h.end_date>='.$d.') '
        .'AND (a.effective_from IS NULL OR a.effective_from<='.$d.') AND (a.effective_to IS NULL OR a.effective_to>='.$d.') '
        .'ORDER BY h.worker_id,a.assignment_id';
    $r = db_query($sql, 'could not resolve direct ESS manager team scope');
    if (!$r)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($r)) {
        $manager_assignment_id = (int)$row['manager_assignment_id'];
        if (!isset($manager_assignments[$manager_assignment_id])
            || (int)$manager_assignments[$manager_assignment_id]['legal_entity_id'] !== (int)$row['legal_entity_id']
            || (int)$row['worker_id'] <= 0 || (int)$row['worker_id'] === $manager_worker_id)
            return false;
        $rows[] = array('worker_id'=>(int)$row['worker_id'],'employment_id'=>(int)$row['employment_id'],
            'assignment_id'=>(int)$row['assignment_id'],'manager_assignment_id'=>$manager_assignment_id,
            'legal_entity_id'=>(int)$row['legal_entity_id'],'employee_number'=>(int)$row['legacy_employee_number']);
    }
    return $rows;
}

function hrm_ess_manager_delegation_digest_is_valid($digest)
{
    return is_string($digest) && preg_match('/^[a-f0-9]{64}$/D', $digest) === 1;
}

/** Resolve active delegation principals for the authenticated Worker's session. */
function hrm_ess_active_delegated_manager_workers($delegate_worker_id, $as_of)
{
    $delegate_worker_id = (int)$delegate_worker_id;
    $as_of = hrm_ess_manager_scope_normalize_date($as_of);
    if ($delegate_worker_id <= 0 || $as_of === false || !hrm_ess_manager_scope_tables_ready(true))
        return false;
    $d = db_escape($as_of);
    $sql = 'SELECT delegation_id,delegator_worker_id,delegate_worker_id,scope_code,effective_from,effective_to,'
        .'evidence_digest,approval_digest,delegation_digest,requested_by,approved_by FROM '.TB_PREF.'hrm_ess_manager_delegations '
        .'WHERE delegate_worker_id='.$delegate_worker_id." AND scope_code='direct_team' "
        .'AND effective_from<='.$d.' AND effective_to>='.$d.' ORDER BY delegation_id';
    $r = db_query($sql, 'could not resolve active ESS manager delegations');
    if (!$r)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($r)) {
        $delegator = (int)$row['delegator_worker_id'];
        if ((int)$row['delegation_id'] <= 0 || $delegator <= 0 || $delegator === $delegate_worker_id
            || (int)$row['requested_by'] <= 0 || (int)$row['approved_by'] <= 0
            || (int)$row['requested_by'] === (int)$row['approved_by']
            || (string)$row['scope_code'] !== 'direct_team'
            || !hrm_ess_manager_delegation_digest_is_valid(strtolower((string)$row['evidence_digest']))
            || !hrm_ess_manager_delegation_digest_is_valid(strtolower((string)$row['approval_digest']))
            || !hrm_ess_manager_delegation_digest_is_valid(strtolower((string)$row['delegation_digest'])))
            return false;
        $rows[] = array('delegation_id'=>(int)$row['delegation_id'],'manager_worker_id'=>$delegator);
    }
    return $rows;
}

/**
 * Current-session manager scope. Identity is session-derived; caller may select
 * only the as-of date. Direct and explicitly delegated direct-team scopes are
 * combined, deduplicated by Worker/Assignment, and contain identifiers only.
 */
function get_hrm_ess_current_manager_scope($as_of)
{
    $as_of = hrm_ess_manager_scope_normalize_date($as_of);
    if ($as_of === false || !hrm_ess_manager_scope_has_area('SA_HRM_ESS_VIEW_TEAM'))
        return false;
    $self = get_hrm_ess_current_self_scope();
    if (!is_array($self) || empty($self['worker_id']))
        return false;
    $actor_worker_id = (int)$self['worker_id'];
    $subjects = array();
    $direct = hrm_ess_direct_team_scope_for_worker($actor_worker_id, $as_of);
    if ($direct === false)
        return false;
    foreach ($direct as $row) {
        $row['scope_origin'] = 'direct';
        $row['delegation_id'] = null;
        $subjects[$row['worker_id'].':'.$row['assignment_id']] = $row;
    }
    $delegations = hrm_ess_active_delegated_manager_workers($actor_worker_id, $as_of);
    if ($delegations === false)
        return false;
    foreach ($delegations as $delegation) {
        $delegated = hrm_ess_direct_team_scope_for_worker((int)$delegation['manager_worker_id'], $as_of);
        if ($delegated === false)
            return false;
        foreach ($delegated as $row) {
            $key = $row['worker_id'].':'.$row['assignment_id'];
            if (!isset($subjects[$key])) {
                $row['scope_origin'] = 'delegated';
                $row['delegation_id'] = (int)$delegation['delegation_id'];
                $subjects[$key] = $row;
            }
        }
    }
    ksort($subjects, SORT_STRING);
    return array_values($subjects);
}

function hrm_ess_current_manager_can_access_worker($worker_id, $as_of)
{
    $worker_id = (int)$worker_id;
    if ($worker_id <= 0)
        return false;
    $scope = get_hrm_ess_current_manager_scope($as_of);
    if (!is_array($scope))
        return false;
    foreach ($scope as $row)
        if ((int)$row['worker_id'] === $worker_id)
            return true;
    return false;
}
