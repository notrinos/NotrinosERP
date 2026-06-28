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
 * RuntimeInterface — Contract for the formula runtime engine.
 *
 * The runtime executes a CompiledFormula against a FormulaContext,
 * walking the AST and producing the evaluation result.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
interface Formula_Contracts_RuntimeInterface
{
    /**
     * Execute a compiled formula against the given context.
     *
     * The runtime walks the AST in post-order (children before parent),
     * resolves variables through registered providers, and invokes
     * functions through the FunctionRegistry.
     *
     * Short-circuit evaluation is applied for AND, OR, and IF.
     *
     * @param Formula_Compiler_CompiledFormula $formula The compiled formula
     * @param Formula_Context_FormulaContext    $context The immutable execution context
     * @return mixed The evaluation result
     * @throws Formula_Exceptions_DivideByZeroException On division by zero
     * @throws Formula_Exceptions_TypeMismatchException On runtime type errors
     * @throws Formula_Exceptions_PermissionDeniedException On permission check failure
     * @throws Formula_Exceptions_ResourceExhaustedException On resource limit exceeded
     * @throws Formula_Exceptions_RuntimeExecutionException On unexpected runtime errors
     */
    public function execute(
        Formula_Compiler_CompiledFormula $formula,
        Formula_Context_FormulaContext $context
    );
}
