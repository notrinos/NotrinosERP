<?php
/**********************************************************************
    PAY-SEC-004 explicit first-link administration page.
***********************************************************************/
$page_security = 'SA_USERS';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/federation_first_link_administration.inc');

page(_($help_context = 'Federated Identity Links'));

$decision_value = get_post('verifier_decision_id', '');
$user_value = get_post('federation_user_id', '');

if (isset($_POST['CreateFederationLink']) && check_csrf_token()) {
    $request = federation_first_link_administration_request($decision_value, $user_value);
    if ($request === false) {
        display_error(_('Enter a valid numeric link request ID and explicitly select one active local user.'));
    } else {
        $result = federation_first_link_administration_create(
            user_company(),
            $request['verifier_decision_id'],
            $request['user_id']
        );
        if (is_array($result) && isset($result['external_subject_link_id'])) {
            display_notification_centered(
                _('The explicit federated identity link has been created. The user must start Federated login again; this page does not create or adopt a login session.')
            );
            $decision_value = '';
            $user_value = '';
        } else {
            display_error(_('The federated identity link could not be created. Recheck the request ID, active user/role, provider/configuration state, decision expiry, and collision/deprovision status. No automatic relink was attempted.'));
        }
    }
}

start_form();
start_table(TABLESTYLE2);
table_section_title(_('Explicit first-link creation'));
text_row(_('Link request ID:'), 'verifier_decision_id', $decision_value, 20, 20);

$options = federation_first_link_administration_user_options();
echo '<tr><td class="label">'._('Local user:').'</td><td>';
if (is_array($options) && count($options) > 0) {
    echo array_selector('federation_user_id', $user_value, $options, array(
        'spec_option'=>_('Select active local user'),
        'spec_id'=>'',
        'async'=>false
    ));
} else {
    echo htmlspecialchars(_('No active local users are available.'));
}
echo '</td></tr>';

label_row(_('Linking policy:'), _('Use only the numeric request shown to the federated user and explicitly select the intended existing local account. Do not infer or match accounts by email, display name, login profile claim, or other identity-provider profile data.'));
end_table(1);
if (is_array($options) && count($options) > 0)
    submit_center('CreateFederationLink', _('Create explicit federation link'), true, '', 'default');
end_form();

end_page();
