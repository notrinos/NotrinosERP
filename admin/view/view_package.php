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
$page_security = 'SA_OPEN';
$path_to_root = "../..";
include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/packages.inc");

page(_($help_context = "Package Details"), true);

include_once($path_to_root . "/includes/ui.inc");

if (!isset($_GET['id'])) 
{
	/*Script was not passed the correct parameters */
	display_note(_("The script must be called with a valid package id to review the info for."));
	end_page();
}

$filter = array(
	'Version' => _('Available version'), 
	'Type' => _('Package type'), 
	'Name' => _('Package content'),
	'Description' => _('Description'), 
	'Author' => _('Author'), 
	'Homepage' => _('Home page'),
	'Maintenance' => _('Package maintainer'),
	'InstallPath' => _('Installation path'),
	'Depends' => _('Minimal software versions'),
	'RTLDir' => _('Right to left'),
	'Encoding' => _('Charset encoding')
);

$pkg = get_package_info($_GET['id'], null, $filter);

display_heading(sprintf(_("Content information for package '%s'"), $_GET['id']));
br();
start_table(TABLESTYLE2, "width='80%'");
$th = array(_("Property"), _("Value"));
table_header($th);

foreach ($pkg as $field => $value) {
	if ($value == '')
		continue;
	start_row();
	label_cells($field, nl2br(html_specials_encode(is_array($value) ? implode("\n", $value) :$value)),
		 "class='tableheader2'");
	end_row();
}
end_table(1);

end_page(true);
