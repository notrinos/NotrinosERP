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
 * Report 312 — Recall Status Report
 *
 * Shows recall campaign status overview with per-campaign progress metrics.
 * Includes: campaign summary, affected items breakdown, customer notification
 * status, and recovery percentages.
 *
 * Parameters:
 *   PARAM_0: Date From (start_date)
 *   PARAM_1: Recall Status filter (empty = all)
 *   PARAM_2: Severity filter (empty = all)
 *   PARAM_3: Item code filter
 *   PARAM_4: Comments
 *   PARAM_5: Orientation (0=Portrait, 1=Landscape)
 *   PARAM_6: Destination (0=PDF, 1=Excel)
 */
$page_security = 'SA_RECALL';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/db/recall_db.inc');

//----------------------------------------------------------------------------------------------------

print_recall_status_report();

function print_recall_status_report()
{
	global $path_to_root;

	$date_from   = $_POST['PARAM_0'];
	$status      = $_POST['PARAM_1'];
	$severity    = $_POST['PARAM_2'];
	$stock_id    = $_POST['PARAM_3'];
	$comments    = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];

	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');

	// Resolve filter labels
	$status_label = _('All');
	if ($status != '') {
		$statuses = get_recall_campaign_statuses();
		$status_label = isset($statuses[$status]) ? $statuses[$status] : $status;
	}

	$severity_label = _('All');
	if ($severity != '') {
		$severities = get_recall_severity_levels();
		$severity_label = isset($severities[$severity]) ? $severities[$severity] : $severity;
	}

	$item_label = $stock_id ? $stock_id : _('All');

	// Convert date filter
	$sql_date_from = '';
	if ($date_from != '')
		$sql_date_from = date2sql($date_from);

	$params = array(
		0 => $comments,
		1 => array('text' => _('Date From'),  'from' => ($date_from ? $date_from : _('Beginning')), 'to' => ''),
		2 => array('text' => _('Status'),      'from' => $status_label, 'to' => ''),
		3 => array('text' => _('Severity'),    'from' => $severity_label, 'to' => ''),
		4 => array('text' => _('Item'),        'from' => $item_label, 'to' => ''),
	);

	// Column definitions
	$cols = array(0, 60, 180, 210, 250, 295, 340, 385, 430, 475, 515);
	$headers = array(
		_('Reference'),
		_('Title'),
		_('Severity'),
		_('Status'),
		_('Total'),
		_('Identified'),
		_('Notified'),
		_('Resolved'),
		_('Unreachable'),
		_('Recovery %'),
	);
	$aligns = array('left', 'left', 'center', 'center', 'right', 'right', 'right', 'right', 'right', 'right');

	$rep = new FrontReport(_('Recall Status Report'), 'RecallStatusReport', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	// Fetch data
	$campaigns = get_recall_campaigns_with_progress($stock_id, $status, $severity, $sql_date_from);

	$grand_total = 0;
	$grand_identified = 0;
	$grand_notified = 0;
	$grand_resolved = 0;
	$grand_unreachable = 0;

	if (empty($campaigns)) {
		$rep->TextCol(0, 5, _('No recall campaigns found matching the criteria.'));
		$rep->NewLine();
	}

	foreach ($campaigns as $row) {
		$total_items     = (int)$row['total_items'];
		$cnt_identified  = (int)$row['cnt_identified'];
		$cnt_notified    = (int)$row['cnt_notified'];
		$cnt_resolved    = (int)$row['cnt_resolved'];
		$cnt_unreachable = (int)$row['cnt_unreachable'];
		$recovery_pct    = $total_items > 0 ? round(($cnt_resolved / $total_items) * 100, 1) : 0;

		$rep->TextCol(0, 1, $row['reference']);
		$rep->TextCol(1, 2, $row['title'], -1);

		$severity_labels = get_recall_severity_levels();
		$sev_label = isset($severity_labels[$row['severity']]) ? $severity_labels[$row['severity']] : $row['severity'];
		$rep->TextCol(2, 3, $sev_label);

		$status_labels = get_recall_campaign_statuses();
		$st_label = isset($status_labels[$row['status']]) ? $status_labels[$row['status']] : $row['status'];
		$rep->TextCol(3, 4, $st_label);

		$rep->TextCol(4, 5, number_format($total_items));
		$rep->TextCol(5, 6, number_format($cnt_identified));
		$rep->TextCol(6, 7, number_format($cnt_notified));
		$rep->TextCol(7, 8, number_format($cnt_resolved));
		$rep->TextCol(8, 9, number_format($cnt_unreachable));
		$rep->TextCol(9, 10, $recovery_pct . '%');

		$rep->NewLine();

		// Sub-detail: Item code and scope
		$rep->fontSize -= 2;
		$rep->TextCol(0, 2, '  ' . _('Item') . ': ' . $row['stock_id'] . ' - '
			. ($row['item_description'] ? $row['item_description'] : ''));
		$scope_parts = array();
		if ($row['affected_serial_from'] && $row['affected_serial_to'])
			$scope_parts[] = _('Serial') . ': ' . $row['affected_serial_from'] . '-' . $row['affected_serial_to'];
		if ($row['affected_batch_ids'])
			$scope_parts[] = _('Batches') . ': ' . $row['affected_batch_ids'];
		if ($row['affected_date_from'])
			$scope_parts[] = _('Mfg Date') . ': ' . sql2date($row['affected_date_from'])
				. ($row['affected_date_to'] ? ' - ' . sql2date($row['affected_date_to']) : '');
		if (!empty($scope_parts))
			$rep->TextCol(2, 10, implode('; ', $scope_parts));
		$rep->fontSize += 2;

		$rep->NewLine();

		// Customer notification sub-line
		if ($row['total_customers'] > 0) {
			$rep->fontSize -= 2;
			$rep->TextCol(0, 5, '  ' . sprintf(_('Customers: %d total, %d notified'),
				(int)$row['total_customers'], (int)$row['notified_customers']));
			$rep->fontSize += 2;
			$rep->NewLine();
		}

		$rep->Line($rep->row + 4);
		$rep->NewLine();

		// Accumulate grand totals
		$grand_total       += $total_items;
		$grand_identified  += $cnt_identified;
		$grand_notified    += $cnt_notified;
		$grand_resolved    += $cnt_resolved;
		$grand_unreachable += $cnt_unreachable;
	}

	// Grand totals
	$rep->NewLine();
	$rep->Font('bold');
	$rep->TextCol(0, 4, _('Grand Total'));
	$rep->TextCol(4, 5, number_format($grand_total));
	$rep->TextCol(5, 6, number_format($grand_identified));
	$rep->TextCol(6, 7, number_format($grand_notified));
	$rep->TextCol(7, 8, number_format($grand_resolved));
	$rep->TextCol(8, 9, number_format($grand_unreachable));

	$grand_pct = $grand_total > 0 ? round(($grand_resolved / $grand_total) * 100, 1) : 0;
	$rep->TextCol(9, 10, $grand_pct . '%');
	$rep->Font();

	$rep->Line($rep->row - 4);
	$rep->NewLine();

	// Campaign count summary
	$rep->NewLine();
	$rep->TextCol(0, 5, sprintf(_('Total campaigns: %d'), count($campaigns)));
	$rep->NewLine();

	$rep->End();
}
