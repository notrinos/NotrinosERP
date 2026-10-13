<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
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

page(_($help_context = 'Log View'), true);

include_once($path_to_root.'/includes/ui.inc');

// Software upgrade access is site-admin only; IDs must name a configured company.
if (!isset($_GET['id']) || !is_string($_GET['id'])
	|| !preg_match('/\A(?:0|[1-9][0-9]*)\z/', $_GET['id'])
	|| !isset($db_connections[$_GET['id']])) {
	/*Script was not passed the correct parameters */
	display_note(_('The script must be called with a valid company number.'));
	end_page(true);
	exit;
}

$company_id = (int)$_GET['id'];
$log_path = VARLOG_PATH.'/upgrade.'.$company_id.'.log';
$log = is_file($log_path) && is_readable($log_path) ? @file_get_contents($log_path) : false;
if ($log === false) {
	display_note(_('No readable upgrade log is available for this company.'));
	end_page(true);
	exit;
}

display_heading(sprintf(_("Upgrade log for company '%s'"), $company_id));
br();
start_table();
start_row();

$log = strtr($log, array('Fatal error' => 'Fatal  error')); // prevent misinterpretation in output_handler
label_cells(null, nl2br(html_specials_encode($log)));
end_row();
end_table(1);

end_page(true);
