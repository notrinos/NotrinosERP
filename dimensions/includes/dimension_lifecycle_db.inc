<?php
/** DIM-LFC-001A: authorize and audit non-destructive master lifecycle changes. */

/** Return true only when the current actor has the existing Dimension authority. */
function dimension_lifecycle_authorized()
{
	$user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	return is_object($user) && method_exists($user, 'can_access') && $user->can_access('SA_DIMENSION');
}

/** Accept only an exact positive native master identity, without SQL coercion. */
function dimension_lifecycle_valid_id($id)
{
	if (is_int($id)) return $id > 0;
	return is_string($id) && preg_match('/^[1-9][0-9]*$/D', $id)
		&& (string)(int)$id === $id;
}

/**
 * C01: shared owners whose rows bound to the Dimension's own identity survive a deletion. The reference allocation
 * (refs) stays reserved, so the number is never reused or remapped, and the audit trail is immutable history. Both are
 * retained as they are; nothing here is ever cascaded. Own comments, tag associations and attachments are
 * user content: they stay blockers until their own pages remove them, because a cascade would destroy them.
 *
 * @return string[]
 */
function dimension_lifecycle_delete_retained_evidence()
{
	return array('refs', 'audit_trail');
}

/**
 * C01: shared owners whose own-identity rows travel with the master when only the axis is reassigned. Foreign
 * references to the Dimension in any owner still block.
 *
 * @return string[]
 */
function dimension_lifecycle_reassign_retained_evidence()
{
	return array('refs', 'audit_trail', 'comments', 'attachments', 'tag_associations');
}

/**
 * C01: run the B10 locked, current-read recheck for one lifecycle decision inside the caller's open transaction and
 * remember which owners denied it (names only, never data). Only an exact `unused` aggregate allows the change;
 * used, unknown, a missing master and any SQL failure deny. The recheck leaves the master row locked FOR UPDATE.
 *
 * @param int|string $id Exact positive native Dimension identity
 * @param string[] $tolerate_own Own-identity shared evidence the change retains
 * @return bool True only when every owner family reports unused
 */
function dimension_lifecycle_locked_unused($id, $tolerate_own)
{
	$GLOBALS['dimension_lifecycle_last_denial'] = array('state'=>'unknown', 'owners'=>array());
	if (!dimension_lifecycle_valid_id($id) || empty($GLOBALS['transaction_level'])) return false;
	require_once __DIR__.'/dimension_reference_recheck.inc';
	$decision = dimension_reference_locked_recheck($id, $tolerate_own);
	$blocking = array();
	foreach ($decision['owners'] as $name=>$state) if ($state !== 'unused') $blocking[$name] = $state;
	$GLOBALS['dimension_lifecycle_last_denial'] = array('state'=>$decision['state'], 'owners'=>$blocking);
	return $decision['state'] === 'unused' && !count($blocking) && $decision['deletion_authorized'] === false;
}

/**
 * C01: owners that denied the latest delete/reassignment decision of this request.
 *
 * @return array state (used|unknown|unused) and owners name=>state; empty owners when allowed or not decided
 */
function dimension_lifecycle_last_denial()
{
	return isset($GLOBALS['dimension_lifecycle_last_denial']) ? $GLOBALS['dimension_lifecycle_last_denial']
		: array('state'=>'unknown', 'owners'=>array());
}

/**
 * C01: does any shared store still hold evidence bound to this Dimension key? Used right after a new master insert,
 * where the answer must be no: a deleted master leaves its reference allocation and audit rows, and a legacy server
 * that recycles the highest auto-increment value must not attach that history to a new Dimension.
 *
 * @param int|string $id Identity returned by the insert
 * @return bool True when the key is retained (or the check cannot be answered)
 */
function dimension_lifecycle_key_retained($id)
{
	if (!dimension_lifecycle_valid_id($id)) return true;
	require_once __DIR__.'/dimension_shared_record_authority.inc';
	foreach (array('attachments', 'audit_trail', 'comments', 'refs') as $table) {
		$predicate = dimension_shared_record_identity_predicate($table, $id);
		if (!is_string($predicate)) return true;
		$rows = db_query('SELECT 1 FROM '.TB_PREF.$table.' WHERE '.$predicate.' LIMIT 1 FOR UPDATE');
		if (!$rows || db_num_rows($rows)) return true;
	}
	$rows = db_query('SELECT 1 FROM '.TB_PREF.'tag_associations ta INNER JOIN '.TB_PREF.'tags t ON t.id=ta.tag_id WHERE t.type='
		.(int)TAG_DIMENSION.' AND ta.record_id='.db_escape((string)(int)$id).' LIMIT 1 FOR UPDATE');
	return !$rows || db_num_rows($rows) > 0;
}

/**
 * C01: leave a lifecycle helper after a pure denial, before it wrote anything. A helper that owns the outer transaction
 * rolls it back. A helper called inside the caller's transaction only releases its own nested level, so a refusal never
 * discards the caller's earlier work (locks taken by the recheck stay until the caller ends its transaction). A database
 * error (lock timeout, deadlock) may already have ended the server transaction, so it always rolls back.
 *
 * @return bool Always false
 */
function dimension_lifecycle_deny()
{
	if (!empty($GLOBALS['transaction_level']) && $GLOBALS['transaction_level'] > 1 && !db_error_no()) commit_transaction();
	else cancel_transaction();
	return false;
}

/** Verify an audit append inside the caller's locked master transaction. */
function dimension_lifecycle_audit($id, $action, $reference)
{
	if (!dimension_lifecycle_authorized() || !function_exists('add_audit_trail') || !function_exists('Today')) return false;
	$description = substr('Dimension '.$action.' '.$reference, 0, 60);
	$predicate = 'type='.ST_DIMENSION.' AND trans_no='.(int)$id;
	$before = db_query('SELECT COALESCE(MAX(id),0) FROM '.TB_PREF.'audit_trail WHERE '.$predicate.' FOR UPDATE');
	$before_row = $before ? db_fetch_row($before) : false;
	if (!$before_row) return false;
	try {
		add_audit_trail(ST_DIMENSION, (int)$id, Today(), $description);
	} catch (Exception $exception) {
		return false;
	}
	$after = db_query('SELECT COALESCE(MAX(id),0) FROM '.TB_PREF.'audit_trail WHERE '.$predicate
		.' AND description='.db_escape($description).' AND user='.(int)$_SESSION['wa_current_user']->user.' AND gl_seq=0 FOR UPDATE');
	$after_row = $after ? db_fetch_row($after) : false;
	return $after_row && (int)$after_row[0] > (int)$before_row[0];
}
