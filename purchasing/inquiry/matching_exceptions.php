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

$page_security = 'SA_PURCHMATCHEXCEPTIONS';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

page(_($help_context = 'Purchase Matching Exceptions'), false, false, '', $js);

/**
 * Return exception type display options.
 *
 * @return array
 */
function get_matching_exception_type_options()
{
    return array(
        ALL_TEXT => _('All Types'),
        'price_variance' => _('Price Variance'),
        'quantity_variance' => _('Quantity Variance'),
        'total_variance' => _('Total Variance'),
        'missing_grn' => _('Missing GRN'),
        'missing_po' => _('Missing PO'),
    );
}

/**
 * Return status display options.
 *
 * @return array
 */
function get_matching_exception_status_options()
{
    return array(
        ALL_TEXT => _('All Statuses'),
        'open' => _('Open'),
        'approved' => _('Approved'),
        'rejected' => _('Rejected'),
        'resolved' => _('Resolved'),
    );
}

$approve_id = find_submit('Approve');
$reject_id = find_submit('Reject');
$resolve_id = find_submit('Resolve');

if ($approve_id > 0) {
    approve_exception($approve_id, (int)$_SESSION['wa_current_user']->user, trim(get_post('resolution_notes_' . $approve_id)));
    display_notification(_('Exception has been approved.'));
}

if ($reject_id > 0) {
    reject_exception($reject_id, (int)$_SESSION['wa_current_user']->user, trim(get_post('resolution_notes_' . $reject_id)));
    display_notification(_('Exception has been rejected.'));
}

if ($resolve_id > 0) {
    resolve_exception($resolve_id, (int)$_SESSION['wa_current_user']->user, trim(get_post('resolution_notes_' . $resolve_id)));
    display_notification(_('Exception has been resolved.'));
}

$filter_status = get_post('filter_status', 'open');
$filter_supplier_id = (int)get_post('filter_supplier_id', 0);
$filter_type = get_post('filter_type', ALL_TEXT);
$filter_from = get_post('filter_from', begin_month(Today()));
$filter_to = get_post('filter_to', Today());

start_form();

start_table(TABLESTYLE2, "width='100%'");
start_row();
    label_cell(_('Status:'));
    echo "<td>" . array_selector('filter_status', $filter_status, get_matching_exception_status_options(), array('class' => array('nosearch'))) . "</td>";
    supplier_list_cells(_('Supplier:'), 'filter_supplier_id', $filter_supplier_id, true, true, false, true);
    label_cell(_('Exception Type:'));
    echo "<td>" . array_selector('filter_type', $filter_type, get_matching_exception_type_options(), array('class' => array('nosearch'))) . "</td>";
end_row();
start_row();
    date_cells(_('From:'), 'filter_from', $filter_from);
    date_cells(_('To:'), 'filter_to', $filter_to);
    submit_cells('SearchExceptions', _('Search'), '', _('Apply filters'), 'default');
end_row();
end_table(1);

$result = get_matching_exceptions($filter_status, $filter_supplier_id, $filter_type, $filter_from, $filter_to);

display_heading(_('Matching Exception List'));
start_table(TABLESTYLE, "width='100%'");
$th = array(
    _('Created'),
    _('Status'),
    _('Type'),
    _('Supplier'),
    _('Document'),
    _('Item'),
    _('Expected'),
    _('Actual'),
    _('Variance'),
    _('Notes'),
    _('Action'),
);
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);

    $doc_label = ($row['trans_type'] > 0 && $row['trans_no'] > 0)
        ? get_trans_view_str($row['trans_type'], $row['trans_no'], $row['trans_no'])
        : '';

    label_cell(sql2date(substr($row['created_date'], 0, 10)));
    label_cell(ucfirst($row['status']));
    label_cell(isset(get_matching_exception_type_options()[$row['exception_type']]) ? get_matching_exception_type_options()[$row['exception_type']] : $row['exception_type']);
    label_cell($row['supp_name']);
    label_cell($doc_label, "nowrap");
    label_cell($row['stock_id']);
    amount_cell($row['expected_value']);
    amount_cell($row['actual_value']);
    label_cell(number_format2($row['variance_amount'], 2) . ' / ' . number_format2($row['variance_percent'], 2) . '%');

    text_cells(null, 'resolution_notes_' . $row['id'], $row['resolution_notes'], 20, 120);

    if ($row['status'] === 'open') {
        echo "<td nowrap>";
        submit('Approve' . $row['id'], _('Approve'), false, _('Approve exception'));
        echo '&nbsp;';
        submit('Reject' . $row['id'], _('Reject'), false, _('Reject exception'));
        echo '&nbsp;';
        submit('Resolve' . $row['id'], _('Resolve'), false, _('Mark as resolved'));
        echo "</td>";
    } else
        label_cell('');

    end_row();
}
end_table(1);

end_form();
end_page();
