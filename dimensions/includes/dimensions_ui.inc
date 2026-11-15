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
include_once($path_to_root . '/includes/ui.inc');

//--------------------------------------------------------------------------------------

/**
 * DIM-LFC-002: name the historical state of a Dimension in its read-only view.
 *
 * A closed master and a master whose identity was legitimately deleted both remain explainable from
 * their stored GL/budget IDs. This only renders state; it never reopens, edits or authorizes posting.
 *
 * @param array|false $row Master row, or false for a retained/deleted identity
 * @param int|string $id Exact native Dimension identity
 * @return void
 */
function display_dimension_history_state($row, $id) {
	if (!is_array($row))
		display_note(sprintf(_('Unknown Dimension #%d (deleted). Its historical GL and budget rows remain readable by the original ID.'), (int)$id));
	elseif ((int)$row['closed'] === 1)
		display_note(_('This dimension is closed. Its historical classifications remain readable; no new posting accepts it.'));
}

//--------------------------------------------------------------------------------------

function display_dimension_balance($id, $from, $to) {
	global $path_to_root;

	$result = get_dimension_balance_all($id, $from, $to);

	if (db_num_rows($result) == 0)
		display_note(_('There are no transactions for this dimension for the selected period.'));
	else {
		display_heading2(_('Balance for this Dimension'));
		br();
		start_table(TABLESTYLE);
		$th = array(_('Account'), _('Debit'), _('Credit'));
		table_header($th);

		$total = $k = 0;
		while ($myrow = db_fetch($result)) {
			alt_table_row_color($k);

			label_cell($myrow['account'].' '.$myrow['account_name']);
			display_debit_or_credit_cells($myrow['amt']);
			$total += $myrow['amt'];
			end_row();
		}
		start_row();
		label_cell('<b>'._('Balance').'</b>');
		if ($total >= 0) {
			amount_cell($total, true);
			label_cell('');
		}
		else {
			label_cell('');
			amount_cell(abs($total), true);
		}
		end_row();

		end_table();
	}
}

//--------------------------------------------------------------------------------------

/**
 * DIM-REP-003: percentage text, or an explicit N/A when no denominator exists.
 *
 * @param float|null $percent Percentage, or null when it cannot be computed
 * @return string
 */
function dimension_tracking_percent_text($percent) {
	return $percent === null ? _('N/A') : number_format2($percent, 2).'%';
}

/**
 * DIM-REP-003: name one source transaction family from its system type.
 *
 * @param int $type Stored `gl_trans.type`
 * @return string
 */
function dimension_tracking_family_label($type) {
	global $systypes_array;

	if (isset($systypes_array[$type]))
		return $systypes_array[$type];
	return sprintf(_('Source type #%d'), (int)$type);
}

/**
 * DIM-REP-003: render one period or lifetime summary with the truthful measure labels.
 *
 * Revenue, costs, result and margin come from the shared tracking reader; the net balance of balance-sheet
 * accounts is shown on its own row and never added to the result.
 *
 * @param array $summary Result of dimension_tracking_summary()
 * @param string $heading Section heading
 * @return void
 */
function display_dimension_tracking_summary($summary, $heading) {
	display_heading2($heading);
	start_table(TABLESTYLE);
	table_header(array(_('Measure'), _('Amount')));
	$rows = array(
		_('Revenue (credit-positive)') => $summary['revenue'],
		_('Cost of goods sold') => $summary['cogs'],
		_('Expenses') => $summary['expense'],
		_('Costs (COGS + expenses, debit-positive)') => $summary['costs'],
		_('Result (revenue - costs)') => $summary['result']);
	$k = 0;
	foreach ($rows as $label => $amount) {
		alt_table_row_color($k);
		label_cell($label);
		amount_cell($amount);
		end_row();
	}
	alt_table_row_color($k);
	label_cell(_('Margin'));
	if ($summary['margin_state'] === 'available')
		label_cell(dimension_tracking_percent_text($summary['margin']), "align='right'");
	else
		label_cell(dimension_tracking_margin_text($summary['margin_state']));
	end_row();
	alt_table_row_color($k);
	label_cell(_('Eligible P&L lines'));
	label_cell((string)$summary['eligible_lines'], "align='right'");
	end_row();
	alt_table_row_color($k);
	label_cell(_('Net GL balance of balance-sheet accounts (not a result)'));
	amount_cell($summary['balance_net']);
	end_row();
	end_table(1);
}

/**
 * DIM-REP-003: render the grouped result of one axis with Unassigned and unknown rows and the coverage line.
 *
 * @param int $axis 1 or 2
 * @param array $axis_data groups and coverage of one axis
 * @return void
 */
function display_dimension_tracking_axis($axis, $axis_data) {
	display_heading2(sprintf(_('Category %s - result by Dimension'), (int)$axis));
	if (!count($axis_data['groups'])) {
		display_note(_('No eligible P&L postings in this period.'));
		return;
	}
	start_table(TABLESTYLE);
	table_header(array(_('Dimension'), _('Revenue'), _('Costs (COGS + expenses)'), _('Result'), _('Lines')));
	$k = 0;
	foreach ($axis_data['groups'] as $row) {
		if ($row['state'] === 'unassigned')
			$name = _('Unassigned');
		elseif ($row['state'] === 'unknown')
			$name = sprintf(_('Unknown Dimension #%d'), $row['dimension_id']);
		else
			$name = $row['reference'].' '.$row['name'].($row['closed'] ? ' ('._('Closed').')' : '');
		alt_table_row_color($k);
		label_cell($name);
		amount_cell($row['revenue']);
		amount_cell($row['costs']);
		amount_cell($row['result']);
		label_cell((string)$row['eligible_lines'], "align='right'");
		end_row();
	}
	end_table(1);
	$coverage = $axis_data['coverage'];
	display_note(sprintf(_('Classification coverage: %s of eligible GL lines; %s of the absolute eligible amount. Unassigned lines: %d; unknown Dimension lines: %d.'),
		dimension_tracking_percent_text($coverage['line_percent']), dimension_tracking_percent_text($coverage['absolute_percent']),
		$coverage['unassigned_lines'], $coverage['unknown_lines']));
}

/**
 * DIM-REP-003: render the per-family classification coverage; an incomplete family is always shown.
 *
 * @param array $families Result of dimension_tracking_family_coverage()
 * @return void
 */
function display_dimension_tracking_families($families) {
	display_heading2(_('Coverage by source transaction family'));
	if ($families['state'] === 'empty') {
		display_note(_('No eligible P&L postings in this period, so no source family is listed.'));
		return;
	}
	$state_text = array('complete' => _('Complete'), 'partial' => _('Incomplete'), 'none' => _('Not classified'));
	start_table(TABLESTYLE);
	table_header(array(_('Source family'), _('Lines'), _('Category 1'), _('Category 2')));
	$k = 0;
	foreach ($families['families'] as $family) {
		alt_table_row_color($k);
		label_cell(dimension_tracking_family_label($family['type']));
		label_cell((string)$family['lines'], "align='right'");
		label_cell($state_text[$family['axis1_state']].' ('.$family['axis1_classified'].'/'.$family['lines'].')');
		label_cell($state_text[$family['axis2_state']].' ('.$family['axis2_classified'].'/'.$family['lines'].')');
		end_row();
	}
	end_table(1);
	if ($families['state'] === 'incomplete')
		display_note(_('Coverage is incomplete: at least one source family has postings without a classification on a category. Measures above describe only the classified part where a Dimension is selected.'));
	else
		display_note(_('Every listed source family is fully classified. Families with no eligible posting in the period are not listed.'));
}

/**
 * DIM-REP-003: render the comparable budget of the scope or the reason it cannot be compared.
 *
 * @param array $budget Result of dimension_tracking_budget_summary()
 * @return void
 */
function display_dimension_tracking_budget($budget) {
	display_heading2(_('Budget comparison'));
	if ($budget['state'] === 'comparable' || $budget['state'] === 'incomplete') {
		start_table(TABLESTYLE);
		table_header(array(_('Measure'), _('Budget')));
		$k = 0;
		foreach (array(_('Revenue budget') => $budget['revenue_budget'], _('Cost budget') => $budget['cost_budget'], _('Result budget') => $budget['result_budget']) as $label => $amount) {
			alt_table_row_color($k);
			label_cell($label);
			amount_cell($amount);
			end_row();
		}
		end_table(1);
		if ($budget['state'] === 'incomplete')
			display_note(sprintf(_('%d of %d accounts have no complete monthly budget and are left out of these totals.'), $budget['accounts_without_budget'], $budget['accounts']));
		return;
	}
	$text = dimension_budget_reason_text($budget['reason']);
	display_note($text !== '' ? $text : _('The budget cannot be compared for this scope and period.'));
}

/**
 * DIM-REP-003: render the source drilldown by account; the GL inquiry link needs GL transaction authority.
 *
 * @param array $drill Result of dimension_tracking_account_drilldown()
 * @param string $from Period start in user date format
 * @param string $to Period end in user date format
 * @param int $scope1 Axis 1 scope
 * @param int $scope2 Axis 2 scope
 * @return void
 */
function display_dimension_tracking_drilldown($drill, $from, $to, $scope1, $scope2) {
	global $path_to_root;

	display_heading2(_('Source drilldown by account'));
	if (!count($drill['rows'])) {
		display_note(_('No eligible P&L postings in this period.'));
		return;
	}
	$link = $_SESSION['wa_current_user']->can_access('SA_GLTRANSVIEW');
	start_table(TABLESTYLE);
	table_header(array(_('Account'), _('Revenue'), _('Costs'), _('Result'), _('Lines')));
	$k = 0;
	foreach ($drill['rows'] as $row) {
		$label = htmlspecialchars($row['account'].' '.$row['account_name'], ENT_QUOTES, 'UTF-8');
		if ($link)
			$label = "<a href='".$path_to_root.'/gl/inquiry/gl_account_inquiry.php?TransFromDate='.urlencode($from).'&TransToDate='.urlencode($to)
				.'&Dimension='.(int)$scope1.'&Dimension2='.(int)$scope2.'&account='.urlencode($row['account'])."'>".$label.'</a>';
		alt_table_row_color($k);
		label_cell($label);
		amount_cell($row['revenue']);
		amount_cell($row['costs']);
		amount_cell($row['result']);
		label_cell((string)$row['eligible_lines'], "align='right'");
		end_row();
	}
	end_table(1);
	if ($drill['truncated'])
		display_note(sprintf(_('Only the first %d accounts are listed; narrow the scope to see the rest.'), dimension_tracking_row_limit()));
}

/**
 * DIM-REP-003: render the full workspace read for one scope.
 *
 * @param array $data Result of dimension_tracking_workspace()
 * @param array|false $drill Optional source drilldown
 * @param string $from Period start in user date format
 * @param string $to Period end in user date format
 * @return void
 */
function display_dimension_tracking_workspace($data, $drill, $from, $to) {
	display_note(sprintf(_('Posted GL measures in company base currency from %s to %s; lifetime measures run to %s. Revenue is credit-positive, costs are debit-positive and result = revenue - costs. These are GL classification measures, not customer attribution or fully allocated profit.'), $from, $to, $to));
	display_dimension_tracking_summary($data['period'], sprintf(_('Period %s - %s'), $from, $to));
	display_dimension_tracking_summary($data['lifetime'], sprintf(_('Lifetime to %s'), $to));
	display_dimension_tracking_axis(1, $data['axis1']);
	display_dimension_tracking_axis(2, $data['axis2']);
	display_dimension_tracking_families($data['families']);
	display_dimension_tracking_budget($data['budget']);
	if (is_array($drill))
		display_dimension_tracking_drilldown($drill, $from, $to, $data['period']['scope1'], $data['period']['scope2']);
}



//--------------------------------------------------------------------------------------

/**
 * DIM-REP-004: status text of one master in the search results.
 *
 * DIM-UX-001: the optional list of deadline categories keeps an ongoing category (a cost center) from being
 * called overdue; without it every category keeps the legacy behavior.
 *
 * @param array $master Master row (closed, due_date in SQL form, type_ when a deadline list is given)
 * @param string $today SQL date, or '' when unknown
 * @param int[]|null $deadline_axes Categories whose required-by date is a real deadline, or null for all
 * @return string Closed, Overdue or Open
 */
function dimension_inquiry_status_text($master, $today, $deadline_axes = null) {
	if ((int)$master['closed'] === 1)
		return _('Closed');
	if ($today !== '' && $master['due_date'] < $today
		&& ($deadline_axes === null || in_array((int)$master['type_'], array_map('intval', (array)$deadline_axes), true)))
		return _('Overdue');
	return _('Open');
}

/**
 * DIM-REP-004: render the result page of a Dimension master search.
 *
 * Master columns always show; the grouped revenue/cost/result columns show only when the caller is allowed the tracking
 * aggregate and are the own-axis P&L measures of the shared reader, with the net balance of balance-sheet accounts kept
 * apart. The note states whether the figures are for the chosen activity period or for the stored lifetime.
 *
 * @param array $search Result of dimension_inquiry_search()
 * @param bool $show_measures Whether the performance columns are shown
 * @param string $today SQL date used for the Overdue status
 * @param array|null $options DIM-UX-001 business-mode presentation: deadline_axes (categories that can be overdue), measures
 *                            (revenue/costs/result subset), can_edit (show the Edit action), type_text (axis => HTML-safe text);
 *                            null keeps the legacy presentation
 * @return void
 */
function display_dimension_inquiry_results($search, $show_measures, $today, $options = null) {
	if (!count($search['rows'])) {
		display_note(_('No dimension matches the selected filters.'));
		return;
	}
	$first = ($search['page'] - 1) * $search['page_size'] + 1;
	display_note(sprintf(_('Showing %d-%d of %d dimensions (page %d of %d).'), $first, $first + count($search['rows']) - 1, $search['total'], $search['page'], $search['pages']));
	$options = is_array($options) ? $options : array();
	$deadline_axes = isset($options['deadline_axes']) ? $options['deadline_axes'] : null;
	$measures = isset($options['measures']) ? $options['measures'] : array('revenue', 'costs', 'result');
	$can_edit = !isset($options['can_edit']) || $options['can_edit'];
	$type_text = isset($options['type_text']) ? $options['type_text'] : array();
	$measure_headers = array('revenue' => _('Revenue'), 'costs' => _('Costs'), 'result' => _('Result'));
	$headers = array(_('#'), _('Reference'), _('Name'), _('Type'), _('Start date'), isset($options['deadline_caption']) ? $options['deadline_caption'] : _('Due Date'), _('Status'));
	if ($show_measures) {
		foreach ($measures as $measure)
			$headers[] = $measure_headers[$measure];
		$headers[] = _('Net GL balance (not a result)');
	}
	if ($can_edit)
		$headers[] = '';
	$headers[] = '';
	start_table(TABLESTYLE);
	table_header($headers);
	$k = 0;
	foreach ($search['rows'] as $master) {
		alt_table_row_color($k);
		label_cell(get_dimensions_trans_view_str(ST_DIMENSION, $master['id']));
		label_cell($master['reference']);
		label_cell($master['name']);
		label_cell(isset($type_text[(int)$master['type_']]) ? $type_text[(int)$master['type_']] : (string)$master['type_']);
		label_cell(sql2date($master['date_']));
		label_cell(sql2date($master['due_date']));
		label_cell(dimension_inquiry_status_text($master, $today, $deadline_axes));
		if ($show_measures) {
			$m = $master['measures'];
			foreach ($measures as $measure)
				amount_cell($m[$measure]);
			amount_cell($m['balance_net']);
		}
		if ($can_edit)
			label_cell(pager_link(_('Edit'), '/dimensions/dimension_entry.php?trans_no='.(int)$master['id'], ICON_EDIT));
		label_cell(print_document_link($master['id'], _('Print'), ST_DIMENSION, ICON_PRINT));
		end_row();
	}
	end_table(1);
	if ($show_measures) {
		if ($search['basis'] === 'period')
			display_note(_('Revenue, costs and result are the posted GL measures of each dimension on its own category for the selected activity period (revenue credit-positive, costs debit-positive). The net balance of balance-sheet accounts is separate and is not a result.'));
		else
			display_note(_('No activity period is selected, so revenue, costs and result are the stored lifetime measures of each dimension on its own category (revenue credit-positive, costs debit-positive). The net balance of balance-sheet accounts is separate and is not a result.'));
	}
}

//--------------------------------------------------------------------------------------

/**
 * DIM-REP-005: name one customer row, keeping an unknown customer identity visible.
 *
 * @param array $customer Row of dimension_customer_economics()
 * @return string Plain text
 */
function dimension_customer_label($customer) {
	if (empty($customer['known']))
		return sprintf(_('Unknown customer #%d'), (int)$customer['customer_no']);
	return $customer['reference'].' '.$customer['name'];
}

/**
 * DIM-REP-005: render the customer economics of one scope.
 *
 * Revenue and matched cost come from the customer source documents; a customer without supported matched cost shows no
 * contribution or margin; costs no customer document supports are listed as unattributed and are never allocated or
 * estimated; fully allocated profit is shown as unavailable.
 *
 * @param array $data Result of dimension_customer_economics()
 * @param string $heading Section heading
 * @return void
 */
function display_dimension_customer_economics($data, $heading) {
	display_heading2($heading);
	if (!count($data['customers']))
		display_note(_('No P&L posting of a customer document in this scope.'));
	else {
		$link = $_SESSION['wa_current_user']->can_access('SA_SALESTRANSVIEW');
		start_table(TABLESTYLE);
		table_header(array(_('Customer'), _('Revenue'), _('Matched cost'), _('Direct contribution'), _('Margin'), _('Share of attributed revenue'), _('Status')));
		$k = 0;
		foreach ($data['customers'] as $customer) {
			alt_table_row_color($k);
			$label = htmlspecialchars(dimension_customer_label($customer), ENT_QUOTES, 'UTF-8');
			if ($link && !empty($customer['known']))
				$label = "<a href='".$GLOBALS['path_to_root'].'/sales/inquiry/customer_inquiry.php?customer_id='.(int)$customer['customer_no']."'>".$label.'</a>';
			label_cell($label);
			amount_cell($customer['revenue']);
			amount_cell($customer['matched_cost']);
			if ($customer['contribution'] === null)
				label_cell(_('N/A'), "align='right'");
			else
				amount_cell($customer['contribution']);
			label_cell(dimension_tracking_percent_text($customer['margin']), "align='right'");
			label_cell(dimension_tracking_percent_text($customer['revenue_share']), "align='right'");
			label_cell(dimension_customer_contribution_text($customer['contribution_state']));
			end_row();
		}
		end_table(1);
	}
	$mix = $data['mix'];
	if ($mix['state'] === 'single')
		display_note(_('Single-customer scope: all attributed revenue belongs to one customer.'));
	elseif ($mix['state'] === 'multi')
		display_note(sprintf(_('Multi-customer scope: %d customers share this scope. Costs that no customer document supports are not split among them.'), $mix['customers']));
	display_heading2(_('Not attributed to a customer'));
	start_table(TABLESTYLE);
	table_header(array(_('Reason'), _('Revenue'), _('Costs'), _('Lines'), _('Next step')));
	$k = 0;
	$listed = 0;
	foreach ($data['unattributed'] as $state => $bucket) {
		if ($bucket['lines'] === 0)
			continue;
		$listed++;
		alt_table_row_color($k);
		label_cell(dimension_customer_state_text($state));
		amount_cell($bucket['revenue']);
		amount_cell($bucket['costs']);
		label_cell((string)$bucket['lines'], "align='right'");
		label_cell(dimension_customer_state_action($state));
		end_row();
	}
	end_table(1);
	if (!$listed)
		display_note(_('Every eligible P&L posting of this scope is attributed to a customer through its source document.'));
	$coverage = $data['coverage'];
	display_note(sprintf(_('Attribution coverage: revenue lines %s (%d of %d), cost lines %s (%d of %d), absolute eligible amount %s.'),
		dimension_tracking_percent_text($coverage['revenue_line_percent']), $coverage['revenue_attributed_lines'], $coverage['revenue_lines'],
		dimension_tracking_percent_text($coverage['cost_line_percent']), $coverage['cost_attributed_lines'], $coverage['cost_lines'],
		dimension_tracking_percent_text($coverage['absolute_percent'])));
	$missing = $data['missing'];
	if ($missing['customers_without_matched_cost'] > 0 || $missing['unattributed_cost_lines'] > 0)
		display_note(sprintf(_('Missing cost, not estimated: %d customer(s) have revenue with no matched cost; %d cost line(s) (COGS %s, expense %s) have no customer source.'),
			$missing['customers_without_matched_cost'], $missing['unattributed_cost_lines'], number_format2($missing['unattributed_cogs'], 2), number_format2($missing['unattributed_expense'], 2)));
	if ($data['fully_allocated']['state'] !== 'available')
		display_note(_('Fully allocated profit is not available: allocation coverage has not been accepted, so unattributed costs are neither allocated nor estimated. Direct contribution counts only cost supported by a customer document.'));
	display_note($data['reconciled']
		? _('Reconciles to the Financial Tracking revenue, costs and result of the same scope, period and cutoff.')
		: _('WARNING: this split does not reconcile to the Financial Tracking measures of the same scope; do not rely on it.'));
}

/**
 * DIM-REP-005: render the attribution queue of unattributed source documents.
 *
 * @param array $queue Result of dimension_customer_attribution_queue()
 * @return void
 */
function display_dimension_customer_queue($queue) {
	display_heading2(_('Attribution queue'));
	if (!count($queue['rows'])) {
		display_note(_('No source document is waiting for customer attribution.'));
		return;
	}
	start_table(TABLESTYLE);
	table_header(array(_('Source'), _('Document #'), _('Reason'), _('First posting'), _('Age (days)'), _('Revenue'), _('Costs'), _('Lines')));
	$k = 0;
	foreach ($queue['rows'] as $row) {
		alt_table_row_color($k);
		label_cell(dimension_tracking_family_label($row['type']));
		label_cell((string)$row['type_no'], "align='right'");
		label_cell(dimension_customer_state_text($row['state']));
		label_cell(sql2date($row['first_date']));
		label_cell((string)$row['age_days'], "align='right'");
		amount_cell($row['revenue']);
		amount_cell($row['costs']);
		label_cell((string)$row['lines'], "align='right'");
		end_row();
	}
	end_table(1);
	if ($queue['truncated'])
		display_note(sprintf(_('Only the first %d documents are listed; narrow the scope or period to see the rest.'), dimension_tracking_row_limit()));
}

//--------------------------------------------------------------------------------------

/**
 * DIM-REP-006: render one section of the shared export dataset.
 *
 * The page, the PDF and the Excel report all render the same section cells: a float is an amount, anything else is text. This is
 * the page side of that one dataset.
 *
 * @param array $section Section of dimension_export_dataset()
 * @return void
 */
function display_dimension_export_section($section) {
	display_heading2($section['title']);
	if (count($section['rows'])) {
		start_table(TABLESTYLE);
		table_header($section['headers']);
		$k = 0;
		foreach ($section['rows'] as $row) {
			alt_table_row_color($k);
			foreach ($row as $column => $cell) {
				if (is_float($cell))
					amount_cell($cell);
				elseif (isset($section['aligns'][$column]) && $section['aligns'][$column] === 'right')
					label_cell((string)$cell, "align='right'");
				else
					label_cell((string)$cell);
			}
			end_row();
		}
		end_table(1);
	}
	foreach ($section['notes'] as $note)
		display_note($note);
}

/**
 * DIM-REP-006: render the whole shared export dataset.
 *
 * @param array $dataset Result of dimension_export_dataset()
 * @return void
 */
function display_dimension_export_dataset($dataset) {
	foreach ($dataset['sections'] as $section)
		display_dimension_export_section($section);
}

//--------------------------------------------------------------------------------------

/**
 * DIM-UX-001: show one plain failure or empty state (title, what it means, what to do next).
 *
 * @param string $code State code of dimension_business_mode_failure_states()
 * @return void
 */
function display_dimension_failure_state($code) {
	$state = dimension_business_mode_failure_state($code);
	if ($state === false)
		return;
	display_note(htmlspecialchars($state['title'], ENT_QUOTES, 'UTF-8').': '.htmlspecialchars($state['explanation'], ENT_QUOTES, 'UTF-8').' '
		.htmlspecialchars($state['next_step'], ENT_QUOTES, 'UTF-8'));
}

/**
 * DIM-UX-001: render the three onboarding steps.
 *
 * An action link is drawn only for a step that carries one, which the step builder gives only to a caller with the authority
 * of the target page; every other step explains why in words.
 *
 * @param array $steps Result of dimension_onboarding_steps()
 * @return void
 */
function display_dimension_onboarding_steps($steps) {
	global $path_to_root;

	$status_text = array('done' => _('Done'), 'todo' => _('To do'), 'blocked' => _('Cannot continue'));
	start_table(TABLESTYLE);
	table_header(array(_('Step'), _('What to do'), _('Where you are'), _('Next')));
	$k = 0;
	foreach ($steps as $step) {
		alt_table_row_color($k);
		label_cell($step['number']);
		label_cell('<b>'.htmlspecialchars($step['title'], ENT_QUOTES, 'UTF-8').'</b><br>'.htmlspecialchars($step['text'], ENT_QUOTES, 'UTF-8'));
		$where = $status_text[$step['status']];
		foreach (array($step['state'], $step['extra_state']) as $code) {
			$state = $code === '' ? false : dimension_business_mode_failure_state($code);
			if ($state !== false)
				$where .= '<br>'.htmlspecialchars($state['title'], ENT_QUOTES, 'UTF-8').': '.htmlspecialchars($state['explanation'], ENT_QUOTES, 'UTF-8');
		}
		label_cell($where);
		if ($step['link'] !== '')
			label_cell("<a href='".$path_to_root.'/'.$step['link']."'>".htmlspecialchars($step['link_label'], ENT_QUOTES, 'UTF-8').'</a>');
		else {
			$next = '';
			foreach (array($step['extra_state'], $step['state']) as $code) {
				$state = $code === '' ? false : dimension_business_mode_failure_state($code);
				if ($state !== false) {
					$next = htmlspecialchars($state['next_step'], ENT_QUOTES, 'UTF-8');
					break;
				}
			}
			label_cell($next);
		}
		end_row();
	}
	end_table(1);
}

/**
 * DIM-UX-001: disabled-history view, one block per category that is switched off but still holds history.
 *
 * @param array $history Result of dimension_onboarding_history_only()
 * @return void
 */
function display_dimension_history_only($history) {
	global $path_to_root;

	if (!count($history))
		return;
	display_heading2(_('Switched off, history kept'));
	foreach ($history as $entry) {
		$text = sprintf(_('Category %d (%s) is switched off for new entries. Its %d values and everything recorded with them stay available for viewing and reports.'),
			$entry['axis'], htmlspecialchars($entry['label'], ENT_QUOTES, 'UTF-8'), $entry['masters']);
		display_note($text);
		hyperlink_no_params($path_to_root.'/dimensions/inquiry/search_dimensions.php?type_='.(int)$entry['axis'], sprintf(_('View the history of category %d'), $entry['axis']));
	}
}

/**
 * DIM-UX-001: render one synthetic demo with its totals, every reconciliation check and a plain explanation of each line.
 *
 * @param array $dataset Result of dimension_demo_dataset()
 * @return void
 */
function display_dimension_demo($dataset) {
	display_heading2($dataset['title']);
	display_note(_('This is an example with made-up numbers. It is not your company data and nothing is saved.'));
	$axes = array();
	foreach (array(1, 2) as $axis)
		if (count($dataset['axes'][$axis]['values']))
			$axes[] = $axis;
	$headers = array(_('Source'), _('Document'), _('Account'), _('Income'), _('Cost'));
	foreach ($axes as $axis)
		$headers[] = htmlspecialchars($dataset['axes'][$axis]['label'], ENT_QUOTES, 'UTF-8');
	start_table(TABLESTYLE);
	table_header($headers);
	$k = 0;
	foreach ($dataset['lines'] as $line) {
		alt_table_row_color($k);
		label_cell(htmlspecialchars($line['source'], ENT_QUOTES, 'UTF-8'));
		label_cell(htmlspecialchars($line['document'], ENT_QUOTES, 'UTF-8'));
		label_cell(htmlspecialchars($line['account'], ENT_QUOTES, 'UTF-8'));
		amount_cell($line['kind'] === 'revenue' ? $line['cents'] / 100 : 0);
		amount_cell($line['kind'] === 'cost' ? $line['cents'] / 100 : 0);
		foreach ($axes as $axis) {
			$value = (int)$line[$axis === 1 ? 'd1' : 'd2'];
			label_cell($value > 0 ? htmlspecialchars($dataset['axes'][$axis]['values'][$value], ENT_QUOTES, 'UTF-8') : _('Not assigned'));
		}
		end_row();
	}
	end_table(1);
	$view = dimension_demo_totals($dataset);
	foreach ($axes as $axis) {
		display_heading2(sprintf(_('By %s'), htmlspecialchars($dataset['axes'][$axis]['label'], ENT_QUOTES, 'UTF-8')));
		start_table(TABLESTYLE);
		table_header(array(_('Name'), _('Income'), _('Cost'), _('Result')));
		$k = 0;
		$keys = array_keys($dataset['axes'][$axis]['values']);
		$keys[] = 0;
		foreach ($keys as $key) {
			if (!isset($view['by_axis'][$axis][$key]))
				continue;
			alt_table_row_color($k);
			label_cell($key > 0 ? htmlspecialchars($dataset['axes'][$axis]['values'][$key], ENT_QUOTES, 'UTF-8') : _('Not assigned'));
			$row = $view['by_axis'][$axis][$key];
			amount_cell($row['revenue'] / 100);
			amount_cell($row['costs'] / 100);
			amount_cell($row['result'] / 100);
			end_row();
		}
		alt_table_row_color($k);
		label_cell('<b>'._('Total').'</b>');
		amount_cell($view['totals']['revenue'] / 100, true);
		amount_cell($view['totals']['costs'] / 100, true);
		amount_cell($view['totals']['result'] / 100, true);
		end_row();
		end_table(1);
	}
	if (count($axes) === 2) {
		display_heading2(sprintf(_('Result by %s and %s'), htmlspecialchars($dataset['axes'][1]['label'], ENT_QUOTES, 'UTF-8'), htmlspecialchars($dataset['axes'][2]['label'], ENT_QUOTES, 'UTF-8')));
		start_table(TABLESTYLE);
		$header = array('');
		foreach (array_keys($dataset['axes'][2]['values']) as $key)
			$header[] = htmlspecialchars($dataset['axes'][2]['values'][$key], ENT_QUOTES, 'UTF-8');
		$header[] = _('Not assigned');
		table_header($header);
		$k = 0;
		$rows = array_keys($dataset['axes'][1]['values']);
		$rows[] = 0;
		foreach ($rows as $key1) {
			alt_table_row_color($k);
			label_cell($key1 > 0 ? htmlspecialchars($dataset['axes'][1]['values'][$key1], ENT_QUOTES, 'UTF-8') : _('Not assigned'));
			$cols = array_keys($dataset['axes'][2]['values']);
			$cols[] = 0;
			foreach ($cols as $key2)
				amount_cell((isset($view['matrix'][$key1][$key2]) ? $view['matrix'][$key1][$key2] : 0) / 100);
			end_row();
		}
		end_table(1);
	}
	$reconciliation = dimension_demo_reconcile($dataset);
	display_heading2(_('Does it add up?'));
	start_table(TABLESTYLE);
	table_header(array(_('Check'), _('Expected'), _('Shown'), _('Result')));
	$k = 0;
	foreach ($reconciliation['checks'] as $check) {
		alt_table_row_color($k);
		label_cell(htmlspecialchars($check['label'], ENT_QUOTES, 'UTF-8'));
		amount_cell($check['expected'] / 100);
		amount_cell($check['actual'] / 100);
		label_cell($check['ok'] ? _('Adds up') : _('Different'));
		end_row();
	}
	end_table(1);
	display_heading2(_('Where each line came from'));
	foreach ($dataset['lines'] as $line)
		display_note(htmlspecialchars(dimension_demo_explain_line($dataset, $line), ENT_QUOTES, 'UTF-8'));
}
