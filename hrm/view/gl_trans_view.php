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
$page_security = 'SA_EMPLOYEETRANSVIEW';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');
page(_("GL Transaction View"));

if (!isset($_GET['type_id']) || !isset($_GET['trans_no'])) {
    display_error(_('This view requires transaction type and transaction number.'));
    end_page();
    return;
}

$type_id = (int)$_GET['type_id'];
$trans_no = (int)$_GET['trans_no'];

$result = get_gl_trans($type_id, $trans_no);
if (!$result || db_num_rows($result) == 0) {
    display_note(_('No GL rows were found for this transaction.'), 0, 1);
    end_page();
    return;
}

$dim = (int)get_company_pref('use_dimension');
$th = array(_('Date'), _('Account Code'), _('Account Name'));
if ($dim >= 1)
    $th[] = _('Dimension 1');
if ($dim >= 2)
    $th[] = _('Dimension 2');
$th[] = _('Debit');
$th[] = _('Credit');
$th[] = _('Memo');

display_heading(_('Transaction Type: ').$type_id.' #'.$trans_no);
start_table(TABLESTYLE, "width='95%'");
table_header($th);

$k = 0;
$debit_total = 0;
$credit_total = 0;
while ($row = db_fetch($result)) {
    if ((float)$row['amount'] == 0)
        continue;

    alt_table_row_color($k);
    label_cell(sql2date($row['tran_date']));
    label_cell($row['account']);
    label_cell($row['account_name']);
    if ($dim >= 1)
        label_cell(get_dimension_string($row['dimension_id'], true));
    if ($dim >= 2)
        label_cell(get_dimension_string($row['dimension2_id'], true));

    display_debit_or_credit_cells($row['amount']);
    label_cell($row['memo_']);
    end_row();

    if ($row['amount'] > 0)
        $debit_total += $row['amount'];
    else
        $credit_total += $row['amount'];
}

start_row("class='inquirybg' style='font-weight:bold'");
label_cell(_('Total'), 'colspan='.($dim >= 2 ? 5 : ($dim == 1 ? 4 : 3)));
amount_cell($debit_total);
amount_cell(-$credit_total);
label_cell('');
end_row();
end_table(1);

end_page();

