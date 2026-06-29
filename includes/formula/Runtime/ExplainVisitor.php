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
 * ExplainVisitor — Step-by-step evaluation tracer.
 *
 * Wraps the standard NodeEvaluator with instrumentation that records
 * every evaluation step: which node was evaluated, the input values,
 * the output value, and the elapsed time per step.
 *
 * The output is an ExplainResult containing a complete, human-readable
 * trace of the entire evaluation. This is used for:
 *
 *  - Debugging formulas that produce unexpected results
 *  - Audit trail for sensitive financial calculations
 *  - Formula editor "Evaluate Step-by-Step" feature (v2.x)
 *  - Developer diagnostics during framework development
 *
 * ## Design
 *
 * The ExplainVisitor implements the same NodeVisitor interface as
 * NodeEvaluator. It delegates actual computation to a wrapped
 * NodeEvaluator instance, but records metadata before and after
 * each visit call.
 *
 * This separation ensures that the ExplainVisitor cannot produce
 * different results from the production evaluator — they share the
 * same implementation.
 *
 * @package Formula\Runtime
 * @since   2.0.0
 */
class Formula_Runtime_ExplainVisitor implements Formula_Compiler_AST_NodeVisitor
{
    /** @var Formula_Runtime_NodeEvaluator The production evaluator to delegate to */
    private $evaluator;

    /** @var Formula_Diagnostics_ExplainResult The accumulated trace result */
    private $result;

    /** @var Formula_Runtime_RuntimeSession|null Active session */
    private $session;

    /** @var int Step counter (1-based) */
    private $stepCounter = 0;

    /** @var int Nodes evaluated (tracked separately for ExplainResult) */
    private $nodesEvaluated = 0;

    /** @var int Variables resolved */
    private $variablesResolved = 0;

    /** @var int Functions called */
    private $functionsCalled = 0;

    /**
     * Construct the explain visitor.
     *
     * @param Formula_Runtime_NodeEvaluator $evaluator The production evaluator
     * @param string                         $formula   The original formula source
     */
    public function __construct(Formula_Runtime_NodeEvaluator $evaluator, $formula = '')
    {
        $this->evaluator = $evaluator;
        $this->result    = new Formula_Diagnostics_ExplainResult();
        $this->result->formulaSource = (string)$formula;
    }

    /**
     * Evaluate the complete AST with step-by-step tracing.
     *
     * @param Formula_Compiler_AST_Node       $node    The AST root
     * @param Formula_Runtime_RuntimeSession  $session The evaluation session
     * @return Formula_Diagnostics_ExplainResult The complete trace
     */
    public function evaluate(
        Formula_Compiler_AST_Node $node,
        Formula_Runtime_RuntimeSession $session
    ) {
        $this->session = $session;
        $startTime = $this->microtimeFloat();

        // Delegate to the production evaluator, which will call back
        // into our visit* methods with instrumentation
        $finalResult = $this->evaluator->evaluate($node, $session);

        $this->result->durationMs = ($this->microtimeFloat() - $startTime) * 1000;
        $this->result->result = $finalResult;
        $this->result->nodesEvaluated = $this->nodesEvaluated;
        $this->result->variablesResolved = $this->variablesResolved;
        $this->result->functionsCalled = $this->functionsCalled;

        return $this->result;
    }

    // -----------------------------------------------------------------------
    //  NodeVisitor — instrumented visit methods
    // -----------------------------------------------------------------------

    /**
     * Visit a literal node with tracing.
     *
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return mixed
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node)
    {
        $stepStart = $this->microtimeFloat();

        $value = $this->evaluator->visitLiteral($node);
        $this->nodesEvaluated++;

        $this->addStep(
            'literal',
            sprintf('Literal value: %s', $this->formatValue($value)),
            null,
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    /**
     * Visit a variable reference with tracing.
     *
     * @param Formula_Compiler_AST_VariableNode $node
     * @return mixed
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node)
    {
        $stepStart = $this->microtimeFloat();

        $value = $this->evaluator->visitVariable($node);
        $this->nodesEvaluated++;
        $this->variablesResolved++;

        $this->addStep(
            'variable',
            sprintf(
                '%s → %s',
                $node->getQualifiedName(),
                $this->formatValue($value)
            ),
            $node->getQualifiedName(),
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    /**
     * Visit a function call with tracing.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return mixed
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node)
    {
        $stepStart = $this->microtimeFloat();

        $value = $this->evaluator->visitFunction($node);
        $this->nodesEvaluated++;
        $this->functionsCalled++;

        $this->addStep(
            'function',
            sprintf(
                '%s(%s) → %s',
                $node->functionName,
                $this->describeArguments($node),
                $this->formatValue($value)
            ),
            $node->functionName,
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    /**
     * Visit a binary operator with tracing.
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return mixed
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        // We must evaluate children ourselves (the wrapper can't
        // intercept the evaluator's internal calls to visitBinary).
        // Instead, we evaluate children through our visit methods,
        // then apply the operator.

        $stepStart = $this->microtimeFloat();

        $left  = $node->left->accept($this);
        $right = $this->evaluator->visitBinary($node);

        $this->nodesEvaluated++;

        $this->addStep(
            'binary',
            sprintf(
                '%s %s %s → %s',
                $this->formatValue($left),
                $node->operator,
                $this->formatValue($this->resolveRightValue($node)),
                $this->formatValue($right)
            ),
            array('left' => $left, 'operator' => $node->operator),
            $right,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $right;
    }

    /**
     * Visit a unary operator with tracing.
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return mixed
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        $stepStart = $this->microtimeFloat();

        $value = $this->evaluator->visitUnary($node);
        $this->nodesEvaluated++;

        $this->addStep(
            'unary',
            sprintf(
                '%s %s → %s',
                $node->operator,
                $this->formatValue($this->resolveOperandValue($node)),
                $this->formatValue($value)
            ),
            $node->operator,
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    /**
     * Visit a conditional (IF) with tracing.
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return mixed
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node)
    {
        $stepStart = $this->microtimeFloat();

        // Evaluate condition through our visitor for tracing
        $conditionValue = $node->condition->accept($this);

        $branch = (bool)$conditionValue ? 'true' : 'false';
        $branchNode = (bool)$conditionValue ? $node->trueBranch : $node->falseBranch;

        // Evaluate the selected branch through our visitor
        $value = $branchNode->accept($this);

        $this->nodesEvaluated++;

        $this->addStep(
            'conditional',
            sprintf(
                'IF(%s) → %s branch → %s',
                $this->formatValue($conditionValue),
                $branch,
                $this->formatValue($value)
            ),
            array('condition' => $conditionValue, 'branch' => $branch),
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    /**
     * Visit a comparison with tracing.
     *
     * @param Formula_Compiler_AST_ComparisonNode $node
     * @return mixed
     */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node)
    {
        $stepStart = $this->microtimeFloat();

        // Evaluate both sides through our visitor
        $left  = $node->left->accept($this);
        $right = $node->right->accept($this);

        $this->nodesEvaluated++;

        $value = $this->evaluator->visitComparison($node);

        $this->addStep(
            'comparison',
            sprintf(
                '%s %s %s → %s',
                $this->formatValue($left),
                $node->operator,
                $this->formatValue($right),
                $value ? 'TRUE' : 'FALSE'
            ),
            array('left' => $left, 'operator' => $node->operator, 'right' => $right),
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    /**
     * Visit a logical operator with tracing.
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return mixed
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node)
    {
        $stepStart = $this->microtimeFloat();

        $left = $node->left->accept($this);

        // Short-circuit tracing
        if ($node->operator === 'AND' && !(bool)$left) {
            $this->nodesEvaluated++;
            $this->addStep(
                'logical',
                sprintf(
                    'FALSE AND (short-circuit) → FALSE'
                ),
                array('left' => $left, 'operator' => 'AND'),
                false,
                $stepStart,
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
            return false;
        }

        if ($node->operator === 'OR' && (bool)$left) {
            $this->nodesEvaluated++;
            $this->addStep(
                'logical',
                sprintf(
                    'TRUE OR (short-circuit) → TRUE'
                ),
                array('left' => $left, 'operator' => 'OR'),
                true,
                $stepStart,
                $node->getSourceLine(),
                $node->getSourceColumn()
            );
            return true;
        }

        $value = $this->evaluator->visitLogical($node);
        $this->nodesEvaluated++;

        $this->addStep(
            'logical',
            sprintf(
                '%s %s %s → %s',
                $this->formatValue($left),
                $node->operator,
                '...',
                $value ? 'TRUE' : 'FALSE'
            ),
            array('left' => $left, 'operator' => $node->operator),
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    /**
     * Visit a range with tracing.
     *
     * @param Formula_Compiler_AST_RangeNode $node
     * @return mixed
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node)
    {
        // Ranges throw in v1 — but we still trace the attempt
        $stepStart = $this->microtimeFloat();

        try {
            $value = $this->evaluator->visitRange($node);
            $this->addStep('range', sprintf('%s:%s', $node->startCell, $node->endCell), null, $value, $stepStart, $node->getSourceLine(), $node->getSourceColumn());
            return $value;
        } catch (Formula_Exceptions_RuntimeExecutionException $e) {
            $this->addStep('range', sprintf('Range %s:%s → NOT SUPPORTED', $node->startCell, $node->endCell), null, null, $stepStart, $node->getSourceLine(), $node->getSourceColumn());
            throw $e;
        }
    }

    /**
     * Visit a null literal with tracing.
     *
     * @param Formula_Compiler_AST_NullNode $node
     * @return mixed
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node)
    {
        $stepStart = $this->microtimeFloat();

        $value = $this->evaluator->visitNull($node);
        $this->nodesEvaluated++;

        $this->addStep(
            'null',
            sprintf('NULL → %s', $this->formatValue($value)),
            null,
            $value,
            $stepStart,
            $node->getSourceLine(),
            $node->getSourceColumn()
        );

        return $value;
    }

    // -----------------------------------------------------------------------
    //  Private helpers
    // -----------------------------------------------------------------------

    /**
     * Add an evaluation step to the trace.
     *
     * @param string $nodeType       Node type description
     * @param string $description    Human-readable description
     * @param mixed  $input          Input values
     * @param mixed  $output         Output value
     * @param float  $stepStart      Step start time (microtime float)
     * @param int    $sourceLine     Source line
     * @param int    $sourceColumn   Source column
     * @return void
     */
    private function addStep($nodeType, $description, $input, $output, $stepStart, $sourceLine, $sourceColumn)
    {
        $this->stepCounter++;
        $duration = ($this->microtimeFloat() - $stepStart) * 1000;

        $this->result->addStep(array(
            'stepNumber'          => $this->stepCounter,
            'nodeType'            => $nodeType,
            'description'         => $description,
            'input'               => $input,
            'output'              => $output,
            'durationMs'          => round($duration, 4),
            'sourceLine'          => $sourceLine,
            'sourceColumn'        => $sourceColumn,
        ));
    }

    /**
     * Format a value for human-readable display in explain trace.
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
            // Display up to 6 decimal places, trim trailing zeros
            $formatted = sprintf('%.6f', $value);
            $formatted = rtrim($formatted, '0');
            $formatted = rtrim($formatted, '.');
            return $formatted;
        }
        if (is_int($value)) {
            return (string)$value;
        }
        if (is_string($value)) {
            $maxLen = 80;
            if (strlen($value) > $maxLen) {
                return '"' . substr($value, 0, $maxLen) . '..."';
            }
            return '"' . $value . '"';
        }
        if (is_array($value)) {
            return 'Array(' . count($value) . ')';
        }
        return (string)$value;
    }

    /**
     * Describe function arguments for explain trace.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return string
     */
    private function describeArguments(Formula_Compiler_AST_FunctionNode $node)
    {
        // Arguments have already been evaluated at this point.
        // We don't have direct access to the resolved values,
        // so we use a descriptive approach.
        $count = count($node->arguments);
        if ($count === 0) {
            return '';
        }
        return $count . ' arg' . ($count > 1 ? 's' : '');
    }

    /**
     * Resolve right operand for display (for the binary node
     * explain step, we need to extract the pre-computed value).
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return mixed
     */
    private function resolveRightValue(Formula_Compiler_AST_BinaryOperatorNode $node)
    {
        // For explain mode, we re-evaluate the right side separately
        // to capture its value for the trace. This is only needed
        // for display purposes — the production visitBinary has
        // already computed the correct result.
        try {
            return $node->right->accept($this);
        } catch (Exception $e) {
            return '?';
        }
    }

    /**
     * Resolve the operand value for unary operator display.
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return mixed
     */
    private function resolveOperandValue(Formula_Compiler_AST_UnaryOperatorNode $node)
    {
        try {
            return $node->operand->accept($this);
        } catch (Exception $e) {
            return '?';
        }
    }

    /**
     * High-resolution time.
     *
     * @return float
     */
    private function microtimeFloat()
    {
        list($usec, $sec) = explode(' ', microtime());
        return ((float)$usec + (float)$sec);
    }
}
