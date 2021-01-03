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
$path_to_root="..";

include($path_to_root . "/includes/session.inc");

page(_($help_context = "System Diagnostics"));

include($path_to_root . "/includes/ui.inc");
include($path_to_root . "/includes/system_tests.inc");
//-------------------------------------------------------------------------------------------------

display_system_tests();

end_page();

