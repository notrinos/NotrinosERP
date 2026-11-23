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
 * DIM-BUD-004: budget overrun alerts.
 *
 * One read-only page that lists the cost budgets of a fiscal year that reached the warning or limit threshold. It needs the Dimension
 * view area from the page security and, inside the authority, the funds-position rule (GL analytics or budget entry); a denied user
 * reads no figure. A user with the company setup area may also change the two thresholds here, with a reason. Nothing on this
 * page blocks a document, posts to the ledger or sends a message.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_values_db.inc');
include_once($path_to_root.'/tracking/includes/tracking_budget_alert.inc');

$js = '';
page(_($help_context = 'Budget Alerts'), false, false, '', $js);

//-----------------------------------------------------------------------------------

if (!dimension_alert_access()) {
	display_error(dimension_alert_state_note(dimension_alert_result('denied')));
	end_page();
	exit;
}

$can_change = user_check_access('SA_SETUPCOMPANY');
$keep_posted_thresholds = false;
if ($can_change && isset($_POST['SaveThresholds']) && check_csrf_token()) {
	$saved = dimension_alert_set_thresholds(get_post('WarningPercent', ''), get_post('LimitPercent', ''), get_post('ThresholdReason', ''));
	if (!$saved['ok']) {
		display_error(implode(' ', $saved['errors']));
		$keep_posted_thresholds = true;
	}
	elseif ($saved['changed'])
		display_notification(_('The budget alert thresholds were saved.'));
	else
		display_notification(_('The thresholds are already set to these values.'));
	if (!$keep_posted_thresholds)
		unset($_POST['ThresholdReason']);
}

if (!isset($_POST['FiscalYear']))
	$_POST['FiscalYear'] = get_company_pref('f_year');
if (!empty($_POST))
	$Ajax->activate('_page_body');

$thresholds = dimension_alert_thresholds();
if (!$keep_posted_thresholds) {
	$_POST['WarningPercent'] = rtrim(rtrim(number_format($thresholds['warning'], 1, '.', ''), '0'), '.');
	$_POST['LimitPercent'] = rtrim(rtrim(number_format($thresholds['limit'], 1, '.', ''), '0'), '.');
}

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
fiscalyears_list_cells(_('Fiscal Year:'), 'FiscalYear', get_post('FiscalYear'));
check_cells(_('Include budgets within the thresholds:'), 'IncludeOk', null);
submit_cells('Show', _('Show'), '', '', 'default');
end_row();
end_table();

//-----------------------------------------------------------------------------------

$alerts = dimension_alert_evaluate((int)get_post('FiscalYear', 0));
echo '<div id="dimension-budget-alerts">';
if ($alerts['state'] !== 'available')
	display_error(dimension_alert_state_note($alerts));
else {
	display_note(dimension_alert_threshold_text($alerts['thresholds']));
	$counts = $alerts['counts'];
	display_note(sprintf(_('%d at the limit, %d at the warning level, %d within the thresholds, %d not evaluated.'), $counts['limit'], $counts['warning'], $counts['ok'], $counts['not_evaluated']), 0, 1);
	$shown = array();
	foreach ($alerts['rows'] as $alert_row)
		if (check_value('IncludeOk') || $alert_row['state'] !== 'ok')
			$shown[] = $alert_row;
	if (!$shown)
		display_note(_('No cost budget has reached its warning threshold.'));
	else
		echo dimension_alert_rows_html($shown);
	if ($alerts['truncated'])
		display_warning(sprintf(_('Only the first %d of %d budgets were evaluated.'), dimension_alert_max_keys(), $alerts['budget_total']));
	if ($alerts['incomplete'] > 0)
		display_warning(sprintf(_('%d budget(s) have commitment rows that failed their integrity check; those rows are not counted.'), $alerts['incomplete']));
	echo '<p style="font-size:90%;">'.dimension_alert_h(dimension_alert_basis_text($alerts)).'</p>';
	echo '<p style="font-size:90%;">'.dimension_alert_h($alerts['label']).'</p>';
}
echo '</div>';

//-----------------------------------------------------------------------------------

if ($can_change) {
	echo '<br>';
	start_table(TABLESTYLE2);
	table_section_title(_('Alert thresholds'));
	text_row(_('Warning at (% of the budget used):'), 'WarningPercent', null, 8, 6);
	text_row(_('Limit at (% of the budget used):'), 'LimitPercent', null, 8, 6);
	text_row(_('Reason for the change:'), 'ThresholdReason', null, 50, 200);
	end_table(1);
	submit_center('SaveThresholds', _('Save thresholds'), true, _('Records the thresholds and the reason'), false);
	$history = dimension_alert_history();
	if ($history) {
		echo '<br>';
		start_table(TABLESTYLE);
		table_header(array(_('When'), _('User'), _('Warning'), _('Limit'), _('Previous'), _('Reason')));
		foreach ($history as $entry) {
			start_row();
			label_cell(dimension_alert_h($entry['at']));
			label_cell((int)$entry['actor']);
			label_cell(dimension_alert_percent_text($entry['warning']), 'align=right');
			label_cell(dimension_alert_percent_text($entry['limit']), 'align=right');
			label_cell(dimension_alert_percent_text($entry['previous_warning']).' / '.dimension_alert_percent_text($entry['previous_limit']), 'align=right');
			label_cell(dimension_alert_h($entry['reason']));
			end_row();
		}
		end_table();
	}
}
end_form();

end_page();
