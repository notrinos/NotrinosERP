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
$page_security = 'SA_ATTENDANCE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

$js = '';
if ($SysPrefs->use_popup_windows)
    $js .= get_js_open_window(900, 500);
if (user_use_date_picker())
    $js .= get_js_date_picker();

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');
include_once($path_to_root.'/hrm/includes/hrm_ui.inc');

/**
 * Get attendance mode options for sheet view.
 *
 * @return array<int,string>
 */
function hrm_attendance_sheet_modes() {
    return array(
        0 => _('Regular Hours / Leave'),
        1 => _('Overtime Hours / Leave')
    );
}

/**
 * Build list of SQL date strings between two UI dates.
 *
 * @param string $from_date From date in user format.
 * @param string $to_date To date in user format.
 * @return array<int,string>
 */
function hrm_attendance_sheet_dates($from_date, $to_date) {
    $dates = array();
    $from = DateTime::createFromFormat('Y-m-d', date2sql($from_date));
    $to = DateTime::createFromFormat('Y-m-d', date2sql($to_date));

    if (!$from || !$to)
        return $dates;

    $to->modify('+1 day');
    $period = new DatePeriod($from, DateInterval::createFromDateString('1 day'), $to);

    foreach ($period as $dt)
        $dates[] = $dt->format('Y-m-d');

    return $dates;
}

/**
 * Validate attendance sheet filter payload.
 *
 * @return bool
 */
function hrm_attendance_sheet_can_process() {
    if (!is_date($_POST['from_date'])) {
        display_error(_('From Date is invalid.'));
        set_focus('from_date');
        return false;
    }

    if (!is_date($_POST['to_date'])) {
        display_error(_('To Date is invalid.'));
        set_focus('to_date');
        return false;
    }

    if (date_comp($_POST['from_date'], $_POST['to_date']) > 0) {
        display_error(_('From Date cannot be greater than To Date.'));
        set_focus('from_date');
        return false;
    }

    return true;
}

page(_($help_context = 'Attendance Sheet'), false, false, '', $js);

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());
if (!isset($_POST['department_id']))
    $_POST['department_id'] = 0;
if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['mode']))
    $_POST['mode'] = 0;

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From Date:'), 'from_date');
date_cells(_('To Date:'), 'to_date');
departments_list_cells(_('Department:'), 'department_id', get_post('department_id'), _('All departments'), true);
employees_list_cells(_('Employee:'), 'employee_id', get_post('employee_id'), _('All employees'), true, false);
filter_cell_open(null);
echo array_selector('mode', get_post('mode'), hrm_attendance_sheet_modes(), array('select_submit' => true));
filter_cell_close();
submit_cells('show_sheet', _('Show'), '', '', false);
end_row();
end_table(1);

if (isset($_POST['show_sheet']) || isset($_POST['department_id']) || isset($_POST['employee_id']) || isset($_POST['mode'])) {
    if (hrm_attendance_sheet_can_process()) {
        $dates = hrm_attendance_sheet_dates($_POST['from_date'], $_POST['to_date']);

        if (count($dates) > 62) {
            display_error(_('Date range is too large. Please select 62 days or less.'));
        } elseif (empty($dates)) {
            display_error(_('No dates found in selected range.'));
        } else {
            $sql = get_attendance(
                $_POST['from_date'],
                $_POST['to_date'],
                get_post('employee_id'),
                (int)get_post('department_id'),
                (int)get_post('mode')
            );

            $result = db_query($sql, 'could not get attendance sheet data');

            start_table(TABLESTYLE, "width='99%'");
            $th = array(_('Employee ID'), _('Employee'));
            foreach ($dates as $d)
                $th[] = date('j', strtotime($d));
            table_header($th);

            $k = 0;
            $rows = 0;
            while ($row = db_fetch_assoc($result)) {
                alt_table_row_color($k);
                label_cell($row['employee_id']);
                label_cell($row['employee_name']);

                foreach ($dates as $d) {
                    $value = isset($row[$d]) ? $row[$d] : '';
                    $value = ($value === null || $value === '') ? '-' : $value;
                    label_cell($value, "align='center'");
                }

                end_row();
                $rows++;
            }

            end_table();

            if ($rows == 0)
                display_notification(_('No attendance found for selected filters.'));
        }
    }
}

end_form();
end_page();

