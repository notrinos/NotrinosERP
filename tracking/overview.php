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
 * DIM-UX-002: the Cost & Profit Tracking Overview (formerly Financial Tracking Home), the one page that lists every page of the module.
 *
 * Read-only. It writes nothing and reads no company figure: it lists the pages of the Cost & Profit Tracking and General Ledger menus
 * that belong to the module, grouped by task, and tells each user which ones that user can open and what a page still needs.
 * A link is drawn only for a page whose own security area the user holds; the target page repeats its own check.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

// DIM-NAM-002: the help key keeps its old value so the existing help page still opens.
$help_context = 'Financial Tracking Home';
page(_('Cost & Profit Tracking Overview'));

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_hub.inc');

//-----------------------------------------------------------------------------------

display_note(_('Cost & Profit Tracking shows where money was spent or earned, by department, project, customer or any other category your company tracks. Its pages are in the Cost & Profit Tracking and General Ledger menus; this page lists them all by task, so you can start here.'));
display_note(dimension_term_former_name_notice().' '.dimension_term_also_known_as());

hyperlink_no_params($path_to_root.'/tracking/user_guide.php?', _('Read the User Guide'));

display_dimension_hub(dimension_hub_view('user_check_access', dimension_hub_enabled_axes()));

end_page();
