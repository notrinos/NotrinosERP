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
 * DIM-UX-001: business modes, three-step onboarding, synthetic demos and plain failure states.
 *
 * This file is a PRESENTATION layer. It translates what the company declared in its DIM-CFG-001 category
 * policy (a purpose such as Project or Cost center) into the words, date captions, measures and
 * "overdue" behavior a non-accountant expects. It never decides what a classification MEANS:
 *  - it writes nothing (no INSERT/UPDATE/DELETE, no preference, no policy, no migration);
 *  - the native categories stay `dimensions.type_` 1 and 2 and every stored `dimension_id` /
 *    `dimension2_id` keeps its meaning, so renaming category 1 "Project" never changes the HRM/payroll
 *    cost-center binding of that same category;
 *  - a company without a (valid) policy sees the neutral wording and the legacy behavior unchanged;
 *  - authorization stays with the existing pages and writers: this layer only HIDES actions the caller
 *    cannot perform and never grants one.
 *
 * The synthetic demos are generated in memory from fixed integer-cent lines. They read no company row,
 * are labeled as examples, and reconcile to themselves through an independent recomputation.
 */

/** Longest caption accepted in the catalog, so a longer translation still fits a table header or button. */
define('DIMENSION_BUSINESS_MODE_CAPTION_LIMIT', 60);
/** Longest help sentence accepted in the catalog. */
define('DIMENSION_BUSINESS_MODE_HELP_LIMIT', 320);

/**
 * Catalog of business modes keyed by the DIM-CFG-001 declared purpose ('' is the neutral mode).
 *
 * `time_boxed` decides whether a "date required by" is a real deadline (so the value can be overdue).
 * `measures` lists the performance columns that are meaningful for the mode.
 *
 * @return array purpose => array(name, value_word, time_boxed, deadline_caption, deadline_help, measures, purpose_help)
 */
function dimension_business_mode_catalog()
{
	return array(
		'' => array(
			'name' => _('Not declared'),
			'value_word' => _('value'),
			'time_boxed' => true,
			'deadline_caption' => _('Date Required By'),
			'deadline_help' => _('Shown as overdue when it is past and the value is still open.'),
			'measures' => array('revenue', 'costs', 'result'),
			'purpose_help' => _('No meaning is declared yet, so the wording stays general and nothing is assumed.'),
		),
		'cost_center' => array(
			'name' => _('Cost center'),
			'value_word' => _('cost center'),
			'time_boxed' => false,
			'deadline_caption' => _('Review date'),
			'deadline_help' => _('An ongoing cost center has no deadline, so it is never shown as overdue.'),
			'measures' => array('costs'),
			'purpose_help' => _('Collects the costs of a part of the business that carries on, such as an office or a workshop.'),
		),
		'department' => array(
			'name' => _('Department'),
			'value_word' => _('department'),
			'time_boxed' => false,
			'deadline_caption' => _('Review date'),
			'deadline_help' => _('A department has no deadline, so it is never shown as overdue.'),
			'measures' => array('costs'),
			'purpose_help' => _('Collects the costs of a team that carries on, such as Sales or Operations.'),
		),
		'project' => array(
			'name' => _('Project'),
			'value_word' => _('project'),
			'time_boxed' => true,
			'deadline_caption' => _('Due date'),
			'deadline_help' => _('A project is shown as overdue when this date has passed and it is still open.'),
			'measures' => array('revenue', 'costs', 'result'),
			'purpose_help' => _('Follows the income and costs of one piece of work from start to finish.'),
		),
		'activity' => array(
			'name' => _('Activity'),
			'value_word' => _('activity'),
			'time_boxed' => true,
			'deadline_caption' => _('End date'),
			'deadline_help' => _('An activity is shown as overdue when its end date has passed and it is still open.'),
			'measures' => array('costs', 'result'),
			'purpose_help' => _('Follows what one event or campaign costs and brings in.'),
		),
		'segment' => array(
			'name' => _('Segment'),
			'value_word' => _('segment'),
			'time_boxed' => false,
			'deadline_caption' => _('Review date'),
			'deadline_help' => _('A segment has no deadline, so it is never shown as overdue.'),
			'measures' => array('revenue', 'costs', 'result'),
			'purpose_help' => _('Compares a market, region or product line that carries on.'),
		),
	);
}

/**
 * Business mode of one native category in the active company.
 *
 * Only a verified published policy can declare a mode; a neutral, invalid or unavailable policy yields
 * the neutral mode ('') so a tampered or missing policy never changes behavior.
 *
 * @param int $axis Native category 1 or 2
 * @return string Declared purpose, or '' for neutral
 */
function dimension_business_mode_for_axis($axis)
{
	$axis = (int)$axis;
	if ($axis !== 1 && $axis !== 2)
		return '';
	$current = dimension_category_policy_current();
	if (!isset($current['state']) || $current['state'] !== 'published')
		return '';
	$purpose = isset($current['axes'][$axis]['purpose']) ? (string)$current['axes'][$axis]['purpose'] : '';
	$catalog = dimension_business_mode_catalog();
	return isset($catalog[$purpose]) ? $purpose : '';
}

/**
 * HTML-safe display text of one category: "2 - Project", or just the number when nothing is declared.
 *
 * The label is company data, so it is escaped here and the result may go straight into a table cell.
 *
 * @param int $axis Native category 1 or 2
 * @return string
 */
function dimension_business_mode_axis_text($axis)
{
	$axis = (int)$axis;
	$current = dimension_category_policy_current();
	$label = isset($current['axes'][$axis]['label']) ? trim((string)$current['axes'][$axis]['label']) : '';
	if ($label === '' || !isset($current['state']) || $current['state'] !== 'published')
		return (string)$axis;
	return $axis.' - '.htmlspecialchars($label, ENT_QUOTES, 'UTF-8');
}

/**
 * The words, deadline behavior and measures of one native category.
 *
 * @param int $axis Native category 1 or 2
 * @return array axis, mode, name (HTML-safe), value_word, time_boxed, deadline_caption, deadline_help, measures, purpose_help
 */
function dimension_business_mode_terms($axis)
{
	$axis = (int)$axis;
	$catalog = dimension_business_mode_catalog();
	$mode = dimension_business_mode_for_axis($axis);
	$entry = $catalog[$mode];
	$entry['axis'] = $axis;
	$entry['mode'] = $mode;
	$entry['name'] = htmlspecialchars(dimension_category_policy_label($axis), ENT_QUOTES, 'UTF-8');
	return $entry;
}

/**
 * Whether a "required by" date is a real deadline for this category (so a value can be overdue).
 *
 * @param int $axis Native category 1 or 2
 * @return bool
 */
function dimension_business_mode_deadline_applies($axis)
{
	$terms = dimension_business_mode_terms($axis);
	return $terms['time_boxed'];
}

/**
 * Categories whose values can be overdue, for the master search predicate.
 *
 * @return int[] Native categories in stored order; empty when none has a deadline
 */
function dimension_business_mode_overdue_axes()
{
	$axes = array();
	foreach (dimension_category_policy_axes() as $axis)
		if (dimension_business_mode_deadline_applies($axis))
			$axes[] = $axis;
	return $axes;
}

/**
 * Caption of the "required by" date for one or several categories.
 *
 * One shared caption when every listed category agrees, the general caption when they differ.
 *
 * @param int[] $axes Native categories the form may create
 * @return string
 */
function dimension_business_mode_deadline_caption($axes)
{
	$captions = array();
	foreach ((array)$axes as $axis) {
		$terms = dimension_business_mode_terms($axis);
		$captions[$terms['deadline_caption']] = true;
	}
	if (count($captions) === 1)
		return key($captions);
	$general = dimension_business_mode_catalog();
	return $general['']['deadline_caption'];
}

/**
 * Status of a master for the list: Closed, Overdue (only where a deadline applies) or Open.
 *
 * @param int $closed 1 when closed
 * @param string $due_date SQL date
 * @param string $today SQL date, or '' when unknown
 * @param int $axis Native category of the master
 * @return string Translated status
 */
function dimension_business_mode_status_text($closed, $due_date, $today, $axis)
{
	if ((int)$closed === 1)
		return _('Closed');
	if ($today !== '' && dimension_business_mode_deadline_applies($axis) && $due_date < $today)
		return _('Overdue');
	return _('Open');
}

/**
 * Performance columns relevant for a result list, in display order.
 *
 * With one category selected only that mode's measures are listed; with both (or an unknown choice) every
 * measure stays visible, because a mixed list must not hide a figure a value really has.
 *
 * @param int $type Selected category (0 for all)
 * @return string[] Subset of revenue, costs, result
 */
function dimension_business_mode_measures($type)
{
	$all = array('revenue', 'costs', 'result');
	$type = (int)$type;
	if ($type !== 1 && $type !== 2)
		return $all;
	$terms = dimension_business_mode_terms($type);
	$measures = array();
	foreach ($all as $measure)
		if (in_array($measure, $terms['measures'], true))
			$measures[] = $measure;
	return count($measures) ? $measures : $all;
}

/**
 * Sentences that explain what each enabled category is and how its date behaves.
 *
 * @param int $enabled_axes Company posting preference (0..2)
 * @return string[] HTML-safe plain sentences, one per enabled category
 */
function dimension_business_mode_entry_guide($enabled_axes)
{
	$lines = array();
	$enabled_axes = max(0, min(2, (int)$enabled_axes));
	for ($axis = 1; $axis <= $enabled_axes; $axis++) {
		$terms = dimension_business_mode_terms($axis);
		$lines[] = sprintf(_('Category %d (%s): %s %s'), $axis, $terms['name'], $terms['purpose_help'], $terms['deadline_help']);
	}
	return $lines;
}

/**
 * Reminder that category 1 keeps its payroll meaning whatever it is called.
 *
 * @param int $axis Native category
 * @return string Empty for category 2
 */
function dimension_business_mode_hrm_note($axis)
{
	if ((int)$axis !== 1)
		return '';
	return _('Category 1 is also the cost center used by payroll and HR. Changing its name changes only the wording you see, never how payroll costs are classified.');
}

/**
 * Whether the signed-in user may create or change Dimension masters (the existing master authority).
 *
 * @return bool
 */
function dimension_business_mode_can_edit()
{
	$user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	return is_object($user) && method_exists($user, 'can_access') && $user->can_access('SA_DIMENSION');
}

/**
 * Whether the signed-in user may open the company policy page (the existing Company Setup authority).
 *
 * @return bool
 */
function dimension_business_mode_can_configure()
{
	$user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	return is_object($user) && method_exists($user, 'can_access') && $user->can_access('SA_SETUPCOMPANY');
}

/**
 * Selector choices for both categories, history-only ones marked.
 *
 * Both categories are always listed: a category that is switched off for new entries keeps its history and
 * must stay searchable.
 *
 * @param int $enabled_axes Company posting preference (0..2)
 * @return array axis => HTML-safe option text
 */
function dimension_business_mode_type_choices($enabled_axes)
{
	$choices = array();
	foreach (dimension_category_policy_axes() as $axis) {
		$text = dimension_business_mode_axis_text($axis);
		$choices[$axis] = $axis > (int)$enabled_axes ? $text.' ('._('history only').')' : $text;
	}
	return $choices;
}

/**
 * Failure and empty states, each with its own title, explanation and next step.
 *
 * The wording avoids table names, authority codes and column names on purpose.
 *
 * @return array code => array(title, explanation, next_step)
 */
function dimension_business_mode_failure_states()
{
	return array(
		'denied' => array(
			'title' => _('You cannot do this'),
			'explanation' => _('Your role can look at this information but is not allowed to change it.'),
			'next_step' => _('Ask the person who manages roles to give you access, or ask a colleague who has it.'),
		),
		'not_enabled' => array(
			'title' => _('Nothing is switched on yet'),
			'explanation' => _('The company has not turned on any category for new entries, so new values cannot be created.'),
			'next_step' => _('A company administrator can turn on one or two categories in the company setup.'),
		),
		'history_only' => array(
			'title' => _('Switched off, history kept'),
			'explanation' => _('This category is not available for new entries any more, but everything recorded with it stays visible in lists and reports.'),
			'next_step' => _('Open the list to review the history, or ask an administrator to switch the category on again.'),
		),
		'no_policy' => array(
			'title' => _('No names chosen yet'),
			'explanation' => _('The company has not said what each category is used for, so general wording is shown.'),
			'next_step' => _('An administrator can name the categories, for example Project or Cost center.'),
		),
		'policy_invalid' => array(
			'title' => _('The saved names cannot be trusted'),
			'explanation' => _('The saved category names were changed outside the normal screen, so general wording is shown instead.'),
			'next_step' => _('An administrator should publish the names again. Nothing recorded so far is changed.'),
		),
		'policy_unavailable' => array(
			'title' => _('Naming is not installed'),
			'explanation' => _('This company has not been updated to the version that lets you name the categories.'),
			'next_step' => _('Ask your administrator to finish updating the company.'),
		),
		'empty' => array(
			'title' => _('Nothing here yet'),
			'explanation' => _('No value has been created in this category, so there is nothing to list.'),
			'next_step' => _('Create the first value, then choose it when you enter an expense.'),
		),
		'no_postings' => array(
			'title' => _('Created, but not used yet'),
			'explanation' => _('Values exist, but no expense or income has been coded to them yet, so every figure is zero.'),
			'next_step' => _('Enter an expense and choose the value on its line.'),
		),
		'no_deadline' => array(
			'title' => _('No category has a deadline'),
			'explanation' => _('Every category in this company is ongoing, such as a cost center or a department, so no value can be overdue.'),
			'next_step' => _('Remove the overdue filter to see the values.'),
		),
		'invalid_input' => array(
			'title' => _('That choice is not valid'),
			'explanation' => _('The page received a choice it does not offer, so nothing was shown.'),
			'next_step' => _('Pick one of the listed choices and try again.'),
		),
	);
}

/**
 * Title, explanation and next step of one failure state.
 *
 * @param string $code State code
 * @return array|false false for an unknown code
 */
function dimension_business_mode_failure_state($code)
{
	$states = dimension_business_mode_failure_states();
	return is_string($code) && isset($states[$code]) ? $states[$code] : false;
}

/**
 * Read the facts the onboarding steps depend on, from the active company only.
 *
 * Three bounded reads: category counts, and one existence probe per enabled category on the posted ledger.
 *
 * @return array enabled_axes, policy_state, axes[axis]{label,mode,enabled,masters,open_masters,has_postings}
 */
function dimension_onboarding_facts()
{
	$enabled = max(0, min(2, (int)get_company_pref('use_dimension')));
	$current = dimension_category_policy_current();
	$facts = array('enabled_axes' => $enabled, 'policy_state' => isset($current['state']) ? $current['state'] : 'neutral', 'axes' => array());
	foreach (dimension_category_policy_axes() as $axis)
		$facts['axes'][$axis] = array('axis' => $axis, 'label' => dimension_category_policy_label($axis), 'mode' => dimension_business_mode_for_axis($axis),
			'enabled' => $axis <= $enabled, 'masters' => 0, 'open_masters' => 0, 'has_postings' => false);
	$result = db_query('SELECT type_, COUNT(*), SUM(CASE WHEN closed=0 THEN 1 ELSE 0 END) FROM '.TB_PREF.'dimensions WHERE type_ IN (1,2) GROUP BY type_');
	while ($result && ($row = db_fetch_row($result))) {
		$facts['axes'][(int)$row[0]]['masters'] = (int)$row[1];
		$facts['axes'][(int)$row[0]]['open_masters'] = (int)$row[2];
	}
	foreach (array(1 => 'dimension_id', 2 => 'dimension2_id') as $axis => $column) {
		$probe = db_query('SELECT 1 FROM '.TB_PREF.'gl_trans WHERE '.$column.' > 0 LIMIT 1');
		$facts['axes'][$axis]['has_postings'] = $probe && db_fetch_row($probe) !== false;
	}
	return $facts;
}

/**
 * Failure state describing the naming policy, or '' when the names are in force.
 *
 * @param string $policy_state published|neutral|invalid|unavailable
 * @return string State code or ''
 */
function dimension_onboarding_policy_failure($policy_state)
{
	if ($policy_state === 'published')
		return '';
	if ($policy_state === 'invalid')
		return 'policy_invalid';
	if ($policy_state === 'unavailable')
		return 'policy_unavailable';
	return 'no_policy';
}

/**
 * The three onboarding steps with their state and the one action the caller may take.
 *
 * Status is `done`, `todo` or `blocked`. An action is offered only to a caller who holds the authority of the
 * page it links to; otherwise the step names the failure state instead of showing a dead button.
 *
 * @param array $facts Result of dimension_onboarding_facts()
 * @param bool $can_configure Company Setup authority
 * @param bool $can_edit Dimension master authority
 * @return array Three steps: number, title, text, status, state, extra_state, link, link_label
 */
function dimension_onboarding_steps($facts, $can_configure, $can_edit)
{
	$first = 0;
	foreach (dimension_category_policy_axes() as $axis)
		if (!empty($facts['axes'][$axis]['enabled'])) {
			$first = $axis;
			break;
		}
	// The wording comes from the mode carried by the facts, so the steps describe exactly the state they were given.
	$catalog = dimension_business_mode_catalog();
	$first_mode = $first && isset($facts['axes'][$first]['mode']) ? $facts['axes'][$first]['mode'] : '';
	$word = isset($catalog[$first_mode]) ? $catalog[$first_mode] : $catalog[''];
	$any_values = false;
	$any_postings = false;
	foreach ($facts['axes'] as $axis_facts) {
		if (!empty($axis_facts['enabled']) && $axis_facts['masters'] > 0)
			$any_values = true;
		if (!empty($axis_facts['enabled']) && !empty($axis_facts['has_postings']))
			$any_postings = true;
	}
	$policy_failure = dimension_onboarding_policy_failure($facts['policy_state']);

	// Step 1: say what is being tracked.
	$step1 = array('number' => 1, 'title' => _('Say what you want to track'),
		'text' => _('Give each category a name such as Project or Cost center, so everyone reads the same words.'),
		'status' => 'todo', 'state' => '', 'extra_state' => '', 'link' => '', 'link_label' => '');
	if ($facts['enabled_axes'] < 1) {
		$step1['status'] = 'blocked';
		$step1['state'] = 'not_enabled';
	}
	elseif ($policy_failure === '') {
		$step1['status'] = 'done';
	}
	else {
		$step1['state'] = $policy_failure;
		if ($policy_failure === 'policy_unavailable')
			$step1['status'] = 'blocked';
		elseif ($can_configure) {
			$step1['link'] = 'dimensions/category_policy.php?';
			$step1['link_label'] = _('Name the categories');
		}
		else
			$step1['extra_state'] = 'denied';
	}

	// Step 2: create the first value.
	$step2 = array('number' => 2, 'title' => sprintf(_('Create your first %s'), $word['value_word']),
		'text' => _('Enter a name, a start date and, where it applies, a date it is needed by.'),
		'status' => 'todo', 'state' => '', 'extra_state' => '', 'link' => '', 'link_label' => '');
	if ($facts['enabled_axes'] < 1) {
		$step2['status'] = 'blocked';
		$step2['state'] = 'not_enabled';
	}
	elseif ($any_values)
		$step2['status'] = 'done';
	elseif ($can_edit) {
		$step2['state'] = 'empty';
		$step2['link'] = 'dimensions/dimension_entry.php?';
		$step2['link_label'] = sprintf(_('Create a %s'), $word['value_word']);
	}
	else {
		$step2['state'] = 'empty';
		$step2['extra_state'] = 'denied';
	}

	// Step 3: code an expense and check where it went.
	$step3 = array('number' => 3, 'title' => _('Code an expense and see where it went'),
		'text' => sprintf(_('On an expense, payment or journal line, choose the %s. Then open the tracking page to see the amount and the document it came from.'), $word['value_word']),
		'status' => 'todo', 'state' => '', 'extra_state' => '', 'link' => '', 'link_label' => '');
	if ($facts['enabled_axes'] < 1) {
		$step3['status'] = 'blocked';
		$step3['state'] = 'not_enabled';
	}
	elseif ($any_postings)
		$step3['status'] = 'done';
	else {
		$step3['state'] = $any_values ? 'no_postings' : 'empty';
		$step3['link'] = 'dimensions/inquiry/dimension_tracking.php?';
		$step3['link_label'] = _('Open financial tracking');
	}
	return array($step1, $step2, $step3);
}

/**
 * Categories switched off for new entries that still hold history.
 *
 * @param array $facts Result of dimension_onboarding_facts()
 * @return array List of axis, label, masters, has_postings
 */
function dimension_onboarding_history_only($facts)
{
	$history = array();
	foreach ($facts['axes'] as $axis => $axis_facts)
		if (empty($axis_facts['enabled']) && ($axis_facts['masters'] > 0 || !empty($axis_facts['has_postings'])))
			$history[] = array('axis' => (int)$axis, 'label' => $axis_facts['label'], 'masters' => (int)$axis_facts['masters'], 'has_postings' => !empty($axis_facts['has_postings']));
	return $history;
}

/**
 * Names of the synthetic demos.
 *
 * @return array code => array(title, description)
 */
function dimension_demo_catalog()
{
	return array(
		'cost_only' => array(
			'title' => _('Cost centers only'),
			'description' => _('One category, no income: an office, a workshop and vehicles collect their costs.'),
		),
		'department_project' => array(
			'title' => _('Department and project'),
			'description' => _('Two categories at once: each line says which department paid and which project it served.'),
		),
	);
}

/**
 * The synthetic lines of one demo, in integer cents, or false for an unknown demo.
 *
 * Nothing here is company data. `kind` is revenue (credit-positive) or cost (debit-positive); a value of 0 means
 * the line carries no value in that category (shown as Not assigned).
 *
 * @param string $demo Demo code
 * @return array|false title, axes{label,values}, lines{number,source,document,account,kind,cents,d1,d2}
 */
function dimension_demo_dataset($demo)
{
	if (!is_string($demo) || !isset(dimension_demo_catalog()[$demo]))
		return false;
	$catalog = dimension_demo_catalog();
	if ($demo === 'cost_only') {
		return array('demo' => $demo, 'title' => $catalog[$demo]['title'],
			'axes' => array(
				1 => array('label' => _('Cost center'), 'values' => array(1 => _('Office'), 2 => _('Workshop'), 3 => _('Vehicles'))),
				2 => array('label' => '', 'values' => array())),
			'lines' => array(
				array('number' => 1, 'source' => _('Supplier invoice'), 'document' => 'SI-1001', 'account' => _('Rent'), 'kind' => 'cost', 'cents' => 120000, 'd1' => 1, 'd2' => 0),
				array('number' => 2, 'source' => _('Supplier invoice'), 'document' => 'SI-1002', 'account' => _('Electricity'), 'kind' => 'cost', 'cents' => 45050, 'd1' => 2, 'd2' => 0),
				array('number' => 3, 'source' => _('Bank payment'), 'document' => 'BP-2001', 'account' => _('Fuel'), 'kind' => 'cost', 'cents' => 31075, 'd1' => 3, 'd2' => 0),
				array('number' => 4, 'source' => _('Supplier invoice'), 'document' => 'SI-1003', 'account' => _('Repairs'), 'kind' => 'cost', 'cents' => 18025, 'd1' => 3, 'd2' => 0),
				array('number' => 5, 'source' => _('Journal entry'), 'document' => 'JE-3001', 'account' => _('Insurance'), 'kind' => 'cost', 'cents' => 24000, 'd1' => 1, 'd2' => 0),
				array('number' => 6, 'source' => _('Bank payment'), 'document' => 'BP-2002', 'account' => _('Office supplies'), 'kind' => 'cost', 'cents' => 9950, 'd1' => 0, 'd2' => 0)));
	}
	return array('demo' => $demo, 'title' => $catalog[$demo]['title'],
		'axes' => array(
			1 => array('label' => _('Department'), 'values' => array(1 => _('Sales'), 2 => _('Operations'))),
			2 => array('label' => _('Project'), 'values' => array(1 => _('Alpha'), 2 => _('Beta')))),
		'lines' => array(
			array('number' => 1, 'source' => _('Customer invoice'), 'document' => 'CI-5001', 'account' => _('Services income'), 'kind' => 'revenue', 'cents' => 500000, 'd1' => 1, 'd2' => 1),
			array('number' => 2, 'source' => _('Customer invoice'), 'document' => 'CI-5002', 'account' => _('Services income'), 'kind' => 'revenue', 'cents' => 320000, 'd1' => 1, 'd2' => 2),
			array('number' => 3, 'source' => _('Supplier invoice'), 'document' => 'SI-6001', 'account' => _('Materials'), 'kind' => 'cost', 'cents' => 180000, 'd1' => 2, 'd2' => 1),
			array('number' => 4, 'source' => _('Supplier invoice'), 'document' => 'SI-6002', 'account' => _('Materials'), 'kind' => 'cost', 'cents' => 95025, 'd1' => 2, 'd2' => 2),
			array('number' => 5, 'source' => _('Payroll journal'), 'document' => 'JE-7001', 'account' => _('Salaries'), 'kind' => 'cost', 'cents' => 150000, 'd1' => 1, 'd2' => 1),
			array('number' => 6, 'source' => _('Bank payment'), 'document' => 'BP-8001', 'account' => _('Travel'), 'kind' => 'cost', 'cents' => 24050, 'd1' => 1, 'd2' => 0),
			array('number' => 7, 'source' => _('Journal entry'), 'document' => 'JE-7002', 'account' => _('Software'), 'kind' => 'cost', 'cents' => 12075, 'd1' => 0, 'd2' => 2),
			array('number' => 8, 'source' => _('Customer invoice'), 'document' => 'CI-5003', 'account' => _('Services income'), 'kind' => 'revenue', 'cents' => 40000, 'd1' => 0, 'd2' => 0)));
}

/**
 * Signed result contribution of a demo line in cents: revenue adds, cost subtracts.
 *
 * @param array $line Demo line
 * @return int
 */
function dimension_demo_line_result_cents($line)
{
	return $line['kind'] === 'revenue' ? (int)$line['cents'] : -(int)$line['cents'];
}

/**
 * Totals of a demo by value on each category, with what has no value shown as Not assigned.
 *
 * Each category is recomputed from the lines on its own, so it is an independent view of the same total.
 *
 * @param array $dataset Result of dimension_demo_dataset()
 * @return array by_axis[axis][value|0]{revenue,costs,result}, matrix[d1][d2] result, totals{revenue,costs,result}
 */
function dimension_demo_totals($dataset)
{
	$blank = array('revenue' => 0, 'costs' => 0, 'result' => 0);
	$totals = $blank;
	$by_axis = array(1 => array(), 2 => array());
	$matrix = array();
	foreach ($dataset['lines'] as $line) {
		$revenue = $line['kind'] === 'revenue' ? (int)$line['cents'] : 0;
		$cost = $line['kind'] === 'cost' ? (int)$line['cents'] : 0;
		$totals['revenue'] += $revenue;
		$totals['costs'] += $cost;
		$totals['result'] += $revenue - $cost;
		foreach (array(1 => 'd1', 2 => 'd2') as $axis => $field) {
			$key = (int)$line[$field];
			if (!isset($by_axis[$axis][$key]))
				$by_axis[$axis][$key] = $blank;
			$by_axis[$axis][$key]['revenue'] += $revenue;
			$by_axis[$axis][$key]['costs'] += $cost;
			$by_axis[$axis][$key]['result'] += $revenue - $cost;
		}
		$d1 = (int)$line['d1'];
		$d2 = (int)$line['d2'];
		$matrix[$d1][$d2] = (isset($matrix[$d1][$d2]) ? $matrix[$d1][$d2] : 0) + $revenue - $cost;
	}
	return array('by_axis' => $by_axis, 'matrix' => $matrix, 'totals' => $totals);
}

/**
 * Reconcile a demo: every view must add up to the same revenue, cost and result.
 *
 * @param array $dataset Result of dimension_demo_dataset()
 * @return array checks[]{code,label,expected,actual,ok}, ok
 */
function dimension_demo_reconcile($dataset)
{
	$view = dimension_demo_totals($dataset);
	$checks = array();
	$push = function ($code, $label, $expected, $actual) use (&$checks) {
		$checks[] = array('code' => $code, 'label' => $label, 'expected' => (int)$expected, 'actual' => (int)$actual, 'ok' => (int)$expected === (int)$actual);
	};
	$line_cost = 0;
	$line_revenue = 0;
	foreach ($dataset['lines'] as $line) {
		if ($line['kind'] === 'revenue')
			$line_revenue += (int)$line['cents'];
		else
			$line_cost += (int)$line['cents'];
	}
	$push('revenue', _('Income of all lines equals the income shown'), $line_revenue, $view['totals']['revenue']);
	$push('costs', _('Costs of all lines equal the costs shown'), $line_cost, $view['totals']['costs']);
	foreach (array(1, 2) as $axis) {
		if (!count($dataset['axes'][$axis]['values']))
			continue;
		$sum = 0;
		foreach ($view['by_axis'][$axis] as $row)
			$sum += $row['result'];
		$push('axis'.$axis, sprintf(_('Result by %s plus not assigned equals the total result'), $dataset['axes'][$axis]['label']), $view['totals']['result'], $sum);
	}
	if (count($dataset['axes'][1]['values']) && count($dataset['axes'][2]['values'])) {
		$sum = 0;
		foreach ($view['matrix'] as $row)
			foreach ($row as $cell)
				$sum += $cell;
		$push('matrix', _('Every combination of the two categories adds up to the total result'), $view['totals']['result'], $sum);
	}
	$ok = true;
	foreach ($checks as $check)
		$ok = $ok && $check['ok'];
	return array('checks' => $checks, 'ok' => $ok);
}

/**
 * Plain sentence that explains where one demo line came from.
 *
 * @param array $dataset Result of dimension_demo_dataset()
 * @param array $line One demo line
 * @return string Plain text (the caller escapes it for HTML)
 */
function dimension_demo_explain_line($dataset, $line)
{
	$parts = array();
	foreach (array(1 => 'd1', 2 => 'd2') as $axis => $field) {
		if (!count($dataset['axes'][$axis]['values']))
			continue;
		$value = (int)$line[$field];
		$parts[] = $dataset['axes'][$axis]['label'].': '.($value > 0 && isset($dataset['axes'][$axis]['values'][$value]) ? $dataset['axes'][$axis]['values'][$value] : _('Not assigned'));
	}
	return sprintf($line['kind'] === 'revenue' ? _('%s %s brought in %s in %s.') : _('%s %s cost %s in %s.'),
		$line['source'], $line['document'], number_format2($line['cents'] / 100, 2), $line['account']).' '.implode(', ', $parts).'.';
}
