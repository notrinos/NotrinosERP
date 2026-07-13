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

require_once dirname(__DIR__) . '/designer_bootstrap.inc';

/**
 * DesignerAPI — routes the Phase 3 metadata endpoints.
 *
 * @package FormulaDesigner\API
 * @since   2.0.0
 */
class FormulaDesigner_API_DesignerAPI
{
    /**
     * Dispatch the incoming request.
     *
     * @return void
     */
    public static function handleRequest()
    {
        $action = isset($_GET['action']) ? strtolower((string)$_GET['action']) : '';

        switch ($action) {
            case 'fields':
                self::renderFields();
                break;

            case 'functions':
                self::renderFunctions();
                break;

            default:
                self::respond(array(
                    'ok' => false,
                    'error' => 'Unknown designer API action.',
                ), 400);
        }
    }

    /**
     * Emit the field palette payload.
     *
     * @return void
     */
    public static function renderFields()
    {
        $module = isset($_GET['module']) ? (string)$_GET['module'] : 'hrm';

        self::respond(array(
            'ok' => true,
            'module' => $module,
            'namespaces' => DesignerFacade::getAvailableFields($module),
        ));
    }

    /**
     * Emit the function palette payload.
     *
     * @return void
     */
    public static function renderFunctions()
    {
        $module = isset($_GET['module']) ? (string)$_GET['module'] : 'hrm';

        self::respond(array(
            'ok' => true,
            'module' => $module,
            'categories' => DesignerFacade::getAvailableFunctions($module),
        ));
    }

    /**
     * Emit a JSON response.
     *
     * @param array $payload
     * @param int   $status_code
     * @return void
     */
    public static function respond(array $payload, $status_code = 200)
    {
        if (!headers_sent()) {
            http_response_code((int)$status_code);
            header('Content-Type: application/json; charset=UTF-8');
            header('Cache-Control: no-store, no-cache, must-revalidate');
        }

        echo json_encode($payload);
        exit;
    }
}

if (!defined('FORMULA_DESIGNER_API_NO_AUTO_RUN')) {
    FormulaDesigner_API_DesignerAPI::handleRequest();
}