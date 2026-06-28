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
 * FormulaFunctionInterface — Contract for all registered formula functions.
 *
 * Every built-in function (150+ from Excel registry) and every extension
 * function MUST implement this interface. The Formula Engine discovers
 * functions exclusively through the FunctionRegistry; there is no other
 * mechanism for function registration.
 *
 * Functions are case-insensitive by name. The getName() method should
 * return the canonical uppercase name.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
interface Formula_Contracts_FormulaFunctionInterface
{
    /**
     * Get the canonical function name (case-insensitive, uppercase).
     *
     * @return string Function name (e.g., 'ABS', 'ROUND', 'SUM')
     */
    public function getName();

    /**
     * Execute the function with resolved arguments.
     *
     * Arguments are already evaluated by the runtime before this method
     * is called. The context provides access to security permissions,
     * company data, and other environmental information.
     *
     * @param Formula_Context_FormulaContext $context Immutable execution context
     * @param array                          $arguments  Resolved argument values in order
     * @return mixed The function result (float, boolean, string, date, array, null)
     * @throws Formula_Exceptions_PermissionDeniedException if user lacks required permission
     * @throws Formula_Exceptions_RuntimeExecutionException on evaluation error
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments);

    /**
     * Get the complete metadata for this function.
     *
     * Metadata includes argument counts, return type, determinism,
     * cacheability, required permissions, and deprecation information.
     *
     * @return Formula_Registry_FunctionMetadata
     */
    public function getMetadata();
}
