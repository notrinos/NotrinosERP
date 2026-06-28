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
 * VariableRegistry — Central registry of variable providers by namespace.
 *
 * Maps namespace strings (e.g., 'Employee', 'Payroll', 'Company') to
 * VariableProviderInterface implementations. When a formula references
 * a namespace-qualified variable like "Employee.BasicSalary", the
 * VariableRegistry finds the provider for the "Employee" namespace
 * and delegates resolution to it.
 *
 * The registry follows the same freeze lifecycle as FunctionRegistry:
 * populated during bootstrap, then frozen for the remainder of the request.
 *
 * @package Formula\Registry
 * @since   2.0.0
 */
class Formula_Registry_VariableRegistry
{
    /** @var Formula_Contracts_VariableProviderInterface[] namespace => provider */
    private $providers = array();

    /** @var bool Whether the registry is frozen */
    private $frozen = false;

    /**
     * Register a variable provider for a namespace.
     *
     * @param string                                       $namespace The namespace to register
     * @param Formula_Contracts_VariableProviderInterface $provider  The provider instance
     * @return void
     * @throws RuntimeException If registry is frozen
     * @throws RuntimeException If the namespace is already registered
     */
    public function register($namespace, Formula_Contracts_VariableProviderInterface $provider)
    {
        if ($this->frozen) {
            throw new RuntimeException(
                'VariableRegistry is frozen. Cannot register namespace: ' . $namespace
            );
        }

        $ns = strtolower($namespace);

        if (isset($this->providers[$ns])) {
            throw new RuntimeException(
                'Variable namespace already registered: ' . $namespace
            );
        }

        $this->providers[$ns] = $provider;
    }

    /**
     * Get the provider for a namespace.
     *
     * @param string $namespace The namespace to look up
     * @return Formula_Contracts_VariableProviderInterface|null The provider or null if not found
     */
    public function getProvider($namespace)
    {
        $ns = strtolower($namespace);
        return isset($this->providers[$ns]) ? $this->providers[$ns] : null;
    }

    /**
     * Check whether a namespace has a registered provider.
     *
     * @param string $namespace
     * @return bool
     */
    public function hasNamespace($namespace)
    {
        return isset($this->providers[strtolower($namespace)]);
    }

    /**
     * Get all registered namespace names.
     *
     * @return string[]
     */
    public function namespaces()
    {
        return array_keys($this->providers);
    }

    /**
     * Freeze the registry, preventing further registrations.
     *
     * @return void
     */
    public function freeze()
    {
        $this->frozen = true;
    }

    /**
     * @return bool
     */
    public function isFrozen()
    {
        return $this->frozen;
    }

    /**
     * @return int
     */
    public function count()
    {
        return count($this->providers);
    }
}
