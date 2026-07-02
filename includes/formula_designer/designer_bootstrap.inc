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
 * Phase 1 only establishes the package scaffold: constants, autoloading,
 * and reserved hook names. Rendering, field registration, and module
 * integration are intentionally deferred to later roadmap phases.
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
 * Phase 1 reserves these identifiers so later phases can wire real hook
 * dispatch without changing the public bootstrap contract.
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
    return $GLOBALS['formula_designer_registered_hooks'];
}

if (!in_array('formula_designer_autoload', (array)spl_autoload_functions(), true)) {
    spl_autoload_register('formula_designer_autoload');
}

$GLOBALS['formula_designer_bootstrap_initialized'] = true;
formula_designer_initialize_framework();