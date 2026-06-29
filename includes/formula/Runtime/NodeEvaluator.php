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
 * NodeEvaluator — Post-order tree-walk AST evaluator.
 *
 * Implements the Formula_Compiler_AST_NodeVisitor interface to evaluate
 * every AST node type. The evaluator is stateless — all state lives in
 * the RuntimeSession passed to each visit method.
 *
 * ## Evaluation Semantics
 *
 * 1. **Post-order traversal**: Children are evaluated before the parent.
 *    For binary operators, left is evaluated before right.
 * 2. **Short-circuit evaluation**: AND (false left → skip right),
 *    OR (true left → skip right), and IF (only the winning branch is evaluated).
 * 3. **Lazy variable resolution**: Variables are resolved only when
 *    referenced, through the NamespaceRegistry.
 * 4. **Deterministic**: Same formula + same context = same result.
 * 5. **Resource-bounded**: Node count, depth, and time are tracked
 *    via RuntimeSession and enforced to prevent DoS.
 *
 * ## Usage
 *
 * This class is instantiated by FormulaRuntime for each evaluation.
 * Modules never use this class directly — they go through FormulaFacade.
 *
 * @package Formula\Runtime
 * @since   2.0.0
 */
class Formula_Runtime_NodeEvaluator implements Formula_Compiler_AST_NodeVisitor
{
    // -----------------------------------------------------------------------
    //  Arithmetic operators (used by visitBinary for +, -, *, /, %, ^)
    // -----------------------------------------------------------------------

    /** @var string[] Binary arithmetic operators */
    private static $arithmeticOps = array('+', '-', '*', '/', '%', '^');

    /** @var string[] Binary comparison operators */
    private static $comparisonOps = array('>', '<', '>=', '<=', '==', '!=', '<>');

    // -----------------------------------------------------------------------
    //  Agent: resolve variables (delegated to FormulaRuntime context)
    // -----------------------------------------------------------------------

    /**
     * Resolver callable: receives (string $qualifiedName, Formula_Context_FormulaContext $ctx)
     * and returns mixed.
     *
     * This is wired during evaluation to the NamespaceRegistry or a fallback
     * that reads from the FormulaContext's flat variable store.
     *
     * @var callable|null
     */
    private $variableResolver = null;

    /**
     * Function executor callable: receives (string $functionName, array $resolvedArgs,
     * Formula_Context_FormulaContext $ctx) and returns mixed.
     *
     * @var callable|null
     */
    private $functionExecutor = null;

    /**
     * Active session (set per evaluation call).
     *
     * @var Formula_Runtime_RuntimeSession|null
     */
    private $session = null;

    // -----------------------------------------------------------------------
    //  Public API
    // -----------------------------------------------------------------------

    /**
     * Construct the evaluator.
     *
     * @param callable $variableResolver Resolves qualified variable names to values
     * @param callable $functionExecutor Invokes registered functions with resolved arguments
     */
    public function __construct($variableResolver, $functionExecutor)
    {
        $this->variableResolver = $variableResolver;
        $this->functionExecutor = $functionExecutor;
    }

    /**
     * Evaluate a complete AST from the root node.
     *
     * @param Formula_Compiler_AST_Node        $node    The AST root node
     * @param Formula_Runtime_RuntimeSession   $session The evaluation session (tracks depth, count, limits)
     * @return mixed The computed result
     * @throws Formula_Exceptions_RuntimeExecutionException On unrecoverable evaluation error
     * @throws Formula_Exceptions_ResourceExhaustedException On resource limit exceeded
     */
    public function evaluate(
        Formula_Compiler_AST_Node $node,
        Formula_Runtime_RuntimeSession $session
    ) {
        $this->session = $session;
        $result = $node->accept($this);
        $this->session = null;
        return $result;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor implementation — visit each node type
    // -----------------------------------------------------------------------

    /**
     * Visit a literal node (number, string, boolean).
     *
     * Literals are terminal — no children, no evaluation needed.
     * The value is returned directly.
     *
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return mixed
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node)
    {
        $this->incrementNodeCount();
        return $node->value;
    }

    /**
     * Visit a variable reference node.
     *
     * Resolves the variable through the variable resolver callable,
     * which delegates to the NamespaceRegistry.
     *
     * @param Formula_Compiler_AST_VariableNode $node
     * @return mixed
     * @throws Formula_Exceptions_UnknownVariableException If variable cannot be resolved
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node)
    {
        $this->incrementNodeCount();

        // Build qualified name: "Namespace.Identifier" or "Identifier"
        $qualifiedName = $node->getQualifiedName();

        // Check memoization cache in the session
        $cached = $this->session->getResolvedVariable($qualifiedName);
        if ($cached !== null) {
            return $cached;
        }

        $value = call_user_func(
            $this->variableResolver,
            $qualifiedName,
            $this->session->getContext()
        );

        // Cache for subsequent references within the same evaluation
        $this->session->setResolvedVariable($qualifiedName, $value);

        // Track variable resolution for diagnostics
        $this->session->incrementVariableResolutions();

        return $value;
    }

    /**
     * Visit a function call node.
     *
     * Evaluates all argument expressions (left to right, eagerly),
     * then invokes the function through the function executor.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return mixed
     * @throws Formula_Exceptions_UnknownFunctionException If function not registered
     * @throws Formula_Exceptions_PermissionDeniedException If user lacks required permission
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        $this->incrementNodeCount();

        // Evaluate arguments eagerly (left to right)
        $args = array();
        foreach ($node->arguments as $argNode) {
            $args[] = $argNode->accept($this);
        }

        $result = call_user_func(
            $this->functionExecutor,
            $node->functionName,
            $args,
            $this->session->getContext()
        );

        $this->session->incrementFunctionCalls();

        return $result;
    }

    /**
     * Visit a binary operator node (+, -, *, /, %, ^, ??).
     *
     * For arithmetic operators: evaluate left then right, apply operator.
     * For null coalescing (??): short-circuit — if left is not null,
     * skip evaluating the right operand.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return mixed
     * @throws Formula_Exceptions_DivideByZeroException On division or modulo by zero
     * @throws Formula_Exceptions_TypeMismatchException On incompatible operand types
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        $this->incrementNodeCount();

        $left = $node->left->accept($this);

        // Null coalescing short-circuit: if left is not null, return it
        if ($node->operator === '??') {
            if ($left !== null) {
                return $left;
            }
            return $node->right->accept($this);
        }

        $right = $node->right->accept($this);

        return $this->applyArithmetic($node->operator, $left, $right, $node->getSourceLine(), $node->getSourceColumn());
    }

    /**
     * Visit a unary operator node (-, +, NOT).
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return mixed
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        $this->incrementNodeCount();

        $operand = $node->operand->accept($this);

        switch ($node->operator) {
            case '-':
                return -(float)$operand;

            case '+':
                return +(float)$operand;

            case 'NOT':
                return !((bool)$operand);

            default:
                throw new Formula_Exceptions_RuntimeExecutionException(
                    'Unknown unary operator: ' . $node->operator,
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
        }
    }

    /**
     * Visit a conditional node: IF(condition, trueBranch, falseBranch).
     *
     * Short-circuit: ONLY the selected branch is evaluated.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return mixed
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        $this->incrementNodeCount();

        $conditionValue = $node->condition->accept($this);

        // Only evaluate the winning branch
        if ((bool)$conditionValue) {
            return $node->trueBranch->accept($this);
        } else {
            return $node->falseBranch->accept($this);
        }
    }

    /**
     * Visit a comparison node (>, <, >=, <=, ==, !=, <>).
     *
     * Evaluates both operands then applies the comparison.
     *
     * @param Formula_Compiler_AST_ComparisonNode $node
     * @return bool
     */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        $this->incrementNodeCount();

        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        return $this->applyComparison($node->operator, $left, $right);
    }

    /**
     * Visit a logical node (AND, OR, XOR).
     *
     * Short-circuit semantics:
     * - AND: false left → skip right, return false
     * - OR:  true left  → skip right, return true
     * - XOR: no short-circuit (both needed)
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return bool
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $this->incrementNodeCount();

        $left = $node->left->accept($this);

        // Short-circuit evaluation
        if ($node->operator === 'AND' && !(bool)$left) {
            return false;
        }

        if ($node->operator === 'OR' && (bool)$left) {
            return true;
        }

        // XOR always evaluates both operands
        $right = $node->right->accept($this);

        switch ($node->operator) {
            case 'AND':
                return ((bool)$left) && ((bool)$right);

            case 'OR':
                return ((bool)$left) || ((bool)$right);

            case 'XOR':
                return ((bool)$left) xor ((bool)$right);

            default:
                throw new Formula_Exceptions_RuntimeExecutionException(
                    'Unknown logical operator: ' . $node->operator,
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );
        }
    }

    /**
     * Visit a range node (e.g., A1:B10).
     *
     * Range resolution is deferred to the spreadsheet data provider.
     * In v1, this throws an exception if no provider is configured
     * (ranges are primarily for Report Builder integration).
     *
     * @param Formula_Compiler_AST_RangeNode $node
     * @return mixed
     * @throws Formula_Exceptions_RuntimeExecutionException If no range provider is available
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node)
    {
        $this->incrementNodeCount();

        throw new Formula_Exceptions_RuntimeExecutionException(
            sprintf(
                'Range references (%s:%s) are not supported without a spreadsheet data provider.',
                $node->startCell,
                $node->endCell
            ),
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
    }

    /**
     * Visit a NULL literal node.
     *
     * In compatibility mode (payroll migration), null evaluates to 0.0.
     * Otherwise, returns PHP null.
     *
     * @param Formula_Compiler_AST_NullNode $node
     * @return mixed
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node)
    {
        $this->incrementNodeCount();

        $context = $this->session->getContext();
        if ($context !== null && $context->isCompatibilityMode()) {
            return 0.0;
        }
        return null;
    }

    // -----------------------------------------------------------------------
    //  Arithmetic operator application
    // -----------------------------------------------------------------------

    /**
     * Apply an arithmetic/comparison operator to two values.
     *
     * @param string $operator The operator character (+, -, *, /, %, ^)
     * @param mixed  $left     Left operand value
     * @param mixed  $right    Right operand value
     * @param int    $line     Source line for error reporting
     * @param int    $column   Source column for error reporting
     * @return float The computed result
     * @throws Formula_Exceptions_DivideByZeroException
     * @throws Formula_Exceptions_TypeMismatchException
     */
    private function applyArithmetic($operator, $left, $right, $line = 0, $column = 0)
    {
        $leftNum  = (float)$left;
        $rightNum = (float)$right;

        switch ($operator) {
            case '+':
                return $leftNum + $rightNum;

            case '-':
                return $leftNum - $rightNum;

            case '*':
                return $leftNum * $rightNum;

            case '/':
                if ($rightNum == 0.0) {
                    throw new Formula_Exceptions_DivideByZeroException(
                        'Division by zero',
                        $line,
                        $column
                    );
                }
                return $leftNum / $rightNum;

            case '%':
                if ($rightNum == 0.0) {
                    throw new Formula_Exceptions_DivideByZeroException(
                        'Modulo by zero',
                        $line,
                        $column
                    );
                }
                return fmod($leftNum, $rightNum);

            case '^':
                return pow($leftNum, $rightNum);

            default:
                throw new Formula_Exceptions_RuntimeExecutionException(
                    'Unknown arithmetic operator: ' . $operator,
                    $line,
                    $column
                );
        }
    }

    /**
     * Apply a comparison operator to two values.
     *
     * Comparisons follow PHP type coercion rules (loose comparison),
     * matching the behavior of the legacy payroll_formula_engine.
     *
     * @param string $operator Comparison operator: >, <, >=, <=, ==, !=, <>
     * @param mixed  $left     Left operand value
     * @param mixed  $right    Right operand value
     * @return bool
     */
    private function applyComparison($operator, $left, $right)
    {
        switch ($operator) {
            case '>':
                return $left > $right;

            case '<':
                return $left < $right;

            case '>=':
                return $left >= $right;

            case '<=':
                return $left <= $right;

            case '==':
                return $left == $right;

            case '!=':
            case '<>':
                return $left != $right;

            default:
                return false;
        }
    }

    // -----------------------------------------------------------------------
    //  Session resource tracking
    // -----------------------------------------------------------------------

    /**
     * Increment the node evaluation counter and check limits.
     *
     * @return void
     * @throws Formula_Exceptions_ResourceExhaustedException If max node evaluations exceeded
     */
    private function incrementNodeCount()
    {
        if ($this->session === null) {
            return;
        }

        $count = $this->session->incrementNodeEvaluations();

        if ($count > FORMULA_MAX_NODE_EVALUATIONS) {
            throw new Formula_Exceptions_ResourceExhaustedException(
                sprintf(
                    'Maximum node evaluations (%d) exceeded. This formula may contain '
                    . 'a degenerate expression or circular logic.',
                    FORMULA_MAX_NODE_EVALUATIONS
                ),
                'MAX_NODE_EVALUATIONS',
                FORMULA_MAX_NODE_EVALUATIONS
            );
        }

        // Check time limit periodically (every 100 evaluations for performance)
        if ($count % 100 === 0) {
            $this->session->checkTimeLimit();
        }
    }
}
