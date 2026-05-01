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

$page_security = 'SA_PURCHREQUISITION';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Purchase Requisitions Inquiry'), false, false, '', $js);

if (isset($_POST['BatchApprove']) || isset($_POST['BatchReject'])) {
	$selected_requisitions = array();
	if (!empty($_POST['selected_req']) && is_array($_POST['selected_req'])) {
		foreach ($_POST['selected_req'] as $requisition_id) {
			if ((int)$requisition_id > 0)
				$selected_requisitions[] = (int)$requisition_id;
		}
	}

	if (empty($selected_requisitions)) {
		display_error(_('Select at least one requisition first.'));
	} elseif (isset($_POST['BatchApprove'])) {
		$approved_count = 0;
		foreach ($selected_requisitions as $requisition_id) {
			if (approve_requisition($requisition_id, (int)$_SESSION['wa_current_user']->user))
				$approved_count++;
		}

		display_notification(sprintf(_('Approved %d requisition(s).'), $approved_count));
	} else {
		$rejection_reason = trim(get_post('batch_rejection_reason'));
		if ($rejection_reason === '') {
			display_error(_('Enter one rejection reason for the selected requisitions.'));
			set_focus('batch_rejection_reason');
		} else {
			$rejected_count = 0;
			foreach ($selected_requisitions as $requisition_id) {
				if (reject_requisition($requisition_id, (int)$_SESSION['wa_current_user']->user, $rejection_reason))
					$rejected_count++;
			}

			display_notification(sprintf(_('Rejected %d requisition(s).'), $rejected_count));
		}
	}
}

$filter_status = get_post('filter_status', '');
$filter_requester = get_post('filter_requester', 0);
$filter_department = get_post('filter_department', 0);
$filter_date_from = get_post('filter_date_from', '');
$filter_date_to = get_post('filter_date_to', '');

$rows = array();
$department_summary = array();

$requests = get_purch_requisitions(
	$filter_requester == ALL_TEXT ? 0 : (int)$filter_requester,
	$filter_status,
	$filter_date_from !== '' ? date2sql($filter_date_from) : '',
	$filter_date_to !== '' ? date2sql($filter_date_to) : '',
	$filter_department == ALL_TEXT ? 0 : (int)$filter_department
);

while ($row = db_fetch($requests)) {
	$rows[] = $row;
	$department_key = $row['department_name'] ? $row['department_name'] : _('Unassigned');
	if (!isset($department_summary[$department_key])) {
		$department_summary[$department_key] = array(
			'count' => 0,
			'total' => 0,
		);
	}

	$department_summary[$department_key]['count']++;
	$department_summary[$department_key]['total'] += (float)$row['total_estimated'];
}

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

$statuses = array('' => _('All Statuses')) + get_purch_requisition_statuses();
echo '<td>'.array_selector('filter_status', $filter_status, $statuses, array('class' => array('nosearch'))).'</td>';

users_list_cells(_('Requester:'), 'filter_requester', $filter_requester, false, true);
departments_list_cells(_('Department:'), 'filter_department', $filter_department, false, true);

end_row();
start_row();

date_cells(_('from:'), 'filter_date_from', $filter_date_from, null, -user_transaction_days());
date_cells(_('to:'), 'filter_date_to', $filter_date_to);
submit_cells('SearchRequests', _('Apply Filter'), '', _('Filter requisitions'), 'default');
	echo '<td>';
	hyperlink_params($path_to_root . '/purchasing/purch_requisition_entry.php', _('New Purchase Requisition'), 'New=1');
	echo '</td>';

end_row();
end_table(1);

if (!empty($department_summary)) {
	echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin:12px 0;">';
	foreach ($department_summary as $department_name => $summary) {
		echo '<div style="background:#fff;border:1px solid #ddd;border-left:4px solid #0d6efd;padding:8px 14px;border-radius:4px;min-width:180px;">';
		echo '<div style="font-size:13px;color:#666;">' . htmlspecialchars($department_name) . '</div>';
		echo '<div style="font-size:18px;font-weight:bold;">' . (int)$summary['count'] . '</div>';
		echo '<div style="font-size:12px;color:#666;">' . _('Estimated Total: ') . price_format($summary['total']) . '</div>';
		echo '</div>';
	}
	echo '</div>';
}

start_table(TABLESTYLE, "width='100%'");
$th = array(
	'',
	_('Reference'),
	_('Status'),
	_('Priority'),
	_('Requester'),
	_('Department'),
	_('Location'),
	_('Request Date'),
	_('Required Date'),
	_('Lines'),
	_('Outstanding Qty'),
	_('Estimated Total'),
	''
);
table_header($th);

$k = 0;
foreach ($rows as $row) {
	alt_table_row_color($k);

	echo '<td align="center"><input type="checkbox" name="selected_req[]" value="' . (int)$row['id'] . '"></td>';
	label_cell('<a href="' . $path_to_root . '/purchasing/purch_requisition_entry.php?requisition_id=' . (int)$row['id'] . '">' . htmlspecialchars($row['reference']) . '</a>');
	label_cell(purch_requisition_status_badge($row['status']));
	label_cell(purch_requisition_priority_badge($row['priority']));
	label_cell($row['requester_name']);
	label_cell($row['department_name'] ? $row['department_name'] : '-');
	label_cell($row['location_name'] ? $row['location_name'] : $row['location']);
	label_cell(sql2date($row['request_date']));
	label_cell($row['required_date'] ? sql2date($row['required_date']) : '-');
	label_cell($row['line_count'], 'align=right');
	qty_cell($row['outstanding_quantity']);
	amount_cell($row['total_estimated']);
	label_cell('<a href="' . $path_to_root . '/purchasing/purch_requisition_entry.php?requisition_id=' . (int)$row['id'] . '">' . _('Open') . '</a>');
	end_row();
}

if ($k == 0)
	label_row('', _('No purchase requisitions matched the selected filters.'), 'colspan=13 align=center');

end_table(1);

if (!empty($rows)) {
	start_table(TABLESTYLE_NOBORDER, "width='60%'");
	text_row(_('Batch Reject Reason:'), 'batch_rejection_reason', null, 40, 255);
	end_table(1);

	echo '<div style="text-align:center;margin-top:14px;">';
	submit('BatchApprove', _('Batch Approve'), true, _('Approve selected requisitions'));
	echo '&nbsp;';
	submit('BatchReject', _('Batch Reject'), true, _('Reject selected requisitions'));
	echo '</div>';
}

end_form();
end_page();