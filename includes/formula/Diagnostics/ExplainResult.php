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
 * ExplainResult — Step-by-step evaluation trace for formula debugging.
 *
 * Produced by the ExplainVisitor which wraps the standard evaluator
 * with instrumentation. Each step records the node evaluated, input
 * values, output value, and timing.
 *
 * Used for:
 *  - Debugging formulas that produce unexpected results
 *  - Formula editor "Evaluate Step-by-Step" feature (v2.x)
 *  - Audit trail for sensitive financial calculations
 *
 * @package Formula\Diagnostics
 * @since   2.0.0
 */
class Formula_Diagnostics_ExplainResult
{
    /** @var mixed The final evaluation result */
    public $result;

    /** @var array Ordered list of execution steps */
    public $steps;

    /** @var float Total evaluation time in milliseconds */
    public $durationMs;

    /** @var int Number of AST nodes evaluated */
    public $nodesEvaluated;

    /** @var int Number of variable resolutions performed */
    public $variablesResolved;

    /** @var int Number of function calls executed */
    public $functionsCalled;

    /** @var string The original formula source */
    public $formulaSource;

    /**
     * Construct an explain result.
     *
     * @param mixed  $result             Final evaluation result
     * @param array  $steps              Execution steps (see addStep for format)
     * @param float  $durationMs         Total duration in milliseconds
     * @param int    $nodesEvaluated     Number of AST nodes evaluated
     * @param int    $variablesResolved  Number of variables resolved
     * @param int    $functionsCalled    Number of functions called
     * @param string $formulaSource      Original formula text
     */
    public function __construct(
        $result = null,
        array $steps = array(),
        $durationMs = 0.0,
        $nodesEvaluated = 0,
        $variablesResolved = 0,
        $functionsCalled = 0,
        $formulaSource = ''
    ) {
        $this->result            = $result;
        $this->steps             = $steps;
        $this->durationMs        = (float)$durationMs;
        $this->nodesEvaluated    = (int)$nodesEvaluated;
        $this->variablesResolved = (int)$variablesResolved;
        $this->functionsCalled   = (int)$functionsCalled;
        $this->formulaSource     = (string)$formulaSource;
    }

    /**
     * Add an execution step to the trace.
     *
     * Each step records:
     *  - stepNumber: 1-based step index
     *  - description: Human-readable description (e.g., "Employee.BasicSalary → 8,000")
     *  - nodeType: AST node type evaluated
     *  - input: Input values (array or scalar)
     *  - output: Output value
     *  - durationMicroseconds: Time spent on this step
     *  - sourceLine: Source line of the node
     *  - sourceColumn: Source column of the node
     *
     * @param array $stepData Step data with keys: stepNumber, description, nodeType,
     *                        input, output, durationMicroseconds, sourceLine, sourceColumn
     * @return void
     */
    public function addStep(array $stepData)
    {
        $this->steps[] = array(
            'stepNumber'          => isset($stepData['stepNumber']) ? (int)$stepData['stepNumber'] : count($this->steps) + 1,
            'description'         => isset($stepData['description']) ? (string)$stepData['description'] : '',
            'nodeType'            => isset($stepData['nodeType']) ? (string)$stepData['nodeType'] : 'unknown',
            'input'               => isset($stepData['input']) ? $stepData['input'] : null,
            'output'              => isset($stepData['output']) ? $stepData['output'] : null,
            'durationMicroseconds' => isset($stepData['durationMicroseconds']) ? (float)$stepData['durationMicroseconds'] : 0.0,
            'sourceLine'          => isset($stepData['sourceLine']) ? (int)$stepData['sourceLine'] : 0,
            'sourceColumn'        => isset($stepData['sourceColumn']) ? (int)$stepData['sourceColumn'] : 0,
        );
    }

    /**
     * Build a human-readable explanation string.
     *
     * Example output:
     *   Expression: (Employee.BasicSalary * 0.10) + Bonus
     *   Step 1: Employee.BasicSalary → 8,000
     *   Step 2: 8,000 × 0.10 → 800
     *   Step 3: Bonus → 500
     *   Step 4: 800 + 500 → 1,300
     *   Result: 1,300
     *   Duration: 0.42ms
     *   Nodes evaluated: 5
     *
     * @return string
     */
    public function toHumanReadable()
    {
        $lines = array();
        $lines[] = 'Expression: ' . $this->formulaSource;

        foreach ($this->steps as $step) {
            $lines[] = sprintf(
                'Step %d: %s',
                $step['stepNumber'],
                $step['description']
            );
        }

        $lines[] = 'Result: ' . (is_scalar($this->result) ? (string)$this->result : json_encode($this->result));
        $lines[] = sprintf('Duration: %.2fms', $this->durationMs);
        $lines[] = 'Nodes evaluated: ' . $this->nodesEvaluated;

        return implode("\n", $lines);
    }

    /**
     * Convert to array for serialization, API responses, and JSON output.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'result'            => $this->result,
            'steps'             => $this->steps,
            'durationMs'        => $this->durationMs,
            'nodesEvaluated'    => $this->nodesEvaluated,
            'variablesResolved' => $this->variablesResolved,
            'functionsCalled'   => $this->functionsCalled,
            'formulaSource'     => $this->formulaSource,
        );
    }

    /**
     * Create from a serialized array.
     *
     * @param array $data
     * @return Formula_Diagnostics_ExplainResult
     */
    public static function fromArray(array $data)
    {
        return new self(
            isset($data['result']) ? $data['result'] : null,
            isset($data['steps']) ? (array)$data['steps'] : array(),
            isset($data['durationMs']) ? (float)$data['durationMs'] : 0.0,
            isset($data['nodesEvaluated']) ? (int)$data['nodesEvaluated'] : 0,
            isset($data['variablesResolved']) ? (int)$data['variablesResolved'] : 0,
            isset($data['functionsCalled']) ? (int)$data['functionsCalled'] : 0,
            isset($data['formulaSource']) ? (string)$data['formulaSource'] : ''
        );
    }
}
