<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * Employee appraisal DB helpers.
 */

/**
 * Ensure employee appraisal transaction type has numbering metadata.
 *
 * @return void
 */
function ensure_employee_appraisal_transaction_registration() {
    static $is_registered = false;

    if ($is_registered || !function_exists('register_systype_db_info'))
        return;

    register_systype_db_info(ST_EMPLOYEE_APPRAISAL, array(
        TB_PREF.'employee_appraisals',
        null,
        'appraisal_id',
        null,
        'appraisal_date'
    ));

    $is_registered = true;
}

/**
 * Convert either an SQL or current-locale appraisal date to SQL form.
 *
 * This accepts the localized dates stored in older approval drafts while new
 * drafts use canonical SQL dates. Invalid dates fail closed.
 *
 * @param string $date
 * @return string
 */
function appraisal_to_sql_date($date) {
    $date = trim((string)$date);
    if ($date === '')
        return '';

    if (preg_match('/^(\d{4})-(\d{2})-(\d{2})$/', $date, $matches)) {
        return checkdate((int)$matches[2], (int)$matches[3], (int)$matches[1])
            ? $date
            : '';
    }

    if (function_exists('is_date') && is_date($date)) {
        $sql_date = date2sql($date);
        return preg_match('/^\d{4}-\d{2}-\d{2}$/', (string)$sql_date)
            ? (string)$sql_date
            : '';
    }

    return '';
}

/**
 * Validate the persisted appraisal fields required by terminal transitions.
 *
 * @param array $appraisal
 * @return bool
 */
function employee_appraisal_row_is_valid($appraisal) {
    $required = array(
        'employee_id',
        'period_from',
        'period_to',
        'appraisal_date',
        'overall_score',
        'rating_scale',
    );
    if (!is_array($appraisal))
        return false;
    foreach ($required as $field) {
        if (!array_key_exists($field, $appraisal))
            return false;
    }

    if (trim((string)$appraisal['employee_id']) === ''
        || appraisal_to_sql_date($appraisal['period_from']) === ''
        || appraisal_to_sql_date($appraisal['period_to']) === ''
        || appraisal_to_sql_date($appraisal['appraisal_date']) === ''
        || appraisal_to_sql_date($appraisal['period_from'])
            > appraisal_to_sql_date($appraisal['period_to'])
        || (int)$appraisal['rating_scale'] <= 0
        || (float)$appraisal['overall_score'] < 0
        || (float)$appraisal['overall_score'] > (float)$appraisal['rating_scale']
    ) {
        return false;
    }

    return true;
}

/**
 * Add employee appraisal.
 *
 * @param array $data
 * @return int|false
 */
function add_employee_appraisal($data) {
    ensure_employee_appraisal_transaction_registration();

    $period_from = appraisal_to_sql_date(
        isset($data['period_from']) ? $data['period_from'] : ''
    );
    $period_to = appraisal_to_sql_date(
        isset($data['period_to']) ? $data['period_to'] : ''
    );
    $appraisal_date = appraisal_to_sql_date(
        isset($data['appraisal_date']) ? $data['appraisal_date'] : ''
    );
    $candidate = array(
        'employee_id' => isset($data['employee_id']) ? $data['employee_id'] : '',
        'period_from' => $period_from,
        'period_to' => $period_to,
        'appraisal_date' => $appraisal_date,
        'overall_score' => isset($data['overall_score']) ? $data['overall_score'] : -1,
        'rating_scale' => isset($data['rating_scale']) ? $data['rating_scale'] : 0,
    );
    $status = isset($data['status']) ? (int)$data['status'] : 1;

    if (!employee_appraisal_row_is_valid($candidate)
        || !in_array($status, array(0, 1), true)
    ) {
        return false;
    }

    $sql = "INSERT INTO ".TB_PREF."employee_appraisals
        (employee_id, reviewer_id, period_from, period_to, appraisal_date, overall_score, rating_scale, status, strengths, improvements, recommendation)
        VALUES (
            ".db_escape(isset($data['employee_id']) ? $data['employee_id'] : '').",
            ".db_escape(isset($data['reviewer_id']) ? $data['reviewer_id'] : '').",
            ".db_escape($period_from).",
            ".db_escape($period_to).",
            ".db_escape($appraisal_date).",
            ".db_escape(isset($data['overall_score']) ? (float)$data['overall_score'] : 0).",
            ".db_escape(isset($data['rating_scale']) ? (int)$data['rating_scale'] : 0).",
            ".db_escape($status).",
            ".db_escape(isset($data['strengths']) ? $data['strengths'] : '').",
            ".db_escape(isset($data['improvements']) ? $data['improvements'] : '').",
            ".db_escape(isset($data['recommendation']) ? $data['recommendation'] : '')."
        )";
    db_query($sql, 'could not add appraisal');

    $appraisal_id = (int)db_insert_id();
    return $appraisal_id > 0 ? $appraisal_id : false;
}

/**
 * Get one employee appraisal by id.
 *
 * @param int $appraisal_id
 * @return array|false
 */
function get_employee_appraisal($appraisal_id) {
    $sql = "SELECT ea.*,
            CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')) employee_name,
            CONCAT(COALESCE(r.first_name,''), ' ', COALESCE(r.last_name,'')) reviewer_name
        FROM ".TB_PREF."employee_appraisals ea
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = ea.employee_id
        LEFT JOIN ".TB_PREF."employees r ON r.employee_id = ea.reviewer_id
        WHERE ea.appraisal_id = ".db_escape((int)$appraisal_id);

    $res = db_query($sql, 'could not get appraisal');
    return db_fetch($res);
}

/**
 * Lock and return one employee appraisal row.
 *
 * @param int $appraisal_id
 * @return array|false
 */
function get_employee_appraisal_for_update($appraisal_id) {
    $sql = "SELECT * FROM ".TB_PREF."employee_appraisals
        WHERE appraisal_id = ".db_escape((int)$appraisal_id)."
        FOR UPDATE";
    $res = db_query($sql, 'could not lock appraisal');
    return db_fetch($res);
}

/**
 * Compare-and-set an appraisal status.
 *
 * @param int $appraisal_id
 * @param array $expected_statuses
 * @param int $status
 * @return bool
 */
function transition_employee_appraisal_status($appraisal_id, $expected_statuses, $status) {
    $allowed = array();
    foreach ((array)$expected_statuses as $expected_status) {
        $allowed[] = (int)$expected_status;
    }
    $allowed = array_values(array_unique($allowed));
    if (empty($allowed))
        return false;

    $sql = "UPDATE ".TB_PREF."employee_appraisals
        SET status = ".db_escape((int)$status)."
        WHERE appraisal_id = ".db_escape((int)$appraisal_id)."
            AND status IN (".implode(',', $allowed).")";
    db_query($sql, 'could not update appraisal status');

    return db_num_affected_rows() === 1;
}

/**
 * Update appraisal status for legacy callers.
 *
 * New approval paths must use transition_employee_appraisal_status() with an
 * explicit expected state.
 *
 * @param int $appraisal_id
 * @param int $status
 * @return bool
 */
function update_employee_appraisal_status($appraisal_id, $status) {
    $sql = "UPDATE ".TB_PREF."employee_appraisals
        SET status = ".db_escape((int)$status)."
        WHERE appraisal_id = ".db_escape((int)$appraisal_id);
    db_query($sql, 'could not update appraisal status');

    return db_num_affected_rows() > 0;
}

/**
 * Find the exact pending core approval draft for an appraisal.
 *
 * @param int $appraisal_id
 * @return array|false
 */
function find_approval_draft_for_employee_appraisal($appraisal_id) {
    $pending_status = defined('APPROVAL_STATUS_PENDING')
        ? APPROVAL_STATUS_PENDING
        : 0;
    $sql = "SELECT d.*, d.id AS draft_id
        FROM ".TB_PREF."approval_drafts d
        WHERE d.trans_type = ".db_escape((int)ST_EMPLOYEE_APPRAISAL)."
            AND d.status = ".db_escape((int)$pending_status)."
        ORDER BY d.id DESC";

    $res = db_query($sql, 'could not find appraisal approval draft');
    while ($row = db_fetch($res)) {
        $draft_source = html_entity_decode(
            (string)$row['draft_data'],
            ENT_QUOTES,
            isset($_SESSION['language']->encoding)
                ? $_SESSION['language']->encoding
                : 'UTF-8'
        );
        $draft_data = json_decode($draft_source, true);
        if (is_array($draft_data)
            && isset($draft_data['appraisal_id'])
            && (int)$draft_data['appraisal_id'] === (int)$appraisal_id
        ) {
            return $row;
        }
    }

    return false;
}

/**
 * Check if appraisal has a pending core approval draft.
 *
 * @param int $appraisal_id
 * @return bool
 */
function has_pending_employee_appraisal_approval($appraisal_id) {
    return find_approval_draft_for_employee_appraisal($appraisal_id) !== false;
}

/**
 * Build the complete canonical approval payload for an appraisal.
 *
 * @param int $appraisal_id
 * @return array|false
 */
function get_employee_appraisal_approval_data($appraisal_id) {
    $appraisal = get_employee_appraisal((int)$appraisal_id);
    if (!$appraisal || !employee_appraisal_row_is_valid($appraisal))
        return false;

    return array(
        'appraisal_id' => (int)$appraisal['appraisal_id'],
        'employee_id' => $appraisal['employee_id'],
        'employee_name' => trim((string)$appraisal['employee_name']),
        'reviewer_id' => isset($appraisal['reviewer_id']) ? $appraisal['reviewer_id'] : '',
        'reviewer_name' => trim((string)$appraisal['reviewer_name']),
        'period_from' => appraisal_to_sql_date($appraisal['period_from']),
        'period_to' => appraisal_to_sql_date($appraisal['period_to']),
        'appraisal_date' => appraisal_to_sql_date($appraisal['appraisal_date']),
        'overall_score' => (float)$appraisal['overall_score'],
        'rating_scale' => (int)$appraisal['rating_scale'],
        'strengths' => isset($appraisal['strengths']) ? $appraisal['strengths'] : '',
        'improvements' => isset($appraisal['improvements']) ? $appraisal['improvements'] : '',
        'recommendation' => isset($appraisal['recommendation']) ? $appraisal['recommendation'] : '',
    );
}

/**
 * Submit an appraisal through the registered core approval workflow.
 *
 * Direct-policy approval uses the same guarded audited executor with draft id
 * zero. A missing registration or unexpected source state fails closed.
 *
 * @param int $appraisal_id
 * @return array|false
 */
function submit_employee_appraisal_for_core_approval($appraisal_id) {
    global $path_to_root;

    $appraisal = get_employee_appraisal((int)$appraisal_id);
    if (!$appraisal || (int)$appraisal['status'] !== 1)
        return false;

    if (has_pending_employee_appraisal_approval((int)$appraisal_id))
        return false;

    if (class_exists('ApprovalWorkflow')
        && !ApprovalWorkflow::isRegistered(ST_EMPLOYEE_APPRAISAL)
        && isset($path_to_root)
        && $path_to_root !== ''
    ) {
        include_once($path_to_root.'/includes/approval/registrations/hrm_approval.inc');
    }

    if (!class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_EMPLOYEE_APPRAISAL)
    ) {
        return false;
    }

    $draft_data = get_employee_appraisal_approval_data((int)$appraisal_id);
    if ($draft_data === false)
        return false;

    $approval = get_approval_workflow_service();
    $approval_amount = (float)$appraisal['overall_score'];
    if (!$approval->isApprovalRequired(ST_EMPLOYEE_APPRAISAL, $approval_amount)) {
        if (!$approval->canDirectPost(ST_EMPLOYEE_APPRAISAL))
            return false;

        return execute_approved_employee_appraisal($draft_data, 0, 0) === false
            ? false
            : array(
                'status' => 'auto_approved',
                'appraisal_id' => (int)$appraisal_id,
            );
    }

    return $approval->submit(
        ST_EMPLOYEE_APPRAISAL,
        $draft_data,
        $approval_amount,
        array(
            'summary' => sprintf(
                _('Employee Appraisal #%d: %s'),
                (int)$appraisal_id,
                $appraisal['employee_id']
            ),
            'person_type' => PT_EMPLOYEE,
            'person_id' => $appraisal['employee_id'],
            'loc_code' => '',
        )
    );
}

/**
 * Get employee appraisals.
 *
 * @return resource
 */
function get_employee_appraisals() {
    $sql = "SELECT ea.*,
            CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')) employee_name,
            CONCAT(COALESCE(r.first_name,''), ' ', COALESCE(r.last_name,'')) reviewer_name
        FROM ".TB_PREF."employee_appraisals ea
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = ea.employee_id
        LEFT JOIN ".TB_PREF."employees r ON r.employee_id = ea.reviewer_id
        ORDER BY ea.appraisal_date DESC, ea.appraisal_id DESC";

    return db_query($sql, 'could not get appraisals');
}
