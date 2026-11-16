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
 * DIM-UX-003: the end-user guide to Financial Tracking.
 *
 * Read-only. It writes nothing and reads no company figure: it explains the Getting Started flow and each Financial Tracking page,
 * and draws a link only to a page the signed-in user can open, using the same view decision as the Financial Tracking Home page.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

page(_($help_context = 'Financial Tracking Guide'));

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/dimensions/includes/dimension_hub.inc');
include_once($path_to_root.'/dimensions/includes/dimension_guide.inc');

//-----------------------------------------------------------------------------------

display_note(_('A short manual for Financial Tracking: the three steps to start and what each page is for. To see which pages you can open, use the Financial Tracking Home page.'));
hyperlink_no_params($path_to_root.'/dimensions/financial_tracking_home.php?', _('See every Financial Tracking page'));

$view = dimension_hub_view('user_check_access', dimension_hub_enabled_axes());
display_dimension_guide(dimension_guide_catalog(), dimension_guide_page_states($view));

end_page();
