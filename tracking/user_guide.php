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
 * DIM-UX-003: the end-user guide (User Guide) to Cost & Profit Tracking.
 *
 * Read-only. It writes nothing and reads no company figure: it explains the Getting Started flow and each page of the module,
 * and draws a link only to a page the signed-in user can open, using the same view decision as the Overview page.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

// DIM-NAM-002: the help key keeps its old value so the existing help page still opens.
$help_context = 'Financial Tracking Guide';
page(_('Cost & Profit Tracking User Guide'));

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_hub.inc');
include_once($path_to_root.'/tracking/includes/tracking_guide.inc');

//-----------------------------------------------------------------------------------

display_note(_('A short manual for Cost & Profit Tracking: the three steps to start and what each page is for. To see which pages you can open, use the Overview page.'));
display_note(dimension_term_former_name_notice().' '.dimension_term_also_known_as());
hyperlink_no_params($path_to_root.'/tracking/overview.php?', _('See every Cost & Profit Tracking page'));

$view = dimension_hub_view('user_check_access', dimension_hub_enabled_axes());
display_dimension_guide(dimension_guide_catalog(), dimension_guide_page_states($view));

end_page();
