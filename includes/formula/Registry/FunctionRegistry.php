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
 * FunctionRegistry — Central registry of all formula functions.
 *
 * The FunctionRegistry is a hash-map of function name (case-insensitive,
 * stored uppercase) to FormulaFunctionInterface implementations. It is
 * the SINGLE source of truth for which functions are available to formulas.
 *
 * The registry is populated during bootstrap:
 * 1. Built-in functions (150+ from Excel registry) are registered first
 * 2. Extensions register via hook_invoke_all('formula_register_providers')
 * 3. registry is frozen — no further registration allowed
 *
 * After freeze(), the registry is read-only for the remainder of the request.
 *
 * @package Formula\Registry
 * @since   2.0.0
 */
class Formula_Registry_FunctionRegistry
{
    /** @var Formula_Contracts_FormulaFunctionInterface[] */
    private $functions = array();

    /** @var bool Whether the registry is frozen (read-only) */
    private $frozen = false;

    /**
     * Register a function implementation.
     *
     * @param Formula_Contracts_FormulaFunctionInterface $function
     * @return void
     * @throws RuntimeException If registry is frozen
     * @throws RuntimeException If a function with the same name is already registered
     */
    public function register(Formula_Contracts_FormulaFunctionInterface $function)
    {
        if ($this->frozen) {
            throw new RuntimeException(
                'FunctionRegistry is frozen. Cannot register function: ' . $function->getName()
            );
        }

        $name = strtoupper($function->getName());

        if (isset($this->functions[$name])) {
            throw new RuntimeException(
                'Function already registered: ' . $name
            );
        }

        $this->functions[$name] = $function;
    }

    /**
     * Retrieve a function by name (case-insensitive).
     *
     * @param string $name The function name
     * @return Formula_Contracts_FormulaFunctionInterface|null The function or null if not found
     */
    public function get($name)
    {
        $key = strtoupper($name);
        return isset($this->functions[$key]) ? $this->functions[$key] : null;
    }

    /**
     * Check whether a function is registered.
     *
     * @param string $name The function name
     * @return bool
     */
    public function has($name)
    {
        return isset($this->functions[strtoupper($name)]);
    }

    /**
     * Get all registered functions.
     *
     * @return Formula_Contracts_FormulaFunctionInterface[]
     */
    public function all()
    {
        return $this->functions;
    }

    /**
     * Get all functions in a specific category.
     *
     * @param string $category Category name (e.g., 'Math', 'Logical', 'Date')
     * @return Formula_Contracts_FormulaFunctionInterface[]
     */
    public function byCategory($category)
    {
        $result = array();
        foreach ($this->functions as $name => $fn) {
            if (strcasecmp($fn->getMetadata()->category, $category) === 0) {
                $result[$name] = $fn;
            }
        }
        return $result;
    }

    /**
     * Search functions by name or description substring.
     *
     * @param string $query Search query
     * @return Formula_Contracts_FormulaFunctionInterface[]
     */
    public function search($query)
    {
        $query  = strtolower($query);
        $result = array();
        foreach ($this->functions as $name => $fn) {
            $meta = $fn->getMetadata();
            if (
                strpos(strtolower($name), $query) !== false
                || strpos(strtolower($meta->description), $query) !== false
            ) {
                $result[$name] = $fn;
            }
        }
        return $result;
    }

    /**
     * Freeze the registry, preventing further registrations.
     *
     * After freeze, the registry is read-only for the remainder
     * of the request. This is called after all extensions have
     * registered their functions during bootstrap.
     *
     * @return void
     */
    public function freeze()
    {
        $this->frozen = true;
    }

    /**
     * Check whether the registry is frozen.
     *
     * @return bool
     */
    public function isFrozen()
    {
        return $this->frozen;
    }

    /**
     * Get the count of registered functions.
     *
     * @return int
     */
    public function count()
    {
        return count($this->functions);
    }
}
