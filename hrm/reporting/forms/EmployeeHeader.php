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

$path_to_root = "../../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
page(_("Employee Report Header"));

start_table(TABLESTYLE2, "width='60%'");
label_row(_('Header Type:'), _('Employee Report'));
label_row(_('Department (PARAM_0):'), get_post('PARAM_0', ''));
label_row(_('Position (PARAM_1):'), get_post('PARAM_1', ''));
label_row(_('Grade (PARAM_2):'), get_post('PARAM_2', ''));
label_row(_('Status (PARAM_3):'), get_post('PARAM_3', ''));
label_row(_('Comments (PARAM_4):'), get_post('PARAM_4', ''));
end_table(1);

display_note(_('This page previews parameter values passed to employee reports.'), 0, 1);

end_page();

