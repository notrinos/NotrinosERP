<?php
/**
 * Authorized authenticated-session inventory and revocation console.
 */
$page_security = 'SA_USERS';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

page(_($help_context = 'Authenticated Sessions'));
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');

/**
 * Render a UTC database timestamp without exposing fingerprint material.
 *
 * @param string|null $value
 * @return string
 */
function session_inventory_timestamp($value)
{
	if (!$value)
		return '';
	return htmlspecialchars($value.' UTC');
}

/**
 * Return whether an unrevoked inventory record is already unusable by time.
 *
 * @param array $row
 * @param int|null $now
 * @return bool
 */
function session_inventory_is_expired($row, $now = null)
{
	$now = isset($now) ? (int)$now : time();
	$idle_expiry = strtotime($row['idle_expires_at'].' UTC');
	$absolute_expiry = strtotime($row['absolute_expires_at'].' UTC');
	return $idle_expiry === false || $absolute_expiry === false
		|| $now >= $idle_expiry || $now >= $absolute_expiry;
}

$revoke_session_id = find_submit('RevokeSession');
$revoke_user_id = find_submit('RevokeUser');
if (($revoke_session_id > 0 || $revoke_user_id > 0) && check_csrf_token()) {
	$actor_id = (int)$_SESSION['wa_current_user']->user;
	if ($revoke_session_id > 0) {
		if (session_registry_revoke_session($revoke_session_id, $actor_id, 'admin_revoke'))
			display_notification(_('The selected session has been revoked.'));
		else
			display_error(_('The selected session could not be revoked.'));
	}
	else {
		if (session_registry_revoke_user_sessions($revoke_user_id, $actor_id, 'admin_user_revoke'))
			display_notification(_('All active sessions for the selected user have been revoked.'));
		else
			display_error(_('The selected user sessions could not be revoked.'));
	}
}

start_form();
display_note(_('Showing up to 500 active and most recently seen session records.'));
start_table(TABLESTYLE);
$headings = array(
	_('Session'),
	_('User'),
	_('Authentication'),
	_('Assurance'),
	_('Created'),
	_('Last seen'),
	_('Idle expiry'),
	_('Absolute expiry'),
	_('Status'),
	'',
	'',
);
table_header($headings);

$result = session_registry_get_inventory();
$row_count = 0;
$row_color = 0;
while ($result && ($row = db_fetch_assoc($result))) {
	$row_count++;
	alt_table_row_color($row_color);
	$is_current = isset($_SESSION['security_session_registry_id'])
		&& (int)$_SESSION['security_session_registry_id'] === (int)$row['id'];
	$login = $row['login_id'] !== '' ? $row['login_id'] : _('Deleted user').' #'.(int)$row['user_id'];
	$is_expired = $row['revoked_at'] === null && session_inventory_is_expired($row);
	$status = $row['revoked_at'] !== null
		? _('Revoked').' - '.htmlspecialchars($row['revocation_reason'])
		: ($is_expired ? _('Expired') : _('Active'));

	label_cell((int)$row['id'].($is_current ? ' ('._('current').')' : ''));
	label_cell(htmlspecialchars($login));
	label_cell(htmlspecialchars($row['auth_method']));
	label_cell((int)$row['assurance_level']);
	label_cell(session_inventory_timestamp($row['created_at']));
	label_cell(session_inventory_timestamp($row['last_seen_at']));
	label_cell(session_inventory_timestamp($row['idle_expires_at']));
	label_cell(session_inventory_timestamp($row['absolute_expires_at']));
	label_cell($status);
	if ($row['revoked_at'] === null && !$is_expired)
		submit_cells('RevokeSession'.(int)$row['id'], _('Revoke'), '', _('Revoke this session'), false);
	else
		label_cell('');
	if ($row['revoked_at'] === null && !$is_expired)
		submit_cells('RevokeUser'.(int)$row['user_id'], _('Revoke user sessions'), '', _('Revoke every active session for this user'), false);
	else
		label_cell('');
	end_row();
}

if ($row_count === 0) {
	start_row();
	label_cell(_('No authenticated sessions have been registered.'), 'colspan="11" align="center"');
	end_row();
}
end_table(1);
end_form();
end_page();
