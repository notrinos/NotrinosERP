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
 * DIM-ADV-002: effective Dimension hierarchy and portfolio rollups.
 *
 * Part 1 lists the hierarchy versions of one axis; an independent checker approves or rejects a proposed version. Part 2
 * proposes a new structure (a node move is a new version with an effective date). Part 3 shows the portfolio rollup, as posted
 * (every posting through the structure effective on its date) or with the current structure (which restates history). The page
 * owns no hierarchy logic: everything is decided by `dimensions/includes/dimension_hierarchy.inc`. Nothing is inferred from
 * names, numbering or the HRM organization structure.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/dimensions/includes/dimensions_db.inc');
include_once($path_to_root.'/dimensions/includes/dimension_hierarchy.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = 'Dimension Hierarchy and Portfolio Rollups'), false, false, '', $js);

//-----------------------------------------------------------------------------------------------

/** Parse the proposal textarea: one `member parent` line per change (parent 0 = root, the word remove drops the member). */
function dimension_hier_page_changes($text)
{
	$changes = array();
	foreach (preg_split('/\r\n|\n|\r/', (string)$text) as $line) {
		$line = trim($line);
		if ($line === '')
			continue;
		$parts = preg_split('/[\s,;]+/', $line);
		$parent = isset($parts[1]) ? $parts[1] : '0';
		$changes[] = array('member' => $parts[0], 'parent' => strtolower($parent) === 'remove' ? null : $parent);
	}
	return $changes;
}

/** One labelled selector inside the current table row. */
function dimension_hier_page_selector_cell($label, $name, $selected, $items, $options = null)
{
	echo "<td class='label'>".$label."</td>
<td>".array_selector($name, $selected, $items, $options)."</td>
";
}

/** Show the errors of a refused call or its success note. */
function dimension_hier_page_show_result($result, $success_text)
{
	if (!$result['ok']) {
		foreach ($result['errors'] as $error)
			display_error($error);
		return;
	}
	display_notification($success_text);
}

//-----------------------------------------------------------------------------------------------

if (dimension_hier_schema_ready() !== true) {
	display_error(_('The hierarchy tables are not installed. Upgrade the company database first.'));
	end_page();
	exit;
}

$axis = (int)get_post('axis', 1) === 2 ? 2 : 1;
if (!isset($_POST['TransFromDate']))
	$_POST['TransFromDate'] = begin_fiscalyear();
if (!isset($_POST['TransToDate']))
	$_POST['TransToDate'] = Today();

if (isset($_POST['ProposeHierarchy']) && check_csrf_token())
	dimension_hier_page_show_result(dimension_hier_propose($axis, dimension_hier_page_changes(get_post('hier_changes', '')), is_date(get_post('hier_effective', '')) ? date2sql($_POST['hier_effective']) : '',
		get_post('hier_policy', 'allowed'), get_post('hier_note', ''), check_value('hier_retroactive')), _('The structure was proposed and waits for an independent checker.'));
$approve_id = find_submit('ApproveHierarchy');
if ($approve_id != -1 && check_csrf_token())
	dimension_hier_page_show_result(dimension_hier_decide($approve_id, 'approve', get_post('decision_note', '')), _('The structure is approved and applies from its effective date.'));
$reject_id = find_submit('RejectHierarchy');
if ($reject_id != -1 && check_csrf_token())
	dimension_hier_page_show_result(dimension_hier_decide($reject_id, 'reject', get_post('decision_note', '')), _('The structure is rejected.'));

//-----------------------------------------------------------------------------------------------

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
dimension_hier_page_selector_cell(_('Category:'), 'axis', $axis, array(1 => _('Category 1'), 2 => _('Category 2')), array('select_submit' => true));
end_row();
end_table(1);

display_heading(_('Part 1 - hierarchy versions'));
$versions = dimension_hier_recent($axis, 100);
if ($versions === false || !count($versions))
	display_note(_('No hierarchy has been proposed for this category. Dimensions are flat until one is approved.'), 0, 1);
else {
	text_row_ex(_('Decision note:'), 'decision_note', 40, 200, '', get_post('decision_note', ''));
	start_table(TABLESTYLE);
	table_header(array(_('Version'), _('Effective from'), _('Parent postings'), _('Members'), _('State'), _('Commentary'), ''));
	foreach ($versions as $version) {
		start_row();
		label_cell('#'.$version['version_id'].' v'.$version['version_no'].((int)$version['retroactive'] ? ' ('._('retroactive').')' : ''));
		label_cell(sql2date($version['effective_from']));
		label_cell($version['parent_policy'] === 'leaf_only' ? _('Leaves only (exceptions reported)') : _('Allowed'));
		label_cell($version['member_count'], "align='right'");
		label_cell($version['status']);
		label_cell(htmlspecialchars($version['commentary']));
		echo '<td>';
		if ($version['status'] === 'pending') {
			submit('ApproveHierarchy'.$version['version_id'], _('Approve'), true, _('Independent checker approval'), false);
			submit('RejectHierarchy'.$version['version_id'], _('Reject'), true, _('Reject this structure'), false);
		}
		echo '</td>';
		end_row();
	}
	end_table(1);
}

display_heading(_('Part 2 - propose a structure'));
display_note(_('One line per change: Dimension identity, parent identity (0 = no parent, the word remove drops a member of the approved structure). Parents must be Dimensions of the same category and a cycle is refused. The structure takes effect on its date; earlier periods keep the structure that applied to them.'), 0, 1);
textarea_row(_('Changes:'), 'hier_changes', get_post('hier_changes', ''), 40, 8);
date_row(_('Effective from:'), 'hier_effective', get_post('hier_effective', Today()));
array_selector_row(_('Direct postings on a parent:'), 'hier_policy', get_post('hier_policy', 'allowed'), array('allowed' => _('Allowed'), 'leaf_only' => _('Leaves only (report exceptions)')));
check_row(_('Retroactive (the date is before today):'), 'hier_retroactive', null);
text_row_ex(_('Commentary (required):'), 'hier_note', 50, 500, '', get_post('hier_note', ''));
submit_center('ProposeHierarchy', _('Propose structure'), true, _('The structure waits for an independent checker'), false);

display_heading(_('Part 3 - portfolio rollup'));
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'TransFromDate');
date_cells(_('To:'), 'TransToDate');
dimension_hier_page_selector_cell(_('View:'), 'hier_view', get_post('hier_view', 'as_posted'), array('as_posted' => _('As posted'), 'current' => _('Current structure (restates history)')));
submit_cells('ShowRollup', _('Show'), '', false, false);
end_row();
end_table(1);
if (is_date($_POST['TransFromDate']) && is_date($_POST['TransToDate'])) {
	$rollup = dimension_hier_rollup($axis, date2sql($_POST['TransFromDate']), date2sql($_POST['TransToDate']), get_post('hier_view', 'as_posted') === 'current' ? 'current' : 'as_posted');
	if ($rollup === false)
		display_error(_('The rollup could not be read: it needs the Dimension view and GL analytics authority and intact approved structures.'));
	else {
		display_note($rollup['label'], 0, 1);
		if ($rollup['retroactive_segments'])
			display_warning(_('A retroactive structure applies to part of this period; the as-posted result includes that deliberate restatement.'));
		$names = array();
		$rows = db_query('SELECT id, reference, name FROM '.TB_PREF.'dimensions WHERE type_='.(int)$axis);
		while ($rows && ($row = db_fetch_assoc($rows)))
			$names[(int)$row['id']] = $row['reference'].' '.$row['name'];
		$parents = array();
		foreach ($rollup['nodes'] as $node)
			$parents[$node['member']] = $node['parent'];
		start_table(TABLESTYLE);
		table_header(array(_('Dimension'), _('Revenue (rolled up)'), _('Costs (rolled up)'), _('Revenue (direct)'), _('Costs (direct)'), _('In structure')));
		foreach ($rollup['nodes'] as $node) {
			$depth = count(dimension_hier_ancestors($parents, $node['member']));
			start_row();
			label_cell(str_repeat('&nbsp;&nbsp;&nbsp;', $depth).htmlspecialchars(isset($names[$node['member']]) ? $names[$node['member']] : '#'.$node['member']));
			amount_cell($node['rollup']['revenue']);
			amount_cell($node['rollup']['costs']);
			amount_cell($node['direct']['revenue']);
			amount_cell($node['direct']['costs']);
			label_cell($node['in_hierarchy'] ? _('Yes') : _('No'));
			end_row();
		}
		end_table(1);
		start_table(TABLESTYLE);
		foreach (array(_('Portfolio (each posting once)') => 'portfolio', _('Dimensions outside the structure') => 'outside', _('No Dimension (unassigned)') => 'unassigned',
			_('Unknown Dimension') => 'unknown', _('All profit and loss postings') => 'total') as $label => $key) {
			start_row();
			label_cell($label);
			amount_cell($rollup[$key]['revenue']);
			amount_cell($rollup[$key]['costs']);
			end_row();
		}
		end_table(1);
		foreach ($rollup['exceptions'] as $exception)
			display_warning(sprintf(_('Policy exception: %s carries %s of direct postings although only leaves may receive them (%s - %s).'),
				isset($names[$exception['member']]) ? $names[$exception['member']] : '#'.$exception['member'], number_format2($exception['costs'] + $exception['revenue'], 2),
				sql2date($exception['from']), sql2date($exception['to'])));
	}
}

end_form();
end_page();
