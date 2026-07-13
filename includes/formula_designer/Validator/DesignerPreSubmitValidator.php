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
 * DesignerPreSubmitValidator — shared validation adapter for the designer UI.
 *
 * @package FormulaDesigner\Validator
 * @since   2.0.0
 */
class FormulaDesigner_Validator_DesignerPreSubmitValidator
{
    /**
     * Validate a formula string through the frozen public facade.
     *
     * @param string $formula
     * @return Formula_Compiler_ValidationResult
     */
    public static function validateFormula($formula)
    {
        self::ensureFormulaFrameworkReady();

        return FormulaFacade::validate((string)$formula);
    }

    /**
     * Convert a validation result into an API payload.
     *
     * @param string $formula
     * @return array
     */
    public static function buildPayload($formula)
    {
        try {
            $result = self::validateFormula($formula);

            return array(
                'isValid' => $result->isValid,
                'errors' => $result->errors,
                'warnings' => $result->warnings,
                'errorCount' => $result->errorCount(),
                'warningCount' => $result->warningCount(),
            );
        } catch (Exception $exception) {
            return array(
                'isValid' => false,
                'errors' => array(
                    array(
                        'message' => $exception->getMessage(),
                        'line' => 0,
                        'column' => 0,
                    ),
                ),
                'warnings' => array(),
                'errorCount' => 1,
                'warningCount' => 0,
            );
        }
    }

    /**
     * Message shown when a submit is blocked by validation errors.
     *
     * @return string
     */
    public static function getSubmitBlockMessage()
    {
        return 'Fix validation errors before saving this formula.';
    }

    /**
     * Ensure the public formula facade is available.
     *
     * @return void
     */
    private static function ensureFormulaBootstrapLoaded()
    {
        if (!isset($GLOBALS['path_to_root']) || !$GLOBALS['path_to_root']) {
            $GLOBALS['path_to_root'] = dirname(dirname(dirname(dirname(__FILE__))));
        }

        if (!class_exists('FormulaFacade', false)) {
            require_once $GLOBALS['path_to_root'] . '/includes/formula/formula_bootstrap.inc';
        }
    }

    /**
     * Ensure the frozen public framework is initialized for standalone requests.
     *
     * @return void
     */
    private static function ensureFormulaFrameworkReady()
    {
        self::ensureFormulaBootstrapLoaded();

        if (class_exists('FormulaFacade', false) && FormulaFacade::isInitialized()) {
            return;
        }

        self::initializeStandaloneFramework();
    }

    /**
     * Initialize the formula framework with built-in providers only.
     *
     * @return void
     */
    private static function initializeStandaloneFramework()
    {
        $function_registry = new Formula_Registry_FunctionRegistry();
        $variable_registry = new Formula_Registry_VariableRegistry();

        self::registerBuiltInFunctions($function_registry);
        self::registerBuiltInVariableProviders($variable_registry);

        FormulaFacade::initialize($function_registry, $variable_registry);
        FormulaFacade::setEngine(
            new Formula_Runtime_FormulaRuntime($function_registry, $variable_registry)
        );
        FormulaFacade::freeze();
    }

    /**
     * Register all built-in function providers shipped with the framework.
     *
     * @param Formula_Registry_FunctionRegistry $registry
     * @return void
     */
    private static function registerBuiltInFunctions(Formula_Registry_FunctionRegistry $registry)
    {
        $classes = self::discoverProviderClasses();
        $index = 0;

        for ($index = 0; $index < count($classes); $index++) {
            $class_name = $classes[$index];
            $reflection = new ReflectionClass($class_name);

            if (!$reflection->isInstantiable()) {
                continue;
            }

            if (!$reflection->implementsInterface('Formula_Contracts_FormulaFunctionInterface')) {
                continue;
            }

            $registry->register($reflection->newInstance());
        }
    }

    /**
     * Register all built-in variable providers shipped with the framework.
     *
     * @param Formula_Registry_VariableRegistry $registry
     * @return void
     */
    private static function registerBuiltInVariableProviders(Formula_Registry_VariableRegistry $registry)
    {
        $classes = self::discoverProviderClasses();
        $class_index = 0;

        for ($class_index = 0; $class_index < count($classes); $class_index++) {
            $class_name = $classes[$class_index];
            $reflection = new ReflectionClass($class_name);
            $provider = null;
            $metadata = null;
            $namespaces = array();
            $namespace_index = 0;

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

            for ($namespace_index = 0; $namespace_index < count($namespaces); $namespace_index++) {
                if (!$registry->hasNamespace($namespaces[$namespace_index])) {
                    $registry->register($namespaces[$namespace_index], $provider);
                }
            }
        }
    }

    /**
     * Discover all provider classes from the aggregate provider files.
     *
     * @return array
     */
    private static function discoverProviderClasses()
    {
        $classes = array();
        $files = self::getProviderFiles();
        $file_index = 0;

        for ($file_index = 0; $file_index < count($files); $file_index++) {
            require_once $files[$file_index];
            $classes = array_merge($classes, self::extractClassNames($files[$file_index]));
        }

        return array_values(array_unique($classes));
    }

    /**
     * Get the built-in provider file list.
     *
     * @return array
     */
    private static function getProviderFiles()
    {
        $provider_path = $GLOBALS['path_to_root'] . '/includes/formula/Providers/*.php';
        $files = glob($provider_path);

        if (!is_array($files)) {
            return array();
        }

        return array_values(array_filter($files, function ($file_path) {
            return basename($file_path) !== 'index.php';
        }));
    }

    /**
     * Extract class names declared in one provider file.
     *
     * @param string $file_path
     * @return array
     */
    private static function extractClassNames($file_path)
    {
        $content = @file_get_contents($file_path);
        $matches = array();

        if ($content === false) {
            return array();
        }

        preg_match_all('/^\s*(?:abstract\s+|final\s+)?class\s+([A-Za-z0-9_]+)/m', $content, $matches);

        return isset($matches[1]) ? $matches[1] : array();
    }
}