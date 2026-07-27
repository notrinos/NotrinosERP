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
 * Check whether table exists in current company schema.
 *
 * @param string $table_name
 * @return bool
 */
function payroll_table_exists($table_name) {
    static $cache = array();

    if (isset($cache[$table_name]))
        return $cache[$table_name];

    $sql = "SHOW TABLES LIKE '".TB_PREF.$table_name."'";
    $result = db_query($sql);
    if ($result && db_num_rows($result) > 0) {
        $cache[$table_name] = true;
        return true;
    }

    // Payroll core tables are expected in this module; allow a safe fallback
    // when metadata lookups fail inside long HRM posting flows.
    if (in_array($table_name, array('payroll_periods', 'payslips'), true)) {
        $cache[$table_name] = true;
        return true;
    }

    $cache[$table_name] = false;
    return false;
}

/**
 * Ensure payroll_periods table exists for the active company schema.
 *
 * @return bool
 */
function ensure_payroll_periods_table() {
    if (payroll_table_exists('payroll_periods'))
        return true;

    $sql = "CREATE TABLE IF NOT EXISTS ".TB_PREF."payroll_periods (
        period_id int(11) NOT NULL AUTO_INCREMENT,
        period_name varchar(60) NOT NULL,
        from_date date NOT NULL,
        to_date date NOT NULL,
        pay_date date DEFAULT NULL,
        department_id int(11) DEFAULT NULL,
        status tinyint(1) NOT NULL DEFAULT 0,
        total_gross double NOT NULL DEFAULT 0,
        total_deductions double NOT NULL DEFAULT 0,
        total_net double NOT NULL DEFAULT 0,
        total_employer_cost double NOT NULL DEFAULT 0,
        approved_by varchar(20) DEFAULT NULL,
        approval_date datetime DEFAULT NULL,
        gl_trans_no int(11) DEFAULT NULL,
        created_by smallint(6) DEFAULT NULL,
        created_date timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
        notes text,
        PRIMARY KEY (period_id),
        KEY from_date (from_date),
        KEY status (status)
    ) ENGINE=InnoDB";

    $db_ok = db_query($sql, 'could not create payroll_periods table');
    return (bool)$db_ok;
}

/**
 * Convert user/system date to SQL date format.
 *
 * @param string $date_value
 * @return string
 */
function payroll_to_sql_date($date_value) {
    $date_value = trim((string)$date_value);
    if ($date_value === '')
        return '';

    if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_value))
        return $date_value;

    if (is_date($date_value))
        return date2sql($date_value);

    return $date_value;
}

/**
 * Resolve payslip header table for totals aggregation.
 *
 * @return string|false
 */
function payroll_payslip_header_table() {
    if (payroll_table_exists('payslips'))
        return 'payslips';

    return false;
}

/**
 * Check whether a linked payslip column exists.
 *
 * @param string $table_name Table name without prefix.
 * @param string $column_name
 * @return bool
 */
function payroll_payslip_has_column($table_name, $column_name) {
    return check_empty_result(
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE()"
        ." AND TABLE_NAME = ".db_escape(TB_PREF.$table_name)
        ." AND COLUMN_NAME = ".db_escape($column_name)
    );
}

/**
 * Add payroll period.
 *
 * @param string $period_name
 * @param string $from_date
 * @param string $to_date
 * @param int|null $department_id
 * @return int|false
 */
function add_payroll_period($period_name, $from_date, $to_date, $department_id=null) {
    if (!ensure_payroll_periods_table())
        return false;

    $period_name = trim((string)$period_name);
    $from_sql = payroll_to_sql_date($from_date);
    $to_sql = payroll_to_sql_date($to_date);

    if ($period_name === '' || $from_sql === '' || $to_sql === '')
        return false;
    if ($from_sql > $to_sql)
        return false;

    $created_by = null;
    if (isset($_SESSION['wa_current_user'])) {
        $user = $_SESSION['wa_current_user'];
        if (isset($user->user) && is_numeric($user->user))
            $created_by = (int)$user->user;
        elseif (isset($user->id) && is_numeric($user->id))
            $created_by = (int)$user->id;
    }

    $fields = array('period_name', 'from_date', 'to_date');
    $values = array(db_escape($period_name), db_escape($from_sql), db_escape($to_sql));

    if ($department_id !== null && $department_id !== '') {
        $fields[] = 'department_id';
        $values[] = db_escape((int)$department_id);
    }

    if ($created_by !== null) {
        $fields[] = 'created_by';
        $values[] = db_escape($created_by);
    }

    $sql = "INSERT INTO ".TB_PREF."payroll_periods (".implode(', ', $fields).") VALUES (".implode(', ', $values).")";
    $db_ok = db_query($sql, 'could not add payroll period');
    if (!$db_ok)
        return false;

    $period_id = (int)db_insert_id();
    if ($period_id <= 0) {
        $lookup = "SELECT period_id FROM ".TB_PREF."payroll_periods WHERE "
            ."period_name = ".db_escape($period_name)
            ." AND from_date = ".db_escape($from_sql)
            ." AND to_date = ".db_escape($to_sql);

        if ($department_id !== null && $department_id !== '')
            $lookup .= " AND department_id = ".db_escape((int)$department_id);
        else
            $lookup .= " AND department_id IS NULL";

        $lookup .= " ORDER BY period_id DESC LIMIT 1";
        $lookup_result = db_query($lookup, 'could not resolve payroll period id');
        $lookup_row = $lookup_result ? db_fetch_row($lookup_result) : false;
        if ($lookup_row)
            $period_id = (int)$lookup_row[0];
    }

    return $period_id > 0 ? $period_id : false;
}

/**
 * Get one payroll period by id FOR UPDATE (row lock).
 *
 * @param int $period_id
 * @return array|false
 */
function get_payroll_period_for_update($period_id) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $sql = "SELECT * FROM ".TB_PREF."payroll_periods
        WHERE period_id = ".db_escape((int)$period_id)." FOR UPDATE";
    $result = db_query($sql, 'could not get payroll period for update');

    return db_fetch($result);
}

/**
 * Get one payroll period by id.
 *
 * @param int $period_id
 * @return array|false
 */
function get_payroll_period($period_id) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $sql = "SELECT * FROM ".TB_PREF."payroll_periods WHERE period_id = ".db_escape((int)$period_id);
    $result = db_query($sql, 'could not get payroll period');

    return db_fetch($result);
}

/**
 * Resolve the current numeric actor for PAY-CORE-011 containment logging.
 *
 * @return int
 */
function payroll_side_effect_control_actor_id() {
    if (!isset($_SESSION['wa_current_user']))
        return 0;

    $user = $_SESSION['wa_current_user'];
    if (isset($user->user) && is_numeric($user->user))
        return (int)$user->user;
    if (isset($user->id) && is_numeric($user->id))
        return (int)$user->id;

    return 0;
}

/**
 * Record a minimized denial event for an unsafe legacy void/reopen attempt.
 *
 * @param string $action
 * @param string $code
 * @param int $period_id
 * @param int $payslip_id
 * @param string $channel
 * @return void
 */
function payroll_side_effect_control_log_denial(
    $action,
    $code,
    $period_id=0,
    $payslip_id=0,
    $channel='hrm_db'
) {
    $payload = array(
        'contract' => 'notrinoserp.hrm.payroll.side-effect-control',
        'version' => '1.0.0',
        'action' => preg_replace('/[^a-z0-9_\-]/', '', strtolower((string)$action)),
        'code' => preg_replace('/[^a-z0-9_\-]/', '', strtolower((string)$code)),
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'actor_id' => payroll_side_effect_control_actor_id(),
        'payroll_period_id' => (int)$period_id,
        'payslip_id' => (int)$payslip_id,
        'channel' => preg_replace('/[^a-z0-9_\-]/', '', strtolower((string)$channel)),
    );

    error_log('PAY-CORE-011 '.json_encode($payload));
}

/**
 * Fetch one linked financial row and optionally lock it.
 *
 * @param string $sql
 * @param string $error
 * @return bool|null True when a row exists, false when none exists, null on query failure.
 */
function payroll_side_effect_row_exists($sql, $error) {
    $result = db_query($sql, $error);
    if (!$result)
        return null;

    return db_fetch_assoc($result) ? true : false;
}

/**
 * Inspect one locked payslip for any payroll-GL or payment side-effect link.
 *
 * Positive legacy transaction links are themselves treated as protected
 * evidence because partial legacy writes can leave references/comments/audit
 * or external bank effects even when a later row lookup is incomplete.
 * Exact GL, bank and core-void identities are also read (and locked when
 * requested) to serialize this guard with posting, payment and generic voids.
 *
 * @param array $payslip
 * @param bool $lock
 * @return array
 */
function payroll_payslip_financial_side_effects($payslip, $lock=true) {
    $result = array(
        'safe' => false,
        'code' => 'invalid_payslip',
        'payslip_id' => 0,
        'payroll_period_id' => 0,
        'evidence' => array(),
    );
    if (!is_array($payslip) || empty($payslip['payslip_id']))
        return $result;

    $result['payslip_id'] = (int)$payslip['payslip_id'];
    $result['payroll_period_id'] = isset($payslip['payroll_period_id'])
        ? (int)$payslip['payroll_period_id'] : 0;
    $lock_sql = $lock ? ' FOR UPDATE' : '';

    $payslip_status = isset($payslip['status']) ? (int)$payslip['status'] : 0;
    if ($payslip_status >= 2 && $payslip_status < 6)
        $result['evidence'][] = 'payslip_status_financial';

    $payroll_trans_no = isset($payslip['gl_trans_no'])
        ? (int)$payslip['gl_trans_no'] : 0;
    if ($payroll_trans_no <= 0 && isset($payslip['trans_no']))
        $payroll_trans_no = (int)$payslip['trans_no'];

    if ($payroll_trans_no > 0) {
        $result['evidence'][] = 'payroll_gl_link';
        $queries = array(
            array(
                "SELECT counter FROM ".TB_PREF."gl_trans WHERE type = ".db_escape(ST_PAYSLIP)
                    ." AND type_no = ".db_escape($payroll_trans_no).$lock_sql,
                'could not inspect linked payroll GL rows'
            ),
            array(
                "SELECT type, id FROM ".TB_PREF."voided WHERE type = ".db_escape(ST_PAYSLIP)
                    ." AND id = ".db_escape($payroll_trans_no).$lock_sql,
                'could not inspect linked payroll void evidence'
            ),
        );
        foreach ($queries as $query) {
            $exists = payroll_side_effect_row_exists($query[0], $query[1]);
            if ($exists === null) {
                $result['code'] = 'payroll_gl_evidence_unavailable';
                return $result;
            }
            if ($exists)
                $result['evidence'][] = strpos($query[0], 'gl_trans') !== false
                    ? 'payroll_gl_rows' : 'payroll_void_record';
        }
    }

    $payment_trans_no = isset($payslip['payment_trans_no'])
        ? (int)$payslip['payment_trans_no'] : 0;
    if ($payment_trans_no > 0) {
        $result['evidence'][] = 'payment_link';
        $queries = array(
            array(
                "SELECT id FROM ".TB_PREF."bank_trans WHERE type = ".db_escape(ST_BANKPAYMENT)
                    ." AND trans_no = ".db_escape($payment_trans_no).$lock_sql,
                'could not inspect linked payroll bank rows',
                'payment_bank_rows'
            ),
            array(
                "SELECT counter FROM ".TB_PREF."gl_trans WHERE type = ".db_escape(ST_BANKPAYMENT)
                    ." AND type_no = ".db_escape($payment_trans_no).$lock_sql,
                'could not inspect linked payroll payment GL rows',
                'payment_gl_rows'
            ),
            array(
                "SELECT type, id FROM ".TB_PREF."voided WHERE type = ".db_escape(ST_BANKPAYMENT)
                    ." AND id = ".db_escape($payment_trans_no).$lock_sql,
                'could not inspect linked payroll payment void evidence',
                'payment_void_record'
            ),
        );
        foreach ($queries as $query) {
            $exists = payroll_side_effect_row_exists($query[0], $query[1]);
            if ($exists === null) {
                $result['code'] = 'payment_evidence_unavailable';
                return $result;
            }
            if ($exists)
                $result['evidence'][] = $query[2];
        }
    }

    if (!empty($result['evidence'])) {
        if (in_array('payment_link', $result['evidence'], true))
            $result['code'] = 'linked_payment_side_effect';
        elseif (in_array('payroll_gl_link', $result['evidence'], true))
            $result['code'] = 'linked_payroll_gl_side_effect';
        else
            $result['code'] = 'payslip_status_financial';
        return $result;
    }

    $result['safe'] = true;
    $result['code'] = 'side_effect_free';
    return $result;
}

/**
 * Inspect one locked payroll period and every linked payslip for effects.
 *
 * @param array $period
 * @param bool $lock
 * @return array
 */
function payroll_period_financial_side_effects($period, $lock=true) {
    $result = array(
        'safe' => false,
        'code' => 'invalid_payroll_period',
        'payroll_period_id' => 0,
        'payslip_id' => 0,
        'payslip_count' => 0,
        'evidence' => array(),
    );
    if (!is_array($period) || empty($period['period_id']))
        return $result;

    $period_id = (int)$period['period_id'];
    $result['payroll_period_id'] = $period_id;
    $period_status = isset($period['status']) ? (int)$period['status'] : 0;
    if ($period_status >= 3 && $period_status < 6) {
        $result['code'] = 'period_status_financial';
        $result['evidence'][] = 'period_status_financial';
        return $result;
    }
    if (!empty($period['gl_trans_no'])) {
        $result['code'] = 'linked_period_gl_side_effect';
        $result['evidence'][] = 'period_gl_link';
        return $result;
    }

    $payslip_table = payroll_payslip_header_table();
    if (!$payslip_table) {
        $result['code'] = 'payslip_repository_unavailable';
        return $result;
    }

    $sql = "SELECT * FROM ".TB_PREF.$payslip_table
        ." WHERE payroll_period_id = ".db_escape($period_id)
        .($lock ? ' FOR UPDATE' : '');
    $payslips = db_query($sql, 'could not inspect linked payroll-period payslips');
    if (!$payslips) {
        $result['code'] = 'payslip_evidence_unavailable';
        return $result;
    }

    while ($payslip = db_fetch_assoc($payslips)) {
        $result['payslip_count']++;
        $payslip_result = payroll_payslip_financial_side_effects($payslip, $lock);
        if (!$payslip_result['safe']) {
            $result['code'] = $payslip_result['code'];
            $result['payslip_id'] = $payslip_result['payslip_id'];
            $result['evidence'] = $payslip_result['evidence'];
            return $result;
        }
    }

    $result['safe'] = true;
    $result['code'] = 'side_effect_free';
    return $result;
}

/**
 * Append immutable evidence for an allowed side-effect-free legacy transition.
 *
 * @param string $entity_type
 * @param array $entity_key
 * @param string $action
 * @param string $reason_code
 * @param array $before
 * @param array $after
 * @param array $changed_fields
 * @param string $mutation_kind
 * @param array $related_changes Minimized non-sensitive linked-row metadata.
 * @return bool
 */
function payroll_append_side_effect_free_transition_audit(
    $entity_type,
    $entity_key,
    $action,
    $reason_code,
    $before,
    $after,
    $changed_fields,
    $mutation_kind,
    $related_changes=array()
) {
    global $path_to_root;

    include_once($path_to_root.'/includes/db/domain_audit_db.inc');
    if (!function_exists('append_domain_audit_event'))
        return false;

    // This transition changes lifecycle flags only. Do not duplicate pay,
    // employee, bank, reference, note, or other restricted values into audit.
    $audit_fields = array_unique(array_merge(array_keys($entity_key), $changed_fields));
    $before_audit = array();
    $after_audit = array();
    foreach ($audit_fields as $field) {
        $before_audit[$field] = is_array($before) && array_key_exists($field, $before)
            ? $before[$field] : null;
        $after_audit[$field] = is_array($after) && array_key_exists($field, $after)
            ? $after[$field] : null;
    }

    $restricted_diff = array(
        'changed_fields' => array_values($changed_fields),
        'mutation_kind' => $mutation_kind,
        'financial_side_effects_present' => false,
        'restricted_values_stored' => false,
    );
    if (!empty($related_changes))
        $restricted_diff['related_changes'] = $related_changes;

    $audit = append_domain_audit_event(array(
        'legal_entity_id' => 0,
        'entity_type' => $entity_type,
        'entity_key' => $entity_key,
        'action' => $action,
        'reason_code' => $reason_code,
        'before' => $before_audit,
        'after' => $after_audit,
        'restricted_diff' => $restricted_diff,
    ));

    return $audit !== false;
}

/**
 * Reopen an Approved/Posted payroll period only when no linked effects exist.
 *
 * @param int $period_id
 * @return array
 */
function reopen_payroll_period_without_financial_side_effects($period_id) {
    $period_id = (int)$period_id;
    $result = array(
        'ok' => false,
        'code' => 'invalid_payroll_period',
        'message' => _('The payroll period could not be reopened.'),
    );
    if ($period_id <= 0)
        return $result;

    begin_transaction();
    $period = get_payroll_period_for_update($period_id);
    if (!$period) {
        cancel_transaction();
        $result['code'] = 'payroll_period_not_found';
        return $result;
    }

    $status = isset($period['status']) ? (int)$period['status'] : -1;
    if (!in_array($status, array(2, 3), true)) {
        cancel_transaction();
        $result['code'] = 'payroll_period_status_not_reopenable';
        $result['message'] = _('Only Approved or Posted payroll periods can use this guarded reopen action.');
        return $result;
    }

    $side_effects = payroll_period_financial_side_effects($period, true);
    if (!$side_effects['safe']) {
        payroll_side_effect_control_log_denial(
            'payroll_period_reopen',
            $side_effects['code'],
            $period_id,
            isset($side_effects['payslip_id']) ? $side_effects['payslip_id'] : 0,
            'payroll_approval'
        );
        cancel_transaction();
        $result['code'] = $side_effects['code'];
        $result['message'] = _(
            'This payroll period has linked payroll or payment effects and cannot be reopened. '
            .'Use a linked reversal after that workflow is implemented.'
        );
        return $result;
    }

    $sql = "UPDATE ".TB_PREF."payroll_periods SET status = 0,"
        ." approved_by = NULL, approval_date = NULL"
        ." WHERE period_id = ".db_escape($period_id)
        ." AND status = ".db_escape($status);
    if (!db_query($sql, 'could not reopen side-effect-free payroll period')
        || db_num_affected_rows() !== 1
    ) {
        cancel_transaction();
        $result['code'] = 'payroll_period_transition_conflict';
        return $result;
    }
    $after = get_payroll_period_for_update($period_id);
    if (!$after || (int)$after['status'] !== 0) {
        cancel_transaction();
        $result['code'] = 'payroll_period_transition_not_persisted';
        return $result;
    }

    if (!payroll_append_side_effect_free_transition_audit(
        'payroll_period',
        array('period_id' => $period_id),
        'payroll_period.reopened',
        'legacy_side_effect_free_reopen',
        $period,
        $after,
        array('status', 'approved_by', 'approval_date'),
        'reopened'
    )) {
        error_log('PAY-AUD-001: payroll-period side-effect-free reopen audit append failed');
        cancel_transaction();
        $result['code'] = 'audit_append_failed';
        return $result;
    }

    commit_transaction();
    return array(
        'ok' => true,
        'code' => 'payroll_period_reopened',
        'message' => _('Payroll period has been reopened as Draft.'),
    );
}

/**
 * Count payroll periods optionally filtered by status.
 *
 * @param int|null $status
 * @return int
 */
function count_payroll_periods($status=null) {
    if (!payroll_table_exists('payroll_periods'))
        return 0;

    $sql = "SELECT COUNT(*) FROM ".TB_PREF."payroll_periods";
    if ($status !== null && $status !== '')
        $sql .= " WHERE status = ".db_escape((int)$status);

    $result = db_query($sql, 'could not count payroll periods');
    $row = db_fetch_row($result);
    return $row ? (int)$row[0] : 0;
}

/**
 * Get payroll periods optionally filtered by status.
 *
 * @param int|null $status
 * @param int|null $limit
 * @param int $offset
 * @return resource|false
 */
function get_payroll_periods($status=null, $limit=null, $offset=0) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $sql = "SELECT * FROM ".TB_PREF."payroll_periods";
    if ($status !== null && $status !== '')
        $sql .= " WHERE status = ".db_escape((int)$status);
    $sql .= " ORDER BY from_date DESC, period_id DESC";

    $offset = max((int)$offset, 0);
    if ($limit !== null && (int)$limit > 0)
        $sql .= " LIMIT ".(int)$limit." OFFSET ".$offset;

    return db_query($sql, 'could not get payroll periods');
}

/**
 * Update payroll period status.
 *
 * @param int $period_id
 * @param int $status
 * @param int|null $expected_status Optional current status required for update
 * @return bool
 */
function update_payroll_period_status(
    $period_id,
    $status,
    $expected_status=null
) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $status = (int)$status;

    // Reopen and void are controlled commands, not generic status updates.
    // This blocks extension/direct-helper flag-only bypasses. The guarded
    // commands below use locked compare-and-set SQL after side-effect checks.
    if ($status === 6)
        return false;

    $set = array("status = ".db_escape($status));

    if ($status === 2) {
        $approved_by = '';
        if (isset($_SESSION['wa_current_user'])) {
            $user = $_SESSION['wa_current_user'];
            if (isset($user->loginname))
                $approved_by = $user->loginname;
            elseif (isset($user->username))
                $approved_by = $user->username;
        }
        if ($approved_by !== '')
            $set[] = "approved_by = ".db_escape($approved_by);
        $set[] = "approval_date = NOW()";
	} elseif ($status < 2) {
		$set[] = "approved_by = NULL";
		$set[] = "approval_date = NULL";
    }

    $sql = "UPDATE ".TB_PREF."payroll_periods SET ".implode(', ', $set).
        " WHERE period_id = ".db_escape((int)$period_id);
    if ($expected_status !== null)
        $sql .= " AND status = ".db_escape((int)$expected_status);
    if ($status < 2)
        $sql .= " AND status < 2";

    $result = db_query($sql, 'could not update payroll period status');
    if (!$result)
        return false;

    return $expected_status === null || db_num_affected_rows() === 1;
}

/**
 * Promote a payroll period to paid when all linked payslips are paid.
 *
 * @param int $period_id
 * @return bool
 */
function sync_payroll_period_payment_status($period_id) {
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !payroll_table_exists('payroll_periods'))
        return false;

    $period = get_payroll_period($period_id);
    if (!$period)
        return false;

    $current_status = isset($period['status']) ? (int)$period['status'] : 0;
    if (!in_array($current_status, array(2, 3, 4), true))
        return true;

    $payslip_table = payroll_payslip_header_table();
    if (!$payslip_table)
        return true;

    $sql = "SELECT COUNT(*) AS total_count,
        SUM(CASE WHEN COALESCE(status, 0) >= 3 AND COALESCE(status, 0) < 6 THEN 1 ELSE 0 END) AS paid_count
        FROM ".TB_PREF.$payslip_table.
        " WHERE payroll_period_id = ".db_escape($period_id)." AND COALESCE(status, 0) <> 6";
    $result = db_query($sql, 'could not summarize payroll period payment status');
    $row = db_fetch_assoc($result);

    $total_count = $row ? (int)$row['total_count'] : 0;
    $paid_count = $row ? (int)$row['paid_count'] : 0;

    if ($total_count > 0 && $paid_count >= $total_count && $current_status !== 4)
        return update_payroll_period_status($period_id, 4);

    return true;
}

/**
 * Mark a payroll period as paid and synchronize linked payslips.
 *
 * @param int $period_id
 * @return bool
 */
function mark_payroll_period_paid($period_id) {
    $period_id = (int)$period_id;
    if ($period_id <= 0)
        return false;

    $period = get_payroll_period($period_id);
    if (!$period)
        return false;

    $current_status = isset($period['status']) ? (int)$period['status'] : 0;
    if ($current_status === 4)
        return true;
    if (!in_array($current_status, array(2, 3), true))
        return false;

    begin_transaction();

    $ok = update_payroll_period_status($period_id, 4);
    $payslip_table = payroll_payslip_header_table();
    if ($ok && $payslip_table) {
        $sql = "UPDATE ".TB_PREF.$payslip_table." SET status = 3"
            ." WHERE payroll_period_id = ".db_escape($period_id)
            ." AND COALESCE(status, 0) <> 6";
        $ok = (bool)db_query($sql, 'could not mark linked payslips as paid');
    }

    if (!$ok) {
        cancel_transaction();
        return false;
    }

    commit_transaction();
    return true;
}

/**
 * Recalculate and update payroll period totals from payslips.
 *
 * @param int $period_id
 * @return bool
 */
function update_payroll_period_totals($period_id) {
    if (!payroll_table_exists('payroll_periods'))
        return false;

    $payslip_table = payroll_payslip_header_table();
    $gross = 0;
    $deductions = 0;
    $net = 0;
    $employer_cost = 0;

    if ($payslip_table) {
        $sum_sql = "SELECT
            SUM(IFNULL(gross_salary, 0)) as total_gross,
            SUM(IFNULL(total_deductions, 0)) as total_deductions,
            SUM(IFNULL(net_salary, IFNULL(payable_amount, 0))) as total_net,
            SUM(IFNULL(total_cost, IFNULL(employer_contributions, 0) + IFNULL(net_salary, IFNULL(payable_amount, 0)))) as total_employer_cost
            FROM ".TB_PREF.$payslip_table." WHERE payroll_period_id = ".db_escape((int)$period_id);
		if (payroll_payslip_has_column($payslip_table, 'status'))
			$sum_sql .= " AND COALESCE(status, 0) <> 6";
        $sum_res = db_query($sum_sql, 'could not aggregate payroll period totals');
        $sum_row = db_fetch_assoc($sum_res);

        $gross = (float)$sum_row['total_gross'];
        $deductions = (float)$sum_row['total_deductions'];
        $net = (float)$sum_row['total_net'];
        $employer_cost = (float)$sum_row['total_employer_cost'];
    }

    $sql = "UPDATE ".TB_PREF."payroll_periods SET
        total_gross = ".db_escape($gross).",
        total_deductions = ".db_escape($deductions).",
        total_net = ".db_escape($net).",
        total_employer_cost = ".db_escape($employer_cost)."
        WHERE period_id = ".db_escape((int)$period_id);

    return (bool)db_query($sql, 'could not update payroll period totals');
}

/**
 * Void payroll period and mark linked payslips as void when possible.
 *
 * @param int $period_id
 * @return bool
 */
function void_payroll_period($period_id) {
    $period_id = (int)$period_id;
    if ($period_id <= 0 || !payroll_table_exists('payroll_periods'))
        return false;

    begin_transaction();
    $period = get_payroll_period_for_update($period_id);
    if (!$period) {
        cancel_transaction();
        return false;
    }
    if ((int)$period['status'] === 6) {
        commit_transaction();
        return true;
    }

    $side_effects = payroll_period_financial_side_effects($period, true);
    if (!$side_effects['safe']) {
        payroll_side_effect_control_log_denial(
            'payroll_period_void',
            $side_effects['code'],
            $period_id,
            isset($side_effects['payslip_id']) ? $side_effects['payslip_id'] : 0,
            'payroll_db'
        );
        cancel_transaction();
        return false;
    }

    $current_status = isset($period['status']) ? (int)$period['status'] : 0;
    $sql = "UPDATE ".TB_PREF."payroll_periods SET status = 6"
        ." WHERE period_id = ".db_escape($period_id)
        ." AND status = ".db_escape($current_status);
    $ok = (bool)db_query($sql, 'could not void side-effect-free payroll period')
        && db_num_affected_rows() === 1;
    if (!$ok) {
        cancel_transaction();
        return false;
    }

    $payslip_table = payroll_payslip_header_table();
    $linked_payslip_changed_fields = array();
    if ($payslip_table) {
        $set_parts = array();
        if (payroll_payslip_has_column($payslip_table, 'status')) {
            $set_parts[] = "status = 6";
            $linked_payslip_changed_fields[] = 'status';
        }
        if (payroll_payslip_has_column($payslip_table, 'voided')) {
            $set_parts[] = "voided = 1";
            $linked_payslip_changed_fields[] = 'voided';
        }
        if (payroll_payslip_has_column($payslip_table, 'is_voided')) {
            $set_parts[] = "is_voided = 1";
            $linked_payslip_changed_fields[] = 'is_voided';
        }
        if (payroll_payslip_has_column($payslip_table, 'inactive')) {
            $set_parts[] = "inactive = 1";
            $linked_payslip_changed_fields[] = 'inactive';
        }
        if (payroll_payslip_has_column($payslip_table, 'voided_date')) {
            $set_parts[] = "voided_date = ".db_escape(date2sql(Today()));
            $linked_payslip_changed_fields[] = 'voided_date';
        }

        if ((int)$side_effects['payslip_count'] > 0 && empty($set_parts)) {
            cancel_transaction();
            return false;
        }
        if (!empty($set_parts) && (int)$side_effects['payslip_count'] > 0) {
            $sql = "UPDATE ".TB_PREF.$payslip_table." SET ".implode(', ', $set_parts)
                ." WHERE payroll_period_id = ".db_escape($period_id);
            $ok = (bool)db_query($sql, 'could not void side-effect-free linked payslips');
        }
    }

    $related_changes = array();
    if ((int)$side_effects['payslip_count'] > 0) {
        $related_changes = array(
            'entity_type' => 'payslip',
            'row_count' => (int)$side_effects['payslip_count'],
            'changed_fields' => $linked_payslip_changed_fields,
        );
    }

    $after = $ok ? get_payroll_period_for_update($period_id) : false;
    if (!$after || (int)$after['status'] !== 6
        || !payroll_append_side_effect_free_transition_audit(
            'payroll_period',
            array('period_id' => $period_id),
            'payroll_period.voided',
            'legacy_side_effect_free_void',
            $period,
            $after,
            array('status'),
            'voided',
            $related_changes
        )
    ) {
        error_log('PAY-AUD-001: side-effect-free payroll-period void audit append failed');
        cancel_transaction();
        return false;
    }

    commit_transaction();
    return true;
}
