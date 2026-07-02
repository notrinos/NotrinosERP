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

require_once dirname(__FILE__) . '/designer_bootstrap.inc';
define('FORMULA_DESIGNER_API_NO_AUTO_RUN', true);
require_once dirname(__FILE__) . '/DesignerAPI.php';

/**
 * DesignerExplainAPI — Phase 7 preview and explain endpoint.
 *
 * Accepts POST requests with the formula, module, and optional sample
 * values. Uses the frozen FormulaFacade to produce a live preview result
 * and a step-by-step explain trace.
 *
 * @package FormulaDesigner\API
 * @since   2.0.0
 */
class FormulaDesigner_API_DesignerExplainAPI
{
    /**
     * Handle the preview / explain request.
     *
     * @return void
     */
    public static function handleRequest()
    {
        if (strtoupper(isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : 'GET') !== 'POST') {
            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => false,
                'error' => 'Explain / preview requests must use POST.',
            ), 405);
        }

        $action  = isset($_REQUEST['action']) ? strtolower((string)$_REQUEST['action']) : '';
        $module  = isset($_REQUEST['module']) ? (string)$_REQUEST['module'] : 'hrm';
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

        switch ($action) {
            case 'preview':
                self::respondPreview($formula, $module);
                break;

            case 'explain':
                self::respondExplain($formula, $module);
                break;

            default:
                FormulaDesigner_API_DesignerAPI::respond(array(
                    'ok' => false,
                    'error' => 'Unknown action. Use "preview" or "explain".',
                ), 400);
        }
    }

    /**
     * Evaluate a preview of the formula and return the result.
     *
     * @param string $formula
     * @param string $module
     * @return void
     */
    private static function respondPreview($formula, $module)
    {
        self::ensureFormulaFrameworkReady();

        $sampleValues = isset($_POST['sampleValues']) && is_array($_POST['sampleValues'])
            ? $_POST['sampleValues']
            : array();

        try {
            $context = self::buildContextFromSampleValues($sampleValues);

            if (trim($formula) === '') {
                FormulaDesigner_API_DesignerAPI::respond(array(
                    'ok' => true,
                    'module' => $module,
                    'result' => 0.0,
                    'note' => 'Empty formula.',
                    'variables' => array(),
                ));
            }

            $result = FormulaFacade::evaluate($formula, $context);

            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => true,
                'module' => $module,
                'result' => $result,
                'note' => null,
                'variables' => self::extractPreviewVariables($formula),
            ));
        } catch (Formula_Exceptions_FormulaException $e) {
            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => false,
                'error' => $e->getMessage(),
                'line' => $e->getSourceLine(),
                'column' => $e->getSourceColumn(),
            ));
        } catch (Exception $e) {
            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => false,
                'error' => 'Preview failed: ' . $e->getMessage(),
            ));
        }
    }

    /**
     * Produce a step-by-step explain trace.
     *
     * @param string $formula
     * @param string $module
     * @return void
     */
    private static function respondExplain($formula, $module)
    {
        self::ensureFormulaFrameworkReady();

        $sampleValues = isset($_POST['sampleValues']) && is_array($_POST['sampleValues'])
            ? $_POST['sampleValues']
            : array();

        try {
            $context = self::buildContextFromSampleValues($sampleValues);

            if (trim($formula) === '') {
                FormulaDesigner_API_DesignerAPI::respond(array(
                    'ok' => true,
                    'module' => $module,
                    'steps' => array(),
                    'result' => 0.0,
                    'durationMs' => 0,
                ));
            }

            $explainResult = FormulaFacade::explain($formula, $context);

            $steps = array();
            foreach ($explainResult->steps as $step) {
                $steps[] = array(
                    'label' => isset($step['label']) ? $step['label'] : '',
                    'detail' => isset($step['detail']) ? $step['detail'] : '',
                    'result' => isset($step['result']) ? $step['result'] : null,
                );
            }

            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => true,
                'module' => $module,
                'steps' => $steps,
                'result' => $explainResult->result,
                'durationMs' => $explainResult->durationMs,
            ));
        } catch (Formula_Exceptions_FormulaException $e) {
            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => false,
                'error' => $e->getMessage(),
                'line' => $e->getSourceLine(),
                'column' => $e->getSourceColumn(),
            ));
        } catch (Exception $e) {
            FormulaDesigner_API_DesignerAPI::respond(array(
                'ok' => false,
                'error' => 'Explain failed: ' . $e->getMessage(),
            ));
        }
    }

    /**
     * Build a FormulaContext from sample values submitted by the designer.
     *
     * Sample values arrive as a flat key→value map (e.g., "Employee.BasicSalary" => "8000").
     * Qualified names (namespace.identifier) are parsed and injected as
     * variable provider lookups; simple names are stored as flat variables.
     *
     * @param array $sampleValues
     * @return Formula_Context_FormulaContext
     */
    private static function buildContextFromSampleValues(array $sampleValues)
    {
        $builder = Formula_Context_FormulaContextBuilder::create();

        foreach ($sampleValues as $key => $value) {
            $key   = trim((string)$key);
            $value = trim((string)$value);

            if ($key === '') {
                continue;
            }

            // Attempt to convert to numeric
            if (is_numeric($value)) {
                $value = strpos($value, '.') !== false ? (float)$value : (int)$value;
            }

            $builder->withVariable($key, $value);
        }

        return $builder->withCompatibilityMode(true)->build();
    }

    /**
     * Extract a preliminary list of variable references from a formula
     * string so the front-end can seed sample inputs.
     *
     * Uses a simple regex-based scanner — does NOT require a compiler.
     *
     * @param string $formula
     * @return array  key => default (0)
     */
    private static function extractPreviewVariables($formula)
    {
        $variables = array();

        // Match qualified identifiers like Employee.BasicSalary or Payroll.Gross
        if (preg_match_all('/\b([A-Za-z_][A-Za-z0-9_]*)\.([A-Za-z_][A-Za-z0-9_]*)\b/', (string)$formula, $matches)) {
            for ($i = 0; $i < count($matches[0]); $i++) {
                $full = $matches[0][$i];
                if (!isset($variables[$full])) {
                    $variables[$full] = 0;
                }
            }
        }

        // Match simple identifiers that look like uppercase variable names
        if (preg_match_all('/\b([A-Z][A-Z0-9_]{2,})\b/', (string)$formula, $matches)) {
            for ($i = 0; $i < count($matches[0]); $i++) {
                $name = $matches[1][$i];
                // Skip Java-style constants
                if (in_array($name, array('NULL', 'TRUE', 'FALSE', 'NOT', 'AND', 'OR', 'XOR', 'IF', 'ROUND', 'ABS', 'INT', 'SQRT', 'SUM', 'MIN', 'MAX', 'AVG', 'COUNT'), true)) {
                    continue;
                }
                $variables[$name] = 0;
            }
        }

        return $variables;
    }

    /**
     * Ensure the public formula framework bootstrap is loaded.
     *
     * @return void
     */
    private static function ensureFormulaFrameworkReady()
    {
        if (!isset($GLOBALS['path_to_root']) || !$GLOBALS['path_to_root']) {
            $GLOBALS['path_to_root'] = dirname(dirname(dirname(dirname(__FILE__))));
        }

        if (!class_exists('FormulaFacade', false)) {
            require_once $GLOBALS['path_to_root'] . '/includes/formula/formula_bootstrap.inc';
        }

        if (class_exists('FormulaFacade', false) && FormulaFacade::isInitialized()) {
            return;
        }

        // Standalone initialisation for the designer preview/explain endpoints
        $functionRegistry = new Formula_Registry_FunctionRegistry();
        $variableRegistry = new Formula_Registry_VariableRegistry();

        FormulaFacade::initialize($functionRegistry, $variableRegistry);
        FormulaFacade::setEngine(
            new Formula_Runtime_FormulaRuntime($functionRegistry, $variableRegistry)
        );
        FormulaFacade::freeze();
    }
}

FormulaDesigner_API_DesignerExplainAPI::handleRequest();
