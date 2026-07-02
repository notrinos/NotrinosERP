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
 * PayrollTemplateProvider — built-in formula templates for the HRM/Payroll module.
 *
 * Each template implements the DesignerTemplateInterface contract so the
 * designer can discover, filter, and insert template formulas.
 *
 * @package FormulaDesigner\Templates
 * @since   2.0.0
 */
class FormulaDesigner_Templates_PayrollTemplateProvider
    implements FormulaDesigner_Contracts_DesignerTemplateInterface
{
    /**
     * Internal template data array.
     *
     * @var array
     */
    private $data = array();

    /**
     * Construct a single payroll template.
     *
     * @param array $data Template metadata array.
     */
    public function __construct(array $data = array())
    {
        $this->data = $data;
    }

    /**
     * Get the unique template identifier.
     *
     * @return string
     */
    public function getId()
    {
        return isset($this->data['id']) ? (string)$this->data['id'] : '';
    }

    /**
     * Get the template formula text (NFX).
     *
     * @return string
     */
    public function getFormula()
    {
        return isset($this->data['formula']) ? (string)$this->data['formula'] : '';
    }

    /**
     * Get template metadata for rendering and filtering.
     *
     * @return array
     */
    public function getMetadata()
    {
        return array(
            'id'          => $this->getId(),
            'module'      => isset($this->data['module']) ? (string)$this->data['module'] : 'hrm',
            'label'       => isset($this->data['label']) ? (string)$this->data['label'] : '',
            'description' => isset($this->data['description']) ? (string)$this->data['description'] : '',
            'formula'     => $this->getFormula(),
            'category'    => isset($this->data['category']) ? (string)$this->data['category'] : 'Payroll',
            'tags'        => isset($this->data['tags']) && is_array($this->data['tags']) ? $this->data['tags'] : array(),
            'difficulty'  => isset($this->data['difficulty']) ? (string)$this->data['difficulty'] : 'beginner',
        );
    }

    // -----------------------------------------------------------------------
    // Static factory: all 10 payroll templates
    // -----------------------------------------------------------------------

    /**
     * Return the complete set of payroll formula templates.
     *
     * @return FormulaDesigner_Templates_PayrollTemplateProvider[]
     */
    public static function all()
    {
        return array(
            // 1. Basic Salary Calculation
            new self(array(
                'id'          => 'payroll.basic_salary_calc',
                'module'      => 'hrm',
                'label'       => 'Basic Salary Calculation',
                'description' => 'Calculate daily basic salary: basic divided by working days in the month.',
                'formula'     => 'ROUND(Employee.BasicSalary / Payroll.WorkingDays, 2)',
                'category'    => 'Salary Components',
                'tags'        => array('salary', 'basic', 'daily'),
                'difficulty'  => 'beginner',
            )),

            // 2. Gross Salary from Components
            new self(array(
                'id'          => 'payroll.gross_salary_components',
                'module'      => 'hrm',
                'label'       => 'Gross Salary (Component Sum)',
                'description' => 'Sum multiple salary components to derive gross salary.',
                'formula'     => 'Payroll.Basic + Payroll.HRA + Payroll.Travel + Payroll.Medical + Payroll.Other',
                'category'    => 'Salary Components',
                'tags'        => array('gross', 'salary', 'sum', 'components'),
                'difficulty'  => 'beginner',
            )),

            // 3. Overtime Pay Calculation
            new self(array(
                'id'          => 'payroll.overtime_pay',
                'module'      => 'hrm',
                'label'       => 'Overtime Pay',
                'description' => 'Calculate overtime pay: hourly rate × 1.5 × overtime hours.',
                'formula'     => 'ROUND(Payroll.HourlyRate * 1.5 * Payroll.OvertimeHours, 2)',
                'category'    => 'Salary Components',
                'tags'        => array('overtime', 'hourly', 'rate'),
                'difficulty'  => 'beginner',
            )),

            // 4. Absent Deduction (Pro-Rata)
            new self(array(
                'id'          => 'payroll.absent_deduction',
                'module'      => 'hrm',
                'label'       => 'Absent Deduction (Pro-Rata)',
                'description' => 'Deduct salary proportionally for absent days.',
                'formula'     => 'ROUND(Employee.BasicSalary / Payroll.WorkingDays * Payroll.AbsentDays, 2)',
                'category'    => 'Deductions',
                'tags'        => array('absent', 'deduction', 'pro-rata'),
                'difficulty'  => 'beginner',
            )),

            // 5. Leave Encashment
            new self(array(
                'id'          => 'payroll.leave_encashment',
                'module'      => 'hrm',
                'label'       => 'Leave Encashment',
                'description' => 'Calculate leave encashment value based on daily rate.',
                'formula'     => 'ROUND(Employee.BasicSalary / Payroll.WorkingDays * Leave.Encashed, 2)',
                'category'    => 'Leave',
                'tags'        => array('leave', 'encashment', 'daily'),
                'difficulty'  => 'intermediate',
            )),

            // 6. Net Salary Calculation
            new self(array(
                'id'          => 'payroll.net_salary',
                'module'      => 'hrm',
                'label'       => 'Net Salary',
                'description' => 'Calculate net salary: gross minus all deductions.',
                'formula'     => 'Payroll.Gross - Payroll.Tax - Payroll.Insurance - Payroll.Loan - Payroll.OtherDeductions',
                'category'    => 'Salary Components',
                'tags'        => array('net', 'salary', 'deductions', 'take-home'),
                'difficulty'  => 'intermediate',
            )),

            // 7. Monthly Tax (Simple Flat Rate)
            new self(array(
                'id'          => 'payroll.monthly_tax_flat',
                'module'      => 'hrm',
                'label'       => 'Monthly Tax (Flat Rate)',
                'description' => 'Calculate simple monthly tax at a fixed percentage of gross.',
                'formula'     => 'ROUND(Payroll.Gross * Payroll.TaxRate, 2)',
                'category'    => 'Tax',
                'tags'        => array('tax', 'flat', 'monthly'),
                'difficulty'  => 'beginner',
            )),

            // 8. Employer Contribution (e.g. EPF)
            new self(array(
                'id'          => 'payroll.employer_contribution',
                'module'      => 'hrm',
                'label'       => 'Employer EPF Contribution',
                'description' => 'Calculate employer EPF contribution at 13% of basic salary.',
                'formula'     => 'ROUND(Employee.BasicSalary * 0.13, 2)',
                'category'    => 'Contributions',
                'tags'        => array('epf', 'employer', 'contribution', 'provident'),
                'difficulty'  => 'beginner',
            )),

            // 9. Bonus Calculation
            new self(array(
                'id'          => 'payroll.annual_bonus',
                'module'      => 'hrm',
                'label'       => 'Annual Bonus (Percentage of Basic)',
                'description' => 'Calculate annual bonus as a percentage of basic salary.',
                'formula'     => 'ROUND(Employee.BasicSalary * 1.5, 2)',
                'category'    => 'Bonus',
                'tags'        => array('bonus', 'annual', 'percentage'),
                'difficulty'  => 'intermediate',
            )),

            // 10. Prorated Salary (Mid-Month Joining)
            new self(array(
                'id'          => 'payroll.prorated_salary',
                'module'      => 'hrm',
                'label'       => 'Prorated Salary (Mid-Month Join)',
                'description' => 'Calculate salary for partial month based on days worked.',
                'formula'     => 'ROUND(Employee.BasicSalary / Payroll.DaysInMonth * Payroll.DaysWorked, 2)',
                'category'    => 'Salary Components',
                'tags'        => array('prorated', 'join', 'partial', 'month'),
                'difficulty'  => 'beginner',
            )),
        );
    }
}
