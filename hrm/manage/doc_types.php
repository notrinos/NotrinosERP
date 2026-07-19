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
$page_security = 'SA_DOCTYPE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/doc_types_entity.inc');

page(_("Document Types"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$security_classes = array(
		'restricted' => _('Restricted HR'),
		'identity' => _('Identity'),
		'work_authorization' => _('Work Authorization'),
		'medical' => _('Medical'),
		'payroll' => _('Payroll Evidence'),
		'case' => _('Case / Investigation'),
	);
    if (trim($_POST['type_name']) == '') {
        display_error(_('Document type name is required.'));
        set_focus('type_name');
    } elseif (!isset($security_classes[get_post('security_class')])) {
		display_error(_('A valid document security class is required.'));
		set_focus('security_class');
	} else {
        $data = array(
            'type_name'     => $_POST['type_name'],
			'security_class' => get_post('security_class'),
            'notify_before' => (int)$_POST['notify_before'],
            'is_required'   => check_value('is_required') ? 1 : 0,
        );
        if ($selected_id != '') {
            doc_types_entity::modify($selected_id, $data);
            display_notification(_('Document type has been updated.'));
        } else {
            doc_types_entity::create($data);
            display_notification(_('Document type has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (key_in_foreign_table($selected_id, 'employee_documents', 'doc_type_id'))
        display_error(_('Document type cannot be deleted because employee documents already use it.'));
    else {
        doc_types_entity::remove($selected_id);
        display_notification(_('Selected document type has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['type_name'] = '';
    $_POST['notify_before'] = 30;
    $_POST['is_required'] = 0;
	$_POST['security_class'] = 'restricted';
}

$security_classes = array(
	'restricted' => _('Restricted HR'),
	'identity' => _('Identity'),
	'work_authorization' => _('Work Authorization'),
	'medical' => _('Medical'),
	'payroll' => _('Payroll Evidence'),
	'case' => _('Case / Investigation'),
);

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Document Type'), _('Security Class'), _('Notify Before (days)'), _('Required'), '', '');
table_header($th);

$result = doc_types_entity::all_db_resource();
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['doc_type_id']);
    label_cell($row['type_name']);
	label_cell(isset($security_classes[$row['security_class']]) ? $security_classes[$row['security_class']] : _('Restricted HR'));
    label_cell($row['notify_before']);
    label_cell(!empty($row['is_required']) ? _('Yes') : _('No'));
    edit_button_cell('Edit'.$row['doc_type_id'], _('Edit'));
    delete_button_cell('Delete'.$row['doc_type_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = doc_types_entity::find($selected_id);
    $_POST['type_name'] = $myrow['type_name'];
    $_POST['notify_before'] = (int)$myrow['notify_before'];
    $_POST['is_required'] = (int)$myrow['is_required'];
	$_POST['security_class'] = $myrow['security_class'];
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Document Type Name:'), 'type_name', 40, 100);
array_selector_row(_('Security Class:'), 'security_class', get_post('security_class', 'restricted'), $security_classes);
small_amount_row(_('Notify Before (days):'), 'notify_before', get_post('notify_before', 30), 0, 3650);
check_row(_('Required by default:'), 'is_required');

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();

end_page();
