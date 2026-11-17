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
 * DIM-ALC-001: guided Dimension allocation and reclassification journal.
 *
 * Step 1 picks posted ledger rows (the source pool), step 2 states a fixed percentage or amount rule, step 3
 * previews the exact balancing journal and, only after a successful preview, runs it through the ordinary
 * journal approval path. A posted run can be reversed by its exact inverse journal. The page owns no posting
 * logic: everything is decided by `tracking/includes/tracking_allocation.inc`.
 */
$page_security = 'SA_JOURNALENTRY';
$path_to_root = '..';
include_once($path_to_root.'/includes/ui/items_cart.inc');
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_validation_contract.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/gl/includes/gl_ui.inc');
include_once($path_to_root.'/taxes/db/tax_types_db.inc');
include_once($path_to_root.'/includes/approval/ui/approval_ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_allocation.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();
$help_context = 'Dimension Allocation Journal';
page(_('Cost Allocation Journal'), false, false, '', $js);

//-----------------------------------------------------------------------------------------------

/** Posted source rows selected on the form. @return int[] */
function dimension_allocation_page_selected_sources()
{
	$selected = array();
	foreach ($_POST as $name => $value)
		if (preg_match('/^src_([0-9]{1,10})$/D', $name, $match) && $value)
			$selected[] = (int)$match[1];
	sort($selected);
	return $selected;
}

/** Raw request built from the posted form. @return array */
function dimension_allocation_page_request()
{
	$targets = array();
	for ($index = 1; $index <= 6; $index++) {
		$dimension_id = (int)get_post('target_'.$index, 0);
		$raw = trim((string)get_post('value_'.$index, ''));
		if ($dimension_id === 0 && $raw === '')
			continue;
		$targets[] = array('dimension_id' => $dimension_id, 'value' => $raw === '' ? '' : input_num('value_'.$index));
	}
	return array('kind' => get_post('kind', ''), 'axis' => get_post('axis', ''), 'mode' => get_post('mode', ''),
		'date' => is_date(get_post('alc_date', '')) ? date2sql($_POST['alc_date']) : '',
		'memo' => get_post('alc_memo', ''), 'sources' => dimension_allocation_page_selected_sources(), 'targets' => $targets);
}

/** Render the exact preview of one valid request. */
function dimension_allocation_page_show_preview($request, $preview)
{
	display_heading(_('Preview - nothing has been posted yet'));
	start_table(TABLESTYLE);
	table_header(array(_('Ledger row'), _('Account'), _('Current scope'), _('Amount'), _('Rounding residual')));
	foreach ($preview['rows'] as $row) {
		start_row();
		label_cell('#'.$row['counter'].' ('.$row['type'].'/'.$row['type_no'].' '.sql2date($row['tran_date']).')');
		label_cell($row['account']);
		label_cell($row['dimension_id'].' / '.$row['dimension2_id']);
		label_cell(dimension_allocation_format_minor($row['amount_minor']), "align='right'");
		label_cell(dimension_allocation_format_minor($row['residual_minor']), "align='right'");
		end_row();
		foreach ($row['shares'] as $share) {
			start_row();
			label_cell('');
			label_cell(sprintf(_('to tracking value #%d'), $share['dimension_id']), "colspan='2'");
			label_cell(dimension_allocation_format_minor($row['amount_minor'] < 0 ? -$share['minor'] : $share['minor']), "align='right'");
			label_cell('');
			end_row();
		}
	}
	end_table(1);
	start_table(TABLESTYLE);
	table_header(array(_('Account'), dimension_axis_label_html(1), dimension_axis_label_html(2), _('Debit'), _('Credit')));
	foreach ($preview['lines'] as $line) {
		start_row();
		label_cell($line['code_id']);
		label_cell($line['dimension_id']);
		label_cell($line['dimension2_id']);
		label_cell($line['minor'] > 0 ? dimension_allocation_format_minor($line['minor']) : '', "align='right'");
		label_cell($line['minor'] < 0 ? dimension_allocation_format_minor(-$line['minor']) : '', "align='right'");
		end_row();
	}
	end_table(1);
	display_note(sprintf(_('Source pool %s moves between tracking values on the same accounts; the company total of every account is unchanged (net %s).'),
		dimension_allocation_format_minor(abs($preview['totals']['source_pool_minor'])), dimension_allocation_format_minor($preview['totals']['net_minor'])), 0, 1);
}

/** Show the outcome of a run or an inverse. */
function dimension_allocation_page_show_outcome($outcome)
{
	if (!$outcome['ok']) {
		foreach ($outcome['errors'] as $error)
			display_error($error);
		return;
	}
	$suffix = $outcome['duplicate'] ? ' '._('(this identical request already exists; nothing new was posted)') : '';
	if ($outcome['state'] === 'posted')
		display_notification(sprintf(_('Allocation run #%d is posted as journal #%d.'), $outcome['run_id'], $outcome['trans_no']).$suffix);
	elseif ($outcome['state'] === 'pending')
		display_notification(sprintf(_('Allocation run #%d is waiting for approval.'), $outcome['run_id']).$suffix);
	else
		display_notification(sprintf(_('Allocation run #%d is %s.'), $outcome['run_id'], $outcome['state']).$suffix);
}

//-----------------------------------------------------------------------------------------------

$schema = dimension_allocation_schema_ready();
if ($schema !== true) {
	display_error(_('The allocation evidence tables are not installed. Upgrade the company database first.'));
	end_page();
	exit;
}

$preview_state = null;
if ((isset($_POST['Preview']) || isset($_POST['RunAllocation'])) && check_csrf_token()) {
	$request = dimension_allocation_page_request();
	$normal = dimension_allocation_normalize_request($request);
	if (!$normal['ok']) {
		foreach ($normal['errors'] as $error)
			display_error($error);
	} elseif (isset($_POST['Preview'])) {
		$rows = dimension_allocation_load_sources($normal['request']['sources']);
		$reserved = $rows === false ? false : dimension_allocation_reservations($normal['request']['sources']);
		$check = $rows === false || $reserved === false ? array('ok' => false, 'errors' => array(_('The source rows could not be read.')))
			: dimension_allocation_check_sources($normal['request'], $rows, $reserved);
		if (!$check['ok']) {
			foreach ($check['errors'] as $error)
				display_error($error);
		} else {
			$preview = dimension_allocation_build_preview($normal['request'], $rows);
			if ($preview['ok'])
				$preview_state = array($normal['request'], $preview);
			else
				foreach ($preview['errors'] as $error)
					display_error($error);
		}
	} else {
		dimension_allocation_page_show_outcome(dimension_allocation_submit($request));
	}
}
$inverse_id = find_submit('Inverse');
if ($inverse_id != -1 && check_csrf_token()) {
	if (!is_date(get_post('inverse_date', '')))
		display_error(_('The inverse date is invalid.'));
	else
		dimension_allocation_page_show_outcome(dimension_allocation_submit_inverse($inverse_id, date2sql($_POST['inverse_date'])));
}
$retry_id = find_submit('Retry');
if ($retry_id != -1 && check_csrf_token())
	dimension_allocation_page_show_outcome(dimension_allocation_retry($retry_id));

//-----------------------------------------------------------------------------------------------

start_form();

display_heading(_('Step 1 - choose the posted ledger rows to allocate'));
start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'from_date', '', null, -30);
date_cells(_('To:'), 'to_date', '', null, 1);
gl_all_accounts_list_cells(_('Account:'), 'filter_account', null, false, false, _('All accounts'));
dimensions_history_list_cells(dimension_term_value().':', 'filter_dimension', null, true, _('Any'));
submit_cells('FindRows', _('Find rows'), '', false, false);
end_row();
end_table(1);

if (isset($_POST['FindRows']) || isset($_POST['Preview']) || isset($_POST['RunAllocation']) || find_submit('Inverse') != -1 || find_submit('Retry') != -1) {
	$conditions = array('g.amount<>0', 'g.person_id IS NULL');
	if (is_date(get_post('from_date', '')))
		$conditions[] = "g.tran_date>='".date2sql($_POST['from_date'])."'";
	if (is_date(get_post('to_date', '')))
		$conditions[] = "g.tran_date<='".date2sql($_POST['to_date'])."'";
	if (get_post('filter_account', '') !== '' && get_post('filter_account', '') !== ALL_TEXT)
		$conditions[] = 'g.account='.db_escape($_POST['filter_account']);
	if ((int)get_post('filter_dimension', 0) > 0)
		$conditions[] = '(g.dimension_id='.(int)$_POST['filter_dimension'].' OR g.dimension2_id='.(int)$_POST['filter_dimension'].')';
	$found = db_query('SELECT g.counter, g.type, g.type_no, g.tran_date, g.account, g.amount, g.dimension_id, g.dimension2_id, g.memo_,'
		.' IF(v.date_ IS NULL,0,1) AS voided, s.run_id AS reserved_by FROM '.TB_PREF.'gl_trans g LEFT JOIN '.TB_PREF.'voided v ON v.type=g.type AND v.id=g.type_no'
		.' LEFT JOIN '.TB_PREF.'dimension_allocation_sources s ON s.active_counter=g.counter WHERE '.implode(' AND ', $conditions)
		.' ORDER BY g.tran_date DESC, g.counter DESC LIMIT 100', 'could not list the source rows');
	start_table(TABLESTYLE);
	table_header(array(_('Select'), _('Date'), _('Document'), _('Account'), dimension_axis_label_html(1), dimension_axis_label_html(2), _('Amount'), _('Memo')));
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

display_heading(_('Step 2 - state the rule'));
start_table(TABLESTYLE2);
array_selector_row(_('Run type:'), 'kind', get_post('kind', 'allocation'), array('allocation' => _('Allocation (split between several targets)'),
	'reclassification' => _('Reclassification (move all to one target)')));
array_selector_row(dimension_term_category().':', 'axis', get_post('axis', '1'), array('1' => dimension_axis_label_html(1), '2' => dimension_axis_label_html(2)));
array_selector_row(_('Rule:'), 'mode', get_post('mode', 'percent'), array('percent' => _('Fixed percentages (total 100)'),
	'amount' => _('Fixed amounts (total the source pool)')));
date_row(_('Journal date:'), 'alc_date');
text_row(_('Memo:'), 'alc_memo', get_post('alc_memo', ''), 50, 200);
end_table(1);
start_table(TABLESTYLE);
table_header(array(_('Target tracking value'), _('Percentage or amount')));
for ($index = 1; $index <= 6; $index++) {
	start_row();
	echo '<td>'.dimensions_list('target_'.$index, null, true, _('(not used)'), false, false, 0).'</td>';
	amount_cells(null, 'value_'.$index, null, null, null, 4);
	end_row();
}
end_table(1);

submit_center_first('Preview', _('Step 3 - Preview'), _('Show the exact journal without posting anything'), false);
if ($preview_state !== null || isset($_POST['RunAllocation']))
	submit_center_last('RunAllocation', _('Run allocation (uses the journal approval path)'), _('Post or submit for approval the journal shown in the preview'), false);
br();

if ($preview_state !== null)
	dimension_allocation_page_show_preview($preview_state[0], $preview_state[1]);

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
