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
 * ConstantFolder — AST optimizer that evaluates constant sub-expressions
 * at compile time.
 *
 * ## Purpose
 *
 * Constant folding transforms sub-expressions consisting entirely of
 * literal values into a single LiteralNode. This reduces runtime work
 * and enables further optimizations (dead branch elimination after
 * constant conditions are folded).
 *
 * ## Transformations
 *
 *   - `2 + 3`       → `LiteralNode(5)`
 *   - `2 + 3 * 4`   → `LiteralNode(14)`  (respects precedence)
 *   - `ABS(-5)`     → `LiteralNode(5)`   (deterministic functions)
 *   - `ROUND(3.14159, 2)` → `LiteralNode(3.14)`
 *   - `Employee.BasicSalary` → left as-is (not constant)
 *   - `5 - 3`       → `LiteralNode(2)`
 *   - `-10`         → `LiteralNode(-10)` (unary negation)
 *
 * ## Safety
 *
 * Constant folding is applied ONLY to:
 *   - Arithmetic operators (+, -, *, /, %, ^) on constant operands
 *   - Comparison operators (>, <, >=, <=, ==, !=, <>) on constant operands
 *   - Logical operators (AND, OR, NOT, XOR) on constant operands
 *   - Functions marked isDeterministic=true with all-constant arguments
 *
 * Variables, non-deterministic functions, and conditional branches are
 * NEVER folded — they require runtime context.
 *
 * If constant evaluation produces NaN or Infinity (e.g., 1/0), the
 * optimizer leaves the subtree un-folded. The runtime will handle
 * the error with proper diagnostics.
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
class Formula_Compiler_ConstantFolder
    implements Formula_Contracts_OptimizerInterface, Formula_Compiler_AST_NodeVisitor
{
    /** @var string[] Descriptions of transformations applied */
    private $transformations = array();

    /** @var Formula_Registry_FunctionRegistry|null Function registry for folding deterministic calls */
    private $functionRegistry;

    /** @var int Node count before optimization */
    private $nodesBefore = 0;

    /** @var int Node count after optimization */
    private $nodesAfter = 0;

    /**
     * Construct a constant folder.
     *
     * @param Formula_Registry_FunctionRegistry|null $functionRegistry
     *        Optional. When provided, calls to deterministic functions
     *        with all-constant arguments will be folded.
     */
    public function __construct($functionRegistry = null)
    {
        $this->functionRegistry = $functionRegistry;
    }

    // -----------------------------------------------------------------------
    //  OptimizerInterface
    // -----------------------------------------------------------------------

    /**
     * Optimize the AST by folding constant sub-expressions.
     *
     * @param Formula_Compiler_AST_Node $ast The validated AST to optimize
     * @return Formula_Compiler_AST_Node A new, optimized AST
     */
    public function optimize(Formula_Compiler_AST_Node $ast)
    {
        $this->transformations = array();
        $this->nodesBefore     = $this->countNodes($ast);

        $optimized = $ast->accept($this);

        $this->nodesAfter = $this->countNodes($optimized);

        return $optimized;
    }

    /**
     * Get the human-readable name of this optimizer.
     *
     * @return string
     */
    public function getName()
    {
        return 'ConstantFolder';
    }

    /**
     * Get the list of transformations applied during the last optimize() call.
     *
     * @return string[]
     */
    public function getTransformationsApplied()
    {
        return $this->transformations;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — Visit methods (one per AST node type)
    // -----------------------------------------------------------------------

    /**
     * Visit a literal node — already constant, return as-is.
     *
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return Formula_Compiler_AST_LiteralNode
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node)
    {
        // Literals are already constants. Return unchanged.
        return $node;
    }

    /**
     * Visit a variable node — NOT constant, return as-is.
     *
     * Variables can NEVER be constant-folded because their values
     * come from the FormulaContext at runtime.
     *
     * @param Formula_Compiler_AST_VariableNode $node
     * @return Formula_Compiler_AST_VariableNode
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node)
    {
        return $node;
    }

    /**
     * Visit a null node — already constant, return as-is.
     *
     * @param Formula_Compiler_AST_NullNode $node
     * @return Formula_Compiler_AST_NullNode
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node)
    {
        return $node;
    }

    /**
     * Visit a range node — NOT constant, return as-is.
     *
     * Cell range references require runtime spreadsheet context
     * and can never be constant-folded.
     *
     * @param Formula_Compiler_AST_RangeNode $node
     * @return Formula_Compiler_AST_RangeNode
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node)
    {
        return $node;
    }

    /**
     * Visit a binary operator node.
     *
     * First, recursively optimize both children. If both children
     * become constant literals after optimization, evaluate the
     * binary operation at compile time and produce a LiteralNode.
     * Otherwise, reconstruct the BinaryOperatorNode with the
     * optimized children.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        // Recursively optimize child nodes first (post-order)
        $leftOptimized  = $node->left->accept($this);
        $rightOptimized = $node->right->accept($this);

        // Check if both children are constant literals
        $leftIsLiteral  = $leftOptimized instanceof Formula_Compiler_AST_LiteralNode;
        $rightIsLiteral = $rightOptimized instanceof Formula_Compiler_AST_LiteralNode;

        // Also accept NullNode as effectively constant (treat null as 0 for arithmetic)
        $leftIsNull     = $leftOptimized instanceof Formula_Compiler_AST_NullNode;
        $rightIsNull    = $rightOptimized instanceof Formula_Compiler_AST_NullNode;

        $leftConstant   = $leftIsLiteral || $leftIsNull;
        $rightConstant  = $rightIsLiteral || $rightIsNull;

        if ($leftConstant && $rightConstant) {
            // Both sides constant — try to fold
            $leftVal  = $leftIsNull ? null : $leftOptimized->value;
            $rightVal = $rightIsNull ? null : $rightOptimized->value;
            $result   = $this->evaluateConstantBinary($node->operator, $leftVal, $rightVal);

            if ($result !== null && is_finite((float)$result)) {
                $resultNode = new Formula_Compiler_AST_LiteralNode(
                    $result,
                    $this->inferNumericType((float)$result),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );

                $this->transformations[] = sprintf(
                    'Constant folding: %s %s %s → %s',
                    $this->formatValue($leftVal),
                    $node->operator,
                    $this->formatValue($rightVal),
                    $this->formatValue($result)
                );

                return $resultNode;
            }
        }

        // Cannot fold — reconstruct with optimized children
        // Only create a new node if children actually changed
        if ($leftOptimized === $node->left && $rightOptimized === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_BinaryOperatorNode(
            $node->operator,
            $leftOptimized,
            $rightOptimized,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        // Preserve metadata
        $newNode->nodeMetadata = $node->nodeMetadata;

        return $newNode;
    }

    /**
     * Visit a unary operator node.
     *
     * Recursively optimize the operand. If the operand becomes a
     * constant literal, evaluate the unary operation at compile time.
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        $operandOptimized = $node->operand->accept($this);

        $isLiteral = $operandOptimized instanceof Formula_Compiler_AST_LiteralNode;
        $isNull    = $operandOptimized instanceof Formula_Compiler_AST_NullNode;

        if ($isLiteral || $isNull) {
            $value  = $isNull ? null : $operandOptimized->value;
            $result = $this->evaluateConstantUnary($node->operator, $value);

            if ($result !== null && is_finite((float)$result)) {
                $resultNode = new Formula_Compiler_AST_LiteralNode(
                    $result,
                    $this->inferNumericType((float)$result),
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );

                $this->transformations[] = sprintf(
                    'Constant folding: %s %s → %s',
                    $node->operator,
                    $this->formatValue($value),
                    $this->formatValue($result)
                );

                return $resultNode;
            }
        }

        if ($operandOptimized === $node->operand) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_UnaryOperatorNode(
            $node->operator,
            $operandOptimized,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;

        return $newNode;
    }

    /**
     * Visit a comparison node.
     *
     * If both sides are constant, evaluate the comparison and produce
     * a boolean LiteralNode.
     *
     * @param Formula_Compiler_AST_ComparisonNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        $leftOptimized  = $node->left->accept($this);
        $rightOptimized = $node->right->accept($this);

        $leftIsLiteral  = $leftOptimized instanceof Formula_Compiler_AST_LiteralNode;
        $rightIsLiteral = $rightOptimized instanceof Formula_Compiler_AST_LiteralNode;

        if ($leftIsLiteral && $rightIsLiteral) {
            $result = $this->evaluateConstantComparison(
                $node->operator,
                $leftOptimized->value,
                $rightOptimized->value
            );

            if ($result !== null) {
                $resultNode = new Formula_Compiler_AST_LiteralNode(
                    $result,
                    'boolean',
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );

                $this->transformations[] = sprintf(
                    'Constant folding: %s %s %s → %s',
                    $this->formatValue($leftOptimized->value),
                    $node->operator,
                    $this->formatValue($rightOptimized->value),
                    $result ? 'TRUE' : 'FALSE'
                );

                return $resultNode;
            }
        }

        if ($leftOptimized === $node->left && $rightOptimized === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_ComparisonNode(
            $node->operator,
            $leftOptimized,
            $rightOptimized,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;

        return $newNode;
    }

    /**
     * Visit a logical node (AND, OR, XOR).
     *
     * If both sides are constant booleans, evaluate the logical
     * operation at compile time.
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $leftOptimized  = $node->left->accept($this);
        $rightOptimized = $node->right->accept($this);

        $leftIsLiteral  = $leftOptimized instanceof Formula_Compiler_AST_LiteralNode;
        $rightIsLiteral = $rightOptimized instanceof Formula_Compiler_AST_LiteralNode;

        if ($leftIsLiteral && $rightIsLiteral
            && $leftOptimized->dataType === 'boolean'
            && $rightOptimized->dataType === 'boolean') {

            $result = $this->evaluateConstantLogical(
                $node->operator,
                $leftOptimized->value,
                $rightOptimized->value
            );

            if ($result !== null) {
                $resultNode = new Formula_Compiler_AST_LiteralNode(
                    $result,
                    'boolean',
                    $node->getSourceLine(),
                    $node->getSourceColumn()
                );

                $this->transformations[] = sprintf(
                    'Constant folding: %s %s %s → %s',
                    $leftOptimized->value ? 'TRUE' : 'FALSE',
                    $node->operator,
                    $rightOptimized->value ? 'TRUE' : 'FALSE',
                    $result ? 'TRUE' : 'FALSE'
                );

                return $resultNode;
            }
        }

        if ($leftOptimized === $node->left && $rightOptimized === $node->right) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_LogicalNode(
            $node->operator,
            $leftOptimized,
            $rightOptimized,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;

        return $newNode;
    }

    /**
     * Visit a function node.
     *
     * If the function is deterministic and all arguments are constant,
     * evaluate the function at compile time.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        // Recursively optimize all arguments
        $optimizedArgs = array();
        $allConstant   = true;
        $argsChanged   = false;

        foreach ($node->arguments as $i => $arg) {
            $optimized = $arg->accept($this);
            $optimizedArgs[] = $optimized;

            if ($optimized !== $arg) {
                $argsChanged = true;
            }

            if (!($optimized instanceof Formula_Compiler_AST_LiteralNode)
                && !($optimized instanceof Formula_Compiler_AST_NullNode)) {
                $allConstant = false;
            }
        }

        // Attempt constant folding if all args are constant and function is deterministic
        if ($allConstant && $this->functionRegistry !== null) {
            $func = $this->functionRegistry->get($node->functionName);

            if ($func !== null) {
                $meta = $func->getMetadata();

                if ($meta->isDeterministic) {
                    // Build argument values array
                    $argValues = array();
                    foreach ($optimizedArgs as $arg) {
                        $argValues[] = ($arg instanceof Formula_Compiler_AST_NullNode)
                            ? null
                            : $arg->value;
                    }

                    // The function's execute() needs a context. For compile-time
                    // constant folding, we construct a minimal empty context.
                    // Deterministic functions by definition should not depend
                    // on context state, so a mock context is adequate.
                    try {
                        $emptyContext = $this->buildEmptyContext();
                        $result = $func->execute($emptyContext, $argValues);

                        if ($result !== null && (!is_float($result) || is_finite($result))) {
                            $dataType = $this->inferDataType($result);

                            $resultNode = new Formula_Compiler_AST_LiteralNode(
                                $result,
                                $dataType,
                                $node->getSourceLine(),
                                $node->getSourceColumn()
                            );

                            $this->transformations[] = sprintf(
                                'Constant folding: %s(%s) → %s',
                                $node->functionName,
                                implode(', ', array_map(array($this, 'formatValue'), $argValues)),
                                $this->formatValue($result)
                            );

                            return $resultNode;
                        }
                    } catch (Exception $e) {
                        // Function execution failed at compile time — leave un-folded.
                        // The runtime will handle the error with proper diagnostics.
                    }
                }
            }
        }

        // Cannot fold — reconstruct with optimized arguments if changed
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
     * Visit a conditional node (IF).
     *
     * Recursively optimize condition and both branches. If the condition
     * becomes a constant boolean, this is reported but NOT eliminated here —
     * DeadBranchEliminator handles that. The ConstantFolder only folds
     * arithmetic/logical sub-expressions.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return Formula_Compiler_AST_Node
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        $condOptimized  = $node->condition->accept($this);
        $trueOptimized  = $node->trueBranch->accept($this);
        $falseOptimized = $node->falseBranch->accept($this);

        if ($condOptimized === $node->condition
            && $trueOptimized === $node->trueBranch
            && $falseOptimized === $node->falseBranch) {
            return $node;
        }

        $newNode = new Formula_Compiler_AST_ConditionalNode(
            $condOptimized,
            $trueOptimized,
            $falseOptimized,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );
        $newNode->nodeMetadata = $node->nodeMetadata;

        return $newNode;
    }

    // -----------------------------------------------------------------------
    //  Constant Evaluation Helpers
    // -----------------------------------------------------------------------

    /**
     * Evaluate a constant binary operation.
     *
     * Returns null if the operation cannot be evaluated (e.g., division
     * by zero, unsupported operator, or null operands for arithmetic).
     *
     * @param string $operator The operator symbol
     * @param mixed  $left     Left operand value
     * @param mixed  $right    Right operand value
     * @return float|int|bool|null Evaluated result, or null if not foldable
     */
    private function evaluateConstantBinary($operator, $left, $right)
    {
        // Null handling: null coallescing is handled by NullCoalescingOptimizer
        if ($left === null && $right === null) {
            return null;
        }

        // Convert null to 0 for arithmetic (PHP-like behavior)
        $l = ($left === null) ? 0.0 : (float)$left;
        $r = ($right === null) ? 0.0 : (float)$right;

        switch ($operator) {
            case '+':
                return $l + $r;

            case '-':
                return $l - $r;

            case '*':
                return $l * $r;

            case '/':
                if ($r == 0.0) {
                    return null; // Division by zero — leave for runtime
                }
                return $l / $r;

            case '%':
                if ($r == 0.0) {
                    return null; // Modulo by zero — leave for runtime
                }
                return fmod($l, $r);

            case '^':
                return pow($l, $r);

            case '??':
                // Null coalescing: left ?? right
                return ($left !== null) ? $left : $right;

            default:
                return null;
        }
    }

    /**
     * Evaluate a constant unary operation.
     *
     * @param string $operator The unary operator
     * @param mixed  $operand  The operand value
     * @return float|int|bool|null Evaluated result, or null if not foldable
     */
    private function evaluateConstantUnary($operator, $operand)
    {
        $val = ($operand === null) ? 0.0 : (float)$operand;

        switch ($operator) {
            case '-':
                return -$val;

            case '+':
                return +$val; // No-op, returns same value

            case 'NOT':
                return !((bool)$operand);

            default:
                return null;
        }
    }

    /**
     * Evaluate a constant comparison operation.
     *
     * @param string $operator Comparison operator
     * @param mixed  $left     Left value
     * @param mixed  $right    Right value
     * @return bool|null Evaluated boolean result, or null if not comparable
     */
    private function evaluateConstantComparison($operator, $left, $right)
    {
        if ($left === null && $right === null) {
            return ($operator === '==');
        }

        $l = ($left === null) ? 0.0 : (float)$left;
        $r = ($right === null) ? 0.0 : (float)$right;

        switch ($operator) {
            case '>':
                return $l > $r;
            case '<':
                return $l < $r;
            case '>=':
                return $l >= $r;
            case '<=':
                return $l <= $r;
            case '==':
                return $l == $r;
            case '!=':
            case '<>':
                return $l != $r;
            default:
                return null;
        }
    }

    /**
     * Evaluate a constant logical operation.
     *
     * @param string $operator Logical operator
     * @param bool   $left     Left boolean value
     * @param bool   $right    Right boolean value
     * @return bool|null
     */
    private function evaluateConstantLogical($operator, $left, $right)
    {
        switch ($operator) {
            case 'AND':
                return (bool)$left && (bool)$right;
            case 'OR':
                return (bool)$left || (bool)$right;
            case 'XOR':
                return (bool)$left xor (bool)$right;
            default:
                return null;
        }
    }

    // -----------------------------------------------------------------------
    //  Utility Helpers
    // -----------------------------------------------------------------------

    /**
     * Infer the data type string for a PHP value.
     *
     * @param mixed $value
     * @return string 'integer', 'decimal', 'boolean', 'string', 'null'
     */
    private function inferDataType($value)
    {
        if ($value === null) {
            return 'null';
        }
        if (is_bool($value)) {
            return 'boolean';
        }
        if (is_int($value)) {
            return 'integer';
        }
        if (is_float($value)) {
            // Check if the float is actually a whole number
            if (floor($value) == $value && is_finite($value) && abs($value) <= PHP_INT_MAX) {
                return 'integer';
            }
            return 'decimal';
        }
        if (is_string($value)) {
            return 'string';
        }
        return 'decimal';
    }

    /**
     * Infer the numeric type for a float value.
     *
     * @param float $value
     * @return string 'integer' or 'decimal'
     */
    private function inferNumericType($value)
    {
        if (floor((float)$value) == (float)$value && is_finite((float)$value) && abs((float)$value) <= PHP_INT_MAX) {
            return 'integer';
        }
        return 'decimal';
    }

    /**
     * Format a value for human-readable transformation messages.
     *
     * @param mixed $value
     * @return string
     */
    private function formatValue($value)
    {
        if ($value === null) {
            return 'NULL';
        }
        if (is_bool($value)) {
            return $value ? 'TRUE' : 'FALSE';
        }
        if (is_float($value)) {
            // Remove trailing zeros for readability
            $str = rtrim(rtrim(sprintf('%.10f', $value), '0'), '.');
            return $str;
        }
        if (is_int($value)) {
            return (string)$value;
        }
        if (is_string($value)) {
            return '"' . $value . '"';
        }
        return (string)$value;
    }

    /**
     * Count total nodes in an AST (for diagnostic comparison).
     *
     * @param Formula_Compiler_AST_Node|Formula_Compiler_AST_NullNode|null $node
     * @return int
     */
    private function countNodes($node)
    {
        if ($node === null) {
            return 0;
        }
        if ($node instanceof Formula_Compiler_AST_NullNode) {
            return 1;
        }
        $count = 1;
        foreach ($node->getChildren() as $child) {
            $count += $this->countNodes($child);
        }
        return $count;
    }

    /**
     * Build a minimal empty FormulaContext for compile-time function folding.
     *
     * Deterministic functions should not depend on context state. This
     * method provides a bare-minimum context that satisfies the interface
     * for function execution during constant folding.
     *
     * @return Formula_Context_FormulaContext
     */
    private function buildEmptyContext()
    {
        // Lazily instantiate to avoid circular dependency.
        // FormulaContext has a private constructor — we create a
        // minimal instance through FormulaContextBuilder.
        if (class_exists('Formula_Context_FormulaContextBuilder')) {
            return Formula_Context_FormulaContextBuilder::create()->build();
        }

        // If the context classes don't exist yet (early development),
        // return null. The function's execute() should handle this
        // gracefully. This path is only hit during testing phases
        // before the context system is implemented.
        return null;
    }
}
