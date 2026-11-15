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
 * DIM-ALC-002: measured allocation drivers and driver-based allocation runs.
 *
 * Part 1 lists the driver versions and lets an independent checker approve or reject a proposed version. Part 2
 * proposes a new version (measured units per Dimension; payroll aggregates need the payroll evidence
 * permission and carry counts of at least three persons, never a person or an amount). Part 3 previews and runs
 * an allocation of posted ledger rows by an approved driver through the DIM-ALC-001 engine and the ordinary
 * journal approval path. The page owns no posting logic: everything is decided by
 * `dimensions/includes/dimension_allocation_driver.inc`.
 */
$page_security = 'SA_JOURNALENTRY';
$path_to_root = '..';
include_once($path_to_root.'/includes/ui/items_cart.inc');
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/dimensions/includes/dimension_validation_contract.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/gl/includes/gl_ui.inc');
include_once($path_to_root.'/taxes/db/tax_types_db.inc');
include_once($path_to_root.'/includes/approval/ui/approval_ui.inc');
include_once($path_to_root.'/dimensions/includes/dimension_allocation.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = 'Dimension Allocation Drivers'), false, false, '', $js);

//-----------------------------------------------------------------------------------------------

/** Posted source rows selected on the form. @return int[] */
function dimension_driver_page_selected_sources()
{
	$selected = array();
	foreach ($_POST as $name => $value)
		if (preg_match('/^src_([0-9]{1,10})$/D', $name, $match) && $value)
			$selected[] = (int)$match[1];
	sort($selected);
	return $selected;
}

/** Parse the measured-units textarea: one `dimension units [members]` line per Dimension. @return array */
function dimension_driver_page_values($text)
{
	$values = array();
	foreach (preg_split('/\r\n|\n|\r/', (string)$text) as $line) {
		$line = trim($line);
		if ($line === '')
			continue;
		$parts = preg_split('/[\s,;]+/', $line);
		$row = array('dimension_id' => $parts[0], 'units' => isset($parts[1]) ? $parts[1] : '');
		if (isset($parts[2]))
			$row['member_count'] = $parts[2];
		$values[] = $row;
	}
	return $values;
}

/** Raw driver proposal built from the posted form. @return array */
function dimension_driver_page_spec()
{
	$spec = array('driver_key' => trim((string)get_post('driver_key', '')), 'axis' => get_post('driver_axis', ''),
		'source_kind' => get_post('driver_source', ''), 'unit_label' => get_post('driver_unit', ''),
		'period_from' => is_date(get_post('driver_from', '')) ? date2sql($_POST['driver_from']) : '',
		'period_to' => is_date(get_post('driver_to', '')) ? date2sql($_POST['driver_to']) : '',
		'population_units' => trim((string)get_post('driver_population', '')), 'note' => get_post('driver_note', ''),
		'values' => dimension_driver_page_values(get_post('driver_values', '')));
	if (trim((string)get_post('driver_aggregate', '')) !== '')
		$spec['aggregate_source_sha256'] = trim((string)$_POST['driver_aggregate']);
	return $spec;
}

/** Raw driver run request built from the posted form. @return array */
function dimension_driver_page_run_request()
{
	$cover = array();
	foreach (preg_split('/[\s,;]+/', trim((string)get_post('run_cover', ''))) as $id)
		if ($id !== '')
			$cover[] = $id;
	return array('driver_id' => get_post('run_driver', ''), 'date' => is_date(get_post('run_date', '')) ? date2sql($_POST['run_date']) : '',
		'memo' => get_post('run_memo', ''), 'sources' => dimension_driver_page_selected_sources(),
		'residual_policy' => get_post('run_policy', 'block'), 'residual_dimension_id' => get_post('run_residual', ''), 'cover' => $cover);
}

/** Show the errors of a refused call or its success note. */
function dimension_driver_page_show_result($result, $success_text)
{
	if (!$result['ok']) {
		foreach ($result['errors'] as $error)
			display_error($error);
		return;
	}
	display_notification($success_text);
}

/** Show the outcome of a run. */
function dimension_driver_page_show_outcome($outcome)
{
	if (!$outcome['ok']) {
		foreach ($outcome['errors'] as $error)
			display_error($error);
		return;
	}
	$suffix = $outcome['duplicate'] ? ' '._('(this identical request already exists; nothing new was posted)') : '';
	$label = $outcome['fully_allocated'] ? ' '._('Fully allocated.') : ' '._('Partly allocated: a residual or an unmeasured Dimension remains.');
	if ($outcome['state'] === 'posted')
		display_notification(sprintf(_('Allocation run #%d is posted as journal #%d.'), $outcome['run_id'], $outcome['trans_no']).$label.$suffix);
	elseif ($outcome['state'] === 'pending')
		display_notification(sprintf(_('Allocation run #%d is waiting for approval.'), $outcome['run_id']).$label.$suffix);
	else
		display_notification(sprintf(_('Allocation run #%d is %s.'), $outcome['run_id'], $outcome['state']).$suffix);
}

/** Render the weights and exact journal of one prepared driver run. */
function dimension_driver_page_show_preview($prepared)
{
	display_heading(_('Preview - nothing has been posted yet'));
	start_table(TABLESTYLE);
	table_header(array(_('Dimension'), _('Role'), _('Measured units'), _('Share (%)')));
	foreach ($prepared['weights']['targets'] as $target) {
		start_row();
		label_cell('#'.$target['dimension_id']);
		label_cell($target['role'] === 'residual' ? _('Explicit residual') : _('Driver'));
		label_cell($target['units'], "align='right'");
		label_cell(number_format($target['ppm'] / 10000, 4, '.', ''), "align='right'");
		end_row();
	}
	end_table(1);
	display_note(dimension_driver_completeness_label($prepared['weights']), 0, 1);
	start_table(TABLESTYLE);
	table_header(array(_('Account'), _('Dimension'), _('Dimension 2'), _('Debit'), _('Credit')));
	foreach ($prepared['preview']['lines'] as $line) {
		start_row();
		label_cell($line['code_id']);
		label_cell($line['dimension_id']);
		label_cell($line['dimension2_id']);
		label_cell($line['minor'] > 0 ? dimension_allocation_format_minor($line['minor']) : '', "align='right'");
		label_cell($line['minor'] < 0 ? dimension_allocation_format_minor(-$line['minor']) : '', "align='right'");
		end_row();
	}
	end_table(1);
}

//-----------------------------------------------------------------------------------------------

if (dimension_allocation_schema_ready() !== true || dimension_driver_schema_ready() !== true) {
	display_error(_('The allocation driver tables are not installed. Upgrade the company database first.'));
	end_page();
	exit;
}

$prepared_state = null;
if (isset($_POST['ProposeDriver']) && check_csrf_token())
	dimension_driver_page_show_result(dimension_driver_submit(dimension_driver_page_spec()), _('The driver version was proposed and waits for an independent checker.'));
$approve_id = find_submit('ApproveDriver');
if ($approve_id != -1 && check_csrf_token())
	dimension_driver_page_show_result(dimension_driver_decide($approve_id, 'approve'), _('The driver version is approved and effective for its period.'));
$reject_id = find_submit('RejectDriver');
if ($reject_id != -1 && check_csrf_token())
	dimension_driver_page_show_result(dimension_driver_decide($reject_id, 'reject'), _('The driver version is rejected.'));
if ((isset($_POST['PreviewDriverRun']) || isset($_POST['RunDriverAllocation'])) && check_csrf_token()) {
	$request = dimension_driver_page_run_request();
	if (isset($_POST['PreviewDriverRun'])) {
		$preview = dimension_driver_preview($request);
		if ($preview['ok'])
			$prepared_state = $preview;
		else
			foreach ($preview['errors'] as $error)
				display_error($error);
	} else
		dimension_driver_page_show_outcome(dimension_driver_submit_run($request));
}
$inverse_id = find_submit('Inverse');
if ($inverse_id != -1 && check_csrf_token()) {
	if (!is_date(get_post('inverse_date', '')))
		display_error(_('The inverse date is invalid.'));
	elseif (!dimension_driver_has_access('SA_JOURNALENTRY'))
		display_error(_('You are not allowed to reverse allocations.'));
	else {
		$outcome = dimension_allocation_submit_inverse($inverse_id, date2sql($_POST['inverse_date']));
		$outcome['fully_allocated'] = false;
		dimension_driver_page_show_outcome($outcome);
	}
}
$retry_id = find_submit('Retry');
if ($retry_id != -1 && check_csrf_token()) {
	$outcome = dimension_allocation_retry($retry_id);
	if (!isset($outcome['fully_allocated']))
		$outcome['fully_allocated'] = false;
	dimension_driver_page_show_outcome($outcome);
}

//-----------------------------------------------------------------------------------------------

start_form();

display_heading(_('Part 1 - driver versions'));
$drivers = dimension_driver_recent(50);
if ($drivers === false || !count($drivers))
	display_note(_('No allocation driver has been proposed yet.'), 0, 1);
else {
	start_table(TABLESTYLE);
	table_header(array(_('Version'), _('Driver'), _('Category'), _('Source'), _('Unit'), _('Effective'), _('Measured'), _('Population'), _('State'), ''));
	foreach ($drivers as $driver) {
		start_row();
		label_cell('#'.$driver['driver_id']);
		label_cell(htmlspecialchars($driver['driver_key']).' v'.$driver['version_no']);
		label_cell($driver['axis']);
		label_cell($driver['source_kind'] === 'payroll_aggregate' ? _('Payroll aggregate') : _('Measured'));
		label_cell(htmlspecialchars($driver['unit_label']));
		label_cell(sql2date($driver['period_from']).' - '.sql2date($driver['period_to']));
		label_cell($driver['units_total'], "align='right'");
		label_cell($driver['population_units'], "align='right'");
		label_cell($driver['status']);
		if ($driver['status'] === 'pending') {
			echo '<td>';
			submit('ApproveDriver'.$driver['driver_id'], _('Approve'), true, _('Independent checker approval'), false);
			submit('RejectDriver'.$driver['driver_id'], _('Reject'), true, _('Reject this version'), false);
			echo '</td>';
		} else
			label_cell('');
		end_row();
	}
	end_table(1);
}

display_heading(_('Part 2 - propose a driver version'));
start_table(TABLESTYLE2);
text_row(_('Driver key:'), 'driver_key', get_post('driver_key', ''), 30, 64);
array_selector_row(_('Dimension category:'), 'driver_axis', get_post('driver_axis', '1'), array('1' => _('Dimension 1'), '2' => _('Dimension 2')));
array_selector_row(_('Source:'), 'driver_source', get_post('driver_source', 'measured'), array('measured' => _('Measured data (floor area, machine hours ...)'),
	'payroll_aggregate' => _('Payroll aggregate (headcount, fte or hours; needs the payroll evidence permission)')));
text_row(_('Unit:'), 'driver_unit', get_post('driver_unit', ''), 20, 40);
date_row(_('Effective from:'), 'driver_from');
date_row(_('Effective to:'), 'driver_to');
text_row(_('Measured population (optional):'), 'driver_population', get_post('driver_population', ''), 15, 15);
text_row(_('Payroll result-set digest (payroll aggregate only):'), 'driver_aggregate', get_post('driver_aggregate', ''), 66, 64);
text_row(_('Note:'), 'driver_note', get_post('driver_note', ''), 50, 200);
textarea_row(_('Measured units (one line per Dimension: id units [persons]):'), 'driver_values', get_post('driver_values', ''), 40, 8);
end_table(1);
submit_center('ProposeDriver', _('Propose driver version'), true, _('The version waits for an independent checker'), false);

display_heading(_('Part 3 - allocate posted ledger rows by an approved driver'));
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'from_date', '', null, -30);
date_cells(_('To:'), 'to_date', '', null, 1);
gl_all_accounts_list_cells(_('Account:'), 'filter_account', null, false, false, _('All accounts'));
submit_cells('FindRows', _('Find rows'), '', false, false);
end_row();
end_table(1);

if (isset($_POST['FindRows']) || isset($_POST['PreviewDriverRun']) || isset($_POST['RunDriverAllocation'])) {
	$conditions = array('g.amount<>0', 'g.person_id IS NULL');
	if (is_date(get_post('from_date', '')))
		$conditions[] = "g.tran_date>='".date2sql($_POST['from_date'])."'";
	if (is_date(get_post('to_date', '')))
		$conditions[] = "g.tran_date<='".date2sql($_POST['to_date'])."'";
	if (get_post('filter_account', '') !== '' && get_post('filter_account', '') !== ALL_TEXT)
		$conditions[] = 'g.account='.db_escape($_POST['filter_account']);
	$found = db_query('SELECT g.counter, g.type, g.type_no, g.tran_date, g.account, g.amount, g.dimension_id, g.dimension2_id, g.memo_,'
		.' IF(v.date_ IS NULL,0,1) AS voided, s.run_id AS reserved_by FROM '.TB_PREF.'gl_trans g LEFT JOIN '.TB_PREF.'voided v ON v.type=g.type AND v.id=g.type_no'
		.' LEFT JOIN '.TB_PREF.'dimension_allocation_sources s ON s.active_counter=g.counter WHERE '.implode(' AND ', $conditions)
		.' ORDER BY g.tran_date DESC, g.counter DESC LIMIT 100', 'could not list the source rows');
	start_table(TABLESTYLE);
	table_header(array(_('Select'), _('Date'), _('Document'), _('Account'), _('Dimension'), _('Dimension 2'), _('Amount'), _('Memo')));
	$shown = 0;
	while ($found && ($row = db_fetch_assoc($found))) {
		if ((int)$row['voided'] === 1 || is_bank_account($row['account']) || is_tax_account($row['account']) || is_subledger_account($row['account']))
			continue;
		$shown++;
		start_row();
		if ($row['reserved_by'] !== null)
			label_cell(sprintf(_('In run #%d'), $row['reserved_by']));
		else
			check_cells(null, 'src_'.$row['counter'], null);
		label_cell(sql2date($row['tran_date']));
		label_cell(get_trans_view_str($row['type'], $row['type_no']));
		label_cell($row['account']);
		label_cell($row['dimension_id']);
		label_cell($row['dimension2_id']);
		amount_cell($row['amount']);
		label_cell(htmlspecialchars($row['memo_']));
		end_row();
	}
	end_table(1);
	if (!$shown)
		display_note(_('No allocatable ledger rows match the filter.'), 0, 1);
}

$options = array();
foreach (is_array($drivers) ? $drivers : array() as $driver)
	if ($driver['status'] === 'approved')
		$options[$driver['driver_id']] = '#'.$driver['driver_id'].' '.$driver['driver_key'].' v'.$driver['version_no'].' ('.sql2date($driver['period_from']).' - '.sql2date($driver['period_to']).')';
start_table(TABLESTYLE2);
if (count($options))
	array_selector_row(_('Approved driver version:'), 'run_driver', get_post('run_driver', ''), $options);
else
	label_row(_('Approved driver version:'), _('No approved driver version exists yet.'));
date_row(_('Journal date:'), 'run_date');
text_row(_('Memo:'), 'run_memo', get_post('run_memo', ''), 50, 200);
array_selector_row(_('If the driver is incomplete:'), 'run_policy', get_post('run_policy', 'block'), array('block' => _('Block the run'),
	'dimension' => _('Allocate the unmeasured remainder to an explicit residual Dimension')));
text_row(_('Residual Dimension (id):'), 'run_residual', get_post('run_residual', ''), 10, 10);
text_row(_('Dimensions that must carry a measure (ids):'), 'run_cover', get_post('run_cover', ''), 40, 100);
end_table(1);
submit_center_first('PreviewDriverRun', _('Preview'), _('Show the exact weights and journal without posting anything'), false);
if ($prepared_state !== null || isset($_POST['RunDriverAllocation']))
	submit_center_last('RunDriverAllocation', _('Run allocation (uses the journal approval path)'), _('Post or submit for approval the journal shown in the preview'), false);
br();
if ($prepared_state !== null)
	dimension_driver_page_show_preview($prepared_state);

//-----------------------------------------------------------------------------------------------

display_heading(_('Recent runs'));
$runs = dimension_allocation_reconcile_pending() === false ? false : dimension_allocation_recent_runs(50);
if ($runs === false || !count($runs))
	display_note(_('No allocation run has been recorded yet.'), 0, 1);
else {
	date_row(_('Date of an inverse:'), 'inverse_date');
	start_table(TABLESTYLE);
	table_header(array(_('Run'), _('Type'), _('Date'), _('State'), _('Journal'), _('Total'), _('Reason'), ''));
	foreach ($runs as $run) {
		start_row();
		label_cell('#'.$run['run_id']);
		label_cell($run['run_kind']);
		label_cell(sql2date($run['tran_date']));
		label_cell($run['display_state']);
		label_cell($run['journal_trans_no'] ? get_gl_view_str(ST_JOURNAL, $run['journal_trans_no'], '#'.$run['journal_trans_no']) : '');
		amount_cell($run['source_total']);
		label_cell(htmlspecialchars($run['failure_reason']));
		if ($run['display_state'] === 'posted' && $run['run_kind'] !== 'inverse')
			submit_cells('Inverse'.$run['run_id'], _('Reverse'), '', _('Post the exact inverse journal'), false);
		elseif (in_array($run['display_state'], array('failed', 'rejected', 'cancelled'), true) && $run['run_kind'] !== 'inverse')
			submit_cells('Retry'.$run['run_id'], _('Retry'), '', _('Submit the same request again as a new run'), false);
		else
			label_cell('');
		end_row();
	}
	end_table(1);
}

end_form();
end_page();
