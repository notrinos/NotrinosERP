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
 * FormulaContextBuilder — Fluent builder for immutable FormulaContext objects.
 *
 * Assembles a FormulaContext step-by-step with a fluent API. This follows
 * the same builder pattern used elsewhere in NotrinosERP (e.g., items_cart,
 * hrm_cart). After calling build(), the resulting FormulaContext is immutable.
 *
 * Usage example (payroll migration):
 *
 *   $context = FormulaContextBuilder::create()
 *       ->withCompany(get_current_company_data())
 *       ->withVariables(array(
 *           'BASIC' => 8000,
 *           'GROSS' => 8000,
 *           'DAYS_WORKED' => 20,
 *       ))
 *       ->withCompatibilityMode(true)
 *       ->build();
 *
 *   $amount = Formula::evaluate($component['formula'], $context);
 *
 * @package Formula\Context
 * @since   2.0.0
 */
class Formula_Context_FormulaContextBuilder
{
    /** @var array Simple key-value variables */
    private $variables = array();

    /** @var Formula_Context_CompanyContext|null */
    private $companyContext = null;

    /** @var Formula_Context_SecurityContext|null */
    private $securityContext = null;

    /** @var array Module-specific business data */
    private $businessData = array();

    /** @var bool Whether to use backward-compatibility mode */
    private $compatibilityMode = false;

    /**
     * Create a new builder instance.
     *
     * @return Formula_Context_FormulaContextBuilder
     */
    public static function create()
    {
        return new self();
    }

    /**
     * Set all simple variables at once.
     *
     * Variable names are converted to uppercase for case-insensitive
     * matching. Values are stored as-is (the variable provider or
     * runtime may coerce types as needed).
     *
     * @param array $vars Associative array of name => value pairs
     * @return $this
     */
    public function withVariables(array $vars)
    {
        foreach ($vars as $name => $value) {
            $this->withVariable($name, $value);
        }
        return $this;
    }

    /**
     * Set a single simple variable.
     *
     * @param string $name  Variable name (converted to uppercase)
     * @param mixed  $value Variable value
     * @return $this
     */
    public function withVariable($name, $value)
    {
        $this->variables[strtoupper((string)$name)] = $value;
        return $this;
    }

    /**
     * Set the company context from a company database row.
     *
     * @param array $companyRow Company database row (associative array)
     * @return $this
     */
    public function withCompany(array $companyRow)
    {
        $this->companyContext = new Formula_Context_CompanyContext($companyRow);
        return $this;
    }

    /**
     * Set the company context directly.
     *
     * @param Formula_Context_CompanyContext $companyContext
     * @return $this
     */
    public function withCompanyContext(Formula_Context_CompanyContext $companyContext)
    {
        $this->companyContext = $companyContext;
        return $this;
    }

    /**
     * Set the security context for permission checks.
     *
     * @param Formula_Context_SecurityContext $securityContext
     * @return $this
     */
    public function withSecurity(Formula_Context_SecurityContext $securityContext)
    {
        $this->securityContext = $securityContext;
        return $this;
    }

    /**
     * Set the security context from raw values.
     *
     * @param int      $userId           Current user database ID
     * @param string[] $permissions      Array of SA_* security areas
     * @param int[]    $allowedCompanies Array of company database IDs
     * @return $this
     */
    public function withSecurityFromValues($userId, array $permissions = array(), array $allowedCompanies = array())
    {
        $this->securityContext = new Formula_Context_SecurityContext(
            $userId,
            $permissions,
            $allowedCompanies
        );
        return $this;
    }

    /**
     * Attach module-specific business data.
     *
     * This is the primary mechanism for modules to pass domain data
     * (employee records, payslip data, stock items, etc.) to the
     * formula engine without the engine knowing about the data structure.
     *
     * @param string $key   Data key (e.g., 'employee', 'payslip', 'stock_item')
     * @param mixed  $value The data value
     * @return $this
     */
    public function withBusinessData($key, $value)
    {
        $this->businessData[(string)$key] = $value;
        return $this;
    }

    /**
     * Enable backward-compatibility mode.
     *
     * When enabled, undefined variables silently return 0.0 instead
     * of throwing UnknownVariableException. This matches the behavior
     * of the legacy payroll_formula_engine.
     *
     * This mode exists ONLY to support migration. New modules should
     * leave it disabled (the default) for strict variable checking.
     *
     * @param bool $enabled Whether to enable compatibility mode
     * @return $this
     */
    public function withCompatibilityMode($enabled = true)
    {
        $this->compatibilityMode = (bool)$enabled;
        return $this;
    }

    /**
     * Build the immutable FormulaContext.
     *
     * After this call, the builder can be reused for another context.
     * The returned FormulaContext is completely independent — modifying
     * the builder afterward does NOT affect the built context.
     *
     * @return Formula_Context_FormulaContext
     */
    public function build()
    {
        return new Formula_Context_FormulaContext(
            $this->variables,
            $this->companyContext,
            $this->securityContext,
            $this->businessData,
            $this->compatibilityMode
        );
    }
}
