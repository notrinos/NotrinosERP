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
 * HRM Module Formula Provider.
 *
 * Registers HRM-specific fields (Employee.*, Payroll.* namespaces)
 * and templates with the Visual Formula Designer via the provider
 * bridge.
 *
 * This file is auto-discovered by {@see formula_designer_autodiscover_providers()}
 * when the designer bootstrap loads during a full NotrinosERP session.
 *
 * @package HRM
 * @since   2.0.0
 */

// ---------------------------------------------------------------------------
// Field provider registration
// ---------------------------------------------------------------------------

/**
 * Register HRM fields with the designer field registry.
 *
 * Called by {@see formula_designer_invoke_field_providers()} when the
 * designer builds its field palette for the 'hrm' module.
 *
 * @param FormulaDesigner_Registry_DesignerFieldRegistry $registry
 * @param string                                         $module
 * @return void
 */
function hrm_formula_designer_register_fields(
    FormulaDesigner_Registry_DesignerFieldRegistry $registry,
    $module
) {
    if ($module !== 'hrm') {
        return;
    }

    $fields = array(
        // Employee namespace
        array('namespace' => 'Employee', 'name' => 'Id',              'label' => 'Employee ID',             'type' => 'number',  'description' => 'Employee database ID.'),
        array('namespace' => 'Employee', 'name' => 'Name',            'label' => 'Employee Name',           'type' => 'text',    'description' => 'Employee full name.'),
        array('namespace' => 'Employee', 'name' => 'FirstName',       'label' => 'First Name',              'type' => 'text',    'description' => 'Employee first name.'),
        array('namespace' => 'Employee', 'name' => 'LastName',        'label' => 'Last Name',               'type' => 'text',    'description' => 'Employee last name.'),
        array('namespace' => 'Employee', 'name' => 'BasicSalary',     'label' => 'Basic Salary',            'type' => 'number',  'description' => 'Employee basic monthly salary.',        'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
        array('namespace' => 'Employee', 'name' => 'GrossSalary',     'label' => 'Gross Salary',            'type' => 'number',  'description' => 'Employee gross salary.',               'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
        array('namespace' => 'Employee', 'name' => 'Department',      'label' => 'Department',              'type' => 'text',    'description' => 'Employee department.'),
        array('namespace' => 'Employee', 'name' => 'Designation',     'label' => 'Designation',             'type' => 'text',    'description' => 'Employee designation.'),
        array('namespace' => 'Employee', 'name' => 'JoinDate',        'label' => 'Join Date',               'type' => 'date',    'description' => 'Employee joining date.'),
        array('namespace' => 'Employee', 'name' => 'ConfirmationDate','label' => 'Confirmation Date',         'type' => 'date',    'description' => 'Employee confirmation date.'),
        array('namespace' => 'Employee', 'name' => 'TerminationDate', 'label' => 'Termination Date',          'type' => 'date',    'description' => 'Employee termination date.'),
        array('namespace' => 'Employee', 'name' => 'IsActive',        'label' => 'Is Active',               'type' => 'boolean', 'description' => 'Whether the employee is active.'),
        array('namespace' => 'Employee', 'name' => 'BankAccount',     'label' => 'Bank Account',            'type' => 'text',    'description' => 'Employee bank account number.',         'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
        array('namespace' => 'Employee', 'name' => 'TaxId',           'label' => 'Tax ID',                  'type' => 'text',    'description' => 'Employee tax identifier.',              'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
        array('namespace' => 'Employee', 'name' => 'EmployeeCode',    'label' => 'Employee Code',           'type' => 'text',    'description' => 'Employee code.'),
        // Payroll namespace
        array('namespace' => 'Payroll',  'name' => 'Basic',           'label' => 'Basic',                   'type' => 'number',  'description' => 'Payroll basic amount for the period.'),
        array('namespace' => 'Payroll',  'name' => 'Gross',           'label' => 'Gross',                   'type' => 'number',  'description' => 'Payroll gross amount for the period.'),
        array('namespace' => 'Payroll',  'name' => 'DaysWorked',      'label' => 'Days Worked',             'type' => 'number',  'description' => 'Days worked in the period.'),
        array('namespace' => 'Payroll',  'name' => 'PayableDays',     'label' => 'Payable Days',            'type' => 'number',  'description' => 'Payable days in the period.'),
        array('namespace' => 'Payroll',  'name' => 'WorkingDays',     'label' => 'Working Days',            'type' => 'number',  'description' => 'Configured working days in the period.'),
        array('namespace' => 'Payroll',  'name' => 'DaysInMonth',     'label' => 'Days In Month',           'type' => 'number',  'description' => 'Days in the current month.'),
        array('namespace' => 'Payroll',  'name' => 'LeaveDays',       'label' => 'Leave Days',              'type' => 'number',  'description' => 'Leave days taken in the period.'),
        array('namespace' => 'Payroll',  'name' => 'PaidLeaveDays',   'label' => 'Paid Leave Days',         'type' => 'number',  'description' => 'Paid leave days in the period.'),
        array('namespace' => 'Payroll',  'name' => 'AbsentDays',      'label' => 'Absent Days',             'type' => 'number',  'description' => 'Absent days in the period.'),
        array('namespace' => 'Payroll',  'name' => 'OvertimeHours',   'label' => 'Overtime Hours',          'type' => 'number',  'description' => 'Overtime hours worked in the period.'),
        array('namespace' => 'Payroll',  'name' => 'UnpaidLeaveDays', 'label' => 'Unpaid Leave Days',       'type' => 'number',  'description' => 'Unpaid leave days in the period.'),
        array('namespace' => 'Payroll',  'name' => 'HourlyRate',      'label' => 'Hourly Rate',             'type' => 'number',  'description' => 'Calculated hourly rate.'),
        array('namespace' => 'Payroll',  'name' => 'TaxRate',         'label' => 'Tax Rate',                'type' => 'number',  'description' => 'Payroll tax rate for the employee.'),
        array('namespace' => 'Payroll',  'name' => 'PeriodStart',     'label' => 'Period Start',            'type' => 'date',    'description' => 'Payroll period start date.'),
        array('namespace' => 'Payroll',  'name' => 'PeriodEnd',       'label' => 'Period End',              'type' => 'date',    'description' => 'Payroll period end date.'),
    );

    foreach ($fields as $field) {
        if (!$registry->has($field['namespace'] . '.' . $field['name'])) {
            $registry->register(new FormulaDesigner_Registry_DesignerFieldMetadata($field));
        }
    }
}

// ---------------------------------------------------------------------------
// Template provider registration
// ---------------------------------------------------------------------------

/**
 * Register HRM templates with the designer template registry.
 *
 * Called by {@see formula_designer_invoke_template_providers()} when the
 * designer builds its template browser for the 'hrm' module.
 *
 * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
 * @param string                                            $module
 * @return void
 */
function hrm_formula_designer_register_templates(
    FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
    $module
) {
    if ($module !== 'hrm') {
        return;
    }

    // Delegate to the existing PayrollTemplateProvider
    // which already defines 10 HRM/payroll templates.
    if (class_exists('FormulaDesigner_Templates_PayrollTemplateProvider')) {
        $templates = FormulaDesigner_Templates_PayrollTemplateProvider::all();
        foreach ($templates as $template) {
            if (!$registry->has($template->getId())) {
                $registry->register($template);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Provider registration with the designer bridge
// ---------------------------------------------------------------------------

// Register field provider callback
if (!isset($GLOBALS['formula_designer_provider_fields'])) {
    $GLOBALS['formula_designer_provider_fields'] = array();
}
$GLOBALS['formula_designer_provider_fields']['hrm'][] = 'hrm_formula_designer_register_fields';

// Register template provider callback
if (!isset($GLOBALS['formula_designer_provider_templates'])) {
    $GLOBALS['formula_designer_provider_templates'] = array();
}
$GLOBALS['formula_designer_provider_templates']['hrm'][] = 'hrm_formula_designer_register_templates';

// Also tell the bridge where this file lives
if (function_exists('formula_designer_register_provider')) {
    $provider_file = __FILE__;
    if (!defined('FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS')) {
        require_once dirname(dirname(dirname(__FILE__)))
            . '/includes/formula_designer/designer_bootstrap.inc';
    }
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS,
        'hrm',
        $provider_file
    );
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES,
        'hrm',
        $provider_file
    );
}
