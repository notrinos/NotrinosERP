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

$page_security = 'SA_SALARYSTRUCTURE';
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/salary_structure_db.inc');
include_once($path_to_root.'/hrm/includes/db/grade_db.inc');
include_once($path_to_root.'/hrm/includes/db/job_position_db.inc');
include_once($path_to_root.'/hrm/includes/db/pay_element_db.inc');

//--------------------------------------------------------------------------

function display_salary_structure($position_id, $grade_id=0) {
	global $selected_id, $Mode;

	$position = get_job_position($position_id);
	$elements = get_pay_elements();
	$th = array(_('Pay Element'), _('Element Type'), _('Amount Type'), _('Amount'), '');

	start_table(TABLESTYLE2, "width='50%'");
	table_header($th);
	start_row("class='inquirybg'");
	label_cell(_('Base Pay'));
	label_cell(_('Earnings'));
	label_cell(_('Fixed Amount'));
	amount_cell($position['basic_amount']);
	label_cell('');
	end_row();

	$k = 0;
	
	foreach($elements as $element) {
		$amount = get_salary_structure_amount($position_id, $grade_id, $element['element_id']);
		alt_table_row_color($k);
		label_cell($element['element_name']);
		label_cell($element['is_deduction'] == 0 ? _('Earnings') : _('Deduction'));
		label_cell($element['amount_type'] == 0 ? _('Fixed Amount') : _('Percentage of Base Pay'));

		if($element['amount_type'] == 0)
			amount_cell($amount);
		else
			label_cell(percent_format($amount).'%', "align='right'");
		edit_button_cell('Edit'.$element['element_id'], _('Edit'));
		end_row();
	}
	end_table(1);

	if($selected_id != '') {
	
		if($Mode == 'Edit') {

			$myrow = get_pay_element($selected_id);
			$amount = get_salary_structure_amount(get_post('position_id'), get_post('_tabs_sel'), $selected_id);
			$_POST['amount'] = $myrow['amount_type'] == 0 ? price_format($amount) : percent_format($amount);

			start_table(TABLESTYLE2);

			label_row(_('Element Name:'), $myrow['element_name']);
			if($myrow['amount_type'] == 0)
				amount_row(_('Amount:'), 'amount', null, null, null, null, true);
			else
				percent_row(_('Percentage of Base Pay:'), 'amount');

			end_table(1);

			submit_add_or_update_center($selected_id == '', '', 'both');
			br();
		}
		hidden('selected_id', $selected_id);
	}
}

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Salary Structure'), false, false, '', $js);
simple_page_mode();

if(!db_has_job_position()) {
	display_error(_('No job position found in the system, please create job positions first.'));
	display_footer_exit();
}

if ($Mode=='UPDATE_ITEM') {

	if(!check_num('amount', 0.0)) {
		display_error(_('Amount/Percentage field must be a positive number.'));
		set_focus('amount');
	}
	else {

		if(!salary_structure_element_exist($_POST['position_id'], get_post('_tabs_sel'), $selected_id)) {
			add_salary_structure_element($_POST['position_id'], get_post('_tabs_sel'), $selected_id, input_num('amount'));
		}
		else {
			update_salary_structure($_POST['position_id'], get_post('_tabs_sel'), $selected_id, input_num('amount'));
		}

		display_notification(_('The selected pay element has been updated.'));
		
		$Mode = 'RESET';
	}
}

start_form();


start_table();
start_row();
positions_list_cells(_('Job Position:'), 'position_id', null, true, false);
end_row();
end_table();

$position_id = get_post('position_id', '');

$tabs = array(0 => array(_('Basic'), 1));
$grades = get_pay_grades();

foreach($grades as $grade) {
	$tabs[$grade['grade_id']] = array($grade['grade_name'], 1);
}

tabbed_content_start('tabs', $tabs);

display_salary_structure($position_id, get_post('_tabs_sel'));

tabbed_content_end();

end_form();
end_page();
