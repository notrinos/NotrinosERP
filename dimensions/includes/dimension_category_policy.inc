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
 * DIM-CFG-001: company Dimension category policy and axis transition preview.
 *
 * The two native Dimension categories stay exactly what they always were: the numeric axes
 * `dimensions.type_` 1 and 2, with axis 1 remaining the HRM Cost Center binding whatever the company
 * calls it. A company may publish an append-only, versioned policy that gives each category a
 * semantic label, a declared purpose, a required flag and optional account/value-pair rules.
 *
 * The policy is descriptive and versioned, never destructive:
 *  - a fresh company has no policy row, so every label is neutral and every rule permissive;
 *  - a published version never updates an earlier one, never renumbers a master, never rewrites a
 *    stored `dimension_id`/`dimension2_id` and never changes HRM/payroll storage;
 *  - reducing the enabled axis count is previewed here and refused while an active source, a stored
 *    default, a pending draft or an HRM binding still depends on the category being disabled.
 *
 * Posting-time enforcement of the declared required/account/value-pair rules belongs to
 * DIM-FND-005/DIM-FND-007; this file only stores and explains the declared policy.
 */

/**
 * The native Dimension categories, in their fixed stored order.
 *
 * @return int[]
 */
function dimension_category_policy_axes()
{
	return array(1, 2);
}

/**
 * Declared purposes a category may carry.
 *
 * The empty purpose means "not declared", which is the neutral state of a company that never
 * published a policy. `cost_center` is the only purpose HRM accepts for axis 1.
 *
 * @return array Declared identifier => translated purpose name
 */
function dimension_category_policy_purposes()
{
	return array(
		'' => _('Not declared'),
		'cost_center' => _('Cost center'),
		'department' => _('Department'),
		'project' => _('Project'),
		'activity' => _('Activity'),
		'segment' => _('Segment'),
	);
}

/**
 * Rule kinds a policy version may declare.
 *
 * @return array Kind => translated name
 */
function dimension_category_policy_rule_kinds()
{
	return array(
		'account' => _('Account rule'),
		'value_pair' => _('Value pair'),
	);
}

/**
 * Neutral state of one category before any policy version is published.
 *
 * @param int $axis Native category 1 or 2
 * @return array label/purpose/required with no semantic claim
 */
function dimension_category_policy_neutral_axis($axis)
{
	return array('label' => '', 'purpose' => '', 'required' => 0, 'axis' => (int)$axis);
}

/**
 * Neutral display name of one category.
 *
 * These names are deliberately generic: a fresh company must not inherit a "Project" or
 * "Department" meaning it never accepted, because that would misstate what is already stored.
 *
 * @param int $axis Native category 1 or 2
 * @return string Translated neutral name
 */
function dimension_category_policy_neutral_label($axis)
{
	return (int)$axis === 2 ? _('Dimension 2') : _('Dimension');
}

/**
 * Check one physical table of the active company prefix.
 *
 * The prefix is resolved by `db_query()` from the same table token the rest of the module uses, so
 * the check is company-scoped and never inspects another company's schema.
 *
 * @param string $table Unprefixed table name, or null to only reset the cached answers
 * @param bool $refresh Re-read the schema instead of reusing this request's answer
 * @return string available|absent|unknown
 */
function dimension_category_policy_table_state($table = null, $refresh = false)
{
	static $states = array();
	if ($refresh)
		$states = array();
	if (!is_string($table))
		return 'unknown';
	if (!preg_match('/^[a-z][a-z0-9_]*$/D', $table))
		return 'unknown';
	if (isset($states[$table]))
		return $states[$table];
	$result = db_query('SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE()'
		." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."' AND TABLE_TYPE='BASE TABLE'");
	if (!$result)
		return $states[$table] = 'unknown';
	$row = db_fetch_row($result);
	return $states[$table] = ($row && (int)$row[0] === 1) ? 'available' : 'absent';
}

/**
 * Check that one physical column of an active-company table exists.
 *
 * @param string $table Unprefixed table name
 * @param string $column Exact column name
 * @return bool True only for an exact base-table column in the active database
 */
function dimension_category_policy_column_available($table, $column)
{
	if (dimension_category_policy_table_state($table) !== 'available')
		return false;
	if (!is_string($column) || !preg_match('/^[a-z][a-z0-9_]*$/D', $column))
		return false;
	$result = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
		." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
		." AND BINARY COLUMN_NAME=BINARY '".db_escape_function($column)."'");
	if (!$result)
		return false;
	$row = db_fetch_row($result);
	return $row && (int)$row[0] === 1;
}

/**
 * Are both policy tables installed for the active company?
 *
 * @param bool $refresh Re-read the schema instead of reusing this request's answer
 * @return bool
 */
function dimension_category_policy_tables_available($refresh = false)
{
	if ($refresh)
		dimension_category_policy_table_state(null, true);
	return dimension_category_policy_table_state('dimension_category_policies') === 'available'
		&& dimension_category_policy_table_state('dimension_category_policy_rules') === 'available';
}

/**
 * Count rows matching a predicate, failing closed when the source cannot be verified.
 *
 * @param string $table Unprefixed table name
 * @param string[] $columns Columns the predicate needs
 * @param string $where Trusted source-owned predicate
 * @return array count and state (counted|absent|unknown)
 */
function dimension_category_policy_count($table, $columns, $where)
{
	foreach ($columns as $column)
		if (!dimension_category_policy_column_available($table, $column))
			return array('count' => 0, 'state' => 'absent');
	$result = db_query('SELECT COUNT(*) FROM '.TB_PREF.$table.' WHERE '.$where);
	if (!$result)
		return array('count' => 0, 'state' => 'unknown');
	$row = db_fetch_row($result);
	if (!$row)
		return array('count' => 0, 'state' => 'unknown');
	return array('count' => (int)$row[0], 'state' => 'counted');
}

/**
 * Canonical digest of one policy version.
 *
 * The digest seals the complete declared content (both categories plus every rule) so a tampered or
 * partially written version cannot be mistaken for an accepted company policy.
 *
 * @param int $version Published version number
 * @param array $axes Axis => label/purpose/required
 * @param array $rules Declared rule rows
 * @return string Lower-case SHA-256 hex digest
 */
function dimension_category_policy_digest($version, $axes, $rules)
{
	$canonical = 'dimension-category-policy|'.(int)$version."\n";
	foreach (dimension_category_policy_axes() as $axis) {
		$entry = isset($axes[$axis]) ? $axes[$axis] : dimension_category_policy_neutral_axis($axis);
		$canonical .= 'A|'.(int)$axis.'|'.(string)$entry['label'].'|'.(string)$entry['purpose'].'|'
			.(int)$entry['required']."\n";
	}
	$ordered = array();
	foreach ($rules as $rule)
		$ordered[] = array((int)$rule['axis'], (string)$rule['kind'], (string)$rule['left_value'],
			(string)$rule['right_value'], (string)$rule['note']);
	sort($ordered);
	foreach ($ordered as $rule)
		$canonical .= 'R|'.$rule[0].'|'.$rule[1].'|'.$rule[2].'|'.$rule[3].'|'.$rule[4]."\n";
	return hash('sha256', $canonical);
}

/**
 * Read one stored policy version and verify its digest.
 *
 * @param int $version Stored version number
 * @return array|false Complete verified version, or false for a missing/tampered/partial version
 */
function dimension_category_policy_version($version)
{
	$version = (int)$version;
	if ($version < 1 || !dimension_category_policy_tables_available())
		return false;
	$result = db_query('SELECT * FROM '.TB_PREF.'dimension_category_policies WHERE version='.$version.' ORDER BY axis');
	if (!$result)
		return false;
	$axes = array();
	$digest = false;
	$created_at = '';
	$created_by = 0;
	while ($row = db_fetch($result)) {
		$axis = (int)$row['axis'];
		if (($axis !== 1 && $axis !== 2) || isset($axes[$axis]))
			return false;
		$axes[$axis] = array('axis' => $axis, 'label' => (string)$row['label'],
			'purpose' => (string)$row['purpose'], 'required' => (int)$row['required']);
		$digest = (string)$row['digest'];
		$created_at = (string)$row['created_at'];
		$created_by = (int)$row['created_by'];
	}
	if (count($axes) !== 2 || $digest === '')
		return false;
	$rules = array();
	$rule_result = db_query('SELECT axis,kind,left_value,right_value,note FROM '.TB_PREF.'dimension_category_policy_rules'
		.' WHERE version='.$version.' ORDER BY axis,kind,left_value,right_value,note');
	if (!$rule_result)
		return false;
	while ($row = db_fetch($rule_result))
		$rules[] = array('axis' => (int)$row['axis'], 'kind' => (string)$row['kind'],
			'left_value' => (string)$row['left_value'], 'right_value' => (string)$row['right_value'],
			'note' => (string)$row['note']);
	if (dimension_category_policy_digest($version, $axes, $rules) !== $digest)
		return false;
	return array('version' => $version, 'state' => 'published', 'axes' => $axes, 'rules' => $rules,
		'digest' => $digest, 'created_at' => $created_at, 'created_by' => $created_by);
}

/**
 * Published policy versions, newest first.
 *
 * @param int $limit Maximum number of versions to return
 * @return array Version => array(version, created_at, created_by, rule_count, digest)
 */
function dimension_category_policy_versions($limit = 20)
{
	$versions = array();
	if (!dimension_category_policy_tables_available())
		return $versions;
	$limit = max(1, min(100, (int)$limit));
	$result = db_query('SELECT version,MAX(created_at) created_at,MAX(created_by) created_by,MAX(digest) digest,'
		.'COUNT(*) axes FROM '.TB_PREF.'dimension_category_policies GROUP BY version ORDER BY version DESC LIMIT '.$limit);
	if (!$result)
		return $versions;
	while ($row = db_fetch($result)) {
		$version = (int)$row['version'];
		$rules = dimension_category_policy_count('dimension_category_policy_rules', array('version'),
			'version='.$version);
		$versions[$version] = array('version' => $version, 'created_at' => (string)$row['created_at'],
			'created_by' => (int)$row['created_by'], 'rule_count' => $rules['count'],
			'digest' => (string)$row['digest'], 'complete' => (int)$row['axes'] === 2);
	}
	return $versions;
}

/**
 * The policy in force for the active company.
 *
 * States:
 *  - `neutral`     no version published: neutral labels, permissive rules (the initial state);
 *  - `published`   a verified version is in force;
 *  - `invalid`     a version exists but is partial or its digest does not verify, so neutral labels
 *                  are used and no declared rule is trusted;
 *  - `unavailable` the policy tables are not installed for this company.
 *
 * @param bool $refresh Re-read the database instead of reusing this request's answer
 * @return array version/state/axes/rules
 */
function dimension_category_policy_current($refresh = false)
{
	static $current = null;
	if (!$refresh && $current !== null)
		return $current;
	$neutral = array('version' => 0, 'state' => 'neutral', 'rules' => array(), 'digest' => '',
		'created_at' => '', 'created_by' => 0);
	foreach (dimension_category_policy_axes() as $axis)
		$neutral['axes'][$axis] = dimension_category_policy_neutral_axis($axis);
	if (!dimension_category_policy_tables_available($refresh)) {
		$neutral['state'] = 'unavailable';
		return $current = $neutral;
	}
	$result = db_query('SELECT COALESCE(MAX(version),0) FROM '.TB_PREF.'dimension_category_policies');
	if (!$result)
		return $current = $neutral;
	$row = db_fetch_row($result);
	$version = $row ? (int)$row[0] : 0;
	if ($version < 1)
		return $current = $neutral;
	$published = dimension_category_policy_version($version);
	if ($published === false) {
		$neutral['version'] = $version;
		$neutral['state'] = 'invalid';
		return $current = $neutral;
	}
	return $current = $published;
}

/**
 * Display name of one category in the active company.
 *
 * A published label is descriptive only; it never renumbers or remaps the stored axis.
 *
 * @param int $axis Native category 1 or 2
 * @return string Published label, or the neutral name when none is declared
 */
function dimension_category_policy_label($axis)
{
	$axis = (int)$axis;
	if ($axis !== 1 && $axis !== 2)
		return dimension_category_policy_neutral_label(0);
	$current = dimension_category_policy_current();
	$label = isset($current['axes'][$axis]['label']) ? trim((string)$current['axes'][$axis]['label']) : '';
	return $label === '' ? dimension_category_policy_neutral_label($axis) : $label;
}

/**
 * Selector text for every category currently enabled for new entries.
 *
 * @param int $enabled_axes Company posting preference (0..2)
 * @return array Axis => text, e.g. 1 => "1 - Account (Project)"
 */
function dimension_category_policy_axis_choices($enabled_axes)
{
	$choices = array();
	$enabled_axes = max(0, min(2, (int)$enabled_axes));
	for ($axis = 1; $axis <= $enabled_axes; $axis++) {
		$current = dimension_category_policy_current();
		$label = isset($current['axes'][$axis]['label']) ? trim((string)$current['axes'][$axis]['label']) : '';
		// A published label is company data, so it is HTML-escaped before it reaches a selector.
		$choices[$axis] = $label === '' ? (string)$axis
			: $axis.' - '.htmlspecialchars($label, ENT_QUOTES, 'UTF-8');
	}
	return $choices;
}

/**
 * Does the actor hold the explicit policy authority?
 *
 * DIM-CFG-001 requires two independent authorities: Company Setup (`SA_SETUPCOMPANY`, the page that
 * applies an axis count) and the Finance-owned Dimension master authority (`SA_DIMENSION`). Holding
 * only one of them can neither publish a policy nor apply a transition.
 *
 * @return bool
 */
function dimension_category_policy_authorized()
{
	$user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	return is_object($user) && method_exists($user, 'can_access')
		&& $user->can_access('SA_SETUPCOMPANY') && $user->can_access('SA_DIMENSION');
}

/**
 * Trim and length-check one free-text policy value.
 *
 * @param mixed $value Raw value
 * @param int $maximum Maximum accepted characters
 * @return string|null Trimmed value, or null when it contains a control character or is too long
 */
function dimension_category_policy_clean_text($value, $maximum)
{
	if (!is_scalar($value))
		return null;
	$value = trim((string)$value);
	if (preg_match('/[\x00-\x1F\x7F<>]/', $value))
		return null;
	$length = function_exists('mb_strlen') ? mb_strlen($value, 'UTF-8') : strlen($value);
	return $length > (int)$maximum ? null : $value;
}

/**
 * Parse and validate a declared rule in its canonical form.
 *
 * @param array $rule axis/kind/left_value/right_value/note
 * @return array|string Normalised rule, or an error message
 */
function dimension_category_policy_validate_rule($rule)
{
	$kinds = dimension_category_policy_rule_kinds();
	$kind = isset($rule['kind']) ? (string)$rule['kind'] : '';
	$axis = isset($rule['axis']) ? (int)$rule['axis'] : 1;
	$left = isset($rule['left_value']) ? trim((string)$rule['left_value']) : '';
	$right = isset($rule['right_value']) ? trim((string)$rule['right_value']) : '';
	$note = isset($rule['note']) ? dimension_category_policy_clean_text($rule['note'], 255) : '';
	if ($note === null)
		return _('A rule note contains a control character or is too long.');
	if (!isset($kinds[$kind]))
		return _('Unknown rule kind. Only account rules and value pairs are supported.');
	if ($kind === 'account') {
		if ($axis !== 1 && $axis !== 2)
			return _('An account rule must name category 1 or 2.');
		if (!in_array($left, array('1', '2', '3', '4', '5', '6'), true))
			return _('An account rule needs a GL account type between 1 and 6.');
		if ($right !== '')
			return _('An account rule has no second value.');
	} else {
		// `*` means "any value" and is stored as empty, so a pair of two wildcards names no Dimension at all.
		if (($left === '' || $left === '*') && ($right === '' || $right === '*'))
			return _('A value pair needs at least one stored Dimension value.');
		foreach (array($left, $right) as $side) {
			if ($side === '' || $side === '*')
				continue;
			if (!preg_match('/^[1-9][0-9]*$/D', $side))
				return _('A value pair accepts only a stored Dimension identity or * for any value.');
		}
		$axis = 1;
	}
	return array('axis' => $axis, 'kind' => $kind, 'left_value' => $left === '*' ? '' : $left,
		'right_value' => $right === '*' ? '' : $right, 'note' => (string)$note);
}

/**
 * Validate a complete policy input before a version is created.
 *
 * @param array $input axes (1/2 => label/purpose/required) and rules
 * @return array errors (strings) and cleaned axes/rules
 */
function dimension_category_policy_validate($input)
{
	$errors = array();
	$purposes = dimension_category_policy_purposes();
	$axes = array();
	$hrm_enabled = (int)get_company_pref('use_hrm') === 1;
	$enabled_axes = (int)get_company_pref('use_dimension');
	foreach (dimension_category_policy_axes() as $axis) {
		$entry = isset($input['axes'][$axis]) && is_array($input['axes'][$axis])
			? $input['axes'][$axis] : array();
		$label = dimension_category_policy_clean_text(isset($entry['label']) ? $entry['label'] : '', 64);
		$purpose = isset($entry['purpose']) ? (string)$entry['purpose'] : '';
		$required = !empty($entry['required']) ? 1 : 0;
		$name = dimension_category_policy_neutral_label($axis);
		if ($label === null) {
			$errors[] = sprintf(_('The %s label must be at most 64 characters without control characters or angle brackets.'), $name);
			$label = '';
		}
		if (!isset($purposes[$purpose])) {
			$errors[] = sprintf(_('The %s purpose is not a declared purpose.'), $name);
			$purpose = '';
		}
		// AT-11: renaming axis 1 never changes what HRM stores, and while HRM is enabled the declared
		// purpose may not pretend the category is something other than the Cost Center it binds.
		if ($axis === 1 && $hrm_enabled && $purpose !== 'cost_center') {
			$errors[] = _('Axis 1 is the HRM Cost Center while Human Resources is enabled; its purpose must stay "Cost center". The label is descriptive and never changes what HRM stores.');
			$purpose = 'cost_center';
		}
		if ($required === 1 && $axis > $enabled_axes) {
			$errors[] = $enabled_axes === 1
				? sprintf(_('%s cannot be required while only one category is enabled for new entries.'), $name)
				: sprintf(_('%s cannot be required while no category is enabled for new entries.'), $name);
			$required = 0;
		}
		$axes[$axis] = array('axis' => $axis, 'label' => $label, 'purpose' => $purpose, 'required' => $required);
	}
	$rules = array();
	$input_rules = isset($input['rules']) && is_array($input['rules']) ? $input['rules'] : array();
	foreach ($input_rules as $index => $rule) {
		$clean = dimension_category_policy_validate_rule(is_array($rule) ? $rule : array());
		if (is_string($clean)) {
			$errors[] = sprintf(_('Rule %d: %s'), (int)$index + 1, $clean);
			continue;
		}
		$rules[$clean['axis'].'|'.$clean['kind'].'|'.$clean['left_value'].'|'.$clean['right_value'].'|'.$clean['note']] = $clean;
	}
	$rules = array_values($rules);
	foreach ($rules as $rule) {
		if ($rule['kind'] !== 'value_pair')
			continue;
		foreach (array(1 => $rule['left_value'], 2 => $rule['right_value']) as $axis => $identity) {
			if ($identity === '')
				continue;
			$found = db_query('SELECT type_ FROM '.TB_PREF.'dimensions WHERE id='.db_escape($identity));
			$row = $found ? db_fetch($found) : false;
			if (!$row)
				$errors[] = sprintf(_('Value pair %s / %s names a Dimension that no longer exists.'),
					$rule['left_value'] === '' ? '*' : $rule['left_value'],
					$rule['right_value'] === '' ? '*' : $rule['right_value']);
			elseif ((int)$row['type_'] !== (int)$axis)
				$errors[] = sprintf(_('Value pair %s / %s uses a Dimension from the wrong category.'),
					$rule['left_value'] === '' ? '*' : $rule['left_value'],
					$rule['right_value'] === '' ? '*' : $rule['right_value']);
		}
	}
	return array('errors' => $errors, 'axes' => $axes, 'rules' => $rules);
}

/**
 * Parse the documented line format used by the policy page.
 *
 * Each non-empty line is either `account:<axis>:<account_type>` or `pair:<axis1 id>:<axis2 id>`,
 * where a value-pair side may be `*` for "any value". Blank lines and `#` comments are ignored.
 *
 * @param string $text Posted rule text
 * @return array rules (normalised input rows) and errors
 */
function dimension_category_policy_parse_rules($text)
{
	$rules = array();
	$errors = array();
	$lines = preg_split('/\r\n|\n|\r/', (string)$text);
	foreach ($lines as $number => $line) {
		$line = trim($line);
		if ($line === '' || substr($line, 0, 1) === '#')
			continue;
		if (preg_match('/^account\s*:\s*([12])\s*:\s*([1-6])$/i', $line, $match)) {
			$rules[] = array('axis' => (int)$match[1], 'kind' => 'account',
				'left_value' => $match[2], 'right_value' => '', 'note' => '');
			continue;
		}
		if (preg_match('/^pair\s*:\s*([1-9][0-9]*|\*)\s*:\s*([1-9][0-9]*|\*)$/i', $line, $match)) {
			$rules[] = array('axis' => 1, 'kind' => 'value_pair',
				'left_value' => $match[1], 'right_value' => $match[2], 'note' => '');
			continue;
		}
		$errors[] = sprintf(_('Rule line %d is not "account:<category>:<account type>" or "pair:<value>:<value>".'), (int)$number + 1);
	}
	return array('rules' => $rules, 'errors' => $errors);
}

/**
 * Canonical text of the declared rules of one policy version.
 *
 * @param array $rules Declared rules
 * @return string One documented line per rule
 */
function dimension_category_policy_rules_text($rules)
{
	$text = '';
	foreach ($rules as $rule) {
		if ($rule['kind'] === 'account')
			$text .= 'account:'.(int)$rule['axis'].':'.$rule['left_value']."\n";
		else
			$text .= 'pair:'.($rule['left_value'] === '' ? '*' : $rule['left_value']).':'
				.($rule['right_value'] === '' ? '*' : $rule['right_value'])."\n";
	}
	return $text;
}

/**
 * Publish a new policy version, append-only and never in place.
 *
 * The whole version is written inside one transaction that first takes the company's `sys_prefs`
 * version row exclusively, so two concurrent publishers cannot allocate the same version number.
 * No earlier version is updated and no Dimension master, GL row, budget row, draft or HRM row is
 * touched. The version rows themselves are the immutable record: actor, timestamp and digest.
 *
 * @param array $input axes and rules, see dimension_category_policy_validate()
 * @return array published, version and errors
 */
function dimension_category_policy_publish($input)
{
	$failed = array('published' => false, 'version' => 0, 'errors' => array());
	if (!dimension_category_policy_authorized()) {
		$failed['errors'][] = _('Publishing a Dimension category policy requires both Company Setup and Dimension entry authority.');
		return $failed;
	}
	if (!dimension_category_policy_tables_available()) {
		$failed['errors'][] = _('The Dimension category policy schema is not installed for this company.');
		return $failed;
	}
	$validation = dimension_category_policy_validate($input);
	if (count($validation['errors'])) {
		$failed['errors'] = $validation['errors'];
		return $failed;
	}
	$axes = $validation['axes'];
	$rules = $validation['rules'];
	$actor = isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
	if (!begin_transaction()) {
		$failed['errors'][] = _('The policy version could not be stored; reload the page and publish again.');
		return $failed;
	}
	$mutex = db_query("SELECT value FROM ".TB_PREF."sys_prefs WHERE name='version_id' FOR UPDATE");
	if (!$mutex || !db_num_rows($mutex)) {
		cancel_transaction();
		$failed['errors'][] = _('The policy version could not be stored; reload the page and publish again.');
		return $failed;
	}
	$max = db_query('SELECT COALESCE(MAX(version),0) FROM '.TB_PREF.'dimension_category_policies');
	$max_row = $max ? db_fetch_row($max) : false;
	if (!$max_row) {
		cancel_transaction();
		$failed['errors'][] = _('The policy version could not be stored; reload the page and publish again.');
		return $failed;
	}
	$version = (int)$max_row[0] + 1;
	$digest = dimension_category_policy_digest($version, $axes, $rules);
	$values = array();
	foreach (dimension_category_policy_axes() as $axis) {
		$values[] = '('.$version.','.$axis.','.db_escape($axes[$axis]['label']).','
			.db_escape($axes[$axis]['purpose']).','.(int)$axes[$axis]['required'].',NOW(),'
			.$actor.','.db_escape($digest).')';
	}
	$stored = db_query('INSERT INTO '.TB_PREF.'dimension_category_policies'
		.' (version,axis,label,purpose,required,created_at,created_by,digest) VALUES '.implode(',', $values));
	foreach ($stored ? $rules : array() as $rule) {
		$stored = db_query('INSERT INTO '.TB_PREF.'dimension_category_policy_rules'
			.' (version,axis,kind,left_value,right_value,note) VALUES ('.$version.','.(int)$rule['axis'].','
			.db_escape($rule['kind']).','.db_escape($rule['left_value']).','.db_escape($rule['right_value']).','
			.db_escape($rule['note']).')');
		if (!$stored)
			break;
	}
	if (!$stored || !commit_transaction()) {
		cancel_transaction();
		$failed['errors'][] = _('The policy version could not be stored; reload the page and publish again.');
		return $failed;
	}
	dimension_category_policy_current(true);
	return array('published' => true, 'version' => $version, 'errors' => array());
}

/**
 * Impact of disabling one category, measured on the active company only.
 *
 * Every count is read from the actual source tables. A source that is not installed for this
 * company (an older schema without that feature) contributes nothing, because it cannot create a
 * future event; a source that is installed but cannot be read is reported `unknown` and blocks.
 *
 * @param int $axis Native category 1 or 2
 * @return array Counts, source states and human-readable conflicts
 */
function dimension_category_axis_impact($axis)
{
	$axis = (int)$axis;
	if ($axis !== 1 && $axis !== 2)
		return array();
	$column = $axis === 1 ? 'dimension_id' : 'dimension2_id';
	$name = dimension_category_policy_label($axis);
	$impact = array('axis' => $axis, 'label' => $name, 'masters_total' => 0, 'masters_open' => 0,
		'masters_closed' => 0, 'gl_rows' => 0, 'budget_rows' => 0, 'pending_approval_drafts' => 0,
		'sources' => array(), 'conflicts' => array(),
		'conflict_counts' => array('open_values' => 0, 'stored_defaults' => 0, 'pending_drafts' => 0,
			'hrm_bindings' => 0, 'policy' => 0, 'unknown' => 0));
	$masters = dimension_category_policy_count('dimensions', array('type_', 'closed'),
		'type_='.$axis.' AND closed=0');
	$total = dimension_category_policy_count('dimensions', array('type_'), 'type_='.$axis);
	$impact['sources']['dimensions'] = $masters['state'];
	$impact['masters_open'] = $masters['count'];
	$impact['masters_total'] = $total['count'];
	$impact['masters_closed'] = max(0, $total['count'] - $masters['count']);
	if ($masters['state'] === 'unknown') {
		$impact['conflict_counts']['unknown']++;
		$impact['conflicts'][] = sprintf(_('The stored %s values could not be verified.'), $name);
	} elseif ($masters['count'] > 0) {
		$impact['conflict_counts']['open_values'] = $masters['count'];
		$impact['conflicts'][] = sprintf(_('%1$d open %2$s value(s) must be closed before the category can be disabled; closing keeps their history readable.'),
			$masters['count'], $name);
	}
	$gl = dimension_category_policy_count('gl_trans', array($column), $column.'<>0');
	$impact['sources']['gl_trans'] = $gl['state'];
	$impact['gl_rows'] = $gl['count'];
	$budget = dimension_category_policy_count('budget_trans', array($column), $column.'<>0');
	$impact['sources']['budget_trans'] = $budget['state'];
	$impact['budget_rows'] = $budget['count'];
	$defaults = array();
	foreach (array('debtors_master', 'suppliers', 'stock_master') as $table) {
		$count = dimension_category_policy_count($table, array($column), $column.'<>0');
		$impact['sources'][$table] = $count['state'];
		if ($count['state'] === 'unknown') {
			$impact['conflict_counts']['unknown']++;
			$impact['conflicts'][] = sprintf(_('The stored default %s of %s could not be verified.'), $name, $table);
		} elseif ($count['count'] > 0) {
			$defaults[$table] = $count['count'];
			$impact['conflict_counts']['stored_defaults'] += $count['count'];
		}
	}
	if (count($defaults)) {
		$parts = array();
		foreach ($defaults as $table => $count)
			$parts[] = $table.' ('.$count.')';
		$impact['conflicts'][] = sprintf(_('Active source defaults still pre-fill %1$s: %2$s.'),
			$name, implode(', ', $parts));
	}
	$hrm = array();
	if ($axis === 1) {
		foreach (array(array('employees', 'cost_center_id'), array('departments', 'cost_center_id')) as $pair) {
			$count = dimension_category_policy_count($pair[0], array($pair[1]), $pair[1].'<>0');
			$impact['sources'][$pair[0].'.'.$pair[1]] = $count['state'];
			if ($count['state'] === 'unknown') {
				$impact['conflict_counts']['unknown']++;
				$impact['conflicts'][] = sprintf(_('The HRM %s binding could not be verified.'), $pair[0]);
			} elseif ($count['count'] > 0) {
				$hrm[] = $pair[0].' ('.$count['count'].')';
				$impact['conflict_counts']['hrm_bindings'] += $count['count'];
			}
		}
	} else {
		$count = dimension_category_policy_count('employees', array('dimension2_id'), 'dimension2_id<>0');
		$impact['sources']['employees.dimension2_id'] = $count['state'];
		if ($count['state'] === 'unknown') {
			$impact['conflict_counts']['unknown']++;
			$impact['conflicts'][] = _('The HRM second-category binding could not be verified.');
		} elseif ($count['count'] > 0) {
			$hrm[] = 'employees ('.$count['count'].')';
			$impact['conflict_counts']['hrm_bindings'] += $count['count'];
		}
	}
	if (count($hrm))
		$impact['conflicts'][] = sprintf(_('HRM still binds %1$s to %2$s: %3$s. HRM stays a separate authority; resolve the binding on its own pages first.'),
			$axis === 1 ? _('Cost Center') : $name, $name, implode(', ', $hrm));
	$drafts = 0;
	$draft_state = dimension_category_policy_table_state('session_draft_references');
	$impact['sources']['session_draft_references'] = $draft_state;
	if ($draft_state === 'available') {
		$slot = $axis === 1 ? "('dimension_id','cost_center_id')" : "('dimension2_id')";
		$count = dimension_category_policy_count('session_draft_references', array('dimension_id', 'slot'),
			"(slot IN ".$slot." OR dimension_id IN (SELECT id FROM ".TB_PREF."dimensions WHERE type_=".$axis.")"
			.' OR dimension_id NOT IN (SELECT id FROM '.TB_PREF.'dimensions))');
		if ($count['state'] === 'unknown') {
			$impact['conflict_counts']['unknown']++;
			$impact['conflicts'][] = sprintf(_('Pending session drafts for %s could not be verified.'), $name);
		} elseif ($count['count'] > 0) {
			$impact['conflict_counts']['pending_drafts'] = $count['count'];
			$impact['conflicts'][] = sprintf(_('%1$d pending draft classification(s) for %2$s must be completed or discarded first.'),
				$count['count'], $name);
		}
		$drafts = $count['count'];
	}
	// Only drafts still awaiting a decision (0 pending, 5 delegated) can execute later; approved,
	// rejected, cancelled and expired drafts are finished and must not inflate the count.
	$approval = dimension_category_policy_count('approval_drafts', array('draft_data', 'status'), 'status IN (0,5)');
	$impact['sources']['approval_drafts'] = $approval['state'];
	$impact['pending_approval_drafts'] = $approval['count'];
	$policy = dimension_category_policy_current();
	if ($policy['state'] === 'invalid') {
		$impact['conflict_counts']['policy']++;
		$impact['conflicts'][] = sprintf(_('The published category policy version %d does not verify, so no transition may rely on it.'), (int)$policy['version']);
	} elseif ($policy['state'] === 'published') {
		$required = !empty($policy['axes'][$axis]['required']);
		$rules = 0;
		foreach ($policy['rules'] as $rule)
			if ((int)$rule['axis'] === $axis)
				$rules++;
		if ($required) {
			$impact['conflict_counts']['policy']++;
			$impact['conflicts'][] = sprintf(_('The published policy still marks %s as required; publish a version that no longer requires it first.'), $name);
		} elseif ($rules > 0) {
			$impact['conflict_counts']['policy']++;
			$impact['conflicts'][] = sprintf(_('The published policy still declares %1$d rule(s) for %2$s; publish a version without them first.'),
				$rules, $name);
		}
	}
	$impact['pending_approval_drafts_note'] = _('Pending approval payloads are validated at execution (DIM-APR-001) and are listed for information only.');
	$impact['session_draft_rows'] = $drafts;
	return $impact;
}

/**
 * Preview one axis-count transition (0..2 -> 0..2) without changing anything.
 *
 * @param int $from Current enabled axis count
 * @param int $to Proposed enabled axis count
 * @return array from/to/direction, per disabled category impact and the blocking conflicts
 */
function dimension_category_transition_preview($from, $to)
{
	$from = max(0, min(2, (int)$from));
	$to = max(0, min(2, (int)$to));
	$preview = array('from' => $from, 'to' => $to,
		'direction' => $to > $from ? 'increase' : ($to < $from ? 'reduction' : 'unchanged'),
		'axes' => array(), 'conflicts' => array(), 'blocked' => false);
	for ($axis = $to + 1; $axis <= $from; $axis++)
		$preview['axes'][$axis] = dimension_category_axis_impact($axis);
	foreach ($preview['axes'] as $impact)
		foreach ($impact['conflicts'] as $conflict)
			$preview['conflicts'][] = $conflict;
	for ($axis = $from + 1; $axis <= $to; $axis++)
		$preview['axes'][$axis] = dimension_category_axis_impact($axis);
	$preview['blocked'] = count($preview['conflicts']) > 0;
	return $preview;
}

/**
 * Preview every ordered axis-count transition.
 *
 * The per-category impact is read once and reused, so the page performs one measurement per
 * category instead of one per transition.
 *
 * @return array from => to => preview
 */
function dimension_category_transition_matrix()
{
	$impacts = array(1 => dimension_category_axis_impact(1), 2 => dimension_category_axis_impact(2));
	$matrix = array();
	foreach (array(0, 1, 2) as $from) {
		foreach (array(0, 1, 2) as $to) {
			if ($from === $to)
				continue;
			$preview = array('from' => $from, 'to' => $to,
				'direction' => $to > $from ? 'increase' : 'reduction',
				'axes' => array(), 'conflicts' => array(), 'blocked' => false);
			for ($axis = $to + 1; $axis <= $from; $axis++) {
				$preview['axes'][$axis] = $impacts[$axis];
				foreach ($impacts[$axis]['conflicts'] as $conflict)
					$preview['conflicts'][] = $conflict;
			}
			for ($axis = $from + 1; $axis <= $to; $axis++)
				$preview['axes'][$axis] = $impacts[$axis];
			$preview['blocked'] = count($preview['conflicts']) > 0;
			$matrix[$from][$to] = $preview;
		}
	}
	return $matrix;
}

/**
 * Conflicts that refuse one transition, or an empty list when it may be applied.
 *
 * @param int $from Current enabled axis count
 * @param int $to Proposed enabled axis count
 * @return string[] Translated conflict descriptions
 */
function dimension_category_transition_refusal($from, $to)
{
	$preview = dimension_category_transition_preview($from, $to);
	return $preview['blocked'] ? $preview['conflicts'] : array();
}

/**
 * Conflicts that refused the latest `update_company_prefs()` call of this request.
 *
 * @return string[] Translated conflict descriptions, empty when nothing was refused
 */
function dimension_category_transition_last_error()
{
	return isset($GLOBALS['dimension_category_transition_error'])
		? $GLOBALS['dimension_category_transition_error'] : array();
}
