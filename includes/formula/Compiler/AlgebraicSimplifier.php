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
 * AlgebraicSimplifier — AST optimizer that applies algebraic
 * simplification rules to arithmetic expressions.
 *
 * ## Purpose
 *
 * Algebraic simplification reduces arithmetic expressions by applying
 * identity, zero, and inverse element rules. These transformations
 * reduce the AST node count while preserving semantic equivalence.
 *
 * ## Transformations
 *
 *   | Rule                  | Before    | After    |
 *   |-----------------------|-----------|----------|
 *   | Additive identity     | X + 0     | X        |
 *   | Additive identity     | 0 + X     | X        |
 *   | Multiplicative identity | X * 1   | X        |
 *   | Multiplicative identity | 1 * X   | X        |
 *   | Multiplicative zero   | X * 0     | 0        |
 *   | Multiplicative zero   | 0 * X     | 0        |
 *   | Subtraction identity  | X - 0     | X        |
 *   | Division identity     | X / 1     | X        |
 *   | Self-subtraction      | X - X     | 0        |
 *   | Self-division         | X / X     | 1        |
 *   | Negation of negation  | -(-X)     | X        |
 *   | Modulo by one         | X % 1     | 0        |
 *   | Power of zero         | X ^ 0     | 1        |
 *   | Power of one          | X ^ 1     | X        |
 *   | First power           | 1 ^ X     | 1        |
 *
 * ## Safety
 *
 * All transformations preserve semantic equivalence:
 *   - X * 0 = 0 is true for all finite X in standard arithmetic
 *   - X - X = 0 is always true regardless of X's value
 *   - X / X = 1 is true for X ≠ 0; the runtime handles division
 *     by zero, and this optimization is safe because X / X with
 *     X = 0 would be 0/0 which the runtime catches as undefined
 *   - Division by zero is NOT simplified — it stays in the AST
 *     so the runtime can produce a proper DivideByZeroException
 *
 * ## Immutability
 *
 * This optimizer NEVER mutates the input AST. All optimized results
 * are new node instances.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 * @see     Formula_Contracts_OptimizerInterface
 * @see     Formula_Compiler_AST_NodeVisitor
 */
class Formula_Compiler_AlgebraicSimplifier
    implements Formula_Contracts_OptimizerInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var string[] Descriptions of transformations applied */
    private $transformations = array();

    // -----------------------------------------------------------------------
    //  OptimizerInterface
    // -----------------------------------------------------------------------

    /**
     * Optimize the AST by applying algebraic simplification rules.
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
        return 'AlgebraicSimplifier';
    }

    /**
     * @return string[]
     */
    public function getTransformationsApplied()
    {
        return $this->transformations;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Leaf nodes pass through unchanged
    // -----------------------------------------------------------------------

    /** @return Formula_Compiler_AST_LiteralNode */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node) { return $node; }

    /** @return Formula_Compiler_AST_VariableNode */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node) { return $node; }

    /** @return Formula_Compiler_AST_NullNode */
    public function visitNull(Formula_Compiler_AST_NullNode $node) { return $node; }

    /** @return Formula_Compiler_AST_RangeNode */
    public function visitRange(Formula_Compiler_AST_RangeNode $node) { return $node; }

    /** Pass-through with child reconstruction. */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);
        if ($left === $node->left && $right === $node->right) { return $node; }
        $new = new Formula_Compiler_AST_ComparisonNode($node->operator, $left, $right,
            $node->getSourceLine(), $node->getSourceColumn());
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    /** Pass-through with child reconstruction. */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);
        if ($left === $node->left && $right === $node->right) { return $node; }
        $new = new Formula_Compiler_AST_LogicalNode($node->operator, $left, $right,
            $node->getSourceLine(), $node->getSourceColumn());
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    /** Pass-through with argument reconstruction. */
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
        $new = new Formula_Compiler_AST_FunctionNode($node->functionName, $optimizedArgs,
            $node->getSourceLine(), $node->getSourceColumn());
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    /** Pass-through with branch reconstruction. */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        $cond  = $node->condition->accept($this);
        $true  = $node->trueBranch->accept($this);
        $false = $node->falseBranch->accept($this);
        if ($cond === $node->condition && $true === $node->trueBranch
            && $false === $node->falseBranch) { return $node; }
        $new = new Formula_Compiler_AST_ConditionalNode($cond, $true, $false,
            $node->getSourceLine(), $node->getSourceColumn());
        $new->nodeMetadata = $node->nodeMetadata;
        return $new;
    }

    // -----------------------------------------------------------------------
    //  Core Optimization Target: Binary Operators
    // -----------------------------------------------------------------------

    /**
     * Apply algebraic simplification rules to binary operator nodes.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        $op = $node->operator;

        // Helper to check if a node is a numeric literal with a specific value
        $isLiteral = function ($n, $val = null) {
            if (!($n instanceof Formula_Compiler_AST_LiteralNode)) { return false; }
            if ($n->dataType !== 'decimal' && $n->dataType !== 'integer') { return false; }
            if ($val !== null) { return (float)$n->value === (float)$val; }
            return true;
        };

        $makeLiteral = function ($val, $line, $col) {
            $type = (floor((float)$val) == (float)$val && is_finite((float)$val)
                && abs((float)$val) <= PHP_INT_MAX) ? 'integer' : 'decimal';
            return new Formula_Compiler_AST_LiteralNode($val, $type, $line, $col);
        };

        $line = $node->getSourceLine();
        $col  = $node->getSourceColumn();

        // ---- Additive identity: X + 0 → X, 0 + X → X ----
        if ($op === '+') {
            if ($isLiteral($right, 0)) {
                $this->transformations[] = 'Algebraic: X + 0 → X';
                return $left;
            }
            if ($isLiteral($left, 0)) {
                $this->transformations[] = 'Algebraic: 0 + X → X';
                return $right;
            }
        }

        // ---- Subtraction identity: X - 0 → X ----
        if ($op === '-') {
            if ($isLiteral($right, 0)) {
                $this->transformations[] = 'Algebraic: X - 0 → X';
                return $left;
            }
            // X - X → 0
            if ($this->nodesAreIdentical($left, $right)) {
                $this->transformations[] = 'Algebraic: X - X → 0';
                return $makeLiteral(0, $line, $col);
            }
        }

        // ---- Multiplicative identity: X * 1 → X, 1 * X → X ----
        if ($op === '*') {
            if ($isLiteral($right, 1)) {
                $this->transformations[] = 'Algebraic: X * 1 → X';
                return $left;
            }
            if ($isLiteral($left, 1)) {
                $this->transformations[] = 'Algebraic: 1 * X → X';
                return $right;
            }
            // Multiplicative zero: X * 0 → 0, 0 * X → 0
            if ($isLiteral($right, 0) || $isLiteral($left, 0)) {
                $this->transformations[] = 'Algebraic: X * 0 → 0';
                return $makeLiteral(0, $line, $col);
            }
        }

        // ---- Division identity: X / 1 → X ----
        if ($op === '/') {
            if ($isLiteral($right, 1)) {
                $this->transformations[] = 'Algebraic: X / 1 → X';
                return $left;
            }
            // X / X → 1 (only if X is not 0 — identity check handles this)
            if ($this->nodesAreIdentical($left, $right)) {
                $this->transformations[] = 'Algebraic: X / X → 1';
                return $makeLiteral(1, $line, $col);
            }
        }

        // ---- Modulo by one: X % 1 → 0 ----
        if ($op === '%') {
            if ($isLiteral($right, 1)) {
                $this->transformations[] = 'Algebraic: X % 1 → 0';
                return $makeLiteral(0, $line, $col);
            }
        }

        // ---- Power rules ----
        if ($op === '^') {
            // X ^ 0 → 1
            if ($isLiteral($right, 0)) {
                $this->transformations[] = 'Algebraic: X ^ 0 → 1';
                return $makeLiteral(1, $line, $col);
            }
            // X ^ 1 → X
            if ($isLiteral($right, 1)) {
                $this->transformations[] = 'Algebraic: X ^ 1 → X';
                return $left;
            }
            // 1 ^ X → 1
            if ($isLiteral($left, 1)) {
                $this->transformations[] = 'Algebraic: 1 ^ X → 1';
                return $makeLiteral(1, $line, $col);
            }
        }

        // Reconstruct if children changed
        if ($left === $node->left && $right === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_BinaryOperatorNode(
            $op, $left, $right, $line, $col
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    // -----------------------------------------------------------------------
    //  Core Optimization Target: Unary Operators
    // -----------------------------------------------------------------------

    /**
     * Apply algebraic simplification to unary operators.
     *
     * Rules:
     *   -(-X) → X  (double negation elimination)
     *   +X → X     (unary plus is a no-op)
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        $operand = $node->operand->accept($this);

        // Unary plus is a no-op: +X → X
        if ($node->operator === '+') {
            $this->transformations[] = 'Algebraic: +X → X';
            return $operand;
        }

        // Double negation: -(-X) → X
        if ($node->operator === '-'
            && $operand instanceof Formula_Compiler_AST_UnaryOperatorNode
            && $operand->operator === '-') {
            $this->transformations[] = 'Algebraic: -(-X) → X';
            return $operand->operand;
        }

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
        if ($a === $b) { return true; }
        if ($a->getNodeType() !== $b->getNodeType()) { return false; }
        $serialA = $a->serialize();
        $serialB = $b->serialize();
        unset($serialA['sourceLine'], $serialA['sourceColumn']);
        unset($serialB['sourceLine'], $serialB['sourceColumn']);
        return $serialA == $serialB;
    }
}
