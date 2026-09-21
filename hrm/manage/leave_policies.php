<?php
/**********************************************************************
    HRM-LVE-001 governed, append-only leave policy administration.
***********************************************************************/
$page_security = 'SA_HRM_MANAGE_LEAVE_POLICY';
$path_to_root = '../..';
include($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/leave_type_db.inc');
include_once($path_to_root . '/hrm/includes/db/hrm_lve_001_governance_db.inc');

$js = user_use_date_picker() ? get_js_date_picker() : '';
page(_('Governed Leave Policies'), false, false, '', $js);
simple_page_mode(false);

function lve1_ui_optional_int($name)
{
    if (!isset($_POST[$name]) || trim((string)$_POST[$name]) === '')
        return null;
    $value = (int)$_POST[$name];
    return $value > 0 ? $value : null;
}

function lve1_ui_optional_text($name)
{
    if (!isset($_POST[$name]))
        return null;
    $value = trim((string)$_POST[$name]);
    return $value === '' ? null : $value;
}

if ($Mode === 'ADD_ITEM') {
    $error = null;
    $legal_entity_id = (int)($_POST['legal_entity_id'] ?? 0);
    $policy_code = trim((string)($_POST['policy_code'] ?? ''));

    // Predecessor identity is derived from governed state, never caller-selected.
    $predecessor = null;
    if ($legal_entity_id > 0 && $policy_code !== '') {
        $q = db_query(
            'SELECT policy_version_id FROM ' . TB_PREF . 'hrm_leave_policy_versions'
            . ' WHERE legal_entity_id=' . $legal_entity_id
            . ' AND policy_code=' . db_escape($policy_code)
            . ' ORDER BY effective_from DESC,policy_version_id DESC LIMIT 1'
        );
        if ($q && db_num_rows($q) === 1) {
            $r = db_fetch_assoc($q);
            $predecessor = (int)$r['policy_version_id'];
        }
    }

    $payload = array(
        'legal_entity_id' => $legal_entity_id,
        'policy_code' => $policy_code,
        'version_token' => trim((string)($_POST['version_token'] ?? '')),
        'leave_id' => (int)($_POST['leave_id'] ?? 0),
        'source_kind' => (string)($_POST['source_kind'] ?? ''),
        'source_ref' => trim((string)($_POST['source_ref'] ?? '')),
        'source_evidence_sha256' => strtolower(trim((string)($_POST['source_evidence_sha256'] ?? ''))),
        'employment_type' => lve1_ui_optional_int('employment_type'),
        'grade_id' => lve1_ui_optional_int('grade_id'),
        'assignment_id' => lve1_ui_optional_int('assignment_id'),
        'work_location_id' => lve1_ui_optional_int('work_location_id'),
        'effective_from' => (isset($_POST['effective_from']) && is_date($_POST['effective_from'])) ? date2sql($_POST['effective_from']) : '',
        'effective_to' => (!isset($_POST['effective_to']) || trim((string)$_POST['effective_to']) === '') ? null : (is_date($_POST['effective_to']) ? date2sql($_POST['effective_to']) : 'invalid'),
        'priority_rank' => (int)($_POST['priority_rank'] ?? 0),
        'unit_code' => (string)($_POST['unit_code'] ?? ''),
        'cycle_basis' => (string)($_POST['cycle_basis'] ?? ''),
        'entitlement_mode' => (string)($_POST['entitlement_mode'] ?? ''),
        'entitlement_units' => trim((string)($_POST['entitlement_units'] ?? '')),
        'accrual_period' => lve1_ui_optional_text('accrual_period'),
        'waiting_days' => (int)($_POST['waiting_days'] ?? 0),
        'proration_mode' => (string)($_POST['proration_mode'] ?? ''),
        'reference_cycle_minutes' => lve1_ui_optional_int('reference_cycle_minutes'),
        'carryover_enabled' => check_value('carryover_enabled') ? 1 : 0,
        'carryover_cap_units' => lve1_ui_optional_text('carryover_cap_units'),
        'expiry_days' => lve1_ui_optional_text('expiry_days'),
        'carryover_consumption_order' => lve1_ui_optional_text('carryover_consumption_order'),
        'negative_balance_limit_units' => trim((string)($_POST['negative_balance_limit_units'] ?? '0')),
        'donation_allowed' => check_value('donation_allowed') ? 1 : 0,
        'cashout_allowed' => check_value('cashout_allowed') ? 1 : 0,
        'min_request_units' => lve1_ui_optional_text('min_request_units'),
        'rounding_scale' => (int)($_POST['rounding_scale'] ?? 2),
        'predecessor_policy_version_id' => $predecessor,
    );

    $result = hrm_lve_001_create_policy($payload, $error);
    if ($result === false) {
        display_error(_('Leave policy was not created: ') . $error);
    } else {
        display_notification(_('Governed leave policy version created. Immutable SHA-256: ') . $result['policy_sha256']);
        $Mode = 'RESET';
    }
}

if ($Mode === 'RESET') {
    $selected_id = '';
    foreach (array(
        'legal_entity_id','policy_code','version_token','leave_id','source_ref','source_evidence_sha256',
        'employment_type','grade_id','assignment_id','work_location_id','effective_to','entitlement_units',
        'accrual_period','reference_cycle_minutes','carryover_cap_units','expiry_days','carryover_consumption_order',
        'negative_balance_limit_units','min_request_units'
    ) as $name)
        unset($_POST[$name]);
    $_POST['effective_from'] = Today();
    $_POST['source_kind'] = 'employer';
    $_POST['priority_rank'] = 0;
    $_POST['unit_code'] = 'days';
    $_POST['cycle_basis'] = 'fiscal';
    $_POST['entitlement_mode'] = 'front_load';
    $_POST['waiting_days'] = 0;
    $_POST['proration_mode'] = 'none';
    $_POST['rounding_scale'] = 2;
}

start_form(); // Framework CSRF token protects every mutation.

start_table(TABLESTYLE, "width='100%'");
table_header(array(
    _('Version ID'), _('Entity'), _('Policy'), _('Version'), _('Leave'), _('Effective'),
    _('Unit'), _('Mode'), _('Priority'), _('Source'), _('SHA-256')
));
$q = db_query(
    'SELECT p.*,l.leave_name FROM ' . TB_PREF . 'hrm_leave_policy_versions p'
    . ' LEFT JOIN ' . TB_PREF . 'leave_types l ON l.leave_id=p.leave_id'
    . ' ORDER BY p.legal_entity_id,p.policy_code,p.effective_from DESC,p.policy_version_id DESC'
);
$k = 0;
while ($q && ($row = db_fetch_assoc($q))) {
    alt_table_row_color($k);
    label_cell((int)$row['policy_version_id']);
    label_cell((int)$row['legal_entity_id']);
    label_cell($row['policy_code']);
    label_cell($row['version_token']);
    label_cell($row['leave_name'] ?: $row['leave_id']);
    label_cell(sql2date($row['effective_from']) . (empty($row['effective_to']) ? '' : ' – ' . sql2date($row['effective_to'])));
    label_cell($row['unit_code']);
    label_cell($row['entitlement_mode']);
    label_cell((int)$row['priority_rank']);
    label_cell($row['source_kind'] . ': ' . $row['source_ref']);
    label_cell($row['policy_sha256']);
    end_row();
}
end_table(1);

display_note(_('Policy versions are immutable. Create a new effective-dated successor to change governed policy. No statutory or jurisdictional defaults are assumed.'));

start_outer_table(TABLESTYLE2);
table_section(1);
text_row_ex(_('Legal Entity ID:'), 'legal_entity_id', 12, 12);
text_row_ex(_('Policy Code:'), 'policy_code', 24, 64);
text_row_ex(_('Version Token:'), 'version_token', 24, 64);
leave_types_list_row(_('Leave Type:'), 'leave_id', null, false, false);
array_selector_row(_('Authoritative Source:'), 'source_kind', null, array(
    'employer' => _('Employer Policy'),
    'pack' => _('Governed Country/Policy Pack'),
    'collective' => _('Collective Agreement'),
    'legacy_migration' => _('Legacy Migration Evidence'),
));
text_row_ex(_('Source Reference:'), 'source_ref', 42, 160);
text_row_ex(_('Source Evidence SHA-256:'), 'source_evidence_sha256', 68, 64);
text_row_ex(_('Employment Type ID (optional):'), 'employment_type', 12, 12);
text_row_ex(_('Grade ID (optional):'), 'grade_id', 12, 12);
text_row_ex(_('Assignment ID (optional):'), 'assignment_id', 12, 12);
text_row_ex(_('Work Location ID (optional):'), 'work_location_id', 12, 12);
date_row(_('Effective From:'), 'effective_from');
date_row(_('Effective To (optional):'), 'effective_to');
text_row_ex(_('Priority Rank:'), 'priority_rank', 8, 8);

table_section(2);
array_selector_row(_('Unit:'), 'unit_code', null, array('days'=>_('Days'),'hours'=>_('Hours'),'minutes'=>_('Minutes')));
array_selector_row(_('Cycle:'), 'cycle_basis', null, array('fiscal'=>_('Fiscal'),'anniversary'=>_('Employment Anniversary')));
array_selector_row(_('Entitlement Mode:'), 'entitlement_mode', null, array('front_load'=>_('Front Load'),'accrual'=>_('Accrual')));
text_row_ex(_('Entitlement Units:'), 'entitlement_units', 12, 24);
array_selector_row(_('Accrual Period:'), 'accrual_period', null, array(''=>_('Not Applicable'),'monthly'=>_('Monthly'),'quarterly'=>_('Quarterly')));
text_row_ex(_('Waiting Days:'), 'waiting_days', 8, 8);
array_selector_row(_('Proration:'), 'proration_mode', null, array('none'=>_('None'),'service_fraction'=>_('Service Fraction'),'work_schedule_fraction'=>_('Work Schedule Fraction')));
text_row_ex(_('Reference Cycle Minutes (schedule proration):'), 'reference_cycle_minutes', 12, 16);
check_row(_('Carryover Enabled:'), 'carryover_enabled');
text_row_ex(_('Carryover Cap Units (optional):'), 'carryover_cap_units', 12, 24);
text_row_ex(_('Carryover Expiry Days (optional):'), 'expiry_days', 8, 8);
array_selector_row(_('Carryover Consumption Order:'), 'carryover_consumption_order', null, array(''=>_('Not Applicable'),'carryover_first'=>_('Carryover First'),'current_first'=>_('Current Entitlement First')));
text_row_ex(_('Negative Balance Limit (zero or negative):'), 'negative_balance_limit_units', 12, 24);
check_row(_('Donation Allowed:'), 'donation_allowed');
check_row(_('Cash-out Quantity Allowed:'), 'cashout_allowed');
text_row_ex(_('Minimum Request Units (optional):'), 'min_request_units', 12, 24);
array_selector_row(_('Rounding Scale:'), 'rounding_scale', null, array(0=>'0',1=>'1',2=>'2',3=>'3',4=>'4',5=>'5',6=>'6'));

end_outer_table(1);
submit_center('ADD_ITEM', _('Create Immutable Policy Version'), true, '', 'default');
end_form();
end_page();
