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

include_once($path_to_root.'/hrm/includes/hrm_constants.inc');

/**
 * HRM combo/list selector functions
 * Provides dropdown selectors for all HRM entities.
 */

//------------------------------------------------------------------
// Employment Type selector
//------------------------------------------------------------------
function employment_type_list($name, $selected=null, $submit_on_change=false) {
	$types = hrm_get_employment_types();
	return array_selector($name, $selected, $types, array(
		'select_submit' => $submit_on_change,
	));
}

function employment_type_list_row($label, $name, $selected=null, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo employment_type_list($name, $selected, $submit_on_change);
	echo "</td></tr>\n";
}

function employment_type_list_cells($label, $name, $selected=null, $submit_on_change=false) {
	if ($label != null)
		echo "<td class='label'>$label</td>\n";
	echo "<td>";
	echo employment_type_list($name, $selected, $submit_on_change);
	echo "</td>\n";
}

//------------------------------------------------------------------
// Gender selector
//------------------------------------------------------------------
function gender_list($name, $selected=null, $submit_on_change=false) {
	$types = hrm_get_gender_types();
	return array_selector($name, $selected, $types, array(
		'select_submit' => $submit_on_change,
	));
}

function gender_list_row($label, $name, $selected=null, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo gender_list($name, $selected, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Payment Method selector
//------------------------------------------------------------------
function payment_method_list($name, $selected=null, $submit_on_change=false) {
	$types = hrm_get_payment_methods();
	return array_selector($name, $selected, $types, array(
		'select_submit' => $submit_on_change,
	));
}

function payment_method_list_row($label, $name, $selected=null, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo payment_method_list($name, $selected, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Separation Reason selector
//------------------------------------------------------------------
function separation_reason_list($name, $selected=null, $submit_on_change=false) {
	$types = hrm_get_separation_reasons();
	return array_selector($name, $selected, $types, array(
		'select_submit' => $submit_on_change,
	));
}

function separation_reason_list_row($label, $name, $selected=null, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo separation_reason_list($name, $selected, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Work Shift selector
//------------------------------------------------------------------
function work_shifts_list($name, $selected=null, $none_option=false, $submit_on_change=false) {
	$sql = "SELECT shift_id, shift_name FROM ".TB_PREF."work_shifts WHERE !inactive";
	return combo_input($name, $selected, $sql, 'shift_id', 'shift_name', array(
		'select_submit' => $submit_on_change,
		'spec_option' => $none_option ? _('-- None --') : false,
		'spec_id' => 0,
	));
}

function work_shifts_list_row($label, $name, $selected=null, $none_option=true, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo work_shifts_list($name, $selected, $none_option, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Document Type selector
//------------------------------------------------------------------
function document_types_list($name, $selected=null, $none_option=false, $submit_on_change=false) {
	$sql = "SELECT doc_type_id, type_name FROM ".TB_PREF."document_types WHERE !inactive";
	return combo_input($name, $selected, $sql, 'doc_type_id', 'type_name', array(
		'select_submit' => $submit_on_change,
		'spec_option' => $none_option ? _('-- Select --') : false,
		'spec_id' => '',
	));
}

function document_types_list_row($label, $name, $selected=null, $none_option=true, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo document_types_list($name, $selected, $none_option, $submit_on_change);
	echo "</td></tr>\n";
}

function document_types_list_cells($label, $name, $selected=null, $none_option=true, $submit_on_change=false) {
	if ($label != null)
		echo "<td class='label'>$label</td>\n";
	echo "<td>";
	echo document_types_list($name, $selected, $none_option, $submit_on_change);
	echo "</td>\n";
}

//------------------------------------------------------------------
// Relationship selector (for dependents)
//------------------------------------------------------------------
function relationship_list($name, $selected=null) {
	$types = array(
		'spouse'  => _('Spouse'),
		'child'   => _('Child'),
		'parent'  => _('Parent'),
		'sibling' => _('Sibling'),
		'other'   => _('Other'),
	);
	return array_selector($name, $selected, $types);
}

function relationship_list_row($label, $name, $selected=null) {
	echo "<tr><td class='label'>$label</td><td>";
	echo relationship_list($name, $selected);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Employee Reporting-to selector (active employees)
//------------------------------------------------------------------
function reporting_to_list($name, $selected=null, $exclude_employee='', $submit_on_change=false) {
	$sql = "SELECT employee_id, CONCAT(first_name, ' ', last_name, ' [', employee_id, ']') as name
			FROM ".TB_PREF."employees WHERE !inactive";
	if (!empty($exclude_employee))
		$sql .= " AND employee_id != ".db_escape($exclude_employee);

	return combo_input($name, $selected, $sql, 'employee_id', 'name', array(
		'select_submit' => $submit_on_change,
		'spec_option' => _('-- None --'),
		'spec_id' => '',
	));
}

function reporting_to_list_row($label, $name, $selected=null, $exclude_employee='', $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo reporting_to_list($name, $selected, $exclude_employee, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Leave Type selector
//------------------------------------------------------------------
function leave_types_list($name, $selected=null, $none_option=false, $submit_on_change=false) {
	$sql = "SELECT leave_id, leave_name FROM ".TB_PREF."leave_types WHERE !inactive";
	return combo_input($name, $selected, $sql, 'leave_id', 'leave_name', array(
		'select_submit' => $submit_on_change,
		'spec_option' => $none_option ? _('-- All --') : false,
		'spec_id' => 0,
	));
}

function leave_types_list_row($label, $name, $selected=null, $none_option=false, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo leave_types_list($name, $selected, $none_option, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Loan Type selector
//------------------------------------------------------------------
function loan_types_list($name, $selected=null, $none_option=false, $submit_on_change=false) {
	$sql = "SELECT loan_type_id, loan_type_name FROM ".TB_PREF."loan_types WHERE !inactive";
	return combo_input($name, $selected, $sql, 'loan_type_id', 'loan_type_name', array(
		'select_submit' => $submit_on_change,
		'spec_option' => $none_option ? _('-- Select --') : false,
		'spec_id' => '',
	));
}

function loan_types_list_row($label, $name, $selected=null, $none_option=true, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo loan_types_list($name, $selected, $none_option, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Pay Element Category selector
//------------------------------------------------------------------
function element_category_list($name, $selected=null, $submit_on_change=false) {
	$types = hrm_get_element_categories();
	return array_selector($name, $selected, $types, array(
		'select_submit' => $submit_on_change,
	));
}

function element_category_list_row($label, $name, $selected=null, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo element_category_list($name, $selected, $submit_on_change);
	echo "</td></tr>\n";
}

//------------------------------------------------------------------
// Amount Type selector
//------------------------------------------------------------------
function amount_type_list($name, $selected=null, $submit_on_change=false) {
	$types = hrm_get_amount_types();
	return array_selector($name, $selected, $types, array(
		'select_submit' => $submit_on_change,
	));
}

function amount_type_list_row($label, $name, $selected=null, $submit_on_change=false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo amount_type_list($name, $selected, $submit_on_change);
	echo "</td></tr>\n";
}

