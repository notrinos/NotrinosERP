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
 * Manage Employees — per HRM_DEVELOPMENT_PLAN.md Section 10.1
 * 7-Tab interface: Personal, Employment, Salary, Documents, History, Dependents, Transactions
 */

$page_security = 'SA_EMPLOYEE';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/db_pager.inc');
include_once($path_to_root.'/includes/session.inc');
include($path_to_root.'/reporting/includes/tcpdf.php');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Manage Employees'), @$_REQUEST['popup'], false, '', $js);

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_hooks.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');
include_once($path_to_root.'/hrm/includes/hrm_ui.inc');

$new_employee = get_post('employee_id') == '' || get_post('cancel');

//======================================================================
// UTILITY FUNCTIONS
//======================================================================

function set_edit($employee_id) {
	$row = get_employee_by_code($employee_id);
	if (!$row) return;

	// Keep the page state aligned when an employee is opened directly or after tab switches.
	$row['NewEmpID'] = $employee_id;
	$row['employee_id'] = $employee_id;
	$_POST = array_merge($_POST, $row);

	// Convert SQL dates to user format
	$date_fields = array('birth_date', 'hire_date', 'released_date', 'confirmation_date', 
		'probation_end_date', 'passport_expiry');
	foreach ($date_fields as $f) {
		$_POST[$f] = empty($_POST[$f]) ? '' : sql2date($_POST[$f]);
	}
	$_POST['del_image'] = 0;
}

function del_image($employee_id) {
	foreach (array('jpg', 'png', 'gif') as $ext) {
		$filename = company_path().'/images/employees/'.item_img_name($employee_id).".".$ext;
		if (file_exists($filename) && !unlink($filename))
			return false;
	}
	return true;
}

function show_image($employee_id) {
	global $SysPrefs;

	$check_remove_image = false;
	$img_link = "<img id='emp_img' alt='No Image' src='".company_path().'/images/no_image.svg'."?nocache=".rand()."' height='100'>";

	if (!empty($employee_id)) {
		foreach (array('jpg', 'png', 'gif') as $ext) {
			$file = company_path().'/images/employees/'.item_img_name($employee_id).'.'.$ext;
			if (file_exists($file)) {
				$check_remove_image = true;
				$img_link = "<img id='emp_img' alt='[".$employee_id.".$ext]' src='".$file."?nocache=".rand()."' height='100' border='0'>";
				break;
			}
		}
	}
	label_row("&nbsp;", $img_link);
	if ($check_remove_image)
		check_row(_('Delete Image:'), 'del_image');
}

function clear_inputs() {
	$fields = array(
		'first_name', 'last_name', 'middle_name', 'address', 'city', 'state', 'country',
		'phone', 'mobile', 'email', 'personal_email',
		'birth_date', 'nationality', 'national_id', 'passport', 'passport_expiry',
		'bank_name', 'bank_branch', 'bank_account', 'bank_routing', 'tax_number', 'social_security_no',
		'emergency_name', 'emergency_relation', 'emergency_phone',
		'notes', 'NewEmpID', 'hire_date', 'confirmation_date', 'probation_end_date', 'released_date',
		'department_id', 'position_id', 'grade_id', 'shift_id', 'login_id', 'reporting_to',
	);
	foreach ($fields as $f)
		unset($_POST[$f]);
}

function collect_employee_data() {
	$data = array();

	// Personal
	$data['first_name']     = $_POST['first_name'];
	$data['last_name']      = $_POST['last_name'];
	$data['middle_name']    = get_post('middle_name', '');
	$data['gender']         = get_post('gender', 0);
	$data['birth_date']     = get_post('birth_date', '');
	$data['nationality']    = get_post('nationality', '');
	$data['marital_status'] = get_post('marital_status', 0);
	$data['dependents_no']  = input_num('dependents_no', 0);

	// Contact
	$data['address']        = get_post('address', '');
	$data['city']           = get_post('city', '');
	$data['state']          = get_post('state', '');
	$data['country']        = get_post('country', '');
	$data['phone']          = get_post('phone', '');
	$data['mobile']         = get_post('mobile', '');
	$data['email']          = get_post('email', '');
	$data['personal_email'] = get_post('personal_email', '');

	// Identification
	$data['national_id']       = get_post('national_id', '');
	$data['passport']          = get_post('passport', '');
	$data['passport_expiry']   = get_post('passport_expiry', '');
	$data['tax_number']        = get_post('tax_number', '');
	$data['social_security_no'] = get_post('social_security_no', '');

	// Banking
	$data['bank_name']     = get_post('bank_name', '');
	$data['bank_branch']   = get_post('bank_branch', '');
	$data['bank_account']  = get_post('bank_account', '');
	$data['bank_routing']  = get_post('bank_routing', '');
	$data['payment_method'] = get_post('payment_method', 0);

	// Emergency
	$data['emergency_name']     = get_post('emergency_name', '');
	$data['emergency_relation'] = get_post('emergency_relation', '');
	$data['emergency_phone']    = get_post('emergency_phone', '');

	// Employment
	$data['hire_date']          = get_post('hire_date', '');
	$data['confirmation_date']  = get_post('confirmation_date', '');
	$data['probation_end_date'] = get_post('probation_end_date', '');
	$data['released_date']      = get_post('released_date', '');
	$data['employment_type']    = get_post('employment_type', 0);
	$data['department_id']      = get_post('department_id', 0);
	$data['position_id']        = get_post('position_id', 0);
	$data['grade_id']           = get_post('grade_id', 0);
	$data['shift_id']           = get_post('shift_id', 0);
	$data['reporting_to']       = get_post('reporting_to', '');
	$data['personal_salary']    = get_post('personal_salary', 0);
	$data['cost_center_id']     = get_post('cost_center_id', 0);
	$data['login_id']           = get_post('login_id', '');

	// Notes
	$data['notes']              = get_post('notes', '');

	return $data;
}

//======================================================================
// TAB 1: PERSONAL INFORMATION
//======================================================================
function tab_personal($employee_id, $new_employee) {
	global $path_to_root, $SysPrefs;

	start_outer_table(TABLESTYLE2);

	// ── LEFT COLUMN ──────────────────────────────────────
	table_section(1);

	table_section_title(_('Identity'));

	if ($new_employee) {
		text_row(_('Employee ID:'), 'NewEmpID', null, 42, 20);
		$_POST['inactive'] = 0;
	} else {
		// Reload employee data if:
		// 1. NewEmpID differs from employee_id (different employee selected), OR
		// 2. Add/Update button was just clicked, OR
		// 3. NewEmpID is set but form data is empty (switching back from another tab)
		if (get_post('NewEmpID') != get_post('employee_id') || get_post('addupdate') || (get_post('NewEmpID') && empty(get_post('first_name')))) {
			set_edit(get_post('employee_id'));
		}
		label_row(_('Employee ID:'), get_post('NewEmpID'));
		hidden('NewEmpID', get_post('NewEmpID'));
		// Only set focus on initial load, not after form submission (let validation handle focus)
		if (!get_post('addupdate'))
			set_focus('first_name');
	}

	file_row(_('Photo (.jpg/.png/.gif):'), 'pic', 'pic');
	show_image(get_post('NewEmpID'));

	table_section_title(_('Personal Information'));

	text_row(_('First Name:'), 'first_name', null, 42, 100);
	text_row(_('Middle Name:'), 'middle_name', null, 42, 100);
	text_row(_('Last Name:'), 'last_name', null, 42, 100);

	gender_list_row(_('Gender:'), 'gender');
	date_row(_('Date of Birth:'), 'birth_date', null, null, 0, 0, -80);
	text_row(_('Nationality:'), 'nationality', null, 42, 60);
	marital_status_list_row(_('Marital Status:'), 'marital_status');
	qty_row(_('Number of Dependents:'), 'dependents_no', null, null, null, 0);

	// ── RIGHT COLUMN ─────────────────────────────────────
	table_section(2);

	table_section_title(_('Contact Information'));

	textarea_row(_('Address:'), 'address', null, 37, 3);
	text_row(_('City:'), 'city', null, 42, 60);
	text_row(_('State/Province:'), 'state', null, 42, 60);
	text_row(_('Country:'), 'country', null, 42, 60);
	text_row(_('Phone:'), 'phone', null, 42, 30);
	text_row(_('Mobile:'), 'mobile', null, 42, 30);
	email_row(_('Work Email:'), 'email', get_post('email'), 42, 100);
	email_row(_('Personal Email:'), 'personal_email', get_post('personal_email'), 42, 100);

	table_section_title(_('Identification'));

	text_row(_('National ID:'), 'national_id', null, 42, 100);
	text_row(_('Passport No:'), 'passport', null, 42, 100);
	date_row(_('Passport Expiry:'), 'passport_expiry', null, null, 0, 0, 1001);
	text_row(_('Tax ID:'), 'tax_number', null, 42, 100);
	text_row(_('Social Security No:'), 'social_security_no', null, 42, 100);

	table_section_title(_('Banking'));

	text_row(_('Bank Name:'), 'bank_name', null, 42, 100);
	text_row(_('Bank Branch:'), 'bank_branch', null, 42, 100);
	text_row(_('Account Number:'), 'bank_account', null, 42, 100);
	text_row(_('Routing/IBAN:'), 'bank_routing', null, 42, 60);
	payment_method_list_row(_('Payment Method:'), 'payment_method');

	table_section_title(_('Emergency Contact'));

	text_row(_('Contact Name:'), 'emergency_name', null, 42, 100);
	text_row(_('Relation:'), 'emergency_relation', null, 42, 60);
	text_row(_('Phone:'), 'emergency_phone', null, 42, 30);

	end_outer_table(1);
}

//======================================================================
// TAB 2: EMPLOYMENT INFORMATION
//======================================================================
function tab_employment($employee_id, $new_employee) {
	global $path_to_root;

	hidden('NewEmpID', get_post('NewEmpID'));
	
	// Reload employee data if editing and employment fields are absent from POST.
	// This happens when switching from a non-Employment tab (e.g. Personal) where
	// hire_date is not part of the submitted form.
	//
	// IMPORTANT: use array_merge($row, $_POST) so that any value the user has
	// already changed on THIS tab — in particular a freshly-selected Job Position
	// triggered via select_submit — is NOT overwritten by the DB-stored value.
	if (!$new_employee && get_post('NewEmpID') && empty(get_post('hire_date'))) {
		$row = get_employee_by_code(get_post('NewEmpID'));
		if ($row) {
			$date_fields = array('hire_date', 'confirmation_date', 'probation_end_date', 'released_date');
			foreach ($date_fields as $f)
				$row[$f] = empty($row[$f]) ? '' : sql2date($row[$f]);
			// DB values fill missing fields; POST values take priority so user
			// selections (e.g. a newly-picked Job Position) are never reverted.
			$_POST = array_merge($row, $_POST);
		}
	}
	
	start_outer_table(TABLESTYLE2);

	// ── LEFT COLUMN ──────────────────────────────────────
	table_section(1);

	table_section_title(_('Employment Details'));

	date_row(_('Hire Date:'), 'hire_date', null, null, 0, 0, 1001);
	date_row(_('Confirmation Date:'), 'confirmation_date', null, null, 0, 0, 1001);
	date_row(_('Probation End Date:'), 'probation_end_date', null, null, 0, 0, 1001);
	employment_type_list_row(_('Employment Type:'), 'employment_type');

	table_section_title(_('Organization'));

	departments_list_row(_('Department:'), 'department_id', null, false, _('Select department'));
	positions_list_row(_('Job Position:'), 'position_id', null, true, _('Select position'));
	
	// Display Job Class as label (read-only) — fetched from the selected position
	$job_class_name = _('Not assigned');
	$position_id = get_post('position_id');
	if ($position_id) {
		$position = get_job_position($position_id);
		if ($position && !empty($position['job_class_id'])) {
			$job_class = get_job_class($position['job_class_id']);
			$job_class_name = $job_class ? $job_class['class_name'] : _('Not assigned');
		}
	}
	label_row(_('Job Class:'), $job_class_name);
	
	grades_list_row(_('Pay Grade:'), 'grade_id', null, _('Basic'));

	// ── RIGHT COLUMN ─────────────────────────────────────
	table_section(2);

	table_section_title(_('Assignment'));

	work_shifts_list_row(_('Work Shift:'), 'shift_id', null, true);
	reporting_to_list_row(_('Reports To:'), 'reporting_to', null, get_post('NewEmpID'));
	users_list_row(_('System Login:'), 'login_id', null, false, _('Select user'));
	dimensions_list_row(_('Cost Center:'), 'cost_center_id', null, true, ' ', false, 1, false);

	table_section_title(_('Salary & Status'));

	yesno_list_row(_('Personal Salary Structure:'), 'personal_salary');
	date_row(_('Release Date:'), 'released_date', null, null, 0, 0, 1001);
	record_status_list_row(_('Employee Status:'), 'inactive');

	table_section_title(_('Notes'));

	textarea_row(_('Notes:'), 'notes', null, 50, 5);

	end_outer_table(1);
}

//======================================================================
// TAB 3: SALARY STRUCTURE
//======================================================================
function tab_salary($employee_id) {
	global $path_to_root, $Ajax;

	if (empty($employee_id)) {
		hidden('NewEmpID', get_post('NewEmpID'));
		display_note(_('Please save the employee first before configuring salary.'));
		return;
	}

	hidden('NewEmpID', get_post('NewEmpID'));
	$emp = get_employee_by_code($employee_id);
	if (!$emp) return;

	// If personal salary, process actions before rendering the current list.
	if ($emp['personal_salary']) {
		if (isset($_GET['edit_salary'])) {
			$editing_salary = get_employee_salary($_GET['edit_salary']);
			if ($editing_salary && $editing_salary['employee_id'] == $employee_id) {
				$_POST['editing_salary_id'] = $editing_salary['salary_id'];
				$_POST['sal_element_id'] = $editing_salary['element_id'];
				$_POST['sal_amount'] = price_format($editing_salary['amount']);
				$_POST['sal_formula'] = $editing_salary['formula'];
				$_POST['sal_reference'] = $editing_salary['reference'];
				$_POST['sal_effective_from'] = !empty($editing_salary['effective_from']) ? sql2date($editing_salary['effective_from']) : '';
				$_POST['sal_effective_to'] = !empty($editing_salary['effective_to']) ? sql2date($editing_salary['effective_to']) : '';
			}
		}

		// Handle delete salary element
		if (isset($_GET['delete_salary'])) {
			delete_employee_salary($_GET['delete_salary']);
			display_notification(_('Salary element removed.'));
			$Ajax->activate('_page_body');
		}

		// Handle add salary element
		if (isset($_POST['add_salary_element'])) {
			$input_error = 0;
			if (!get_post('sal_element_id')) {
				$input_error = 1;
				display_error(_('Please select a pay element.'));
			}
			if (input_num('sal_amount') == 0 && empty(get_post('sal_formula'))) {
				$input_error = 1;
				display_error(_('Please enter an amount or formula.'));
			}
			if (!get_post('sal_effective_from')) {
				$input_error = 1;
				display_error(_('Effective from date is required.'));
			}

			if (!$input_error) {
				if (get_post('editing_salary_id')) {
					$ok = update_employee_salary(
						get_post('editing_salary_id'),
						input_num('sal_amount'),
						get_post('sal_effective_from'),
						get_post('sal_effective_to'),
						get_post('sal_formula'),
						get_post('sal_reference')
					);
					if ($ok)
						display_notification(_('Salary element updated.'));
					else
						display_error(_('Could not update salary element.'));
				} else {
					$salary_id = add_employee_salary(
						$employee_id,
						get_post('sal_element_id'),
						input_num('sal_amount'),
						get_post('sal_effective_from'),
						get_post('sal_effective_to'),
						get_post('sal_formula'),
						get_post('sal_reference')
					);
					if ($salary_id)
						display_notification(_('Salary element added.'));
					else
						display_error(_('Could not add salary element. Check effective dates and duplicates.'));
				}
				unset($_POST['editing_salary_id'], $_POST['sal_element_id'], $_POST['sal_amount'], $_POST['sal_formula'], $_POST['sal_reference'], $_POST['sal_effective_from'], $_POST['sal_effective_to']);
				$Ajax->activate('_page_body');
			}
		}
	}

	// Header info
	start_table(TABLESTYLE2);
	label_row(_('Employee:'), $emp['first_name'].' '.$emp['last_name'].' ['.$employee_id.']');
	label_row(_('Position:'), $emp['position_name'] ?: _('Not assigned'));
	label_row(_('Grade:'), $emp['grade_name'] ?: _('Not assigned'));
	label_row(_('Salary Mode:'), $emp['personal_salary'] ? _('Personal (Individual Override)') : _('Position-based (Salary Structure)'));
	end_table(1);

	// Display salary components after processing actions so the list is current.
	$editable = $emp['personal_salary'] ? true : false;
	display_salary_components($employee_id, $editable);

	// If personal salary, show add form
	if ($emp['personal_salary']) {
		br();
		display_heading(_('Add / Edit Salary Element'));

		start_outer_table(TABLESTYLE2);
		table_section(1);

		hidden('editing_salary_id', get_post('editing_salary_id', ''));

		$sql = "SELECT element_id, CONCAT(element_code, ' - ', element_name) as name 
				FROM ".TB_PREF."pay_elements WHERE !inactive";
		label_row(_('Pay Element:'), combo_input('sal_element_id', get_post('sal_element_id'), $sql, 'element_id', 'name', array(
			'spec_option' => _('-- Select --'), 'spec_id' => '',
		)));

		amount_row(_('Amount:'), 'sal_amount');
		text_row(_('Formula:'), 'sal_formula', null, 50, 200);

		table_section(2);

		date_row(_('Effective From:'), 'sal_effective_from');
		date_row(_('Effective To:'), 'sal_effective_to', '', false, 0, 0, 1001);
		text_row(_('Reference:'), 'sal_reference', null, 42, 60);

		end_outer_table(1);

		submit_center('add_salary_element', _('Add / Update Element'), true, '', 'default');
	}
}

//======================================================================
// TAB 4: DOCUMENTS
//======================================================================
function tab_documents($employee_id) {
	global $Ajax;

	hidden('NewEmpID', get_post('NewEmpID'));

	if (empty($employee_id)) {
		display_note(_('Please save the employee first before managing documents.'));
		return;
	}

	// Handle delete document
	if (isset($_GET['delete_doc'])) {
		$doc = get_employee_document($_GET['delete_doc']);
		if ($doc && !empty($doc['file_path']) && file_exists($doc['file_path']))
			@unlink($doc['file_path']);
		delete_employee_document($_GET['delete_doc']);
		display_notification(_('Document deleted.'));
	}

	$editing_doc = null;
	if (isset($_GET['edit_doc'])) {
		$editing_doc = get_employee_document($_GET['edit_doc']);
		if ($editing_doc) {
			$_POST['doc_type_id'] = $editing_doc['doc_type_id'];
			$_POST['doc_name']    = $editing_doc['doc_name'];
			$_POST['doc_issue_date']  = !empty($editing_doc['issue_date']) ? sql2date($editing_doc['issue_date']) : '';
			$_POST['doc_expiry_date'] = !empty($editing_doc['expiry_date']) ? sql2date($editing_doc['expiry_date']) : '';
			$_POST['doc_notes']   = $editing_doc['notes'];
			$_POST['editing_doc_id'] = $editing_doc['doc_id'];
		}
	}

	if (isset($_POST['save_document'])) {
		$input_error = 0;
		if (!get_post('doc_type_id')) {
			$input_error = 1;
			display_error(_('Please select a document type.'));
		}
		if (empty(trim(get_post('doc_name')))) {
			$input_error = 1;
			display_error(_('Please enter a document name.'));
		}

		$file_path = '';
		if (isset($_FILES['doc_file']) && $_FILES['doc_file']['name'] != '') {
			$upload_dir = company_path().'/documents/employees';
			if (!file_exists($upload_dir))
				mkdir($upload_dir, 0777, true);
			$file_path = $upload_dir.'/'.$employee_id.'_'.time().'_'.$_FILES['doc_file']['name'];
			if (!move_uploaded_file($_FILES['doc_file']['tmp_name'], $file_path)) {
				$input_error = 1;
				display_error(_('Failed to upload file.'));
				$file_path = '';
			}
		}

		if (!$input_error) {
			if (get_post('editing_doc_id')) {
				$existing = get_employee_document(get_post('editing_doc_id'));
				if (empty($file_path) && $existing)
					$file_path = $existing['file_path'];
				update_employee_document(
					get_post('editing_doc_id'),
					get_post('doc_type_id'),
					get_post('doc_name'),
					$file_path,
					get_post('doc_issue_date'),
					get_post('doc_expiry_date'),
					get_post('doc_notes')
				);
				display_notification(_('Document updated.'));
			} else {
				add_employee_document(
					$employee_id,
					get_post('doc_type_id'),
					get_post('doc_name'),
					$file_path,
					get_post('doc_issue_date'),
					get_post('doc_expiry_date'),
					get_post('doc_notes')
				);
				display_notification(_('Document added.'));

				// Fire hook
				hrm_fire_hook('on_document_uploaded', $employee_id, get_post('doc_type_id'));
			}
			unset($_POST['doc_type_id'], $_POST['doc_name'], $_POST['doc_issue_date'], 
				$_POST['doc_expiry_date'], $_POST['doc_notes'], $_POST['editing_doc_id']);
			$Ajax->activate('_page_body');
		}
	}

	// Display existing documents after processing so the table is current.
	display_employee_documents($employee_id);

	// Add/Edit document form
	br();
	display_heading(_('Add / Edit Document'));

	start_outer_table(TABLESTYLE2);

	table_section(1);

	hidden('editing_doc_id', get_post('editing_doc_id', ''));
	document_types_list_row(_('Document Type:'), 'doc_type_id', null, true);
	text_row(_('Document Name:'), 'doc_name', null, 42, 200);
	file_row(_('Attachment:'), 'doc_file');

	table_section(2);

	date_row(_('Issue Date:'), 'doc_issue_date', '', false, 0, 0, 1001);
	date_row(_('Expiry Date:'), 'doc_expiry_date', '', false, 0, 0, 1001);
	textarea_row(_('Notes:'), 'doc_notes', null, 35, 3);

	end_outer_table(1);

	submit_center('save_document', _('Save Document'), true, '', 'default');
}

//======================================================================
// TAB 5: HISTORY
//======================================================================
function tab_history($employee_id) {
	hidden('NewEmpID', get_post('NewEmpID'));
	if (empty($employee_id)) {
		display_note(_('Please save the employee first.'));
		return;
	}

	display_employee_history($employee_id);
}

//======================================================================
// TAB 6: DEPENDENTS
//======================================================================
function tab_dependents($employee_id) {
	global $Ajax;

	hidden('NewEmpID', get_post('NewEmpID'));

	if (empty($employee_id)) {
		display_note(_('Please save the employee first before managing dependents.'));
		return;
	}

	// Handle delete dependent
	if (isset($_GET['delete_dep'])) {
		delete_employee_dependent($_GET['delete_dep']);
		display_notification(_('Dependent deleted.'));
	}

	$editing_dep = null;
	if (isset($_GET['edit_dep'])) {
		$editing_dep = get_employee_dependent($_GET['edit_dep']);
		if ($editing_dep) {
			$_POST['dep_name']          = $editing_dep['name'];
			$_POST['dep_relationship']  = $editing_dep['relationship'];
			$_POST['dep_birth_date']    = !empty($editing_dep['birth_date']) ? sql2date($editing_dep['birth_date']) : '';
			$_POST['dep_gender']        = $editing_dep['gender'];
			$_POST['dep_national_id']   = $editing_dep['national_id'];
			$_POST['dep_is_beneficiary'] = $editing_dep['is_beneficiary'];
			$_POST['editing_dep_id']    = $editing_dep['dependent_id'];
		}
	}

	if (isset($_POST['save_dependent'])) {
		$input_error = 0;
		if (empty(trim(get_post('dep_name')))) {
			$input_error = 1;
			display_error(_('Please enter the dependent\'s name.'));
		}
		if (empty(get_post('dep_relationship'))) {
			$input_error = 1;
			display_error(_('Please select a relationship.'));
		}

		if (!$input_error) {
			if (get_post('editing_dep_id')) {
				update_employee_dependent(
					get_post('editing_dep_id'),
					get_post('dep_name'),
					get_post('dep_relationship'),
					get_post('dep_birth_date'),
					get_post('dep_gender', 0),
					get_post('dep_national_id'),
					get_post('dep_is_beneficiary', 0)
				);
				display_notification(_('Dependent updated.'));
			} else {
				add_employee_dependent(
					$employee_id,
					get_post('dep_name'),
					get_post('dep_relationship'),
					get_post('dep_birth_date'),
					get_post('dep_gender', 0),
					get_post('dep_national_id'),
					get_post('dep_is_beneficiary', 0)
				);
				display_notification(_('Dependent added.'));
			}
			unset($_POST['dep_name'], $_POST['dep_relationship'], $_POST['dep_birth_date'],
				$_POST['dep_gender'], $_POST['dep_national_id'], $_POST['dep_is_beneficiary'],
				$_POST['editing_dep_id']);
			$Ajax->activate('_page_body');
		}
	}

	// Display existing dependents after processing so the table is current.
	display_employee_dependents($employee_id);

	// Add/Edit form
	br();
	display_heading(_('Add / Edit Dependent'));

	start_outer_table(TABLESTYLE2);

	table_section(1);

	hidden('editing_dep_id', get_post('editing_dep_id', ''));
	text_row(_('Name:'), 'dep_name', null, 42, 100);
	relationship_list_row(_('Relationship:'), 'dep_relationship');
	gender_list_row(_('Gender:'), 'dep_gender');

	table_section(2);

	date_row(_('Date of Birth:'), 'dep_birth_date', '', false, 0, 0, -80);
	text_row(_('National ID:'), 'dep_national_id', null, 42, 100);
	yesno_list_row(_('Beneficiary:'), 'dep_is_beneficiary');

	end_outer_table(1);

	submit_center('save_dependent', _('Save Dependent'), true, '', 'default');
}

//======================================================================
// TAB 7: TRANSACTIONS (Payslips, Leave, Loans)
//======================================================================
function tab_transactions($employee_id) {
	hidden('NewEmpID', get_post('NewEmpID'));
	if (empty($employee_id)) {
		display_note(_('Please save the employee first.'));
		return;
	}

	display_employee_transactions($employee_id);
}

//======================================================================
// HANDLE EMPLOYEE ID SELECTION / NAVIGATION
//======================================================================

if (isset($_GET['employee_no']))
	$_POST['employee_id'] = $_GET['employee_no'];
elseif (isset($_GET['employee_id']))
	$_POST['employee_id'] = $_GET['employee_id'];

if (isset($_GET['_tabs_sel']))
	$_POST['_tabs_sel'] = $_GET['_tabs_sel'];

$employee_id = get_post('employee_id');

if (list_updated('employee_id')) {
	// user picked from dropdown
	$employee_id = get_post('employee_id');
	
	if (empty($employee_id)) {
		// Selecting "New employee" - clear the form
		$_POST['NewEmpID'] = '';
		clear_inputs();
		$new_employee = true;
	} else {
		// Selecting an existing employee - load their data
		$_POST['NewEmpID'] = $employee_id;
		set_edit($employee_id);
		$new_employee = false;
	}
	
	$Ajax->activate('details');
	$Ajax->activate('controls');
}

if (get_post('cancel')) {
	$_POST['NewEmpID'] = '';
	$employee_id = '';
	$_POST['employee_id'] = '';
	clear_inputs();
	set_focus('NewEmpID');
	$Ajax->activate('_page_body');
}

// recompute new_employee after any of the above handlers
$new_employee = get_post('employee_id') == '' || get_post('cancel');

//======================================================================
// HANDLE IMAGE UPLOAD
//======================================================================

$upload_file = '';
if (isset($_FILES['pic']) && $_FILES['pic']['name'] != '') {
	$employee_id = $_POST['NewEmpID'];
	$result = $_FILES['pic']['error'];
	$upload_file = 'Yes';
	$filename = company_path().'/images/employees';
	if (!file_exists($filename))
		mkdir($filename, 0777, true);

	$filename .= '/'.item_img_name($employee_id).(substr(trim($_FILES['pic']['name']), strrpos($_FILES['pic']['name'], '.')));

	if ($_FILES['pic']['error'] == UPLOAD_ERR_INI_SIZE) {
		display_error(_('The file size is over the maximum allowed.'));
		$upload_file = 'No';
	}
	elseif ($_FILES['pic']['error'] > 0) {
		display_error(_('Error uploading file.'));
		$upload_file = 'No';
	}

	if ((list($width, $height, $type, $attr) = getimagesize($_FILES['pic']['tmp_name'])) !== false)
		$imagetype = $type;
	else
		$imagetype = false;

	if ($imagetype != IMAGETYPE_GIF && $imagetype != IMAGETYPE_JPEG && $imagetype != IMAGETYPE_PNG) {
		display_warning(_('Only graphics files can be uploaded.'));
		$upload_file = 'No';
	}
	elseif (!in_array(strtoupper(substr(trim($_FILES['pic']['name']), strlen($_FILES['pic']['name']) - 3)), array('JPG','PNG','GIF'))) {
		display_warning(_('Only graphics files are supported — a file extension of .jpg, .png or .gif is expected.'));
		$upload_file = 'No';
	}
	elseif ($_FILES['pic']['size'] > ($SysPrefs->max_image_size * 1024)) {
		display_warning(_('The file size is over the maximum allowed. The maximum size allowed in KB is').' '.$SysPrefs->max_image_size);
		$upload_file = 'No';
	}
	elseif ($_FILES['pic']['type'] == 'text/plain') {
		display_warning(_('Only graphics files can be uploaded.'));
		$upload_file = 'No';
	}
	elseif (!del_image($employee_id)) {
		display_error(_('The existing image could not be removed.'));
		$upload_file = 'No';
	}

	if ($upload_file == 'Yes') {
		$result = move_uploaded_file($_FILES['pic']['tmp_name'], $filename);
		if ($msg = check_image_file($filename)) {
			display_error($msg);
			unlink($filename);
			$upload_file = 'No';
		}
	}
	$Ajax->activate('details');
}

//======================================================================
// HANDLE ADD / UPDATE EMPLOYEE
//======================================================================

if (isset($_POST['addupdate'])) {
	// If updating from a non-Personal tab, only a subset of fields
	// will be posted. To avoid validation failing for required
	// personal fields, pre-fill any missing values from the DB
	// while preserving values submitted on the current tab.
	if (!empty(get_post('NewEmpID')) && !$new_employee && (empty(get_post('first_name')) || empty(get_post('last_name')))) {
		$dbrow = get_employee_by_code(get_post('NewEmpID'));
		if ($dbrow) {
			// Convert SQL dates to user format
			$date_fields = array('birth_date', 'hire_date', 'released_date', 'confirmation_date', 'probation_end_date', 'passport_expiry');
			foreach ($date_fields as $f) {
				if (!empty($dbrow[$f]))
					$dbrow[$f] = sql2date($dbrow[$f]);
				else
					$dbrow[$f] = '';
			}
			// Merge DB values first, then submitted POST values so POST overrides DB
			$_POST = array_merge($dbrow, $_POST);
		}
	}

	$input_error = 0;

	if ($upload_file == 'No')
		$input_error = 1;

	if (empty(trim(get_post('NewEmpID')))) {
		$input_error = 1;
		display_error(_('Employee ID must be entered.'));
		set_focus('NewEmpID');
	}
	elseif (empty(trim(get_post('first_name')))) {
		$input_error = 1;
		display_error(_('Employee first name must be entered.'));
		set_focus('first_name');
	}
	elseif (empty(trim(get_post('last_name')))) {
		$input_error = 1;
		display_error(_('Employee last name must be entered.'));
		set_focus('last_name');
	}
	elseif (strstr($_POST['NewEmpID'], ' ') || strstr($_POST['NewEmpID'], "'")
		|| strstr($_POST['NewEmpID'], '+') || strstr($_POST['NewEmpID'], '"')
		|| strstr($_POST['NewEmpID'], '&') || strstr($_POST['NewEmpID'], "\t")) {
		$input_error = 1;
		display_error(_('The employee ID cannot contain any of the following characters — & + OR a space OR quotes.'));
		set_focus('NewEmpID');
	}
	elseif ($new_employee && key_in_foreign_table($_POST['NewEmpID'], 'employees', 'employee_id')) {
		$input_error = 1;
		display_error(_('Duplicated employee ID found.'));
		set_focus('NewEmpID');
	}
	elseif (!check_num('dependents_no', 0)) {
		$input_error = 1;
		display_error(_('Number of dependents must be a positive integer.'));
		set_focus('dependents_no');
	}
	$email = get_post('email', '');
	if (!empty($email) && !filter_var($email, FILTER_VALIDATE_EMAIL)) {
		$input_error = 1;
		display_error(_('Work email address is invalid.'));
		set_focus('email');
	}
	$personal_email = get_post('personal_email', '');
	if (!empty($personal_email) && !filter_var($personal_email, FILTER_VALIDATE_EMAIL)) {
		$input_error = 1;
		display_error(_('Personal email address is invalid.'));
		set_focus('personal_email');
	}

	// Validate date fields
	$birth_date = get_post('birth_date', '');
	if (!empty($birth_date) && !is_date($birth_date)) {
		$input_error = 1;
		display_error(_('Birth date is not in a valid format.'));
		set_focus('birth_date');
	} elseif (!empty($birth_date) && date_comp($birth_date, Today()) > 0) {
		$input_error = 1;
		display_error(_('Birth date cannot be in the future.'));
		set_focus('birth_date');
	}

	$hire_date = get_post('hire_date', '');
	if (!empty($hire_date) && !is_date($hire_date)) {
		$input_error = 1;
		display_error(_('Hire date is not in a valid format.'));
		set_focus('hire_date');
	}

	$confirmation_date = get_post('confirmation_date', '');
	if (!empty($confirmation_date) && !is_date($confirmation_date)) {
		$input_error = 1;
		display_error(_('Confirmation date is not in a valid format.'));
		set_focus('confirmation_date');
	} elseif (!empty($confirmation_date) && !empty($hire_date) && is_date($hire_date) && date_comp($confirmation_date, $hire_date) < 0) {
		$input_error = 1;
		display_error(_('Confirmation date cannot be before hire date.'));
		set_focus('confirmation_date');
	}

	$probation_end_date = get_post('probation_end_date', '');
	if (!empty($probation_end_date) && !is_date($probation_end_date)) {
		$input_error = 1;
		display_error(_('Probation end date is not in a valid format.'));
		set_focus('probation_end_date');
	} elseif (!empty($probation_end_date) && !empty($hire_date) && is_date($hire_date) && date_comp($probation_end_date, $hire_date) < 0) {
		$input_error = 1;
		display_error(_('Probation end date cannot be before hire date.'));
		set_focus('probation_end_date');
	}

	$released_date = get_post('released_date', '');
	if (!empty($released_date) && !is_date($released_date)) {
		$input_error = 1;
		display_error(_('Release date is not in a valid format.'));
		set_focus('released_date');
	} elseif (!empty($released_date) && !empty($hire_date) && is_date($hire_date) && date_comp($released_date, $hire_date) < 0) {
		$input_error = 1;
		display_error(_('Release date cannot be before hire date.'));
		set_focus('released_date');
	}

	$passport_expiry = get_post('passport_expiry', '');
	if (!empty($passport_expiry) && !is_date($passport_expiry)) {
		$input_error = 1;
		display_error(_('Passport expiry date is not in a valid format.'));
		set_focus('passport_expiry');
	}

	if (!$input_error) {
		if (check_value('del_image'))
			del_image($_POST['NewEmpID']);

		$data = collect_employee_data();

		if (!$new_employee) {
			// Track changes for history
			$old_emp = get_employee_by_code($_POST['NewEmpID']);
			$changes_detected = false;

			if ($old_emp) {
				if ($old_emp['department_id'] != $data['department_id']
					|| $old_emp['position_id'] != $data['position_id']
					|| $old_emp['grade_id'] != $data['grade_id']) {
					$changes_detected = true;
					$change_type = HRM_HIST_TRANSFER;
					if ($old_emp['grade_id'] != $data['grade_id'])
						$change_type = HRM_HIST_GRADE_CHANGE;
				}
			}

			update_employee($_POST['NewEmpID'], $data);
			update_record_status($_POST['NewEmpID'], get_post('inactive'), 'employees', 'employee_id');

			// Record history if significant changes
			if ($changes_detected && $old_emp) {
				record_employee_history($_POST['NewEmpID'], $change_type,
					Today(),
					array(
						'department_id' => $old_emp['department_id'],
						'position_id'   => $old_emp['position_id'],
						'grade_id'      => $old_emp['grade_id'],
						'salary'        => 0,
					),
					array(
						'department_id' => $data['department_id'],
						'position_id'   => $data['position_id'],
						'grade_id'      => $data['grade_id'],
						'salary'        => 0,
					)
				);
			}

			// Fire hook
			hrm_fire_hook('on_employee_updated', $_POST['NewEmpID'], $old_emp, $data);

			set_focus('employee_id');
			$Ajax->activate('employee_id');
			display_notification(_('Employee information has been updated.'));
		}
		else {
			$data['employee_id'] = $_POST['NewEmpID'];
			$emp_number = add_employee($data);

			// Record hire history
			if (!empty($data['hire_date'])) {
				record_employee_history($_POST['NewEmpID'], HRM_HIST_HIRE, $data['hire_date'],
					array(),
					array(
						'department_id' => $data['department_id'],
						'position_id'   => $data['position_id'],
						'grade_id'      => $data['grade_id'],
						'salary'        => 0,
					)
				);
			}

			// Fire hook
			hrm_fire_hook('on_employee_created', $_POST['NewEmpID']);

			display_notification(_('A new employee has been added.'));
			$_POST['employee_id'] = '';
			$_POST['NewEmpID'] = '';
			clear_inputs();
			set_focus('NewEmpID');
		}
		$Ajax->activate('_page_body');
	}
}

//======================================================================
// HANDLE DELETE EMPLOYEE
//======================================================================

if (isset($_POST['delete'])) {
	$emp_id = get_post('NewEmpID');
	if (!employee_can_delete($emp_id)) {
		display_error(_('Cannot delete this employee — there are transactions linked to this employee.'));
	} else {
		del_image($emp_id);
		if (delete_employee($emp_id)) {
			display_notification(_('Employee has been deleted.'));
			$_POST['employee_id'] = '';
			$_POST['NewEmpID'] = '';
			clear_inputs();
			$Ajax->activate('_page_body');
		} else {
			display_error(_('Could not delete this employee.'));
		}
	}
}

//======================================================================
// PAGE RENDERING
//======================================================================

start_form(true);

// ── Employee selector ──────────────────────────────────────
if (db_has_employees()) {
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	employees_list_cells(_('Select an employee:'), 'employee_id', null, _('New employee'), true,
		check_value('show_inactive'), false, array('search_submit' => true));
	$new_employee = get_post('employee_id') == '';
	check_cells(_('Show inactive:'), 'show_inactive', null, true);
	end_row();
	end_table();

	if (get_post('_show_inactive_update')) {
		$Ajax->activate('employee_id');
		set_focus('employee_id');
	}
} else {
	hidden('employee_id', get_post('employee_id'));
}

div_start('details');

$employee_id = get_post('employee_id');
if (!$employee_id) // force personal tab for new employee
	unset($_POST['_tabs_sel']);

// ── Build tabs array ───────────────────────────────────────
$tabs = array(
	'tab_personal'     => array(_('&Personal'), $employee_id),
	'tab_employment'   => array(_('E&mployment'), $employee_id),
	'tab_salary'       => array(_('&Salary'), (user_check_access('SA_SALARYSTRUCTURE') ? $employee_id : null)),
	'tab_documents'    => array(_('&Documents'), (user_check_access('SA_EMPLOYEE') ? $employee_id : null)),
	'tab_history'      => array(_('&History'), $employee_id),
	'tab_dependents'   => array(_('De&pendents'), (user_check_access('SA_EMPLOYEE') ? $employee_id : null)),
	'tab_transactions' => array(_('&Transactions'), (user_check_access('SA_EMPLOYEETRANSVIEW') ? $employee_id : null)),
);

tabbed_content_start('tabs', $tabs);

switch (get_post('_tabs_sel')) {
	default:
	case 'tab_personal':
		tab_personal($employee_id, $new_employee);
		break;
	case 'tab_employment':
		tab_employment($employee_id, $new_employee);
		break;
	case 'tab_salary':
		tab_salary($employee_id);
		break;
	case 'tab_documents':
		tab_documents($employee_id);
		break;
	case 'tab_history':
		tab_history($employee_id);
		break;
	case 'tab_dependents':
		tab_dependents($employee_id);
		break;
	case 'tab_transactions':
		tab_transactions($employee_id);
		break;
}

// ── Submit buttons (shown on Personal & Employment tabs) ───
$current_tab = get_post('_tabs_sel', 'tab_personal');
if (in_array($current_tab, array('tab_personal', 'tab_employment', ''))) {
	br();
	div_start('controls');
	if (@$_REQUEST['popup'])
		hidden('popup', 1);
	if (!isset($_POST['NewEmpID']) || $new_employee) {
		submit_center('addupdate', _('Add New Employee'), true, '', 'default');
	} else {
		submit_center_first('addupdate', _('Update Employee'), '', @$page_nested ? true : 'default');
		submit_return('select', get_post('employee_id'), _('Select this employee and return to document entry.'));
		submit('delete', _('Delete This Employee'), true, '', true);
		submit_center_last('cancel', _('Cancel'), _('Cancel Edition'), 'cancel');
	}
	div_end();
}

if ($new_employee && $current_tab == 'tab_personal' && !get_post('addupdate') && !list_updated('employee_id') && !get_post('_show_inactive_update'))
	set_focus('NewEmpID');

br();
tabbed_content_end();

div_end();

end_form();
end_page(@$_REQUEST['popup']);
