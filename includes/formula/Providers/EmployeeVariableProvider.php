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
 * EmployeeVariableProvider — Resolves Employee.* namespace variables.
 *
 * Provides formula access to employee attributes through the "Employee"
 * namespace. Variables are resolved lazily — the provider only fetches
 * data when a specific variable is referenced in the formula.
 *
 * Supported variables:
 *   Employee.Id               — Employee database ID
 *   Employee.Name              — Full name (first + last)
 *   Employee.FirstName         — First name
 *   Employee.LastName          — Last name
 *   Employee.BasicSalary       — Current basic salary
 *   Employee.GrossSalary       — Gross salary for the period
 *   Employee.Department        — Department name
 *   Employee.Designation       — Job title / designation
 *   Employee.JoinDate          — Date of joining
 *   Employee.ConfirmationDate  — Date of confirmation
 *   Employee.TerminationDate   — Date of termination (null if active)
 *   Employee.IsActive          — TRUE if the employee is currently active
 *   Employee.BankAccount       — Bank account number (restricted)
 *   Employee.TaxId             — Tax identification number
 *   Employee.EmployeeCode      — Employee code / payroll ID
 *
 * Security note: compensation fields require SA_HRM_VIEW_SALARY. Full bank
 * and tax values remain denied even when a caller has a masked-view policy.
 * A missing security context fails closed for every restricted field.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_EmployeeVariableProvider implements Formula_Contracts_VariableProviderInterface
{
    /**
     * Restricted fields and their required security permissions.
     *
     * SA_DENIED is an explicit containment marker. It prevents any masked
     * employee-page permission from becoming full-value formula authority.
     *
     * @var string[]
     */
    private static $restrictedFieldPermissions = array(
        'BASICSALARY' => 'SA_HRM_VIEW_SALARY',
        'GROSSSALARY' => 'SA_HRM_VIEW_SALARY',
        'BANKACCOUNT' => 'SA_DENIED',
        'TAXID' => 'SA_DENIED',
    );

    /**
     * {@inheritdoc}
     */
    public function supports($namespace)
    {
        return strcasecmp((string)$namespace, 'Employee') === 0;
    }

    /**
     * {@inheritdoc}
     *
     * @param string                        $identifier The employee attribute name
     * @param Formula_Context_FormulaContext $context    The execution context
     * @return mixed The resolved attribute value
     * @throws Formula_Exceptions_UnknownVariableException If the attribute is not recognized
     * @throws Formula_Exceptions_PermissionDeniedException If user lacks required access
     */
    public function resolve($identifier, Formula_Context_FormulaContext $context)
    {
        $key = strtoupper((string)$identifier);

        // Check permissions for restricted fields before reading business data.
        if (isset(self::$restrictedFieldPermissions[$key])) {
            $requiredPermission = self::$restrictedFieldPermissions[$key];
            $securityCtx = $context->getSecurityContext();
            if ($requiredPermission === 'SA_DENIED'
                || $securityCtx === null
                || !$securityCtx->hasPermission($requiredPermission)) {
                throw new Formula_Exceptions_PermissionDeniedException(
                    'Permission denied accessing Employee.' . $identifier
                    . '. ' . $requiredPermission . ' is required.',
                    $requiredPermission,
                    'Employee.' . $identifier
                );
            }
        }

        // Resolve from business data in context (pre-loaded by the calling module)
        $employeeData = $context->getBusinessData('employee', array());

        // Build a lookup map of supported attributes
        if (!empty($employeeData)) {
            $value = $this->resolveFromData($key, $employeeData);
            if ($value !== null) {
                return $value;
            }
        }

        // Attribute not found
        throw new Formula_Exceptions_UnknownVariableException(
            'Unknown employee attribute: ' . $identifier,
            'Employee',
            $identifier
        );
    }

    /**
     * Resolve a variable from pre-loaded employee data array.
     *
     * @param string $key  The uppercase attribute name
     * @param array  $data The employee data array from context
     * @return mixed|null The value, or null if not found
     */
    private function resolveFromData($key, array $data)
    {
        switch ($key) {
            case 'ID':
                return isset($data['id']) ? (int)$data['id'] : (isset($data['employee_id']) ? (int)$data['employee_id'] : null);

            case 'NAME':
                if (isset($data['name'])) {
                    return (string)$data['name'];
                }
                $first = isset($data['first_name']) ? (string)$data['first_name'] : '';
                $last  = isset($data['last_name']) ? (string)$data['last_name'] : '';
                return trim($first . ' ' . $last) ?: null;

            case 'FIRSTNAME':
                return isset($data['first_name']) ? (string)$data['first_name'] : null;

            case 'LASTNAME':
                return isset($data['last_name']) ? (string)$data['last_name'] : null;

            case 'BASICSALARY':
                return isset($data['basic_salary']) ? (float)$data['basic_salary']
                    : (isset($data['BASIC']) ? (float)$data['BASIC'] : null);

            case 'GROSSSALARY':
                return isset($data['gross_salary']) ? (float)$data['gross_salary']
                    : (isset($data['GROSS']) ? (float)$data['GROSS'] : null);

            case 'DEPARTMENT':
                return isset($data['department']) ? (string)$data['department']
                    : (isset($data['dept_name']) ? (string)$data['dept_name'] : null);

            case 'DESIGNATION':
                return isset($data['designation']) ? (string)$data['designation']
                    : (isset($data['job_title']) ? (string)$data['job_title'] : null);

            case 'JOINDATE':
                $val = isset($data['join_date']) ? $data['join_date'] : (isset($data['employment_start']) ? $data['employment_start'] : null);
                return $this->toDateTime($val);

            case 'CONFIRMATIONDATE':
                $val = isset($data['confirmation_date']) ? $data['confirmation_date'] : (isset($data['probation_end']) ? $data['probation_end'] : null);
                return $this->toDateTime($val);

            case 'TERMINATIONDATE':
                $val = isset($data['termination_date']) ? $data['termination_date'] : (isset($data['employment_end']) ? $data['employment_end'] : null);
                return $this->toDateTime($val);

            case 'ISACTIVE':
                if (isset($data['inactive'])) {
                    return !(bool)$data['inactive'];
                }
                if (isset($data['termination_date']) && !empty($data['termination_date'])) {
                    return false;
                }
                return true;

            case 'BANKACCOUNT':
                return isset($data['bank_account']) ? (string)$data['bank_account']
                    : (isset($data['bank_account_number']) ? (string)$data['bank_account_number'] : null);

            case 'TAXID':
                return isset($data['tax_id']) ? (string)$data['tax_id']
                    : (isset($data['tax_number']) ? (string)$data['tax_number'] : null);

            case 'EMPLOYEECODE':
                return isset($data['employee_code']) ? (string)$data['employee_code']
                    : (isset($data['code']) ? (string)$data['code']
                    : (isset($data['id']) ? (string)$data['id'] : null));

            default:
                // Fallback: direct key lookup on lowercase
                $lowerKey = strtolower($key);
                return isset($data[$lowerKey]) ? $data[$lowerKey] : (isset($data[$key]) ? $data[$key] : null);
        }
    }

    /**
     * Convert a value to DateTimeImmutable, if possible.
     *
     * @param mixed $value The raw date value
     * @return DateTimeImmutable|null
     */
    private function toDateTime($value)
    {
        if ($value instanceof DateTimeImmutable) {
            return $value;
        }
        if ($value instanceof DateTime) {
            return DateTimeImmutable::createFromMutable($value);
        }
        if (is_string($value) && $value !== '' && $value !== '0000-00-00') {
            try {
                return new DateTimeImmutable($value);
            } catch (Exception $e) {
                return null;
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
            'namespaces'  => array('Employee'),
            'version'     => '1.0',
            'description' => 'Resolves employee attributes: Id, Name, BasicSalary, Department, Designation, IsActive, and more.',
        ));
    }
}
