<?php
/** PAY-SEC-004 authenticated recovery-material re-enrollment/rotation route. */
$page_security = 'SA_CHGPASSWD';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/account_recovery_material.inc');
include_once($path_to_root.'/includes/account_recovery_material_route.inc');

account_recovery_material_route_security_headers();
page(_($help_context = 'Recovery material'));

$company = function_exists('user_company') ? (int)user_company() : -1;
$actor_id = account_recovery_material_current_actor_id();
$rotation_result = false;
$route_ready = $company >= 0 && $actor_id > 0 && account_recovery_material_route_activation_ready();

if (!$route_ready) {
    display_error(_('Recovery material management is not active for this company. Complete the required Software Upgrade before using this page.'));
} elseif ((isset($_POST['ROTATE_RECOVERY_MATERIAL']) || isset($_POST['REVOKE_RECOVERY_MATERIAL']))) {
    $route_token = isset($_POST['recovery_material_csrf']) ? (string)$_POST['recovery_material_csrf'] : '';
    $csrf_ok = check_csrf_token()
        && account_recovery_material_route_consume_csrf($company, $actor_id, $route_token, time());
    if (!$csrf_ok) {
        display_error(_('Recovery material request was not accepted. Reload this page and try again.'));
    } elseif ($SysPrefs->allow_demo_mode) {
        display_warning(_('Recovery material cannot be changed in demo mode.'));
    } elseif (isset($_POST['ROTATE_RECOVERY_MATERIAL'])) {
        $confirmed = isset($_POST['confirm_rotation']) && (string)$_POST['confirm_rotation'] === '1';
        $current_password = isset($_POST['current_password']) ? (string)$_POST['current_password'] : '';
        if (!$confirmed) {
            display_error(_('Confirm that rotating recovery material revokes every previous active recovery code.'));
        } else {
            $rotation_result = account_recovery_material_rotate_current_user($company, $current_password, time());
            if (!is_array($rotation_result)) {
                display_error(_('Recovery material could not be rotated. Verify the current local password and retry after the rotation cooldown.'));
            } else {
                display_notification(_('Recovery material was rotated. Save every ID and recovery code now; this plaintext is shown only in this response.'));
            }
        }
    } else {
        $confirmed = isset($_POST['confirm_revocation']) && (string)$_POST['confirm_revocation'] === '1';
        if (!$confirmed) {
            display_error(_('Confirm that all active recovery material should be revoked.'));
        } else {
            $revoked = account_recovery_material_revoke_for_user($company, $actor_id, 'self_revoked', time());
            if (!is_array($revoked))
                display_error(_('Recovery material could not be revoked securely.'));
            else if ((int)$revoked['revoked_count'] === 0)
                display_notification(_('No active recovery material remained to revoke.'));
            else
                display_notification(sprintf(_('%d active recovery code(s) were revoked.'), (int)$revoked['revoked_count']));
        }
    }
}

if (is_array($rotation_result) && isset($rotation_result['materials']) && is_array($rotation_result['materials'])) {
    display_warning(_('These recovery codes are not stored in plaintext and cannot be displayed again. If this response is interrupted or you cannot save the complete set, wait for the rotation cooldown and perform a fresh rotation; the next successful rotation revokes this unseen set.'));
    start_table(TABLESTYLE);
    table_header(array(_('Recovery code ID'), _('Recovery code')));
    foreach ($rotation_result['materials'] as $material) {
        if (!is_array($material) || !isset($material['recovery_material_id'], $material['secret']))
            continue;
        start_row();
        label_cell((string)(int)$material['recovery_material_id']);
        label_cell('<code>'.account_recovery_material_route_escape($material['secret']).'</code>');
        end_row();
    }
    end_table(1);
}

if ($route_ready) {
    $route_csrf = account_recovery_material_route_issue_csrf($company, $actor_id, time());
    if (!is_string($route_csrf)) {
        display_error(_('Recovery material action token could not be created. No recovery material action is available in this response.'));
    } else {
        display_note(_('Recovery material is available only after signing in. After an account-recovery password reset, sign in with the new password before creating a new set.'), 0, 1);

        start_form(false, '', 'recovery_material_rotate_form');
        start_table(TABLESTYLE2);
        table_section_title(_('Rotate recovery material'));
        label_row(_('Effect:'), _('Revokes all currently active recovery codes and creates a complete new set.'));
        password_row(_('Current local password:'), 'current_password', '');
        check_row(_('I understand previous active recovery codes will be revoked.'), 'confirm_rotation', 0, false);
        hidden('recovery_material_csrf', $route_csrf);
        end_table(1);
        submit_center('ROTATE_RECOVERY_MATERIAL', _('Rotate recovery codes'), true, '', 'nonajax');
        end_form(1);

        start_form(false, '', 'recovery_material_revoke_form');
        start_table(TABLESTYLE2);
        table_section_title(_('Revoke recovery material'));
        label_row(_('Effect:'), _('Revokes all active recovery codes without issuing replacements.'));
        check_row(_('I understand account recovery will remain unavailable until a later authorized rotation.'), 'confirm_revocation', 0, false);
        hidden('recovery_material_csrf', $route_csrf);
        end_table(1);
        submit_center('REVOKE_RECOVERY_MATERIAL', _('Revoke all recovery codes'), true, '', 'nonajax');
        end_form();
    }
}

end_page();
