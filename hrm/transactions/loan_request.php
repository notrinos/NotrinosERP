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
include_once($path_to_root . '/hrm/includes/db/loan_type_db.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');

/**
 * Resolve one Loan Request history Employee name at the page-level instant.
 *
 * Loan Request remains authorized by SA_LOAN, which is deliberately not a
 * Person/Worker identity-read capability. Canonical naming is additive only
 * when the same principal independently holds an existing approved identity
 * read area. Approval draft, loan persistence and terminal workflow paths keep
 * using exact legacy identity values.
 *
 * @param string $employee_ref
 * @param string $legacy_name
 * @return string
 */
function loan_request_authoritative_history_name($employee_ref, $legacy_name) {
    global $loan_request_history_as_of;

    $legacy_name = (string)$legacy_name;
    if (trim((string)$employee_ref) === '' || $loan_request_history_as_of === false)
        return $legacy_name;

    $identity = get_hrm_person_worker_report_name_as_of(
        $employee_ref, $loan_request_history_as_of
    );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_name;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);

    return $canonical_name === '' ? $legacy_name : $canonical_name;
}

/**
 * Resolve current username/login for audit fields.
 *
 * @return string
 */
function current_hrm_user_login() {
    if (!isset($_SESSION['wa_current_user']))
        return '';

    $user = $_SESSION['wa_current_user'];
    if (!empty($user->loginname))
        return $user->loginname;
    if (!empty($user->username))
        return $user->username;

    return '';
}

$js = '';

if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = "Loan Request"), false, false, '', $js);

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif ((int)$_POST['loan_type_id'] <= 0) {
        display_error(_('Loan type is required.'));
        set_focus('loan_type_id');
    } elseif (!is_date($_POST['loan_date']) || !is_date($_POST['first_repayment'])) {
        display_error(_('Loan date and first repayment date are required.'));
    } elseif (date1_greater_date2($_POST['loan_date'], $_POST['first_repayment'])) {
        display_error(_('First repayment date must be on or after the loan date.'));
        set_focus('first_repayment');
    } elseif ((int)$_POST['installments'] <= 0) {
        display_error(_('Installments must be greater than zero.'));
        set_focus('installments');
    } elseif (input_num('loan_amount') <= 0) {
        display_error(_('Loan amount must be greater than zero.'));
        set_focus('loan_amount');
    } elseif (input_num('interest_rate') < 0) {
        display_error(_('Interest rate cannot be negative.'));
        set_focus('interest_rate');
    } elseif ((int)$_POST['installments'] > 360) {
        display_error(_('Installments cannot exceed 360.'));
        set_focus('installments');
    } else {
        $loan_type = get_loan_type((int)$_POST['loan_type_id']);
        $interest_rate = input_num('interest_rate');
        if ($interest_rate == 0 && $loan_type)
            $interest_rate = (float)$loan_type['interest_rate'];

        if ($loan_type && $loan_type['max_amount'] !== null && $loan_type['max_amount'] > 0
            && input_num('loan_amount') > (float)$loan_type['max_amount']) {
            display_error(sprintf(_('Loan amount exceeds the maximum of %s for this loan type.'),
                price_format((float)$loan_type['max_amount'])));
            set_focus('loan_amount');
        } elseif ($loan_type && $loan_type['max_installments'] !== null && $loan_type['max_installments'] > 0
            && (int)$_POST['installments'] > (int)$loan_type['max_installments']) {
            display_error(sprintf(_('Installments exceed the maximum of %d for this loan type.'),
                (int)$loan_type['max_installments']));
            set_focus('installments');
        } else {

        $installments = max((int)$_POST['installments'], 1);
        $total = input_num('loan_amount') + (input_num('loan_amount') * $interest_rate / 100);
        $installment_amount = round2($total / $installments, user_price_dec());

        if ($selected_id != '') {
            if (has_pending_loan_request_approval((int)$selected_id)) {
                display_error(_('This loan request is pending core approval and cannot be edited.'));
                set_focus('selected_id');
            } else {
                update_employee_loan(
                    $selected_id,
                    (int)$_POST['loan_type_id'],
                    input_num('loan_amount'),
                    $interest_rate,
                    $installments,
                    $installment_amount,
                    $_POST['loan_date'],
                    $_POST['first_repayment'],
                    $_POST['notes']
                );
                display_notification(_('Loan request has been updated.'));
            }
        } else {
            begin_transaction();
            $created_loan_id = add_employee_loan(
                $_POST['employee_id'],
                (int)$_POST['loan_type_id'],
                input_num('loan_amount'),
                $interest_rate,
                $installments,
                $installment_amount,
                $_POST['loan_date'],
                $_POST['first_repayment'],
                $_POST['notes']
            );

            if (!$created_loan_id) {
                cancel_transaction();
                display_error(_('Loan request was created but could not be submitted to core approval.'));
            } else {
                $approval_result = submit_employee_loan_for_core_approval($created_loan_id);
                if (!$approval_result) {
                    cancel_transaction();
                    display_error(_('Loan request could not be submitted to the core approval workflow.'));
                } elseif (isset($approval_result['status']) && $approval_result['status'] === 'pending') {
                    commit_transaction();
                    display_notification(_('Loan request has been submitted to the core approval workflow.'));
                } elseif (isset($approval_result['status']) && $approval_result['status'] === 'auto_approved') {
                    commit_transaction();
                    display_notification(_('Loan request has been auto-approved by the core approval workflow.'));
                } else {
                    cancel_transaction();
                    display_error(_('Loan request approval returned an unexpected result. No loan request was created.'));
                }
            }
        }

        $Mode = 'RESET';
        } // end max_amount/max_installments else
    }
}

if ($Mode == 'Delete') {
    $core_draft = find_approval_draft_for_hrm_request(
        ST_LOAN_REQUEST,
        (int)$selected_id
    );
    if ($core_draft) {
        $approval_service = get_approval_workflow_service();
        $result = $approval_service->cancel(
            (int)$core_draft['draft_id'],
            _('Cancelled by the original loan requester.')
        );
        if (isset($result['status']) && $result['status'] === 'cancelled')
            display_notification(_('The loan request and its approval draft were cancelled.'));
        else
            display_error(isset($result['message'])
                ? $result['message']
                : _('The loan request could not be cancelled.'));
    } else {
        if (!delete_employee_loan($selected_id))
            display_error(_('Only pending loans can be deleted.'));
        else
            display_notification(_('Selected loan has been deleted.'));
    }

    $Mode = 'RESET';
}

foreach ($_POST as $name => $value) {
    if (strpos($name, 'Submit') === 0) {
        $loan_id = (int)substr($name, 6);
        if ($loan_id > 0) {
            $approval_result = reconcile_pending_loan_request_approval($loan_id);
            if (isset($approval_result['message'])
                && isset($approval_result['status'])
                && in_array(
                    $approval_result['status'],
                    array('pending', 'auto_approved', 'already_pending'),
                    true
                )
            ) {
                display_notification($approval_result['message']);
            } else {
                display_error(isset($approval_result['message'])
                    ? $approval_result['message']
                    : _('Loan request could not be submitted to core approval workflow.'));
            }

            $Mode = 'RESET';
        }
    }
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['employee_id'] = '';
    $_POST['loan_type_id'] = 0;
    $_POST['loan_amount'] = 0;
    $_POST['interest_rate'] = 0;
    $_POST['installments'] = 1;
    $_POST['loan_date'] = Today();
    $_POST['first_repayment'] = Today();
    $_POST['notes'] = '';
}

$status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));

$loan_request_history_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('employee_loan_request_history');

start_form();

start_outer_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_employee_loan($selected_id);
    if ($myrow && (int)$myrow['status'] == 0) {
        $_POST['employee_id'] = $myrow['employee_id'];
        $_POST['loan_type_id'] = $myrow['loan_type_id'];
        $_POST['loan_amount'] = qty_format($myrow['loan_amount']);
        $_POST['interest_rate'] = qty_format($myrow['interest_rate']);
        $_POST['installments'] = $myrow['installments'];
        $_POST['loan_date'] = sql2date($myrow['loan_date']);
        $_POST['first_repayment'] = sql2date($myrow['first_repayment']);
        $_POST['notes'] = $myrow['notes'];
        hidden('selected_id', $selected_id);
    }
}
table_section(1);
start_row();
label_cell(_('Employee:'));
employees_list_cells(null, 'employee_id', null, false, false, false, false, array('layout_class' => 'combo-layout-equal'));
end_row();
loan_types_list_row(_('Loan Type:'), 'loan_type_id');
amount_row(_('Loan Amount:'), 'loan_amount');
amount_row(_('Interest Rate (%):'), 'interest_rate');

table_section(2);
small_amount_row(_('Installments:'), 'installments', get_post('installments', 1), 1, 360);
date_row(_('Loan Date:'), 'loan_date');
date_row(_('First Repayment Date:'), 'first_repayment');
textarea_row(_('Notes:'), 'notes', null, 50, 3);

end_outer_table();
submit_add_or_update_center($selected_id == '', '', 'both');

br();

start_table(TABLESTYLE, "width='100%'");
$th = array(_('ID'), _('Employee'), _('Loan Type'), _('Amount'), _('Outstanding'), _('Installments'), _('Loan Date'), _('Status'), '', '', '');
table_header($th);

$result = get_employee_loans();
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['loan_id']);
    label_cell($row['employee_id'].' '.loan_request_authoritative_history_name(
        $row['employee_id'], $row['employee_name']
    ));
    label_cell($row['loan_type_name']);
    amount_cell($row['loan_amount']);
    amount_cell($row['outstanding_amount']);
    label_cell($row['installments']);
    label_cell(sql2date($row['loan_date']));
    $row_status = $status_labels[(int)$row['status']];
    $has_pending_core_approval = ((int)$row['status'] === 0) ? has_pending_loan_request_approval((int)$row['loan_id']) : false;
    if ($has_pending_core_approval)
        $row_status .= ' (' . _('Pending Core Approval') . ')';
    label_cell($row_status);

    if ((int)$row['status'] == 0 && !$has_pending_core_approval) {
        edit_button_cell('Edit'.$row['loan_id'], _('Edit'));
        delete_button_cell('Delete'.$row['loan_id'], _('Delete'));
        submit_cells('Submit'.$row['loan_id'], _('Submit For Core Approval'));
    } else {
        label_cell('');
        label_cell('');
        label_cell('');
    }

    end_row();
}
end_table(1);

end_form();
end_page();
