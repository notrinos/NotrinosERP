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
$page_security = 'SA_LOANREPORT';
$path_to_root = "../..";
include_once($path_to_root . '/includes/db_pager.inc');
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Loan Outstanding"), false, false, '', $js);

/**
 * Format loan status code for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function loan_report_status_label($row, $cell) {
    $status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));
    $status = (int)$cell;

    return isset($status_labels[$status]) ? $status_labels[$status] : $cell;
}

/**
 * Resolve one Loan Outstanding inquiry Employee label at the page-level instant.
 *
 * The exact legacy pager cell remains the fallback. Only accepted canonical-link
 * evidence may replace its name suffix, while the legacy employee identifier
 * prefix remains unchanged.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function loan_report_authoritative_employee($row, $cell) {
    global $loan_report_as_of;

    $legacy_label = (string)$cell;
    if (!is_array($row) || !isset($row['employee_id'])
        || trim((string)$row['employee_id']) === '')
        return $legacy_label;

    $identity = $loan_report_as_of === false
        ? false
        : get_hrm_person_worker_report_name_as_of(
            $row['employee_id'], $loan_report_as_of
        );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_label;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);
    if ($canonical_name === '')
        return $legacy_label;

    return (string)$row['employee_id'].' '.$canonical_name;
}

/**
 * Resolve one Loan Outstanding Employee selector label at the page instant.
 *
 * The shared employee-list formatter remains the exact fail-closed fallback.
 * SA_LOANREPORT itself is not identity-read authority; canonical identity is
 * presentation-only for principals that independently hold an approved
 * Person/Worker read capability.
 *
 * @param array $row
 * @return string
 */
function loan_report_authoritative_employee_list($row) {
    global $loan_report_as_of;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $loan_report_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $loan_report_as_of
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

$loan_report_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('loan_report_inquiry_selector');

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
employees_list_cells(_('Employee:'), 'employee_id', null, true, false, false, false, array(
    'format' => 'loan_report_authoritative_employee_list'
));
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$employee_id = get_post('employee_id', '');

$sql = "SELECT l.loan_id,
        CONCAT(l.employee_id, ' ', TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')))) employee_label,
        l.employee_id,
        lt.loan_type_name,
        l.loan_date,
        l.loan_amount,
        l.outstanding_amount,
        l.installments,
        l.status
    FROM ".TB_PREF."employee_loans l
    LEFT JOIN ".TB_PREF."loan_types lt ON lt.loan_type_id = l.loan_type_id
    LEFT JOIN ".TB_PREF."employees e ON e.employee_id = l.employee_id
    WHERE 1=1";

if ($employee_id !== '' && $employee_id !== ALL_TEXT)
    $sql .= " AND l.employee_id = ".db_escape($employee_id);

$sql .= " ORDER BY l.loan_date DESC, l.loan_id DESC";

hrm_log_restricted_employee_projection('loan_report_inquiry');

$cols = array(
    _('Loan ID') => array('name' => 'loan_id', 'ord' => 'desc'),
    _('Employee') => array('name' => 'employee_label', 'fun' => 'loan_report_authoritative_employee', 'ord' => ''),
    _('Loan Type') => array('name' => 'loan_type_name', 'ord' => ''),
    _('Loan Date') => array('name' => 'loan_date', 'type' => 'date', 'ord' => ''),
    _('Loan Amount') => array('name' => 'loan_amount', 'type' => 'amount'),
    _('Outstanding') => array('name' => 'outstanding_amount', 'type' => 'amount'),
    _('Installments') => array('name' => 'installments', 'ord' => ''),
    _('Status') => array('name' => 'status', 'fun' => 'loan_report_status_label', 'ord' => '')
);

$table =& new_db_pager('loan_report_tbl', $sql, $cols);
$table->width = '100%';
display_db_pager($table);

end_form();

end_page();

