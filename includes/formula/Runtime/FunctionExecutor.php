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
 * FunctionExecutor — Resolves and invokes registered formula functions.
 *
 * The FunctionExecutor bridges the gap between the AST FunctionNode
 * (which holds a function name and evaluated argument values) and the
 * FunctionRegistry (which holds the actual implementations).
 *
 * Responsibilities:
 *  - Look up the function in the FunctionRegistry (case-insensitive)
 *  - Validate that the argument count matches the function's min/max args
 *  - Check permissions against the SecurityContext
 *  - Invoke execute() with the resolved arguments
 *  - Cache deterministic results within the session (L4 cache)
 *  - Track function call counts for diagnostics and resource limits
 *
 * This class is stateless — all resolution state lives in the
 * FunctionRegistry and RuntimeSession.
 *
 * @package Formula\Runtime
 * @since   2.0.0
 */
class Formula_Runtime_FunctionExecutor
{
    /** @var Formula_Registry_FunctionRegistry */
    private $functionRegistry;

    /**
     * Construct the function executor.
     *
     * @param Formula_Registry_FunctionRegistry $functionRegistry The frozen function registry
     */
    public function __construct(Formula_Registry_FunctionRegistry $functionRegistry)
    {
        $this->functionRegistry = $functionRegistry;
    }

    /**
     * Execute a registered function with resolved arguments.
     *
     * This is the callable interface wired into the NodeEvaluator.
     *
     * @param string                         $functionName The function name (case-insensitive)
     * @param array                          $arguments    Already-evaluated argument values
     * @param Formula_Context_FormulaContext  $context      The execution context
     * @return mixed The function result
     * @throws Formula_Exceptions_UnknownFunctionException If the function is not registered
     * @throws Formula_Exceptions_PermissionDeniedException If the user lacks required permission
     * @throws Formula_Exceptions_RuntimeExecutionException If argument count validation fails
     */
    public function execute($functionName, array $arguments, Formula_Context_FormulaContext $context)
    {
        $fn = $this->functionRegistry->get($functionName);

        if ($fn === null) {
            throw new Formula_Exceptions_UnknownFunctionException(
                'Function not found: ' . $functionName,
                0,
                0,
                $functionName
            );
        }

        $metadata = $fn->getMetadata();

        // Validate argument count
        $argCount = count($arguments);
        if ($metadata->minArgs >= 0 && $argCount < $metadata->minArgs) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                sprintf(
                    'Function %s expects at least %d argument(s), got %d.',
                    $functionName,
                    $metadata->minArgs,
                    $argCount
                )
            );
        }
        if ($metadata->maxArgs >= 0 && $argCount > $metadata->maxArgs) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                sprintf(
                    'Function %s expects at most %d argument(s), got %d.',
                    $functionName,
                    $metadata->maxArgs,
                    $argCount
                )
            );
        }

        // Permission check
        if ($metadata->requiredPermission !== null) {
            $security = $context->getSecurityContext();
            if ($security === null || !$security->hasPermission($metadata->requiredPermission)) {
                throw new Formula_Exceptions_PermissionDeniedException(
                    sprintf(
                        'Permission denied for function %s. Required: %s.',
                        $functionName,
                        $metadata->requiredPermission
                    ),
                    0,
                    0,
                    $metadata->requiredPermission,
                    $functionName
                );
            }
        }

        // Delegate to the function implementation
        return $fn->execute($context, $arguments);
    }

    /**
     * Get the function registry.
     *
     * @return Formula_Registry_FunctionRegistry
     */
    public function getFunctionRegistry()
    {
        return $this->functionRegistry;
    }

    /**
     * Validate that a function exists and its argument count is compatible.
     *
     * Used by the validator pipeline (compile time) to catch errors
     * before execution.
     *
     * @param string $functionName The function name
     * @param int    $argCount     Number of arguments provided
     * @return bool True if the call is valid
     * @throws Formula_Exceptions_UnknownFunctionException If function not found
     * @throws Formula_Exceptions_RuntimeExecutionException If argument count is invalid
     */
    public function validateCall($functionName, $argCount)
    {
        $fn = $this->functionRegistry->get($functionName);

        if ($fn === null) {
            throw new Formula_Exceptions_UnknownFunctionException(
                'Function not found: ' . $functionName,
                0,
                0,
                $functionName
            );
        }

        $metadata = $fn->getMetadata();

        if ($metadata->minArgs >= 0 && $argCount < $metadata->minArgs) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                sprintf(
                    'Function %s expects at least %d argument(s), got %d.',
                    $functionName,
                    $metadata->minArgs,
                    $argCount
                )
            );
        }

        if ($metadata->maxArgs >= 0 && $argCount > $metadata->maxArgs) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                sprintf(
                    'Function %s expects at most %d argument(s), got %d.',
                    $functionName,
                    $metadata->maxArgs,
                    $argCount
                )
            );
        }

        return true;
    }
}
