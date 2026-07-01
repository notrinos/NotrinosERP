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
 * FormulaFacade — Single public API entry point for the Formula Framework.
 *
 * Every business module (HRM, sales, inventory, manufacturing, GL, CRM,
 * reports) executes formulas through this facade. No module shall implement
 * its own parser, evaluator, or variable resolver.
 *
 * This is a STATIC facade — modules call FormulaFacade::evaluate(...),
 * never instantiate. The singleton engine is lazy-initialized during
 * bootstrap and frozen before any formula evaluation.
 *
 * ## Usage Examples
 *
 * ### Payroll (legacy migration)
 *   $context = FormulaContextBuilder::create()
 *       ->withVariables(array('BASIC' => 8000, 'GROSS' => 8000))
 *       ->withCompatibilityMode(true)
 *       ->build();
 *   $amount = FormulaFacade::evaluate($component['formula'], $context);
 *
 * ### Sales Pricelist (formula computation type)
 *   $context = FormulaContextBuilder::create()
 *       ->withStockItem($stock_id)
 *       ->build();
 *   $price = FormulaFacade::evaluate($rule['formula_text'], $context);
 *
 * ### Batch Payroll (compile once, evaluate many)
 *   $compiled = FormulaFacade::compile($salaryFormula);
 *   $results = FormulaFacade::evaluateBatch($compiled, $employeeContexts);
 *
 * @package Formula
 * @since   2.0.0
 */
class FormulaFacade
{
    /**
     * Singleton Formula Engine instance.
     *
     * Lazy-initialized during bootstrap. Null until the framework
     * bootstrap process initializes it with registries and services.
     *
     * @var Formula_Runtime_FormulaRuntime|null
     */
    private static $engine = null;

    /**
     * Function registry (available before engine initialization for
     * extension registration during bootstrap).
     *
     * @var Formula_Registry_FunctionRegistry|null
     */
    private static $functionRegistry = null;

    /**
     * Variable registry (available before engine initialization for
     * extension registration during bootstrap).
     *
     * @var Formula_Registry_VariableRegistry|null
     */
    private static $variableRegistry = null;

    /**
     * Whether the framework has been fully bootstrapped.
     *
     * @var bool
     */
    private static $initialized = false;

    /**
     * Whether the registries have been frozen (read-only).
     *
     * @var bool
     */
    private static $frozen = false;

    // -----------------------------------------------------------------------
    // Bootstrap & Initialization
    // -----------------------------------------------------------------------

    /**
     * Initialize the Formula Framework with registries.
     *
     * Called during session bootstrap (from includes/formula/bootstrap.inc)
     * after the hook system is available but before any module pages render.
     *
     * The initialization sequence:
     * 1. Store function and variable registries
     * 2. Register 150+ built-in functions
     * 3. Fire hook_invoke_all('formula_register_providers') for extensions
     * 4. Freeze registries (read-only)
     * 5. Mark framework as initialized
     *
     * @param Formula_Registry_FunctionRegistry $functionRegistry
     * @param Formula_Registry_VariableRegistry $variableRegistry
     * @return void
     */
    public static function initialize(
        Formula_Registry_FunctionRegistry $functionRegistry,
        Formula_Registry_VariableRegistry $variableRegistry
    ) {
        self::$functionRegistry = $functionRegistry;
        self::$variableRegistry = $variableRegistry;
        self::$initialized = true;
    }

    /**
     * Freeze the registries after all extensions have registered.
     *
     * After freeze, no further function or variable registrations are allowed.
     * This is called after hook_invoke_all('formula_register_providers').
     *
     * @return void
     */
    public static function freeze()
    {
        if (self::$functionRegistry !== null) {
            self::$functionRegistry->freeze();
        }
        if (self::$variableRegistry !== null) {
            self::$variableRegistry->freeze();
        }
        self::$frozen = true;
    }

    /**
     * Check whether the framework has been initialized.
     *
     * @return bool
     */
    public static function isInitialized()
    {
        return self::$initialized;
    }

    /**
     * Check whether registries are frozen.
     *
     * @return bool
     */
    public static function isFrozen()
    {
        return self::$frozen;
    }

    /**
     * Inject a Formula Engine instance (for testing or custom configurations).
     *
     * Normally the engine is lazy-initialized during bootstrap.
     * This method allows injection of a pre-configured engine.
     *
     * @param Formula_Runtime_FormulaRuntime $engine
     * @return void
     */
    public static function setEngine(Formula_Runtime_FormulaRuntime $engine)
    {
        self::$engine = $engine;
    }

    // -----------------------------------------------------------------------
    // Compilation
    // -----------------------------------------------------------------------

    /**
     * Compile a formula string into an optimized, cacheable form.
     *
     * The compilation process: lexer → parser → validation → optimization → cache.
     * The returned CompiledFormula can be evaluated multiple times with
     * different contexts without re-parsing.
     *
     * Use this method when evaluating the same formula multiple times
     * (e.g., batch payroll processing for 1000 employees). For single
     * evaluations, use evaluate() which compiles and executes in one call.
     *
     * @param string $formula The raw formula source string
     * @return Formula_Compiler_CompiledFormula The compiled, optimized formula
     * @throws Formula_Exceptions_SyntaxErrorException On lexer/parser errors
     * @throws Formula_Exceptions_UnknownFunctionException On unknown function references
     * @throws Formula_Exceptions_UnknownVariableException On unknown variable references
     * @throws Formula_Exceptions_TypeMismatchException On type incompatibility
     * @throws Formula_Exceptions_CircularReferenceException On circular dependencies
     * @throws Formula_Exceptions_ResourceExhaustedException On resource limit exceeded
     * @throws RuntimeException If the framework has not been initialized
     */
    public static function compile($formula)
    {
        self::ensureInitialized();

        $formula = self::preprocessFormula($formula);

        if ($formula === '') {
            return self::createEmptyCompiledFormula('');
        }

        return self::getEngine()->compile($formula);
    }

    // -----------------------------------------------------------------------
    // Evaluation
    // -----------------------------------------------------------------------

    /**
     * Compile and evaluate a formula in a single call.
     *
     * This is the primary method for one-off formula evaluations.
     * For repeated evaluations of the same formula, use compile()
     * followed by evaluateBatch() for better performance.
     *
     * @param string                            $formula The formula to evaluate
     * @param Formula_Context_FormulaContext     $context The immutable execution context
     * @return mixed The evaluation result
     * @throws Formula_Exceptions_FormulaException On compilation or execution errors
     * @throws RuntimeException If the framework has not been initialized
     */
    public static function evaluate($formula, Formula_Context_FormulaContext $context)
    {
        self::ensureInitialized();

        $formula = self::preprocessFormula($formula);

        if ($formula === '') {
            return 0.0;
        }

        return self::getEngine()->evaluate($formula, $context);
    }

    /**
     * Evaluate a compiled formula against multiple contexts.
     *
     * Compile once, evaluate many times. Essential for batch processing:
     * payroll for 1000 employees, report generation for 500 rows, etc.
     *
     * @param Formula_Compiler_CompiledFormula    $formula  The compiled formula
     * @param Formula_Context_FormulaContext[]     $contexts Array of contexts
     * @return array Results in the same order as contexts
     * @throws Formula_Exceptions_FormulaException On execution errors
     * @throws RuntimeException If the framework has not been initialized
     */
    public static function evaluateBatch(
        Formula_Compiler_CompiledFormula $formula,
        array $contexts
    ) {
        self::ensureInitialized();

        $results = array();
        foreach ($contexts as $context) {
            $results[] = self::getEngine()->execute($formula, $context);
        }
        return $results;
    }

    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------

    /**
     * Validate a formula without executing it.
     *
     * Runs the full validation pipeline (syntax, semantic, type, dependency
     * checks) and returns a ValidationResult with all errors and warnings.
     * The formula is not executed.
     *
     * Use this in formula editors, admin panels, and any user-facing
     * formula input to provide real-time feedback.
     *
     * @param string $formula The formula to validate
     * @return Formula_Compiler_ValidationResult
     * @throws RuntimeException If the framework has not been initialized
     */
    public static function validate($formula)
    {
        self::ensureInitialized();

        $formula = self::preprocessFormula($formula);

        if ($formula === '') {
            $result = new Formula_Compiler_ValidationResult();
            return $result;
        }

        return self::getEngine()->validate($formula);
    }

    // -----------------------------------------------------------------------
    // Explain Mode (Step-by-Step Evaluation Trace)
    // -----------------------------------------------------------------------

    /**
     * Evaluate a formula with step-by-step explanation.
     *
     * Produces an ExplainResult containing every intermediate evaluation
     * step, variable resolution, and function call. Used for debugging
     * formulas, audit trails, and the future formula debugger (v2.x).
     *
     * @param string                            $formula The formula to explain
     * @param Formula_Context_FormulaContext     $context The execution context
     * @return Formula_Diagnostics_ExplainResult
     * @throws Formula_Exceptions_FormulaException On compilation or execution errors
     * @throws RuntimeException If the framework has not been initialized
     */
    public static function explain($formula, Formula_Context_FormulaContext $context)
    {
        self::ensureInitialized();

        $formula = self::preprocessFormula($formula);

        if ($formula === '') {
            return new Formula_Diagnostics_ExplainResult(
                0.0,
                array(),
                0.0,
                0,
                0,
                0,
                ''
            );
        }

        return self::getEngine()->explain($formula, $context);
    }

    // -----------------------------------------------------------------------
    // Extension Registration
    // -----------------------------------------------------------------------

    /**
     * Register a custom function implementation.
     *
     * Called by extensions during bootstrap (via FormulaServiceProvider).
     * Must be called before registries are frozen.
     *
     * @param Formula_Contracts_FormulaFunctionInterface $function
     * @return void
     * @throws RuntimeException If registries are frozen
     * @throws RuntimeException If a function with the same name already exists
     */
    public static function registerFunction(Formula_Contracts_FormulaFunctionInterface $function)
    {
        if (self::$functionRegistry === null) {
            throw new RuntimeException(
                'Formula Framework: Cannot register function "' . $function->getName()
                . '" — framework has not been initialized.'
            );
        }

        if (self::$frozen) {
            throw new RuntimeException(
                'Formula Framework: Cannot register function "' . $function->getName()
                . '" — registries are frozen. Register during bootstrap.'
            );
        }

        self::$functionRegistry->register($function);
    }

    /**
     * Register a variable provider for a namespace.
     *
     * Called by extensions during bootstrap (via FormulaServiceProvider).
     * Must be called before registries are frozen.
     *
     * @param string                                       $namespace The namespace to register
     * @param Formula_Contracts_VariableProviderInterface   $provider  The provider instance
     * @return void
     * @throws RuntimeException If registries are frozen
     * @throws RuntimeException If the namespace is already registered
     */
    public static function registerVariableProvider(
        $namespace,
        Formula_Contracts_VariableProviderInterface $provider
    ) {
        if (self::$variableRegistry === null) {
            throw new RuntimeException(
                'Formula Framework: Cannot register variable provider for namespace "' . $namespace
                . '" — framework has not been initialized.'
            );
        }

        if (self::$frozen) {
            throw new RuntimeException(
                'Formula Framework: Cannot register variable provider for namespace "' . $namespace
                . '" — registries are frozen. Register during bootstrap.'
            );
        }

        self::$variableRegistry->register($namespace, $provider);
    }

    // -----------------------------------------------------------------------
    // Cache Management
    // -----------------------------------------------------------------------

    /**
     * Clear all formula caches.
     *
     * Invalidates the source cache, compiler cache, and runtime cache.
     * Used during deployment, after compiler/language version bumps,
     * or when formula source data changes.
     *
     * @return void
     */
    public static function clearCache()
    {
        if (self::$engine !== null) {
            self::$engine->clearCache();
        }
    }

    // -----------------------------------------------------------------------
    // Framework Information
    // -----------------------------------------------------------------------

    /**
     * Get framework version information.
     *
     * Returns an associative array with version numbers for the framework,
     * language specification, and compiler.
     *
     * @return array Associative array with keys: framework, language, compiler
     */
    public static function version()
    {
        return array(
            'framework' => FORMULA_VERSION,
            'language'  => FORMULA_LANGUAGE_VERSION,
            'compiler'  => FORMULA_COMPILER_VERSION,
        );
    }

    // -----------------------------------------------------------------------
    // Internal Helpers
    // -----------------------------------------------------------------------

    /**
     * Ensure the framework has been initialized before any operation.
     *
     * @return void
     * @throws RuntimeException If the framework has not been initialized
     */
    private static function ensureInitialized()
    {
        if (!self::$initialized) {
            throw new RuntimeException(
                'Formula Framework has not been initialized. '
                . 'Ensure includes/formula/bootstrap.inc is loaded during session bootstrap.'
            );
        }
    }

    /**
     * Get or create the singleton formula engine.
     *
     * Creates the engine on first call with the registered function
     * and variable registries.
     *
     * @return Formula_Runtime_FormulaRuntime
     */
    private static function getEngine()
    {
        if (self::$engine === null) {
            if (self::$functionRegistry === null || self::$variableRegistry === null) {
                throw new RuntimeException(
                    'Formula Framework: Cannot create engine — registries not configured.'
                );
            }

            self::$engine = new Formula_Runtime_FormulaRuntime(
                self::$functionRegistry,
                self::$variableRegistry
            );
        }

        return self::$engine;
    }

    /**
     * Preprocess a formula string before compilation.
     *
     * Performs normalization that matches the existing payroll_formula_engine
     * behavior: trimming whitespace, stripping a leading '=' (Excel convention),
     * and removing BOM characters.
     *
     * @param string $formula The raw formula string
     * @return string The preprocessed formula
     */
    private static function preprocessFormula($formula)
    {
        $formula = trim((string)$formula);

        // Strip BOM if present
        if (strncmp($formula, "\xEF\xBB\xBF", 3) === 0) {
            $formula = substr($formula, 3);
        }

        // Strip leading '=' (Excel convention)
        if (isset($formula[0]) && $formula[0] === '=') {
            $formula = substr($formula, 1);
        }

        $formula = trim($formula);

        // Reject formulas exceeding maximum length (security limit)
        if (strlen($formula) > FORMULA_MAX_SOURCE_LENGTH) {
            throw new Formula_Exceptions_ResourceExhaustedException(
                sprintf(
                    'Formula length (%d characters) exceeds maximum allowed length (%d characters).',
                    strlen($formula),
                    FORMULA_MAX_SOURCE_LENGTH
                ),
                'source_length',
                FORMULA_MAX_SOURCE_LENGTH,
                0,
                0
            );
        }

        return $formula;
    }

    /**
     * Create an empty compiled formula (for empty formula input).
     *
     * An empty formula is valid and evaluates to 0.0, matching the
     * behavior of the legacy payroll_formula_engine.
     *
     * @param string $source The original (empty) formula source
     * @return Formula_Compiler_CompiledFormula
     */
    private static function createEmptyCompiledFormula($source)
    {
        $checksum = sha1($source);

        $metadata = new Formula_Compiler_FormulaMetadata(array(
            'sourceChecksum'      => $checksum,
            'referencedVariables'  => array(),
            'referencedFunctions'  => array(),
            'referencedNamespaces' => array(),
            'astNodeCount'         => 0,
            'astDepth'             => 0,
            'estimatedComplexity'  => 0.0,
            'isCacheable'          => true,
            'compileTimeMs'        => 0.0,
        ));

        return new Formula_Compiler_CompiledFormula(
            new Formula_Compiler_AST_NullNode(),
            $metadata,
            $checksum,
            0.0
        );
    }
}
