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
 * DIM-BUD-002: budget baselines, approved revisions and the active comparison.
 *
 * Part 1 lists the versions of a fiscal year; an independent checker approves or rejects a proposed version and an
 * authorized user chooses which approved version (or the live budget entry) the reports compare against. Part 2 captures
 * the exact baseline of the live budget. Part 3 proposes a revision with commentary. Part 4 compares two versions or a
 * version with the live budget. The page owns no budget logic: everything is decided by
 * `dimensions/includes/dimension_budget_version.inc`. The editable budget entry page stays unchanged.
 */
$page_security = 'SA_BUDGETENTRY';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/admin/db/fiscalyears_db.inc');
include_once($path_to_root.'/admin/db/fiscal_years_entity.inc');
include_once($path_to_root.'/dimensions/includes/dimension_budget_basis.inc');

page(_($help_context = 'Budget Baselines and Revisions'));

//-----------------------------------------------------------------------------------------------

/** Parse the revision textarea: one `account dimension1 dimension2 date amount` line per change (amount `remove` drops the line). */
function dimension_budget_page_changes($text)
{
	$changes = array();
	foreach (preg_split('/\r\n|\n|\r/', (string)$text) as $line) {
		$line = trim($line);
		if ($line === '')
			continue;
		$parts = preg_split('/[\s,;]+/', $line);
		$date = isset($parts[3]) && is_date($parts[3]) ? date2sql($parts[3]) : (isset($parts[3]) ? $parts[3] : '');
		$amount = isset($parts[4]) ? $parts[4] : '';
		$changes[] = array('account' => $parts[0], 'coordinate_1' => isset($parts[1]) ? $parts[1] : '', 'coordinate_2' => isset($parts[2]) ? $parts[2] : '',
			'period_start' => $date, 'amount' => strtolower($amount) === 'remove' ? null : $amount);
	}
	return $changes;
}

/** Show the errors of a refused call or its success note. */
function dimension_budget_page_show_result($result, $success_text)
{
	if (!$result['ok']) {
		foreach ($result['errors'] as $error)
			display_error($error);
		return;
	}
	display_notification($success_text.(!empty($result['duplicate']) ? ' '._('(an identical request already exists; nothing new was recorded)') : ''));
}

//-----------------------------------------------------------------------------------------------

if (dimension_budget_version_schema_ready() !== true) {
	display_error(_('The budget version tables are not installed. Upgrade the company database first.'));
	end_page();
	exit;
}

$fiscal_year = (int)get_post('fyear', 0);
if (!$fiscal_year && function_exists('get_current_fiscalyear')) {
	$current_year = get_current_fiscalyear();
	$fiscal_year = $current_year ? (int)$current_year['id'] : 0;
	$_POST['fyear'] = $fiscal_year;
}

$compare_result = null;
if ($fiscal_year) {
	if (isset($_POST['CaptureBaseline']) && check_csrf_token())
		dimension_budget_page_show_result(dimension_budget_version_capture_baseline($fiscal_year, get_post('baseline_note', '')),
			_('The baseline was captured from the live budget and waits for an independent checker.'));
	if (isset($_POST['ProposeRevision']) && check_csrf_token())
		dimension_budget_page_show_result(dimension_budget_version_propose_revision($fiscal_year, get_post('revision_basis', 0),
			dimension_budget_page_changes(get_post('revision_changes', '')), get_post('revision_note', '')),
			_('The revision was proposed and waits for an independent checker.'));
	$approve_id = find_submit('ApproveVersion');
	if ($approve_id != -1 && check_csrf_token())
		dimension_budget_page_show_result(dimension_budget_version_decide($approve_id, 'approve', get_post('decision_note', '')),
			_('The version is approved. It is not used by the reports until it is made the active comparison.'));
	$reject_id = find_submit('RejectVersion');
	if ($reject_id != -1 && check_csrf_token())
		dimension_budget_page_show_result(dimension_budget_version_decide($reject_id, 'reject', get_post('decision_note', '')), _('The version is rejected.'));
	$activate_id = find_submit('ActivateVersion');
	if ($activate_id != -1 && check_csrf_token())
		dimension_budget_page_show_result(dimension_budget_version_activate($fiscal_year, $activate_id, get_post('activation_reason', '')),
			_('The version is now the active comparison of this fiscal year.'));
	if (isset($_POST['ActivateLive']) && check_csrf_token())
		dimension_budget_page_show_result(dimension_budget_version_activate($fiscal_year, 0, get_post('activation_reason', '')),
			_('The live budget entry is the active comparison of this fiscal year again.'));
	if (isset($_POST['CompareVersions']) && check_csrf_token())
		$compare_result = dimension_budget_version_compare((int)get_post('compare_left', 0), (int)get_post('compare_right', 0));
}

//-----------------------------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
fiscalyears_list_cells(_('Fiscal Year:'), 'fyear', null);
submit_cells('SelectYear', _('Get'), '', false, false);
end_row();
end_table(1);

if (!$fiscal_year) {
	display_note(_('Choose a fiscal year.'), 0, 1);
	end_form();
	end_page();
	exit;
}

$active = dimension_budget_version_active($fiscal_year);
$versions = dimension_budget_version_recent($fiscal_year, 100);
$latest = dimension_budget_version_latest_approved($fiscal_year);

display_heading(_('Part 1 - versions of this fiscal year'));
display_note($active ? sprintf(_('The reports compare against version #%d of this fiscal year.'), $active)
	: _('The reports compare against the live budget entry (no version is active).'), 0, 1);
if ($versions === false || !count($versions))
	display_note(_('No baseline has been captured for this fiscal year yet.'), 0, 1);
else {
	text_row_ex(_('Decision note / activation reason:'), 'decision_note', 40, 200, '', get_post('decision_note', ''));
	start_table(TABLESTYLE);
	table_header(array(_('Version'), _('Kind'), _('Lines'), _('Total'), _('State'), _('Commentary'), _('Proposed'), _('Decided'), ''));
	foreach ($versions as $version) {
		start_row();
		label_cell('#'.$version['version_id'].' v'.$version['version_no'].((int)$version['version_id'] === (int)$active ? ' *' : ''));
		label_cell($version['version_kind'] === 'baseline' ? _('Baseline') : _('Revision of #').(int)$version['basis_version_id']);
		label_cell($version['line_count'], "align='right'");
		amount_cell($version['total_amount']);
		label_cell($version['status']);
		label_cell(htmlspecialchars($version['commentary']));
		label_cell(sql2date(substr($version['created_at'], 0, 10)).' ('.(int)$version['maker_id'].')');
		label_cell($version['decided_at'] === null ? '' : sql2date(substr($version['decided_at'], 0, 10)).' ('.(int)$version['checker_id'].')');
		echo '<td>';
		if ($version['status'] === 'pending') {
			submit('ApproveVersion'.$version['version_id'], _('Approve'), true, _('Independent checker approval'), false);
			submit('RejectVersion'.$version['version_id'], _('Reject'), true, _('Reject this version'), false);
		} elseif ($version['status'] === 'approved' && (int)$version['version_id'] !== (int)$active)
			submit('ActivateVersion'.$version['version_id'], _('Make active'), true, _('Compare reports against this version'), false);
		echo '</td>';
		end_row();
	}
	end_table(1);
	text_row_ex(_('Reason for an activation change:'), 'activation_reason', 40, 200, '', get_post('activation_reason', ''));
	if ($active)
		submit_center('ActivateLive', _('Use the live budget entry again'), true, _('Reports compare against the editable budget entry'), false);
}

display_heading(_('Part 2 - capture the baseline of the live budget'));
display_note(_('The baseline is the live budget as it stands now. An earlier overwritten revision cannot be recovered and is never invented.'), 0, 1);
text_row_ex(_('Commentary (optional):'), 'baseline_note', 50, 500, '', get_post('baseline_note', ''));
submit_center('CaptureBaseline', _('Capture baseline'), true, _('The baseline waits for an independent checker'), false);

display_heading(_('Part 3 - propose a revision'));
if (!$latest)
	display_note(_('A revision needs an approved baseline first.'), 0, 1);
else {
	hidden('revision_basis', (int)$latest['version_id']);
	display_note(sprintf(_('The revision builds on the latest approved version #%d (v%d). One line per change: account, dimension 1, dimension 2, date, amount. An amount of 0 is an explicit zero; the word remove drops the line.'),
		$latest['version_id'], $latest['version_no']), 0, 1);
	textarea_row(_('Changes:'), 'revision_changes', get_post('revision_changes', ''), 50, 8);
	text_row_ex(_('Commentary (required):'), 'revision_note', 50, 500, '', get_post('revision_note', ''));
	submit_center('ProposeRevision', _('Propose revision'), true, _('The revision waits for an independent checker'), false);
}

display_heading(_('Part 4 - compare'));
if ($versions !== false && count($versions)) {
	$options = array();
	foreach ($versions as $version)
		$options[$version['version_id']] = '#'.$version['version_id'].' v'.$version['version_no'].' ('.$version['status'].')';
	$right = $options;
	$right[0] = _('Live budget entry');
	start_table(TABLESTYLE2);
	array_selector_row(_('Earlier version:'), 'compare_left', get_post('compare_left', ''), $options);
	array_selector_row(_('Compared with:'), 'compare_right', get_post('compare_right', '0'), $right);
	end_table(1);
	submit_center('CompareVersions', _('Compare'), true, _('Line differences between the two budgets'), false);
}
if ($compare_result !== null) {
	if (!$compare_result['ok'])
		foreach ($compare_result['errors'] as $error)
			display_error($error);
	else {
		$counts = $compare_result['counts'];
		display_note(sprintf(_('Same: %d. Changed: %d. Only in the later budget: %d. Only in the earlier version: %d. Zero versus absent: %d.'),
			$counts['same'], $counts['changed'], $counts['added'], $counts['removed'], $counts['zero_vs_absent']), 0, 1);
		if (count($compare_result['rows'])) {
			start_table(TABLESTYLE);
			table_header(array(_('Account'), _('Dimension 1'), _('Dimension 2'), _('Date'), _('Earlier'), _('Later'), _('Difference')));
			foreach ($compare_result['rows'] as $row) {
				start_row();
				label_cell($row['account_code']);
				label_cell($row['coordinate_1']);
				label_cell($row['coordinate_2']);
				label_cell(sql2date($row['period_start']));
				label_cell($row['left'] === null ? _('absent') : number_format2($row['left'], 2), "align='right'");
				label_cell($row['right'] === null ? _('absent') : number_format2($row['right'], 2), "align='right'");
				label_cell($row['state'].($row['zero_vs_absent'] ? ' ('._('zero vs absent').')' : ''));
				end_row();
			}
			end_table(1);
		}
		if ($compare_result['truncated'])
			display_note(_('Only the first differences are listed.'), 0, 1);
	}
}

display_heading(_('Active comparison history'));
$history = dimension_budget_version_activations($fiscal_year, 50);
if ($history === false || !count($history))
	display_note(_('The active comparison of this fiscal year has never been changed.'), 0, 1);
else {
	start_table(TABLESTYLE);
	table_header(array(_('When'), _('Active comparison'), _('User'), _('Reason')));
	foreach ($history as $entry) {
		start_row();
		label_cell($entry['created_at']);
		label_cell($entry['version_id'] === null ? _('Live budget entry') : '#'.(int)$entry['version_id']);
		label_cell((int)$entry['actor_id']);
		label_cell(htmlspecialchars($entry['reason']));
		end_row();
	}
	end_table(1);
}

end_form();
end_page();
