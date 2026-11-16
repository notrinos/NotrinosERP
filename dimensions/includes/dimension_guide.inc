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
 * A short manual, kept inside the application (the development folders are denied to the web, so a file in DOCS would never reach a
 * user). It explains the Getting Started flow and every Financial Tracking page of the DIM-UX-002 catalog: what the page is for, how to
 * use it and what to read carefully. It is a PRESENTATION layer, like the home page:
 *  - it writes nothing and reads no company figure (no query, no preference);
 *  - it never grants access: a topic shows a link only for a page the user can open, using the same view decision as the home page,
 *    and the target page still performs its own check;
 *  - every sentence is wrapped in `_()` so a translation of the application translates the guide with it. The only language shipped
 *    with this release is English (`lang/installed_languages.inc`), so the guide ships in English; no machine translation was produced.
 *
 * The contract suite proves that every page of the hub catalog has a topic, that every topic that names a page names a real one, and
 * that the guide states the rules the figures depend on (revenue and costs signs, whole-month budget, held-back allocation, advisory
 * budget checks) in the same words the pages use.
 */

/**
 * The topics of the guide, in reading order.
 *
 * A topic has an `id`, a `title`, a one-sentence `summary`, optional `use` (how to use it, numbered) and `watch` (what to read carefully,
 * bulleted) lists, and `hub`: the id of the hub catalog entry the topic explains, or '' for a topic that explains a concept. The first
 * topic names the guide itself (`guide`), and Getting Started is explained by the `start` topic.
 *
 * @return array List of topics
 */
function dimension_guide_catalog()
{
	return array(
		array('id'=>'what', 'hub'=>'guide', 'title'=>_('What Financial Tracking does'),
			'summary'=>_('Financial Tracking shows where money was earned or spent, by Dimension. A Dimension is a label you put on a line of a document, such as a project, a department or a cost center.'),
			'use'=>array(
				_('A company can use up to two independent categories of Dimension. Each posted line can carry one value of each category, or none.'),
				_('The label travels with the line from the document through approval and posting into the General Ledger, and the figures here are read from the posted ledger.'),
			),
			'watch'=>array(
				_('Posted history is never edited. A mistake is corrected by a linked, approved journal, not by changing the label of a posted line.'),
				_('Every page here is a view of posted figures in the company currency. Revenue is credit-positive, costs are debit-positive and result is revenue minus costs.'),
			)),
		array('id'=>'start', 'hub'=>'onboarding', 'title'=>_('Getting Started in three steps'),
			'summary'=>_('The Getting Started page walks a new company through the three steps and says, for each one, whether it is done, what is missing and who can do it.'),
			'use'=>array(
				_('Say what you want to track: give each category a name such as Project or Cost center, so everyone reads the same words (a user with Company Setup rights does this on the Dimension Category Policy page).'),
				_('Create your first value: enter a name, a start date and, where it applies, a date it is needed by (a user with Dimension entry rights does this on Dimension Entry).'),
				_('Code an expense and see where it went: on an expense, payment or journal line choose the Dimension, then open Financial Tracking to see the amount and the document it came from.'),
				_('Open one of the two examples on the Getting Started page first if you want to see the result before you start.'),
			),
			'watch'=>array(
				_('The examples use made-up numbers. They are not your company data and nothing is saved.'),
				_('If no category is switched on for new entries, the steps say so; history already recorded stays readable.'),
			)),
		array('id'=>'tracking', 'hub'=>'tracking', 'title'=>_('Financial Tracking'),
			'summary'=>_('Revenue, costs and result of a Dimension for the period you choose and for its lifetime, with the documents behind each figure.'),
			'use'=>array(
				_('Choose the period with From and To, then a value of Dimension 1 and/or Dimension 2, and press Show. Leave both on All Dimensions to see everything.'),
				_('Tick Unassigned only to list the postings that carry no Dimension on that category.'),
				_('Read the result by Dimension first, then the coverage by source family, the budget comparison and the drilldown by account.'),
			),
			'watch'=>array(
				_('Coverage tells you how much of the eligible amount carries a Dimension. When coverage is incomplete, the figures describe only the classified part.'),
				_('The net balance of balance-sheet accounts is separate from revenue, costs and result and is not profit.'),
				_('The drilldown lists a limited number of accounts; narrow the scope to see the rest.'),
				_('You need the Dimension view area and GL analytical reports and inquiries. Without the second you see an explicit denial and no amount.'),
			)),
		array('id'=>'customers', 'hub'=>'customers', 'title'=>_('Customer Economics'),
			'summary'=>_('Revenue, matched cost, direct contribution, margin and share by customer, with what could not be attributed to a customer listed beside it.'),
			'use'=>array(
				_('Choose the period and the Dimension scope as on Financial Tracking, then press Show.'),
				_('Read each customer row with its status, then the table of what is not attributed to a customer and the next step for each reason.'),
				_('Use the attribution queue to find the source documents that still wait for a customer.'),
			),
			'watch'=>array(
				_('Revenue, direct contribution and fully allocated profit are three different measures. They are kept apart and labelled and are never presented as one profit.'),
				_('Direct contribution counts only cost that a customer document supports. A cost that no customer document supports is not split among customers and is not estimated.'),
				_('Fully allocated profit is shown as not available until allocation coverage has been accepted.'),
				_('The split must reconcile to Financial Tracking for the same scope and period; if the page warns that it does not, do not rely on it.'),
				_('You need the Dimension view area, GL analytical reports and inquiries and Sales transactions view.'),
			)),
		array('id'=>'exceptions', 'hub'=>'exceptions', 'title'=>_('Dimension Exceptions'),
			'summary'=>_('Compares a period with the previous period, with last year or with the monthly budget, and lists the postings that have no Dimension or an invalid one.'),
			'use'=>array(
				_('Choose the period and what to compare it to: Previous period, Year to date vs last year or Monthly budget.'),
				_('Tick Account matrix to see accounts against Dimensions, and Exception queue to list the lines that need correcting.'),
				_('Save filter keeps your comparison and scope for next time; only the choices are saved, the amounts are always read again.'),
				_('The same figures are printed by the Dimension Comparison and Exceptions report, as PDF and as Excel.'),
			),
			'watch'=>array(
				_('A line with no Dimension is shown as Not assigned. Correct it with a linked approved journal; it is not edited in place.'),
				_('A year-to-date comparison needs the fiscal year to start on or before the end date.'),
				_('You need the Dimension view area and GL analytical reports and inquiries. The customer reason in the queue also needs Sales transactions view.'),
			)),
		array('id'=>'inquiry', 'hub'=>'inquiry', 'title'=>_('Dimension Inquiry'),
			'summary'=>_('Find a Dimension, then open its balance and its transactions.'),
			'use'=>array(
				_('Search by reference or name, narrow the list by category, start date, activity period, Only Open or Only Overdue, then open a Dimension to see its balance for the period and the transactions behind it.'),
			),
			'watch'=>array(
				_('A closed Dimension stays readable: it only stops accepting new postings.'),
				_('A Dimension that no longer exists is shown by its number; its historical rows remain readable.'),
			)),
		array('id'=>'hierarchy', 'hub'=>'hierarchy', 'title'=>_('Dimension Hierarchy'),
			'summary'=>_('An optional, approved grouping of Dimensions of one category and the portfolio totals above them.'),
			'use'=>array(
				_('Propose a structure: one line per change, with the Dimension and its parent (0 means no parent), an effective date and a commentary. It waits for an independent checker.'),
				_('A different user approves or rejects the proposal. An approved structure applies from its effective date.'),
				_('Use the portfolio rollup to see revenue and costs rolled up to the parents, with each posting counted once.'),
			),
			'watch'=>array(
				_('Until a structure is approved, Dimensions are flat and nothing is rolled up.'),
				_('As posted shows each posting under the structure of its date; Current structure restates history under today\'s structure. Read the label before comparing two figures.'),
				_('The proposing user cannot decide the same structure; ask an independent checker.'),
			)),
		array('id'=>'reports', 'hub'=>'reports', 'title'=>_('Dimension Reports'),
			'summary'=>_('Printable and exportable Dimension reports, including Comparison and Exceptions and the Dimension Summary.'),
			'use'=>array(
				_('Open Dimension Reports, choose a report, set its parameters and print it or export it to PDF or Excel.'),
			),
			'watch'=>array(
				_('A report shows the same figures as its page. A report that needs an area you do not hold is refused, not shown with blanks.'),
			)),
		array('id'=>'alerts', 'hub'=>'alerts', 'title'=>_('Budget Alerts'),
			'summary'=>_('Lists the cost budgets that have reached the warning or the limit level of the fiscal year, and shows two tiles on the Dimensions dashboard.'),
			'use'=>array(
				_('Choose the fiscal year and press Show. Each row gives the budget, the actual, commitments, reservations, the share used and the free amount.'),
				_('A user with Company Setup rights can change the warning and limit percentages (default 90% and 100%) and must give a reason; the newest changes are kept.'),
			),
			'watch'=>array(
				_('Alerts are advisory. They never block a document and never write anything.'),
				_('A budget that is incomplete or cannot be compared by whole months is shown as Not evaluated with the reason, never guessed.'),
				_('Only cost accounts are evaluated: a revenue budget is a target, not a limit.'),
			)),
		array('id'=>'panel', 'hub'=>'', 'title'=>_('Budget check on purchasing documents'),
			'summary'=>_('On purchase order entry, the saved order, the requisition and the approval review, a panel shows the budget still free for the account and Dimension of each line.'),
			'use'=>array(
				_('Read, per account and Dimension: budget, actual, commitments, reservations, free now, this document, free after, and Within budget, Over by an amount or No comparable budget.'),
			),
			'watch'=>array(
				_('The panel is advisory: it never blocks, writes or reserves anything.'),
				_('A purchasing-only user is told that the figures exist and sees no amount until an administrator grants a budget area.'),
				_('An order saved a moment ago may not be counted yet: the panel prints the time its commitments were observed.'),
			)),
		array('id'=>'budget', 'hub'=>'budget', 'title'=>_('Budget Entry'),
			'summary'=>_('Enter and change the budget by account and Dimension.'),
			'use'=>array(
				_('Choose the fiscal year, the account and the Dimension, press Get, enter the amount of each period and save.'),
			),
			'watch'=>array(
				_('Changes made here affect the live budget entry only. Reports compare with it unless an approved version has been made the active comparison.'),
				_('Budget comparison is whole-month. A partial period shows actuals with budget and variance as N/A, and a zero budget is not the same as no budget.'),
			)),
		array('id'=>'versions', 'hub'=>'versions', 'title'=>_('Budget Baselines and Revisions'),
			'summary'=>_('Fix an approved baseline of the budget, propose revisions and choose which version reports compare with.'),
			'use'=>array(
				_('Capture the baseline: it is the live budget as it stands now. A revision proposes changes to the latest approved version and needs a commentary. A different user, the independent checker, approves or rejects each one.'),
				_('Press Make active on an approved version to compare reports against it, or Use the live budget entry again to go back. Give a reason for the change.'),
				_('Use Compare to see the differences between two versions or between a version and the live budget.'),
			),
			'watch'=>array(
				_('An approved version is not used by the reports until it is made the active comparison. Until then reports compare with the live budget exactly as before.'),
				_('The proposing user cannot approve the same version.'),
			)),
		array('id'=>'commitments', 'hub'=>'commitments', 'title'=>_('Commitments, Reservations and Forecast'),
			'summary'=>_('Open purchase commitments, optional reservations and a forecast shown beside the budget.'),
			'use'=>array(
				_('Read the funds position of an account and Dimension: budget, actual, commitments, reservations and what is free.'),
				_('A reservation is refused until a user with Company Setup rights switches reservations on.'),
			),
			'watch'=>array(
				_('Everything here is advisory. A commitment or a forecast never changes the ledger.'),
			)),
		array('id'=>'allocation', 'hub'=>'allocation', 'title'=>_('Dimension Allocation Journal'),
			'summary'=>_('A journal that reclassifies or shares posted cost between Dimensions by fixed amounts or percentages, with an exact inverse.'),
			'use'=>array(
				_('This page is held back from the first release because it posts to the ledger. It waits for a second pilot, and the page says so.'),
			),
			'watch'=>array(
				_('Do not use it for production postings until your administrator tells you it is released for your company.'),
			)),
		array('id'=>'drivers', 'hub'=>'drivers', 'title'=>_('Dimension Allocation Drivers'),
			'summary'=>_('Measured drivers, such as overhead or labor, and the allocation runs that use them.'),
			'use'=>array(
				_('This page is held back from the first release because its runs post to the ledger. It waits for a second pilot, and the page says so.'),
			),
			'watch'=>array(
				_('Do not use it for production postings until your administrator tells you it is released for your company.'),
			)),
		array('id'=>'entry', 'hub'=>'entry', 'title'=>_('Dimension Entry'),
			'summary'=>_('Create a Dimension, or edit one.'),
			'use'=>array(
				_('Enter a reference, a name, the category (Type), a start date and the date it is needed by; add tags and a memo if you like, then Add.'),
				_('To retire a Dimension, close it. A closed Dimension accepts no new postings, keeps its history and can be re-opened.'),
			),
			'watch'=>array(
				_('A Dimension that has been used is closed, never deleted. Delete works only for a Dimension that nothing references.'),
				_('The category of a Dimension can be changed only while no record, draft or approval refers to it.'),
			)),
		array('id'=>'tags', 'hub'=>'tags', 'title'=>_('Dimension Tags'),
			'summary'=>_('Maintain the tags used to group Dimensions.'),
			'use'=>array(
				_('Add, rename or remove a tag, then pick tags on Dimension Entry.'),
			),
			'watch'=>array(
				_('A tag is only a label for finding Dimensions. It never changes a figure.'),
			)),
		array('id'=>'policy', 'hub'=>'policy', 'title'=>_('Dimension Category Policy'),
			'summary'=>_('Name the two categories and say what each one is for.'),
			'use'=>array(
				_('Choose a label and a purpose for each category, say whether it is required for new events, and publish. Each publication is a new version.'),
				_('Use the transition preview before you switch a category on or off: it lists what would block the change.'),
			),
			'watch'=>array(
				_('A policy is descriptive. It never renumbers or remaps a stored Dimension and never rewrites ledger, budget or payroll rows.'),
				_('Publishing needs both Company Setup and Dimension entry rights.'),
			)),
		array('id'=>'words', 'hub'=>'', 'title'=>_('Words used on these pages'),
			'summary'=>_('The words that appear on several pages and what they mean.'),
			'use'=>array(),
			'watch'=>array(),
			'terms'=>array(
				_('Dimension')=>_('A value you put on a document line, such as a project or a cost center.'),
				_('Category')=>_('One of the two independent kinds of Dimension (Dimension 1 and Dimension 2). A company names them on the Category Policy page.'),
				_('Not assigned')=>_('A posted line that carries no value for that category.'),
				_('Actual')=>_('What has been posted to the ledger.'),
				_('Commitment')=>_('An open purchase order or requisition that is not yet posted. It is counted beside the budget, never in the actual.'),
				_('Reservation')=>_('An optional hold on part of a budget. It is advisory and is refused until it is switched on.'),
				_('Free')=>_('Budget minus actual, commitments and reservations.'),
				_('Advisory')=>_('Shown to help a decision; it never blocks a document or changes the ledger.'),
				_('Baseline')=>_('A budget version that has been approved and fixed.'),
				_('Held back')=>_('A feature that is part of the product but not offered in the first release.'),
			)),
		array('id'=>'help', 'hub'=>'', 'title'=>_('If something looks wrong'),
			'summary'=>_('The most common questions, and where to look.'),
			'use'=>array(),
			'watch'=>array(),
			'questions'=>array(
				_('A page says I am not allowed to see figures.')=>_('Open Financial Tracking Home: it names the areas the page still needs. Ask your administrator for them; nothing here can be unlocked by a setting on the page.'),
				_('A line shows Not assigned.')=>_('The document was posted without a Dimension. Open Dimension Exceptions, find it in the queue and correct it with an approved journal.'),
				_('The budget column shows N/A.')=>_('The period is not made of whole months, or the budget is incomplete for that account. Choose whole months or complete the budget.'),
				_('A figure here differs from the Profit and Loss.')=>_('Financial Tracking shows posted revenue and costs by Dimension and states its coverage. Lines without a Dimension are listed separately, so compare the totals including Not assigned.'),
				_('I cannot find a Dimension to code a line with.')=>_('It may be closed, or its category may not be switched on for new entries. Ask a user with Dimension entry rights to re-open it or check the Category Policy.'),
				_('A menu entry I expected is missing.')=>_('The menu only lists pages your role can open. Financial Tracking Home shows every page and names those that are not in your role.'),
			)),
	);
}

/**
 * Decide what each topic shows for one user: the hub state of the page it explains.
 *
 * @param array $view Result of dimension_hub_view()
 * @return array Topic id => array(state, title, url, held) for a topic that names a page; other topics are omitted
 */
function dimension_guide_page_states($view)
{
	$entries = array();
	foreach ($view as $section)
		foreach ($section['entries'] as $entry)
			$entries[$entry['id']] = $entry;
	$states = array();
	foreach (dimension_guide_catalog() as $topic) {
		if ($topic['hub'] === '' || $topic['hub'] === 'guide' || !isset($entries[$topic['hub']]))
			continue;
		$entry = $entries[$topic['hub']];
		$states[$topic['id']] = array('state'=>$entry['state'], 'title'=>$entry['title'], 'url'=>$entry['url'], 'held'=>$entry['held'], 'missing'=>$entry['missing']);
	}
	return $states;
}

/**
 * Draw the guide body.
 *
 * Every value is escaped. A page the user cannot open is named with no link and no list of missing areas, as on the home page.
 *
 * @param array $topics Result of dimension_guide_catalog()
 * @param array $states Result of dimension_guide_page_states()
 * @return void
 */
function display_dimension_guide($topics, $states)
{
	global $path_to_root;
	$h = function ($text) { return htmlspecialchars($text, ENT_QUOTES, 'UTF-8'); };

	display_heading2(_('In this guide'));
	echo '<ul>';
	foreach ($topics as $topic)
		echo "<li><a href='#guide_".$h($topic['id'])."'>".$h($topic['title']).'</a></li>';
	echo "</ul>\n";

	foreach ($topics as $topic) {
		echo "<a name='guide_".$h($topic['id'])."'></a>";
		display_heading2($h($topic['title']));
		echo '<p>'.$h($topic['summary'])."</p>\n";
		if (isset($states[$topic['id']])) {
			$state = $states[$topic['id']];
			if ($state['state'] === 'open' || $state['state'] === 'partial') {
				echo "<p><a href='".$path_to_root.'/'.$h($state['url'])."'><b>".$h(sprintf(_('Open %s'), $state['title'])).'</b></a>';
				if ($state['state'] === 'partial')
					echo ' - '.$h(sprintf(_('To see figures you also need: %s.'), implode('; ', $state['missing'])));
				echo "</p>\n";
			}
			elseif ($state['state'] === 'denied')
				echo '<p><i>'.$h(_('This page is not in your role.')).'</i></p>'."\n";
			else
				echo '<p><i>'.$h(_('This page is switched off for new entries.')).'</i></p>'."\n";
			if ($state['held'])
				echo '<p><b>'.$h(_('Held back from the first release: it posts to the ledger and awaits a second pilot.')).'</b></p>'."\n";
		}
		if (count($topic['use'])) {
			echo '<b>'.$h(_('How to use it')).'</b><ol>';
			foreach ($topic['use'] as $line)
				echo '<li>'.$h($line).'</li>';
			echo "</ol>\n";
		}
		if (count($topic['watch'])) {
			echo '<b>'.$h(_('Read it carefully')).'</b><ul>';
			foreach ($topic['watch'] as $line)
				echo '<li>'.$h($line).'</li>';
			echo "</ul>\n";
		}
		foreach (array('terms', 'questions') as $kind) {
			if (empty($topic[$kind]))
				continue;
			echo '<dl>';
			foreach ($topic[$kind] as $term => $text)
				echo '<dt><b>'.$h($term).'</b></dt><dd>'.$h($text).'</dd>';
			echo "</dl>\n";
		}
	}
}
