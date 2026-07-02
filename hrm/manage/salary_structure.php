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
include_once($path_to_root.'/hrm/includes/db/grades_entity.inc');
include_once($path_to_root.'/hrm/includes/db/job_positions_entity.inc');
include_once($path_to_root.'/hrm/includes/db/pay_element_db.inc');

// ---------------------------------------------------------------------------
// Designer bootstrap — load the Visual Formula Designer when available.
// When the designer is loaded, an "Open Formula Designer" button appears
// next to formula fields. The designer renders in a modal; on "Create Formula"
// the serialized formula is transferred back to the page-level input.
// ---------------------------------------------------------------------------

$designer_available = false;
$designer_web_base = $path_to_root . '/includes/formula_designer';
$designer_bootstrap = $designer_web_base . '/designer_bootstrap.inc';
if (file_exists($designer_bootstrap)) {
    include_once $designer_bootstrap;
    if (class_exists('DesignerFacade')) {
        $designer_available = true;
        add_css_file($designer_web_base . '/assets/css/formula-designer.css');
        add_js_ufile($designer_web_base . '/assets/js/formula-designer.js');
        add_js_ufile($designer_web_base . '/assets/js/formula-dragdrop.js');
        add_js_ufile($designer_web_base . '/assets/js/formula-preview.js');
    }
}

//--------------------------------------------------------------------------

function display_salary_structure($position_id, $grade_id=0) {
	global $selected_id, $Mode, $designer_available;

	$position = job_positions_entity::find($position_id);
	$elements = get_pay_elements();
	$th = array(_('Pay Element'), _('Element Type'), _('Amount Type'), _('Amount'), _('Effective From'), '');

	start_table(TABLESTYLE, "width='100%'");
	table_header($th);
	start_row("class='inquirybg'");
	label_cell(_('Base Pay'));
	label_cell(_('Earnings'));
	label_cell(_('Fixed Amount'));
	amount_cell($position['basic_amount']);
	label_cell('');
	end_row();

	$k = 0;
	
	$as_of_date = get_post('effective_from', Today());
	while ($elements && ($element = db_fetch($elements))) {
		$amount = get_salary_structure_amount($position_id, $grade_id, $element['element_id'], $as_of_date);
		alt_table_row_color($k);
		label_cell($element['element_name']);
		label_cell($element['is_deduction'] == 0 ? _('Earnings') : _('Deduction'));
		label_cell($element['amount_type'] == 0 ? _('Fixed Amount') : _('Percentage of Base Pay'));

		if($element['amount_type'] == 0)
			amount_cell($amount);
		else
			label_cell(percent_format($amount).'%', "align='right'");
		label_cell($as_of_date ? $as_of_date : '-');
		edit_button_cell('Edit'.$element['element_id'], _('Edit'));
		end_row();
	}
	end_table(1);

	if($selected_id != '') {
	
		if($Mode == 'Edit') {

			$myrow = get_pay_element($selected_id);
			$amount = get_salary_structure_amount(get_post('position_id'), get_post('_tabs_sel'), $selected_id, get_post('effective_from', Today()));
			$_POST['amount'] = $myrow['amount_type'] == 0 ? price_format($amount) : percent_format($amount);

			start_table(TABLESTYLE2);

			label_row(_('Element Name:'), $myrow['element_name']);
			if($myrow['amount_type'] == 0)
				amount_row(_('Amount:'), 'amount', null, null, null, null, true);
			else
				percent_row(_('Percentage of Base Pay:'), 'amount');
			date_row(_('Effective From:'), 'effective_from', get_post('effective_from', Today()), null, 0, 0, 1001);
			date_row(_('Effective To:'), 'effective_to', get_post('effective_to', ''), null, 0, 0, 1001);
			
			textarea_row(_('Formula Override:'), 'formula', get_post('formula', ''), 255, 3);
			if ($designer_available) {
				start_row();
				echo '<td><button type="button" class="fd-modal-trigger-btn" '
					. 'id="formula-designer-trigger">'
					. _('Open Formula Designer') . '</button></td>';
				end_row();
			}

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

if(!job_positions_entity::has_records()) {
	display_error(_('No job position found in the system, please create job positions first.'));
	display_footer_exit();
}

if ($Mode=='UPDATE_ITEM') {

	if(!check_num('amount', 0.0)) {
		display_error(_('Amount/Percentage field must be a positive number.'));
		set_focus('amount');
	}
	elseif (!is_date(get_post('effective_from', Today()))) {
		display_error(_('Effective from date is invalid.'));
		set_focus('effective_from');
	}
	elseif (get_post('effective_to', '') !== '' && !is_date(get_post('effective_to'))) {
		display_error(_('Effective to date is invalid.'));
		set_focus('effective_to');
	}
	elseif (get_post('effective_to', '') !== '' && date1_greater_date2(get_post('effective_from', Today()), get_post('effective_to'))) {
		display_error(_('Effective to date cannot be earlier than effective from date.'));
		set_focus('effective_to');
	}
	else {
		$effective_from = get_post('effective_from', Today());
		$effective_to = get_post('effective_to', '');
		$formula = get_post('formula', '');

		$extra = array(
			'effective_from' => $effective_from,
			'effective_to' => $effective_to,
			'formula' => $formula,
			'is_active' => 1
		);

		$saved = false;
		if(!salary_structure_exact_row_exists($_POST['position_id'], get_post('_tabs_sel'), $selected_id, $effective_from)) {
			$saved = add_salary_structure_element($_POST['position_id'], get_post('_tabs_sel'), $selected_id, input_num('amount'), $extra) !== false;
		}
		else {
			$saved = update_salary_structure($_POST['position_id'], get_post('_tabs_sel'), $selected_id, input_num('amount'), $extra);
		}

		if ($saved) {
			display_notification(_('The selected pay element has been updated.'));
			$Mode = 'RESET';
		} else {
			display_error(_('Could not save the selected pay element.'));
		}
	}
}

start_form();

start_table();
start_row();
positions_list_cells(_('Job Position:'), 'position_id', null, true, false);
// date_cells(_('As of Date:'), 'effective_from', get_post('effective_from', Today()));
end_row();
end_table();

$position_id = get_post('position_id', '');

$tabs = array(0 => array(_('Basic'), 1));
$grades = grades_entity::all_db_resource('!inactive', array('grade_level', 'grade_name'));
while ($grades && ($grade = db_fetch($grades))) {
	$tabs[(int)$grade['grade_id']] = array($grade['grade_name'], 1);
}

tabbed_content_start('tabs', $tabs);

display_salary_structure($position_id, get_post('_tabs_sel'));

tabbed_content_end();

end_form();

// ---------------------------------------------------------------------------
// Formula Designer Modal (centralized via DesignerFacade)
// ---------------------------------------------------------------------------
if ($designer_available) {
    DesignerFacade::renderModal(array(
        'formulaValue'        => get_post('formula', ''),
        'module'              => 'hrm',
        'textareaName'        => 'formula_designer_modal_ss',
        'targetFieldSelector' => 'textarea[name="formula"]',
    ));
}

end_page();
