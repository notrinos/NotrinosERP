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
 * Employee form widgets, display helpers, and tab content functions.
 */

//------------------------------------------------------------------
// Display salary components table (read-only or editable)
//------------------------------------------------------------------
function display_salary_components($employee_id, $editable=false) {
	global $Ajax;

	$components = get_employee_salary_components($employee_id);
	$emp = get_employee_by_code($employee_id);
	
	if (empty($components)) {
		if ($emp && $emp['personal_salary']) {
			display_note(_('No personal salary components configured. Add elements below.'));
		} else {
			display_note(_('No salary structure found for this employee\'s position/grade combination. Configure salary structure or enable personal salary.'));
		}
		return;
	}

	$categories = hrm_get_element_categories();
	
	start_table(TABLESTYLE);

	$th = array(_('#'), _('Element Code'), _('Pay Element'), _('Category'), _('Type'), _('Amount'), _('Effective From'));
	if ($editable)
		$th[] = _('Actions');
	table_header($th);

	$total_earnings = 0;
	$total_deductions = 0;
	$k = 0;

	foreach ($components as $i => $comp) {
		alt_table_row_color($k);

		label_cell($i + 1);
		label_cell($comp['element_code']);
		label_cell($comp['element_name']);
		label_cell(isset($categories[$comp['element_category']]) ? $categories[$comp['element_category']] : '-');

		$amt_types = hrm_get_amount_types();
		label_cell(isset($amt_types[$comp['amount_type']]) ? $amt_types[$comp['amount_type']] : '-');

		if ($comp['amount_type'] == HRM_AMTTYPE_PCT_BASIC || $comp['amount_type'] == HRM_AMTTYPE_PCT_GROSS)
			amount_cell($comp['amount'], false, '', '%');
		else
			amount_cell($comp['amount']);

		label_cell(sql2date($comp['effective_from']));

		if ($editable) {
			$actions = '';
			if (isset($comp['salary_id'])) {
				$actions = "<a href='".$_SERVER['PHP_SELF']."?employee_no=".urlencode($employee_id)."&_tabs_sel=tab_salary&edit_salary=".$comp['salary_id']."'>"._('Edit')."</a>"
					." | <a href='".$_SERVER['PHP_SELF']."?employee_no=".urlencode($employee_id)."&_tabs_sel=tab_salary&delete_salary=".$comp['salary_id']."' onclick=\"return confirm('"._('Are you sure?')."');\">"._('Delete')."</a>";
			}
			label_cell($actions, "align='center' nowrap");
		}

		if ($comp['is_deduction'])
			$total_deductions += $comp['amount'];
		else
			$total_earnings += $comp['amount'];

		end_row();
	}

	// Summary rows
	start_row();
	label_cells(_('Total Earnings'), price_format($total_earnings), "colspan=5 class='label' align='right'", "align='right'");
	label_cell('');
	if ($editable) label_cell('');
	end_row();

	start_row();
	label_cells(_('Total Deductions'), price_format($total_deductions), "colspan=5 class='label' align='right'", "align='right' style='color:red'");
	label_cell('');
	if ($editable) label_cell('');
	end_row();

	start_row();
	label_cells('<b>'._('Net Salary').'</b>', '<b>'.price_format($total_earnings - $total_deductions).'</b>', 
		"colspan=5 class='label' align='right'", "align='right'");
	label_cell('');
	if ($editable) label_cell('');
	end_row();

	end_table(1);
}

//------------------------------------------------------------------
// Display employee documents table
//------------------------------------------------------------------
function display_employee_documents($employee_id) {
	$docs = get_employee_documents($employee_id);

	start_table(TABLESTYLE);

	$th = array(_('#'), _('Document Type'), _('Document Name'), _('Issue Date'), _('Expiry Date'), _('Status'), _('Notes'), '');
	table_header($th);

	$k = 0;
	$row_num = 0;

	while ($doc = db_fetch_assoc($docs)) {
		$row_num++;
		alt_table_row_color($k);

		label_cell($row_num);
		label_cell($doc['type_name']);
		label_cell($doc['doc_name']);
		label_cell(!empty($doc['issue_date']) ? sql2date($doc['issue_date']) : '-');

		if (!empty($doc['expiry_date'])) {
			$expiry = $doc['expiry_date'];
			$days_left = (strtotime($expiry) - time()) / 86400;
			$style = '';
			if ($days_left < 0) $style = "style='color:red; font-weight:bold'";
			elseif ($days_left < 30) $style = "style='color:orange; font-weight:bold'";
			label_cell(sql2date($expiry), "nowrap $style");

			if ($days_left < 0)
				label_cell(_('EXPIRED'), "style='color:red; font-weight:bold'");
			elseif ($days_left < 30)
				label_cell(sprintf(_('Expires in %d days'), ceil($days_left)), "style='color:orange'");
			else
				label_cell(_('Valid'), "style='color:green'");
		} else {
			label_cell('-');
			label_cell(_('N/A'));
		}

		label_cell($doc['notes']);

		// Action buttons
		$actions = "<a href='".($_SERVER['PHP_SELF']."?employee_no=".urlencode($employee_id)."&_tabs_sel=tab_documents&edit_doc=".$doc['doc_id'])."'>"._('Edit')."</a>"
			." | <a href='".($_SERVER['PHP_SELF']."?employee_no=".urlencode($employee_id)."&_tabs_sel=tab_documents&delete_doc=".$doc['doc_id'])."' onclick=\"return confirm('"._('Are you sure?')."');\">"._('Delete')."</a>";
		if (!empty($doc['file_path']))
			$actions .= " | <a href='".$doc['file_path']."' target='_blank'>"._('View')."</a>";
		label_cell($actions, "nowrap");

		end_row();
	}

	if ($row_num == 0) {
		start_row();
		label_cell(_('No documents found.'), "colspan=8 align='center'");
		end_row();
	}

	end_table(1);
}

//------------------------------------------------------------------
// Display employee dependents table
//------------------------------------------------------------------
function display_employee_dependents($employee_id) {
	$deps = get_employee_dependents($employee_id);
	$genders = hrm_get_gender_types();

	start_table(TABLESTYLE);

	$th = array(_('#'), _('Name'), _('Relationship'), _('Gender'), _('Birth Date'), _('National ID'), _('Beneficiary'), '');
	table_header($th);

	$k = 0;
	$row_num = 0;

	while ($dep = db_fetch_assoc($deps)) {
		$row_num++;
		alt_table_row_color($k);

		label_cell($row_num);
		label_cell($dep['name']);
		label_cell(ucfirst($dep['relationship']));
		label_cell(isset($genders[$dep['gender']]) ? $genders[$dep['gender']] : '-');
		label_cell(!empty($dep['birth_date']) ? sql2date($dep['birth_date']) : '-');
		label_cell($dep['national_id']);
		label_cell($dep['is_beneficiary'] ? _('Yes') : _('No'));

		$actions = "<a href='".($_SERVER['PHP_SELF']."?employee_no=".urlencode($employee_id)."&_tabs_sel=tab_dependents&edit_dep=".$dep['dependent_id'])."'>"._('Edit')."</a>"
			." | <a href='".($_SERVER['PHP_SELF']."?employee_no=".urlencode($employee_id)."&_tabs_sel=tab_dependents&delete_dep=".$dep['dependent_id'])."' onclick=\"return confirm('"._('Are you sure?')."');\">"._('Delete')."</a>";
		label_cell($actions, "nowrap");

		end_row();
	}

	if ($row_num == 0) {
		start_row();
		label_cell(_('No dependents found.'), "colspan=8 align='center'");
		end_row();
	}

	end_table(1);
}

//------------------------------------------------------------------
// Display employee history timeline
//------------------------------------------------------------------
function display_employee_history($employee_id) {
	$history = get_employee_history($employee_id);

	start_table(TABLESTYLE);

	$th = array(_('Date'), _('Change Type'), _('Details'), _('Reason'), _('By'));
	table_header($th);

	$k = 0;
	$row_num = 0;

	while ($rec = db_fetch_assoc($history)) {
		$row_num++;
		alt_table_row_color($k);

		label_cell(sql2date($rec['effective_date']));
		label_cell(get_change_type_label($rec['change_type']));

		// Build details string
		$details = array();
		if ($rec['old_department_name'] || $rec['new_department_name']) {
			$details[] = _('Dept:').' '.($rec['old_department_name'] ?: '-').' ? '.($rec['new_department_name'] ?: '-');
		}
		if ($rec['old_position_name'] || $rec['new_position_name']) {
			$details[] = _('Position:').' '.($rec['old_position_name'] ?: '-').' ? '.($rec['new_position_name'] ?: '-');
		}
		if ($rec['old_grade_name'] || $rec['new_grade_name']) {
			$details[] = _('Grade:').' '.($rec['old_grade_name'] ?: '-').' ? '.($rec['new_grade_name'] ?: '-');
		}
		if ($rec['old_salary'] || $rec['new_salary']) {
			$details[] = _('Salary:').' '.price_format($rec['old_salary']).' ? '.price_format($rec['new_salary']);
		}
		label_cell(implode('<br>', $details));

		label_cell($rec['reason']);
		label_cell($rec['created_by']);

		end_row();
	}

	if ($row_num == 0) {
		start_row();
		label_cell(_('No history records found.'), "colspan=5 align='center'");
		end_row();
	}

	end_table(1);
}

//------------------------------------------------------------------
// Display employee transactions summary (payslips, leave, loans)
//------------------------------------------------------------------
function display_employee_transactions($employee_id) {
	global $path_to_root;

	// Recent payslips
	display_heading(_('Recent Payslips'));
	$sql = "SELECT p.payslip_id, p.tran_date, p.from_date, p.to_date, 
				p.gross_salary, p.total_deductions, p.net_salary, p.status
			FROM ".TB_PREF."payslips p
			WHERE p.employee_id = ".db_escape($employee_id)."
			ORDER BY p.tran_date DESC LIMIT 12";
	$result = db_query($sql, 'could not get payslips');

	start_table(TABLESTYLE);
	$th = array(_('#'), _('Date'), _('Period'), _('Gross'), _('Deductions'), _('Net'), _('Status'));
	table_header($th);
	$k = 0;
	$count = 0;
	while ($row = db_fetch_assoc($result)) {
		$count++;
		alt_table_row_color($k);
		label_cell($count);
		label_cell(sql2date($row['tran_date']));
		label_cell(sql2date($row['from_date']).' - '.sql2date($row['to_date']));
		amount_cell($row['gross_salary']);
		amount_cell($row['total_deductions']);
		amount_cell($row['net_salary']);
		$statuses = hrm_get_payslip_statuses();
		label_cell(isset($statuses[$row['status']]) ? $statuses[$row['status']] : $row['status']);
		end_row();
	}
	if ($count == 0) {
		start_row();
		label_cell(_('No payslips found.'), "colspan=7 align='center'");
		end_row();
	}
	end_table(1);

	// Leave balances
	display_heading(_('Leave Balances'));
	$year = date('Y');
	$sql = "SELECT lb.*, lt.leave_name
			FROM ".TB_PREF."leave_balances lb
			JOIN ".TB_PREF."leave_types lt ON lb.leave_id = lt.leave_id
			WHERE lb.employee_id = ".db_escape($employee_id)."
			AND lb.fiscal_year = ".db_escape($year)."
			ORDER BY lt.leave_name";
	$result = db_query($sql, 'could not get leave balances');

	start_table(TABLESTYLE);
	$th = array(_('Leave Type'), _('Entitled'), _('Carried Forward'), _('Taken'), _('Pending'), _('Adjusted'), _('Balance'));
	table_header($th);
	$k = 0;
	$count = 0;
	while ($row = db_fetch_assoc($result)) {
		$count++;
		alt_table_row_color($k);
		label_cell($row['leave_name']);
		qty_cell($row['entitled'], false, 1);
		qty_cell($row['carried_forward'], false, 1);
		qty_cell($row['taken'], false, 1);
		qty_cell($row['pending'], false, 1);
		qty_cell($row['adjusted'], false, 1);
		$balance = $row['entitled'] + $row['carried_forward'] - $row['taken'] - $row['pending'] + $row['adjusted'];
		qty_cell($balance, false, 1);
		end_row();
	}
	if ($count == 0) {
		start_row();
		label_cell(_('No leave balances found for current year.'), "colspan=7 align='center'");
		end_row();
	}
	end_table(1);

	// Outstanding loans
	display_heading(_('Active Loans'));
	$sql = "SELECT l.*, lt.loan_type_name
			FROM ".TB_PREF."employee_loans l
			JOIN ".TB_PREF."loan_types lt ON l.loan_type_id = lt.loan_type_id
			WHERE l.employee_id = ".db_escape($employee_id)."
			AND l.status IN (0, 1)
			ORDER BY l.loan_date DESC";
	$result = db_query($sql, 'could not get loans');

	start_table(TABLESTYLE);
	$th = array(_('Loan Type'), _('Amount'), _('Installment'), _('Outstanding'), _('Start Date'), _('Status'));
	table_header($th);
	$k = 0;
	$count = 0;
	while ($row = db_fetch_assoc($result)) {
		$count++;
		alt_table_row_color($k);
		label_cell($row['loan_type_name']);
		amount_cell($row['loan_amount']);
		amount_cell($row['installment_amount']);
		amount_cell($row['outstanding_amount']);
		label_cell(sql2date($row['loan_date']));
		$statuses = array(0 => _('Pending'), 1 => _('Active'));
		label_cell(isset($statuses[$row['status']]) ? $statuses[$row['status']] : $row['status']);
		end_row();
	}
	if ($count == 0) {
		start_row();
		label_cell(_('No active loans.'), "colspan=6 align='center'");
		end_row();
	}
	end_table(1);
}

//------------------------------------------------------------------
// Employee summary card display (compact info box)
//------------------------------------------------------------------
function display_employee_summary_card($employee_id) {
	$emp = get_employee_by_code($employee_id);
	if (!$emp) return;

	start_table(TABLESTYLE2);

	label_row(_('Employee ID:'), $emp['employee_id']);
	label_row(_('Name:'), $emp['first_name'].' '.($emp['middle_name'] ? $emp['middle_name'].' ' : '').$emp['last_name']);
	label_row(_('Department:'), $emp['department_name'] ?: '-');
	label_row(_('Position:'), $emp['position_name'] ?: '-');
	label_row(_('Grade:'), $emp['grade_name'] ?: '-');
	label_row(_('Hire Date:'), !empty($emp['hire_date']) ? sql2date($emp['hire_date']) : '-');

	$emp_types = hrm_get_employment_types();
	label_row(_('Employment Type:'), isset($emp_types[$emp['employment_type']]) ? $emp_types[$emp['employment_type']] : '-');

	if (!empty($emp['released_date']))
		label_row(_('Release Date:'), sql2date($emp['released_date']));

	$status = $emp['inactive'] ? _('Inactive') : _('Active');
	label_row(_('Status:'), $status);

	end_table(1);
}

