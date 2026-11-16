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
 * DIM-UX-002: Financial Tracking Home, the one page that lists every Financial Tracking page.
 *
 * Read-only. It writes nothing and reads no company figure: it lists the pages of the Dimensions and General Ledger menus that
 * belong to Financial Tracking, grouped by task, and tells each user which ones that user can open and what a page still needs.
 * A link is drawn only for a page whose own security area the user holds; the target page repeats its own check.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

page(_($help_context = 'Financial Tracking Home'));

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/dimensions/includes/dimension_hub.inc');

//-----------------------------------------------------------------------------------

display_note(_('Financial Tracking shows where money was spent or earned, by Dimension. Its pages are in the Dimensions and General Ledger menus; this page lists them all by task, so you can start here.'));

hyperlink_no_params($path_to_root.'/dimensions/financial_tracking_guide.php?', _('Read the Financial Tracking Guide'));

display_dimension_hub(dimension_hub_view('user_check_access', dimension_hub_enabled_axes()));

end_page();
