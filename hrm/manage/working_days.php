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
$page_security = 'SA_WORKINGDAYS';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/working_days_db.inc');
page(_("Working Days"));

if (isset($_POST['save'])) {
    $rules = array();
    for ($day = 0; $day <= 6; $day++) {
        $rules[$day] = array(
            'is_working' => check_value('is_working_' . $day) ? 1 : 0,
            'work_hours' => input_num('work_hours_' . $day)
        );
    }
    save_working_days($rules);
    display_notification(_('Working days configuration has been saved.'));
}

$day_labels = array(
    0 => _('Sunday'),
    1 => _('Monday'),
    2 => _('Tuesday'),
    3 => _('Wednesday'),
    4 => _('Thursday'),
    5 => _('Friday'),
    6 => _('Saturday')
);

$current = array();
$result = get_working_days();
while ($row = db_fetch($result))
    $current[(int)$row['day_of_week']] = $row;

start_form();
start_table(TABLESTYLE, "width='60%'");
table_header(array(_('Day'), _('Is Working'), _('Work Hours')));

$k = 0;
for ($day = 0; $day <= 6; $day++) {
    $row = isset($current[$day]) ? $current[$day] : array('is_working' => 0, 'work_hours' => 0);
    alt_table_row_color($k);
    label_cell($day_labels[$day]);
    check_cells('', 'is_working_' . $day, $row['is_working']);
    text_cells('', 'work_hours_' . $day, qty_format($row['work_hours']), 8, 6);
    end_row();
}
end_table(1);
submit_center('save', _('Save Working Days'));
end_form();

end_page();

