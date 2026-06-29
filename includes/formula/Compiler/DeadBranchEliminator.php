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
 * DeadBranchEliminator — AST optimizer that removes unreachable
 * branches from conditional expressions (IF) when the condition is a
 * compile-time constant.
 *
 * ## Purpose
 *
 * When an IF() condition can be statically determined to be TRUE or
 * FALSE (after constant folding), the dead branch is eliminated
 * entirely. This reduces AST size and prevents the runtime from
 * evaluating unused code paths.
 *
 * ## Transformations
 *
 *   | Condition     | Before                    | After  |
 *   |---------------|---------------------------|--------|
 *   | TRUE          | IF(TRUE, A, B)            | A      |
 *   | FALSE         | IF(FALSE, A, B)           | B      |
 *   | 1 > 0         | IF(1 > 0, A, B)           | A      |
 *   | 5 == 3        | IF(5 == 3, A, B)          | B      |
 *   | TRUE AND TRUE | IF(TRUE AND TRUE, A, B)   | A      |
 *
 * Dead-branch elimination depends on constant folding having already
 * run to reduce conditions to boolean literals. When the condition is
 * not constant (e.g., a variable reference), no elimination occurs.
 *
 * ## Safety
 *
 * NFX is a pure expression language with no side effects. Therefore,
 * eliminating a dead branch is ALWAYS safe — no observable behavior
 * can depend on an expression that is never evaluated.
 *
 * Short-circuit semantics are preserved: the runtime would never
 * evaluate the dead branch anyway. This optimization simply makes
 * that explicit in the AST.
 *
 * ## Immutability
 *
 * This optimizer NEVER mutates the input AST. Eliminated branches
 * are replaced by their surviving counterpart node.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 * @see     Formula_Contracts_OptimizerInterface
 * @see     Formula_Compiler_AST_NodeVisitor
 */
class Formula_Compiler_DeadBranchEliminator
    implements Formula_Contracts_OptimizerInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var string[] Descriptions of transformations applied */
    private $transformations = array();

    // -----------------------------------------------------------------------
    //  OptimizerInterface
    // -----------------------------------------------------------------------

    /**
     * Optimize the AST by eliminating dead conditional branches.
     *
     * @param Formula_Compiler_AST_Node $ast The validated AST to optimize
     * @return Formula_Compiler_AST_Node A new, optimized AST
     */
    public function optimize(Formula_Compiler_AST_Node $ast)
    {
        $this->transformations = array();
        return $ast->accept($this);
    }

    /**
     * @return string
     */
    public function getName()
    {
        return 'DeadBranchEliminator';
    }

    /**
     * @return string[]
     */
    public function getTransformationsApplied()
    {
        return $this->transformations;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Pass-through for leaf nodes
    // -----------------------------------------------------------------------

    /** @return Formula_Compiler_AST_LiteralNode */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node) { return $node; }

    /** @return Formula_Compiler_AST_VariableNode */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node) { return $node; }

    /** @return Formula_Compiler_AST_NullNode */
    public function visitNull(Formula_Compiler_AST_NullNode $node) { return $node; }

    /** @return Formula_Compiler_AST_RangeNode */
    public function visitRange(Formula_Compiler_AST_RangeNode $node) { return $node; }

    /**
     * Recursively optimize children, reconstruct if changed.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        if ($left === $node->left && $right === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_BinaryOperatorNode(
            $node->operator,
            $left,
            $right,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    /**
     * Recursively optimize operand, reconstruct if changed.
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        $operand = $node->operand->accept($this);

        if ($operand === $node->operand) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_UnaryOperatorNode(
            $node->operator,
            $operand,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    /**
     * Recursively optimize children, reconstruct if changed.
     *
     * @param Formula_Compiler_AST_ComparisonNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        if ($left === $node->left && $right === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_ComparisonNode(
            $node->operator,
            $left,
            $right,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    /**
     * Recursively optimize children, reconstruct if changed.
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        if ($left === $node->left && $right === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_LogicalNode(
            $node->operator,
            $left,
            $right,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    /**
     * Recursively optimize function arguments.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        $argsChanged = false;
        $optimizedArgs = array();

        foreach ($node->arguments as $arg) {
            $optimized = $arg->accept($this);
            $optimizedArgs[] = $optimized;
            if ($optimized !== $arg) {
                $argsChanged = true;
            }
        }

        if (!$argsChanged) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_FunctionNode(
            $node->functionName,
            $optimizedArgs,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    /**
     * Eliminate dead branches when the condition is a compile-time
     * constant boolean.
     *
     * This is the core optimization. After constant folding, IF()
     * conditions may resolve to literal TRUE or FALSE. When they do,
     * the entire conditional is replaced by the surviving branch.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        // First, recursively optimize the condition and both branches.
        // This ensures nested IF() or complex conditions are handled.
        $cond  = $node->condition->accept($this);
        $true  = $node->trueBranch->accept($this);
        $false = $node->falseBranch->accept($this);

        // Check if the condition is a constant boolean literal
        if ($cond instanceof Formula_Compiler_AST_LiteralNode
            && $cond->dataType === 'boolean') {

            if ($cond->value === true) {
                $this->transformations[] = sprintf(
                    'Dead branch elimination: IF(TRUE, ..., ...) → true branch (line %d)',
                    $node->getSourceLine()
                );
                return $true;
            }

            if ($cond->value === false) {
                $this->transformations[] = sprintf(
                    'Dead branch elimination: IF(FALSE, ..., ...) → false branch (line %d)',
                    $node->getSourceLine()
                );
                return $false;
            }
        }

        // Check if both branches are identical — if so, the IF is pointless
        // and we can return either branch regardless of condition.
        if ($this->nodesAreIdentical($true, $false)) {
            $this->transformations[] = sprintf(
                'Dead branch elimination: IF(cond, X, X) → X (identical branches, line %d)',
                $node->getSourceLine()
            );
            return $true;
        }

        // Reconstruct if any child changed
        if ($cond === $node->condition
            && $true === $node->trueBranch
            && $false === $node->falseBranch) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_ConditionalNode(
            $cond,
            $true,
            $false,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    // -----------------------------------------------------------------------
    //  Helpers
    // -----------------------------------------------------------------------

    /**
     * Check if two AST nodes are structurally identical.
     *
     * @param Formula_Compiler_AST_Node $a
     * @param Formula_Compiler_AST_Node $b
     * @return bool
     */
    private function nodesAreIdentical($a, $b)
    {
        if ($a === $b) {
            return true;
        }

        if ($a->getNodeType() !== $b->getNodeType()) {
            return false;
        }

        $serialA = $a->serialize();
        $serialB = $b->serialize();

        unset($serialA['sourceLine'], $serialA['sourceColumn']);
        unset($serialB['sourceLine'], $serialB['sourceColumn']);

        return $serialA == $serialB;
    }
}
