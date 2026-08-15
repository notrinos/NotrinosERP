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
$page_security = 'SA_LOAN';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');

/**
 * Resolve one Loan Repayment Employee selector label at the page-level instant.
 *
 * Loan Repayment remains authorized by SA_LOAN, which deliberately does not
 * grant Person/Worker identity-read access. Canonical naming is additive only
 * when the same principal independently holds an approved identity-read area.
 * The submitted employee_id and repayment mutation keys remain exact legacy
 * values; this helper changes presentation only.
 *
 * @param array $row
 * @return string
 */
function loan_repayment_authoritative_employee_list($row) {
    global $loan_repayment_selector_as_of;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $loan_repayment_selector_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $loan_repayment_selector_as_of
    );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_label;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);
    if ($canonical_name === '')
        return $legacy_label;

    return (user_show_codes() ? ((string)$row[0].' - ') : '').$canonical_name;
}

$js = '';

if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Loan Repayment'), false, false, '', $js);

if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

foreach ($_POST as $name => $value) {
    if (strpos($name, 'Pay') === 0) {
        $repayment_id = (int)substr($name, 3);
        if ($repayment_id > 0) {
            $sql = "SELECT total_amount, paid_amount FROM ".TB_PREF."loan_repayments WHERE repayment_id = ".db_escape($repayment_id);
            $result = db_query($sql, 'could not get repayment amount');
            $row = db_fetch_assoc($result);
            if ($row) {
                $due = (float)$row['total_amount'] - (float)$row['paid_amount'];
                if (floatcmp($due, 0) > 0) {
                    apply_loan_repayment($repayment_id, $due, Today(), 0);
                    display_notification(_('Repayment has been marked as paid.'));
                }
            }
        }
    }
}

$loan_repayment_selector_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('employee_loan_repayment_selector');

start_form();

start_table(TABLESTYLE2);
start_row();
label_cell(_('Employee:'));
employees_list_cells(null, 'employee_id', null, true, true, false, false, array(
    'layout_class' => 'combo-layout-equal',
    'format' => 'loan_repayment_authoritative_employee_list'
));
end_row();
date_row(_('From Date:'), 'from_date');
date_row(_('To Date:'), 'to_date');
end_table(1);
submit_center('Refresh', _('Refresh'));
br();

if ($_POST['employee_id'] != '' && $_POST['employee_id'] != ALL_TEXT) {
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Repayment ID'), _('Loan ID'), _('Installment #'), _('Due Date'), _('Total Amount'), _('Paid Amount'), _('Outstanding'), _('Status'), '');
    table_header($th);

    $result = get_due_loan_repayments($_POST['employee_id'], $_POST['from_date'], $_POST['to_date']);
    $k = 0;
    while ($row = db_fetch($result)) {
        $due = (float)$row['total_amount'] - (float)$row['paid_amount'];
        alt_table_row_color($k);
        label_cell($row['repayment_id']);
        label_cell($row['loan_id']);
        label_cell($row['installment_no']);
        label_cell(sql2date($row['due_date']));
        amount_cell($row['total_amount']);
        amount_cell($row['paid_amount']);
        amount_cell($due);
        label_cell((int)$row['status'] == 1 ? _('Paid') : ((int)$row['status'] == 2 ? _('Overdue') : _('Scheduled')));
        if (floatcmp($due, 0) > 0)
            submit_cells('Pay'.$row['repayment_id'], _('Mark Paid'));
        else
            label_cell('');
        end_row();
    }

    end_table(1);

    $total_due = get_total_due_loan_deduction($_POST['employee_id'], $_POST['from_date'], $_POST['to_date']);
    display_note(_('Total due in selected period: ').price_format($total_due), 0, 1);
}

end_form();
end_page();

