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
 * FormulaContext — Immutable execution context for formula evaluation.
 *
 * The FormulaContext is the single data carrier for all formula evaluation.
 * It contains:
 *  - Simple key-value variables (backward compatible with payroll engine)
 *  - Company-specific configuration (currency, locale, fiscal year)
 *  - Security/permission context for access control
 *  - Module-specific business data (employee, payslip, stock item, etc.)
 *
 * The context is IMMUTABLE after construction. It is created by the calling
 * module via FormulaContextBuilder and read by the Formula Framework during
 * execution. The framework NEVER modifies the context.
 *
 * The Formula Engine contains ZERO business data. Every piece of data needed
 * for evaluation arrives through this context. This ensures:
 *  - Deterministic evaluation (same context = same result)
 *  - Multi-tenant isolation (no cross-company data leaks)
 *  - Unit testability (no database, no session, no global state)
 *  - Thread safety (immutable, shareable across parallel evaluations)
 *
 * @package Formula\Context
 * @since   2.0.0
 */
class Formula_Context_FormulaContext
{
    /** @var array Simple key-value variable store (case-insensitive keys, uppercase) */
    private $variables;

    /** @var Formula_Context_CompanyContext|null Company-specific configuration */
    private $companyContext;

    /** @var Formula_Context_SecurityContext|null Security/permission context */
    private $securityContext;

    /** @var array Module-specific business data (typed sub-contexts) */
    private $businessData;

    /** @var bool Whether to use compatibility mode (undefined vars → 0.0) */
    private $compatibilityMode;

    /**
     * Private constructor — contexts are created via FormulaContextBuilder.
     *
     * @param array                                    $variables         Simple variable key-value pairs
     * @param Formula_Context_CompanyContext|null       $companyContext    Company configuration
     * @param Formula_Context_SecurityContext|null      $securityContext   Security permissions
     * @param array                                    $businessData      Module-specific data
     * @param bool                                     $compatibilityMode Whether missing variables return 0.0
     */
    public function __construct(
        array $variables = array(),
        Formula_Context_CompanyContext $companyContext = null,
        Formula_Context_SecurityContext $securityContext = null,
        array $businessData = array(),
        $compatibilityMode = false
    ) {
        $this->variables         = $variables;
        $this->companyContext    = $companyContext;
        $this->securityContext   = $securityContext;
        $this->businessData      = $businessData;
        $this->compatibilityMode = (bool)$compatibilityMode;
    }

    // -----------------------------------------------------------------------
    // Variable access (backward compatible with payroll_formula_engine)
    // -----------------------------------------------------------------------

    /**
     * Get a simple variable value by name.
     *
     * Variable names are case-insensitive and stored uppercase.
     * This is the primary mechanism for legacy payroll formulas
     * that use flat variable names like BASIC, GROSS, DAYS_WORKED.
     *
     * If compatibility mode is enabled (default during payroll migration),
     * missing variables silently return 0.0 instead of throwing an exception.
     *
     * @param string $name Variable name (case-insensitive)
     * @return mixed The variable value
     * @throws Formula_Exceptions_UnknownVariableException If variable not found and NOT in compatibility mode
     */
    public function getVariable($name)
    {
        $key = strtoupper((string)$name);

        if (isset($this->variables[$key])) {
            return $this->variables[$key];
        }

        if ($this->compatibilityMode) {
            return 0.0;
        }

        throw new Formula_Exceptions_UnknownVariableException(
            'Undefined variable: ' . $name,
            '',     // No namespace — simple (unqualified) variable
            $name   // The variable identifier
        );
    }

    /**
     * Check whether a simple variable exists in the context.
     *
     * @param string $name Variable name (case-insensitive)
     * @return bool
     */
    public function hasVariable($name)
    {
        return isset($this->variables[strtoupper((string)$name)]);
    }

    /**
     * Get all simple variables.
     *
     * @return array
     */
    public function getVariables()
    {
        return $this->variables;
    }

    /**
     * Check whether compatibility mode is active.
     *
     * In compatibility mode, undefined variables return 0.0
     * instead of throwing an exception. This matches the legacy
     * payroll_formula_engine behavior.
     *
     * @return bool
     */
    public function isCompatibilityMode()
    {
        return $this->compatibilityMode;
    }

    // -----------------------------------------------------------------------
    // Sub-context access
    // -----------------------------------------------------------------------

    /**
     * Get the company context, if set.
     *
     * @return Formula_Context_CompanyContext|null
     */
    public function getCompanyContext()
    {
        return $this->companyContext;
    }

    /**
     * Get the security context, if set.
     *
     * @return Formula_Context_SecurityContext|null
     */
    public function getSecurityContext()
    {
        return $this->securityContext;
    }

    // -----------------------------------------------------------------------
    // Business data access (module-specific)
    // -----------------------------------------------------------------------

    /**
     * Get a module-specific business data value by key.
     *
     * Modules attach domain data (employee records, payslip data,
     * stock items, etc.) to the context. The framework passes this
     * data to registered providers without understanding it.
     *
     * @param string $key     The business data key
     * @param mixed  $default Default value if key not found
     * @return mixed
     */
    public function getBusinessData($key, $default = null)
    {
        return isset($this->businessData[$key]) ? $this->businessData[$key] : $default;
    }

    /**
     * Get all business data.
     *
     * @return array
     */
    public function getBusinessDataAll()
    {
        return $this->businessData;
    }

    // -----------------------------------------------------------------------
    // Serialization (for caching and debugging)
    // -----------------------------------------------------------------------

    /**
     * Convert the context to an array for serialization.
     *
     * Note: Security context is NOT serialized for audit safety.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'variables'         => $this->variables,
            'compatibilityMode' => $this->compatibilityMode,
            'hasCompanyContext'  => ($this->companyContext !== null),
            'hasSecurityContext' => ($this->securityContext !== null),
            'businessKeys'      => array_keys($this->businessData),
        );
    }
}
