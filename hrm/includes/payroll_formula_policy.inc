<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

if (!defined('HRM_LEGACY_FORMULA_POLICY_ID'))
    define('HRM_LEGACY_FORMULA_POLICY_ID', 'notrinoserp.hrm.payroll.legacy_formula_policy');
if (!defined('HRM_LEGACY_FORMULA_POLICY_VERSION'))
    define('HRM_LEGACY_FORMULA_POLICY_VERSION', '1.0.0');
if (!defined('HRM_LEGACY_FORMULA_MAX_LENGTH'))
    define('HRM_LEGACY_FORMULA_MAX_LENGTH', 1000);
if (!defined('HRM_LEGACY_FORMULA_MAX_TOKENS'))
    define('HRM_LEGACY_FORMULA_MAX_TOKENS', 256);
if (!defined('HRM_LEGACY_FORMULA_MAX_DEPTH'))
    define('HRM_LEGACY_FORMULA_MAX_DEPTH', 32);

/**
 * Return the fixed legacy payroll-formula source catalog.
 *
 * No request value is ever accepted as a table or column identifier.
 *
 * @return array
 */
function hrm_legacy_formula_source_catalog() {
    return array(
        'pay_elements.formula' => array('table' => 'pay_elements', 'column' => 'formula'),
        'salary_structure.formula' => array('table' => 'salary_structure', 'column' => 'formula'),
        'employee_salary.formula' => array('table' => 'employee_salary', 'column' => 'formula')
    );
}

/**
 * Return variables supported by the frozen legacy evaluator.
 *
 * @return array
 */
function hrm_legacy_formula_variable_catalog() {
    return array(
        'BASIC' => true,
        'GROSS' => true,
        'DAYS_WORKED' => true,
        'PAYABLE_DAYS' => true,
        'WORKING_DAYS' => true,
        'DAYS_IN_MONTH' => true,
        'LEAVE_DAYS' => true,
        'PAID_LEAVE_DAYS' => true,
        'ABSENT_DAYS' => true,
        'OVERTIME_HOURS' => true,
        'UNPAID_LEAVE_DAYS' => true,
        'HOURLY_RATE' => true
    );
}

/**
 * Normalize only transport-level surrounding whitespace.
 *
 * Inner bytes are retained because the legacy formula source is frozen.
 *
 * @param mixed $formula
 * @return string
 */
function hrm_legacy_formula_normalize($formula) {
    return trim((string)$formula);
}

/**
 * Return a non-reversible formula fingerprint.
 *
 * @param mixed $formula
 * @return string
 */
function hrm_legacy_formula_fingerprint($formula) {
    return hash('sha256', hrm_legacy_formula_normalize($formula));
}

/**
 * Classify one formula without executing it or invoking FormulaFacade.
 *
 * @param mixed $formula
 * @return array
 */
function hrm_legacy_formula_classify($formula) {
    $formula = hrm_legacy_formula_normalize($formula);
    $base = array(
        'classification' => 'unsupported',
        'reason' => 'invalid_syntax',
        'formula_hash' => hash('sha256', $formula),
        'token_count' => 0,
        'variable_count' => 0
    );

    if ($formula === '') {
        $base['classification'] = 'empty';
        $base['reason'] = 'empty';
        return $base;
    }
    if (strlen($formula) > HRM_LEGACY_FORMULA_MAX_LENGTH) {
        $base['reason'] = 'length_limit';
        return $base;
    }
    if (preg_match('/[\x00-\x1F\x7F]/', $formula)) {
        $base['reason'] = 'control_character';
        return $base;
    }

    $length = strlen($formula);
    $offset = 0;
    $depth = 0;
    $expect_value = true;
    $variables = array();
    $tokens = 0;
    $allowed_variables = hrm_legacy_formula_variable_catalog();

    while ($offset < $length) {
        if (preg_match('/\G\s+/A', $formula, $match, 0, $offset)) {
            $offset += strlen($match[0]);
            continue;
        }

        $token = '';
        $type = '';
        if (preg_match('/\G(?:\d+(?:\.\d*)?|\.\d+)/A', $formula, $match, 0, $offset)) {
            $token = $match[0];
            $type = 'number';
        } elseif (preg_match('/\G[A-Za-z_][A-Za-z0-9_]*/A', $formula, $match, 0, $offset)) {
            $token = strtoupper($match[0]);
            $type = 'identifier';
        } elseif (preg_match('/\G[+\-*\/%()]/A', $formula, $match, 0, $offset)) {
            $token = $match[0];
            $type = 'symbol';
        } else {
            $base['reason'] = 'hostile_or_unknown_token';
            return $base;
        }

        $offset += strlen($match[0]);
        $tokens++;
        if ($tokens > HRM_LEGACY_FORMULA_MAX_TOKENS) {
            $base['reason'] = 'token_limit';
            return $base;
        }

        if ($type === 'number' || $type === 'identifier') {
            if (!$expect_value) {
                $base['reason'] = 'missing_operator';
                return $base;
            }
            if ($type === 'identifier') {
                if (!isset($allowed_variables[$token])) {
                    $base['reason'] = 'unsupported_identifier';
                    return $base;
                }
                $variables[$token] = true;
            }
            $expect_value = false;
            continue;
        }

        if ($token === '(') {
            if (!$expect_value) {
                $base['reason'] = 'missing_operator';
                return $base;
            }
            $depth++;
            if ($depth > HRM_LEGACY_FORMULA_MAX_DEPTH) {
                $base['reason'] = 'depth_limit';
                return $base;
            }
            $expect_value = true;
            continue;
        }

        if ($token === ')') {
            if ($expect_value || $depth <= 0) {
                $base['reason'] = 'unbalanced_parenthesis';
                return $base;
            }
            $depth--;
            $expect_value = false;
            continue;
        }

        if ($expect_value) {
            if ($token !== '+' && $token !== '-') {
                $base['reason'] = 'operator_without_value';
                return $base;
            }
            continue;
        }
        $expect_value = true;
    }

    if ($expect_value) {
        $base['reason'] = 'trailing_operator';
        return $base;
    }
    if ($depth !== 0) {
        $base['reason'] = 'unbalanced_parenthesis';
        return $base;
    }

    $base['classification'] = 'legacy_compatible';
    $base['reason'] = 'accepted_legacy_subset';
    $base['token_count'] = $tokens;
    $base['variable_count'] = count($variables);
    return $base;
}

/**
 * Decide whether a legacy formula source change is admitted after the freeze.
 *
 * Unchanged formulas remain usable and may accompany unrelated metadata edits.
 * Clearing a formula is allowed so the legacy execution surface can shrink.
 * New or changed non-empty formulas are denied until PAY-RULE-002/006 provides
 * a reviewed publication and migration path.
 *
 * @param string $source
 * @param mixed $old_formula
 * @param mixed $new_formula
 * @return array
 */
function hrm_legacy_formula_change_decision($source, $old_formula, $new_formula) {
    $catalog = hrm_legacy_formula_source_catalog();
    $old = hrm_legacy_formula_normalize($old_formula);
    $new = hrm_legacy_formula_normalize($new_formula);

    $decision = array(
        'allowed' => false,
        'reason' => 'unknown_source',
        'source' => (string)$source,
        'old_hash' => hash('sha256', $old),
        'new_hash' => hash('sha256', $new)
    );
    if (!isset($catalog[$source]))
        return $decision;

    if (hash_equals(hash('sha256', $old), hash('sha256', $new))) {
        $decision['allowed'] = true;
        $decision['reason'] = 'unchanged';
        return $decision;
    }
    if ($new === '') {
        $decision['allowed'] = true;
        $decision['reason'] = 'retired';
        return $decision;
    }

    $decision['reason'] = $old === '' ? 'new_formula_frozen' : 'changed_formula_frozen';
    return $decision;
}

/**
 * Return a numeric actor id for minimized operational evidence.
 *
 * @return int
 */
function hrm_legacy_formula_actor_id() {
    if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user']))
        return 0;
    $user = $_SESSION['wa_current_user'];
    return isset($user->user) && is_numeric($user->user) ? (int)$user->user : 0;
}

/**
 * Return the current numeric company id where available.
 *
 * @return int
 */
function hrm_legacy_formula_company_id() {
    if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user']))
        return 0;
    $user = $_SESSION['wa_current_user'];
    if (isset($user->cur_con) && is_numeric($user->cur_con))
        return (int)$user->cur_con;
    return isset($user->company) && is_numeric($user->company) ? (int)$user->company : 0;
}

/**
 * Enforce and log the legacy formula change freeze.
 *
 * Formula text, employee identifiers, account values and amounts are never
 * written to the event. Record identity must be numeric or a numeric-only
 * composite supplied by a trusted database helper.
 *
 * @param string $source
 * @param mixed $record_identity
 * @param mixed $old_formula
 * @param mixed $new_formula
 * @return bool
 */
function hrm_legacy_formula_guard_change($source, $record_identity, $old_formula, $new_formula) {
    $decision = hrm_legacy_formula_change_decision($source, $old_formula, $new_formula);
    $GLOBALS['hrm_legacy_formula_last_decision'] = $decision;
    if (!empty($decision['allowed']))
        return true;

    $safe_identity = preg_replace('/[^0-9:.-]/', '', (string)$record_identity);
    error_log(json_encode(array(
        'contract' => HRM_LEGACY_FORMULA_POLICY_ID,
        'version' => HRM_LEGACY_FORMULA_POLICY_VERSION,
        'action' => 'legacy_formula_change_denied',
        'company' => hrm_legacy_formula_company_id(),
        'actor' => hrm_legacy_formula_actor_id(),
        'source' => isset(hrm_legacy_formula_source_catalog()[$source]) ? $source : 'unknown',
        'record' => $safe_identity,
        'reason' => $decision['reason'],
        'old_hash' => $decision['old_hash'],
        'new_hash' => $decision['new_hash']
    )));
    return false;
}

/**
 * Return the most recent safe denial reason for UI/controller handling.
 *
 * @return string
 */
function hrm_legacy_formula_last_error() {
    if (empty($GLOBALS['hrm_legacy_formula_last_decision'])
        || !empty($GLOBALS['hrm_legacy_formula_last_decision']['allowed'])
        || empty($GLOBALS['hrm_legacy_formula_last_decision']['reason']))
        return '';
    $reason = $GLOBALS['hrm_legacy_formula_last_decision']['reason'];
    if ($reason === 'new_formula_frozen' || $reason === 'changed_formula_frozen')
        return 'Legacy payroll formula authoring is frozen. Existing formulas may remain unchanged or be cleared only.';
    return 'Legacy payroll formula change was denied by policy.';
}

/**
 * Inventory and classify legacy formulas without returning formula source text.
 *
 * @return array|false
 */
function hrm_legacy_formula_inventory() {
    $inventory = array();
    foreach (hrm_legacy_formula_source_catalog() as $source => $definition) {
        $table = $definition['table'];
        $column = $definition['column'];
        $result = db_query(
            "SELECT TRIM(".$column.") AS formula_source, COUNT(*) AS row_count"
            ." FROM ".TB_PREF.$table
            ." WHERE TRIM(COALESCE(".$column.", '')) <> ''"
            ." GROUP BY TRIM(".$column.") ORDER BY SHA2(TRIM(".$column."), 256)",
            'could not inventory legacy payroll formulas'
        );
        if ($result === false)
            return false;

        $formula_count = 0;
        $hashes = array();
        $classifications = array('legacy_compatible' => 0, 'unsupported' => 0);
        $reason_counts = array();
        while ($row = db_fetch_assoc($result)) {
            $classification = hrm_legacy_formula_classify($row['formula_source']);
            $row_count = (int)$row['row_count'];
            $formula_count += $row_count;
            $hashes[] = $classification['formula_hash'];
            $classifications[$classification['classification']] += $row_count;
            $reason = $classification['reason'];
            if (!isset($reason_counts[$reason]))
                $reason_counts[$reason] = 0;
            $reason_counts[$reason] += $row_count;
        }
        ksort($reason_counts, SORT_STRING);
        $inventory[$source] = array(
            'formula_count' => $formula_count,
            'distinct_formula_count' => count($hashes),
            'legacy_compatible_count' => $classifications['legacy_compatible'],
            'unsupported_count' => $classifications['unsupported'],
            'reason_counts' => $reason_counts,
            'formula_set_hash' => hash('sha256', implode('|', $hashes))
        );
    }
    return $inventory;
}
