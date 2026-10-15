<?php
/** Session-bound employee search controls using the existing combo and Select2 UI. */
include_once($path_to_root.'/hrm/includes/db/employee_selector_search_db.inc');
include_once($path_to_root.'/reporting/includes/report_artifact_security.inc');
include_once($path_to_root.'/hrm/includes/ui/employee_selector_snapshot.inc');

/** Refresh the existing role representation after a current database permission check. */
function hrm_employee_selector_refresh_access($area)
{
    if (!report_artifact_has_fresh_permission($area)) return false;
    $role = get_security_role($_SESSION['wa_current_user']->access);
    if (!$role || !empty($role['inactive'])) return false;
    $_SESSION['wa_current_user']->role_set = array();
    foreach ($role['areas'] as $code)
        if (in_array($code & ~0xff, $role['sections'])) $_SESSION['wa_current_user']->role_set[] = $code;
    return user_check_access($area);
}

/** Compute only an internal continuation revision; this never substitutes for the full guard. */
function hrm_employee_selector_page_revision()
{
    $fingerprint = hrm_employee_projection_state_fingerprint();
    if ($fingerprint === false) return false;
    $result = db_query('SELECT department_id,department_name FROM '.TB_PREF.'departments ORDER BY department_id');
    if (!$result) return false;
    $digest = hash_init('sha256');
    $user = get_user(hrm_person_worker_actor_id());
    $role = $user ? get_security_role($user['role_id']) : false;
    if (!$user || !$role || !empty($user['inactive']) || !empty($role['inactive'])) return false;
    hash_update($digest, $fingerprint.serialize(array($user['role_id'], $role['areas'], $role['sections'])));
    while ($row = db_fetch_assoc($result)) hash_update($digest, serialize($row));
    db_free_result($result);
    return hash_final($digest);
}

/** Search a stable observed cohort after the caller's full guard; reject changed continuation. */
function hrm_employee_selector_read_page($settings, $term, $after, $selected, $revision = null, $check_selected = false)
{
    if ($selected !== null && !$check_selected) return hrm_employee_selector_search($settings, $term, $after, $selected);
    $roles = $_SESSION['wa_current_user']->role_set;
    $before = hrm_employee_selector_page_revision();
    if ($before === false) return false;
    if ($revision !== null && $revision !== $before) return array('changed' => true);
    $found = hrm_employee_selector_search($settings, $term, $after, $selected);
    if ($found === false) return false;
    if ($before !== hrm_employee_selector_page_revision()
        || !hrm_employee_selector_refresh_access($settings['area'])
        || $roles !== $_SESSION['wa_current_user']->role_set) return array('changed' => true);
    $found['revision'] = $before;
    return $found;
}

/**
 * Build and retain one server-owned selector contract for this rendered control.
 * No SQL, actor, company, permission or instant is accepted from the client.
 * @param string $name
 * @param array $options
 * @param bool $manager
 * @return string Opaque grant, empty when the page cannot issue a control.
 */
function hrm_employee_selector_register($name, $options, $manager)
{
    global $page_security, $db_connections;
    $company = hrm_person_worker_current_company_id();
    $area = $page_security;
    if ($area === 'SA_OPEN' && !empty($options['employee_selector_security']))
        $area = $options['employee_selector_security'];
    if ($area === 'SA_OPEN' || $area === 'SA_DENIED' || !user_check_access($area)
        || $company === false || hrm_person_worker_actor_id() <= 0
        || !hrm_employee_selector_refresh_access($area))
        return '';
    $where = isset($options['where']) ? $options['where'] : array();
    if (!is_array($where)) $where = array($where);
    $settings = array('where' => $where, 'manager' => $manager,
        'show_inactive' => !empty($options['show_inactive']),
        'canonical' => isset($options['employee_name_batch']),
        'as_of' => isset($options['employee_name_batch']['as_of']) ? $options['employee_name_batch']['as_of'] : false,
        'codes' => user_show_codes(), 'area' => $area, 'actor' => hrm_person_worker_actor_id(),
        'company' => $company, 'database' => $db_connections[$company]['dbname'],
        'prefix' => $db_connections[$company]['tbpref'], 'control' => $name,
        'spec_option' => isset($options['spec_option']) ? $options['spec_option'] : false,
        'spec_id' => isset($options['spec_id']) ? (string)$options['spec_id'] : '');
    if (!isset($_SESSION['hrm_employee_selectors'])) $_SESSION['hrm_employee_selectors'] = array();
    foreach ($_SESSION['hrm_employee_selectors'] as $key => $grant)
        if ($grant['expires'] < time()) unset($_SESSION['hrm_employee_selectors'][$key]);
    foreach (array('hrm_employee_selector_pages', 'hrm_employee_selector_cursors') as $store) {
        if (!isset($_SESSION[$store])) continue;
        foreach ($_SESSION[$store] as $key => $value) {
            $owner = $store === 'hrm_employee_selector_pages' ? $key : $value['control'];
            if (!isset($_SESSION['hrm_employee_selectors'][$owner])) unset($_SESSION[$store][$key]);
        }
    }
    // Reuse identical controls on rerenders; retain independent browser tabs.
    foreach ($_SESSION['hrm_employee_selectors'] as $key => $grant)
        if ($grant['settings'] === $settings) {
            $_SESSION['hrm_employee_selectors'][$key]['expires'] = time()+1800;
            return $key;
        }
    if (count($_SESSION['hrm_employee_selectors']) >= 100) {
        reset($_SESSION['hrm_employee_selectors']); $oldest = key($_SESSION['hrm_employee_selectors']);
        unset($_SESSION['hrm_employee_selectors'][$oldest], $_SESSION['hrm_employee_selector_pages'][$oldest]);
    }
    $key = random_id();
    $_SESSION['hrm_employee_selectors'][$key] = array('settings' => $settings, 'expires' => time()+1800);
    return $key;
}

/**
 * Render a complete presentation snapshot plus a bounded native/fallback select.
 * Original callback, batch extra_refs, special options and command buttons stay
 * with combo_input; async search has its own authorized endpoint and never posts
 * a business command. Fallback continuation replaces one page of 50 results.
 * @param string $name
 * @param mixed $selected
 * @param string $sql Retained original contract for other combo callers.
 * @param array $options
 * @param string|null $type
 * @param bool $manager
 * @return string
 */
function hrm_employee_selector_control($name, $selected, $sql, $options, $type = null, $manager = false)
{
    global $path_to_root, $Ajax;
    if ($selected === null) $selected = get_post($name, '');
    $selected = is_array($selected) ? $selected : array((string)$selected);
    $options['search_box'] = true;
    $options['search_submit'] = true;
    if (!isset($options['max'])) $options['max'] = 255;
    if (!isset($options['spec_option']) || $options['spec_option'] === false)
        $options['spec_option'] = _('Select employee');
    $token = hrm_employee_selector_register($name, $options, $manager);
    if ($token === '') return '';
    $settings = $_SESSION['hrm_employee_selectors'][$token]['settings'];
    $selection_refs = array_values(array_diff($selected, array($settings['spec_id'])));
    $continuation_valid = false;
    if (isset($_POST['_'.$name.'_more']) && is_string($_POST['_'.$name.'_more'])) {
        $prior_token = $_POST['_'.$name.'_more'];
        $grant = isset($_SESSION['hrm_employee_selectors'][$prior_token]) ? $_SESSION['hrm_employee_selectors'][$prior_token] : false;
        if ($grant && $grant['expires'] >= time()) {
            $current_contract = $settings; $prior_contract = $grant['settings'];
            unset($current_contract['as_of'], $prior_contract['as_of']);
            if ($current_contract === $prior_contract) {
                $token = $prior_token; $settings = $grant['settings']; $continuation_valid = true;
            }
        }
    }
    $searching = isset($_POST['_'.$name.'_button']) || isset($_POST['_'.$name.'_more']);
    $term = get_post('_'.$name.'_edit', '');
    $after = null;
    $revision = null;
    if (isset($_POST['_'.$name.'_more'], $_SESSION['hrm_employee_selector_pages'][$token])) {
        $prior = $_SESSION['hrm_employee_selector_pages'][$token];
        if (is_string($term) && $prior['term'] === $term && $prior['after'] !== null) {
            $after = $prior['after']; $revision = $prior['revision'];
        } else $continuation_valid = false;
    }
    if (isset($_POST['_'.$name.'_more']) && (!$continuation_valid || $after === null)) {
        display_error(_('Employee search expired or changed. Search again to refresh the choices.'));
        $searching = false;
    }
    $found = hrm_employee_selector_read_page($settings, $searching ? $term : '', $after,
        $searching ? null : $selection_refs, $revision);
    if ($found === false && $searching) {
        display_error(_('Employee search is unavailable or invalid. Search again.'));
        $searching = false;
        $found = hrm_employee_selector_read_page($settings, '', null, $selection_refs);
    }
    if (is_array($found) && !empty($found['changed'])) {
        display_error(_('Employees changed. Search again to refresh the choices.'));
        $searching = false;
        $found = hrm_employee_selector_read_page($settings, '', null, $selection_refs);
    }
    $rows = $found === false ? array() : $found['rows'];
    if ($searching && $found !== false) {
        // Exact codes remain selectable even when their fragment matches more
        // than a page of other IDs. Hydration preserves each control's cohort.
        $exact_ids = array(trim($term));
        // The old form matcher also accepted the complete formatted label.
        // Consider at most 50 exact code candidates, then require exact label
        // equality; a pasted prefix never authorizes an unrelated selection.
        $separator = $manager ? ' [' : ' - ';
        $offset = 0;
        while (count($exact_ids) < 50 && ($position = strpos($term, $separator, $offset)) !== false) {
            $candidate = $manager && substr($term, -1) === ']'
                ? substr($term, $position+2, -1) : (!$manager ? substr($term, 0, $position) : '');
            if ($candidate !== '' && strlen($candidate) <= 255 && !in_array($candidate, $exact_ids, true)) $exact_ids[] = $candidate;
            $offset = $position+strlen($separator);
        }
        // A full label may be longer than the employee-code hydration bound.
        $exact_ids = array_values(array_filter($exact_ids, function($id) { return strlen($id) <= 255; }));
        $exact = hrm_employee_selector_search($settings, '', null, $exact_ids);
        $exact_selected = false;
        if ($exact !== false) foreach ($exact['rows'] as $row) {
            if (trim($term) === (string)$row['employee_id']) {
                $selected = array((string)$row['employee_id']); $exact_selected = true; break;
            }
        }
        if ($exact !== false && !$exact_selected) foreach ($exact['rows'] as $row) {
            if (trim($term) === hrm_employee_selector_result_label($row, $settings, $exact['batch'])) {
                $selected = array((string)$row['employee_id']); $exact_selected = true; break;
            }
        }
        foreach ($rows as $row) {
            if ($exact_selected) break;
            $label = hrm_employee_selector_result_label($row, $settings, $found['batch']);
            if (trim($term) === (string)$row['employee_id'] || trim($term) === $row['name'] || trim($term) === $label) {
                $selected = array((string)$row['employee_id']); break;
            }
        }
        $selected_rows = hrm_employee_selector_search($settings, '', null,
            array_values(array_diff($selected, array($settings['spec_id']))));
        if ($selected_rows !== false) {
            foreach ($selected_rows['rows'] as $row) {
                $present = false;
                foreach ($rows as $existing) if ($existing['employee_id'] === $row['employee_id']) { $present = true; break; }
                if (!$present) $rows[] = $row;
            }
            if (is_array($selected_rows['batch'])) {
                if (!is_array($found['batch'])) $found['batch'] = $selected_rows['batch'];
                else $found['batch']['names'] = array_merge($found['batch']['names'], $selected_rows['batch']['names']);
            }
        }
    }
    foreach ($rows as &$row) {
        $row[1] = $row['name'] = $row['sort_name'].($manager ? ' ['.$row['employee_id'].']' : '');
    }
    unset($row);
    $snapshot = hrm_employee_selector_snapshot_payload($settings,
        $_SESSION['hrm_employee_selectors'][$token]['expires'], $snapshot_batch);
    $options['employee_selector_snapshot'] = $snapshot === false ? array('complete' => false) : $snapshot;
    $options['employee_selector_rows'] = $rows;
    $options['employee_selector_token'] = $token;
    $options['employee_selector_url'] = $path_to_root.'/hrm/employee_selector_search.php';
    $options['order'] = false;
    $options['employee_selector_preserve_selection'] = true;
    if ($searching) {
        $options['employee_selector_term'] = $term;
        $options['async'] = false;
        $Ajax->activate('_page_body');
    }
    $options['box_hint'] = _('Search employee code, name or department; * lists all choices');
    // A fallback continuation retains its search instant. History/roster batches
    // keep the caller's fresh instant and complete extra_refs independently.
    $selector_batch = $found !== false ? $found['batch'] : false;
    $options['format'] = function($row) use ($settings, $selector_batch) {
        return hrm_employee_selector_result_label($row, $settings, $selector_batch);
    };
    $options['data_format'] = $options['format'];
    if (!empty($settings['canonical']) && $found !== false)
        $GLOBALS[$options['employee_name_batch']['target']] = $found['batch'];
    if (!$searching && is_array($snapshot_batch) && isset($options['employee_name_batch']))
        $GLOBALS[$options['employee_name_batch']['target']] = $snapshot_batch;
    if (isset($_POST['_'.$name.'_more'])) $Ajax->activate($name);
    $html = combo_input($name, $selected, $sql, 'employee_id', 'name', $options, $type);
    $html .= '<noscript><style>[data-employee-fallback]{display:inline-block!important}</style></noscript>';
    if ($searching && $found !== false) {
        $_SESSION['hrm_employee_selector_pages'][$token] = array('term' => $term, 'after' => $found['after'], 'revision' => $found['revision']);
        if ($found['more'])
            $html .= "<button type='submit' aspect='fallback' name='_".htmlspecialchars($name, ENT_QUOTES, 'UTF-8')
                ."_more' value='".htmlspecialchars($token, ENT_QUOTES, 'UTF-8')."'>"._('More employees').'</button>';
    }
    return $html;
}
