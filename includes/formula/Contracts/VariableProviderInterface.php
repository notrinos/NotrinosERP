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
 * VariableProviderInterface — Contract for all variable providers.
 *
 * Variable providers resolve namespace-qualified variable references
 * (e.g., "Employee.BasicSalary") at runtime. Each provider owns one
 * or more namespaces. The VariableRegistry maps namespaces to providers.
 *
 * Providers MUST resolve variables lazily — only fetching data when
 * the formula actually references a specific variable. They MUST
 * enforce security permissions for restricted fields.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
interface Formula_Contracts_VariableProviderInterface
{
    /**
     * Check whether this provider handles the given namespace.
     *
     * Called during variable resolution to find the appropriate
     * provider for a namespace-qualified variable reference.
     *
     * @param string $namespace The namespace to check (e.g., 'Employee', 'Payroll')
     * @return bool True if this provider resolves variables in this namespace
     */
    public function supports($namespace);

    /**
     * Resolve a variable value within a supported namespace.
     *
     * Called lazily — only when the formula actually references
     * the variable. The provider fetches the value from its data
     * source (which may be a database, context data, or computed value).
     *
     * @param string                        $identifier The variable name within the namespace
     * @param Formula_Context_FormulaContext $context    The immutable execution context
     * @return mixed The resolved variable value
     * @throws Formula_Exceptions_UnknownVariableException If the identifier is not recognized
     * @throws Formula_Exceptions_PermissionDeniedException If the user lacks access
     */
    public function resolve($identifier, Formula_Context_FormulaContext $context);

    /**
     * Get metadata describing this provider.
     *
     * @return Formula_Registry_ProviderMetadata
     */
    public function getMetadata();
}
