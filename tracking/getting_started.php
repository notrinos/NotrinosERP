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
 * DIM-UX-001: three-step onboarding, synthetic demos and the disabled-history view.
 *
 * Read-only. It explains how to name what is tracked, create the first value and code an expense, using the words the
 * company declared. It writes nothing: every action it offers is a link to the page that already owns that action, shown only
 * to a user who holds that page's authority. The demos are made-up examples generated in memory.
 */
$page_security = 'SA_DIMTRANSVIEW';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

// DIM-NAM-002: the help key keeps its old value so the existing help page still opens.
$help_context = 'Getting Started with Dimensions';
page(_('Getting Started with Cost & Profit Tracking'));

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/tracking/includes/tracking_values_db.inc');
include_once($path_to_root.'/tracking/includes/tracking_category_policy.inc');
include_once($path_to_root.'/tracking/includes/tracking_business_mode.inc');
include_once($path_to_root.'/tracking/includes/tracking_values_ui.inc');

//-----------------------------------------------------------------------------------

display_note(_('Cost & Profit Tracking lets you see where money was spent or earned, for example by project or by cost center. Follow the three steps below; each one says what is missing and who can do it.'));

// DIM-UX-002: the Overview that lists every Cost & Profit Tracking page and what the user can open.
hyperlink_no_params($path_to_root.'/tracking/overview.php?', _('See every Cost & Profit Tracking page'));
// DIM-UX-003: the end-user guide to the flow and to every page.
hyperlink_no_params($path_to_root.'/tracking/user_guide.php?', _('Read the User Guide'));

$facts = dimension_onboarding_facts();
$steps = dimension_onboarding_steps($facts, dimension_business_mode_can_configure(), dimension_business_mode_can_edit());
display_dimension_onboarding_steps($steps);

$hrm_note = dimension_business_mode_hrm_note(1);
if ($hrm_note !== '' && $facts['enabled_axes'] >= 1)
	display_note($hrm_note);

display_dimension_history_only(dimension_onboarding_history_only($facts));

//-----------------------------------------------------------------------------------
// The two examples are chosen with a plain link; an unknown choice is reported, never rendered.

display_heading2(_('See an example first'));
foreach (dimension_demo_catalog() as $code => $entry)
	hyperlink_no_params($path_to_root.'/tracking/getting_started.php?demo='.$code, $entry['title'].' - '.$entry['description']);

if (isset($_GET['demo'])) {
	$dataset = dimension_demo_dataset(is_string($_GET['demo']) ? $_GET['demo'] : '');
	if ($dataset === false)
		display_dimension_failure_state('invalid_input');
	else
		display_dimension_demo($dataset);
}

end_page();
