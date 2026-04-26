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

/*
    Phase 7: Discount Effectiveness Report.
    Shows discount program usage, amounts, and ROI.
    Uses HTML page pattern (consistent with other custom reports in this codebase).
*/

$page_security = 'SA_SALESREPORT';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

page(_($help_context = 'Discount Effectiveness'));

$date_from = get_post('date_from', begin_fiscalyear());
$date_to   = get_post('date_to', Today());

$data = get_discount_effectiveness_report($date_from, $date_to);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
submit_cells('RefreshDiscount', _('Apply Filter'), '', _('Refresh discount analysis'), 'default');
end_row();
end_table(1);

display_heading(_('Discount Effectiveness'));

if (empty($data)) {
    display_note(_('No discount programs found. Enable discount programs (Phase 4) to see usage data.'));
} else {
    start_table(TABLESTYLE, "width='100%'");
    table_header(array(_('Program Name'), _('Type'), _('Usage Count'), _('Total Discount'), _('Avg Discount')));

    $total_usage    = 0;
    $total_discount = 0;
    $k = 0;
    foreach ($data as $row) {
        alt_table_row_color($k);
        label_cell($row['program_name']);
        label_cell(ucfirst($row['program_type']));
        label_cell((int)$row['usage_count'], 'align=right');
        amount_cell($row['total_discount']);
        $avg = $row['usage_count'] > 0 ? $row['total_discount'] / $row['usage_count'] : 0;
        amount_cell($avg);
        end_row();
        $total_usage    += $row['usage_count'];
        $total_discount += $row['total_discount'];
    }

    // Totals row
    start_row();
    label_cell('<strong>'._('Total').'</strong>');
    label_cell('');
    label_cell('<strong>'.(int)$total_usage.'</strong>', 'align=right');
    amount_cell($total_discount);
    $grand_avg = $total_usage > 0 ? $total_discount / $total_usage : 0;
    amount_cell($grand_avg);
    end_row();

    end_table(1);
}

end_form();
end_page();
