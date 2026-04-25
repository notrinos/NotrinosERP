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

$page_security = 'SA_VENDOREVALUATION';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Vendor Evaluation Criteria'));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	if (trim(get_post('criteria_name')) === '') {
		display_error(_('The criteria name cannot be empty.'));
		set_focus('criteria_name');
	} elseif (!check_num('criteria_weight', 0)) {
		display_error(_('The criteria weight must be greater than zero.'));
		set_focus('criteria_weight');
	} else {
		if ($selected_id !== '') {
			update_evaluation_criteria(
				$selected_id,
				trim(get_post('criteria_name')),
				get_post('criteria_category', 'quality'),
				input_num('criteria_weight', 1),
				trim(get_post('criteria_description')),
				get_post('scoring_method', 'manual'),
				trim(get_post('calculation_formula')),
				check_value('inactive') ? 1 : 0
			);
			display_notification(_('Vendor evaluation criteria has been updated.'));
		} else {
			add_evaluation_criteria(
				trim(get_post('criteria_name')),
				get_post('criteria_category', 'quality'),
				input_num('criteria_weight', 1),
				trim(get_post('criteria_description')),
				get_post('scoring_method', 'manual'),
				trim(get_post('calculation_formula')),
				check_value('inactive') ? 1 : 0
			);
			display_notification(_('New vendor evaluation criteria has been added.'));
		}

		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {
	if (key_in_foreign_table($selected_id, 'vendor_evaluation_scores', 'criteria_id')) {
		display_error(_('This criteria cannot be deleted because evaluation scores already reference it.'));
	} elseif (delete_evaluation_criteria($selected_id)) {
		display_notification(_('Selected criteria has been deleted.'));
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['criteria_name'] = '';
	$_POST['criteria_category'] = 'quality';
	$_POST['criteria_weight'] = 1;
	$_POST['criteria_description'] = '';
	$_POST['scoring_method'] = 'manual';
	$_POST['calculation_formula'] = '';
	$_POST['inactive'] = 0;
}

start_form();

start_table(TABLESTYLE, "width='85%'");
$th = array(
	_('ID'),
	_('Name'),
	_('Category'),
	_('Weight'),
	_('Method'),
	_('Usage'),
	'',
	''
);
inactive_control_column($th);
table_header($th);

$category_labels = get_vendor_evaluation_categories();
$method_labels = get_vendor_evaluation_scoring_methods();
$result = get_evaluation_criteria(false);
$k = 0;
while ($row = db_fetch($result)) {
	alt_table_row_color($k);
	label_cell($row['id']);
	label_cell($row['name']);
	label_cell(isset($category_labels[$row['category']]) ? $category_labels[$row['category']] : $row['category']);
	qty_cell($row['weight']);
	label_cell(isset($method_labels[$row['scoring_method']]) ? $method_labels[$row['scoring_method']] : $row['scoring_method']);
	label_cell((int)$row['usage_count'], 'align=right');
	inactive_control_cell($row['id'], $row['inactive'], 'vendor_evaluation_criteria', 'id');
	edit_button_cell('Edit' . $row['id'], _('Edit'));
	delete_button_cell('Delete' . $row['id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2, "width='65%'");

if ($selected_id !== '' && $Mode == 'Edit') {
	$criteria = get_vendor_evaluation_criteria_row($selected_id);
	$_POST['criteria_name'] = $criteria['name'];
	$_POST['criteria_category'] = $criteria['category'];
	$_POST['criteria_weight'] = $criteria['weight'];
	$_POST['criteria_description'] = $criteria['description'];
	$_POST['scoring_method'] = $criteria['scoring_method'];
	$_POST['calculation_formula'] = $criteria['calculation_formula'];
	$_POST['inactive'] = $criteria['inactive'];
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Criteria Name:'), 'criteria_name', 50, 100);

echo "<tr><td class='label'>" . _('Category:') . "</td><td>";
echo array_selector('criteria_category', get_post('criteria_category', 'quality'), $category_labels, array('class' => array('nosearch')));
echo "</td></tr>\n";

qty_row(_('Weight:'), 'criteria_weight', get_post('criteria_weight', 1), 6);

echo "<tr><td class='label'>" . _('Scoring Method:') . "</td><td>";
echo array_selector('scoring_method', get_post('scoring_method', 'manual'), $method_labels, array('class' => array('nosearch')));
echo "</td></tr>\n";

textarea_row(_('Description:'), 'criteria_description', get_post('criteria_description', ''), 45, 2);
textarea_row(_('Calculation Formula:'), 'calculation_formula', get_post('calculation_formula', ''), 45, 2);
check_row(_('Inactive:'), 'inactive', check_value('inactive'));

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();