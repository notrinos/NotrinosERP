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

function add_user($login_id, $real_name, $password, $phone, $email, $role_id, $language, $profile, $rep_popup, $pos) {
	$sql = "INSERT INTO ".TB_PREF."users (login_id, real_name, password"
		.", phone, email, role_id, language, pos, print_profile, rep_popup)
		VALUES (".db_escape($login_id).", 
		".db_escape($real_name).", ".db_escape($password).",".db_escape($phone)
		.",".db_escape($email).", ".db_escape($role_id).", ".db_escape($language)
		.", ".db_escape($pos).",".db_escape($profile).",".db_escape($rep_popup)
		." )";

	return db_query($sql, 'could not add user for '.$login_id);
}

function update_user_password($id, $login_id, $password) {
	if (function_exists('session_registry_table_exists')
		&& session_registry_table_exists()) {
		$actor_id = isset($_SESSION['wa_current_user']->user)
			? (int)$_SESSION['wa_current_user']->user : 0;
		if (!session_registry_revoke_user_sessions((int)$id, $actor_id, 'password_changed'))
			return false;
	}
	$sql = "UPDATE ".TB_PREF."users SET password=".db_escape($password).",login_id = ".db_escape($login_id)." WHERE id=".db_escape($id);

	$result = db_query($sql, 'could not update user password for '.$login_id);
	return $result;
}

/** Load the PAY-SEC-004 recovery/audit dependencies for administrator reset. */
function administrator_password_reset_load_dependencies() {
	global $path_to_root;
	if (!function_exists('account_recovery_material_active_ids_for_update')) {
		$root = isset($path_to_root) ? realpath($path_to_root) : false;
		if ($root === false)
			return false;
		$file = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'account_recovery_material.inc';
		if (!is_file($file))
			return false;
		require_once($file);
	}
	if (!function_exists('account_recovery_material_load_audit_gateway')
		|| !account_recovery_material_load_audit_gateway())
		return false;
	return function_exists('account_recovery_material_active_ids_for_update')
		&& function_exists('account_recovery_material_update_active_revocation')
		&& function_exists('account_recovery_material_revocation_audit')
		&& function_exists('session_registry_table_exists')
		&& function_exists('session_registry_revoke_user_sessions')
		&& function_exists('append_domain_audit_event');
}

/** Restricted PAY-AUD evidence for one committed SA_USERS credential reset. */
function administrator_password_reset_audit($company, $user_id, $actor_id, $revoked_material_count) {
	return append_domain_audit_event(array(
		'company_id'=>(int)$company,'legal_entity_id'=>0,
		'entity_type'=>'security_user_credential_reset',
		'entity_key'=>array('user_id'=>(int)$user_id),
		'action'=>'security.user.credential.administrator_reset',
		'reason_code'=>'pay_sec_004_administrator_password_reset_transaction',
		'before'=>array('credential_reset_committed'=>false),
		'after'=>array('credential_reset_committed'=>true),
		'restricted_diff'=>array(
			'actor_id'=>(int)$actor_id,'user_id'=>(int)$user_id,
			'authorization'=>'SA_USERS','password_policy'=>'existing_admin_policy',
			'recovery_material_revoked'=>(int)$revoked_material_count,
			'durable_sessions_revoked'=>true,'federated_browser_sessions_invalidated'=>true,
			'privileged_assurance_invalidated_by_session_revocation'=>true,
			'post_reset_reenrollment_required'=>true,'federation_links_mutated'=>false,
			'assurance_history_rewritten'=>false,'plaintext_password_audited'=>false,
			'password_hash_audited'=>false,'recovery_secret_audited'=>false,
			'public_recovery_route_added'=>false,'anonymous_account_lookup_added'=>false,
			'break_glass_authority_added'=>false,'role_granted'=>false,
			'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
			'accounting_authority_changed'=>false))) !== false;
}

/**
 * Reset one active user's local password under existing SA_USERS authority.
 *
 * This is the live administrator-reset transaction boundary. It intentionally
 * accepts no recovery secret and exposes no anonymous lookup/oracle. Recovery
 * material revocation, durable-session invalidation, restricted PAY-AUD and
 * the credential mutation commit atomically. Federation links and immutable
 * assurance history are preserved; the revoked durable sessions make existing
 * federated browser/privileged-assurance state unusable.
 */
function administrator_reset_user_password($company, $id, $login_id, $password, $now_epoch = null) {
	$company = (int)$company;
	$id = (int)$id;
	$actor_id = isset($_SESSION['wa_current_user']->user)
		? (int)$_SESSION['wa_current_user']->user : 0;
	$now = $now_epoch === null ? time() : $now_epoch;
	if ($company < 0 || $id <= 0 || $actor_id <= 0 || !is_int($now) || $now <= 0
		|| !is_string($login_id) || strlen($login_id) < 4 || strlen($login_id) > 60
		|| !is_string($password) || strlen($password) < 4 || strlen($password) > 512
		|| strstr($password, $login_id) !== false
		|| !function_exists('user_check_access') || !user_check_access('SA_USERS')
		|| (function_exists('user_company') && (int)user_company() !== $company)
		|| !administrator_password_reset_load_dependencies()
		|| !function_exists('begin_transaction') || !function_exists('commit_transaction')
		|| !function_exists('cancel_transaction'))
		return false;
	if (!session_registry_table_exists())
		return false;
	$password_hash = password_hash($password, PASSWORD_DEFAULT);
	if (!is_string($password_hash) || $password_hash === '')
		return false;

	begin_transaction();
	$committed = false;
	$result_value = false;
	do {
		$locked = db_query(
			"SELECT u.id,u.login_id,u.inactive user_inactive,r.inactive role_inactive "
			."FROM ".TB_PREF."users u JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
			."WHERE u.id=".$id." FOR UPDATE",
			'could not lock user for PAY-SEC-004 administrator password reset'
		);
		if (!$locked || db_num_rows($locked) !== 1)
			break;
		$user = db_fetch_assoc($locked);
		if (!is_array($user) || (int)$user['user_inactive'] !== 0 || (int)$user['role_inactive'] !== 0
			|| (string)$user['login_id'] !== $login_id)
			break;

		$active_ids = account_recovery_material_active_ids_for_update($id);
		if (!is_array($active_ids))
			break;
		$revoked_material_count = count($active_ids);
		if ($revoked_material_count > 0) {
			$now_at = gmdate('Y-m-d H:i:s', $now);
			if (!account_recovery_material_update_active_revocation($id, $actor_id, 'credential_reset', $now_at)
				|| !account_recovery_material_revocation_audit($company, $id, $actor_id, 'credential_reset', $revoked_material_count))
				break;
		}
		if (!session_registry_revoke_user_sessions($id, $actor_id, 'administrator_password_reset'))
			break;
		if (!administrator_password_reset_audit($company, $id, $actor_id, $revoked_material_count))
			break;
		$sql = "UPDATE ".TB_PREF."users SET password=".db_escape($password_hash)
			." WHERE id=".$id." AND login_id=".db_escape($login_id);
		if (!db_query($sql, 'could not commit PAY-SEC-004 administrator password reset')
			|| !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
			break;
		commit_transaction();
		$committed = true;
		$result_value = array('user_id'=>$id,'recovery_material_revoked'=>$revoked_material_count,
			'durable_sessions_revoked'=>true,'credential_reset_committed'=>true,
			'post_reset_reenrollment_required'=>true,'federation_links_mutated'=>false,
			'assurance_history_rewritten'=>false);
	} while (false);
	if (!$committed) {
		cancel_transaction();
		return false;
	}
	return $result_value;
}

function update_user($id, $login_id, $real_name, $phone, $email, $role_id, $language, $profile, $rep_popup, $pos) {
	$sql = "UPDATE ".TB_PREF."users SET real_name=".db_escape($real_name).
	", phone=".db_escape($phone).",
		email=".db_escape($email).",
		role_id=".db_escape($role_id).",
		language=".db_escape($language).",
		print_profile=".db_escape($profile).",
		rep_popup=".db_escape($rep_popup).",
		pos=".db_escape($pos).",
		login_id = " . db_escape($login_id)
		. " WHERE id=" . db_escape($id);
	return db_query($sql, 'could not update user for '.$login_id);
}

function update_user_prefs($id, $prefs) {
	$security_context_changed = false;
	if (isset($prefs['role_id']) || isset($prefs['inactive'])) {
		$result = db_query(
			"SELECT role_id, inactive FROM ".TB_PREF."users WHERE id=".db_escape($id),
			'could not verify user security context for '.$id
		);
		if (!$result || db_num_rows($result) !== 1)
			return false;
		$current = db_fetch($result);
		$security_context_changed = (isset($prefs['role_id'])
			&& (int)$prefs['role_id'] !== (int)$current['role_id'])
			|| (isset($prefs['inactive'])
			&& (int)$prefs['inactive'] !== (int)$current['inactive']);
	}
	if ($security_context_changed && function_exists('session_registry_table_exists')
		&& session_registry_table_exists()) {
		$actor_id = isset($_SESSION['wa_current_user']->user)
			? (int)$_SESSION['wa_current_user']->user : 0;
		if (!session_registry_revoke_user_sessions((int)$id, $actor_id, 'role_or_status_changed'))
			return false;
	}
	$sql = "UPDATE ".TB_PREF."users SET ";
	foreach($prefs as $name => $value) {
		$prefs[$name] = $name.'='.db_escape($value);
	}
	$sql .= implode(',', $prefs)." WHERE id=".db_escape($id);

	$result = db_query($sql, 'could not update user display prefs for '.$id);
	return $result;
}

/**
 * Update one user's active status after revoking every authenticated session.
 *
 * @param int $id
 * @param bool $inactive
 * @return bool
 */
function update_user_inactive($id, $inactive) {
	if (function_exists('session_registry_table_exists')
		&& session_registry_table_exists()) {
		$actor_id = isset($_SESSION['wa_current_user']->user)
			? (int)$_SESSION['wa_current_user']->user : 0;
		if (!session_registry_revoke_user_sessions((int)$id, $actor_id, 'status_changed'))
			return false;
	}
	$sql = "UPDATE ".TB_PREF."users SET inactive=".((int)(bool)$inactive)
		." WHERE id=".db_escape($id);
	return db_query($sql, 'could not update user status for '.$id);
}

function get_users($all=false) {
	$sql = "SELECT u.*, r.role FROM ".TB_PREF."users u, ".TB_PREF."security_roles r
		WHERE u.role_id=r.id";
	if (!$all) $sql .= " AND !u.inactive";
	
	return db_query($sql, 'could not get users');
}

function get_user($id) {
	$sql = "SELECT * FROM ".TB_PREF."users WHERE id=".db_escape($id);

	$result = db_query($sql, 'could not get user '.$id);

	return db_fetch($result);
}

function get_user_by_login($login_id) {
	$sql = "SELECT * FROM ".TB_PREF."users WHERE login_id=".db_escape($login_id);

	$result = db_query($sql, 'could not get user '.$login_id);

	return db_fetch($result);
}

function get_user_by_email($email) {
	$sql = "SELECT * FROM ".TB_PREF."users WHERE email=".db_escape($email);

	$result = db_query($sql, 'could not get user for email '.$email);

	if (db_num_rows($result) != 1)
		return false;

	return db_fetch($result);
}

function delete_user($id) {
	if (function_exists('session_registry_table_exists')
		&& session_registry_table_exists()) {
		$actor_id = isset($_SESSION['wa_current_user']->user)
			? (int)$_SESSION['wa_current_user']->user : 0;
		if (!session_registry_revoke_user_sessions((int)$id, $actor_id, 'user_deleted'))
			return false;
	}
	$sql = "DELETE FROM ".TB_PREF."users WHERE id=".db_escape($id);

	return db_query($sql, 'could not delete user '.$id);
}

function authenticate_user($login_id, $password) {
	
	$sql1 = "SELECT password FROM ".TB_PREF."users WHERE login_id = ".db_escape($login_id);

	$result = db_query($sql1, 'could not get user login for '.$login_id);

	if(db_num_rows($result) == 0)
		return false;

	$hash = db_fetch($result)[0];

	// The user's password hash may have been created long ago by md5 password algorithm on the old NotrinosERP versions
	if(password_verify($password, $hash) || $hash == md5($password)) {
		if(password_needs_rehash($hash, PASSWORD_DEFAULT) === true) {
			$new_hash = password_hash($password, PASSWORD_DEFAULT);
			$sql2 = "UPDATE ".TB_PREF."users SET password = ".db_escape($new_hash)." WHERE login_id = ".db_escape($login_id);
			db_query($sql2, 'could not update password hash for '.$login_id);
		}

		return true;
	}

	return false;
}

function update_user_visitdate($login_id) {
	$sql = "UPDATE ".TB_PREF."users SET last_visit_date='". date("Y-m-d H:i:s") ."'
		WHERE login_id=".db_escape($login_id);

	db_query($sql, 'could not update last visit date for user '.$login_id);
}

function check_user_activity($id)  {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."audit_trail WHERE audit_trail.user="
		. db_escape($id);
	$result = db_query($sql, 'Cannot check user activity');
	$ret = db_fetch($result);

	return $ret[0];
}

function show_users_online() {
	global $db, $GetText, $SysPrefs;

	if (!isset($SysPrefs->show_users_online) || $SysPrefs->show_users_online == 0 || !defined('TB_PREF') || !isset($GetText) || !isset($db))
		return '';
	$result = db_query("SHOW TABLES LIKE '".TB_PREF."useronline'");	
	if (db_num_rows($result) == 1) {
		$timeoutseconds = 120;

		$timestamp=time();
		$timeout=$timestamp-$timeoutseconds;
		/*
		This will find out if user is from behind proxy server. 
		In that case, the script would count them all as 1 user.
		This function tryes to get real IP address.
		*/
		if (isset($_SERVER['HTTP_CLIENT_IP']))
			$ip = $_SERVER['HTTP_CLIENT_IP'];
		elseif (isset($_SERVER['HTTP_X_FORWARDED_FOR']))
			$ip = $_SERVER['HTTP_X_FORWARDED_FOR'];
		elseif (isset($_SERVER['HTTP_X_FORWARDED']))
			$ip = $_SERVER['HTTP_X_FORWARDED'];
		elseif (isset($_SERVER['HTTP_FORWARDED_FOR']))
			$ip = $_SERVER['HTTP_FORWARDED_FOR'];
		elseif (isset($_SERVER['HTTP_FORWARDED']))
			$ip = $_SERVER['HTTP_FORWARDED'];
		else
			$ip = $_SERVER['REMOTE_ADDR'];

		// Add user to database
		db_query("INSERT INTO ".TB_PREF."useronline (timestamp, ip, file) VALUES ('". $timestamp ."',". db_escape($ip) .",". db_escape($_SERVER['PHP_SELF']) .")");
		//Remove users that were not online within $timeoutseconds.
		db_query("DELETE FROM ".TB_PREF."useronline WHERE timestamp<". $timeout);

		// Select online users
		$result = db_query("SELECT DISTINCT ip FROM ".TB_PREF."useronline");
		$users = db_num_rows($result);
	}
	else
		$users = 1;
	return ' - '."$users ".($users == 1 ? _('user online') : _('users online'));
}
