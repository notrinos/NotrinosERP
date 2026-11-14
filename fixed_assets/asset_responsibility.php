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
 * DIM-FA-001: prospective, effective-dated asset responsibility.
 *
 * The page previews and records who is responsible for the cost of a fixed asset from a date onward.
 * A change never touches a posting that already exists: the effective date must follow the last
 * depreciation posted and the previous responsibility.
 */
$page_security = 'SA_DEPRECIATION';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/admin/db/fiscalyears_db.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/fixed_assets/includes/fixed_assets_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Asset Responsibility'), false, false, '', $js);

//---------------------------------------------------------------------------------------------

/**
 * Handle the preview and record buttons.
 *
 * @return void
 */
function handle_responsibility_submit() {
	$carrier = fixed_asset_responsibility_carrier_from_post();
	if ($carrier === false) {
		display_error(_('A category cannot be both selected and explicitly unassigned.'));
		return;
	}

	if (get_post('record')) {
		try {
			$stored = fixed_asset_responsibility_record($_POST['stock_id'], $_POST['effective_date'], $carrier, get_post('reason'));
			display_notification(sprintf(_('The responsibility %s / %s applies to %s from %s. Earlier postings are unchanged.'),
				$stored['pair'][1], $stored['pair'][2], $_POST['stock_id'], $_POST['effective_date']));
		}
		catch (DimensionFinancialReferenceWriteException $exception) {
			display_error($exception->getMessage());
		}
		return;
	}

	$preview = fixed_asset_responsibility_preview($_POST['stock_id'], $_POST['effective_date'], $carrier);
	if (!$preview['ok']) {
		foreach ($preview['errors'] as $error)
			display_error($error);
		return;
	}
	display_notification(sprintf(_('Preview: from %s the asset is attributed to %s / %s. Depreciation already posted%s and every earlier posting keep their categories.'),
		$_POST['effective_date'], $preview['pair'][1], $preview['pair'][2],
		$preview['depreciated_through'] === null ? '' : sprintf(_(' through %s'), $preview['depreciated_through'])));
}

/**
 * Show the responsibility history and the classified depreciation of the selected asset.
 *
 * @param string $stock_id Fixed asset code
 * @return void
 */
function show_responsibility_history($stock_id) {
	$report = fixed_asset_responsibility_report($stock_id);
	if ($report === null)
		return;

	display_heading(_('Responsibility history'));
	if (!count($report['responsibility']))
		display_note(_('No responsibility is recorded yet; the first depreciation freezes the item default categories.'));
	else {
		start_table(TABLESTYLE, "width='60%'");
		table_header(array(_('Effective from'), _('Category 1'), _('Category 2'), _('Reason')));
		foreach ($report['responsibility'] as $entry) {
			start_row();
			label_cell(sql2date($entry['effective']));
			label_cell($entry[1]);
			label_cell($entry[2]);
			label_cell(isset($entry['evidence']['reason']) ? $entry['evidence']['reason'] : (isset($entry['evidence']['lineage']) ? $entry['evidence']['lineage'] : ''));
			end_row();
		}
		end_table();
	}

	display_heading(_('Classified depreciation'));
	start_table(TABLESTYLE, "width='60%'");
	label_row(_('Gross asset value (balance sheet, never expense)'), $report['gross_asset_value'], "align='right'", "align='right'");
	label_row(_('Depreciation expense, lifetime'), $report['expense']['lifetime'], "align='right'", "align='right'");
	foreach ($report['expense']['pairs'] as $pair => $amount)
		label_row(sprintf(_('  of which on categories %s'), $pair), $amount, "align='right'", "align='right'");
	label_row(_('Accumulated depreciation (contra)'), $report['accumulated'], "align='right'", "align='right'");
	label_row(_('Expense and accumulated depreciation reconcile'), $report['reconciles'] ? _('Yes') : _('No'), "align='right'", "align='right'");
	end_table(1);
}

//---------------------------------------------------------------------------------------------

if (get_post('preview') || get_post('record'))
	handle_responsibility_submit();

//---------------------------------------------------------------------------------------------

check_db_has_depreciable_fixed_assets(_('There are no fixed assets that could be depreciated.'));

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
stock_depreciable_fa_list_cells(_('Select an asset:'), 'stock_id', null, false, true);
end_row();
end_table();

start_table(TABLESTYLE2);

if (!isset($_POST['effective_date']))
	$_POST['effective_date'] = Today();

date_row(_('Effective from:'), 'effective_date');

if (fixed_asset_responsibility_ready()) {
	dimensions_list_row(dimension_category_policy_label(1).':', 'far_dimension_id', null, true,
		_('Choose a category'), false, 1, false);
	check_row(_('Leave this category explicitly unassigned'), '_far_classification_unassigned_1');
	if (get_company_pref('use_dimension') == 2) {
		dimensions_list_row(dimension_category_policy_label(2).':', 'far_dimension2_id', null, true,
			_('Choose a category'), false, 2, false);
		check_row(_('Leave this category explicitly unassigned'), '_far_classification_unassigned_2');
	}
	text_row(_('Reason:'), 'reason', null, 50, 200);
}
else
	label_row('', _('The Dimension source revision schema is not installed; asset responsibility cannot be recorded.'));

end_table(1);

if (fixed_asset_responsibility_ready()) {
	submit_center_first('preview', _('Preview'), true, false);
	submit_center_last('record', _('Record responsibility'), true, false);
	show_responsibility_history(get_post('stock_id'));
}

end_form();

end_page();
