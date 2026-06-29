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
 * FormulaRuntime — Core formula execution engine.
 *
 * The FormulaRuntime orchestrates compilation and execution of formulas.
 * It holds references to the FunctionRegistry and VariableRegistry,
 * and delegates compilation to the compiler pipeline and execution to
 * the tree-walk evaluator.
 *
 * This class is NOT instantiated by modules. Modules use the static
 * FormulaFacade, which internally manages a singleton FormulaRuntime.
 *
 * The compiler pipeline (Lexer, Parser, Validators, Optimizers) will be
 * integrated into this class when those components are built.
 *
 * @package Formula\Runtime
 * @since   2.0.0
 */
class Formula_Runtime_FormulaRuntime
{
    /** @var Formula_Registry_FunctionRegistry */
    private $functionRegistry;

    /** @var Formula_Registry_VariableRegistry */
    private $variableRegistry;

    /** @var Formula_Compiler_FormulaCompiler|null Lazily-initialized compiler */
    private $compiler = null;

    /** @var string[] Simple variable names known to the context (for semantic validation) */
    private $knownContextVariables = array();

    /**
     * Construct the runtime engine.
     *
     * The compiler is lazily-initialized on first use (compile, evaluate,
     * validate, or explain). This allows the registries to be populated
     * during bootstrap before any formula processing begins.
     *
     * @param Formula_Registry_FunctionRegistry $functionRegistry
     * @param Formula_Registry_VariableRegistry $variableRegistry
     * @param string[]                          $knownContextVariables Simple variable names valid without namespace
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

    /**
     * Get or create the compiler instance.
     *
     * Lazy initialization ensures the registries are fully populated
     * before the compiler's validator pipeline is constructed.
     *
     * @return Formula_Compiler_FormulaCompiler
     */
    private function getCompiler()
    {
        if ($this->compiler === null) {
            $this->compiler = new Formula_Compiler_FormulaCompiler(
                $this->functionRegistry,
                $this->variableRegistry,
                array(),               // Default validators (created by FormulaCompiler)
                $this->createOptimizers(), // Standard optimizer pipeline
                100,                   // Max AST depth
                10000,                 // Max source length
                $this->knownContextVariables
            );
        }
        return $this->compiler;
    }

    /**
     * Create the standard optimizer pipeline.
     *
     * Optimizers are applied in this order: ConstantFolder →
     * BooleanSimplifier → DeadBranchEliminator → AlgebraicSimplifier →
     * NullCoalescingOptimizer.
     *
     * Each optimizer transforms the AST into a semantically equivalent
     * but more efficient form. The order matters — constant folding must
     * run first so that boolean simplifier and dead branch eliminator
     * can act on the resulting constant booleans.
     *
     * @return Formula_Contracts_OptimizerInterface[]
     */
    private function createOptimizers()
    {
        return array(
            new Formula_Compiler_ConstantFolder($this->functionRegistry),
            new Formula_Compiler_BooleanSimplifier(),
            new Formula_Compiler_DeadBranchEliminator(),
            new Formula_Compiler_AlgebraicSimplifier(),
            new Formula_Compiler_NullCoalescingOptimizer(),
        );
    }

    /**
     * Compile a formula string into a compiled form.
     *
     * Delegates to the FormulaCompiler which runs the full pipeline:
     * preprocess → lex → parse → validate (syntax, semantic, type,
     * dependency) → optimize → CompiledFormula.
     *
     * @param string $formula The preprocessed formula string
     * @return Formula_Compiler_CompiledFormula
     * @throws Formula_Exceptions_SyntaxErrorException On lexer/parser errors
     * @throws Formula_Exceptions_UnknownFunctionException On unknown function references
     * @throws Formula_Exceptions_UnknownVariableException On unknown variable references
     * @throws Formula_Exceptions_TypeMismatchException On type incompatibility
     * @throws Formula_Exceptions_CircularReferenceException On circular dependencies
     * @throws Formula_Exceptions_ResourceExhaustedException On resource limit exceeded
     */
    public function compile($formula)
    {
        return $this->getCompiler()->compile($formula);
    }

    /**
     * Compile and evaluate in one call.
     *
     * Compiles the formula (with caching via facade), then executes
     * the resulting AST against the provided context.
     *
     * @param string                        $formula The preprocessed formula string
     * @param Formula_Context_FormulaContext $context The execution context
     * @return mixed
     * @throws Formula_Exceptions_FormulaException On compilation or execution errors
     */
    public function evaluate($formula, Formula_Context_FormulaContext $context)
    {
        $compiled = $this->compile($formula);
        return $this->execute($compiled, $context);
    }

    /**
     * Execute a compiled formula against a context.
     *
     * Creates a RuntimeSession for this evaluation, constructs a
     * NodeEvaluator (tree-walk interpreter) wired to the variable and
     * function resolution infrastructure, and walks the AST in
     * post-order (children before parent) to produce the result.
     *
     * ## Evaluation Semantics
     *
     * - Post-order traversal: children evaluated before parent
     * - Short-circuit: AND, OR, IF skip unused branches
     * - Lazy variable resolution via NamespaceRegistry
     * - Resource limits enforced via RuntimeSession
     *
     * @param Formula_Compiler_CompiledFormula $compiled The compiled formula
     * @param Formula_Context_FormulaContext   $context  The immutable execution context
     * @return mixed The evaluation result
     * @throws Formula_Exceptions_DivideByZeroException On division by zero
     * @throws Formula_Exceptions_TypeMismatchException On runtime type errors
     * @throws Formula_Exceptions_PermissionDeniedException On permission check failure
     * @throws Formula_Exceptions_ResourceExhaustedException On resource limit exceeded
     * @throws Formula_Exceptions_RuntimeExecutionException On unexpected runtime errors
     */
    public function execute(
        Formula_Compiler_CompiledFormula $compiled,
        Formula_Context_FormulaContext $context
    ) {
        // Build the variable resolver callable: delegates to NamespaceRegistry
        $namespaceRegistry = new Formula_Registry_NamespaceRegistry($this->variableRegistry);

        $variableResolver = function ($qualifiedName, Formula_Context_FormulaContext $ctx) use ($namespaceRegistry) {
            return $namespaceRegistry->resolve($qualifiedName, $ctx);
        };

        // Build the function executor callable
        $functionExecutor = new Formula_Runtime_FunctionExecutor($this->functionRegistry);

        $fnCallable = function ($functionName, array $args, Formula_Context_FormulaContext $ctx) use ($functionExecutor) {
            return $functionExecutor->execute($functionName, $args, $ctx);
        };

        // Create the tree-walk evaluator
        $evaluator = new Formula_Runtime_NodeEvaluator($variableResolver, $fnCallable);

        // Create a per-evaluation session for resource tracking and memoization
        $session = new Formula_Runtime_RuntimeSession($context);

        // Walk the AST and produce the result
        $startTime = microtime(true);

        try {
            $result = $evaluator->evaluate($compiled->ast, $session);
        } catch (Formula_Exceptions_FormulaException $e) {
            // Re-throw framework exceptions as-is
            throw $e;
        } catch (Exception $e) {
            // Wrap unexpected exceptions
            throw new Formula_Exceptions_RuntimeExecutionException(
                'Unexpected runtime error: ' . $e->getMessage(),
                0,
                0,
                $e
            );
        }

        return $result;
    }

    /**
     * Validate a formula without executing.
     *
     * Runs the full validation pipeline (syntax, semantic, type,
     * dependency checks) via the FormulaCompiler and returns a
     * ValidationResult with all errors and warnings.
     *
     * @param string $formula
     * @return Formula_Compiler_ValidationResult
     */
    public function validate($formula)
    {
        $result = new Formula_Compiler_ValidationResult();

        try {
            $compiler = $this->getCompiler();
            // Compile to trigger validation pipeline
            $compiled = $compiler->compile($formula);
            $result->isValid = true;
            if (!empty($compiled->warnings)) {
                foreach ($compiled->warnings as $warning) {
                    $result->addWarning(
                        isset($warning['message']) ? $warning['message'] : $warning
                    );
                }
            }
        } catch (Formula_Exceptions_FormulaException $e) {
            $result->isValid = false;
            $result->addError(
                $e->getMessage(),
                $e->getSourceLine(),
                $e->getSourceColumn()
            );
        }

        return $result;
    }

    /**
     * Explain formula evaluation step by step.
     *
     * Compiles the formula through the full pipeline, then evaluates
     * it with the ExplainVisitor which records every evaluation step,
     * intermediate value, and timing.
     *
     * The ExplainVisitor wraps the production NodeEvaluator, guaranteeing
     * that the explain trace produces identical results to production
     * evaluation.
     *
     * @param string                        $formula
     * @param Formula_Context_FormulaContext $context
     * @return Formula_Diagnostics_ExplainResult
     */
    public function explain($formula, Formula_Context_FormulaContext $context)
    {
        $compiled = $this->compile($formula);

        // Build the same resolver/executor as execute()
        $namespaceRegistry = new Formula_Registry_NamespaceRegistry($this->variableRegistry);
        $functionExecutor = new Formula_Runtime_FunctionExecutor($this->functionRegistry);

        $variableResolver = function ($qualifiedName, Formula_Context_FormulaContext $ctx) use ($namespaceRegistry) {
            return $namespaceRegistry->resolve($qualifiedName, $ctx);
        };

        $fnCallable = function ($functionName, array $args, Formula_Context_FormulaContext $ctx) use ($functionExecutor) {
            return $functionExecutor->execute($functionName, $args, $ctx);
        };

        // Create production evaluator
        $evaluator = new Formula_Runtime_NodeEvaluator($variableResolver, $fnCallable);

        // Wrap with ExplainVisitor for step-by-step tracing
        $explainVisitor = new Formula_Runtime_ExplainVisitor($evaluator, $formula);

        // Create session and evaluate
        $session = new Formula_Runtime_RuntimeSession($context);

        return $explainVisitor->evaluate($compiled->ast, $session);
    }

    /**
     * Clear all formula caches.
     *
     * @return void
     */
    public function clearCache()
    {
        // Cache layer is initialized lazily. When cache layers are implemented,
        // this will delegate to SourceCache, CompilerCache, and RuntimeCache
        // clear() methods to invalidate all cached entries.
    }

    /**
     * Get the function registry.
     *
     * @return Formula_Registry_FunctionRegistry
     */
    public function getFunctionRegistry()
    {
        return $this->functionRegistry;
    }

    /**
     * Get the variable registry.
     *
     * @return Formula_Registry_VariableRegistry
     */
    public function getVariableRegistry()
    {
        return $this->variableRegistry;
    }

    // -----------------------------------------------------------------------
    //  Private Helpers
    // -----------------------------------------------------------------------

    /**
     * Build formula metadata by walking the AST.
     *
     * Extracts referenced variables, functions, namespaces, node count,
     * depth, and estimated complexity from the parsed AST.
     *
     * @param string                     $formula  The original formula source
     * @param Formula_Compiler_AST_Node  $ast      The parsed AST root
     * @param array                      $warnings Compilation warnings
     * @return Formula_Compiler_FormulaMetadata
     */
    private function buildMetadata($formula, Formula_Compiler_AST_Node $ast, array $warnings)
    {
        $variables  = array();
        $functions  = array();
        $namespaces = array();
        $this->extractReferences($ast, $variables, $functions, $namespaces);

        $nodeCount = $this->countNodes($ast);
        $depth     = $this->calculateDepth($ast);
        $complexity = $this->estimateComplexity($nodeCount, $depth, $functions);

        return new Formula_Compiler_FormulaMetadata(array(
            'sourceChecksum'       => sha1($formula),
            'referencedVariables'  => array_unique($variables),
            'referencedFunctions'  => array_unique($functions),
            'referencedNamespaces' => array_unique($namespaces),
            'astNodeCount'         => $nodeCount,
            'astDepth'             => $depth,
            'estimatedComplexity'  => $complexity,
            'isCacheable'          => empty($warnings),
        ));
    }

    /**
     * Recursively extract variable, function, and namespace references from AST.
     *
     * @param Formula_Compiler_AST_Node $node
     * @param array                     &$variables  Accumulator for variable names
     * @param array                     &$functions  Accumulator for function names
     * @param array                     &$namespaces Accumulator for namespace prefixes
     * @return void
     */
    private function extractReferences(
        Formula_Compiler_AST_Node $node,
        array &$variables,
        array &$functions,
        array &$namespaces
    ) {
        if ($node instanceof Formula_Compiler_AST_VariableNode) {
            $key = $node->getQualifiedName();
            if (!in_array($key, $variables, true)) {
                $variables[] = $key;
            }
            if ($node->namespace !== '' && !in_array($node->namespace, $namespaces, true)) {
                $namespaces[] = $node->namespace;
            }
        }

        if ($node instanceof Formula_Compiler_AST_FunctionNode) {
            if (!in_array($node->functionName, $functions, true)) {
                $functions[] = $node->functionName;
            }
        }

        if ($node instanceof Formula_Compiler_AST_ConditionalNode) {
            // IF() is represented as ConditionalNode, also track as function reference
            if (!in_array('IF', $functions, true)) {
                $functions[] = 'IF';
            }
        }

        foreach ($node->getChildren() as $child) {
            if ($child instanceof Formula_Compiler_AST_Node) {
                $this->extractReferences($child, $variables, $functions, $namespaces);
            }
        }
    }

    /**
     * Count the total number of nodes in the AST.
     *
     * @param Formula_Compiler_AST_Node $node
     * @return int
     */
    private function countNodes(Formula_Compiler_AST_Node $node)
    {
        $count = 1; // count this node
        foreach ($node->getChildren() as $child) {
            if ($child instanceof Formula_Compiler_AST_Node) {
                $count += $this->countNodes($child);
            }
        }
        return $count;
    }

    /**
     * Calculate the maximum nesting depth of the AST.
     *
     * @param Formula_Compiler_AST_Node $node
     * @return int
     */
    private function calculateDepth(Formula_Compiler_AST_Node $node)
    {
        $maxChildDepth = 0;
        foreach ($node->getChildren() as $child) {
            if ($child instanceof Formula_Compiler_AST_Node) {
                $childDepth = $this->calculateDepth($child);
                if ($childDepth > $maxChildDepth) {
                    $maxChildDepth = $childDepth;
                }
            }
        }
        return 1 + $maxChildDepth;
    }

    /**
     * Estimate formula complexity on a scale of 1-100.
     *
     * Complexity factors: node count, nesting depth, function call count.
     *
     * @param int        $nodeCount  Number of AST nodes
     * @param int        $depth      Maximum nesting depth
     * @param string[]   $functions  Referenced function names
     * @return float Complexity score 1-100
     */
    private function estimateComplexity($nodeCount, $depth, array $functions)
    {
        // Base: 1 point per node, 2 points per depth level, 5 points per function call
        $raw = (float)$nodeCount + (float)$depth * 2.0 + count($functions) * 5.0;

        // Normalize to 1-100 scale (logarithmic to handle wide range)
        if ($raw <= 1.0) {
            return 1.0;
        }

        $score = log($raw) * 15.0;
        return max(1.0, min(100.0, $score));
    }
}
