<?php
$page_security = 'SA_HRM_ESS_MANAGE_PROXY';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once(__DIR__.'/_common.inc');
page(_('ESS Delegation'));
$context = hrm_ess_002_require_self();
if (!$context) {
    display_error(_('Self scope unavailable.'));
    end_page();
    exit;
}
$message = null;
$is_error = false;
if (isset($_POST['grant_proxy']) || isset($_POST['revoke_proxy'])) {
    if (!hrm_ess_002_route_post_valid()) {
        $message = _('The delegation action was rejected by CSRF protection.');
        $is_error = true;
    } else {
        $error = null;
        $reason = isset($_POST['reason_sha256']) ? $_POST['reason_sha256'] : '';
        if (isset($_POST['grant_proxy'])) {
            $delegation_id = isset($_POST['delegation_id']) ? (int)$_POST['delegation_id'] : 0;
            $legal_entity_id = isset($_POST['legal_entity_id']) ? (int)$_POST['legal_entity_id'] : 0;
            $capability = isset($_POST['capability']) ? $_POST['capability'] : '';
            $from = isset($_POST['effective_from']) ? $_POST['effective_from'] : '';
            $to = isset($_POST['effective_to']) ? $_POST['effective_to'] : '';
            $result = hrm_ess_002_grant_proxy_capability($delegation_id, $legal_entity_id, $capability, $from, $to, $reason, $error);
        } else {
            $capability_id = isset($_POST['capability_id']) ? (int)$_POST['capability_id'] : 0;
            $result = hrm_ess_002_revoke_proxy_capability($capability_id, $reason, $error);
        }
        $message = $result === false ? hrm_ess_002_safe_error($error) : _('Delegation custody recorded.');
        $is_error = $result === false;
    }
}
hrm_ess_002_emit_shell_start(_('ESS Delegation'), 'proxy');
if ($message !== null)
    hrm_ess_002_ui_status($message, $is_error);
echo '<p>'._('This layer narrows an already-approved ESS-001 direct-team delegation. It never expands the delegator’s authority, bypasses effective dates, or grants payroll/accounting authority.').'</p>';
echo '<h2>'._('Grant capability').'</h2><form class="ess2-form" method="post">';
echo '<input type="hidden" name="_token" value="'.hrm_ess_002_h(ensure_csrf_token()).'">';
echo '<label>'._('ESS-001 delegation ID').'<input name="delegation_id" inputmode="numeric" required></label>';
echo '<label>'._('Legal Entity ID').'<input name="legal_entity_id" inputmode="numeric" required></label>';
echo '<label>'._('Capability').'<select name="capability" required>';
foreach (hrm_ess_002_proxy_capabilities() as $capability)
    echo '<option value="'.hrm_ess_002_h($capability).'">'.hrm_ess_002_h(hrm_ess_002_capability_label($capability)).'</option>';
echo '</select></label><label>'._('Effective from').'<input type="date" name="effective_from" required></label>';
echo '<label>'._('Effective to').'<input type="date" name="effective_to" required></label>';
echo '<label>'._('Reason evidence SHA-256').'<input name="reason_sha256" maxlength="64" pattern="[a-fA-F0-9]{64}" required></label>';
echo '<button name="grant_proxy" value="1">'._('Grant narrowed capability').'</button></form>';
echo '<h2>'._('Revoke capability').'</h2><form class="ess2-form" method="post">';
echo '<input type="hidden" name="_token" value="'.hrm_ess_002_h(ensure_csrf_token()).'">';
echo '<label>'._('Capability ID').'<input name="capability_id" inputmode="numeric" required></label>';
echo '<label>'._('Reason evidence SHA-256').'<input name="reason_sha256" maxlength="64" pattern="[a-fA-F0-9]{64}" required></label>';
echo '<button name="revoke_proxy" value="1">'._('Revoke capability').'</button></form>';
hrm_ess_002_emit_shell_end();
end_page();
?>
