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
 * DesignerTemplateAPI — AJAX endpoint for template discovery.
 *
 * Accepts GET requests with a `module` parameter and returns
 * category-grouped template metadata in JSON format.
 *
 * URL: /includes/formula_designer/API/DesignerTemplateAPI.php?module=hrm
 *
 * @package FormulaDesigner\API
 * @since   2.0.0
 */

// ---------------------------------------------------------------------------
// Bootstrap guard — load designer infrastructure
// ---------------------------------------------------------------------------

$path_to_root = dirname(dirname(dirname(__FILE__)));

require_once $path_to_root . '/includes/formula_designer/designer_bootstrap.inc';
require_once $path_to_root . '/includes/formula_designer/DesignerFacade.php';

// When running outside a full NotrinosERP session, initialize the designer
// package directly so the facade is ready.
if (method_exists('DesignerFacade', 'initialize')) {
    DesignerFacade::initialize();
}

$module = isset($_GET['module']) ? (string)$_GET['module'] : '';

// ---------------------------------------------------------------------------
// Build and emit the response
// ---------------------------------------------------------------------------

$sections = DesignerFacade::getAvailableTemplates($module);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('X-Content-Type-Options: nosniff');

echo json_encode(array(
    'success'  => true,
    'module'   => $module,
    'sections' => $sections,
    'total'    => array_reduce($sections, function ($carry, $section) {
        return $carry + (isset($section['count']) ? (int)$section['count'] : 0);
    }, 0),
));
