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
 * BooleanSimplifier — AST optimizer that applies boolean algebra
 * simplification rules.
 *
 * ## Purpose
 *
 * Boolean simplification reduces logical expressions to their simplest
 * equivalent form by applying identity, annihilation, idempotence, and
 * double-negation elimination rules. This reduces the AST node count
 * and enables further optimizations downstream.
 *
 * ## Transformations
 *
 *   | Rule                    | Before           | After    |
 *   |-------------------------|------------------|----------|
 *   | AND identity            | TRUE AND X       | X        |
 *   | AND identity (reverse)  | X AND TRUE       | X        |
 *   | AND annihilation        | FALSE AND X      | FALSE    |
 *   | AND annihilation (rev)  | X AND FALSE      | FALSE    |
 *   | OR identity             | FALSE OR X       | X        |
 *   | OR identity (reverse)   | X OR FALSE       | X        |
 *   | OR annihilation         | TRUE OR X        | TRUE     |
 *   | OR annihilation (rev)   | X OR TRUE        | TRUE     |
 *   | XOR with FALSE          | FALSE XOR X      | X        |
 *   | XOR with FALSE (rev)    | X XOR FALSE      | X        |
 *   | Double negation         | NOT(NOT(X))      | X        |
 *   | NOT of literal          | NOT TRUE         | FALSE    |
 *   | NOT of literal          | NOT FALSE        | TRUE     |
 *   | Idempotence             | X AND X          | X        |
 *   | Idempotence             | X OR X           | X        |
 *
 * ## Safety
 *
 * The boolean simplifier produces semantically equivalent output for all
 * inputs. Short-circuit semantics are preserved — if a function call
 * appears as X in "FALSE AND X", the function is NOT eliminated because
 * the ShortCircuitEvaluator at runtime would not call it. However, the
 * AST simplification to "FALSE" is correct because the overall expression
 * always evaluates to FALSE regardless of X's value.
 *
 * Note: Variable side effects are impossible in NFX (pure expression
 * language with no assignment), so eliminating unreferenced subtrees is
 * always safe.
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
class Formula_Compiler_BooleanSimplifier
    implements Formula_Contracts_OptimizerInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var string[] Descriptions of transformations applied */
    private $transformations = array();

    // -----------------------------------------------------------------------
    //  OptimizerInterface
    // -----------------------------------------------------------------------

    /**
     * Optimize the AST by simplifying boolean expressions.
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
        return 'BooleanSimplifier';
    }

    /**
     * @return string[]
     */
    public function getTransformationsApplied()
    {
        return $this->transformations;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Pass-through for non-logical nodes
    // -----------------------------------------------------------------------

    /**
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return Formula_Compiler_AST_LiteralNode
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node)
    {
        return $node;
    }

    /**
     * @param Formula_Compiler_AST_VariableNode $node
     * @return Formula_Compiler_AST_VariableNode
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node)
    {
        return $node;
    }

    /**
     * @param Formula_Compiler_AST_NullNode $node
     * @return Formula_Compiler_AST_NullNode
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node)
    {
        return $node;
    }

    /**
     * @param Formula_Compiler_AST_RangeNode $node
     * @return Formula_Compiler_AST_RangeNode
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node)
    {
        return $node;
    }

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

        // NOT applied to a constant boolean literal
        if ($operand instanceof Formula_Compiler_AST_LiteralNode
            && $operand->dataType === 'boolean') {

            $result = !((bool)$operand->value);
            $resultNode = new Formula_Compiler_AST_LiteralNode(
                $result,
                'boolean',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );

            $this->transformations[] = sprintf(
                'Boolean simplification: NOT %s → %s',
                $operand->value ? 'TRUE' : 'FALSE',
                $result ? 'TRUE' : 'FALSE'
            );

            return $resultNode;
        }

        // NOT(NOT(X)) → X (double negation elimination)
        if ($operand instanceof Formula_Compiler_AST_UnaryOperatorNode
            && $node->operator === 'NOT'
            && $operand->operator === 'NOT') {

            $this->transformations[] = 'Boolean simplification: NOT(NOT(X)) → X';
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
     * Apply boolean simplification rules to logical nodes.
     *
     * This is the main optimization target — AND, OR, XOR nodes
     * are simplified using identity, annihilation, and idempotence rules.
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        $leftIsLiteral  = $left instanceof Formula_Compiler_AST_LiteralNode;
        $rightIsLiteral = $right instanceof Formula_Compiler_AST_LiteralNode;

        // Check if left is a known boolean literal
        $leftIsTrue  = $leftIsLiteral && $left->dataType === 'boolean' && $left->value === true;
        $leftIsFalse = $leftIsLiteral && $left->dataType === 'boolean' && $left->value === false;
        $rightIsTrue  = $rightIsLiteral && $right->dataType === 'boolean' && $right->value === true;
        $rightIsFalse = $rightIsLiteral && $right->dataType === 'boolean' && $right->value === false;

        $op = $node->operator;

        // ---- AND rules ----
        if ($op === 'AND') {
            // FALSE AND X → FALSE (annihilation)
            if ($leftIsFalse) {
                $this->transformations[] = 'Boolean simplification: FALSE AND X → FALSE';
                return new Formula_Compiler_AST_LiteralNode(false, 'boolean',
                    $node->getSourceLine(), $node->getSourceColumn());
            }
            // X AND FALSE → FALSE (annihilation, reverse)
            if ($rightIsFalse) {
                $this->transformations[] = 'Boolean simplification: X AND FALSE → FALSE';
                return new Formula_Compiler_AST_LiteralNode(false, 'boolean',
                    $node->getSourceLine(), $node->getSourceColumn());
            }
            // TRUE AND X → X (identity)
            if ($leftIsTrue) {
                $this->transformations[] = 'Boolean simplification: TRUE AND X → X';
                return $right;
            }
            // X AND TRUE → X (identity, reverse)
            if ($rightIsTrue) {
                $this->transformations[] = 'Boolean simplification: X AND TRUE → X';
                return $left;
            }
            // X AND X → X (idempotence)
            if ($this->nodesAreIdentical($left, $right)) {
                $this->transformations[] = 'Boolean simplification: X AND X → X (idempotence)';
                return $left;
            }
        }

        // ---- OR rules ----
        if ($op === 'OR') {
            // TRUE OR X → TRUE (annihilation)
            if ($leftIsTrue) {
                $this->transformations[] = 'Boolean simplification: TRUE OR X → TRUE';
                return new Formula_Compiler_AST_LiteralNode(true, 'boolean',
                    $node->getSourceLine(), $node->getSourceColumn());
            }
            // X OR TRUE → TRUE (annihilation, reverse)
            if ($rightIsTrue) {
                $this->transformations[] = 'Boolean simplification: X OR TRUE → TRUE';
                return new Formula_Compiler_AST_LiteralNode(true, 'boolean',
                    $node->getSourceLine(), $node->getSourceColumn());
            }
            // FALSE OR X → X (identity)
            if ($leftIsFalse) {
                $this->transformations[] = 'Boolean simplification: FALSE OR X → X';
                return $right;
            }
            // X OR FALSE → X (identity, reverse)
            if ($rightIsFalse) {
                $this->transformations[] = 'Boolean simplification: X OR FALSE → X';
                return $left;
            }
            // X OR X → X (idempotence)
            if ($this->nodesAreIdentical($left, $right)) {
                $this->transformations[] = 'Boolean simplification: X OR X → X (idempotence)';
                return $left;
            }
        }

        // ---- XOR rules ----
        if ($op === 'XOR') {
            // FALSE XOR X → X (identity)
            if ($leftIsFalse) {
                $this->transformations[] = 'Boolean simplification: FALSE XOR X → X';
                return $right;
            }
            // X XOR FALSE → X (identity, reverse)
            if ($rightIsFalse) {
                $this->transformations[] = 'Boolean simplification: X XOR FALSE → X';
                return $left;
            }
            // TRUE XOR TRUE → FALSE
            if ($leftIsTrue && $rightIsTrue) {
                $this->transformations[] = 'Boolean simplification: TRUE XOR TRUE → FALSE';
                return new Formula_Compiler_AST_LiteralNode(false, 'boolean',
                    $node->getSourceLine(), $node->getSourceColumn());
            }
        }

        // Reconstruct if children changed
        if ($left === $node->left && $right === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_LogicalNode(
            $op,
            $left,
            $right,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;
        return $newNode;
    }

    /**
     * Recursively optimize function call arguments.
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
     * Recursively optimize conditional branches.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        $cond  = $node->condition->accept($this);
        $true  = $node->trueBranch->accept($this);
        $false = $node->falseBranch->accept($this);

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
     * Compares node type, operator/value, and recursively compares
     * child nodes. Used for idempotence detection (X AND X → X).
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

        $typeA = $a->getNodeType();
        $typeB = $b->getNodeType();

        if ($typeA !== $typeB) {
            return false;
        }

        // Compare by serialized form for simplicity
        $serialA = $a->serialize();
        $serialB = $b->serialize();

        // Remove source location from comparison
        unset($serialA['sourceLine'], $serialA['sourceColumn']);
        unset($serialB['sourceLine'], $serialB['sourceColumn']);

        return $serialA == $serialB;
    }
}
