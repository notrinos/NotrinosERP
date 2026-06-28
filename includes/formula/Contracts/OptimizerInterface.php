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
 * OptimizerInterface — Contract for AST optimizers.
 *
 * Optimizers transform an AST into a semantically equivalent but more
 * efficient form. Examples include constant folding, boolean simplification,
 * dead branch elimination, and algebraic simplification.
 *
 * CRITICAL: Optimizers MUST produce NEW nodes. They must NEVER mutate
 * the input AST. This ensures cache correctness and thread safety.
 *
 * Optimizers also report what transformations they applied for use
 * in diagnostics and explain mode.
 *
 * @package Formula\Contracts
 * @since   2.0.0
 */
interface Formula_Contracts_OptimizerInterface
{
    /**
     * Optimize the AST.
     *
     * Produces a semantically equivalent but (potentially) smaller or
     * simpler AST. The original AST is NOT modified.
     *
     * @param Formula_Compiler_AST_Node $ast The validated AST to optimize
     * @return Formula_Compiler_AST_Node A new, optimized AST
     * @throws Formula_Exceptions_FormulaException If optimization fails
     */
    public function optimize(Formula_Compiler_AST_Node $ast);

    /**
     * Get the human-readable name of this optimizer.
     *
     * Used in diagnostics and explain mode to report which
     * optimizations were applied.
     *
     * @return string Optimizer name (e.g., 'ConstantFolder', 'DeadBranchEliminator')
     */
    public function getName();

    /**
     * Get the list of transformations applied during the last optimize() call.
     *
     * Each entry is a human-readable description of a transformation.
     * Example: "Constant folding: 2+3 -> 5", "Dead branch: IF(TRUE, X, Y) -> X"
     *
     * @return string[] Applied transformation descriptions
     */
    public function getTransformationsApplied();
}
