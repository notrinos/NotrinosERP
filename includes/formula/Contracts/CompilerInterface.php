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
 * CompilerInterface — Contract for the formula compiler.
 *
 * The compiler converts a raw formula string into a CompiledFormula
 * value object containing an optimized, immutable AST and metadata.
 *
 * Implementations handle the full pipeline: lexer → parser →
 * validation → optimization → compilation.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
interface Formula_Contracts_CompilerInterface
{
    /**
     * Compile a formula string into a compiled, optimized form.
     *
     * The compilation process:
     * 1. Preprocess the formula string (trim, normalize, strip leading =)
     * 2. Lex into tokens
     * 3. Parse tokens into AST
     * 4. Validate AST (syntax, semantic, type, dependency checks)
     * 5. Optimize AST (constant folding, dead branch elimination, etc.)
     * 6. Produce CompiledFormula with metadata
     *
     * @param string $formula The raw formula source string
     * @return Formula_Compiler_CompiledFormula The compiled, optimized formula
     * @throws Formula_Exceptions_SyntaxErrorException On lexer/parser errors
     * @throws Formula_Exceptions_UnknownFunctionException On unknown function references
     * @throws Formula_Exceptions_UnknownVariableException On unknown variable references
     * @throws Formula_Exceptions_TypeMismatchException On type incompatibility
     * @throws Formula_Exceptions_CircularReferenceException On circular dependencies
     * @throws Formula_Exceptions_ResourceExhaustedException On resource limit exceeded
     */
    public function compile($formula);
}
