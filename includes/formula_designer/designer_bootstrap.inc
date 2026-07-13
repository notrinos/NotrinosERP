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
 * Visual Formula Designer bootstrap file.
 *
 *
 * @package FormulaDesigner
 * @since   2.0.0
 */

defined('FORMULA_DESIGNER_BOOTSTRAP_LOADED') || define('FORMULA_DESIGNER_BOOTSTRAP_LOADED', true);
defined('FORMULA_DESIGNER_VERSION') || define('FORMULA_DESIGNER_VERSION', '1.0.0');
defined('FORMULA_DESIGNER_ASSET_VERSION') || define('FORMULA_DESIGNER_ASSET_VERSION', FORMULA_DESIGNER_VERSION);
defined('FORMULA_DESIGNER_HOOK_REGISTER_FIELDS') || define(
    'FORMULA_DESIGNER_HOOK_REGISTER_FIELDS',
    'formula_designer_register_fields'
);
defined('FORMULA_DESIGNER_HOOK_REGISTER_TEMPLATES') || define(
    'FORMULA_DESIGNER_HOOK_REGISTER_TEMPLATES',
    'formula_designer_register_templates'
);
defined('FORMULA_DESIGNER_HOOK_REGISTER_EXTENSIONS') || define(
    'FORMULA_DESIGNER_HOOK_REGISTER_EXTENSIONS',
    'formula_designer_register_extensions'
);
defined('FORMULA_DESIGNER_HOOK_REGISTER_AI_PROVIDERS') || define(
    'FORMULA_DESIGNER_HOOK_REGISTER_AI_PROVIDERS',
    'formula_designer_register_ai_providers'
);

/**
 * ---------------------------------------------------------------------------
 * Module provider bridge
 * ---------------------------------------------------------------------------
 *
 * The existing NotrinosERP hook system dispatches only to extension hook
 * classes (modules/ hooks classes), not to core module provider files under
 * {module}/includes/. This bridge allows core modules to register fields,
 * templates, and validator callbacks without requiring a full extension
 * hook class.
 *
 * Module provider files call `formula_designer_register_provider()` to
 * register their callback. The designer facade dispatches to registered
 * callbacks in addition to the legacy hook_invoke_all() path, preserving
 * backward compatibility for extensions that already use hooks.
 *
 * @since 2.0.0
 */

defined('FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS') || define(
    'FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS',
    'fields'
);
defined('FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES') || define(
    'FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES',
    'templates'
);

/**
 * Register a provider callback that will be invoked when the designer builds
 * its field or template registry.
 *
 * @param string $provider_type   One of FORMULA_DESIGNER_PROVIDER_TYPE_* constants.
 * @param string $module          Lowercase module identifier (e.g. 'hrm').
 * @param string $file_path       Absolute path to the provider include file.
 * @return void
 */
function formula_designer_register_provider($provider_type, $module, $file_path)
{
    if (!isset($GLOBALS['formula_designer_providers'])) {
        $GLOBALS['formula_designer_providers'] = array();
    }

    $key = $provider_type . '::' . strtolower($module);

    if (!isset($GLOBALS['formula_designer_providers'][$key])) {
        $GLOBALS['formula_designer_providers'][$key] = array();
    }

    $GLOBALS['formula_designer_providers'][$key][] = $file_path;
}

/**
 * Load registered provider files for a given type and module.
 *
 * Each provider .inc file is expected to define one or more functions that
 * implement the designer's provider contract. Loading the file registers
 * those functions (or callback data) with the global provider store.
 *
 * @param string $provider_type  One of FORMULA_DESIGNER_PROVIDER_TYPE_* constants.
 * @param string $module         Lowercase module identifier.
 * @return void
 */
function formula_designer_load_providers($provider_type, $module)
{
    if (!isset($GLOBALS['formula_designer_providers'])) {
        return;
    }

    $key = $provider_type . '::' . strtolower($module);

    if (!isset($GLOBALS['formula_designer_providers'][$key])) {
        return;
    }

    foreach ((array)$GLOBALS['formula_designer_providers'][$key] as $file) {
        if (is_string($file) && file_exists($file)) {
            require_once $file;
        }
    }
}

/**
 * Invoke all registered field-provider callbacks for a module.
 *
 * Modules register callbacks via the `formula_designer_register_field_`
 * family of functions. This dispatcher calls each callback with the field
 * registry and module identifier.
 *
 * @param FormulaDesigner_Registry_DesignerFieldRegistry $registry
 * @param string                                         $module
 * @return void
 */
function formula_designer_invoke_field_providers(
    FormulaDesigner_Registry_DesignerFieldRegistry $registry,
    $module
) {
    if (!isset($GLOBALS['formula_designer_provider_fields'])) {
        return;
    }

    $module_lower = strtolower($module);
    $callbacks = &$GLOBALS['formula_designer_provider_fields'];

    // Invoke module-specific callbacks
    if (isset($callbacks[$module_lower])) {
        foreach ((array)$callbacks[$module_lower] as $cb) {
            if (is_callable($cb)) {
                call_user_func($cb, $registry, $module_lower);
            }
        }
    }

    // Invoke wildcard callbacks (register fields for all modules)
    if ($module_lower !== '*' && isset($callbacks['*'])) {
        foreach ((array)$callbacks['*'] as $cb) {
            if (is_callable($cb)) {
                call_user_func($cb, $registry, $module_lower);
            }
        }
    }
}

/**
 * Invoke all registered template-provider callbacks for a module.
 *
 * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
 * @param string                                            $module
 * @return void
 */
function formula_designer_invoke_template_providers(
    FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
    $module
) {
    if (!isset($GLOBALS['formula_designer_provider_templates'])) {
        return;
    }

    $module_lower = strtolower($module);
    $callbacks = &$GLOBALS['formula_designer_provider_templates'];

    if (isset($callbacks[$module_lower])) {
        foreach ((array)$callbacks[$module_lower] as $cb) {
            if (is_callable($cb)) {
                call_user_func($cb, $registry, $module_lower);
            }
        }
    }

    if ($module_lower !== '*' && isset($callbacks['*'])) {
        foreach ((array)$callbacks['*'] as $cb) {
            if (is_callable($cb)) {
                call_user_func($cb, $registry, $module_lower);
            }
        }
    }
}

/**
 * ---------------------------------------------------------------------------
 * Auto-discovery of module providers
 * ---------------------------------------------------------------------------
 *
 * When the designer is loaded during a full session-bootstrap (i.e. when
 * $path_to_root is available), this function automatically discovers and
 * loads module formula providers from their canonical locations.
 *
 * Providers are located at: {module}/includes/{module}_formula_provider.inc
 * for the six target modules.
 *
 * @return void
 */
function formula_designer_autodiscover_providers()
{
    global $path_to_root;

    if (!isset($path_to_root) || !$path_to_root) {
        return;
    }

    $root = rtrim($path_to_root, '/\\');

    $module_provider_map = array(
        'hrm'           => $root . '/hrm/includes/hrm_formula_provider.inc',
        'sales'         => $root . '/sales/includes/sales_formula_provider.inc',
        'inventory'     => $root . '/inventory/includes/inventory_formula_provider.inc',
        'purchasing'    => $root . '/purchasing/includes/purchasing_formula_provider.inc',
        'manufacturing' => $root . '/manufacturing/includes/manufacturing_formula_provider.inc',
        'gl'            => $root . '/gl/includes/gl_formula_provider.inc',
    );

    foreach ($module_provider_map as $module => $file_path) {
        if (file_exists($file_path)) {
            require_once $file_path;
        }
    }
}

// ---------------------------------------------------------------------------
// Original constants and autoloader (unchanged)
// ---------------------------------------------------------------------------

/**
 * Resolve the absolute package base path.
 *
 * @return string
 */
function formula_designer_base_path()
{
    global $path_to_root;

    if (isset($path_to_root) && $path_to_root !== '') {
        return rtrim($path_to_root, '/\\') . '/includes/formula_designer';
    }

    return dirname(__FILE__);
}

/**
 * Visual Formula Designer autoloader.
 *
 * The public facade keeps the short `DesignerFacade` name while all internal
 * classes use the `FormulaDesigner_` prefix to avoid collisions with the
 * existing `Formula_` framework classes.
 *
 * @param string $class_name
 * @return void
 */
function formula_designer_autoload($class_name)
{
    if ($class_name === 'DesignerFacade') {
        $facade_path = formula_designer_base_path() . '/DesignerFacade.php';
        if (file_exists($facade_path)) {
            require_once $facade_path;
        }
        return;
    }

    $prefix = 'FormulaDesigner_';
    if (strncmp($class_name, $prefix, strlen($prefix)) !== 0) {
        return;
    }

    $relative_class = substr($class_name, strlen($prefix));
    $file_path = str_replace('_', DIRECTORY_SEPARATOR, $relative_class) . '.php';
    $absolute_path = formula_designer_base_path() . DIRECTORY_SEPARATOR . $file_path;

    if (file_exists($absolute_path)) {
        require_once $absolute_path;
    }
}

/**
 * Get the reserved hook names owned by the designer package.
 *
 * @return array
 */
function formula_designer_get_registered_hooks()
{
    return array(
        FORMULA_DESIGNER_HOOK_REGISTER_FIELDS,
        FORMULA_DESIGNER_HOOK_REGISTER_TEMPLATES,
        FORMULA_DESIGNER_HOOK_REGISTER_EXTENSIONS,
    );
}

/**
 * Initialize the scaffold-level bootstrap state.
 *
 * @return array
 */
function formula_designer_initialize_framework()
{
    $GLOBALS['formula_designer_registered_hooks'] = formula_designer_get_registered_hooks();

    // auto-discover and load module providers
    formula_designer_autodiscover_providers();

    return $GLOBALS['formula_designer_registered_hooks'];
}

if (!in_array('formula_designer_autoload', (array)spl_autoload_functions(), true)) {
    spl_autoload_register('formula_designer_autoload');
}

$GLOBALS['formula_designer_bootstrap_initialized'] = true;
formula_designer_initialize_framework();