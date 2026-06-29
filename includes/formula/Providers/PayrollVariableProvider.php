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
 * PayrollVariableProvider — Resolves Payroll.* namespace variables.
 *
 * Provides formula access to payroll-specific context through the
 * "Payroll" namespace. Variables are resolved lazily from the
 * FormulaContext's business data or legacy variable store.
 *
 * Supported variables:
 *   Payroll.Basic               — Employee basic salary for the period
 *   Payroll.Gross               — Gross salary for the period
 *   Payroll.DaysWorked           — Days actually worked in the period
 *   Payroll.PayableDays          — Payable days for the period
 *   Payroll.WorkingDays          — Total working days in the period
 *   Payroll.DaysInMonth          — Days in the current month
 *   Payroll.LeaveDays            — Total leave days taken
 *   Payroll.PaidLeaveDays        — Paid leave days
 *   Payroll.AbsentDays           — Absent days (unpaid)
 *   Payroll.OvertimeHours        — Overtime hours worked
 *   Payroll.UnpaidLeaveDays      — Unpaid leave days
 *   Payroll.HourlyRate           — Calculated hourly rate
 *   Payroll.TaxRate              — Tax rate for the employee
 *   Payroll.PeriodStart          — Payroll period start date
 *   Payroll.PeriodEnd            — Payroll period end date
 *
 * Backward compatibility: All variable names match the legacy
 * payroll_formula_engine context variables exactly. Simple (unqualified)
 * variable names like BASIC, GROSS, DAYS_WORKED are also resolved
 * through the FormulaContext's flat variable store for compatibility.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_PayrollVariableProvider implements Formula_Contracts_VariableProviderInterface
{
    /**
     * {@inheritdoc}
     */
    public function supports($namespace)
    {
        return strcasecmp((string)$namespace, 'Payroll') === 0;
    }

    /**
     * {@inheritdoc}
     *
     * @param string                        $identifier The payroll attribute name
     * @param Formula_Context_FormulaContext $context    The execution context
     * @return mixed The resolved attribute value
     * @throws Formula_Exceptions_UnknownVariableException If the attribute is not recognized
     */
    public function resolve($identifier, Formula_Context_FormulaContext $context)
    {
        $key = strtoupper((string)$identifier);

        // Attempt to resolve from the context's business data
        $payrollData = $context->getBusinessData('payroll', array());

        if (!empty($payrollData)) {
            $value = $this->resolveFromData($key, $payrollData);
            if ($value !== null) {
                return $value;
            }
        }

        // Fallback: resolve from legacy flat variable store (for backward compatibility)
        // The FormulaContext's getVariable() returns 0.0 for missing variables
        // when compatibility mode is enabled, matching legacy behavior.
        if ($context->hasVariable($key)) {
            return $context->getVariable($key);
        }

        // In compatibility mode, undefined variables return 0.0
        if ($context->isCompatibilityMode()) {
            return 0.0;
        }

        throw new Formula_Exceptions_UnknownVariableException(
            'Unknown payroll attribute: ' . $identifier,
            'Payroll',
            $identifier
        );
    }

    /**
     * Resolve from payroll business data array.
     *
     * @param string $key  The uppercase attribute name
     * @param array  $data The payroll data array from context
     * @return mixed|null
     */
    private function resolveFromData($key, array $data)
    {
        // Map payroll attribute keys to possible array keys in the data
        $map = array(
            'BASIC'              => array('basic', 'BASIC', 'basic_salary'),
            'GROSS'              => array('gross', 'GROSS', 'gross_salary'),
            'DAYSWORKED'         => array('days_worked', 'DAYS_WORKED', 'worked_days'),
            'PAYABLEDAYS'        => array('payable_days', 'PAYABLE_DAYS', 'pay_days'),
            'WORKINGDAYS'        => array('working_days', 'WORKING_DAYS', 'total_working_days'),
            'DAYSINMONTH'        => array('days_in_month', 'DAYS_IN_MONTH', 'month_days'),
            'LEAVEDAYS'          => array('leave_days', 'LEAVE_DAYS', 'total_leave_days'),
            'PAIDLEAVEDAYS'      => array('paid_leave_days', 'PAID_LEAVE_DAYS'),
            'ABSENTDAYS'         => array('absent_days', 'ABSENT_DAYS', 'absence_days'),
            'OVERTIMEHOURS'      => array('overtime_hours', 'OVERTIME_HOURS', 'ot_hours'),
            'UNPAIDLEAVEDAYS'    => array('unpaid_leave_days', 'UNPAID_LEAVE_DAYS'),
            'HOURLYRATE'         => array('hourly_rate', 'HOURLY_RATE', 'rate_per_hour'),
            'TAXRATE'            => array('tax_rate', 'TAX_RATE', 'income_tax_rate'),
            'PERIODSTART'        => array('period_start', 'from_date', 'pay_period_start'),
            'PERIODEND'          => array('period_end', 'to_date', 'pay_period_end'),
        );

        if (isset($map[$key])) {
            foreach ($map[$key] as $candidate) {
                if (array_key_exists($candidate, $data)) {
                    $value = $data[$candidate];
                    // Ensure numeric values are returned as float
                    $numericKeys = array(
                        'BASIC', 'GROSS', 'DAYSWORKED', 'PAYABLEDAYS',
                        'WORKINGDAYS', 'DAYSINMONTH', 'LEAVEDAYS', 'PAIDLEAVEDAYS',
                        'ABSENTDAYS', 'OVERTIMEHOURS', 'UNPAIDLEAVEDAYS',
                        'HOURLYRATE', 'TAXRATE',
                    );
                    if (in_array($key, $numericKeys, true)) {
                        return (float)$value;
                    }
                    return $value;
                }
            }
        }

        return null;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_ProviderMetadata(array(
            'namespaces'  => array('Payroll'),
            'version'     => '1.0',
            'description' => 'Resolves payroll attributes: Basic, Gross, DaysWorked, OvertimeHours, HourlyRate, TaxRate, and more.',
        ));
    }
}
