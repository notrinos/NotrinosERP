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
 * SyntaxValidator — Post-parse AST structural integrity validation.
 *
 * The SyntaxValidator performs structural checks on the AST that go beyond
 * what the parser can enforce. While the recursive descent parser catches
 * most syntax errors during token consumption (missing parens, double commas,
 * stray operators), this validator enforces structural constraints that
 * emerge from the full tree:
 *
 *   - Function argument counts must fall within the function's declared
 *     min/max argument range.
 *   - Binary operators must have both left and right operands present.
 *   - Conditional nodes (IF) must have condition, true-branch, and
 *     false-branch children.
 *   - Unary operators must have an operand.
 *   - The AST depth must not exceed the configured limit.
 *
 * The validator implements both ValidatorInterface (entry point) and
 * NodeVisitor (traversal). Errors are collected during traversal rather
 * than stopping at the first issue, providing comprehensive feedback.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_SyntaxValidator
    implements Formula_Contracts_ValidatorInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var Formula_Registry_FunctionRegistry|null Function registry for argument validation */
    private $functionRegistry;

    /** @var Formula_Compiler_ValidationResult Accumulated validation result */
    private $result;

    /** @var int Current AST depth during traversal */
    private $depth;

    /** @var int Maximum allowed AST depth (configurable) */
    private $maxDepth;

    /**
     * Construct a syntax validator.
     *
     * The function registry is optional. When provided, the validator
     * checks argument counts against registered function metadata.
     * Without it, argument count checks are skipped (the semantic
     * validator will catch those).
     *
     * @param Formula_Registry_FunctionRegistry|null $functionRegistry
     * @param int                                   $maxDepth Maximum allowed AST depth (default 100)
     */
    public function __construct($functionRegistry = null, $maxDepth = 100)
    {
        $this->functionRegistry = $functionRegistry;
        $this->maxDepth         = (int)$maxDepth;
        $this->depth            = 0;
    }

    // -----------------------------------------------------------------------
    //  ValidatorInterface
    // -----------------------------------------------------------------------

    /**
     * Validate the AST structure and return the result.
     *
     * Traverses the entire AST via the visitor pattern, collecting
     * all structural errors into a ValidationResult. The traversal
     * does NOT stop on the first error.
     *
     * @param Formula_Compiler_AST_Node $ast Root node of the AST
     * @return Formula_Compiler_ValidationResult
     */
    public function validate(Formula_Compiler_AST_Node $ast)
    {
        $this->result = new Formula_Compiler_ValidationResult();
        $this->depth  = 0;

        // A null or empty AST is valid (represents empty formula which
        // evaluates to 0.0 per legacy payroll engine compatibility).
        if ($ast === null) {
            return $this->result;
        }

        $ast->accept($this);

        return $this->result;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Literals are always valid
    // -----------------------------------------------------------------------

    /**
     * Visit a literal node.
     *
     * Literals are inherently valid leaf nodes. No structural checks needed.
     *
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return void
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node)
    {
        $this->checkDepth();
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Variable references
    // -----------------------------------------------------------------------

    /**
     * Visit a variable node.
     *
     * Structural check: the variable identifier must be non-empty.
     * Semantic checks (provider registration) are handled by SemanticValidator.
     *
     * @param Formula_Compiler_AST_VariableNode $node
     * @return void
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node)
    {
        $this->checkDepth();

        if ($node->identifier === '') {
            $this->result->addError(
                'Variable reference has an empty identifier.',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Function calls
    // -----------------------------------------------------------------------

    /**
     * Visit a function call node.
     *
     * Structural checks:
     *   1. Function name must be non-empty.
     *   2. Argument count must fall within the declared min/max range
     *      (when function registry is available).
     *   3. Recurse into argument expressions.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return void
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        $this->checkDepth();

        if ($node->functionName === '') {
            $this->result->addError(
                'Function call has an empty function name.',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($this->functionRegistry !== null) {
            $function = $this->functionRegistry->get($node->functionName);
            if ($function !== null) {
                $metadata = $function->getMetadata();
                $argCount  = count($node->arguments);
                $minArgs   = $metadata->minArgs;
                $maxArgs   = $metadata->maxArgs;

                if ($minArgs >= 0 && $argCount < $minArgs) {
                    $this->result->addError(
                        sprintf(
                            "Function '%s' expects at least %d argument(s), got %d.",
                            $node->functionName,
                            $minArgs,
                            $argCount
                        ),
                        $node->getSourceLine(),
                        $node->getSourceColumn()
                    );
                }

                if ($maxArgs >= 0 && $argCount > $maxArgs) {
                    $this->result->addError(
                        sprintf(
                            "Function '%s' expects at most %d argument(s), got %d.",
                            $node->functionName,
                            $maxArgs,
                            $argCount
                        ),
                        $node->getSourceLine(),
                        $node->getSourceColumn()
                    );
                }
            }
        }

        // Recurse into argument expressions
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
     * Structural checks:
     *   1. Left and right operands must be present (non-null).
     *   2. The operator must be a recognized binary operator.
     *   3. Recurse into left and right subtrees.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return void
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        $this->checkDepth();

        if ($node->left === null) {
            $this->result->addError(
                sprintf("Binary operator '%s' is missing its left operand.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->right === null) {
            $this->result->addError(
                sprintf("Binary operator '%s' is missing its right operand.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        // Validate operator is known
        $validOps = array('+', '-', '*', '/', '%', '^', '??');
        if (!in_array($node->operator, $validOps, true)) {
            $this->result->addError(
                sprintf("Unknown binary operator: '%s'.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->left !== null) {
            $node->left->accept($this);
        }
        if ($node->right !== null) {
            $node->right->accept($this);
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Unary operators
    // -----------------------------------------------------------------------

    /**
     * Visit a unary operator node.
     *
     * Structural checks:
     *   1. The operand must be present (non-null).
     *   2. The operator must be a recognized unary operator.
     *   3. Recurse into the operand subtree.
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return void
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        $this->checkDepth();

        if ($node->operand === null) {
            $this->result->addError(
                sprintf("Unary operator '%s' is missing its operand.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        $validOps = array('-', '+', 'NOT');
        if (!in_array($node->operator, $validOps, true)) {
            $this->result->addError(
                sprintf("Unknown unary operator: '%s'.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->operand !== null) {
            $node->operand->accept($this);
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Conditional (IF)
    // -----------------------------------------------------------------------

    /**
     * Visit a conditional node (IF expression).
     *
     * Structural checks:
     *   1. Condition expression must be present.
     *   2. True-branch expression must be present.
     *   3. False-branch expression must be present.
     *   4. Recurse into all three branches.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return void
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        $this->checkDepth();

        if ($node->condition === null) {
            $this->result->addError(
                'IF expression is missing its condition.',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->trueBranch === null) {
            $this->result->addError(
                'IF expression is missing its true-branch.',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->falseBranch === null) {
            $this->result->addError(
                'IF expression is missing its false-branch.',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->condition !== null) {
            $node->condition->accept($this);
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
     * Structural checks:
     *   1. Left and right operands must be present.
     *   2. The comparison operator must be recognized.
     *   3. Recurse into left and right subtrees.
     *
     * @param Formula_Compiler_AST_ComparisonNode $node
     * @return void
     */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        $this->checkDepth();

        if ($node->left === null) {
            $this->result->addError(
                sprintf("Comparison operator '%s' is missing its left operand.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->right === null) {
            $this->result->addError(
                sprintf("Comparison operator '%s' is missing its right operand.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        $validOps = array('>', '<', '>=', '<=', '==', '!=', '<>');
        if (!in_array($node->operator, $validOps, true)) {
            $this->result->addError(
                sprintf("Unknown comparison operator: '%s'.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->left !== null) {
            $node->left->accept($this);
        }
        if ($node->right !== null) {
            $node->right->accept($this);
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Logical operators
    // -----------------------------------------------------------------------

    /**
     * Visit a logical operator node (AND, OR, XOR).
     *
     * Structural checks:
     *   1. Left and right operands must be present.
     *   2. The logical operator must be recognized.
     *   3. Recurse into left and right subtrees.
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return void
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $this->checkDepth();

        if ($node->left === null) {
            $this->result->addError(
                sprintf("Logical operator '%s' is missing its left operand.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->right === null) {
            $this->result->addError(
                sprintf("Logical operator '%s' is missing its right operand.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        $validOps = array('AND', 'OR', 'XOR');
        if (!in_array($node->operator, $validOps, true)) {
            $this->result->addError(
                sprintf("Unknown logical operator: '%s'.", $node->operator),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->left !== null) {
            $node->left->accept($this);
        }
        if ($node->right !== null) {
            $node->right->accept($this);
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Range references
    // -----------------------------------------------------------------------

    /**
     * Visit a cell range node.
     *
     * Structural checks:
     *   1. Both start and end cell references must be non-empty.
     *
     * @param Formula_Compiler_AST_RangeNode $node
     * @return void
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node)
    {
        $this->checkDepth();

        if ($node->startCell === '') {
            $this->result->addError(
                'Cell range is missing its start reference.',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        if ($node->endCell === '') {
            $this->result->addError(
                'Cell range is missing its end reference.',
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — NULL literal
    // -----------------------------------------------------------------------

    /**
     * Visit a null literal node.
     *
     * NULL is structurally valid. No checks needed.
     *
     * @param Formula_Compiler_AST_NullNode $node
     * @return void
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node)
    {
        $this->checkDepth();
    }

    // -----------------------------------------------------------------------
    //  Helpers
    // -----------------------------------------------------------------------

    /**
     * Check the current AST depth against the configured maximum.
     *
     * Records an error if the depth limit is exceeded. This prevents
     * stack overflow from deeply nested formulas like ((((...)))).
     *
     * @return void
     */
    private function checkDepth()
    {
        $this->depth++;
        if ($this->depth > $this->maxDepth) {
            $this->result->addError(
                sprintf(
                    'AST depth exceeds maximum allowed depth of %d. The formula may be too deeply nested.',
                    $this->maxDepth
                )
            );
        }
    }

    /**
     * Get the current traversal depth.
     *
     * @return int
     */
    public function getDepth()
    {
        return $this->depth;
    }
}
