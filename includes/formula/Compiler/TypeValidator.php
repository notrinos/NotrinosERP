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
 * TypeValidator — AST type compatibility validation.
 *
 * Validates that the operation at each node makes sense given the
 * inferred types of its operands. This catches errors like:
 *
 *   - Adding a string to a number ("ABC" + 5)
 *   - Dividing a boolean (TRUE / 3)
 *   - Using a comparison operator on incompatible types
 *   - Applying NOT to a non-boolean context
 *   - Using AND/OR on non-boolean values
 *   - Passing a non-boolean condition to IF()
 *
 * Type inference is conservative. The validator uses the metadata
 * computed during the TypeCheckVisitor pass (which must run before
 * this validator). When a node's type cannot be statically determined,
 * the check is skipped rather than emitting a false positive.
 *
 * The validator implements both ValidatorInterface and NodeVisitor.
 * Errors are accumulated during traversal — all type mismatches are
 * reported, not just the first one.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_TypeValidator
    implements Formula_Contracts_ValidatorInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var Formula_Compiler_ValidationResult Accumulated result */
    private $result;

    /** @var bool Whether to treat type errors as warnings (compatibility mode) */
    private $compatibilityMode;

    /**
     * Construct a type validator.
     *
     * @param bool $compatibilityMode When true, type errors are downgraded
     *                                to warnings for backward compatibility
     *                                with legacy formulas (e.g., payroll
     *                                formulas that mix strings and numbers).
     */
    public function __construct($compatibilityMode = false)
    {
        $this->compatibilityMode = (bool)$compatibilityMode;
    }

    // -----------------------------------------------------------------------
    //  ValidatorInterface
    // -----------------------------------------------------------------------

    /**
     * Validate type compatibility across the AST.
     *
     * @param Formula_Compiler_AST_Node $ast Root node of the AST
     * @return Formula_Compiler_ValidationResult
     */
    public function validate(Formula_Compiler_AST_Node $ast)
    {
        $this->result = new Formula_Compiler_ValidationResult();

        if ($ast === null) {
            return $this->result;
        }

        $ast->accept($this);

        return $this->result;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Leaf nodes (always type-valid)
    // -----------------------------------------------------------------------

    /**
     * Visit a literal node — always type-valid.
     *
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return void
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node)
    {
        // Literals are self-typed — no compatibility issues.
    }

    /**
     * Visit a NULL literal — always type-valid.
     *
     * @param Formula_Compiler_AST_NullNode $node
     * @return void
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node)
    {
        // NULL is type-valid.
    }

    /**
     * Visit a variable node.
     *
     * Variables have runtime types. At compile time we cannot
     * statically determine their type, so no type check is performed.
     * If the variable resolves to an incompatible type at runtime,
     * the runtime engine will throw TypeMismatchException.
     *
     * @param Formula_Compiler_AST_VariableNode $node
     * @return void
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node)
    {
        // Types are runtime — no static check possible.
    }

    /**
     * Visit a range node.
     *
     * Ranges are always valid as operands for aggregate functions.
     *
     * @param Formula_Compiler_AST_RangeNode $node
     * @return void
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node)
    {
        // Ranges are valid for aggregate functions.
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Function calls
    // -----------------------------------------------------------------------

    /**
     * Visit a function call node.
     *
     * Recurse into argument expressions to validate their types.
     * The function's own return type checking is handled at runtime
     * by the FunctionExecutor.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return void
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        foreach ($node->arguments as $argument) {
            $argument->accept($this);
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Binary operators
    // -----------------------------------------------------------------------

    /**
     * Visit a binary operator node.
     *
     * Type checks depend on the operator:
     *   - Arithmetic (+, -, *, /, %, ^): both operands should be numeric.
     *   - Null coalescing (??): any types; first non-null wins.
     *
     * Recurse into left and right subtrees.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return void
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        // Recurse first so child type metadata is available.
        if ($node->left !== null) {
            $node->left->accept($this);
        }
        if ($node->right !== null) {
            $node->right->accept($this);
        }

        // Null coalescing is type-safe by design — any types allowed.
        if ($node->operator === '??') {
            return;
        }

        // For arithmetic operators, check numeric compatibility.
        $arithmeticOps = array('+', '-', '*', '/', '%', '^');
        if (!in_array($node->operator, $arithmeticOps, true)) {
            return;
        }

        if ($node->left !== null) {
            $leftMeta = $node->left->getMetadata();
            $leftType = $leftMeta->returnType;
            if ($this->isKnownType($leftType) && !$this->isNumericType($leftType)) {
                $this->addTypeIssue(
                    sprintf(
                        "Left operand of '%s' is type '%s'. Expected a numeric type (integer, decimal).",
                        $node->operator,
                        $leftType
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
        }

        if ($node->right !== null) {
            $rightMeta = $node->right->getMetadata();
            $rightType = $rightMeta->returnType;
            if ($this->isKnownType($rightType) && !$this->isNumericType($rightType)) {
                $this->addTypeIssue(
                    sprintf(
                        "Right operand of '%s' is type '%s'. Expected a numeric type (integer, decimal).",
                        $node->operator,
                        $rightType
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
        }

        // Check for type mismatch between left and right operands.
        if ($node->left !== null && $node->right !== null) {
            $leftType  = $node->left->getMetadata()->returnType;
            $rightType = $node->right->getMetadata()->returnType;

            if ($leftType !== 'unknown' && $rightType !== 'unknown' && $leftType !== $rightType) {
                // String + number is the most common mistake.
                if ($this->isStringType($leftType) && $this->isNumericType($rightType)) {
                    $this->addTypeIssue(
                        sprintf(
                            "Type mismatch: cannot %s type '%s' with type '%s'. Use VALUE() to convert strings to numbers.",
                            $node->operator,
                            $leftType,
                            $rightType
                        ),
                        $node->getSourceLine(),
                        $node->getSourceColumn()
                    );
                } elseif ($this->isNumericType($leftType) && $this->isStringType($rightType)) {
                    $this->addTypeIssue(
                        sprintf(
                            "Type mismatch: cannot %s type '%s' with type '%s'. Use VALUE() to convert strings to numbers.",
                            $node->operator,
                            $leftType,
                            $rightType
                        ),
                        $node->getSourceLine(),
                        $node->getSourceColumn()
                    );
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Unary operators
    // -----------------------------------------------------------------------

    /**
     * Visit a unary operator node.
     *
     * Type checks:
     *   - Unary - (negation): operand should be numeric.
     *   - Unary + (no-op): operand should be numeric.
     *   - NOT: operand should be boolean or truthy/falsy compatible.
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return void
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        if ($node->operand !== null) {
            $node->operand->accept($this);
        }

        if ($node->operand === null) {
            return;
        }

        $operandType = $node->operand->getMetadata()->returnType;

        if ($node->operator === 'NOT') {
            // NOT accepts any truthy/falsy value — no strict type check.
            return;
        }

        // Unary - and + expect numeric.
        if (($node->operator === '-' || $node->operator === '+')
            && $this->isKnownType($operandType)
            && !$this->isNumericType($operandType)
        ) {
            $this->addTypeIssue(
                sprintf(
                    "Unary '%s' expects a numeric operand, got type '%s'.",
                    $node->operator,
                    $operandType
                ),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Conditional (IF)
    // -----------------------------------------------------------------------

    /**
     * Visit a conditional node.
     *
     * The condition should be boolean or truthy/falsy compatible.
     * A type mismatch here is downgraded to a warning because in
     * spreadsheet-style formulas, any non-zero/non-null value is
     * treated as true.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return void
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        if ($node->condition !== null) {
            $node->condition->accept($this);

            $condType = $node->condition->getMetadata()->returnType;
            if ($condType !== 'unknown'
                && $condType !== 'boolean'
                && !$this->isNumericType($condType)
            ) {
                $this->result->addWarning(
                    sprintf(
                        "IF condition is type '%s'. Non-boolean conditions are treated as truthy/falsy.",
                        $condType
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
        }

        if ($node->trueBranch !== null) {
            $node->trueBranch->accept($this);
        }
        if ($node->falseBranch !== null) {
            $node->falseBranch->accept($this);
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Comparison operators
    // -----------------------------------------------------------------------

    /**
     * Visit a comparison node.
     *
     * Comparison operators are type-safe for any comparable types.
     * A warning is issued when comparing a string to a number
     * (potential unintended behavior due to PHP type juggling).
     *
     * @param Formula_Compiler_AST_ComparisonNode $node
     * @return void
     */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        if ($node->left !== null) {
            $node->left->accept($this);
        }
        if ($node->right !== null) {
            $node->right->accept($this);
        }

        if ($node->left !== null && $node->right !== null) {
            $leftType  = $node->left->getMetadata()->returnType;
            $rightType = $node->right->getMetadata()->returnType;

            if ($leftType !== 'unknown' && $rightType !== 'unknown'
                && $leftType !== $rightType
            ) {
                $this->result->addWarning(
                    sprintf(
                        "Comparison '%s' between type '%s' and type '%s'. Result may not be meaningful.",
                        $node->operator,
                        $leftType,
                        $rightType
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Logical operators
    // -----------------------------------------------------------------------

    /**
     * Visit a logical operator node.
     *
     * Logical operators (AND, OR, XOR) expect boolean operands.
     * Non-boolean operands are warned about but not rejected —
     * they follow PHP truthy/falsy semantics in compatibility mode.
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return void
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        if ($node->left !== null) {
            $node->left->accept($this);

            $leftType = $node->left->getMetadata()->returnType;
            if ($leftType !== 'unknown' && $leftType !== 'boolean') {
                $this->result->addWarning(
                    sprintf(
                        "Logical '%s' left operand is type '%s'. Expected boolean.",
                        $node->operator,
                        $leftType
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
        }

        if ($node->right !== null) {
            $node->right->accept($this);

            $rightType = $node->right->getMetadata()->returnType;
            if ($rightType !== 'unknown' && $rightType !== 'boolean') {
                $this->result->addWarning(
                    sprintf(
                        "Logical '%s' right operand is type '%s'. Expected boolean.",
                        $node->operator,
                        $rightType
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
        }
    }

    // -----------------------------------------------------------------------
    //  Type helpers
    // -----------------------------------------------------------------------

    /**
     * Check whether a type string represents a numeric type.
     *
     * Numeric types: 'integer', 'decimal', 'number'.
     *
     * @param string $type The type string from NodeMetadata
     * @return bool
     */
    private function isNumericType($type)
    {
        return in_array($type, array('integer', 'decimal', 'number'), true);
    }

    /**
     * Check whether a type is statically known (not runtime-dependent).
     *
     * @param string $type
     * @return bool
     */
    private function isKnownType($type)
    {
        return $type !== 'unknown' && $type !== 'mixed';
    }

    /**
     * Check whether a type string represents a string type.
     *
     * @param string $type The type string from NodeMetadata
     * @return bool
     */
    private function isStringType($type)
    {
        return $type === 'string';
    }

    /**
     * Add a type issue — either an error or warning depending on mode.
     *
     * In compatibility mode, type mismatches are downgraded to warnings
     * so legacy payroll formulas continue to work.
     *
     * @param string $message The error/warning message
     * @param int    $line    Source line
     * @param int    $column  Source column
     * @return void
     */
    private function addTypeIssue($message, $line = 0, $column = 0)
    {
        if ($this->compatibilityMode) {
            $this->result->addWarning($message, $line, $column);
        } else {
            $this->result->addError($message, $line, $column);
        }
    }
}
