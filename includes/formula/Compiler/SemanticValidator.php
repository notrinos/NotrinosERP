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
 * SemanticValidator — AST semantic correctness validation.
 *
 * Validates that every variable and function reference in the AST has a
 * registered provider or implementation. This validator answers the
 * question: "Can this formula actually be executed given the current
 * registries?"
 *
 * Checks performed:
 *   - Every VariableNode must have a registered provider for its namespace.
 *     Simple variables (no namespace) are checked against a list of
 *     fallback/context variables when provided.
 *   - Every FunctionNode must have a registered implementation in the
 *     FunctionRegistry.
 *   - Variables in namespaces without providers are flagged.
 *   - Functions not in the registry are flagged with the unknown function
 *     name and location.
 *
 * The SemanticValidator requires both registries. It does NOT check
 * argument counts (that's the SyntaxValidator's job) or type
 * compatibility (that's the TypeValidator's job).
 *
 * Implements both ValidatorInterface and NodeVisitor for AST traversal.
 * Errors are accumulated — traversal does not stop on the first error.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_SemanticValidator
    implements Formula_Contracts_ValidatorInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var Formula_Registry_FunctionRegistry */
    private $functionRegistry;

    /** @var Formula_Registry_VariableRegistry */
    private $variableRegistry;

    /** @var Formula_Compiler_ValidationResult */
    private $result;

    /** @var string[] Simple variable names known to the context (optional) */
    private $knownContextVariables;

    /**
     * Construct a semantic validator.
     *
     * @param Formula_Registry_FunctionRegistry $functionRegistry
     * @param Formula_Registry_VariableRegistry $variableRegistry
     * @param string[]                          $knownContextVariables Optional list of simple variable names valid without namespace
     */
    public function __construct(
        Formula_Registry_FunctionRegistry $functionRegistry,
        Formula_Registry_VariableRegistry $variableRegistry,
        array $knownContextVariables = array()
    ) {
        $this->functionRegistry      = $functionRegistry;
        $this->variableRegistry      = $variableRegistry;
        $this->knownContextVariables = $knownContextVariables;
    }

    // -----------------------------------------------------------------------
    //  ValidatorInterface
    // -----------------------------------------------------------------------

    /**
     * Validate the AST semantics and return the result.
     *
     * Traverses all variable and function nodes, checking registry
     * presence for each.
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
    //  NodeVisitor — Pass-through for non-variable/non-function nodes
    // -----------------------------------------------------------------------

    /**
     * Visit a literal node — always semantically valid.
     *
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return void
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node)
    {
        // Literals are always valid — no further checks.
    }

    /**
     * Visit a NULL literal — always semantically valid.
     *
     * @param Formula_Compiler_AST_NullNode $node
     * @return void
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node)
    {
        // NULL is always semantically valid.
    }

    /**
     * Visit a range node — always semantically valid.
     *
     * Cell ranges are resolved at runtime against the spreadsheet
     * data grid. They cannot be checked at compile time.
     *
     * @param Formula_Compiler_AST_RangeNode $node
     * @return void
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node)
    {
        // Ranges resolved at runtime.
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Variable references
    // -----------------------------------------------------------------------

    /**
     * Visit a variable node.
     *
     * Checks:
     *   1. Namespace-qualified variables (e.g., Employee.BasicSalary):
     *      the namespace must be registered in the VariableRegistry.
     *   2. Simple variables (no namespace, e.g., Salary):
     *      must be in the known context variables list, or treated as
     *      unresolvable. A warning is emitted for unknown simple variables.
     *
     * @param Formula_Compiler_AST_VariableNode $node
     * @return void
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node)
    {
        // Namespace-qualified: check that the namespace is registered.
        if ($node->namespace !== '') {
            if (!$this->variableRegistry->hasNamespace($node->namespace)) {
                $this->result->addError(
                    sprintf(
                        "Variable namespace '%s' is not registered. No variable provider claims namespace '%s'.",
                        $node->namespace,
                        $node->namespace
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
            return;
        }

        // Simple variable: check against the known context variables list.
        if (!empty($this->knownContextVariables)) {
            // Case-insensitive matching for payroll backward compatibility.
            $found = false;
            $upper = strtoupper($node->identifier);
            foreach ($this->knownContextVariables as $known) {
                if (strtoupper($known) === $upper) {
                    $found = true;
                    break;
                }
            }
            if (!$found) {
                $this->result->addWarning(
                    sprintf(
                        "Variable '%s' is not in the known context variables list. It may resolve to 0.0 at runtime.",
                        $node->identifier
                    ),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
            }
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Function calls
    // -----------------------------------------------------------------------

    /**
     * Visit a function call node.
     *
     * Checks:
     *   1. The function name must be registered in the FunctionRegistry.
     *   2. Recurse into argument expressions to check their semantics.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return void
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        if (!$this->functionRegistry->has($node->functionName)) {
            $this->result->addError(
                sprintf(
                    "Unknown function '%s'. The function is not registered in the FunctionRegistry.",
                    $node->functionName
                ),
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
        }

        // Recurse into arguments to check variables/functions within.
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
     * Recurses into left and right subtrees to validate their semantics.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return void
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
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
     * Recurses into the operand subtree.
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return void
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        if ($node->operand !== null) {
            $node->operand->accept($this);
        }
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Conditional (IF)
    // -----------------------------------------------------------------------

    /**
     * Visit a conditional node.
     *
     * Recurses into all three branches.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return void
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
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
     * Recurses into left and right subtrees.
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
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Logical operators
    // -----------------------------------------------------------------------

    /**
     * Visit a logical operator node.
     *
     * Recurses into left and right subtrees.
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return void
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        if ($node->left !== null) {
            $node->left->accept($this);
        }
        if ($node->right !== null) {
            $node->right->accept($this);
        }
    }
}
