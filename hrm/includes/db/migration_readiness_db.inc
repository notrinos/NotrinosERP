<?php
include_once(dirname(__FILE__).'/migration_registry_db.inc');
/**
 * Shared fail-closed helpers for HRM migration metadata and readiness checks.
 *
 * FrontAccounting keeps TB_PREF as the literal &TB_PREF& token until db_query()
 * substitutes the active company's physical prefix. Metadata values must resolve
 * that prefix before db_escape() HTML-escapes the token.
 */

if (!defined('HRM_MIGRATION_MISSING_SCHEMA'))
    define('HRM_MIGRATION_MISSING_SCHEMA', 'MISSING_SCHEMA');
if (!defined('HRM_MIGRATION_MISSING_SEED_OR_PROJECTION'))
    define('HRM_MIGRATION_MISSING_SEED_OR_PROJECTION', 'MISSING_SEED_OR_PROJECTION');
if (!defined('HRM_MIGRATION_MISSING_AUDIT_EVIDENCE'))
    define('HRM_MIGRATION_MISSING_AUDIT_EVIDENCE', 'MISSING_AUDIT_EVIDENCE');
if (!defined('HRM_MIGRATION_PROJECTION_MISMATCH'))
    define('HRM_MIGRATION_PROJECTION_MISMATCH', 'PROJECTION_MISMATCH');
if (!defined('HRM_MIGRATION_FK_MISMATCH'))
    define('HRM_MIGRATION_FK_MISMATCH', 'FK_MISMATCH');
if (!defined('HRM_MIGRATION_INDEX_MISMATCH'))
    define('HRM_MIGRATION_INDEX_MISMATCH', 'INDEX_MISMATCH');
if (!defined('HRM_MIGRATION_PREDECESSOR_MIGRATION_ABSENT'))
    define('HRM_MIGRATION_PREDECESSOR_MIGRATION_ABSENT', 'PREDECESSOR_MIGRATION_ABSENT');
if (!defined('HRM_MIGRATION_PARTIAL_MIGRATION'))
    define('HRM_MIGRATION_PARTIAL_MIGRATION', 'PARTIAL_MIGRATION');
if (!defined('HRM_MIGRATION_ALREADY_COMPLETE'))
    define('HRM_MIGRATION_ALREADY_COMPLETE', 'ALREADY_COMPLETE');
if (!defined('HRM_MIGRATION_DUPLICATE_AUDIT'))
    define('HRM_MIGRATION_DUPLICATE_AUDIT', 'DUPLICATE_AUDIT');
if (!defined('HRM_MIGRATION_TAMPER_SUSPECT'))
    define('HRM_MIGRATION_TAMPER_SUSPECT', 'TAMPER_SUSPECT');

/** @param object $patch @param string $name @return bool */
function hrm_migration_require_registered_edge($patch, $name)
{
    if (is_object($patch) && isset($patch->previous) && isset($patch->version)
        && hrm_migration_registered_edge_matches($name, $patch->previous, $patch->version))
        return true;
    return hrm_migration_fail($patch, HRM_MIGRATION_TAMPER_SUSPECT,
        'Immutable HRM migration edge registry mismatch for '.(string)$name.'.');
}

/**
 * Return every stable HRM migration preflight state.
 *
 * @return array
 */
function hrm_migration_preflight_states()
{
    return array(
        HRM_MIGRATION_MISSING_SCHEMA,
        HRM_MIGRATION_MISSING_SEED_OR_PROJECTION,
        HRM_MIGRATION_MISSING_AUDIT_EVIDENCE,
        HRM_MIGRATION_PROJECTION_MISMATCH,
        HRM_MIGRATION_FK_MISMATCH,
        HRM_MIGRATION_INDEX_MISMATCH,
        HRM_MIGRATION_PREDECESSOR_MIGRATION_ABSENT,
        HRM_MIGRATION_PARTIAL_MIGRATION,
        HRM_MIGRATION_ALREADY_COMPLETE,
        HRM_MIGRATION_DUPLICATE_AUDIT,
        HRM_MIGRATION_TAMPER_SUSPECT,
    );
}

/**
 * Resolve the active prefix exactly as db_query() does.
 *
 * Literal-prefix disposable harnesses remain supported, while real application
 * execution resolves &TB_PREF& from the active company connection.
 *
 * @return string|false
 */
function hrm_migration_active_table_prefix()
{
    global $db_connections;

    if (!defined('TB_PREF'))
        return false;
    if ((string)TB_PREF !== '&TB_PREF&') {
        $prefix = (string)TB_PREF;
    } else {
        $company = 0;
        if (isset($_SESSION['wa_current_user'])
            && is_object($_SESSION['wa_current_user'])
            && isset($_SESSION['wa_current_user']->cur_con))
            $company = (int)$_SESSION['wa_current_user']->cur_con;
        if (!is_array($db_connections)
            || !isset($db_connections[$company])
            || !is_array($db_connections[$company])
            || !isset($db_connections[$company]['tbpref']))
            return false;
        $prefix = (string)$db_connections[$company]['tbpref'];
    }

    return preg_match('/^[A-Za-z0-9_]*$/D', $prefix) === 1 ? $prefix : false;
}

/**
 * Return a validated physical table name for metadata string comparisons.
 *
 * @param string $table Unprefixed table suffix.
 * @return string|false
 */
function hrm_migration_physical_table_name($table)
{
    $table = (string)$table;
    $prefix = hrm_migration_active_table_prefix();
    if ($prefix === false || preg_match('/^[A-Za-z0-9_]+$/D', $table) !== 1)
        return false;
    $physical = $prefix.$table;
    return strlen($physical) <= 64 ? $physical : false;
}

/**
 * Test exact base-table existence without LIKE wildcard ambiguity.
 *
 * @param string $table Unprefixed table suffix.
 * @return bool
 */
function hrm_migration_table_exists($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $result = db_query("SELECT COUNT(*) row_count FROM INFORMATION_SCHEMA.TABLES "
        ."WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)
        ." AND TABLE_TYPE='BASE TABLE'");
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && (int)$row['row_count'] === 1;
}

/**
 * Require an exact set of predecessor base tables and identify the first gap.
 *
 * @param object $patch Patch instance.
 * @param array $tables Unprefixed table suffixes.
 * @param string $context Migration/predecessor label.
 * @return bool
 */
function hrm_migration_require_tables($patch, $tables, $context)
{
    foreach ($tables as $table) {
        if (!hrm_migration_table_exists($table))
            return hrm_migration_fail($patch, HRM_MIGRATION_MISSING_SCHEMA,
                (string)$context.' requires table '.(string)$table.'.');
    }
    return true;
}

/**
 * Return physical index names for one table.
 *
 * @param string $table Unprefixed table suffix.
 * @return array|false
 */
function hrm_migration_index_names($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $result = db_query("SELECT DISTINCT INDEX_NAME index_name FROM INFORMATION_SCHEMA.STATISTICS "
        ."WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical));
    if (!$result)
        return false;
    $names = array();
    while ($row = db_fetch_assoc($result))
        $names[(string)$row['index_name']] = true;
    return $names;
}

/**
 * Return physical foreign-key constraint names for one table.
 *
 * @param string $table Unprefixed table suffix.
 * @return array|false
 */
function hrm_migration_foreign_key_names($table)
{
    $physical = hrm_migration_physical_table_name($table);
    if ($physical === false)
        return false;
    $result = db_query("SELECT CONSTRAINT_NAME constraint_name FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS "
        ."WHERE CONSTRAINT_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical));
    if (!$result)
        return false;
    $names = array();
    while ($row = db_fetch_assoc($result))
        $names[(string)$row['constraint_name']] = true;
    return $names;
}

/**
 * Classify a conflicting existing schema as index, FK, or other tamper.
 *
 * The caller invokes this only after its complete immutable schema contract
 * has failed. Missing target tables are reported separately as MISSING_SCHEMA.
 *
 * @param string $table Unprefixed table suffix.
 * @param array $required_indexes Physical-independent index names.
 * @param array $required_foreign_keys Unprefixed FK constraint suffixes.
 * @return string
 */
function hrm_migration_schema_failure_state($table, $required_indexes = array(), $required_foreign_keys = array())
{
    if (!hrm_migration_table_exists($table))
        return HRM_MIGRATION_MISSING_SCHEMA;
    $indexes = hrm_migration_index_names($table);
    if (!is_array($indexes))
        return HRM_MIGRATION_TAMPER_SUSPECT;
    foreach ($required_indexes as $name) {
        if (!isset($indexes[(string)$name]))
            return HRM_MIGRATION_INDEX_MISMATCH;
    }
    $foreign_keys = hrm_migration_foreign_key_names($table);
    if (!is_array($foreign_keys))
        return HRM_MIGRATION_TAMPER_SUSPECT;
    foreach ($required_foreign_keys as $suffix) {
        $physical = hrm_migration_physical_table_name($suffix);
        if ($physical === false || !isset($foreign_keys[$physical]))
            return HRM_MIGRATION_FK_MISMATCH;
    }
    return HRM_MIGRATION_TAMPER_SUSPECT;
}

/**
 * Log the most specific state available for a conflicting schema contract.
 *
 * @param object $patch Patch instance.
 * @param string $table Unprefixed table suffix.
 * @param array $required_indexes Required index names.
 * @param array $required_foreign_keys Required FK suffixes.
 * @param string $detail Human-readable invariant detail.
 * @return false
 */
function hrm_migration_fail_schema_contract($patch, $table, $required_indexes,
    $required_foreign_keys, $detail)
{
    return hrm_migration_fail($patch,
        hrm_migration_schema_failure_state($table, $required_indexes, $required_foreign_keys),
        $detail);
}

/**
 * Store a machine-readable preflight state on a patch instance.
 *
 * @param object $patch Patch instance.
 * @param string $state Stable preflight state.
 * @param string $detail Human-readable invariant detail.
 * @return bool
 */
function hrm_migration_record_state($patch, $state, $detail = '')
{
    if (!in_array((string)$state, hrm_migration_preflight_states(), true))
        return false;
    if (is_object($patch) && property_exists($patch, 'preflight_state')
        && property_exists($patch, 'preflight_detail')) {
        $patch->preflight_state = (string)$state;
        $patch->preflight_detail = (string)$detail;
    }
    return true;
}

/**
 * Log one exact fail-closed preflight state.
 *
 * @param object $patch Patch instance exposing log_error().
 * @param string $state Stable preflight state.
 * @param string $detail Human-readable invariant detail.
 * @return false
 */
function hrm_migration_fail($patch, $state, $detail)
{
    if (!hrm_migration_record_state($patch, $state, $detail))
        return false;
    return $patch->log_error((string)$state.': '.(string)$detail);
}

/**
 * Record a non-error terminal preflight state.
 *
 * @param object $patch Patch instance exposing log_error().
 * @param string $state Stable preflight state.
 * @param string $detail Human-readable invariant detail.
 * @return bool
 */
function hrm_migration_info($patch, $state, $detail)
{
    if (!hrm_migration_record_state($patch, $state, $detail))
        return false;
    $patch->log_error((string)$state.': '.(string)$detail, 'Info');
    return true;
}
