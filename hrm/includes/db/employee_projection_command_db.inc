<?php
/** FND-006 authorization and command-to-owner bridge; no additional public route. */
include_once(dirname(__FILE__).'/employee_compatibility_projector_db.inc');

/** Restrict generic profile changes and require typed independent lifecycle authority. */
function hrm_employee_projection_update_authority($before, $candidate, $command_id = 0, $assignment_change = null)
{
    if (!function_exists('user_check_access') || hrm_person_worker_actor_id() <= 0) return false;
    $changed = hrm_employee_projection_difference($before, $candidate);
    if ($changed === false) return false;
    $profile = array('first_name','middle_name','last_name','gender','birth_date','nationality','marital_status',
        'dependents_no','address','city','state','country','phone','mobile','email','personal_email',
        'emergency_name','emergency_relation','emergency_phone','notes');
    if ((int)$command_id === 0)
        return (user_check_access('SA_EMPLOYEE') || user_check_access('SA_IMPORTEMP'))
            && $assignment_change === null && !array_diff($changed, $profile);
    $command = hrm_employee_projection_one('SELECT c.*,d.draft_data FROM '.TB_PREF.'hrm_lifecycle_commands c'
        .' INNER JOIN '.TB_PREF.'approval_drafts d ON d.id=c.approval_draft_id AND d.status=1'
        .' INNER JOIN '.TB_PREF.'hrm_employee_worker_map m ON m.worker_id=c.worker_id'
        .' WHERE c.lifecycle_command_id='.(int)$command_id.' AND m.employee_number='.(int)$before['employee_number']
        .' AND m.retired_at IS NULL FOR UPDATE');
    if (!$command || $command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || (int)$command['requested_by'] <= 0 || (int)$command['requested_by'] === hrm_person_worker_actor_id()
        || (int)$command['approval_draft_id'] <= 0) return false;
    $allowed = array(
        'employee_confirmation'=>array('SA_EMPLOYEE',array('confirmation_date')),
        'employee_probation_schedule'=>array('SA_EMPLOYEE',array('probation_end_date')),
        'employee_salary_revision'=>array('SA_SALARYREVISION',array('personal_salary')),
        'employee_separation'=>array('SA_EMPSEPARATION',array('inactive','released_date')),
        'employee_transfer'=>array('SA_EMPTRANSFER',array('department_id','position_id','grade_id','reporting_to')),
        'employee_promotion'=>array('SA_EMPTRANSFER',array('department_id','position_id','grade_id','reporting_to')));
    $type = $command['command_type'];
    if (!isset($allowed[$type]) || !user_check_access($allowed[$type][0])
        || array_diff($changed, $allowed[$type][1])) return false;
    $draft = json_decode(html_entity_decode($command['draft_data'], ENT_QUOTES, 'UTF-8'), true);
    if (!is_array($draft) || !isset($draft['lifecycle_command_id'], $draft['request_hash'])
        || (int)$draft['lifecycle_command_id'] !== (int)$command_id
        || !hash_equals((string)$command['request_hash'], (string)$draft['request_hash'])) return false;
    $date = substr($command['effective_at'], 0, 10);
    if ($type === 'employee_confirmation') return $assignment_change === null && $candidate['confirmation_date'] === $date;
    if ($type === 'employee_probation_schedule') return $assignment_change === null && $candidate['probation_end_date'] === $date;
    if ($type === 'employee_salary_revision') return $assignment_change === null && (int)$candidate['personal_salary'] === 1;
    if (!is_array($assignment_change) || !isset($assignment_change['effective_date'])
        || $assignment_change['effective_date'] !== $date) return false;
    if ($type === 'employee_separation') return (int)$candidate['inactive'] === 1 && $candidate['released_date'] === $date
        && $assignment_change['change_type'] === 'separation';
    foreach (array('department_id','position_id','grade_id') as $field)
        if (!isset($draft['new_'.$field]) || (int)$candidate[$field] !== (int)$draft['new_'.$field]) return false;
    foreach (array('job_id','work_location_id') as $field)
        if (isset($assignment_change[$field]) && (!isset($draft['new_'.$field])
            || (int)$assignment_change[$field] !== (int)$draft['new_'.$field])) return false;
    if (isset($assignment_change['manager_employee_id']) || in_array('reporting_to', $changed, true)) {
        $manager = isset($draft['manager_employee_number']) ? get_hrm_person_worker_binding((int)$draft['manager_employee_number']) : false;
        if (!$manager || $candidate['reporting_to'] !== $manager['employee_id']
            || !isset($assignment_change['manager_employee_id']) || $assignment_change['manager_employee_id'] !== $manager['employee_id']) return false;
    }
    return $assignment_change['change_type'] === ($type === 'employee_transfer' ? 'transfer' : 'promotion');
}

/** Verify the existing sealed Hire command rather than admit direct helper creation. */
function hrm_employee_projection_hire_authority($data, $command_id)
{
    if ((int)$command_id <= 0 || !function_exists('load_hrm_lifecycle_employee_hire_payload_for_update')
        || !function_exists('user_check_access') || !user_check_access('SA_EMPLOYEE')) return false;
    $loaded = load_hrm_lifecycle_employee_hire_payload_for_update((int)$command_id, array('pending'));
    $draft = $loaded ? hrm_employee_projection_one('SELECT id FROM '.TB_PREF.'approval_drafts WHERE id='
        .(int)$loaded['command']['approval_draft_id'].' AND status=1 FOR UPDATE') : false;
    return $loaded && $draft && hrm_person_worker_actor_id() > 0 && (int)$loaded['command']['requested_by'] > 0
        && (int)$loaded['command']['requested_by'] !== hrm_person_worker_actor_id()
        && (int)$loaded['command']['approval_draft_id'] > 0
        && hrm_lifecycle_hire_payload_canonical_json($data) === hrm_lifecycle_hire_payload_canonical_json($loaded['payload']);
}

/** Project all bindings of a changed Person to prevent canonical-link sibling drift. */
function hrm_employee_projection_materialize_person($employee_number)
{
    $binding = get_hrm_person_worker_binding((int)$employee_number, true);
    if (!$binding) return false;
    $result = db_query('SELECT m.employee_number FROM '.TB_PREF.'hrm_employee_worker_map m'
        .' INNER JOIN '.TB_PREF.'hrm_workers w ON w.worker_id=m.worker_id WHERE w.person_id='.(int)$binding['person_id']
        .' AND m.retired_at IS NULL ORDER BY m.employee_number FOR UPDATE');
    if (!$result) return false;
    while ($mapped = db_fetch_assoc($result)) {
        $stored = hrm_employee_projection_one('SELECT * FROM '.TB_PREF.'employees WHERE employee_number='.(int)$mapped['employee_number'].' FOR UPDATE');
        $expected = $stored ? hrm_employee_projection_build((int)$mapped['employee_number']) : false;
        if (!$expected || !hrm_employee_projection_store($stored, $expected)) return false;
    }
    return true;
}
