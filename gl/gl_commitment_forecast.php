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
 * DIM-ADV-001: purchase commitments, funds reservations and forecast.
 *
 * Part 1 shows the funds position of an account and Dimension scope: budget, GL actual, the commitment stages, open
 * reservations and the free funds. Part 2 observes the purchase orders into the commitment register (advisory evidence; a
 * cancelled order releases its remaining commitment once). Part 3 shows the reservation mode (separately activated) and the
 * claims. Part 4 lists, proposes and decides forecast versions and shows the forecast of one scope. The page owns no logic:
 * everything is decided by `dimensions/includes/dimension_commitment.inc` and `dimension_forecast.inc`.
 */
$page_security = 'SA_BUDGETENTRY';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/admin/db/fiscalyears_db.inc');
include_once($path_to_root.'/dimensions/includes/dimension_commitment.inc');

page(_($help_context = 'Commitments, Reservations and Forecast'));

//-----------------------------------------------------------------------------------------------

/** Parse the forecast textarea: one `account dimension1 dimension2 estimate` line per change (estimate `remove` drops the line). */
function dimension_commit_page_changes($text)
{
	$changes = array();
	foreach (preg_split('/\r\n|\n|\r/', (string)$text) as $line) {
		$line = trim($line);
		if ($line === '')
			continue;
		$parts = preg_split('/[\s,;]+/', $line);
		$estimate = isset($parts[3]) ? $parts[3] : '';
		$changes[] = array('account' => $parts[0], 'coordinate_1' => isset($parts[1]) ? $parts[1] : '', 'coordinate_2' => isset($parts[2]) ? $parts[2] : '',
			'etc_amount' => strtolower($estimate) === 'remove' ? null : $estimate);
	}
	return $changes;
}

/** Show the errors of a refused call or its success note. */
function dimension_commit_page_show_result($result, $success_text)
{
	if (!$result['ok']) {
		foreach ($result['errors'] as $error)
			display_error($error);
		return;
	}
	display_notification($success_text);
}

//-----------------------------------------------------------------------------------------------

if (dimension_commit_schema_ready() !== true) {
	display_error(_('The commitment tables are not installed. Upgrade the company database first.'));
	end_page();
	exit;
}

$fiscal_year = (int)get_post('fyear', 0);
if (!$fiscal_year && function_exists('get_current_fiscalyear')) {
	$current_year = get_current_fiscalyear();
	$fiscal_year = $current_year ? (int)$current_year['id'] : 0;
	$_POST['fyear'] = $fiscal_year;
}
if (!isset($_POST['TransFromDate']))
	$_POST['TransFromDate'] = begin_fiscalyear();
if (!isset($_POST['TransToDate']))
	$_POST['TransToDate'] = Today();

if (isset($_POST['Observe']) && check_csrf_token()) {
	$observed = dimension_commit_observe(0);
	dimension_commit_page_show_result($observed, sprintf(_('Observed: %d registered, %d updated, %d unchanged, %d released.'),
		$observed['registered'], $observed['updated'], $observed['unchanged'], $observed['released']));
	foreach ($observed['unresolved'] as $entry)
		display_warning(sprintf(_('Order line %d is not valued: %s'), $entry['po_detail_item'], $entry['text']));
}
if (isset($_POST['SetMode']) && check_csrf_token())
	dimension_commit_page_show_result(dimension_commit_set_mode(get_post('mode', ''), get_post('mode_reason', '')), _('The reservation mode was recorded.'));
$release_id = find_submit('ReleaseReservation');
if ($release_id != -1 && check_csrf_token())
	dimension_commit_page_show_result(dimension_commit_release($release_id), _('The open amount of the reservation was released.'));
if ($fiscal_year) {
	if (isset($_POST['ProposeForecast']) && check_csrf_token()) {
		$latest_forecast = dimension_forecast_latest_approved($fiscal_year);
		dimension_commit_page_show_result(dimension_forecast_propose($fiscal_year, $latest_forecast ? (int)$latest_forecast['version_id'] : 0,
			dimension_commit_page_changes(get_post('forecast_changes', '')), get_post('forecast_note', ''), date2sql(get_post('forecast_as_of', Today()))),
			_('The forecast was proposed and waits for an independent checker.'));
	}
	$approve_id = find_submit('ApproveForecast');
	if ($approve_id != -1 && check_csrf_token())
		dimension_commit_page_show_result(dimension_forecast_decide($approve_id, 'approve', get_post('decision_note', '')), _('The forecast is approved.'));
	$reject_id = find_submit('RejectForecast');
	if ($reject_id != -1 && check_csrf_token())
		dimension_commit_page_show_result(dimension_forecast_decide($reject_id, 'reject', get_post('decision_note', '')), _('The forecast is rejected.'));
}

//-----------------------------------------------------------------------------------------------

start_form();

display_heading(_('Part 1 - funds position'));
display_note(_('Commitments and reservations are advisory evidence. Only the actual column is posted to the general ledger.'), 0, 1);
start_table(TABLESTYLE_NOBORDER);
start_row();
text_cells(_('Account:'), 'fp_account', get_post('fp_account', ''), 15, 15);
text_cells(_('Dimension 1 (0 = all, -1 = unassigned):'), 'fp_dim1', get_post('fp_dim1', '0'), 8, 10);
text_cells(_('Dimension 2:'), 'fp_dim2', get_post('fp_dim2', '0'), 8, 10);
date_cells(_('From:'), 'TransFromDate');
date_cells(_('To:'), 'TransToDate');
submit_cells('ShowPosition', _('Show'), '', false, false);
end_row();
end_table(1);
if (get_post('fp_account', '') !== '' && is_date($_POST['TransFromDate']) && is_date($_POST['TransToDate'])) {
	$position = dimension_commit_funds_position(get_post('fp_account'), (int)get_post('fp_dim1', 0), (int)get_post('fp_dim2', 0), date2sql($_POST['TransFromDate']), date2sql($_POST['TransToDate']));
	if ($position === false)
		display_error(_('The funds position could not be read for this scope.'));
	else {
		start_table(TABLESTYLE);
		$rows = array(_('Budget') => $position['budget'], _('GL actual') => $position['actual'], _('Commitment: not yet received') => $position['commitment_open'],
			_('Commitment: received, not billed') => $position['commitment_received'], _('Billed against orders') => $position['commitment_billed'],
			_('Remaining commitment') => $position['commitment_remaining'], _('Open reservations') => $position['reservations_open'],
			_('Consumed (actual + commitment + reservations)') => $position['consumed'], _('Free funds') => $position['free']);
		foreach ($rows as $label => $value) {
			start_row();
			label_cell($label);
			label_cell($value === null ? _('no comparable budget') : number_format2($value, 2), "align='right'");
			end_row();
		}
		end_table(1);
		if (!$position['complete'])
			display_warning(sprintf(_('%d register row(s) fail their digest and are excluded; the position is not complete.'), $position['tampered_lines']));
		if ($position['unclassified_lines'])
			display_note(sprintf(_('%d counted line(s) have no durable Dimension classification and read as unassigned.'), $position['unclassified_lines']), 0, 1);
	}
}

display_heading(_('Part 2 - observe purchase commitments'));
display_note(_('Observing registers new purchase order lines, records changed receipt and bill quantities and releases the remaining commitment of a cancelled order exactly once. It changes no order.'), 0, 1);
submit_center('Observe', _('Observe purchase orders'), true, _('Update the advisory commitment register'), false);
$register = dimension_commit_register(0, 200);
if ($register !== false && count($register)) {
	start_table(TABLESTYLE);
	table_header(array(_('Order'), _('Line'), _('Account'), _('Dimension 1'), _('Dimension 2'), _('Lineage'), _('State'), _('Not received'), _('Received, not billed'), _('Billed'), _('Released')));
	foreach ($register as $row) {
		start_row();
		label_cell((int)$row['order_no']);
		label_cell((int)$row['po_detail_item']);
		label_cell($row['account_code']);
		label_cell((int)$row['coordinate_1']);
		label_cell((int)$row['coordinate_2']);
		label_cell($row['lineage_state']);
		label_cell($row['intact'] ? $row['state'] : _('digest mismatch'));
		amount_cell($row['stages']['open']);
		amount_cell($row['stages']['received']);
		amount_cell($row['stages']['billed']);
		amount_cell($row['stages']['released']);
		end_row();
	}
	end_table(1);
}

display_heading(_('Part 3 - reservations'));
$mode = dimension_commit_mode();
display_note($mode === 'reservations' ? _('Reservations are activated: claims on free funds are enforced atomically for callers that use them.')
	: _('Reservations are not activated. Commitments stay advisory.'), 0, 1);
start_table(TABLESTYLE2);
array_selector_row(_('Mode:'), 'mode', $mode === 'reservations' ? 'reservations' : 'advisory', array('advisory' => _('Advisory only'), 'reservations' => _('Reservations activated')));
text_row_ex(_('Reason (required):'), 'mode_reason', 50, 200, '', get_post('mode_reason', ''));
end_table(1);
submit_center('SetMode', _('Record mode'), true, _('Requires the company setup authority'), false);
$claims = dimension_commit_reservation_list(100);
if ($claims !== false && count($claims)) {
	start_table(TABLESTYLE);
	table_header(array(_('Reference'), _('Account'), _('Dimension 1'), _('Dimension 2'), _('Period'), _('Amount'), _('Consumed'), _('Released'), _('State'), ''));
	foreach ($claims as $claim) {
		start_row();
		label_cell(htmlspecialchars($claim['reference']));
		label_cell($claim['account_code']);
		label_cell((int)$claim['coordinate_1']);
		label_cell((int)$claim['coordinate_2']);
		label_cell(sql2date($claim['period_from']).' - '.sql2date($claim['period_to']));
		amount_cell($claim['amount']);
		amount_cell($claim['consumed_amount']);
		amount_cell($claim['released_amount']);
		label_cell($claim['state']);
		echo '<td>';
		if ($claim['state'] === 'reserved')
			submit('ReleaseReservation'.$claim['reservation_id'], _('Release'), true, _('Release the open amount'), false);
		echo '</td>';
		end_row();
	}
	end_table(1);
}

display_heading(_('Part 4 - forecast'));
start_table(TABLESTYLE_NOBORDER);
start_row();
fiscalyears_list_cells(_('Fiscal Year:'), 'fyear', null);
submit_cells('SelectYear', _('Get'), '', false, false);
end_row();
end_table(1);
if ($fiscal_year) {
	$versions = dimension_forecast_recent($fiscal_year, 100);
	if ($versions === false || !count($versions))
		display_note(_('No forecast has been proposed for this fiscal year.'), 0, 1);
	else {
		text_row_ex(_('Decision note:'), 'decision_note', 40, 200, '', get_post('decision_note', ''));
		start_table(TABLESTYLE);
		table_header(array(_('Version'), _('Kind'), _('As of'), _('Lines'), _('Total estimate'), _('State'), _('Commentary'), ''));
		foreach ($versions as $version) {
			start_row();
			label_cell('#'.$version['version_id'].' v'.$version['version_no']);
			label_cell($version['version_kind']);
			label_cell(sql2date($version['as_of_date']));
			label_cell($version['line_count'], "align='right'");
			amount_cell($version['total_amount']);
			label_cell($version['status']);
			label_cell(htmlspecialchars($version['commentary']));
			echo '<td>';
			if ($version['status'] === 'pending') {
				submit('ApproveForecast'.$version['version_id'], _('Approve'), true, _('Independent checker approval'), false);
				submit('RejectForecast'.$version['version_id'], _('Reject'), true, _('Reject this forecast'), false);
			}
			echo '</td>';
			end_row();
		}
		end_table(1);
	}
	display_note(_('One line per estimate: account, dimension 1, dimension 2, estimate to complete (the total still expected, including committed work; the word remove drops a line of the approved forecast).'), 0, 1);
	textarea_row(_('Estimates:'), 'forecast_changes', get_post('forecast_changes', ''), 50, 6);
	date_row(_('As of:'), 'forecast_as_of', get_post('forecast_as_of', Today()));
	text_row_ex(_('Commentary (required):'), 'forecast_note', 50, 500, '', get_post('forecast_note', ''));
	submit_center('ProposeForecast', _('Propose forecast'), true, _('The forecast waits for an independent checker'), false);
	if (get_post('fp_account', '') !== '') {
		$forecast = dimension_forecast_position($fiscal_year, get_post('fp_account'), max(0, (int)get_post('fp_dim1', 0)), max(0, (int)get_post('fp_dim2', 0)));
		if ($forecast !== false) {
			display_note($forecast['label'], 0, 1);
			if ($forecast['state'] === 'complete' || $forecast['state'] === 'no_estimate') {
				start_table(TABLESTYLE);
				foreach (array(_('GL actual') => $forecast['actual'], _('Remaining commitment') => $forecast['commitment'], _('Estimate to complete') => $forecast['etc'],
					_('Uncommitted estimate') => $forecast['uncommitted_etc'], _('Forecast') => $forecast['forecast']) as $label => $value) {
					start_row();
					label_cell($label);
					label_cell($value === null ? _('no estimate') : number_format2($value, 2), "align='right'");
					end_row();
				}
				end_table(1);
			} elseif ($forecast['state'] === 'forecast_tampered')
				display_error(_('The approved forecast no longer matches its digest and is not shown.'));
			else
				display_note(_('No approved forecast exists for this fiscal year.'), 0, 1);
		}
	}
}

end_form();
end_page();
