<?php
/** Complete employee presentation reads; callers retain their full readable guard. */

/**
 * Read one complete, stable authorized cohort without caching verification.
 * Inactive ordinary choices are delivered for exact lookup, never fragment browsing.
 * The completed name batch remains server-only and may serve this same render.
 * @param array $settings Fresh server-owned control contract.
 * @param array|false|null $prepared_batch Completed names, cleared on any failure.
 * @return array|false Minimal presentation snapshot, never command authority.
 */
function hrm_employee_selector_snapshot($settings, &$prepared_batch = null)
{
    $prepared_batch = false;
    if (!hrm_employee_selector_search_scope_is_current()
        || !hrm_employee_selector_refresh_access($settings['area'])) return false;
    $roles = $_SESSION['wa_current_user']->role_set;
    $identity_allowed = !empty($settings['canonical']) && hrm_person_worker_user_can_read();
    $before = hrm_employee_selector_page_revision();
    if ($before === false) return false;
    $cohort = hrm_employee_selector_cohort_sql($settings, false);
    $result = db_query('SELECT employee_id,name,department_name,inactive,legacy_name sort_name,sort_key FROM ('
        .$cohort.') choices ORDER BY sort_key,BINARY employee_id', 'could not prepare employee choices');
    if (!$result) return false;
    $rows = array();
    while ($row = db_fetch_assoc($result)) $rows[] = $row;
    db_free_result($result);
    $batch = $identity_allowed
        ? get_hrm_person_worker_report_names_batch_as_of(array_column($rows, 'employee_id'), $settings['as_of']) : false;
    $choices = array();
    $name_settings = $settings;
    $name_settings['codes'] = false;
    $name_settings['manager'] = false;
    foreach ($rows as $row) {
        $choice = array('id' => (string)$row['employee_id'],
            'text' => hrm_employee_selector_result_label($row, $settings, $batch),
            'name' => hrm_employee_selector_result_label($row, $name_settings, $batch),
            'inactive' => !empty($row['inactive']));
        if (empty($settings['manager'])) $choice['department'] = $row['department_name'];
        $choices[] = $choice;
    }
    if ($before !== hrm_employee_selector_page_revision()
        || !hrm_employee_selector_search_scope_is_current()
        || !hrm_employee_selector_refresh_access($settings['area'])
        || $roles !== $_SESSION['wa_current_user']->role_set
        || $identity_allowed !== (!empty($settings['canonical']) && hrm_person_worker_user_can_read())) return false;
    $prepared_batch = $batch;
    return array('results' => $choices, 'complete' => true,
        'show_inactive' => !empty($settings['show_inactive']), 'as_of' => $settings['as_of']);
}

/** Attach the control's sentinel and bounded presentation lifetime to a complete read. */
function hrm_employee_selector_snapshot_payload($settings, $expires, &$prepared_batch = null)
{
    $snapshot = hrm_employee_selector_snapshot($settings, $prepared_batch);
    if ($snapshot === false) return false;
    $snapshot['expires'] = $expires;
    $snapshot['special'] = $settings['spec_option'] === false ? null
        : array('id' => (string)$settings['spec_id'], 'text' => $settings['spec_option']);
    return $snapshot;
}
