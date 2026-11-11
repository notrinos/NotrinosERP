<?php
/**
 * DIM-LFC-001 B08: authoritative registration and lifecycle fencing of session drafts.
 *
 * Unsaved carts (sales/purchase/GL/inventory/allocation editors) live only inside the PHP
 * session of their author. A deleter in another request can neither see them nor may it
 * unserialize session files, trust a caller's claim, or treat an unreadable session as unused.
 * This authority therefore keeps a durable, company-scoped, server-derived registration:
 *
 *  - session_draft_seals: one row per registered authenticated session (user_sessions.id) that
 *    says whether the session's draft state was fully classified ('complete') or not ('unknown'),
 *    for which company and user, and the fingerprint of what was registered.
 *  - session_draft_references: the exact positive native Dimension identities the live carts
 *    hold (draft key, property slot, identity). Nothing else (no cart payload) is stored.
 *
 * Writer side: dimension_session_draft_register() runs at the end of every authenticated request
 * (before the session is written), reads the LIVE in-memory $_SESSION (an allowlisted object walk,
 * never unserialize), then replaces the session's references and seal and share-locks the referenced
 * masters in ascending order inside one transaction, owner rows first and masters last (the order
 * the locked recheck uses). A failed registration fails closed (the seal becomes unknown, and when even
 * that cannot be written the cart keys are dropped from the session so no unregistered draft persists).
 *
 * Reader side: dimension_session_draft_observe() runs inside the locked recheck transaction. A
 * session counts only while it is live in the durable registry (not revoked, not idle/absolute expired);
 * a revoked, logged-out or expired session can no longer open its carts. Every live session needs a
 * 'complete' seal of this company: a missing seal (a session that never registered, or an upgrade
 * that left it unsealed), an unknown seal, a company mismatch, an unavailable table or a failed
 * read all report unknown, never unused. The deleter's own request is also classified from its live
 * in-memory carts. A live reference reports used. This is NOT a deletion certificate (C01 owns delete).
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_lifecycle_db.inc';

/** Maximum nodes and depth walked in one session before the draft state is declared unknown. */
define('DIMENSION_SESSION_DRAFT_MAX_NODES', 20000);
define('DIMENSION_SESSION_DRAFT_MAX_DEPTH', 12);

/**
 * Cart classes the authority knows how to read (lower-case). Any other object found in the
 * session is opaque and makes the session draft state unknown.
 *
 * @return array
 */
function dimension_session_draft_allowed_classes()
{
    return array('items_cart'=>true, 'gl_item'=>true, 'line_item'=>true, 'purch_order'=>true, 'po_line_details'=>true,
        'supp_trans'=>true, 'grn_item'=>true, 'gl_codes'=>true, 'cart'=>true, 'line_details'=>true,
        'allocation'=>true, 'allocation_item'=>true);
}

/**
 * Classes reviewed as read-only presentation state (the inquiry pager holds a query, its columns and the
 * current page of result rows). They cannot write anything, so they are not drafts; any coordinate a result
 * row shows is re-validated by the writers' own fences at write time.
 *
 * @return array
 */
function dimension_session_draft_ignored_classes()
{
    return array('db_pager'=>true);
}

/**
 * Session keys reviewed as holding no Dimension coordinate: authentication, presentation,
 * transport, security and protected HRM/payroll idempotency state (protected evidence has its own authority).
 *
 * @return array
 */
function dimension_session_draft_ignored_keys()
{
    return array('wa_current_user'=>true, 'language'=>true, 'SysPrefs'=>true, 'App'=>true, 'IPaddress'=>true,
        'userAgent'=>true, 'OBSOLETE'=>true, 'EXPIRES'=>true, 'timeout'=>true, 'installed_extensions_signature'=>true,
        'page_title'=>true, 'csrf_token'=>true, 'sel_app'=>true, '_default_date'=>true, 'reports'=>true,
        'print_job'=>true, '_print_job'=>true, 'company_setup_updated_notice'=>true, 'ui_context'=>true,
        'report_artifact_grants'=>true, 'formula_designer_rate_limits'=>true, 'purch_matching_last_result'=>true,
        'Processing'=>true, 'security_session_registry_id'=>true, 'security_session_secret'=>true,
        'wa_global_supplier_id'=>true, 'wa_global_stock_id'=>true, 'wa_global_customer_id'=>true,
        'wa_global_employee_id'=>true, 'wa_global_curr_code'=>true);
}

/**
 * Keys that retain an uploaded batch whose rows the session cannot classify (the importers parse the bytes
 * when the batch is confirmed). While one is present the session is unknown, never unused; the importers'
 * own writers still validate every coordinate at write time.
 *
 * @return array
 */
function dimension_session_draft_opaque_keys()
{
    return array('customer_csv_preview'=>true, 'csv_import_file'=>true);
}

/** Reviewed key prefixes of protected evidence or security state that never holds a draft coordinate. */
function dimension_session_draft_ignored_prefixes()
{
    return array('federation_', 'hrm_', 'pay_ctry_', 'security_');
}

/** Return true when a session key is reviewed as holding no Dimension coordinate. */
function dimension_session_draft_key_ignored($key)
{
    if (!is_string($key)) return false;
    $keys = dimension_session_draft_ignored_keys();
    if (isset($keys[$key])) return true;
    foreach (dimension_session_draft_ignored_prefixes() as $prefix)
        if (strncmp($key, $prefix, strlen($prefix)) === 0) return true;
    return false;
}

/**
 * Property/key names that carry a native Dimension identity in the maintained carts and editors.
 *
 * @return array
 */
function dimension_session_draft_reference_names()
{
    return array('dimension'=>true, 'dimension2'=>true, 'dimension_id'=>true, 'dimension2_id'=>true,
        'gl_dim'=>true, 'gl_dim2'=>true);
}

/**
 * Classify one scalar found under a Dimension-like name.
 *
 * @param mixed $value
 * @return int|bool Positive identity, 0 for blank, false when the value is not an exact native identity
 */
function dimension_session_draft_reference_id($value)
{
    if ($value === null || $value === '' || $value === false) return 0;
    if (is_int($value)) return $value >= 0 && $value <= 2147483647 ? $value : false;
    if (!is_string($value) || !preg_match('/^(?:0|[1-9][0-9]{0,9})$/D', $value)) return false;
    $integer = (int)$value;
    return $integer <= 2147483647 && (string)$integer === $value ? $integer : false;
}

/** Strip the visibility marker PHP adds to protected/private property names in an array cast. */
function dimension_session_draft_property_name($name)
{
    $position = strrpos($name, "\0");
    return $position === false ? $name : substr($name, $position + 1);
}

/**
 * Walk one session value without unserializing anything, collecting exact references.
 *
 * @param mixed $value Live in-memory value
 * @param string $draft Top-level session key the value belongs to
 * @param string $slot Name of the property/key holding this value ('' for list members)
 * @param int $depth Current depth
 * @param array $state Accumulator: references (key => array), unclassified (name => true), nodes, stack
 */
function dimension_session_draft_walk($value, $draft, $slot, $depth, &$state)
{
    if (++$state['nodes'] > DIMENSION_SESSION_DRAFT_MAX_NODES || $depth > DIMENSION_SESSION_DRAFT_MAX_DEPTH) {
        $state['unclassified']['bounds:'.$draft] = true;
        return;
    }
    $dimension_like = $slot !== '' && preg_match('/dim|cost_cent/i', $slot);
    if (is_object($value) || is_array($value)) {
        if ($dimension_like) { $state['unclassified']['structured:'.$draft.'.'.$slot] = true; return; }
        if (is_object($value)) {
            $class = strtolower(get_class($value));
            $ignored = dimension_session_draft_ignored_classes();
            if (isset($ignored[$class])) return;
            $allowed = dimension_session_draft_allowed_classes();
            if (!isset($allowed[$class])) { $state['unclassified']['class:'.$draft] = true; return; }
            $hash = spl_object_hash($value);
            if (isset($state['stack'][$hash])) return; // cycle already being walked
            $state['stack'][$hash] = true;
            $members = (array)$value;
        } else {
            $members = $value;
        }
        foreach ($members as $name=>$member) {
            $child = is_int($name) ? '' : dimension_session_draft_property_name($name);
            dimension_session_draft_walk($member, $draft, $child, $depth + 1, $state);
        }
        if (isset($hash)) unset($state['stack'][$hash]);
        return;
    }
    if (!$dimension_like) return;
    $names = dimension_session_draft_reference_names();
    $identity = dimension_session_draft_reference_id($value);
    if ($identity === 0) return; // blank
    if (!isset($names[strtolower($slot)]) || $identity === false) { $state['unclassified']['value:'.$draft.'.'.$slot] = true; return; }
    $state['references'][$draft."\0".strtolower($slot)."\0".$identity] = array($draft, strtolower($slot), $identity);
}

/**
 * Classify the draft state of one session array from its live in-memory values.
 *
 * @param array $session The session array to classify (normally $_SESSION)
 * @return array state complete|unknown, references list of array(draft key, slot, identity), reasons list of names
 */
function dimension_session_draft_collect($session)
{
    $state = array('references'=>array(), 'unclassified'=>array(), 'nodes'=>0, 'stack'=>array());
    if (!is_array($session)) return array('state'=>'unknown', 'references'=>array(), 'reasons'=>array('session'));
    foreach ($session as $key=>$value) {
        if (!is_string($key)) { $state['unclassified']['key'] = true; continue; }
        $opaque = dimension_session_draft_opaque_keys();
        if (isset($opaque[$key])) { if (!empty($value)) $state['unclassified']['opaque:'.$key] = true; continue; }
        if (dimension_session_draft_key_ignored($key)) continue;
        dimension_session_draft_walk($value, substr($key, 0, 40), '', 0, $state);
    }
    $references = array_values($state['references']);
    usort($references, 'dimension_session_draft_compare');
    $reasons = array_keys($state['unclassified']);
    sort($reasons);
    return array('state'=>count($reasons) ? 'unknown' : 'complete', 'references'=>$references, 'reasons'=>$reasons);
}

/** Deterministic order of collected references (draft, slot, identity). */
function dimension_session_draft_compare($left, $right)
{
    $cmp = strcmp($left[0], $right[0]);
    if ($cmp === 0) $cmp = strcmp($left[1], $right[1]);
    if ($cmp === 0) $cmp = $left[2] < $right[2] ? -1 : ($left[2] > $right[2] ? 1 : 0);
    return $cmp;
}

/** Stable digest of one classification, bound to company and user. */
function dimension_session_draft_fingerprint($company, $user, $collected)
{
    $parts = array((int)$company, (int)$user, $collected['state']);
    foreach ($collected['references'] as $reference) $parts[] = $reference[0].'|'.$reference[1].'|'.$reference[2];
    return hash('sha256', implode("\n", $parts));
}

/**
 * Return whether the live company schema provides the registry tables with the registered columns.
 * Re-read on every call: no session, company or connection availability cache.
 *
 * @return bool|string True when ready, 'absent' when the read succeeded but a table or column is missing
 *                     (an upgrade not applied yet), false when the schema could not be read
 */
function dimension_session_draft_schema_ready()
{
    $expected = array(
        'user_sessions'=>array('id', 'user_id', 'revoked_at', 'idle_expires_at', 'absolute_expires_at'),
        'session_draft_seals'=>array('session_id', 'company_id', 'user_id', 'state', 'fingerprint', 'updated_at'),
        'session_draft_references'=>array('id', 'session_id', 'draft_key', 'slot', 'dimension_id'));
    $names = array();
    foreach (array_keys($expected) as $table) $names[] = "'".db_escape_function(TB_PREF.$table)."'";
    $rows = db_query('SELECT TABLE_NAME, COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('
        .implode(',', $names).')');
    if (!$rows) return false;
    $company = isset($_SESSION['wa_current_user']->cur_con) ? $_SESSION['wa_current_user']->cur_con : 0;
    $prefix = isset($GLOBALS['db_connections'][$company]['tbpref']) ? (string)$GLOBALS['db_connections'][$company]['tbpref'] : '';
    $present = array();
    while ($row = db_fetch_row($rows)) $present[$row[0]][$row[1]] = true;
    foreach ($expected as $table=>$columns)
        foreach ($columns as $column)
            if (!isset($present[$prefix.$table][$column])) return 'absent';
    return true;
}

/** Current company and actor identity of the running request, or false when unauthenticated. */
function dimension_session_draft_actor()
{
    $user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
    if (!is_object($user) || !isset($user->user, $user->cur_con) || (int)$user->user <= 0) return false;
    return array('company'=>(int)$user->cur_con, 'user'=>(int)$user->user);
}

/**
 * Register the live drafts of the current request's session. Idempotent: an unchanged classification
 * writes nothing. Fails closed (returns false) on any unreadable table, SQL or lock failure.
 *
 * @return string|bool registered, unchanged, skipped (not a registered authenticated session), unavailable
 *                     (upgrade not applied) or false
 */
function dimension_session_draft_register()
{
    $actor = dimension_session_draft_actor();
    $registry_id = isset($_SESSION['security_session_registry_id']) ? (int)$_SESSION['security_session_registry_id'] : 0;
    if ($actor === false || $registry_id <= 0) return 'skipped';
    try {
        $ready = dimension_session_draft_schema_ready();
        // Before the upgrade the tables do not exist: nothing can be registered, and the observer treats every
        // live unsealed session as unknown. Never drop a user's carts for an unapplied upgrade.
        if ($ready === 'absent') return 'unavailable';
        if ($ready !== true) return false;
        $collected = dimension_session_draft_collect($_SESSION);
        $fingerprint = dimension_session_draft_fingerprint($actor['company'], $actor['user'], $collected);
        $current = db_query('SELECT fingerprint FROM '.TB_PREF.'session_draft_seals WHERE session_id='.$registry_id
            .' AND company_id='.$actor['company'].' AND user_id='.$actor['user']);
        if (!$current) return false;
        $row = db_fetch_row($current);
        if ($row && $row[0] === $fingerprint) return 'unchanged';
        if (!begin_transaction()) return false;
        // Owner rows first: the seal row (lock), then the references, then the masters (shared, ascending).
        $ok = db_query('INSERT INTO '.TB_PREF.'session_draft_seals (session_id,company_id,user_id,state,fingerprint,updated_at) VALUES ('
            .$registry_id.','.$actor['company'].','.$actor['user'].','.db_escape($collected['state']).','.db_escape($fingerprint)
            .',UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE company_id=VALUES(company_id),user_id=VALUES(user_id),state=VALUES(state),'
            .'fingerprint=VALUES(fingerprint),updated_at=VALUES(updated_at)') !== false
            && db_query('DELETE FROM '.TB_PREF.'session_draft_references WHERE session_id='.$registry_id) !== false;
        $identities = array();
        $values = array();
        if ($ok) {
            foreach ($collected['references'] as $reference) {
                $identities[$reference[2]] = $reference[2];
                $values[] = '('.$registry_id.','.db_escape($reference[0]).','.db_escape($reference[1]).','.(int)$reference[2].')';
            }
            if (count($values))
                $ok = db_query('INSERT INTO '.TB_PREF.'session_draft_references (session_id,draft_key,slot,dimension_id) VALUES '
                    .implode(',', $values)) !== false;
        }
        if ($ok && count($identities)) {
            sort($identities);
            $locked = db_query('SELECT id FROM '.TB_PREF.'dimensions WHERE id IN ('.implode(',', $identities).') ORDER BY id LOCK IN SHARE MODE');
            $ok = $locked !== false;
        }
        if (!$ok || !commit_transaction()) { cancel_transaction(); return false; }
        return 'registered';
    } catch (Exception $exception) {
        cancel_transaction();
        return false;
    }
}

/**
 * Seal the current session unknown with one autocommit statement; used after a failed registration so
 * a stale 'complete' seal can never describe carts the session now holds.
 *
 * @return bool
 */
function dimension_session_draft_mark_unknown()
{
    $actor = dimension_session_draft_actor();
    $registry_id = isset($_SESSION['security_session_registry_id']) ? (int)$_SESSION['security_session_registry_id'] : 0;
    if ($actor === false || $registry_id <= 0) return false;
    try {
        return db_query('INSERT INTO '.TB_PREF.'session_draft_seals (session_id,company_id,user_id,state,fingerprint,updated_at) VALUES ('
            .$registry_id.','.$actor['company'].','.$actor['user'].",'unknown','',UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE "
            ."company_id=VALUES(company_id),user_id=VALUES(user_id),state='unknown',fingerprint='',updated_at=VALUES(updated_at)") !== false;
    } catch (Exception $exception) {
        return false;
    }
}

/**
 * End-of-request hook: register the live drafts before the session is written. When the
 * registration cannot be made and the seal cannot be marked unknown either, every draft key is dropped from
 * the session so that no unregistered draft survives the request.
 *
 * @return string|bool Result of the registration
 */
function dimension_session_draft_shutdown()
{
    $result = dimension_session_draft_register();
    if ($result !== false) return $result;
    if (dimension_session_draft_mark_unknown()) return false;
    foreach (array_keys($_SESSION) as $key)
        if (!dimension_session_draft_key_ignored($key)) unset($_SESSION[$key]);
    if (function_exists('session_log_security_event')) session_log_security_event('denied_session_draft_registration_failed');
    return false;
}

/**
 * Drop one session's registration (logout, revocation, expiry cleanup). Dead sessions are ignored by
 * the observer anyway; this only keeps the tables small.
 *
 * @param int $session_id Durable registry identity
 * @return bool
 */
function dimension_session_draft_release($session_id)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0) return false;
    try {
        if (dimension_session_draft_schema_ready() !== true) return false;
        return db_query('DELETE FROM '.TB_PREF.'session_draft_references WHERE session_id='.$session_id) !== false
            && db_query('DELETE FROM '.TB_PREF.'session_draft_seals WHERE session_id='.$session_id) !== false;
    } catch (Exception $exception) {
        return false;
    }
}

/**
 * Locked current read of every live session's draft registration for one Dimension. Must run inside
 * the caller's transaction (repeatable read or serializable) so the shared locks hold until it ends.
 *
 * @param int $id Exact positive native Dimension identity
 * @return array session_drafts => used|unused|unknown
 */
function dimension_session_draft_observe($id)
{
    $unknown = array('session_drafts'=>'unknown');
    $actor = dimension_session_draft_actor();
    if ($actor === false || !dimension_lifecycle_valid_id($id) || empty($GLOBALS['transaction_level'])) return $unknown;
    $id = (int)$id;
    try {
        if (dimension_session_draft_schema_ready() !== true) return $unknown;
        $state = 'unused';
        // The deleter's own request: live in-memory carts, registered or not.
        $own = dimension_session_draft_collect($_SESSION);
        foreach ($own['references'] as $reference) if ((int)$reference[2] === $id) return array('session_drafts'=>'used');
        if ($own['state'] !== 'complete') $state = 'unknown';
        $registry_id = isset($_SESSION['security_session_registry_id']) ? (int)$_SESSION['security_session_registry_id'] : 0;
        if ($registry_id <= 0) $state = 'unknown'; // the actor's own session must be a registered session
        // Every live session of this company: not revoked, not idle or absolutely expired.
        $sessions = db_query('SELECT s.id, d.company_id, d.user_id, d.state, s.user_id FROM '.TB_PREF.'user_sessions s LEFT JOIN '
            .TB_PREF.'session_draft_seals d ON d.session_id=s.id WHERE s.revoked_at IS NULL AND s.idle_expires_at>UTC_TIMESTAMP()'
            .' AND s.absolute_expires_at>UTC_TIMESTAMP() ORDER BY s.id LOCK IN SHARE MODE');
        if (!$sessions) return $unknown;
        $live = array();
        while ($row = db_fetch_row($sessions)) {
            $live[] = (int)$row[0];
            if ($row[3] === null || $row[3] !== 'complete' || (int)$row[1] !== $actor['company'] || (int)$row[2] !== (int)$row[4]) $state = 'unknown';
        }
        if (count($live)) {
            $found = db_query('SELECT 1 FROM '.TB_PREF.'session_draft_references WHERE dimension_id='.$id.' AND session_id IN ('
                .implode(',', $live).') LIMIT 1 LOCK IN SHARE MODE');
            if (!$found) return $unknown;
            if (db_num_rows($found)) return array('session_drafts'=>'used');
        }
        return array('session_drafts'=>$state);
    } catch (Exception $exception) {
        return $unknown;
    }
}
