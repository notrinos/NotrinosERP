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

define('FORMULA_DESIGNER_API_NO_AUTO_RUN', true);
require_once dirname(__FILE__) . '/DesignerAPI.php';
require_once dirname(__DIR__) . '/Validator/DesignerPreSubmitValidator.php';

/**
 * DesignerValidateAPI — Phase 5 validation endpoint.
 *
 * @package FormulaDesigner\API
 * @since   2.0.0
 */
class FormulaDesigner_API_DesignerValidateAPI
{
    /**
     * Handle the validation request.
     *
     * @return void
     */
    public static function handleRequest()
    {
        if (strtoupper(isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : 'GET') !== 'POST') {
            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => false,
                'error' => 'Validation requests must use POST.',
            ), 405);
        }

        $module = isset($_REQUEST['module']) ? (string)$_REQUEST['module'] : 'hrm';
        $formula = isset($_POST['formula']) ? (string)$_POST['formula'] : '';

        if (
            function_exists('check_csrf_token')
            && isset($_POST['_token'])
            && $_POST['_token'] !== ''
            && !check_csrf_token()
        ) {
            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => false,
                'error' => 'Invalid CSRF token.',
            ), 403);
        }

        FormulaDesigner_API_DesignerAPI::respond(array(
            'ok' => true,
            'module' => $module,
            'formula' => $formula,
            'validation' => FormulaDesigner_Validator_DesignerPreSubmitValidator::buildPayload($formula),
        ));
    }
}

FormulaDesigner_API_DesignerValidateAPI::handleRequest();