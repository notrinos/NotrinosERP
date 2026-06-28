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
 * ValidatorInterface — Contract for AST validators.
 *
 * Validators inspect the AST after parsing but before execution.
 * They check syntax, semantics, type compatibility, and dependencies.
 * Multiple validators can be composed into a validation pipeline.
 *
 * Validators are typically implemented as NodeVisitors that collect
 * errors during traversal rather than stopping on the first error.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
interface Formula_Contracts_ValidatorInterface
{
    /**
     * Validate an AST and return the validation result.
     *
     * Validators should collect ALL errors, not stop at the first one.
     * The returned ValidationResult aggregates errors from the full
     * AST traversal.
     *
     * @param Formula_Compiler_AST_Node $ast The root node of the AST
     * @return Formula_Compiler_ValidationResult
     */
    public function validate(Formula_Compiler_AST_Node $ast);
}
