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
 * NotrinosERP Centralized Formula Framework — Bootstrap File
 *
 * This file registers the autoloader for the Formula Framework and
 * defines all framework-wide constants. It must be included during
 * session bootstrap (after session.inc loads the hook system).
 *
 * The autoloader maps the Formula_ prefix to directories under
 * includes/formula/, converting underscores to directory separators.
 *
 * Class: Formula_Compiler_Lexer → includes/formula/Compiler/Lexer.php
 * Class: Formula_Runtime_FormulaRuntime → includes/formula/Runtime/FormulaRuntime.php
 *
 * @package Formula
 * @version 1.0.0
 * @since   2.0.0
 */

defined('FORMULA_BOOTSTRAP_LOADED') || define('FORMULA_BOOTSTRAP_LOADED', true);

// ---------------------------------------------------------------------------
// Framework version constants
// ---------------------------------------------------------------------------

/**
 * Formula Framework version.
 * Uses Semantic Versioning: MAJOR.MINOR.PATCH
 */
define('FORMULA_VERSION', '1.0.0');

/**
 * Formula language specification version.
 * Incremented when the NFX grammar changes.
 */
define('FORMULA_LANGUAGE_VERSION', '1.0');

/**
 * Compiler version. Incremented when the compiler produces
 * structurally different compiled output. Triggers cache
 * invalidation for all cached formulas.
 */
define('FORMULA_COMPILER_VERSION', '1.0.0');

// ---------------------------------------------------------------------------
// Security and resource limit constants
// ---------------------------------------------------------------------------

/**
 * Maximum formula source length in characters.
 * Formulas longer than this are rejected before lexing.
 *
 * @see Formula_Exceptions_ResourceExhaustedException
 */
define('FORMULA_MAX_SOURCE_LENGTH', 10000);

/**
 * Maximum AST depth (nesting level of expressions).
 * Prevents stack overflow in recursive descent parser
 * and tree-walk evaluator.
 *
 * @see Formula_Exceptions_ResourceExhaustedException
 */
define('FORMULA_MAX_AST_DEPTH', 100);

/**
 * Maximum number of AST node evaluations per execution session.
 * Prevents infinite evaluation in degenerate cases.
 */
define('FORMULA_MAX_NODE_EVALUATIONS', 10000);

/**
 * Maximum execution time per formula evaluation in milliseconds.
 */
define('FORMULA_MAX_EXECUTION_TIME_MS', 5000);

/**
 * Maximum memory consumption per formula evaluation in bytes.
 */
define('FORMULA_MAX_MEMORY_BYTES', 16 * 1024 * 1024);

/**
 * Maximum number of elements in a collection aggregate function.
 */
define('FORMULA_MAX_COLLECTION_SIZE', 10000);

/**
 * Default number of decimal places for financial rounding.
 */
define('FORMULA_DEFAULT_PRECISION', 6);

// ---------------------------------------------------------------------------
// Autoloader registration
// ---------------------------------------------------------------------------

/**
 * Formula Framework autoloader.
 *
 * Converts a Formula_-prefixed class name into a file path by:
 * 1. Stripping the Formula_ prefix
 * 2. Replacing remaining underscores with DIRECTORY_SEPARATOR
 * 3. Appending .php
 * 4. Looking in includes/formula/
 *
 * @param string $class_name The fully qualified class name
 * @return void
 */
function formula_autoload($class_name)
{
    // Handle FormulaFacade (no underscore separator)
    if ($class_name === 'FormulaFacade') {
        global $path_to_root;
        $absolute_path = $path_to_root . '/includes/formula/FormulaFacade.php';
        if (file_exists($absolute_path)) {
            require_once $absolute_path;
        }
        return;
    }

    // Only handle Formula_ prefixed classes
    $prefix = 'Formula_';
    if (strncmp($class_name, $prefix, strlen($prefix)) !== 0) {
        return;
    }

    // Strip prefix and prepare relative path
    $relative_class = substr($class_name, strlen($prefix));
    $file_path = str_replace('_', DIRECTORY_SEPARATOR, $relative_class) . '.php';

    // Resolve to absolute path within includes/formula/
    global $path_to_root;
    $absolute_path = $path_to_root . '/includes/formula/' . $file_path;

    if (file_exists($absolute_path)) {
        require_once $absolute_path;
    }
}

// Register the autoloader
spl_autoload_register('formula_autoload');

// ---------------------------------------------------------------------------
// Backward compatibility — rename old directory if it exists
// ---------------------------------------------------------------------------

/**
 * The framework was originally planned in includes/formula_engine/.
 * If that directory still exists (from older installs), issue a
 * deprecation notice but do NOT break the application.
 */
if (is_dir(dirname(__FILE__) . '/../formula_engine')) {
    error_log(
        'NotrinosERP Formula Framework: Deprecated directory "includes/formula_engine/" '
        . 'still exists. Framework code now resides in "includes/formula/". '
        . 'The old directory can be safely removed.'
    );
}

// ---------------------------------------------------------------------------
// Framework initialization
// ---------------------------------------------------------------------------

/**
 * Initialize the Centralized Formula Framework.
 *
 * Performs the full bootstrap sequence:
 * 1. Create empty FunctionRegistry and VariableRegistry
 * 2. Create the FormulaRuntime engine (serves as singleton container)
 * 3. Fire hook_invoke_all('formula_register_providers') for extensions
 * 4. Freeze registries — no further registrations allowed
 * 5. Register engine with FormulaFacade
 *
 * This function MUST be called once during session bootstrap, after
 * the hook system (includes/hooks.inc) is available.
 *
 * @param string[] $knownContextVariables Simple variable names known to the calling context
 * @param array    $variableProviders     Namespace => VariableProviderInterface map
 * @return void
 */
function formula_initialize_framework(
    array $knownContextVariables = array(),
    array $variableProviders = array()
) {
    $functionRegistry = new Formula_Registry_FunctionRegistry();
    $variableRegistry = new Formula_Registry_VariableRegistry();

    foreach ($variableProviders as $namespace => $provider) {
        $variableRegistry->register($namespace, $provider);
    }

    formula_register_builtin_functions($functionRegistry);
    formula_register_builtin_variable_providers($variableRegistry);

    $runtime = new Formula_Runtime_FormulaRuntime(
        $functionRegistry,
        $variableRegistry,
        $knownContextVariables
    );

    if (function_exists('hook_invoke_all')) {
        hook_invoke_all('formula_register_providers', $functionRegistry, $variableRegistry);
    }

    $functionRegistry->freeze();
    $variableRegistry->freeze();

    FormulaFacade::initialize($functionRegistry, $variableRegistry);
    FormulaFacade::setEngine($runtime);
    FormulaFacade::freeze();
}

/**
 * Register all built-in function providers shipped with the framework.
 *
 * @param Formula_Registry_FunctionRegistry $registry
 * @return void
 */
function formula_register_builtin_functions(Formula_Registry_FunctionRegistry $registry)
{
    foreach (formula_discover_provider_classes() as $class_name) {
        $reflection = new ReflectionClass($class_name);

        if (!$reflection->isInstantiable()) {
            continue;
        }

        if (!$reflection->implementsInterface('Formula_Contracts_FormulaFunctionInterface')) {
            continue;
        }

        $function = $reflection->newInstance();
        if (!$registry->has($function->getName())) {
            $registry->register($function);
        }
    }
}

/**
 * Register all built-in variable providers shipped with the framework.
 *
 * @param Formula_Registry_VariableRegistry $registry
 * @return void
 */
function formula_register_builtin_variable_providers(Formula_Registry_VariableRegistry $registry)
{
    foreach (formula_discover_provider_classes() as $class_name) {
        $reflection = new ReflectionClass($class_name);

        if (!$reflection->isInstantiable()) {
            continue;
        }

        if (!$reflection->implementsInterface('Formula_Contracts_VariableProviderInterface')) {
            continue;
        }

        $provider = $reflection->newInstance();
        $metadata = $provider->getMetadata();
        $namespaces = $metadata instanceof Formula_Registry_ProviderMetadata
            ? $metadata->namespaces
            : array();

        foreach ($namespaces as $namespace) {
            if (!$registry->hasNamespace($namespace)) {
                $registry->register($namespace, $provider);
            }
        }
    }
}

/**
 * Discover all provider classes from the aggregate provider files.
 *
 * @return string[]
 */
function formula_discover_provider_classes()
{
    $classes = array();
    $files = glob(dirname(__FILE__) . '/Providers/*.php');

    if (!is_array($files)) {
        return array();
    }

    foreach ($files as $file_path) {
        if (basename($file_path) === 'index.php') {
            continue;
        }

        require_once $file_path;

        $content = @file_get_contents($file_path);
        $matches = array();

        if ($content === false) {
            continue;
        }

        preg_match_all('/^\s*(?:abstract\s+|final\s+)?class\s+([A-Za-z0-9_]+)/m', $content, $matches);
        if (isset($matches[1])) {
            $classes = array_merge($classes, $matches[1]);
        }
    }

    return array_values(array_unique($classes));
}

// Mark bootstrap as loaded
$GLOBALS['formula_bootstrap_initialized'] = true;
