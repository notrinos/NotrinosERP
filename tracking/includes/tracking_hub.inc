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
include_once(dirname(__FILE__).'/tracking_terms.inc');

/**
 * DIM-UX-002: one home (the Overview) for Cost & Profit Tracking.
 *
 * The pages that belong to Financial Tracking live in two application menus (Dimensions and General Ledger) and, until this
 * slice, nothing listed them together. This file is the catalog of those pages and the rule that decides what a given user is
 * shown. It is a PRESENTATION layer:
 *  - it writes nothing (no INSERT/UPDATE/DELETE, no preference, no migration) and reads no company row;
 *  - it never grants access: an entry is drawn as a link only for a user who holds the page's own security area, and the
 *    target page still performs its own check (the hub is a map, not a gate);
 *  - it states, per entry, which further areas the page needs up front, so a user is told before opening a page that its
 *    figures need an area the user lacks, instead of being sent to a refusal;
 *  - it says so for an entry whose family the release matrix holds back (allocation), so the home page never presents a held
 *    feature as part of the first release. The `held` flag is DECLARED here, not read from the release matrix at run time: the
 *    release rehearsal functions are decision support that no runtime page may call (DIM-REL-001), and the contract suite proves
 *    that every declared flag equals the matrix.
 *
 * DIM-NAM-002: superseded by the owner entry of 2026-11-17; the module is shown as Cost & Profit Tracking (labels only, see
 * tracking_terms.inc). Historical decision (2026-11-16): the application keeps its name "Dimensions". Renaming it would touch the application identity, every
 * translation of the menu and the HRM/payroll wording that already points at it; one home page reached from the menu, the
 * Getting Started guide and the General Ledger menu gives users the single starting point without that risk.
 */

/**
 * Catalog of the Cost & Profit Tracking pages, grouped by what a user is trying to do. Page names come from the term source.
 *
 * Every entry names the page's own security area (`area`, the `$page_security` of the target, which is also its menu area), the
 * complete list of areas the page needs before it shows a figure (`needs`, all required) and, for the budget pages, a rule that
 * any one of several areas is enough (`needs_any`). `family` names the release-matrix family of the entry; `held` (declared) is true when that family is held back from the first
 * release; `axis` marks an entry that only makes sense while at least one category is switched on for new entries.
 *
 * @return array List of sections: id, title, intro, entries (id, title, text, url, area, needs, needs_any, family, axis, optional held)
 */
function dimension_hub_catalog()
{
	$names = dimension_term_page_names();
	return array(
		array('id'=>'start', 'title'=>_('Start here'),
			'intro'=>_('New to Cost & Profit Tracking? The guide explains the three steps and shows made-up examples.'),
			'entries'=>array(
				array('id'=>'onboarding', 'title'=>$names['getting_started'], 'text'=>_('Three steps: name what you track, create the first value, code an expense. Examples included.'),
					'url'=>'tracking/getting_started.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
				array('id'=>'guide', 'title'=>$names['user_guide'], 'text'=>_('A short manual: the three steps to start, and what each page is for and how to read it.'),
					'url'=>'tracking/user_guide.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
			)),
		array('id'=>'see', 'title'=>_('See where money went'),
			'intro'=>_('Read-only views of revenue and costs by tracking value. They show posted figures and say how complete they are.'),
			'entries'=>array(
				array('id'=>'tracking', 'title'=>$names['explorer'], 'text'=>_('Revenue, costs and result of a tracking value for a period and for its lifetime, with the documents behind them.'),
					'url'=>'tracking/inquiry/cost_profit_explorer.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
				array('id'=>'customers', 'title'=>$names['customer_economics'], 'text'=>_('Revenue, contribution and allocated profit by customer, kept apart and labelled.'),
					'url'=>'tracking/inquiry/customer_economics.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC', 'SA_SALESTRANSVIEW'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
				array('id'=>'exceptions', 'title'=>$names['missing_tracking'], 'text'=>_('Comparisons with the previous period and the budget, and the queue of lines with a missing or invalid tracking value.'),
					'url'=>'tracking/inquiry/missing_or_invalid_tracking.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
				array('id'=>'inquiry', 'title'=>$names['find_values'], 'text'=>_('Find a tracking value, open its balance and its transactions.'),
					'url'=>'tracking/inquiry/search_tracking_values.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
				array('id'=>'hierarchy', 'title'=>$names['hierarchy'], 'text'=>_('The approved grouping of tracking values and the portfolio totals above them.'),
					'url'=>'tracking/tracking_hierarchy.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC'), 'needs_any'=>array(), 'family'=>'commitments_and_hierarchy', 'axis'=>false),
				array('id'=>'reports', 'title'=>$names['reports'], 'text'=>_('Printable and exportable cost and profit reports.'),
					'url'=>'reporting/reports_main.php?Class=4', 'area'=>'SA_DIMENSIONREP', 'needs'=>array('SA_DIMENSIONREP'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
			)),
		array('id'=>'plan', 'title'=>_('Plan and watch the budget'),
			'intro'=>_('Budgets, what is already committed, and which budgets are close to their limit. Advisory: nothing here blocks a document.'),
			'entries'=>array(
				array('id'=>'alerts', 'title'=>$names['budget_alerts'], 'text'=>_('Cost budgets that reached the warning or limit level of the year.'),
					'url'=>'tracking/inquiry/budget_alerts.php?', 'area'=>'SA_DIMTRANSVIEW', 'needs'=>array('SA_DIMTRANSVIEW'), 'needs_any'=>array('SA_GLANALYTIC', 'SA_BUDGETENTRY'), 'family'=>'', 'axis'=>false),
				array('id'=>'budget', 'title'=>_('Budget Entry'), 'text'=>_('Enter and change the budget by account and tracking value.'),
					'url'=>'gl/gl_budget.php?', 'area'=>'SA_BUDGETENTRY', 'needs'=>array('SA_BUDGETENTRY'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
				array('id'=>'versions', 'title'=>_('Budget Baselines and Revisions'), 'text'=>_('Fix an approved baseline, propose revisions and choose which version reports compare with.'),
					'url'=>'gl/gl_budget_version.php?', 'area'=>'SA_BUDGETENTRY', 'needs'=>array('SA_BUDGETENTRY'), 'needs_any'=>array(), 'family'=>'budget_baselines', 'axis'=>false),
				array('id'=>'commitments', 'title'=>_('Commitments, Reservations and Forecast'), 'text'=>_('Open purchase commitments, optional reservations and the forecast beside the budget.'),
					'url'=>'gl/gl_commitment_forecast.php?', 'area'=>'SA_BUDGETENTRY', 'needs'=>array('SA_BUDGETENTRY'), 'needs_any'=>array(), 'family'=>'commitments_and_hierarchy', 'axis'=>false),
			)),
		array('id'=>'move', 'title'=>_('Move cost between tracking values'),
			'intro'=>_('Journals that reclassify or share posted cost. They post to the ledger, so they are held back from the first release.'),
			'entries'=>array(
				array('id'=>'allocation', 'title'=>$names['allocation_journal'], 'text'=>_('Allocate or reclassify posted rows by fixed percentages or amounts, with an exact inverse.'),
					'url'=>'gl/gl_allocation.php?', 'area'=>'SA_JOURNALENTRY', 'needs'=>array('SA_JOURNALENTRY'), 'needs_any'=>array(), 'family'=>'allocation_journals', 'axis'=>false, 'held'=>true),
				array('id'=>'drivers', 'title'=>$names['allocation_drivers'], 'text'=>_('Measured drivers (overhead, labor) and allocation runs that use them.'),
					'url'=>'gl/gl_allocation_driver.php?', 'area'=>'SA_JOURNALENTRY', 'needs'=>array('SA_JOURNALENTRY'), 'needs_any'=>array(), 'family'=>'driver_allocation', 'axis'=>false, 'held'=>true),
			)),
		array('id'=>'setup', 'title'=>_('Set up and maintain'),
			'intro'=>_('Create the values you track, label the categories and keep tags tidy.'),
			'entries'=>array(
				array('id'=>'entry', 'title'=>$names['new_value'], 'text'=>_('Create a tracking value, or edit one.'),
					'url'=>'tracking/tracking_value_entry.php?', 'area'=>'SA_DIMENSION', 'needs'=>array('SA_DIMENSION'), 'needs_any'=>array(), 'family'=>'', 'axis'=>true),
				array('id'=>'tags', 'title'=>$names['tags'], 'text'=>_('Maintain the tags used to group tracking values.'),
					'url'=>'admin/tags.php?type=dimension', 'area'=>'SA_DIMTAGS', 'needs'=>array('SA_DIMTAGS'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
				array('id'=>'policy', 'title'=>$names['categories'], 'text'=>_('Name the two categories and say what each one is for.'),
					'url'=>'tracking/category_policy.php?', 'area'=>'SA_SETUPCOMPANY', 'needs'=>array('SA_SETUPCOMPANY'), 'needs_any'=>array(), 'family'=>'', 'axis'=>false),
			)),
	);
}

/**
 * Plain name of one security area, for the sentence that says what a user lacks.
 *
 * Uses the installed area names; an unknown area gets a generic phrase and never the internal code.
 *
 * @param string $area Security area such as SA_GLANALYTIC
 * @return string
 */
function dimension_hub_area_name($area)
{
	global $security_areas;
	if (isset($security_areas[$area][1]) && is_string($security_areas[$area][1]) && $security_areas[$area][1] !== '')
		return $security_areas[$area][1];
	return _('a further permission');
}

/**
 * Decide what one user sees for every Financial Tracking page.
 *
 * States of an entry: `open` (the user holds the page area and every area it needs), `partial` (the page opens, but a figure
 * needs areas the user lacks; `missing` names them), `denied` (the user lacks the page's own area, so no link is drawn) and
 * `off` (the entry needs a category that is switched on for new entries and none is). `held` is true when the catalog declares the entry's family held back
 * from the first release.
 *
 * @param callable|null $has_access Takes a security area and returns bool; defaults to the session's user_check_access()
 * @param int $enabled_axes Number of categories switched on for new entries
 * @return array Same sections as the catalog; each entry gains state, missing (list of names) and a boolean held
 */
function dimension_hub_view($has_access = null, $enabled_axes = 1)
{
	if ($has_access === null)
		$has_access = 'user_check_access';
	$can = function ($area) use ($has_access) {
		return is_callable($has_access) && (bool)call_user_func($has_access, $area);
	};
	$view = array();
	foreach (dimension_hub_catalog() as $section) {
		$entries = array();
		foreach ($section['entries'] as $entry) {
			$missing = array();
			if (!$can($entry['area']))
				$state = 'denied';
			else {
				foreach ($entry['needs'] as $area)
					if (!$can($area))
						$missing[$area] = dimension_hub_area_name($area);
				if (count($entry['needs_any'])) {
					$any = false;
					foreach ($entry['needs_any'] as $area)
						if ($can($area))
							$any = true;
					if (!$any)
						$missing['any'] = implode(' '._('or').' ', array_map('dimension_hub_area_name', $entry['needs_any']));
				}
				$state = count($missing) ? 'partial' : 'open';
				if ($entry['axis'] && (int)$enabled_axes < 1 && $state !== 'denied')
					$state = 'off';
			}
			$entry['state'] = $state;
			$entry['missing'] = array_values($missing);
			$entry['held'] = !empty($entry['held']);
			$entries[] = $entry;
		}
		$section['entries'] = $entries;
		$view[] = $section;
	}
	return $view;
}

/**
 * Number of categories switched on for new entries in the active company.
 *
 * @return int 0, 1 or 2
 */
function dimension_hub_enabled_axes()
{
	return max(0, min(2, (int)get_company_pref('use_dimension')));
}

/**
 * Draw the home page body for a view produced by dimension_hub_view().
 *
 * Every value is escaped. A `denied` entry is plain text with no link, so the page never offers a click that ends in a refusal.
 *
 * @param array $view Result of dimension_hub_view()
 * @return void
 */
function display_dimension_hub($view)
{
	global $path_to_root;

	$state_text = array('open'=>_('Open'), 'partial'=>_('Opens, but figures need more'), 'denied'=>_('Not in your role'), 'off'=>_('Switched off for new entries'));
	foreach ($view as $section) {
		display_heading2($section['title']);
		display_note($section['intro']);
		start_table(TABLESTYLE);
		table_header(array(_('Page'), _('What it is for'), _('For you')));
		$k = 0;
		foreach ($section['entries'] as $entry) {
			alt_table_row_color($k);
			$title = htmlspecialchars($entry['title'], ENT_QUOTES, 'UTF-8');
			if ($entry['state'] === 'denied' || $entry['state'] === 'off')
				label_cell('<b>'.$title.'</b>');
			else
				label_cell("<a href='".$path_to_root.'/'.htmlspecialchars($entry['url'], ENT_QUOTES, 'UTF-8')."'><b>".$title.'</b></a>');
			label_cell(htmlspecialchars($entry['text'], ENT_QUOTES, 'UTF-8'));
			$status = htmlspecialchars($state_text[$entry['state']], ENT_QUOTES, 'UTF-8');
			if ($entry['state'] === 'partial')
				$status .= '<br>'.htmlspecialchars(sprintf(_('To see figures you also need: %s.'), implode('; ', $entry['missing'])), ENT_QUOTES, 'UTF-8');
			elseif ($entry['state'] === 'denied')
				$status .= '<br>'.htmlspecialchars(sprintf(_('Ask an administrator for: %s.'), dimension_hub_area_name($entry['area'])), ENT_QUOTES, 'UTF-8');
			if ($entry['held'])
				$status .= '<br><b>'.htmlspecialchars(_('Held back from the first release: it posts to the ledger and awaits a second pilot.'), ENT_QUOTES, 'UTF-8').'</b>';
			label_cell($status);
			end_row();
		}
		end_table(1);
	}
}
