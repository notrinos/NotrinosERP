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
 * FormulaRuntime — Core formula execution engine.
 *
 * The FormulaRuntime orchestrates compilation and execution of formulas.
 * It holds references to the FunctionRegistry and VariableRegistry,
 * and delegates compilation to the compiler pipeline and execution to
 * the tree-walk evaluator.
 *
 * This class is NOT instantiated by modules. Modules use the static
 * FormulaFacade, which internally manages a singleton FormulaRuntime.
 *
 * The compiler pipeline (Lexer, Parser, Validators, Optimizers) will be
 * integrated into this class when those components are built.
 *
 * @package Formula\Runtime
 * @since   2.0.0
 */
class Formula_Runtime_FormulaRuntime
{
    /** @var Formula_Registry_FunctionRegistry */
    private $functionRegistry;

    /** @var Formula_Registry_VariableRegistry */
    private $variableRegistry;

    /**
     * Construct the runtime engine.
     *
     * @param Formula_Registry_FunctionRegistry $functionRegistry
     * @param Formula_Registry_VariableRegistry $variableRegistry
     */
    public function __construct(
        Formula_Registry_FunctionRegistry $functionRegistry,
        Formula_Registry_VariableRegistry $variableRegistry
    ) {
        $this->functionRegistry = $functionRegistry;
        $this->variableRegistry = $variableRegistry;
    }

    /**
     * Compile a formula string into a compiled form.
     *
     * @param string $formula The preprocessed formula string
     * @return Formula_Compiler_CompiledFormula
     * @throws RuntimeException The compiler is not yet implemented
     */
    public function compile($formula)
    {
        // The compiler pipeline (Lexer → Parser → Validator → Optimizer)
        // is not yet implemented. This method will delegate to the compiler
        // when those components are complete.
        //
        // For the current implementation state, see:
        //   includes/formula/Compiler/ — Token, TokenType, TokenStream exist
        //   includes/formula/Compiler/AST/ — Node types exist
        //   Pending: Lexer.php, Parser.php, Validator/*, Optimizer/*

        throw new RuntimeException(
            'Formula compilation is not yet available. '
            . 'The compiler pipeline (Lexer, Parser, Validators, Optimizers) '
            . 'is scheduled for implementation. Token and AST infrastructure is ready.'
        );
    }

    /**
     * Compile and evaluate in one call.
     *
     * @param string                        $formula The preprocessed formula string
     * @param Formula_Context_FormulaContext $context The execution context
     * @return mixed
     * @throws RuntimeException The compiler is not yet implemented
     */
    public function evaluate($formula, Formula_Context_FormulaContext $context)
    {
        $compiled = $this->compile($formula);
        return $this->execute($compiled, $context);
    }

    /**
     * Execute a compiled formula against a context.
     *
     * @param Formula_Compiler_CompiledFormula $compiled
     * @param Formula_Context_FormulaContext   $context
     * @return mixed
     * @throws RuntimeException The runtime evaluator is not yet implemented
     */
    public function execute(
        Formula_Compiler_CompiledFormula $compiled,
        Formula_Context_FormulaContext $context
    ) {
        throw new RuntimeException(
            'Formula execution is not yet available. '
            . 'The runtime evaluator (tree-walk AST interpreter) '
            . 'is scheduled for implementation.'
        );
    }

    /**
     * Validate a formula without executing.
     *
     * @param string $formula
     * @return Formula_Compiler_ValidationResult
     * @throws RuntimeException The validator is not yet implemented
     */
    public function validate($formula)
    {
        throw new RuntimeException(
            'Formula validation is not yet available. '
            . 'The validation pipeline is scheduled for implementation.'
        );
    }

    /**
     * Explain formula evaluation step by step.
     *
     * @param string                        $formula
     * @param Formula_Context_FormulaContext $context
     * @return Formula_Diagnostics_ExplainResult
     * @throws RuntimeException The explain mode is not yet implemented
     */
    public function explain($formula, Formula_Context_FormulaContext $context)
    {
        throw new RuntimeException(
            'Formula explain mode is not yet available. '
            . 'The ExplainVisitor is scheduled for implementation.'
        );
    }

    /**
     * Clear all formula caches.
     *
     * @return void
     */
    public function clearCache()
    {
        // Cache layer not yet implemented. This is a no-op for now.
        // When cache layers exist, this will delegate to SourceCache,
        // CompilerCache, and RuntimeCache clear() methods.
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
     * Get the variable registry.
     *
     * @return Formula_Registry_VariableRegistry
     */
    public function getVariableRegistry()
    {
        return $this->variableRegistry;
    }
}
