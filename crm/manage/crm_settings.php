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
/**
 * CRM Module Settings
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_SETTINGS';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');

page(_($help_context = 'CRM Settings'));

//--------------------------------------------------------------------------

if (isset($_POST['Save'])) {
    $settings_to_save = array(
        'auto_create_contact',
        'carry_forward_communication',
        'auto_close_days',
        'lead_ref_prefix',
        'opportunity_ref_prefix',
        'contract_ref_prefix',
        'default_probability',
        'enable_lead_scoring',
        'assignment_mode',
    );

    begin_transaction();
    foreach ($settings_to_save as $key) {
        if (in_array($key, array('auto_create_contact', 'carry_forward_communication', 'enable_lead_scoring'))) {
            set_crm_setting($key, check_value($key) ? '1' : '0');
        } else {
            set_crm_setting($key, get_post($key, ''));
        }
    }
    commit_transaction();

    display_notification(_('CRM settings have been saved.'));
}

//--------------------------------------------------------------------------
// Load settings
//--------------------------------------------------------------------------

$all_settings = get_all_crm_settings();

/**
 * Get a setting value with default fallback.
 *
 * @param string $key
 * @param string $default
 * @return string
 */
function _setting_val($key, $default = '')
{
    global $all_settings;
    return isset($all_settings[$key]) ? $all_settings[$key]['setting_value'] : $default;
}

//--------------------------------------------------------------------------

start_form();

display_heading(_('General Settings'));

start_table(TABLESTYLE2);

text_row(_('Lead Ref Prefix:'), 'lead_ref_prefix',
    _setting_val('lead_ref_prefix', 'LD'), 10, 10);

text_row(_('Opportunity Ref Prefix:'), 'opportunity_ref_prefix',
    _setting_val('opportunity_ref_prefix', 'OP'), 10, 10);

text_row(_('Contract Ref Prefix:'), 'contract_ref_prefix',
    _setting_val('contract_ref_prefix', 'CT'), 10, 10);

text_row(_('Default Probability (%):'), 'default_probability',
    _setting_val('default_probability', '10'), 10, 3);

text_row(_('Auto-Close Stale Opportunities (days, 0=disabled):'), 'auto_close_days',
    _setting_val('auto_close_days', '90'), 10, 5);

end_table(1);

display_heading(_('Automation'));

start_table(TABLESTYLE2);

check_row(_('Auto-create Contact for New Leads:'), 'auto_create_contact',
    _setting_val('auto_create_contact', '1'));

check_row(_('Carry Forward Communication on Conversion:'), 'carry_forward_communication',
    _setting_val('carry_forward_communication', '1'));

check_row(_('Enable Lead Scoring:'), 'enable_lead_scoring',
    _setting_val('enable_lead_scoring', '0'));

$modes = array('manual' => _('Manual'), 'auto' => _('Auto (Round-Robin)'));
$current_mode = _setting_val('assignment_mode', 'manual');
$mode_html = "<select name='assignment_mode'>";
foreach ($modes as $key => $label) {
    $sel = ($current_mode === $key) ? ' selected' : '';
    $mode_html .= "<option value='$key'$sel>" . htmlspecialchars($label) . "</option>";
}
$mode_html .= "</select>";
label_row(_('Lead Assignment Mode:'), $mode_html);

end_table(1);

submit_center('Save', _('Save Settings'), true, '', 'default');

end_form();
end_page();

