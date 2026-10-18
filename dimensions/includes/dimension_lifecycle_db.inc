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
