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
 * CRM Appointment Entry - Create / Edit Appointment
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_APPOINTMENT';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_appointments_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

//--------------------------------------------------------------------------
// Determine mode
//--------------------------------------------------------------------------

$appointment_id = 0;
$is_new         = true;
$appointment    = null;

$raw_id = isset($_GET['AppointmentID']) ? $_GET['AppointmentID'] : get_post('AppointmentID', 0);
if ((int)$raw_id > 0) {
    $appointment_id = (int)$raw_id;
    $appointment = get_crm_appointment($appointment_id);
    if (!$appointment) {
        display_error(_('Appointment not found.'));
        hyperlink_params($path_to_root . '/crm/manage/appointments.php', _('Back to Appointments'), 'sel_app=crm');
        end_page();
        exit;
    }
    $is_new = false;
}

// Pre-fill from GET params (e.g., linked from a lead page)
if ($is_new) {
    if (isset($_GET['entity_type'])) $_POST['entity_type'] = $_GET['entity_type'];
    if (isset($_GET['entity_id']))   $_POST['entity_id']   = $_GET['entity_id'];
}

$page_title = $is_new ? _('New Appointment') : sprintf(_('Edit Appointment #%d'), $appointment_id);
page(_($help_context = 'CRM Appointment Entry'), false, false, '', $js);

//--------------------------------------------------------------------------
// Handle Save
//--------------------------------------------------------------------------

if (isset($_POST['Save'])) {
    $input_error = 0;

    if (strlen(trim($_POST['title'])) < 1) {
        display_error(_('Appointment title is required.'));
        set_focus('title');
        $input_error = 1;
    }
    if (!is_date($_POST['appointment_date'])) {
        display_error(_('Appointment date is required.'));
        $input_error = 1;
    }

    if ($input_error == 0) {
        // Combine date and time for date_time column
        $date_time = date2sql($_POST['appointment_date']) . ' ' . $_POST['start_time'] . ':00';

        // Calculate duration from start/end time
        $duration = 60; // default
        if (!empty($_POST['start_time']) && !empty($_POST['end_time'])) {
            $start_ts = strtotime('2000-01-01 ' . $_POST['start_time']);
            $end_ts   = strtotime('2000-01-01 ' . $_POST['end_time']);
            if ($end_ts > $start_ts) {
                $duration = ($end_ts - $start_ts) / 60;
            }
        }

        $data = array(
            'title'               => $_POST['title'],
            'appointment_type_id' => (int)$_POST['appt_type_id'],
            'lead_id'             => $_POST['entity_type'] === 'lead' ? (int)$_POST['entity_id'] : null,
            'customer_id'         => $_POST['entity_type'] === 'customer' ? (int)$_POST['entity_id'] : null,
            'date_time'           => $date_time,
            'duration_minutes'    => $duration,
            'location'            => $_POST['location'],
            'video_link'          => $_POST['video_link'],
            'status'              => $_POST['status'],
            'notes'               => $_POST['notes'],
        );

        begin_transaction();
        if ($is_new) {
            $data['created_by'] = $_SESSION['wa_current_user']->user;
            $appointment_id = add_crm_appointment($data);
            display_notification(_('Appointment has been created.'));
        } else {
            update_crm_appointment($appointment_id, $data);
            display_notification(_('Appointment has been updated.'));
        }
        commit_transaction();

        if ($is_new) {
            meta_forward($_SERVER['PHP_SELF'], 'AppointmentID=' . $appointment_id . crm_sel_app_param());
        }
        $appointment = get_crm_appointment($appointment_id);
    }
}

//--------------------------------------------------------------------------
// Load data into POST
//--------------------------------------------------------------------------

if (!$is_new && !isset($_POST['Save'])) {
    $_POST['title']            = $appointment['title'];
    $_POST['appt_type_id']     = $appointment['appointment_type_id'];
    // Derive entity_type/entity_id from lead_id/customer_id
    if (!empty($appointment['lead_id'])) {
        $_POST['entity_type'] = CRM_ENTITY_LEAD;
        $_POST['entity_id']   = $appointment['lead_id'];
    } elseif (!empty($appointment['customer_id'])) {
        $_POST['entity_type'] = CRM_ENTITY_CUSTOMER;
        $_POST['entity_id']   = $appointment['customer_id'];
    } else {
        $_POST['entity_type'] = '';
        $_POST['entity_id']   = 0;
    }
    // Parse date_time into separate date and time parts
    $_POST['appointment_date'] = sql2date(substr($appointment['date_time'], 0, 10));
    $_POST['start_time']       = substr($appointment['date_time'], 11, 5);
    // Compute end_time from start_time + duration_minutes
    $start_ts = strtotime('2000-01-01 ' . substr($appointment['date_time'], 11, 5));
    $end_ts   = $start_ts + ((int)$appointment['duration_minutes'] * 60);
    $_POST['end_time']         = date('H:i', $end_ts);
    $_POST['location']         = $appointment['location'];
    $_POST['video_link']       = $appointment['video_link'];
    $_POST['status']           = $appointment['status'];
    $_POST['notes']            = $appointment['notes'];
}

//--------------------------------------------------------------------------
// Display Form
//--------------------------------------------------------------------------

start_form();

if (!$is_new) {
    hidden('AppointmentID', $appointment_id);
}

display_heading(_('Appointment Details'));
start_table(TABLESTYLE2);

text_row(_('Title:'), 'title', null, 60, 200);
crm_appointment_type_list_row(_('Type:'), 'appt_type_id', null);

date_row(_('Date:'), 'appointment_date');

// Time inputs
$start_time = get_post('start_time', '09:00');
$end_time   = get_post('end_time', '09:30');
label_row(_('Start Time:'), "<input type='time' name='start_time' value='" . htmlspecialchars($start_time) . "'>");
label_row(_('End Time:'), "<input type='time' name='end_time' value='" . htmlspecialchars($end_time) . "'>");

text_row(_('Location:'), 'location', null, 60, 200);
text_row(_('Video/Meeting Link:'), 'video_link', null, 60, 500);

$statuses = array(
    CRM_APPT_SCHEDULED   => _('Scheduled'),
    CRM_APPT_CONFIRMED   => _('Confirmed'),
    CRM_APPT_COMPLETED   => _('Completed'),
    CRM_APPT_CANCELLED   => _('Cancelled'),
    CRM_APPT_RESCHEDULED => _('Rescheduled'),
    CRM_APPT_NO_SHOW     => _('No Show'),
);
array_selector_row(_('Status:'), 'status', null, $statuses);

// Assigned To
$users_sql = "SELECT id, real_name FROM " . TB_PREF . "users ORDER BY real_name";
$users_result = db_query($users_sql);
$user_options = array(0 => _('-- Unassigned --'));
while ($u = db_fetch($users_result)) {
    $user_options[$u['id']] = $u['real_name'];
}
array_selector_row(_('Assigned To:'), 'assigned_to', null, $user_options);

end_table(1);

// -- Link to Entity (Lead/Customer) --------------------------------------
display_heading(_('Linked Entity'));
start_table(TABLESTYLE2);

$entity_types = array(
    ''              => _('-- None --'),
    CRM_ENTITY_LEAD     => _('Lead'),
    CRM_ENTITY_CUSTOMER => _('Customer'),
);
array_selector_row(_('Entity Type:'), 'entity_type', null, $entity_types);

$entity_id_val = get_post('entity_id', 0);
text_row(_('Entity ID:'), 'entity_id', $entity_id_val, 10, 10);

end_table(1);

// -- Notes ---------------------------------------------------------------
display_heading(_('Notes'));
start_table(TABLESTYLE2);

textarea_row(_('Notes:'), 'notes', null, 60, 4);

end_table(1);

echo "<center>";
submit('Save', $is_new ? _('Create Appointment') : _('Update Appointment'), true, '', 'default');
echo "</center><br>";

end_form();
end_page();

