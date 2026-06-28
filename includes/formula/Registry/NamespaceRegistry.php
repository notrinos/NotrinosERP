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
 * NamespaceRegistry — Resolves qualified variable references.
 *
 * The NamespaceRegistry bridges the gap between a namespace-qualified
 * variable reference in a formula (e.g., "Employee.BasicSalary") and
 * the actual variable provider. It splits the dotted path into namespace
 * and identifier parts and delegates to the appropriate provider.
 *
 * In v1, the path depth is limited to 2 (Namespace.Identifier). Future
 * versions may support deeper hierarchies (e.g., Payroll.Tax.Rate).
 *
 * @package Formula\Registry
 * @since   2.0.0
 */
class Formula_Registry_NamespaceRegistry
{
    /** @var Formula_Registry_VariableRegistry */
    private $variableRegistry;

    /** @var int Maximum dot-separated parts in a variable path */
    const MAX_PATH_DEPTH = 2;

    /**
     * @param Formula_Registry_VariableRegistry $variableRegistry
     */
    public function __construct(Formula_Registry_VariableRegistry $variableRegistry)
    {
        $this->variableRegistry = $variableRegistry;
    }

    /**
     * Resolve a qualified variable reference.
     *
     * Splits "Employee.BasicSalary" into namespace "Employee" and
     * identifier "BasicSalary", then delegates to the appropriate
     * provider for resolution.
     *
     * @param string $qualifiedName The fully qualified variable name (e.g., "Employee.BasicSalary")
     * @param Formula_Context_FormulaContext $context The execution context
     * @return mixed The resolved value
     * @throws Formula_Exceptions_UnknownVariableException If no provider handles the namespace
     * @throws Formula_Exceptions_UnknownVariableException If the identifier is not recognized
     */
    public function resolve($qualifiedName, Formula_Context_FormulaContext $context)
    {
        $parts = explode('.', $qualifiedName);

        if (count($parts) > self::MAX_PATH_DEPTH) {
            throw new Formula_Exceptions_UnknownVariableException(
                'Too many nested namespaces in variable reference: ' . $qualifiedName,
                $parts[0],
                $qualifiedName
            );
        }

        if (count($parts) === 1) {
            // Simple (unqualified) variable — resolve from the context's
            // flat variable store for backward compatibility with payroll engine.
            return $context->getVariable($parts[0]);
        }

        // Qualified variable: Namespace.Identifier
        $namespace  = $parts[0];
        $identifier = $parts[1];

        $provider = $this->variableRegistry->getProvider($namespace);

        if ($provider === null) {
            throw new Formula_Exceptions_UnknownVariableException(
                'No variable provider registered for namespace: ' . $namespace,
                $namespace,
                $identifier
            );
        }

        return $provider->resolve($identifier, $context);
    }

    /**
     * Check whether a namespace has a registered provider.
     *
     * @param string $namespace
     * @return bool
     */
    public function hasNamespace($namespace)
    {
        return $this->variableRegistry->hasNamespace($namespace);
    }

    /**
     * Get all registered namespace names.
     *
     * @return string[]
     */
    public function namespaces()
    {
        return $this->variableRegistry->namespaces();
    }
}
