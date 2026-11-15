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


