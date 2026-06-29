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

    /**
     * Construct the runtime engine.
     *
     * @param Formula_Registry_FunctionRegistry $functionRegistry
     * @param Formula_Registry_VariableRegistry $variableRegistry
     */
    public function __construct(
        Formula_Registry_FunctionRegistry $functionRegistry,
        Formula_Registry_VariableRegistry $variableRegistry
    ) {
        $this->functionRegistry = $functionRegistry;
        $this->variableRegistry = $variableRegistry;
    }

    /**
     * Compile a formula string into a compiled form.
     *
     * The compilation pipeline:
     * 1. Lex the formula into tokens via Formula_Compiler_Lexer
     * 2. Parse tokens into AST via Formula_Compiler_Parser
     * 3. (Future) Validate AST via validator pipeline
     * 4. (Future) Optimize AST via optimizer pipeline
     * 5. Produce CompiledFormula with metadata
     *
     * @param string $formula The preprocessed formula string
     * @return Formula_Compiler_CompiledFormula The compiled, validated, optimized formula
     * @throws Formula_Exceptions_SyntaxErrorException On lexer/parser errors
     * @throws Formula_Exceptions_UnknownFunctionException On unknown function references
     * @throws Formula_Exceptions_UnknownVariableException On unknown variable references
     * @throws Formula_Exceptions_ResourceExhaustedException On resource limit exceeded
     */
    public function compile($formula)
    {
        $compileStart = microtime(true);

        // Stage 1: Lexical analysis
        $lexer  = new Formula_Compiler_Lexer($formula);
        $tokens = $lexer->tokenize();

        // Stage 2: Parse tokens into AST
        $tokenStream = new Formula_Compiler_TokenStream($tokens);
        $parser      = new Formula_Compiler_Parser(
            $tokenStream,
            $this->functionRegistry,
            $this->variableRegistry
        );
        $ast = $parser->parse();

        // Stage 3-4: Validation and optimization will be added in future sprints.
        // For now, the AST is used as-is.

        // Build metadata from the AST
        $metadata = $this->buildMetadata($formula, $ast, $parser->getWarnings());

        // Compute elapsed time
        $compileTimeMs = (microtime(true) - $compileStart) * 1000.0;

        return new Formula_Compiler_CompiledFormula(
            $ast,
            $metadata,
            sha1($formula),
            $compileTimeMs,
            $parser->getWarnings()
        );
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
     * Walks the AST in post-order (children before parent) and evaluates
     * each node. The tree-walk evaluator (NodeEvaluator) is the component
     * that performs the actual computation.
     *
     * @param Formula_Compiler_CompiledFormula $compiled
     * @param Formula_Context_FormulaContext   $context
     * @return mixed
     * @throws RuntimeException The tree-walk evaluator is not yet implemented
     */
    public function execute(
        Formula_Compiler_CompiledFormula $compiled,
        Formula_Context_FormulaContext $context
    ) {
        // The tree-walk evaluator (Runtime/NodeEvaluator.php) is the next
        // component to be implemented. It traverses the AST and evaluates
        // each node type. Until then, compilation works end-to-end but
        // execution throws this controlled exception.
        throw new RuntimeException(
            'Formula execution is not yet available. '
            . 'The tree-walk AST evaluator (NodeEvaluator) is the next component '
            . 'to be implemented. Compilation works — formulas can be parsed '
            . 'into ASTs and validated.'
        );
    }

    /**
     * Validate a formula without executing.
     *
     * Compiles the formula to check for syntax and semantic errors.
     * Returns a ValidationResult with errors and warnings.
     *
     * @param string $formula
     * @return Formula_Compiler_ValidationResult
     */
    public function validate($formula)
    {
        $result = new Formula_Compiler_ValidationResult();

        try {
            $lexer  = new Formula_Compiler_Lexer($formula);
            $tokens = $lexer->tokenize();

            $tokenStream = new Formula_Compiler_TokenStream($tokens);
            $parser      = new Formula_Compiler_Parser(
                $tokenStream,
                $this->functionRegistry,
                $this->variableRegistry
            );
            $ast = $parser->parse();

            $warnings = $parser->getWarnings();
            if (!empty($warnings)) {
                foreach ($warnings as $warning) {
                    $result->addWarning($warning);
                }
            }

            $result->isValid = true;
            $result->astNodeCount = $this->countNodes($ast);
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
     * Compiles the formula and produces an explanation of the AST structure.
     * Full step-by-step evaluation trace requires the ExplainVisitor.
     *
     * @param string                        $formula
     * @param Formula_Context_FormulaContext $context
     * @return Formula_Diagnostics_ExplainResult
     */
    public function explain($formula, Formula_Context_FormulaContext $context)
    {
        $compiled = $this->compile($formula);
        $explain  = new Formula_Diagnostics_ExplainResult();
        $explain->result        = null;
        $explain->formulaSource = $formula;
        $explain->durationMs    = $compiled->compileTimeMs;
        $explain->nodesEvaluated = $compiled->metadata->astNodeCount;
        $explain->steps          = array(
            array(
                'step'       => 1,
                'operation'  => 'compile',
                'input'      => $formula,
                'output'     => 'AST with ' . $compiled->metadata->astNodeCount . ' nodes',
                'durationMs' => $compiled->compileTimeMs,
                'nodeType'   => get_class($compiled->ast),
            ),
        );
        return $explain;
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
