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

$page_security = 'SA_PAYELEMENT';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/db/pay_element_db.inc');

//--------------------------------------------------------------------------
	
// ---------------------------------------------------------------------------
// Designer bootstrap — load the Visual Formula Designer when available.
// The designer replaces the plain textarea with a visual formula builder.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Designer bootstrap — load the Visual Formula Designer when available.
// The designer renders in a modal triggered by a button next to the
// formula textarea. The modal's "Create Formula" button fills the textarea.
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

// ---------------------------------------------------------------------------

page(_($help_context = 'Manage Pay Elements'));
simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	
	if(empty(trim($_POST['element_name']))) {
		display_error(_('Element Name cannot be empty.'));
		set_focus('element_name');
	}
	elseif(check_pay_element_duplicated($selected_id, $_POST['account_code'])) {
		display_error(_('Selected account is being used for another element.'));
		set_focus('account_code');
	}
	elseif((int)get_post('amount_type', 0) == HRM_AMTTYPE_FORMULA && empty(trim(get_post('formula')))) {
		display_error(_('Formula is required when Amount Type is Formula.'));
		set_focus('formula');
	}
	else {
		$extra = array(
			'element_code' => get_post('element_code', ''),
			'element_category' => get_post('element_category', ((int)get_post('is_deduction', 0) ? 2 : 1)),
			'default_amount' => input_num('default_amount'),
			'formula' => get_post('formula', ''),
			'employer_account' => get_post('employer_account', ''),
			'is_taxable' => get_post('is_taxable', 1),
			'affects_gross' => get_post('affects_gross', 1),
			'max_amount' => get_post('max_amount', ''),
			'min_amount' => get_post('min_amount', ''),
			'display_order' => get_post('display_order', 0),
			'description' => get_post('description', '')
		);

		if($selected_id == '') {
			add_pay_element($_POST['element_name'], $_POST['account_code'], $_POST['is_deduction'], $_POST['amount_type'], $extra);
			display_notification(_('Pay element has been added.'));
		}
		else {
			update_pay_element($selected_id, $_POST['element_name'], $_POST['account_code'], $_POST['is_deduction'], $_POST['amount_type'], $extra);
			display_notification(_('The selected pay element has been updated.'));
		}
		
		$Mode = 'RESET';
	}
}

//--------------------------------------------------------------------------

if($Mode == 'Delete') {

	if(pay_element_used($selected_id))
		display_error(_('Cannot delete this account because payroll rules have been created using it.'));
	else {
		delete_pay_element($selected_id);
		display_notification(_('Selected account has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['account_code'] = '';
	$_POST['element_code'] = '';
	$_POST['element_name'] = '';
	$_POST['element_category'] = 1;
	$_POST['default_amount'] = '';
	$_POST['formula'] = '';
	$_POST['employer_account'] = '';
	$_POST['is_taxable'] = 1;
	$_POST['affects_gross'] = 1;
	$_POST['max_amount'] = '';
	$_POST['min_amount'] = '';
	$_POST['display_order'] = 0;
	$_POST['description'] = '';
}

//--------------------------------------------------------------------------

$result = get_pay_elements();
$categories = hrm_get_element_categories();
$amount_types = hrm_get_amount_types();

start_form();
start_table(TABLESTYLE, "width='100%'");
$th = array(_('Code'), _('Element Name'), _('Category'), _('Element Type'), _('Amount Type'), _('Account Code'), _('Account Name'), '', '');

table_header($th);

$k = 0; 
while($myrow = db_fetch($result)) {

	alt_table_row_color($k);

	label_cell($myrow['element_code']);
	label_cell($myrow['element_name']);
	label_cell(isset($categories[$myrow['element_category']]) ? $categories[$myrow['element_category']] : '-');
	label_cell($myrow['is_deduction'] == 0 ? _('Earnings') : _('Deduction'));
	label_cell(isset($amount_types[$myrow['amount_type']]) ? $amount_types[$myrow['amount_type']] : _('Fixed Amount'));
	label_cell($myrow['account_code'], "align='center'");
	label_cell($myrow['account_name']);
	edit_button_cell('Edit'.$myrow['element_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['element_id'], _('Delete'));
	
	end_row();
}

end_table(1);

//--------------------------------------------------------------------------

start_outer_table(TABLESTYLE2, "data-order-header='1'");

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		$myrow = get_pay_element($selected_id);
		$_POST['element_code']  = @$myrow['element_code'];
		$_POST['element_name']  = $myrow['element_name'];
		$_POST['account_code']  = $myrow['account_code'];
		$_POST['is_deduction'] = $myrow['is_deduction'];
		$_POST['amount_type'] = $myrow['amount_type'];
		$_POST['element_category'] = @$myrow['element_category'];
		$_POST['default_amount'] = @$myrow['default_amount'];
		$_POST['formula'] = @$myrow['formula'];
		$_POST['employer_account'] = @$myrow['employer_account'];
		$_POST['is_taxable'] = isset($myrow['is_taxable']) ? $myrow['is_taxable'] : 1;
		$_POST['affects_gross'] = isset($myrow['affects_gross']) ? $myrow['affects_gross'] : 1;
		$_POST['max_amount'] = @$myrow['max_amount'];
		$_POST['min_amount'] = @$myrow['min_amount'];
		$_POST['display_order'] = @$myrow['display_order'];
		$_POST['description'] = @$myrow['description'];
	}
	hidden('selected_id', $selected_id);
}

table_section(1);

text_row_ex(_('Element Code:'), 'element_code', 20, 20);
text_row_ex(_('Element Name:'), 'element_name', 37, 50);
array_selector_row(_('Element Category:'), 'element_category', get_post('element_category', 1), $categories);
gl_all_accounts_list_row(_('Select Account:'), 'account_code', null, true);
gl_all_accounts_list_row(_('Employer Account:'), 'employer_account', null, true, false, _('Optional'));
label_row(_('Element Type:'), radio(_('Earnings'), 'is_deduction', 0, 1).'&nbsp;&nbsp;'.radio(_('Deduction'), 'is_deduction', 1));
$types = array(_('Fixed Amount'), _('Percentage of Basic'), _('Percentage of Gross'), _('Formula'), _('Attendance Based'));
array_selector_row(_('Amount Type:'), 'amount_type', 0, $types);
amount_row(_('Default Amount:'), 'default_amount');

table_section(2);

// Phase 10: Keep the original textarea for formula input.
// If the Visual Formula Designer is available, add an
// "Open Formula Designer" button that launches the designer in
// a modal overlay.  The modal's "Create Formula" button fills
// the textarea from the designer's serialized output.
textarea_row(_('Formula:'), 'formula', null, 255, 3);
if ($designer_available) {
    echo '<tr><td></td><td>';
    echo '<button type="button" class="fd-modal-trigger-btn" '
        . 'id="formula-designer-trigger">'
        . _('Open Formula Designer') . '</button>';
    echo '</td></tr>';
} else {
    textarea_row(_('Formula:'), 'formula', null, 255, 3);
}
yesno_list_row(_('Taxable:'), 'is_taxable');
yesno_list_row(_('Affects Gross:'), 'affects_gross');
amount_row(_('Minimum Amount:'), 'min_amount');
amount_row(_('Maximum Amount:'), 'max_amount');
small_amount_row(_('Display Order:'), 'display_order', get_post('display_order', 0), 0, 9999);
textarea_row(_('Description:'), 'description', null, 50, 3);

end_outer_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();

// ---------------------------------------------------------------------------
// Phase 10: Formula Designer Modal
// Rendered when the "Open Formula Designer" button is clicked.
// The designer's serialized formula is transferred to the hidden source
// textarea (named 'formula') when the user clicks "Create Formula".
// ---------------------------------------------------------------------------
if ($designer_available) {
    $formula_value = get_post('formula', '');

    echo '<div class="fd-modal-overlay" id="fd-modal-overlay" style="display:none;">';
    echo '<div class="fd-modal-panel" id="fd-modal-panel">';
    echo '<div class="fd-modal-header">';
    echo '<h3 class="fd-modal-title">' . _('Formula Designer') . '</h3>';
    echo '<button type="button" class="fd-modal-close" '
        . 'aria-label="' . _('Close') . '">×</button>';
    echo '</div>';
    echo '<div class="fd-modal-body" id="fd-modal-body">';
    // Render the designer inside the modal, writing to a temporary
    // textarea so the modal instance does not interfere with the
    // page-level source textarea.
    echo DesignerFacade::renderEditor(
        $formula_value,
        'hrm',
        array(
            'textareaName' => 'formula_designer_modal',
            'baseUrl'      => $path_to_root,
        )
    );
    echo '</div>';
    echo '<div class="fd-modal-footer">';
    echo '<button type="button" class="fd-modal-action fd-modal-action--create">'
        . _('Create Formula') . '</button>';
    echo '<button type="button" class="fd-modal-action fd-modal-action--cancel">'
        . _('Cancel') . '</button>';
    echo '</div>';
    echo '</div>';
    echo '</div>';

    // Inline JavaScript controllers for modal open/close and formula transfer.
    add_js_source('
        jQuery(function($) {
            $("#fd-modal-overlay").on("click", function(e) {
                if (e.target === this) {
                    $(this).hide();
                }
            });
            $("#formula-designer-trigger").on("click", function() {
                $("#fd-modal-overlay").css("display", "flex");
            });
            $(".fd-modal-close, .fd-modal-action--cancel").on("click", function() {
                $("#fd-modal-overlay").hide();
            });
            $(".fd-modal-action--create").on("click", function() {
                var modalVal = $(\'textarea[name="formula_designer_modal"]\').val();
                $(\'textarea[name="formula"]\').val(modalVal).trigger("input");
                $("#fd-modal-overlay").hide();
            });
        });
    ');

    // Modal styling — injected inline to avoid a separate CSS request.
    add_js_source('
        (function() {
            var style = document.createElement("style");
            style.textContent = '
        . json_encode(
            '.fd-modal-overlay {'
            . 'display:flex; align-items:flex-start; justify-content:center;'
            . 'position:fixed; z-index:10000; left:0; top:0; width:100%;'
            . 'height:100%; overflow:auto; background-color:rgba(0,0,0,0.55);'
            . 'padding-top:60px; }'
            . '.fd-modal-panel {'
            . 'background:#fff; border-radius:8px; min-width:1100px;'
            . 'max-width:96vw; max-height:90vh; overflow:auto;'
            . 'box-shadow:0 20px 60px rgba(0,0,0,0.25); display:flex;'
            . 'flex-direction:column; }'
            . '.fd-modal-header {'
            . 'display:flex; justify-content:space-between; align-items:center;'
            . 'padding:14px 20px; border-bottom:1px solid #e5e7eb; }'
            . '.fd-modal-title { margin:0; font-size:1.15rem; }'
            . '.fd-modal-close {'
            . 'background:none; border:none; font-size:1.5rem;'
            . 'cursor:pointer; line-height:1; padding:0 4px; }'
            . '.fd-modal-body { padding:0; flex:1 1 auto; overflow:auto; }'
            . '.fd-modal-footer {'
            . 'display:flex; justify-content:flex-end; gap:10px;'
            . 'padding:12px 20px; border-top:1px solid #e5e7eb; }'
            . '.fd-modal-action--create {'
            . 'background:var(--modern-color-primary,#4361ee); color:#fff;'
            . 'border:none; border-radius:5px; padding:8px 22px;'
            . 'cursor:pointer; font-weight:600; }'
            . '.fd-modal-action--cancel {'
            . 'background:#f1f5f9; color:#334155;'
            . 'border:1px solid #cbd5e1; border-radius:5px; padding:8px 22px;'
            . 'cursor:pointer; }'
            . '.fd-modal-trigger-btn {'
            . 'background:var(--modern-color-primary,#4361ee); color:#fff;'
            . 'border:none; border-radius:5px; padding:8px 22px;'
            . 'cursor:pointer; font-weight:600;'
            . 'display:inline-flex; align-items:center; gap:6px; }'
            . '.fd-modal-trigger-btn:hover { opacity:0.92; }'
        )
        . ';
        document.head.appendChild(style);
        })();
    ');
}

end_page();
