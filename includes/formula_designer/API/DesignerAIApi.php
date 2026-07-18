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

if (!defined('FORMULA_DESIGNER_AUTHORIZED_CONTROLLER')) {
    http_response_code(404);
    header('Content-Type: application/json; charset=UTF-8');
    header('Cache-Control: no-store');
    header('X-Content-Type-Options: nosniff');
    error_log('[formula_designer_security] outcome=denied_direct_endpoint endpoint=ai method=' . strtoupper(isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : 'GET'));
    echo json_encode(array('success' => false, 'errorMessage' => 'Not found.'));
    exit;
}

require_once dirname(__FILE__) . '/../designer_bootstrap.inc';

// Ensure AI classes are loaded
if (file_exists(dirname(__FILE__) . '/../AI/AIAssistantAdapter.php')) {
    require_once dirname(__FILE__) . '/../AI/AIAssistantAdapter.php';
    require_once dirname(__FILE__) . '/../AI/AIResult.php';
    require_once dirname(__FILE__) . '/../AI/RuleBasedProvider.php';
    require_once dirname(__FILE__) . '/../AI/ExternalLLMProvider.php';
}

/**
 * DesignerAIApi — Phase 15 AI features AJAX endpoint.
 *
 * Handles AJAX requests for AI formula assistance:
 *  - Natural language → NFX conversion
 *  - Excel formula import
 *  - Variable suggestions
 *  - Formula optimization suggestions
 *  - Formula explanation
 *  - Duplicate detection
 *  - Formula documentation generation
 *  - Formula debugging
 *
 * @package FormulaDesigner\API
 * @since   2.0.0
 */
class FormulaDesigner_API_DesignerAIApi
{
    /**
     * Handle the AI AJAX request.
     *
     * @return void
     */
    public static function handleRequest()
    {
        if (strtoupper(isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : 'GET') !== 'POST') {
            self::respondError('AI requests must use POST.', 405);
        }

        $action = isset($_REQUEST['action']) ? (string)$_REQUEST['action'] : '';

        // Initialize AI assistant
        FormulaDesigner_AI_AIAssistantAdapter::initialize();

        // Check if AI features are enabled
        if (!FormulaDesigner_AI_AIAssistantAdapter::isEnabled()) {
            self::respondError('AI features are currently disabled.', 403);
        }

        switch ($action) {
            case 'nl_to_formula':
                self::handleNaturalLanguageToFormula();
                break;

            case 'convert_excel':
                self::handleConvertExcel();
                break;

            case 'suggest_variables':
                self::handleSuggestVariables();
                break;

            case 'suggest_optimization':
                self::handleSuggestOptimization();
                break;

            case 'explain':
                self::handleExplain();
                break;

            case 'detect_duplicates':
                self::handleDetectDuplicates();
                break;

            case 'generate_documentation':
                self::handleGenerateDocumentation();
                break;

            case 'debug':
                self::handleDebug();
                break;

            case 'status':
                self::handleStatus();
                break;

            default:
                self::respondError('Unknown AI action. Supported: nl_to_formula, convert_excel, explain, debug, status.', 400);
        }
    }

    /**
     * Handle natural language → NFX conversion.
     *
     * @return void
     */
    private static function handleNaturalLanguageToFormula()
    {
        $description = isset($_POST['description']) ? trim((string)$_POST['description']) : '';
        $module      = isset($_POST['module']) ? (string)$_POST['module'] : 'hrm';

        $context = array(
            'availableVariables' => isset($_POST['availableVariables']) ? (array)$_POST['availableVariables'] : array(),
            'availableFunctions' => isset($_POST['availableFunctions']) ? (array)$_POST['availableFunctions'] : array(),
            'businessRules'      => isset($_POST['businessRules']) ? (array)$_POST['businessRules'] : array(),
        );

        if ($description === '') {
            self::respondError('No description provided. Please describe the formula in plain language.');
        }

        try {
            $result = FormulaDesigner_AI_AIAssistantAdapter::naturalLanguageToFormula($description, $module, $context);
            self::respondSuccess($result->toArray());
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle Excel formula import.
     *
     * @return void
     */
    private static function handleConvertExcel()
    {
        $excelFormula = isset($_POST['excel_formula']) ? trim((string)$_POST['excel_formula']) : '';

        if ($excelFormula === '') {
            self::respondError('No Excel formula provided.');
        }

        try {
            $result = FormulaDesigner_AI_AIAssistantAdapter::convertExcelFormula($excelFormula);
            self::respondSuccess($result->toArray());
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle variable suggestions.
     *
     * @return void
     */
    private static function handleSuggestVariables()
    {
        $partialFormula = isset($_POST['partial_formula']) ? trim((string)$_POST['partial_formula']) : '';
        $module         = isset($_POST['module']) ? (string)$_POST['module'] : 'hrm';

        $context = array(
            'availableVariables' => isset($_POST['availableVariables']) ? (array)$_POST['availableVariables'] : array(),
        );

        try {
            $result = FormulaDesigner_AI_AIAssistantAdapter::suggestVariables($partialFormula, $module, $context);
            self::respondSuccess($result->toArray());
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle optimization suggestions.
     *
     * @return void
     */
    private static function handleSuggestOptimization()
    {
        $formula = isset($_POST['formula']) ? trim((string)$_POST['formula']) : '';

        if ($formula === '') {
            self::respondError('No formula provided for optimization analysis.');
        }

        try {
            $result = FormulaDesigner_AI_AIAssistantAdapter::suggestOptimization($formula);
            self::respondSuccess($result->toArray());
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle formula explanation.
     *
     * @return void
     */
    private static function handleExplain()
    {
        $formula = isset($_POST['formula']) ? trim((string)$_POST['formula']) : '';

        if ($formula === '') {
            self::respondError('No formula provided for explanation.');
        }

        try {
            $explanation = FormulaDesigner_AI_AIAssistantAdapter::explainInPlainLanguage($formula);
            self::respondSuccess(array(
                'formula'     => $formula,
                'explanation' => $explanation,
            ));
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle duplicate detection.
     *
     * @return void
     */
    private static function handleDetectDuplicates()
    {
        $formula  = isset($_POST['formula']) ? trim((string)$_POST['formula']) : '';
        $module   = isset($_POST['module']) ? (string)$_POST['module'] : 'hrm';
        $existing = isset($_POST['existingFormulas']) ? (array)$_POST['existingFormulas'] : array();

        if ($formula === '') {
            self::respondError('No formula provided for duplicate detection.');
        }

        try {
            $result = FormulaDesigner_AI_AIAssistantAdapter::detectDuplicates($formula, $module, $existing);
            self::respondSuccess($result->toArray());
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle documentation generation.
     *
     * @return void
     */
    private static function handleGenerateDocumentation()
    {
        $formula = isset($_POST['formula']) ? trim((string)$_POST['formula']) : '';

        if ($formula === '') {
            self::respondError('No formula provided for documentation.');
        }

        try {
            $doc = FormulaDesigner_AI_AIAssistantAdapter::generateDocumentation($formula);
            self::respondSuccess(array(
                'formula'       => $formula,
                'documentation' => $doc,
            ));
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle formula debugging.
     *
     * @return void
     */
    private static function handleDebug()
    {
        $formula = isset($_POST['formula']) ? trim((string)$_POST['formula']) : '';

        if ($formula === '') {
            self::respondError('No formula provided for debugging.');
        }

        try {
            $result = FormulaDesigner_AI_AIAssistantAdapter::debugFormula($formula);
            self::respondSuccess($result->toArray());
        } catch (Exception $e) {
            self::respondError($e->getMessage());
        }
    }

    /**
     * Handle AI status check.
     *
     * @return void
     */
    private static function handleStatus()
    {
        self::respondSuccess(array(
            'enabled'    => FormulaDesigner_AI_AIAssistantAdapter::isEnabled(),
            'available'  => FormulaDesigner_AI_AIAssistantAdapter::isAvailable(),
            'provider'   => FormulaDesigner_AI_AIAssistantAdapter::getProviderName(),
        ));
    }

    /**
     * Send a success JSON response.
     *
     * @param array $data
     * @return void
     */
    private static function respondSuccess(array $data)
    {
        if (function_exists('formula_designer_api_json_response')) {
            formula_designer_api_json_response(array_merge(array('success' => true), $data), 200);
        }
        header('Content-Type: application/json; charset=utf-8');
        echo json_encode(array_merge(array('success' => true), $data));
        exit;
    }

    /**
     * Send an error JSON response.
     *
     * @param string $message
     * @param int    $httpCode
     * @return void
     */
    private static function respondError($message, $httpCode = 400)
    {
        if (function_exists('formula_designer_api_json_response')) {
            formula_designer_api_json_response(array(
                'success' => false,
                'errorMessage' => $message,
            ), $httpCode);
        }
        http_response_code($httpCode);
        header('Content-Type: application/json; charset=utf-8');
        echo json_encode(array(
            'success'      => false,
            'errorMessage' => $message,
        ));
        exit;
    }
}

// Auto-handle the request if this file is accessed directly
if (!defined('FORMULA_DESIGNER_API_NO_AUTO_RUN')) {
    FormulaDesigner_API_DesignerAIApi::handleRequest();
}
