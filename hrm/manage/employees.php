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

$page_security = 'SA_EMPLOYEE';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/db_pager.inc');
include_once($path_to_root.'/includes/session.inc');
include($path_to_root.'/reporting/includes/tcpdf.php');

$js = '';
if($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if(user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Manage Employees'), @$_REQUEST['popup'], false, '', $js);

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');

$new_employee = get_post('employee_id') == '' || get_post('cancel');

//------------------------------------------------------------------------------------------------------

function set_edit($employee_id) {
	$_POST = array_merge($_POST, get_employee_by_code($employee_id));

	$_POST['birth_date'] = empty($_POST['birth_date']) ? '' : sql2date($_POST['birth_date']);
	$_POST['hire_date'] = empty($_POST['hire_date']) ? '' : sql2date($_POST['hire_date']);
	$_POST['released_date'] = empty($_POST['released_date']) ? '' : sql2date($_POST['released_date']);
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
	$img_link = "<img id='emp_img' alt = 'No Image' src='".company_path().'/images/no_image.svg'."?nocache=".rand()."'"." height='100'>";

	if (@$employee_id) {
		foreach (array('jpg', 'png', 'gif') as $ext) {
			$file = company_path().'/images/employees/'.item_img_name($employee_id).'.'.$ext;
			if (file_exists($file)) {
				// rand() call is necessary here to avoid caching problems.
				$check_remove_image = true;
				$img_link = "<img id='emp_img' alt = '[".$employee_id.".$ext"."]' src='".$file."?nocache=".rand()."'"." height='100' border='0'>";
				break;
			}
		}
	}
	label_row("&nbsp;", $img_link);
	if ($check_remove_image)
		check_row(_('Delete Image:'), 'del_image');
}

if (isset($_GET['employee_no']))
	$_POST['employee_id'] = $_GET['employee_no'];

$employee_id = get_post('employee_id');
if (list_updated('employee_id')) {
	$_POST['NewEmpID'] = get_post('employee_id');
	$employee_id = get_post('employee_id');
	clear_inputs();
	$Ajax->activate('details');
	$Ajax->activate('controls');
}
if (get_post('cancel')) {
	$_POST['NewEmpID'] = '';
	$employee_id = '';
	$_POST['employee_id'] = '';
	clear_inputs();
	set_focus('employee_id');
	$Ajax->activate('_page_body');
}

$upload_file = '';
if (isset($_FILES['pic']) && $_FILES['pic']['name'] != '') {
	$employee_id = $_POST['NewEmpID'];
	$result = $_FILES['pic']['error'];
	$upload_file = 'Yes'; //Assume all is well to start off with
	$filename = company_path().'/images/employees';
	if (!file_exists($filename))
		mkdir($filename);
	
	$filename .= '/'.item_img_name($employee_id).(substr(trim($_FILES['pic']['name']), strrpos($_FILES['pic']['name'], '.')));

	if ($_FILES['pic']['error'] == UPLOAD_ERR_INI_SIZE) {
		display_error(_('The file size is over the maximum allowed.'));
		$upload_file = 'No';
	}
	elseif ($_FILES['pic']['error'] > 0) {
		display_error(_('Error uploading file.'));
		$upload_file = 'No';
	}
	
	//But check for the worst 
	if ((list($width, $height, $type, $attr) = getimagesize($_FILES['pic']['tmp_name'])) !== false)
		$imagetype = $type;
	else
		$imagetype = false;

	if ($imagetype != IMAGETYPE_GIF && $imagetype != IMAGETYPE_JPEG && $imagetype != IMAGETYPE_PNG) {
		display_warning( _('Only graphics files can be uploaded'));
		$upload_file = 'No';
	}
	elseif (!in_array(strtoupper(substr(trim($_FILES['pic']['name']), strlen($_FILES['pic']['name']) - 3)), array('JPG','PNG','GIF'))) {
		display_warning(_('Only graphics files are supported - a file extension of .jpg, .png or .gif is expected'));
		$upload_file = 'No';
	} 
	elseif ( $_FILES['pic']['size'] > ($SysPrefs->max_image_size * 1024)) { //File Size Check
		display_warning(_('The file size is over the maximum allowed. The maximum size allowed in KB is').' '.$SysPrefs->max_image_size);
		$upload_file = 'No';
	} 
	elseif ( $_FILES['pic']['type'] == 'text/plain' ) {  //File type Check
		display_warning( _('Only graphics files can be uploaded'));
		$upload_file = 'No';
	} 
	elseif (!del_image($employee_id)) {
		display_error(_('The existing image could not be removed'));
		$upload_file = 'No';
	}

	if ($upload_file == 'Yes') {
		$result  =  move_uploaded_file($_FILES['pic']['tmp_name'], $filename);
		if ($msg = check_image_file($filename)) {
			display_error($msg);
			unlink($filename);
			$upload_file = 'No';
		}
	}
	$Ajax->activate('details');
}

function clear_inputs() {
	unset($_POST['first_name']);
	unset($_POST['last_name']);
	unset($_POST['address']);
	unset($_POST['mobile']);
	unset($_POST['email']);
	unset($_POST['birth_date']);
	unset($_POST['national_id']);
	unset($_POST['passport']);
	unset($_POST['bank_account']);
	unset($_POST['tax_number']);
	unset($_POST['notes']);
	unset($_POST['NewEmpID']);
	unset($_POST['department_id']);
	unset($_POST['position_id']);
	unset($_POST['grade_id']);
	unset($_POST['released_date']);
	unset($_POST['user_id']);
}

function employee_settings($employee_id, $new_employee) {
	global $path_to_root, $SysPrefs, $page_nested;

	start_outer_table(TABLESTYLE2);

	table_section(1);

	table_section_title(_('Work Information'));

	file_row(_('Image File (.jpg)').':', 'pic', 'pic');

	show_image(@$_POST['NewEmpID']);

	if($new_employee) {
		text_row(_('Employee ID:'), 'NewEmpID', null, 31, 20);
		$_POST['inactive'] = 0;
	}
	else { // must be modifying an existing employee
		// first employee display
		if (get_post('NewEmpID') != get_post('employee_id') || get_post('addupdate')) {

			$_POST['NewEmpID'] = $_POST['employee_id'];
			set_edit($_POST['employee_id']);
		}
		label_row(_('Employee ID:'), $_POST['NewEmpID']);
		hidden('NewEmpID', $_POST['NewEmpID']);
		set_focus('first_name');
	}

	table_section_title(_('Personal Information'));

	text_row(_('First Name:'), 'first_name', null, 31, 100);
	text_row(_('Last Name:'), 'last_name', null, 31, 100);
	textarea_row(_('Address:'), 'address', null, 27, 5);
	
	start_row();
	echo "<td class='label'>"._('Gender:')."</td><td>";
	echo radio(_('Male'), 'gender', 1, true);
	echo radio(_('Female'), 'gender', 0);
	echo radio(_('Other'), 'gender', 2).'</td>';
	end_row();

	date_row(_('Birth Date:'), 'birth_date', null, null, 0, 0, -22);
	martial_status_list_row(_('Martial Status:'), 'martial_status');
	qty_row(_('Number of Dependents:'), 'dependents_no', null, null, null, 0);

	table_section(2);

	table_section_title(_('Work Information'));

	text_row(_('Mobile:'), 'mobile', null, 31, 30);
	email_row(_('e-Mail:'), 'email', get_post('email'), 31, 100);
	text_row(_('National ID:'), 'national_id', get_post('national_id'), 31, 100);
	text_row(_('Passport:'), 'passport', get_post('passport'), 31, 100);
	text_row(_('Bank Name/Account:'), 'bank_account', get_post('bank_account'), 31, 100);
	text_row(_('Tax ID:'), 'tax_number', get_post('tax_number'), 31, 100);
	textarea_row(_('Notes:'), 'notes', null, 27, 5);
	date_row(_('Hire Date:'), 'hire_date', null, null, 0, 0, 1001);
	users_list_row(_('Related user:'), 'user_id', null, false, _('Select user'));
	departments_list_row(_('Department:'), 'department_id', null, false, _('Select department'));
	positions_list_row(_('Job Posittion:'), 'position_id', null, false, _('Select job position'));
	grades_list_row(_('Salary Grade:'), 'grade_id', null, _('Basic'));
	yesno_list_row(_('Use Personal Salary Structure:'), 'personal_salary');
	date_row(_('Release Date:'), 'released_date', null, null, 0, 0, 1001);
	record_status_list_row(_('Employee status:'), 'inactive');

	end_outer_table(1);

	div_start('controls');
	if (@$_REQUEST['popup'])
		hidden('popup', 1);
	if (!isset($_POST['NewEmpID']) || $new_employee)
		submit_center('addupdate', _('Add New Employee'), true, '', 'default');
	else {
		submit_center_first('addupdate', _('Update Employee'), '', $page_nested ? true : 'default');
		submit_return('select', get_post('employee_id'), _('Select this employee and return to document entry.'));
		submit('delete', _('Delete This Employee'), true, '', true);
		submit_center_last('cancel', _('Cancel'), _('Cancel Edition'), 'cancel');
	}

	div_end();
}

if (isset($_POST['addupdate'])) {

	$input_error = 0;
	if ($upload_file == 'No')
		$input_error = 1;
	if(empty(trim($_POST['NewEmpID']))) {
		$input_error = 1;
		display_error( _('Employee ID must be entered.'));
		set_focus('NewEmpID');
	}
	elseif(empty(trim($_POST['first_name']))) {
		$input_error = 1;
		display_error( _('Employee first name must be entered.'));
		set_focus('first_name');
	}
	elseif(empty(trim($_POST['last_name']))) {
		$input_error = 1;
		display_error( _('Employee last name must be entered.'));
		set_focus('last_name');
	} 
	elseif (strstr($_POST['NewEmpID'], ' ') || strstr($_POST['NewEmpID'],"'") || strstr($_POST['NewEmpID'], '+') || strstr($_POST['NewEmpID'], "\"") || strstr($_POST['NewEmpID'], '&') || strstr($_POST['NewEmpID'], "\t")) {
		$input_error = 1;
		display_error( _('The employee ID cannot contain any of the following characters -  & + OR a space OR quotes'));
		set_focus('NewEmpID');
	}
	elseif ($new_employee && key_in_foreign_table($_POST['NewEmpID'], 'employees', 'employee_id')) {
		$input_error = 1;
		display_error( _('Duplicated employee ID found.'));
		set_focus('NewEmpID');
	}
	
	if ($input_error != 1) {
		if (check_value('del_image'))
			del_image($_POST['NewEmpID']);
		
		if (!$new_employee) {
			update_employee($_POST['NewEmpID'], $_POST['first_name'], $_POST['last_name'], $_POST['gender'], $_POST['birth_date'], $_POST['address'], $_POST['mobile'], $_POST['email'], $_POST['national_id'], $_POST['passport'], $_POST['bank_account'], $_POST['tax_number'], $_POST['martial_status'], input_num('dependents_no'), $_POST['notes'], $_POST['hire_date'], $_POST['department_id'], $_POST['position_id'], $_POST['grade_id'], $_POST['personal_salary'], $_POST['released_date'], $_POST['user_id']);

			update_record_status($_POST['NewEmpID'], $_POST['inactive'], 'employees', 'employee_id');
			set_focus('employee_id');
			$Ajax->activate('employee_id'); // in case of status change
			display_notification(_('Employee information has been updated.'));
		} 
		else {
			add_employee($_POST['NewEmpID'], $_POST['first_name'], $_POST['last_name'], $_POST['gender'], $_POST['birth_date'], $_POST['address'], $_POST['mobile'], $_POST['email'], $_POST['national_id'], $_POST['passport'], $_POST['bank_account'], $_POST['tax_number'], $_POST['martial_status'], input_num('dependents_no'), $_POST['notes'], $_POST['hire_date'], $_POST['department_id'], $_POST['position_id'], $_POST['grade_id'], $_POST['personal_salary'], $_POST['user_id']);

			display_notification(_('A new employee has been added.'));
			$_POST['employee_id'] = '';
			$_POST['NewEmpID'] = '';
			$_POST['first_name'] = '';
			$_POST['last_name'] = '';
			$_POST['address'] = '';
			$_POST['grade_id'] = 0;
			$_POST['position_id'] = 0;
			$_POST['department_id'] = 0;
			$_POST['user_id'] = 0;
			set_focus('NewEmpID');
		}
		$Ajax->activate('_page_body');
	}
}

start_form(true);

if (db_has_employees()) {
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	employees_list_cells(_('Select an employee:'), 'employee_id', null, _('New employee'), true, check_value('show_inactive'), false, array('search_submit'=>true));
	$new_employee = get_post('employee_id') == '';
	check_cells(_('Show inactive:'), 'show_inactive', null, true);
	end_row();
	end_table();

	if (get_post('_show_inactive_update')) {
		$Ajax->activate('employee_id');
		set_focus('employee_id');
	}
}
else
	hidden('employee_id', get_post('employee_id'));

div_start('details');

$employee_id = get_post('employee_id');
if (!$employee_id)// force settings tab for new employee
	unset($_POST['_tabs_sel']);

$tabs = array(
		'settings' => array(_('Employee &Information'), $employee_id),
		'payroll_info' => array(_('&Payroll Settings'), (user_check_access('SA_STANDARDCOST') ? $employee_id : null)),
		'employee_trans' => array(_('&Transactions'), (user_check_access('SA_ITEMSTRANSVIEW') ? $employee_id : null))
	);

tabbed_content_start('tabs', $tabs);

switch (get_post('_tabs_sel')) {
	default:
	case 'settings':
		employee_settings($employee_id, $new_employee);
		break;
	case 'payroll_settings':
		break;
	case 'transactions':
		break;

}

br();
tabbed_content_end();

div_end();

end_form();
end_page(@$_REQUEST['popup']);
