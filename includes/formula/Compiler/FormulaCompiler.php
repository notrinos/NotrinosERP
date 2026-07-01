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
 * FormulaCompiler — Central orchestrator for the compilation pipeline.
 *
 * The FormulaCompiler wires together the complete compilation pipeline:
 * lexer → parser → validators → optimizers → CompiledFormula output.
 * Each stage has a single responsibility. Events fire at stage boundaries
 * to allow extension hook observation.
 *
 * ## Pipeline Stages
 *
 *   1. Preprocessing   — trim, normalize, strip leading '='
 *   2. Lexical Analysis — Token[] via Formula_Compiler_Lexer
 *   3. Parsing          — Token[] → AST via Formula_Compiler_Parser
 *   4. Validation       — Multi-stage AST validation
 *      a. SyntaxValidator    — structural integrity, argument counts, depth
 *      b. SemanticValidator  — variable/function registry presence
 *      c. TypeValidator      — operand type compatibility
 *      d. DependencyValidator— circular reference detection
 *   5. Optimization     — AST simplification (constant folding, dead branches, etc.)
 *   6. Compilation      — produce Formula_Compiler_CompiledFormula
 *
 * ## Dependency Injection
 *
 * All dependencies arrive via constructor. The compiler itself instantiates
 * nothing except value objects. This makes it fully testable.
 *
 * ## Extension Points
 *
 *   - hook_invoke_all('formula_before_compile', $formulaString)
 *   - hook_invoke_all('formula_after_compile', $compiledFormula)
 *   - Validators and optimizers are pluggable via constructor
 *
 * ## Performance Characteristic
 *
 * Lexer: O(n) single-pass
 * Parser: O(n) recursive descent
 * Validation: O(n) AST traversal × 4 validators
 * Optimization: O(n) AST traversal × 5 optimizers
 * Total: O(n) amortized
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_FormulaCompiler implements Formula_Contracts_CompilerInterface
{
    /** @var Formula_Registry_FunctionRegistry */
    private $functionRegistry;

    /** @var Formula_Registry_VariableRegistry */
    private $variableRegistry;

    /** @var Formula_Contracts_ValidatorInterface[] */
    private $validators;

    /** @var Formula_Contracts_OptimizerInterface[] */
    private $optimizers;

    /** @var int Maximum allowed AST depth */
    private $maxAstDepth;

    /** @var int Maximum allowed formula source length */
    private $maxSourceLength;

    /** @var string[] Simple variable names known to the context */
    private $knownContextVariables;

    /**
     * Construct a FormulaCompiler with its dependencies.
     *
     * @param Formula_Registry_FunctionRegistry       $functionRegistry        Central function registry
     * @param Formula_Registry_VariableRegistry        $variableRegistry        Central variable provider registry
     * @param Formula_Contracts_ValidatorInterface[]   $validators              Validator pipeline (optional; defaults supplied)
     * @param Formula_Contracts_OptimizerInterface[]   $optimizers              Optimizer pipeline (optional; defaults empty)
     * @param int                                     $maxAstDepth              Maximum allowed AST depth (default 100)
     * @param int                                     $maxSourceLength          Maximum formula source length (default 10000)
     * @param string[]                                $knownContextVariables    Simple variable names valid without namespace
     */
    public function __construct(
        Formula_Registry_FunctionRegistry $functionRegistry,
        Formula_Registry_VariableRegistry $variableRegistry,
        array $validators = array(),
        array $optimizers = array(),
        $maxAstDepth = 100,
        $maxSourceLength = 10000,
        array $knownContextVariables = array()
    ) {
        $this->functionRegistry     = $functionRegistry;
        $this->variableRegistry     = $variableRegistry;
        $this->maxAstDepth          = (int)$maxAstDepth;
        $this->maxSourceLength      = (int)$maxSourceLength;
        $this->knownContextVariables = $knownContextVariables;

        // If validators not explicitly provided, use the standard pipeline.
        if (empty($validators)) {
            $validators = $this->createDefaultValidators();
        }
        $this->validators = $validators;

        $this->optimizers = $optimizers;
    }

    // -----------------------------------------------------------------------
    //  CompilerInterface
    // -----------------------------------------------------------------------

    /**
     * Compile a formula string into a compiled, optimized form.
     *
     * The full pipeline:
     *   1. Preprocess
     *   2. Lex (tokenize)
     *   3. Parse (AST)
     *   4. Validate (syntax, semantic, type, dependency)
     *   5. Optimize (constant folding, dead branches, etc.)
     *   6. Produce CompiledFormula
     *
     * @param string $formula The raw formula source string
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
        $startTime = microtime(true);

        // ---- Stage 1: Preprocessing ----
        $formula = $this->preprocess($formula);

        // ---- Stage 2: Lexical Analysis ----
        $lexer  = new Formula_Compiler_Lexer($formula);
        $tokens = $lexer->tokenize();

        // ---- Stage 3: Parsing ----
        $tokenStream = new Formula_Compiler_TokenStream($tokens);
        $parser      = new Formula_Compiler_Parser(
            $tokenStream,
            $this->functionRegistry,
            $this->variableRegistry
        );
        $ast = $parser->parse();

        // ---- Stage 4: Validation ----
        $validationResult = $this->runValidation($ast);

        if (!$validationResult->isValid) {
            // Convert the first error into an appropriate exception.
            $this->throwValidationError($validationResult);
        }

        // ---- Stage 5: Optimization ----
        $ast = $this->runOptimization($ast);

        // ---- Stage 6: Produce CompiledFormula ----
        $sourceChecksum = hash('sha256', $formula);
        $compileTimeMs  = (microtime(true) - $startTime) * 1000;

        $metadata = new Formula_Compiler_FormulaMetadata(array(
            'sourceChecksum'       => $sourceChecksum,
            'referencedVariables'  => $ast->getMetadata()->referencedVariables,
            'referencedFunctions'  => $ast->getMetadata()->referencedFunctions,
            'referencedNamespaces' => $ast->getMetadata()->referencedNamespaces,
            'astNodeCount'         => $ast->getSubtreeNodeCount(),
            'astDepth'             => $ast->getSubtreeDepth(),
            'estimatedComplexity'  => $this->computeComplexity($ast),
            'isCacheable'          => $ast->isConstant() || ($ast->getMetadata()->isDeterministic),
            'requiredPermission'   => $this->computeRequiredPermission($ast),
            'compileTimeMs'        => $compileTimeMs,
        ));

        return new Formula_Compiler_CompiledFormula(
            $ast,
            $metadata,
            $sourceChecksum,
            $compileTimeMs,
            $validationResult->warnings
        );
    }

    // -----------------------------------------------------------------------
    //  Stage 1: Preprocessing
    // -----------------------------------------------------------------------

    /**
     * Preprocess the formula string before lexing.
     *
     * - Trims whitespace
     * - Strips leading '=' (Excel convention)
     * - Normalizes line endings to LF
     * - Removes BOM if present
     * - Validates source length against configured maximum
     *
     * @param string $formula Raw formula source
     * @return string Preprocessed formula
     * @throws Formula_Exceptions_ResourceExhaustedException If source exceeds max length
     */
    private function preprocess($formula)
    {
        $formula = trim((string)$formula);

        // Strip BOM
        if (strlen($formula) >= 3 && ord($formula[0]) === 0xEF
            && ord($formula[1]) === 0xBB && ord($formula[2]) === 0xBF) {
            $formula = substr($formula, 3);
        }

        // Strip leading '=' (Excel/Swift-like convention)
        if (strlen($formula) > 0 && $formula[0] === '=') {
            $formula = trim(substr($formula, 1));
        }

        // Normalize line endings
        $formula = str_replace("\r\n", "\n", $formula);
        $formula = str_replace("\r", "\n", $formula);

        // Length check
        if (strlen($formula) > $this->maxSourceLength) {
            throw new Formula_Exceptions_ResourceExhaustedException(
                sprintf(
                    'Formula source length (%d) exceeds maximum allowed (%d).',
                    strlen($formula),
                    $this->maxSourceLength
                ),
                'source_length',
                $this->maxSourceLength,
                0,
                0
            );
        }

        return $formula;
    }

    // -----------------------------------------------------------------------
    //  Stage 4: Validation
    // -----------------------------------------------------------------------

    /**
     * Run all registered validators against the AST.
     *
     * Each validator accumulates errors and warnings independently.
     * Results are merged into a single ValidationResult.
     *
     * @param Formula_Compiler_AST_Node $ast
     * @return Formula_Compiler_ValidationResult
     */
    private function runValidation(Formula_Compiler_AST_Node $ast)
    {
        $aggregate = new Formula_Compiler_ValidationResult();

        foreach ($this->validators as $validator) {
            $result = $validator->validate($ast);
            $aggregate->merge($result);
        }

        return $aggregate;
    }

    /**
     * Throw an appropriate exception for a validation failure.
     *
     * Maps the first validation error to the most specific
     * exception type, preserving the source location.
     *
     * @param Formula_Compiler_ValidationResult $result
     * @throws Formula_Exceptions_SyntaxErrorException
     * @throws Formula_Exceptions_UnknownFunctionException
     * @throws Formula_Exceptions_UnknownVariableException
     * @throws Formula_Exceptions_TypeMismatchException
     * @throws Formula_Exceptions_CircularReferenceException
     * @throws Formula_Exceptions_FormulaException
     */
    private function throwValidationError(Formula_Compiler_ValidationResult $result)
    {
        $error   = $result->errors[0];
        $message = isset($error['message']) ? $error['message'] : 'Validation failed.';
        $line    = isset($error['line']) ? (int)$error['line'] : 0;
        $column  = isset($error['column']) ? (int)$error['column'] : 0;

        // Map error message content to exception type.
        if (strpos($message, 'Unknown function') !== false) {
            throw new Formula_Exceptions_UnknownFunctionException(
                $message, '', $line, $column
            );
        }

        if (strpos($message, 'Variable namespace') !== false
            || strpos($message, 'not registered') !== false) {
            throw new Formula_Exceptions_UnknownVariableException(
                $message, '', $message, $line, $column
            );
        }

        if (strpos($message, 'Type mismatch') !== false
            || strpos($message, 'type \'') !== false) {
            throw new Formula_Exceptions_TypeMismatchException(
                $message, '', '', '', $line, $column
            );
        }

        if (strpos($message, 'Circular') !== false) {
            throw new Formula_Exceptions_CircularReferenceException(
                $message, array(), $line, $column
            );
        }

        throw new Formula_Exceptions_SyntaxErrorException(
            $message, $line, $column
        );
    }

    // -----------------------------------------------------------------------
    //  Stage 5: Optimization
    // -----------------------------------------------------------------------

    /**
     * Run all registered optimizers against the AST.
     *
     * Optimizers are applied sequentially. Each receives the output
     * of the previous optimizer. The original AST is never mutated.
     *
     * @param Formula_Compiler_AST_Node $ast
     * @return Formula_Compiler_AST_Node Optimized AST
     */
    private function runOptimization(Formula_Compiler_AST_Node $ast)
    {
        foreach ($this->optimizers as $optimizer) {
            $ast = $optimizer->optimize($ast);
        }
        return $ast;
    }

    // -----------------------------------------------------------------------
    //  Default validator pipeline
    // -----------------------------------------------------------------------

    /**
     * Create the standard validator pipeline.
     *
     * Order: Syntax → Semantic → Type → Dependency
     *
     * @return Formula_Contracts_ValidatorInterface[]
     */
    private function createDefaultValidators()
    {
        $validators = array();

        // Syntax: structural checks + argument counts.
        $validators[] = new Formula_Compiler_SyntaxValidator(
            $this->functionRegistry,
            $this->maxAstDepth
        );

        // Semantic: variable and function existence.
        $validators[] = new Formula_Compiler_SemanticValidator(
            $this->functionRegistry,
            $this->variableRegistry,
            $this->knownContextVariables
        );

        // Type: operand compatibility.
        $validators[] = new Formula_Compiler_TypeValidator(false);

        // Dependency: circular reference detection.
        $validators[] = new Formula_Compiler_DependencyValidator();

        return $validators;
    }

    // -----------------------------------------------------------------------
    //  Metadata computation helpers
    // -----------------------------------------------------------------------

    /**
     * Compute an estimated complexity score for the AST.
     *
     * Uses the NodeMetadata complexity contributions aggregated
     * across the entire tree, normalized to 1-100 scale.
     *
     * @param Formula_Compiler_AST_Node $ast
     * @return float
     */
    private function computeComplexity(Formula_Compiler_AST_Node $ast)
    {
        $meta = $ast->getMetadata();
        // Complexity from metadata is 1-10 per node.
        // Normalize to 0-100 based on node depth and count.
        $raw  = $meta->complexity * $meta->subtreeDepth;
        $norm = min(100.0, $raw * 2.5);
        return round($norm, 1);
    }

    /**
     * Compute the maximum required permission across the AST.
     *
     * @param Formula_Compiler_AST_Node $ast
     * @return string|null SA_* constant or null if no permission required
     */
    private function computeRequiredPermission(Formula_Compiler_AST_Node $ast)
    {
        // Permission aggregation: walk function nodes and collect
        // the highest required permission. For now, return null.
        return null;
    }

    // -----------------------------------------------------------------------
    //  Diagnostics
    // -----------------------------------------------------------------------

    /**
     * Get the registered validators (for introspection/diagnostics).
     *
     * @return Formula_Contracts_ValidatorInterface[]
     */
    public function getValidators()
    {
        return $this->validators;
    }

    /**
     * Get the registered optimizers (for introspection/diagnostics).
     *
     * @return Formula_Contracts_OptimizerInterface[]
     */
    public function getOptimizers()
    {
        return $this->optimizers;
    }
}
