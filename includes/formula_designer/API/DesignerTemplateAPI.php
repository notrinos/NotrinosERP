<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

if (!defined('FORMULA_DESIGNER_AUTHORIZED_CONTROLLER')) {
    http_response_code(404);
    header('Content-Type: application/json; charset=UTF-8');
    header('Cache-Control: no-store');
    header('X-Content-Type-Options: nosniff');
    error_log('[formula_designer_security] outcome=denied_direct_endpoint endpoint=templates method=' . strtoupper(isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : 'GET'));
    echo json_encode(array('success' => false, 'error' => 'Not found.'));
    exit;
}
require_once dirname(__DIR__) . '/designer_bootstrap.inc';

/**
 * Authorized template-discovery endpoint implementation.
 *
 * @package FormulaDesigner\API
 * @since   2.0.0
 */
class FormulaDesigner_API_DesignerTemplateAPI
{
    /**
     * Emit template metadata for the validated module request.
     *
     * @return void
     */
    public static function handleRequest()
    {
        $module = isset($_GET['module']) ? (string)$_GET['module'] : '';
        $sections = DesignerFacade::getAvailableTemplates($module);

        FormulaDesigner_API_DesignerAPI::respond(array(
            'success' => true,
            'module' => $module,
            'sections' => $sections,
            'total' => array_reduce($sections, function ($carry, $section) {
                return $carry + (isset($section['count']) ? (int)$section['count'] : 0);
            }, 0),
        ));
    }
}
