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
include_once(dirname(__DIR__).'/tracking/includes/tracking_terms.inc');

class DimensionsApp extends application {
	function __construct() {
		$dim = (int)get_company_pref('use_dimension');
		// DIM-LFC-002: the application stays reachable even when no axis is enabled for new events.
		// A disabled posting preference is not evidence that historical classifications never existed,
		// so inquiry and report access must not disappear with the entry forms.
		// DIM-NAM-002: the tab shows the module name; the help key stays '&Dimensions' so the existing help page still opens.
		$this->help_context = '&Dimensions';
		parent::__construct('proj', dimension_term_module_tab(), true);

		if ($dim > 0) {
			$this->add_module(_('Transactions'));
			$this->add_lapp_function(0, _('&New Tracking Value'), 'tracking/tracking_value_entry.php?', 'SA_DIMENSION', MENU_ENTRY);
			$this->add_lapp_function(0, _('Open Tracking &Values'), 'tracking/inquiry/search_tracking_values.php?outstanding_only=1', 'SA_DIMTRANSVIEW', MENU_TRANSACTION);
		}

		$this->add_module(_('Inquiries and Reports'));
		// DIM-UX-002: the Overview that lists every Cost & Profit Tracking page of this menu and of General Ledger, by task.
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('&Overview'), 'tracking/overview.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		// DIM-UX-003: the end-user guide; read-only and open to every Dimension viewer.
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('&User Guide'), 'tracking/user_guide.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		// DIM-UX-001: the three-step guide, the made-up examples and the disabled-history view; read-only and open to every Dimension viewer.
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('&Getting Started'), 'tracking/getting_started.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('&Find Tracking Values'), 'tracking/inquiry/search_tracking_values.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		// DIM-REP-003: the shared workspace (Cost & Profit Explorer); the page itself also requires GL analytics.
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('Cost & Profit &Explorer'), 'tracking/inquiry/cost_profit_explorer.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		// DIM-REP-005: customer and project economics; the page itself also requires GL analytics and sales view.
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('&Customer Economics'), 'tracking/inquiry/customer_economics.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		// DIM-REP-006: comparisons, account matrix and the exception work queue; the page also requires GL analytics.
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('&Missing or Invalid Tracking'), 'tracking/inquiry/missing_or_invalid_tracking.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		// DIM-ADV-002: optional effective hierarchy and portfolio rollups; the page also requires GL analytics to read a rollup.
		// DIM-BUD-004: cost budgets at their warning or limit threshold; the page also requires GL analytics or budget entry.
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('Budget &Alerts'), 'tracking/inquiry/budget_alerts.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function($dim > 0 ? 1 : 0, _('Tracking &Hierarchy'), 'tracking/tracking_hierarchy.php?', 'SA_DIMTRANSVIEW', MENU_INQUIRY);
		$this->add_rapp_function($dim > 0 ? 1 : 0, _('Cost & Profit &Reports'), 'reporting/reports_main.php?Class=4', 'SA_DIMENSIONREP', MENU_REPORT);

		$this->add_module(_('Maintenance'));
		$this->add_lapp_function($dim > 0 ? 2 : 1, _('Trac&king Tags'), 'admin/tags.php?type=dimension', 'SA_DIMTAGS', MENU_MAINTENANCE);

		// DIM-CFG-001: the category policy and the read-only transition preview live under Company Setup
		// authority; publishing additionally requires the Dimension master authority.
		$this->add_module(_('Setup'));
		$this->add_lapp_function($dim > 0 ? 3 : 2, _('Tracking Categor&ies'), 'tracking/category_policy.php?', 'SA_SETUPCOMPANY', MENU_SETTINGS);

		$this->add_extensions();
	}
}
