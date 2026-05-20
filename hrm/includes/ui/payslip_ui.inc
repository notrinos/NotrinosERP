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

function get_employee_details_to_payslip($order, $employee_id) {

	$error = '';
	if ($employee_id === '' || $employee_id === null || $employee_id === ALL_TEXT)
		return _('The selected employee is invalid.');

	$employee_row = get_employee_to_payslip($employee_id);
	if (!$employee_row)
		return _('The selected employee was not found.');

	if($employee_row['position_id'] == 0)
		$error = _('The selected employee does not have a job position, please select another employee or set job position for the selected employee.');

	$order->set_employee($employee_id, $employee_row['name']);

	return $error;
}

/**
 * Build a preview payslip document for the current employee and date range.
 *
 * @param hrm_cart $order
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return array Array(doc, error_message)
 */
function build_payslip_preview($order, $employee_id, $from_date, $to_date) {
	$employee_id = trim((string)$employee_id);
	if ($employee_id === '' || $employee_id === ALL_TEXT)
		return array(null, '');

	if (!is_date($from_date) || !is_date($to_date))
		return array(null, '');

	if (date_comp($from_date, $to_date) > 0)
		return array(null, _('End date cannot be before the start date.'));

	$employee = get_employee_by_code($employee_id);
	if (!$employee)
		return array(null, _('The selected employee was not found.'));

	if (!employee_has_position($employee_id))
		return array(null, _('The selected employee does not have a job position, please select another employee or set job position for the selected employee.'));

	$salary_components = function_exists('get_employee_salary_components')
		? get_employee_salary_components($employee_id, date2sql($to_date))
		: array();

	if (empty($salary_components))
		return array(null, _('No salary components were found for the selected employee and period. Configure a salary structure or personal salary first.'));

	$preview_doc = calculate_employee_payslip(
		$employee,
		$from_date,
		$to_date,
		0,
		array('salary_components' => array($employee_id => $salary_components))
	);

	if (!$preview_doc)
		return array(null, _('Could not calculate payslip preview for the selected employee.'));

	$preview_doc->reference = get_post('reference');
	$preview_doc->tran_date = get_post('tran_date', $to_date);
	$preview_doc->comments = get_post('Comments');

	return array($preview_doc, '');
}

/**
 * Get the active preview payslip document from the order/cart context.
 *
 * @param mixed $order
 * @return payslip_doc|false
 */
function get_payslip_display_doc($order) {
	if (is_object($order) && $order instanceof payslip_doc)
		return $order;

	if (is_object($order) && isset($order->payslip_preview) && is_object($order->payslip_preview) && $order->payslip_preview instanceof payslip_doc)
		return $order->payslip_preview;

	return false;
}

/**
 * Render page-local styling for the payslip entry workspace.
 *
 * @return void
 */
function render_payslip_entry_styles() {
	echo <<<EOT
<style>
.payslip-entry-page {
	max-width: 1280px;
	margin: 0 auto;
	padding-bottom: 24px;
}

.payslip-entry-hero {
	display: flex;
	justify-content: space-between;
	gap: 24px;
	flex-wrap: wrap;
	padding: 28px;
	margin-bottom: 18px;
	border-radius: 22px;
	background: linear-gradient(135deg, #0f766e 0%, #155e75 58%, #f59e0b 150%);
	color: #fff;
	box-shadow: 0 18px 42px rgba(15, 23, 42, 0.16);
}

.payslip-entry-hero-copy {
	max-width: 620px;
}

.payslip-entry-eyebrow {
	display: inline-block;
	margin-bottom: 10px;
	padding: 5px 12px;
	border-radius: 999px;
	background: rgba(255, 255, 255, 0.14);
	font-size: 0.78rem;
	font-weight: 700;
	letter-spacing: 0.08em;
	text-transform: uppercase;
}

.payslip-entry-hero h2 {
	margin: 0 0 10px;
	font-size: 2rem;
	line-height: 1.1;
}

.payslip-entry-hero p {
	margin: 0;
	max-width: 58ch;
	color: rgba(255, 255, 255, 0.88);
	line-height: 1.6;
}

.payslip-entry-hero-stats {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
	gap: 12px;
	flex: 1 1 360px;
	align-self: stretch;
}

.payslip-entry-hero-stat,
.payslip-entry-stat,
.payslip-entry-key-value,
.payslip-entry-empty,
.payslip-entry-detail-table tbody tr,
.payslip-entry-mini-table tbody tr {
	transition: background 0.2s ease, transform 0.2s ease, border-color 0.2s ease;
}

.payslip-entry-hero-stat {
	padding: 16px 18px;
	border: 1px solid rgba(255, 255, 255, 0.18);
	border-radius: 18px;
	background: rgba(255, 255, 255, 0.1);
	backdrop-filter: blur(10px);
}

.payslip-entry-hero-stat-label,
.payslip-entry-stat-label,
.payslip-entry-key-value span {
	display: block;
	font-size: 0.8rem;
	font-weight: 700;
	letter-spacing: 0.04em;
	text-transform: uppercase;
	color: #5f6b7a;
}

.payslip-entry-hero-stat-label {
	color: rgba(255, 255, 255, 0.72);
}

.payslip-entry-hero-stat-value,
.payslip-entry-stat strong,
.payslip-entry-key-value strong {
	display: block;
	margin-top: 8px;
	font-size: 1.2rem;
	font-weight: 700;
	color: inherit;
}

.payslip-entry-form-panel,
.payslip-entry-panel {
	margin-bottom: 18px;
	padding: 22px;
	border: 1px solid #d7e2ea;
	border-radius: 20px;
	background: linear-gradient(180deg, #ffffff 0%, #f8fbfd 100%);
	box-shadow: 0 12px 32px rgba(15, 23, 42, 0.08);
}

.payslip-entry-form-panel .form-grid {
	gap: 18px;
}

.payslip-entry-form-panel .form-section {
	padding: 18px;
	border-radius: 18px;
	background: #f4f8fb;
	border: 1px solid #e2e8ef;
}

.payslip-entry-form-panel .form-row {
	min-height: 68px;
	padding: 10px 0;
	border-bottom: 1px solid #e4edf3;
}

.payslip-entry-form-panel .form-row:last-child {
	border-bottom: 0;
	padding-bottom: 0;
}

.payslip-entry-grid {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
	gap: 16px;
	margin-bottom: 18px;
}

.payslip-entry-panel-head {
	display: flex;
	justify-content: space-between;
	gap: 12px;
	align-items: baseline;
	margin-bottom: 16px;
}

.payslip-entry-panel-head h3 {
	margin: 0;
	font-size: 1.05rem;
	color: #0f172a;
}

.payslip-entry-panel-head p {
	margin: 0;
	font-size: 0.92rem;
	color: #64748b;
	line-height: 1.5;
}

.payslip-entry-stats-grid,
.payslip-entry-context-grid {
	display: grid;
	grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
	gap: 12px;
}

.payslip-entry-stat,
.payslip-entry-key-value {
	padding: 14px 16px;
	border-radius: 16px;
	border: 1px solid #dce6ee;
	background: #fff;
}

.payslip-entry-stat.is-accent {
	background: linear-gradient(135deg, #ecfeff 0%, #cffafe 100%);
	border-color: #a5f3fc;
	color: #155e75;
}

.payslip-entry-stat.is-success {
	background: linear-gradient(135deg, #ecfdf5 0%, #d1fae5 100%);
	border-color: #a7f3d0;
	color: #166534;
}

.payslip-entry-stat.is-warn {
	background: linear-gradient(135deg, #fff7ed 0%, #ffedd5 100%);
	border-color: #fdba74;
	color: #9a3412;
}

.payslip-entry-detail-table,
.payslip-entry-mini-table {
	width: 100%;
	border-collapse: separate;
	border-spacing: 0;
	font-size: 0.95rem;
}

.payslip-entry-detail-table th,
.payslip-entry-mini-table th {
	padding: 13px 14px;
	border-bottom: 1px solid #dbe4ea;
	background: #eff5f8;
	font-size: 0.8rem;
	font-weight: 700;
	letter-spacing: 0.04em;
	text-transform: uppercase;
	color: #475569;
	text-align: left;
}

.payslip-entry-detail-table th.is-right,
.payslip-entry-mini-table th.is-right,
.payslip-entry-detail-table td.is-right,
.payslip-entry-mini-table td.is-right {
	text-align: right;
}

.payslip-entry-detail-table td,
.payslip-entry-mini-table td {
	padding: 14px;
	border-bottom: 1px solid #e8eef3;
	color: #1e293b;
	vertical-align: middle;
	background: #fff;
}

.payslip-entry-detail-table tbody tr:hover td,
.payslip-entry-mini-table tbody tr:hover td {
	background: #f8fafc;
}

.payslip-entry-detail-table tfoot td {
	padding: 14px;
	font-weight: 700;
	border-top: 1px solid #cbd5e1;
	background: #f8fbfd;
	color: #0f172a;
}

.payslip-entry-badge {
	display: inline-flex;
	align-items: center;
	padding: 5px 10px;
	border-radius: 999px;
	font-size: 0.76rem;
	font-weight: 700;
	letter-spacing: 0.04em;
	text-transform: uppercase;
	white-space: nowrap;
}

.payslip-entry-badge-basic {
	background: #cffafe;
	color: #155e75;
}

.payslip-entry-badge-earning {
	background: #dcfce7;
	color: #166534;
}

.payslip-entry-badge-deduction {
	background: #fee2e2;
	color: #991b1b;
}

.payslip-entry-muted {
	display: inline-block;
	margin-left: 6px;
	font-size: 0.82rem;
	color: #64748b;
}

.payslip-entry-empty {
	padding: 14px 16px;
	border-radius: 16px;
	border: 1px dashed #cbd5e1;
	background: #f8fafc;
	color: #64748b;
	line-height: 1.6;
}

.payslip-entry-page .form-actions {
	margin: 14px 0 18px;
	justify-content: center !important;
}

.payslip-entry-page .inputsubmit,
.payslip-entry-page .ajaxsubmit {
	min-width: 220px;
	border-radius: 999px;
	box-shadow: 0 10px 20px rgba(15, 23, 42, 0.1);
}

.payslip-entry-page textarea {
	min-height: 118px;
}

@media (max-width: 900px) {
	.payslip-entry-hero {
		padding: 22px;
	}

	.payslip-entry-hero h2 {
		font-size: 1.6rem;
	}

	.payslip-entry-form-panel,
	.payslip-entry-panel {
		padding: 18px;
	}

	.payslip-entry-detail-table,
	.payslip-entry-mini-table {
		font-size: 0.9rem;
	}
}

@media (max-width: 640px) {
	.payslip-entry-hero,
	.payslip-entry-form-panel,
	.payslip-entry-panel {
		padding: 16px;
		border-radius: 18px;
	}

	.payslip-entry-hero-stats,
	.payslip-entry-grid,
	.payslip-entry-stats-grid,
	.payslip-entry-context-grid {
		grid-template-columns: 1fr;
	}

	.payslip-entry-detail-table,
	.payslip-entry-mini-table {
		display: block;
		overflow-x: auto;
	}
}
</style>
EOT;
}

/**
 * Escape one payslip UI string for HTML output.
 *
 * @param mixed $value
 * @return string
 */
function payslip_escape($value) {
	return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
}

/**
 * Format day/hour style measures for payslip preview output.
 *
 * @param float|int $value
 * @param string $suffix
 * @return string
 */
function payslip_format_measure($value, $suffix='') {
	$formatted = number_format2((float)$value, 2);
	return $suffix !== '' ? $formatted.' '.$suffix : $formatted;
}

/**
 * Render the employee and period context panel.
 *
 * @param payslip_doc $doc
 * @return void
 */
function display_payslip_context_panel($doc) {
	$employee = is_array($doc->employee) ? $doc->employee : array();
	$employee_name = '';
	if (isset($employee['employee_name']) && trim($employee['employee_name']) !== '')
		$employee_name = trim($employee['employee_name']);
	elseif (isset($employee['name']) && trim($employee['name']) !== '')
		$employee_name = trim($employee['name']);
	else
		$employee_name = trim($doc->employee_id);

	$context_rows = array(
		array(_('Employee ID'), $doc->employee_id),
		array(_('Period'), trim($doc->from_date.' - '.$doc->to_date)),
		array(_('Department'), isset($employee['department_name']) ? $employee['department_name'] : ''),
		array(_('Position'), isset($employee['position_name']) ? $employee['position_name'] : ''),
		array(_('Grade'), isset($employee['grade_name']) ? $employee['grade_name'] : ''),
	);

	if (!empty($employee['hire_date']) && $employee['hire_date'] !== '0000-00-00')
		$context_rows[] = array(_('Hire Date'), sql2date($employee['hire_date']));

	if (!empty($doc->currency_code))
		$context_rows[] = array(_('Currency'), $doc->currency_code);

	if (!empty($doc->exchange_rate) && (float)$doc->exchange_rate != 1)
		$context_rows[] = array(_('Exchange Rate'), number_format2($doc->exchange_rate, user_exrate_dec()));

	echo "<section class='payslip-entry-panel'>";
	echo "<div class='payslip-entry-panel-head'><div><h3>".payslip_escape($employee_name)."</h3><p>".payslip_escape(_('Employee context used for this preview.'))."</p></div></div>";
	echo "<div class='payslip-entry-context-grid'>";
	foreach ($context_rows as $row) {
		if (trim((string)$row[1]) === '')
			continue;
		echo "<div class='payslip-entry-key-value'><span>".payslip_escape($row[0])."</span><strong>".payslip_escape($row[1])."</strong></div>";
	}
	echo "</div>";
	echo "</section>";
}

/**
 * Render salary total cards for the preview.
 *
 * @param payslip_doc $doc
 * @return void
 */
function display_payslip_salary_panel($doc) {
	$stats = array(
		array(_('Basic Salary'), price_format($doc->basic_salary), 'is-accent'),
		array(_('Gross Salary'), price_format($doc->gross_salary), ''),
		array(_('Total Deductions'), price_format($doc->total_deductions), 'is-warn'),
		array(_('Net Salary'), price_format($doc->net_salary), 'is-success'),
	);

	if ((float)$doc->loan_deduction > 0)
		$stats[] = array(_('Loan Deduction'), price_format($doc->loan_deduction), 'is-warn');

	echo "<section class='payslip-entry-panel'>";
	echo "<div class='payslip-entry-panel-head'><div><h3>".payslip_escape(_('Salary Snapshot'))."</h3><p>".payslip_escape(_('Final earnings and deductions that will post if you process this payslip.'))."</p></div></div>";
	echo "<div class='payslip-entry-stats-grid'>";
	foreach ($stats as $stat) {
		$class_name = $stat[2] !== '' ? 'payslip-entry-stat '.$stat[2] : 'payslip-entry-stat';
		echo "<div class='".payslip_escape($class_name)."'><span class='payslip-entry-stat-label'>".payslip_escape($stat[0])."</span><strong>".payslip_escape($stat[1])."</strong></div>";
	}
	echo "</div>";
	echo "</section>";
}

/**
 * Render attendance and work metrics for the preview.
 *
 * @param payslip_doc $doc
 * @return void
 */
function display_payslip_working_info_panel($doc) {
	$stats = array(
		array(_('Working Days'), payslip_format_measure($doc->working_days)),
		array(_('Payable Days'), payslip_format_measure($doc->payable_days)),
		array(_('Worked Days'), payslip_format_measure($doc->worked_days)),
		array(_('Leave Days'), payslip_format_measure($doc->leave_days)),
		array(_('Paid Leave Days'), payslip_format_measure($doc->paid_leave_days)),
		array(_('Unpaid Leave Days'), payslip_format_measure($doc->unpaid_leave_days)),
		array(_('Absent Days'), payslip_format_measure($doc->absent_days)),
		array(_('Extra Absent Days'), payslip_format_measure($doc->extra_absent_days)),
		array(_('Regular Hours'), payslip_format_measure($doc->regular_hours, _('hrs'))),
		array(_('Overtime Hours'), payslip_format_measure($doc->overtime_hours, _('hrs'))),
	);

	echo "<section class='payslip-entry-panel'>";
	echo "<div class='payslip-entry-panel-head'><div><h3>".payslip_escape(_('Working Information'))."</h3><p>".payslip_escape(_('Attendance, leave and overtime figures that feed the payroll formulas.'))."</p></div></div>";
	echo "<div class='payslip-entry-stats-grid'>";
	foreach ($stats as $stat)
		echo "<div class='payslip-entry-stat'><span class='payslip-entry-stat-label'>".payslip_escape($stat[0])."</span><strong>".payslip_escape($stat[1])."</strong></div>";
	echo "</div>";
	echo "</section>";
}

/**
 * Render leave totals grouped by leave type.
 *
 * @param payslip_doc $doc
 * @return void
 */
function display_payslip_leave_breakdown_panel($doc) {
	echo "<section class='payslip-entry-panel'>";
	echo "<div class='payslip-entry-panel-head'><div><h3>".payslip_escape(_('Leave Breakdown'))."</h3><p>".payslip_escape(_('Approved leave overlapping the selected period, split by leave type.'))."</p></div></div>";

	if (empty($doc->leave_breakdown)) {
		echo "<div class='payslip-entry-empty'>".payslip_escape(_('No approved leave entries overlap this payroll window.'))."</div>";
		echo "</section>";
		return;
	}

	echo "<table class='payslip-entry-mini-table'><thead><tr>";
	echo "<th>".payslip_escape(_('Leave Type'))."</th>";
	echo "<th class='is-right'>".payslip_escape(_('Days'))."</th>";
	echo "<th class='is-right'>".payslip_escape(_('Paid'))."</th>";
	echo "<th class='is-right'>".payslip_escape(_('Unpaid'))."</th>";
	echo "</tr></thead><tbody>";
	foreach ((array)$doc->leave_breakdown as $row) {
		$label = isset($row['leave_label']) ? $row['leave_label'] : _('Unspecified Leave');
		$code = isset($row['leave_code']) ? trim($row['leave_code']) : '';
		echo "<tr><td>".payslip_escape($label);
		if ($code !== '')
			echo "<span class='payslip-entry-muted'>".payslip_escape($code)."</span>";
		echo "</td><td class='is-right'>".payslip_escape(payslip_format_measure(isset($row['leave_days']) ? $row['leave_days'] : 0))."</td>";
		echo "<td class='is-right'>".payslip_escape(payslip_format_measure(isset($row['paid_days']) ? $row['paid_days'] : 0))."</td>";
		echo "<td class='is-right'>".payslip_escape(payslip_format_measure(isset($row['unpaid_days']) ? $row['unpaid_days'] : 0))."</td></tr>";
	}
	echo "</tbody></table>";
	echo "</section>";
}

/**
 * Render overtime totals grouped by overtime type.
 *
 * @param payslip_doc $doc
 * @return void
 */
function display_payslip_overtime_breakdown_panel($doc) {
	echo "<section class='payslip-entry-panel'>";
	echo "<div class='payslip-entry-panel-head'><div><h3>".payslip_escape(_('Overtime Breakdown'))."</h3><p>".payslip_escape(_('Captured overtime hours split by overtime type and rate.'))."</p></div></div>";

	if (empty($doc->overtime_breakdown)) {
		echo "<div class='payslip-entry-empty'>".payslip_escape(_('No overtime hours were recorded in this payroll window.'))."</div>";
		echo "</section>";
		return;
	}

	echo "<table class='payslip-entry-mini-table'><thead><tr>";
	echo "<th>".payslip_escape(_('Overtime Type'))."</th>";
	echo "<th class='is-right'>".payslip_escape(_('Hours'))."</th>";
	echo "<th class='is-right'>".payslip_escape(_('Rate'))."</th>";
	echo "</tr></thead><tbody>";
	foreach ((array)$doc->overtime_breakdown as $row) {
		$rate_value = 0.0;
		if (isset($row['pay_rate']) && is_numeric($row['pay_rate']) && (float)$row['pay_rate'] > 0)
			$rate_value = (float)$row['pay_rate'];
		elseif (isset($row['applied_rate']) && is_numeric($row['applied_rate']) && (float)$row['applied_rate'] > 0)
			$rate_value = (float)$row['applied_rate'];

		echo "<tr><td>".payslip_escape(isset($row['overtime_name']) ? $row['overtime_name'] : _('Unassigned Overtime'))."</td>";
		echo "<td class='is-right'>".payslip_escape(payslip_format_measure(isset($row['overtime_hours']) ? $row['overtime_hours'] : 0, _('hrs')))."</td>";
		echo "<td class='is-right'>".payslip_escape($rate_value > 0 ? number_format2($rate_value, 2).'x' : '-')."</td></tr>";
	}
	echo "</tbody></table>";
	echo "</section>";
}

/**
 * Build visible payslip detail rows with a guaranteed basic salary line first.
 *
 * @param payslip_doc $doc
 * @return array
 */
function get_payslip_display_lines($doc) {
	$rows = array();
	$basic_category = defined('HRM_ELEM_BASIC') ? HRM_ELEM_BASIC : 1;

	$rows[] = array(
		'element_name' => _('Basic Salary'),
		'type_key' => 'basic',
		'amount' => (float)$doc->basic_salary,
	);

	foreach ((array)$doc->lines as $line) {
		if (empty($line->is_deduction) && (int)$line->element_category === (int)$basic_category)
			continue;

		$rows[] = array(
			'element_name' => isset($line->element_name) ? $line->element_name : '',
			'type_key' => !empty($line->is_deduction) ? 'deduction' : 'earning',
			'amount' => isset($line->final_amount) ? (float)$line->final_amount : 0.0,
		);
	}

	return $rows;
}

function display_payslip_header($Order) {
	global $Ajax;

	$new = $Order->trans_no == 0;
	$emp_error = '';
	$preview_doc = false;

	if($new) {
		if (!isset($_POST['employee_id']) && (get_global_employee() != ALL_TEXT))
			$_POST['employee_id'] = get_global_employee();
	}

	$selected_employee_post = trim((string)get_post('employee_id', ''));

	if($Order->employee_id != get_post('employee_id', -1) || list_updated('employee_id')) {
		if ($selected_employee_post !== '' && $selected_employee_post !== ALL_TEXT)
			$emp_error = get_employee_details_to_payslip($Order, $selected_employee_post);
		else
			$Order->set_employee('', '');
		$Ajax->activate('_page_body');
	}
	set_global_employee($selected_employee_post);
	$selected_employee_id = get_post('employee_id', $Order->employee_id);

	if ($emp_error == '') {
		list($preview_doc, $preview_error) = build_payslip_preview($Order, $selected_employee_id, get_post('from_date'), get_post('to_date'));
		if ($preview_error !== '')
			$emp_error = $preview_error;
	}

	$Order->payslip_preview = $preview_doc ? $preview_doc : null;
	$hero_title = $preview_doc ? (isset($preview_doc->employee['employee_name']) ? $preview_doc->employee['employee_name'] : $preview_doc->employee_id) : _('Prepare Payslip Preview');
	$hero_description = $preview_doc
		? _('Review salary, attendance, leave, and overtime before posting the payslip to the ledger.')
		: _('Select an employee and period to generate a live payslip preview with salary, attendance, leave, and overtime details.');
	$hero_stats = array(
		array(_('Period'), trim(get_post('from_date').' - '.get_post('to_date'))),
		array(_('Basic Salary'), $preview_doc ? price_format($preview_doc->basic_salary) : price_format(0)),
		array(_('Net Salary'), $preview_doc ? price_format($preview_doc->net_salary) : price_format(0)),
		array(_('Working Days'), $preview_doc ? payslip_format_measure($preview_doc->working_days) : payslip_format_measure(0)),
	);

	echo "<section class='payslip-entry-hero'>";
	echo "<div class='payslip-entry-hero-copy'><span class='payslip-entry-eyebrow'>".payslip_escape(_('Payroll Workspace'))."</span><h2>".payslip_escape($hero_title)."</h2><p>".payslip_escape($hero_description)."</p></div>";
	echo "<div class='payslip-entry-hero-stats'>";
	foreach ($hero_stats as $hero_stat)
		echo "<div class='payslip-entry-hero-stat'><span class='payslip-entry-hero-stat-label'>".payslip_escape($hero_stat[0])."</span><strong class='payslip-entry-hero-stat-value'>".payslip_escape($hero_stat[1])."</strong></div>";
	echo "</div></section>";

	echo "<section class='payslip-entry-form-panel'>";
	start_outer_table(TABLESTYLE2, "data-order-header='1'");
	table_section(1);

	if($new) {
		employees_list_row(_('Employee:'), 'employee_id', null, _('Select employee'), true, false, true);
	}
	else {
		hidden('employee_id', $Order->employee_id);
		label_row(_('Employee:'), $Order->employee_name);
	}

	date_row(_('From Date:'), 'from_date');
	date_row(_('To Date:'), 'to_date');

	table_section(2);

	date_row(_('Payslip Date:'), 'tran_date', '', $new, 0, 0, 0, null, true);
	ref_row(_('Reference:'), 'reference', null, get_post('reference'), false, ST_PAYSLIP, get_post('tran_date'));

	date_row(_('Payment Due:'), 'due_date', '', $new, 0, 0, 0, null, true);

	end_outer_table();
	echo "</section>";

	return $emp_error;
}

function display_order_summary($title, $order, $editable_items=false) {
	$doc = get_payslip_display_doc($order);

	div_start('payslip_details');

	if (!$doc) {
		display_note(_('No payslip calculation is available for the selected employee and period.'), 0, 1);
		div_end();
		return;
	}

	echo "<div class='payslip-entry-grid'>";
	display_payslip_context_panel($doc);
	display_payslip_salary_panel($doc);
	echo "</div>";

	echo "<div class='payslip-entry-grid'>";
	display_payslip_working_info_panel($doc);
	display_payslip_leave_breakdown_panel($doc);
	display_payslip_overtime_breakdown_panel($doc);
	echo "</div>";

	echo "<section class='payslip-entry-panel'>";
	echo "<div class='payslip-entry-panel-head'><div><h3>".payslip_escape($title)."</h3><p>".payslip_escape(_('Basic salary is pinned first, followed by the remaining earnings and deductions that will be posted.'))."</p></div></div>";
	echo "<table class='payslip-entry-detail-table'><thead><tr>";
	echo "<th>".payslip_escape(_('Pay Element'))."</th>";
	echo "<th>".payslip_escape(_('Type'))."</th>";
	echo "<th class='is-right'>".payslip_escape(_('Earnings'))."</th>";
	echo "<th class='is-right'>".payslip_escape(_('Deduction'))."</th>";
	echo "</tr></thead><tbody>";
	foreach (get_payslip_display_lines($doc) as $row) {
		$type_key = $row['type_key'];
		$type_label = $type_key === 'basic' ? _('Basic') : ($type_key === 'deduction' ? _('Deduction') : _('Earning'));
		echo "<tr><td>".payslip_escape($row['element_name'])."</td><td><span class='payslip-entry-badge payslip-entry-badge-".payslip_escape($type_key)."'>".payslip_escape($type_label)."</span></td>";
		if ($type_key === 'deduction') {
			echo "<td class='is-right'>-</td><td class='is-right'>".payslip_escape(price_format($row['amount']))."</td>";
		} else {
			echo "<td class='is-right'>".payslip_escape(price_format($row['amount']))."</td><td class='is-right'>-</td>";
		}
		echo "</tr>";
	}
	echo "</tbody><tfoot>";
	echo "<tr><td colspan='3'>".payslip_escape(_('Gross Salary'))."</td><td class='is-right'>".payslip_escape(price_format($doc->gross_salary))."</td></tr>";
	echo "<tr><td colspan='3'>".payslip_escape(_('Total Deductions'))."</td><td class='is-right'>".payslip_escape(price_format($doc->total_deductions))."</td></tr>";
	echo "<tr><td colspan='3'>".payslip_escape(_('Net Salary'))."</td><td class='is-right'>".payslip_escape(price_format($doc->net_salary))."</td></tr>";
	echo "</tfoot></table></section>";
	div_end();
}
