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
 * NullCoalescingOptimizer — AST optimizer that simplifies null coalescing
 * expressions (??) where the left-hand side is known at compile time.
 *
 * ## Purpose
 *
 * The null coalescing operator `??` evaluates to the left-hand side if it
 * is not NULL; otherwise, it evaluates to the right-hand side. When the
 * left operand is a compile-time constant, the coalescing can be resolved
 * at compile time, eliminating the right branch entirely.
 *
 * ## Transformations
 *
 *   | Left         | Before            | After      |
 *   |--------------|-------------------|------------|
 *   | Non-null     | 42 ?? Default     | 42         |
 *   | Non-null     | "Hello" ?? X      | "Hello"    |
 *   | Non-null     | TRUE ?? Expensive()| TRUE       |
 *   | Null literal | NULL ?? Default   | Default    |
 *   | Variable     | Salary ?? 0       | unchanged  |
 *
 * Variables can never be resolved at compile time, so `Salary ?? 0` is
 * left unchanged — the runtime must evaluate it.
 *
 * ## Safety
 *
 * Eliminating the right branch of `??` when the left is known non-null
 * is always safe because:
 *   - NFX is a pure expression language (no side effects)
 *   - The right branch would never be evaluated at runtime anyway
 *   - Short-circuit semantics are preserved
 *
 * ## Immutability
 *
 * This optimizer NEVER mutates the input AST.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 * @see     Formula_Contracts_OptimizerInterface
 * @see     Formula_Compiler_AST_NodeVisitor
 */
class Formula_Compiler_NullCoalescingOptimizer
    implements Formula_Contracts_OptimizerInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var string[] Descriptions of transformations applied */
    private $transformations = array();

    // -----------------------------------------------------------------------
    //  OptimizerInterface
    // -----------------------------------------------------------------------

    /**
     * Optimize the AST by simplifying null coalescing expressions.
     *
     * @param Formula_Compiler_AST_Node $ast
     * @return Formula_Compiler_AST_Node
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
        return 'NullCoalescingOptimizer';
    }

    /**
     * @return string[]
     */
    public function getTransformationsApplied()
    {
        return $this->transformations;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Leaf nodes
    // -----------------------------------------------------------------------

    /** @return Formula_Compiler_AST_LiteralNode */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node) { return $node; }

    /** @return Formula_Compiler_AST_VariableNode */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node) { return $node; }

    /** @return Formula_Compiler_AST_NullNode */
    public function visitNull(Formula_Compiler_AST_NullNode $node) { return $node; }

    /** @return Formula_Compiler_AST_RangeNode */
    public function visitRange(Formula_Compiler_AST_RangeNode $node) { return $node; }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Internal nodes (recursive)
    // -----------------------------------------------------------------------

    /**
     * Main optimization target: binary `??` operator.
     *
     * If the left operand is a non-null literal, return it directly
     * (eliminating the right branch). If the left operand is NULL,
     * return the right branch (which will be recursively optimized
     * by this visitor's accept() call chain).
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        if ($node->operator !== '??') {
            // Not a null coalescing operator — reconstruct if children changed
            if ($left === $node->left && $right === $node->right) {
                return $node;
            }
            $newNode = new Formula_Compiler_AST_BinaryOperatorNode(
                $node->operator, $left, $right,
                $node->getSourceLine(), $node->getSourceColumn()
            );
            $newNode->nodeMetadata = $node->nodeMetadata;
            return $newNode;
        }

        // ---- Null coalescing optimization ----

        // NULL ?? X → X
        if ($left instanceof Formula_Compiler_AST_NullNode) {
            $this->transformations[] = sprintf(
                'Null coalescing: NULL ?? X → X (line %d)',
                $node->getSourceLine()
            );
            return $right;
        }

        // Non-null literal ?? X → literal
        if ($left instanceof Formula_Compiler_AST_LiteralNode && $left->value !== null) {
            $this->transformations[] = sprintf(
                'Null coalescing: %s ?? X → %s (line %d)',
                $this->formatValue($left->value),
                $this->formatValue($left->value),
                $node->getSourceLine()
            );
            return $left;
        }

        // Variable ?? X — cannot resolve at compile time
        // Reconstruct if children changed
        if ($left === $node->left && $right === $node->right) {
            return $node;
        }
        $newNode = new Formula_Compiler_AST_BinaryOperatorNode(
            '??', $left, $right,
            $node->getSourceLine(), $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    /** Pass-through. */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        $operand = $node->operand->accept($this);
        if ($operand === $node->operand) { return $node; }
        $new = new Formula_Compiler_AST_UnaryOperatorNode(
            $node->operator, $operand,
            $node->getSourceLine(), $node->getSourceColumn()
        );
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    /** Pass-through. */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);
        if ($left === $node->left && $right === $node->right) { return $node; }
        $new = new Formula_Compiler_AST_ComparisonNode(
            $node->operator, $left, $right,
            $node->getSourceLine(), $node->getSourceColumn()
        );
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    /** Pass-through. */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);
        if ($left === $node->left && $right === $node->right) { return $node; }
        $new = new Formula_Compiler_AST_LogicalNode(
            $node->operator, $left, $right,
            $node->getSourceLine(), $node->getSourceColumn()
        );
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    /** Pass-through. */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        $argsChanged = false;
        $optimizedArgs = array();
        foreach ($node->arguments as $arg) {
            $opt = $arg->accept($this);
            $optimizedArgs[] = $opt;
            if ($opt !== $arg) { $argsChanged = true; }
        }
        if (!$argsChanged) { return $node; }
        $new = new Formula_Compiler_AST_FunctionNode(
            $node->functionName, $optimizedArgs,
            $node->getSourceLine(), $node->getSourceColumn()
        );
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    /** Pass-through. */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        $cond  = $node->condition->accept($this);
        $true  = $node->trueBranch->accept($this);
        $false = $node->falseBranch->accept($this);
        if ($cond === $node->condition && $true === $node->trueBranch
            && $false === $node->falseBranch) { return $node; }
        $new = new Formula_Compiler_AST_ConditionalNode(
            $cond, $true, $false,
            $node->getSourceLine(), $node->getSourceColumn()
        );
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    // -----------------------------------------------------------------------
    //  Helpers
    // -----------------------------------------------------------------------

    /**
     * Format a value for human-readable transformation messages.
     *
     * @param mixed $value
     * @return string
     */
    private function formatValue($value)
    {
        if ($value === null) { return 'NULL'; }
        if (is_bool($value)) { return $value ? 'TRUE' : 'FALSE'; }
        if (is_float($value)) {
            $str = rtrim(rtrim(sprintf('%.10f', $value), '0'), '.');
            return $str;
        }
        if (is_int($value)) { return (string)$value; }
        if (is_string($value)) { return '"' . $value . '"'; }
        return (string)$value;
    }
}
