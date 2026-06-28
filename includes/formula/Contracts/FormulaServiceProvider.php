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
 * FormulaServiceProvider — Abstract base class for extension formula providers.
 *
 * Extensions (modules in modules/) create a subclass of FormulaServiceProvider
 * and implement the register() method to register their custom functions,
 * variable providers, validators, optimizers, and hook subscriptions.
 *
 * The framework discovers extensions via hook_invoke_all('formula_register_providers')
 * during bootstrap. Each extension's register() method is called before the
 * registries are frozen.
 *
 * This follows the existing NotrinosERP hook pattern — developers already
 * understand hooks, so no new mechanism is needed.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
abstract class Formula_Contracts_FormulaServiceProvider
{
    /** @var Formula_Registry_FunctionRegistry */
    protected $functionRegistry;

    /** @var Formula_Registry_VariableRegistry */
    protected $variableRegistry;

    /**
     * Register functions, variables, validators, and hook subscriptions.
     *
     * Called during bootstrap, before registries are frozen.
     * Subclasses MUST implement this method.
     *
     * @return void
     */
    abstract public function register();

    /**
     * Boot the provider after all registrations are complete.
     *
     * Called after all providers have registered and registries are frozen.
     * Override for post-registration initialization (e.g., warmup caches).
     *
     * @return void
     */
    public function boot()
    {
        // Default: no-op. Override in subclasses.
    }

    /**
     * Set the function registry (called by framework during bootstrap).
     *
     * @param Formula_Registry_FunctionRegistry $registry
     * @return void
     */
    public function setFunctionRegistry(Formula_Registry_FunctionRegistry $registry)
    {
        $this->functionRegistry = $registry;
    }

    /**
     * Set the variable registry (called by framework during bootstrap).
     *
     * @param Formula_Registry_VariableRegistry $registry
     * @return void
     */
    public function setVariableRegistry(Formula_Registry_VariableRegistry $registry)
    {
        $this->variableRegistry = $registry;
    }

    /**
     * Convenience method: register a function.
     *
     * @param Formula_Contracts_FormulaFunctionInterface $function
     * @return void
     */
    protected function registerFunction(Formula_Contracts_FormulaFunctionInterface $function)
    {
        $this->functionRegistry->register($function);
    }

    /**
     * Convenience method: register a variable provider for a namespace.
     *
     * @param string                                       $namespace
     * @param Formula_Contracts_VariableProviderInterface $provider
     * @return void
     */
    protected function registerVariable($namespace, Formula_Contracts_VariableProviderInterface $provider)
    {
        $this->variableRegistry->register($namespace, $provider);
    }

    /**
     * Subscribe to a hook.
     *
     * Uses the existing NotrinosERP hook system (includes/hooks.inc).
     * The callback will be invoked when the hook fires.
     *
     * @param string   $hookName Hook name (e.g., 'formula_after_execute')
     * @param callable $callback Callable to invoke
     * @return void
     */
    protected function subscribeToHook($hookName, $callback)
    {
        // Hook subscription is managed by the existing hook system.
        // This method records the subscription for use during bootstrap.
        // The framework invokes these callbacks at the appropriate lifecycle points.
        //
        // In practice, this integrates with hook_invoke_all() — the callback
        // is stored and invoked when the hook fires.
        global $formula_hook_subscriptions;
        if (!isset($formula_hook_subscriptions)) {
            $formula_hook_subscriptions = array();
        }
        if (!isset($formula_hook_subscriptions[$hookName])) {
            $formula_hook_subscriptions[$hookName] = array();
        }
        $formula_hook_subscriptions[$hookName][] = $callback;
    }
}
