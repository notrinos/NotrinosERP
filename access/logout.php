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

define('FA_LOGOUT_PHP_FILE', '');

$page_security = 'SA_OPEN';
$path_to_root = '..';

include($path_to_root.'/includes/session.inc');
add_js_file('login.js');
include($path_to_root.'/includes/page/header.inc');

page_header(_('Logout'), true, false, '');

echo "<div class='modern-logout-card'>";
echo "<img src='".$path_to_root."/themes/default/images/notrinos_erp.png' alt='NotrinosERP'>";
echo "<div class='logout-message'>"._('Thank you for using')."</div>";
echo "<div class='logout-app'>".$SysPrefs->app_title.' '.$version."</div>";
echo "<a class='logout-login-link' href='".$path_to_root."/index.php'>"._('Click here to Login Again.')."</a>";
echo "</div>\n";

end_page(false, true);
session_unset();
@session_destroy();
