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
 * DIM-CFG-001: Dimension category policy and axis transition preview.
 *
 * This page declares what each native Dimension category means for the active company and previews
 * the six axis-count transitions. A published policy is append-only and descriptive: it never
 * renumbers a stored Dimension, never rewrites GL, budget, draft or HRM rows and never changes what
 * HRM binds to axis 1.
 */
$page_security = 'SA_SETUPCOMPANY';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Dimension Category Policy');

page($_SESSION['page_title']);

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/dimensions/includes/dimension_category_policy.inc');

//-------------------------------------------------------------------------------------------------

/**
 * Build the policy input posted by this page.
 *
 * @return array axes and rules accepted by dimension_category_policy_publish()
 */
function dimension_category_policy_posted_input() {
	$input = array('axes' => array(), 'rules' => array());
	foreach (dimension_category_policy_axes() as $axis) {
		$input['axes'][$axis] = array(
			'label' => (string)get_post('axis'.$axis.'_label', ''),
			'purpose' => (string)get_post('axis'.$axis.'_purpose', ''),
			'required' => check_value('axis'.$axis.'_required') ? 1 : 0,
		);
	}
	$parsed = dimension_category_policy_parse_rules((string)get_post('policy_rules', ''));
	$input['rules'] = $parsed['rules'];
	$GLOBALS['dimension_category_policy_rule_errors'] = $parsed['errors'];
	return $input;
}

if (isset($_POST['publish_policy']) && check_csrf_token()) {
	$policy_input = dimension_category_policy_posted_input();
	$policy_rule_errors = $GLOBALS['dimension_category_policy_rule_errors'];
	if (count($policy_rule_errors)) {
		foreach ($policy_rule_errors as $policy_rule_error)
			display_error($policy_rule_error);
		set_focus('policy_rules');
	} else {
		$policy_result = dimension_category_policy_publish($policy_input);
		if ($policy_result['published']) {
			display_notification(sprintf(_('Dimension category policy version %d has been published.'), (int)$policy_result['version']));
		} else {
			foreach ($policy_result['errors'] as $policy_error)
				display_error($policy_error);
		}
	}
}

//-------------------------------------------------------------------------------------------------

$current_policy = dimension_category_policy_current(true);
$policy_authorized = dimension_category_policy_authorized();
$enabled_axes = (int)get_company_pref('use_dimension');
$policy_states = array(
	'neutral' => _('No policy published: neutral labels and permissive rules'),
	'published' => _('Published'),
	'invalid' => _('A published version does not verify; neutral labels are shown and no declared rule is trusted'),
	'unavailable' => _('The policy schema is not installed for this company'),
);

start_form();

start_outer_table(TABLESTYLE2, "width='98%'");
table_section(1);
table_section_title(_('Category policy in force'));

label_row(_('State:'), isset($policy_states[$current_policy['state']]) ? $policy_states[$current_policy['state']] : $current_policy['state']);
label_row(_('Published version:'), $current_policy['version'] > 0 ? (int)$current_policy['version'] : _('none'));
label_row(_('Enabled categories for new entries:'), (int)$enabled_axes);

foreach (dimension_category_policy_axes() as $axis) {
	$axis_policy = $current_policy['axes'][$axis];
	$axis_name = dimension_category_policy_label($axis);
	$purposes = dimension_category_policy_purposes();
	label_row(sprintf(_('Category %d:'), $axis), htmlspecialchars($axis_name, ENT_QUOTES, 'UTF-8')
		.($axis === 1 ? ' &mdash; '._('HRM Cost Center').' '.($enabled_axes >= 1 ? _('(enabled)') : _('(not enabled for new entries)')) : ''));
	label_row(_('Declared purpose:'), htmlspecialchars(isset($purposes[$axis_policy['purpose']]) ? $purposes[$axis_policy['purpose']] : $axis_policy['purpose'], ENT_QUOTES, 'UTF-8'));
	label_row(_('Required for new events:'), $axis_policy['required'] ? _('Yes') : _('No'));
}

$policy_rule_text = dimension_category_policy_rules_text($current_policy['rules']);
label_row(_('Declared rules:'), $policy_rule_text === '' ? _('None (permissive)') : '<pre>'.htmlspecialchars($policy_rule_text, ENT_QUOTES, 'UTF-8').'</pre>');

table_section(2);
table_section_title(_('How categories behave'));

display_note(_('A published policy is descriptive only. It never renumbers or remaps a stored Dimension, never rewrites GL, budget, draft, payroll or HRM rows, and never changes what HRM binds to category 1. Reducing the enabled categories is refused while an active source, a stored default, a pending draft, an HRM binding or a declared rule still depends on the category being disabled.'), 0, 0, 'class="info"');
display_note(_('Publishing requires both Company Setup and Dimension entry authority. History stays readable through every transition, including for closed categories and closed values.'), 0, 0, 'class="info"');

end_outer_table(1);

//-------------------------------------------------------------------------------------------------

if ($policy_authorized) {
	start_outer_table(TABLESTYLE2, "width='98%'");
	table_section(1);
	table_section_title(_('Publish a new policy version'));
	$purposes = dimension_category_policy_purposes();
	foreach (dimension_category_policy_axes() as $axis) {
		$axis_policy = $current_policy['axes'][$axis];
		// text_input() writes the value into a single-quoted attribute unescaped, and a label may hold
		// quotes, so the value shown (stored or just posted) is HTML-escaped before it reaches the form.
		$label_field = 'axis'.$axis.'_label';
		$_POST[$label_field] = htmlspecialchars(isset($_POST[$label_field]) ? (string)$_POST[$label_field]
			: (string)$axis_policy['label'], ENT_QUOTES, 'UTF-8');
		text_row_ex(sprintf(_('Category %d label:'), $axis), $label_field, 40, 64,
			_('Descriptive only; leave blank for the neutral name'));
		array_selector_row(_('Category ').$axis.' '._('purpose:'), 'axis'.$axis.'_purpose',
			$axis_policy['purpose'], $purposes, array('select_submit' => false, 'async' => false));
		check_row(sprintf(_('Category %d is required for new events:'), $axis), 'axis'.$axis.'_required', $axis_policy['required']);
	}
	table_section(2);
	table_section_title(_('Declared rules (optional)'));
	textarea_row(_('Rules:'), 'policy_rules', htmlspecialchars($policy_rule_text, ENT_QUOTES, 'UTF-8'), 60, 5, 4000,
		_('One rule per line: account:<category>:<account type> or pair:<category 1 value>:<category 2 value>; * means any value; # starts a comment'));
	label_row(_('Posting enforcement:'), _('Declared rules are recorded and explained here; new-event enforcement belongs to DIM-FND-005/DIM-FND-007.'));
	submit_center('publish_policy', _('Publish version'), true, _('Append a new immutable policy version'), false);
	end_outer_table(1);
} else {
	display_error(_('Publishing a Dimension category policy requires both Company Setup and Dimension entry authority. The policy and the transition preview remain visible.'));
}

//-------------------------------------------------------------------------------------------------

start_outer_table(TABLESTYLE2, "width='98%'");
table_section(1);
table_section_title(_('Six axis-count transition preview'));
display_note(_('Each transition below is read-only. "Blocked" means the Company Setup page refuses the change until every listed conflict is resolved; no historical row changes either way.'), 0, 0, 'class="info"');

$transition_matrix = dimension_category_transition_matrix();
echo "<tr><th class='label'>"._('Transition')."</th><th>"._('Direction')."</th>"
	.'<th>'._('Open values to close').'</th><th>'._('Stored defaults').'</th><th>'._('Pending drafts').'</th>'
	.'<th>'._('HRM bindings').'</th><th>'._('Policy conflicts').'</th><th>'._('Unverifiable sources').'</th><th>'._('Verdict')."</th></tr>\n";
foreach (array(0, 1, 2) as $from) {
	foreach (array(0, 1, 2) as $to) {
		if ($from === $to)
			continue;
		$preview = $transition_matrix[$from][$to];
		$counts = array('open_values' => 0, 'stored_defaults' => 0, 'pending_drafts' => 0,
			'hrm_bindings' => 0, 'policy' => 0, 'unknown' => 0);
		foreach ($preview['axes'] as $impact)
			foreach ($counts as $kind => $amount)
				$counts[$kind] += (int)$impact['conflict_counts'][$kind];
		echo '<tr><td>'.sprintf(_('%1$d &rarr; %2$d'), $from, $to).'</td>'
			.'<td>'.($preview['direction'] === 'increase' ? _('Enable') : _('Reduce')).'</td>'
			.'<td>'.(int)$counts['open_values'].'</td><td>'.(int)$counts['stored_defaults'].'</td>'
			.'<td>'.(int)$counts['pending_drafts'].'</td><td>'.(int)$counts['hrm_bindings'].'</td>'
			.'<td>'.(int)$counts['policy'].'</td><td>'.(int)$counts['unknown'].'</td>'
			.'<td>'.($preview['blocked'] ? _('Blocked') : _('Allowed'))."</td></tr>\n";
	}
}
end_outer_table(1);

//-------------------------------------------------------------------------------------------------

$reductions = array(array(2, 1), array(2, 0), array(1, 0));
foreach ($reductions as $transition) {
	$preview = $transition_matrix[$transition[0]][$transition[1]];
	if (!count($preview['conflicts']))
		continue;
	echo '<h3>'.sprintf(_('Unresolved conflicts for %1$d &rarr; %2$d'), $transition[0], $transition[1]).'</h3>';
	echo '<ul>';
	foreach ($preview['conflicts'] as $conflict)
		echo '<li>'.htmlspecialchars($conflict, ENT_QUOTES, 'UTF-8').'</li>';
	echo '</ul>';
}

//-------------------------------------------------------------------------------------------------

$policy_versions = dimension_category_policy_versions(20);
if (count($policy_versions)) {
	start_outer_table(TABLESTYLE2, "width='98%'");
	table_section(1);
	table_section_title(_('Published versions (append-only)'));
	echo "<tr><th class='label'>"._('Version').'</th><th>'._('Published at').'</th><th>'._('By user').'</th>'
		.'<th>'._('Rules').'</th><th>'._('Digest')."</th></tr>\n";
	foreach ($policy_versions as $policy_version) {
		echo '<tr><td>'.(int)$policy_version['version'].'</td>'
			.'<td>'.htmlspecialchars((string)$policy_version['created_at'], ENT_QUOTES, 'UTF-8').'</td>'
			.'<td>'.(int)$policy_version['created_by'].'</td>'
			.'<td>'.(int)$policy_version['rule_count'].'</td>'
			.'<td>'.($policy_version['complete'] ? htmlspecialchars((string)$policy_version['digest'], ENT_QUOTES, 'UTF-8') : _('incomplete')).'</td></tr>'."\n";
	}
	end_outer_table(1);
}

end_form();

end_page();
