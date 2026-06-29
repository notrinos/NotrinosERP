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
 * RuntimeSession — Per-evaluation session with memoization and resource limits.
 *
 * Each call to FormulaRuntime::execute() creates a short-lived RuntimeSession.
 * The session tracks:
 *
 *  - The immutable FormulaContext for this evaluation
 *  - Memoized variable resolutions (avoid re-resolving within one evaluation)
 *  - Memoized function results for deterministic calls
 *  - Node evaluation count (resource limit enforcement)
 *  - Variable resolution count (diagnostics)
 *  - Function call count (diagnostics)
 *  - Start time (time limit enforcement)
 *  - Peak memory usage (memory limit enforcement)
 *
 * The session is discarded after execution. No state persists across
 * formula evaluations. This ensures:
 *  - Multi-tenant isolation (no cross-company data leaks)
 *  - Deterministic evaluation (no session carryover)
 *  - Thread safety (session is single-evaluation, single-thread)
 *
 * @package Formula\Runtime
 * @since   2.0.0
 */
class Formula_Runtime_RuntimeSession
{
    /** @var Formula_Context_FormulaContext The immutable execution context */
    private $context;

    /** @var array Resolved variable cache: qualifiedName => value */
    private $resolvedVariables = array();

    /** @var array Function result cache: "functionName:" . serialize(args) => result */
    private $functionResults = array();

    /** @var int Number of AST nodes evaluated so far */
    private $nodeEvaluationCount = 0;

    /** @var int Number of variable resolutions performed */
    private $variableResolutions = 0;

    /** @var int Number of function calls executed */
    private $functionCalls = 0;

    /** @var float Unix timestamp with microseconds when evaluation started */
    private $startTime = 0.0;

    /** @var int Peak memory usage at start (bytes) */
    private $startMemory = 0;

    /** @var int Maximum node evaluation count before ResourceExhaustedException */
    private $maxNodeEvaluations;

    /** @var int Maximum execution time in milliseconds */
    private $maxExecutionTimeMs;

    /** @var int Maximum memory delta in bytes */
    private $maxMemoryBytes;

    /**
     * Construct a runtime session.
     *
     * @param Formula_Context_FormulaContext $context         The immutable execution context
     * @param int                            $maxNodeEvaluations Max node evaluations (0 = constant)
     * @param int                            $maxExecutionTimeMs Max execution time in ms (0 = constant)
     * @param int                            $maxMemoryBytes    Max additional memory in bytes (0 = constant)
     */
    public function __construct(
        Formula_Context_FormulaContext $context,
        $maxNodeEvaluations = 0,
        $maxExecutionTimeMs = 0,
        $maxMemoryBytes = 0
    ) {
        $this->context            = $context;
        $this->startTime          = $this->microtimeFloat();
        $this->startMemory        = memory_get_peak_usage(true);
        $this->maxNodeEvaluations = $maxNodeEvaluations > 0
            ? $maxNodeEvaluations
            : FORMULA_MAX_NODE_EVALUATIONS;
        $this->maxExecutionTimeMs = $maxExecutionTimeMs > 0
            ? $maxExecutionTimeMs
            : FORMULA_MAX_EXECUTION_TIME_MS;
        $this->maxMemoryBytes     = $maxMemoryBytes > 0
            ? $maxMemoryBytes
            : FORMULA_MAX_MEMORY_BYTES;
    }

    // -----------------------------------------------------------------------
    //  Context access
    // -----------------------------------------------------------------------

    /**
     * Get the immutable execution context.
     *
     * @return Formula_Context_FormulaContext
     */
    public function getContext()
    {
        return $this->context;
    }

    // -----------------------------------------------------------------------
    //  Variable memoization
    // -----------------------------------------------------------------------

    /**
     * Get a previously resolved variable value.
     *
     * @param string $qualifiedName The fully qualified variable name
     * @return mixed|null The cached value, or null if not yet resolved
     */
    public function getResolvedVariable($qualifiedName)
    {
        $key = strtoupper($qualifiedName);
        return isset($this->resolvedVariables[$key])
            ? $this->resolvedVariables[$key]
            : null;
    }

    /**
     * Cache a resolved variable value.
     *
     * Once resolved, subsequent references to the same variable within
     * the same evaluation return the cached value without re-resolving.
     * This is both a performance optimization and a correctness guarantee
     * (a variable that changes during evaluation would produce inconsistent
     * results within a single formula).
     *
     * @param string $qualifiedName The fully qualified variable name
     * @param mixed  $value         The resolved value
     * @return void
     */
    public function setResolvedVariable($qualifiedName, $value)
    {
        $this->resolvedVariables[strtoupper($qualifiedName)] = $value;
    }

    // -----------------------------------------------------------------------
    //  Function result cache
    // -----------------------------------------------------------------------

    /**
     * Get a cached deterministic function result.
     *
     * @param string $functionName The function name
     * @param array  $arguments    The resolved arguments
     * @return mixed|null The cached result, or null if not cached
     */
    public function getFunctionResult($functionName, array $arguments)
    {
        $key = strtoupper($functionName) . ':' . serialize($arguments);
        return isset($this->functionResults[$key])
            ? $this->functionResults[$key]
            : null;
    }

    /**
     * Cache a deterministic function result.
     *
     * @param string $functionName The function name
     * @param array  $arguments    The resolved arguments
     * @param mixed  $result       The function result
     * @return void
     */
    public function setFunctionResult($functionName, array $arguments, $result)
    {
        $key = strtoupper($functionName) . ':' . serialize($arguments);
        $this->functionResults[$key] = $result;
    }

    // -----------------------------------------------------------------------
    //  Resource tracking
    // -----------------------------------------------------------------------

    /**
     * Increment the node evaluation counter.
     *
     * @return int The new count
     */
    public function incrementNodeEvaluations()
    {
        $this->nodeEvaluationCount++;
        return $this->nodeEvaluationCount;
    }

    /**
     * Increment the variable resolution counter.
     *
     * @return int The new count
     */
    public function incrementVariableResolutions()
    {
        $this->variableResolutions++;
        return $this->variableResolutions;
    }

    /**
     * Increment the function call counter.
     *
     * @return int The new count
     */
    public function incrementFunctionCalls()
    {
        $this->functionCalls++;
        return $this->functionCalls;
    }

    /**
     * Check execution time limit.
     *
     * @return void
     * @throws Formula_Exceptions_ResourceExhaustedException If time limit exceeded
     */
    public function checkTimeLimit()
    {
        $elapsed = ($this->microtimeFloat() - $this->startTime) * 1000;
        if ($elapsed > $this->maxExecutionTimeMs) {
            throw new Formula_Exceptions_ResourceExhaustedException(
                sprintf(
                    'Formula execution time exceeded maximum of %d ms.',
                    $this->maxExecutionTimeMs
                ),
                'MAX_EXECUTION_TIME_MS',
                $this->maxExecutionTimeMs
            );
        }
    }

    /**
     * Check memory limit.
     *
     * @return void
     * @throws Formula_Exceptions_ResourceExhaustedException If memory limit exceeded
     */
    public function checkMemoryLimit()
    {
        $currentMemory = memory_get_peak_usage(true);
        $delta = $currentMemory - $this->startMemory;
        if ($delta > $this->maxMemoryBytes) {
            throw new Formula_Exceptions_ResourceExhaustedException(
                sprintf(
                    'Formula execution memory exceeded maximum of %d bytes.',
                    $this->maxMemoryBytes
                ),
                'MAX_MEMORY_BYTES',
                $this->maxMemoryBytes
            );
        }
    }

    // -----------------------------------------------------------------------
    //  Diagnostics
    // -----------------------------------------------------------------------

    /**
     * Get the total number of nodes evaluated.
     *
     * @return int
     */
    public function getNodeEvaluationCount()
    {
        return $this->nodeEvaluationCount;
    }

    /**
     * Get the number of variable resolutions performed.
     *
     * @return int
     */
    public function getVariableResolutions()
    {
        return $this->variableResolutions;
    }

    /**
     * Get the number of function calls executed.
     *
     * @return int
     */
    public function getFunctionCalls()
    {
        return $this->functionCalls;
    }

    /**
     * Get the total elapsed time in milliseconds.
     *
     * @return float
     */
    public function getElapsedMs()
    {
        return ($this->microtimeFloat() - $this->startTime) * 1000;
    }

    /**
     * Get peak memory used during this evaluation (bytes).
     *
     * @return int
     */
    public function getPeakMemoryBytes()
    {
        return memory_get_peak_usage(true) - $this->startMemory;
    }

    // -----------------------------------------------------------------------
    //  Private
    // -----------------------------------------------------------------------

    /**
     * High-resolution time for performance measurement.
     *
     * @return float Current time in seconds with microsecond precision
     */
    private function microtimeFloat()
    {
        list($usec, $sec) = explode(' ', microtime());
        return ((float)$usec + (float)$sec);
    }
}
