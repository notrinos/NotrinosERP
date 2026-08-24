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
$page_security = 'SA_SOFTWAREUPGRADE';
$path_to_root = '../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/packages.inc');
include_once($path_to_root.'/admin/includes/upgrade_log.inc');

page(_($help_context = 'Log View'), true);

include_once($path_to_root.'/includes/ui.inc');

$company_id = resolve_upgrade_log_company_id(isset($_GET['id']) ? $_GET['id'] : null, $db_connections);
if ($company_id === false) {
	display_note(_('The script must be called with a valid company number.'));
	end_page();
	exit;
}

display_heading(sprintf(_("Upgrade log for company '%s'"), $company_id));
br();
start_table();
start_row();

$log_file = VARLOG_PATH.'/upgrade.'.$company_id.'.log';
$log = is_readable($log_file) ? @file_get_contents($log_file) : false;
if ($log === false)
	label_cells(null, _('The requested upgrade log is not available.'));
else {
	$log = strtr($log, array('Fatal error' => 'Fatal  error')); // prevent misinterpretation in output_handler
	label_cells(null, nl2br(html_specials_encode($log)));
}
end_row();
end_table(1);

end_page(true);
