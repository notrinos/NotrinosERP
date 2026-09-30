<?php
/** HRM-TAL-001 current-effective manager/team subject resolver.
 *
 * The browser never supplies an employee selector. Team membership comes only
 * from the accepted HRM-ESS-002 team_view scope, backed by the HRM-ESS-001
 * Assignment resolver, for the authenticated session and current date.
 */
include_once($path_to_root.'/hrm/includes/db/hrm_tal_001_catalog_db.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_ess_002_context_db.inc');

/** @return array|false */
function hrm_tal_001_manager_scope_subjects($as_of, &$error = null)
{
    $error = null;
    $version = hrm_tal_001_runtime_database_version();
    if (!is_string($version) || !hrm_tal_001_manager_scope_allowed_for_database_version($version))
        return hrm_tal_001_catalog_fail($error, 'manager_scope_not_active');
    if (!function_exists('user_check_access') || !user_check_access('SA_HRM_ESS_VIEW_TEAM'))
        return hrm_tal_001_catalog_fail($error, 'permission_denied');
    if (hrm_tal_001_runtime_company_id() < 0 || hrm_tal_001_runtime_actor_id() <= 0)
        return hrm_tal_001_catalog_fail($error, 'authenticated_scope_required');
    if (!function_exists('hrm_ess_002_date') || !function_exists('hrm_ess_002_today')
        || !function_exists('get_hrm_ess_002_current_manager_scope'))
        return hrm_tal_001_catalog_fail($error, 'manager_scope_dependency_unavailable');
    $as_of = hrm_ess_002_date($as_of);
    if ($as_of === false || $as_of !== hrm_ess_002_today())
        return hrm_tal_001_catalog_fail($error, 'manager_scope_date_invalid');
    $scope = get_hrm_ess_002_current_manager_scope($as_of, 'team_view');
    if (!is_array($scope))
        return hrm_tal_001_catalog_fail($error, 'manager_scope_unavailable');

    $subjects = array();
    $seen_employees = array();
    foreach ($scope as $row) {
        if (!is_array($row) || empty($row['worker_id']) || empty($row['assignment_id'])
            || empty($row['legal_entity_id']) || !isset($row['scope_origin'])
            || !in_array((string)$row['scope_origin'], array('direct','delegated'), true))
            return hrm_tal_001_catalog_fail($error, 'manager_scope_subject_invalid');
        $worker_id = (int)$row['worker_id'];
        if ($worker_id <= 0)
            return hrm_tal_001_catalog_fail($error, 'manager_scope_worker_invalid');

        $mapping = db_query('SELECT mapping_id,worker_id,employee_number,employee_id FROM '.TB_PREF.'hrm_employee_worker_map '
            .'WHERE worker_id='.$worker_id.' AND retired_at IS NULL ORDER BY mapping_id',
            'could not resolve HRM-TAL-001 manager employee mapping');
        if (!$mapping || db_num_rows($mapping) !== 1)
            return hrm_tal_001_catalog_fail($error, 'manager_scope_employee_mapping_missing_or_ambiguous');
        $map = db_fetch_assoc($mapping);
        $employee_id = trim((string)$map['employee_id']);
        if ((int)$map['mapping_id'] <= 0 || (int)$map['worker_id'] !== $worker_id
            || $employee_id === '' || strlen($employee_id) > 20
            || isset($seen_employees[$employee_id]))
            return hrm_tal_001_catalog_fail($error, 'manager_scope_employee_mapping_invalid');
        $seen_employees[$employee_id] = true;
        $subjects[] = array(
            'worker_id' => $worker_id,
            'assignment_id' => (int)$row['assignment_id'],
            'legal_entity_id' => (int)$row['legal_entity_id'],
            'employee_id' => $employee_id,
            'scope_origin' => (string)$row['scope_origin']
        );
    }
    return $subjects;
}
?>
