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

// Security gate — deny direct access.
if (!defined('FORMULA_DESIGNER_BOOTSTRAP_LOADED')) {
    header('HTTP/1.0 403 Forbidden');
    exit('Direct access not permitted.');
}

/**
 * ExternalLLMProvider — Cloud LLM integration for AI formula assistance.
 *
 * Connects to external LLM APIs (OpenAI, Anthropic, etc.) for advanced
 * formula authoring, explanation, and analysis. Requires configuration
 * with an API key and endpoint URL.
 *
 * IMPORTANT SECURITY PRINCIPLE:
 *  - NO business data (salaries, prices, customer data) is EVER sent
 *    to external providers.
 *  - Only formula text, variable names, function names, and business
 *    rule descriptions are included in AI prompts.
 *  - All AI-generated formulas pass through FormulaFacade::validate()
 *    before being shown to the user.
 *
 * @package FormulaDesigner\AI
 * @since   2.0.0
 */
class FormulaDesigner_AI_ExternalLLMProvider implements FormulaDesigner_Contracts_IAIProvider
{
    /** @var string Provider identifier */
    private $name;

    /** @var string API endpoint URL */
    private $endpoint;

    /** @var string API key */
    private $apiKey;

    /** @var string Model name (e.g., 'gpt-4', 'claude-3-opus') */
    private $model;

    /** @var float Temperature (0.0–2.0) */
    private $temperature;

    /** @var int Maximum response tokens */
    private $maxTokens;

    /** @var FormulaDesigner_AI_RuleBasedProvider Fallback for offline mode */
    private $fallback;

    /** @var bool Whether to auto-fallback on failure */
    private $autoFallback;

    /**
     * Construct an external LLM provider.
     *
     * @param string      $name        Human-readable provider name
     * @param string      $endpoint    API endpoint URL
     * @param string      $apiKey      API authentication key
     * @param string      $model       Model identifier
     * @param float       $temperature Generation temperature
     * @param int         $maxTokens   Maximum response tokens
     * @param bool        $autoFallback Whether to fallback to RuleBasedProvider on failure
     */
    public function __construct(
        $name = 'OpenAI GPT-4',
        $endpoint = 'https://api.openai.com/v1/chat/completions',
        $apiKey = '',
        $model = 'gpt-4',
        $temperature = 0.3,
        $maxTokens = 500,
        $autoFallback = true
    ) {
        $this->name         = (string)$name;
        $this->endpoint     = (string)$endpoint;
        $this->apiKey       = (string)$apiKey;
        $this->model        = (string)$model;
        $this->temperature  = (float)$temperature;
        $this->maxTokens    = (int)$maxTokens;
        $this->autoFallback = (bool)$autoFallback;
        $this->fallback     = new FormulaDesigner_AI_RuleBasedProvider();
    }

    /** @return string */
    public function getName()
    {
        return $this->name;
    }

    /** @return bool */
    public function isAvailable()
    {
        return $this->apiKey !== '' && $this->endpoint !== '';
    }

    /** @inheritDoc */
    public function naturalLanguageToFormula($description, $module, array $context = array())
    {
        if (!$this->isAvailable()) {
            return $this->handleUnavailable(
                $description, $module, $context,
                'natural language to formula'
            );
        }

        $prompt = $this->buildNLToFormulaPrompt($description, $module, $context);
        return $this->callAPI('nl_to_formula', $prompt, function () use ($description, $module, $context) {
            return $this->fallback->naturalLanguageToFormula($description, $module, $context);
        });
    }

    /** @inheritDoc */
    public function suggestVariables($partialFormula, $module, array $context = array())
    {
        return $this->fallback->suggestVariables($partialFormula, $module, $context);
    }

    /** @inheritDoc */
    public function suggestOptimization($formulaText)
    {
        return $this->fallback->suggestOptimization($formulaText);
    }

    /** @inheritDoc */
    public function explainInPlainLanguage($formulaText)
    {
        if (!$this->isAvailable()) {
            return $this->fallback->explainInPlainLanguage($formulaText);
        }

        $prompt = $this->buildExplainPrompt($formulaText);
        $result = $this->callAPI('explain', $prompt, function () use ($formulaText) {
            return FormulaDesigner_AI_AIResult::success(
                $this->fallback->explainInPlainLanguage($formulaText),
                'Rule-based explanation (LLM unavailable).',
                0.5,
                'RuleBased'
            );
        });

        return $result->isSuccess() ? $result->getResult() : $this->fallback->explainInPlainLanguage($formulaText);
    }

    /** @inheritDoc */
    public function detectDuplicates($formulaText, $module, array $existingFormulas = array())
    {
        return $this->fallback->detectDuplicates($formulaText, $module, $existingFormulas);
    }

    /** @inheritDoc */
    public function generateDocumentation($formulaText)
    {
        return $this->fallback->generateDocumentation($formulaText);
    }

    /** @inheritDoc */
    public function convertExcelFormula($excelFormula)
    {
        return $this->fallback->convertExcelFormula($excelFormula);
    }

    /** @inheritDoc */
    public function debugFormula($formulaText)
    {
        return $this->fallback->debugFormula($formulaText);
    }

    /**
     * Set the auto-fallback behavior.
     *
     * @param bool $autoFallback
     * @return void
     */
    public function setAutoFallback($autoFallback)
    {
        $this->autoFallback = (bool)$autoFallback;
    }

    // -----------------------------------------------------------------------
    // Prompt Builders — ZERO business data leaves the server
    // -----------------------------------------------------------------------

    /**
     * Build a prompt for natural language → NFX conversion.
     *
     * NOTE: Only formula text, variable names, function names, and
     * business rule descriptions are sent. NO salary data, prices,
     * customer records, or other business data.
     *
     * @param string $description
     * @param string $module
     * @param array  $context
     * @return string
     */
    private function buildNLToFormulaPrompt($description, $module, array $context)
    {
        $varList = '';
        if (isset($context['availableVariables']) && is_array($context['availableVariables'])) {
            $vars = array();
            foreach ($context['availableVariables'] as $v) {
                $vars[] = is_array($v) ? (isset($v['qualifiedName']) ? $v['qualifiedName'] : '') : (string)$v;
            }
            $varList = implode(', ', array_filter($vars));
        }

        $fnList = '';
        if (isset($context['availableFunctions']) && is_array($context['availableFunctions'])) {
            $fns = array();
            foreach ($context['availableFunctions'] as $f) {
                $fns[] = is_array($f) ? (isset($f['name']) ? $f['name'] : '') : (string)$f;
            }
            $fnList = implode(', ', array_filter($fns));
        }

        $rules = '';
        if (isset($context['businessRules']) && is_array($context['businessRules'])) {
            $rules = implode("\n", $context['businessRules']);
        }

        return <<<PROMPT
You are a formula assistant for NotrinosERP. Convert the following natural language
description into a valid NFX formula for the "{$module}" module.

## Description
{$description}

## Available Variables
{$varList}

## Available Functions
{$fnList}

## Business Rules
{$rules}

## Instructions
1. Output ONLY the NFX formula string — no explanations, no markdown.
2. Use the exact variable names listed above.
3. Use the exact function names listed above (case-insensitive).
4. Use standard arithmetic operators: + - * / %
5. For conditional logic, use: IF(condition, true_value, false_value)
6. Ensure parentheses are balanced.
7. Do NOT include any extra text, quotes, or formatting.
PROMPT;
    }

    /**
     * Build a prompt for formula explanation.
     *
     * @param string $formulaText
     * @return string
     */
    private function buildExplainPrompt($formulaText)
    {
        return <<<PROMPT
You are a formula documentation assistant for NotrinosERP. Explain the following
NFX formula in plain, non-technical language suitable for a business user.

## Formula
{$formulaText}

## Instructions
1. Explain what the formula calculates in 2-3 sentences.
2. Break down each component (variables, operators, functions).
3. Describe the business logic in simple terms.
4. Keep the explanation under 150 words.
5. Do NOT use technical jargon.
PROMPT;
    }

    // -----------------------------------------------------------------------
    // API Communication
    // -----------------------------------------------------------------------

    /**
     * Call the external LLM API.
     *
     * @param string   $operation Operation name for logging
     * @param string   $prompt    The formatted prompt
     * @param callable $fallbackFn Fallback function on API failure
     * @return FormulaDesigner_AI_AIResult
     */
    private function callAPI($operation, $prompt, $fallbackFn)
    {
        $start = microtime(true);

        try {
            $response = $this->sendRequest($prompt);
            $result = $this->parseResponse($operation, $response);

            $durationMs = (microtime(true) - $start) * 1000;

            return new FormulaDesigner_AI_AIResult(
                true,
                $result,
                array(),
                'Generated by ' . $this->name . ' (' . $this->model . ').',
                0.85,
                'beginner',
                0,
                $this->name,
                $durationMs
            );
        } catch (Exception $e) {
            if ($this->autoFallback && $fallbackFn !== null) {
                $fallbackResult = call_user_func($fallbackFn);
                if ($fallbackResult instanceof FormulaDesigner_AI_AIResult) {
                    return $fallbackResult;
                }
            }

            return FormulaDesigner_AI_AIResult::failure(
                'API request to ' . $this->name . ' failed: ' . $e->getMessage(),
                $this->name
            );
        }
    }

    /**
     * Send a request to the LLM API.
     *
     * Uses WordPress/NotrinosERP HTTP API when available,
     * falls back to PHP stream context for basic HTTP POST.
     *
     * @param string $prompt
     * @return string Raw API response body
     * @throws RuntimeException On connection failure or HTTP error
     */
    private function sendRequest($prompt)
    {
        $payload = json_encode(array(
            'model'       => $this->model,
            'messages'    => array(
                array('role' => 'user', 'content' => $prompt),
            ),
            'temperature' => $this->temperature,
            'max_tokens'  => $this->maxTokens,
        ));

        if ($payload === false) {
            throw new RuntimeException('Failed to encode API request payload.');
        }

        // Use NotrinosERP HTTP API if available (preferred — handles timeouts, proxy, SSL)
        if (function_exists('http_request')) {
            $response = http_request(
                $this->endpoint,
                'POST',
                $payload,
                array(
                    'Content-Type: application/json',
                    'Authorization: Bearer ' . $this->apiKey,
                ),
                $statusCode
            );

            if ($statusCode < 200 || $statusCode >= 300) {
                throw new RuntimeException(
                    sprintf('API returned HTTP %d: %s', $statusCode, substr($response, 0, 200))
                );
            }

            return $response;
        }

        // Fallback: PHP stream context (basic)
        $context = stream_context_create(array(
            'http' => array(
                'method'  => 'POST',
                'header'  => implode("\r\n", array(
                    'Content-Type: application/json',
                    'Authorization: Bearer ' . $this->apiKey,
                    'Content-Length: ' . strlen($payload),
                )),
                'content' => $payload,
                'timeout' => 30,
            ),
        ));

        $response = @file_get_contents($this->endpoint, false, $context);

        if ($response === false) {
            throw new RuntimeException(
                'Failed to connect to ' . $this->endpoint . '. Check network and API key configuration.'
            );
        }

        return $response;
    }

    /**
     * Parse the LLM API response and extract the formula text.
     *
     * @param string $operation Operation name
     * @param string $response  Raw API response JSON
     * @return string Extracted content
     * @throws RuntimeException On parse failure
     */
    private function parseResponse($operation, $response)
    {
        $data = json_decode($response, true);

        if ($data === null) {
            throw new RuntimeException('Failed to parse API response JSON.');
        }

        if (isset($data['error'])) {
            $msg = isset($data['error']['message']) ? $data['error']['message'] : 'Unknown API error';
            throw new RuntimeException('API error: ' . $msg);
        }

        // OpenAI-style response format
        if (isset($data['choices'][0]['message']['content'])) {
            return trim($data['choices'][0]['message']['content']);
        }

        // Anthropic-style response format
        if (isset($data['content'][0]['text'])) {
            return trim($data['content'][0]['text']);
        }

        throw new RuntimeException('Unexpected API response format. Could not extract content.');
    }

    /**
     * Handle the case where the external provider is not available.
     *
     * @param string $description
     * @param string $module
     * @param array  $context
     * @param string $operation
     * @return FormulaDesigner_AI_AIResult
     */
    private function handleUnavailable($description, $module, array $context, $operation)
    {
        if ($this->autoFallback) {
            return $this->fallback->naturalLanguageToFormula($description, $module, $context);
        }

        return FormulaDesigner_AI_AIResult::failure(
            'External AI provider "' . $this->name . '" is not configured. '
            . 'Set an API key and endpoint, or enable offline mode.',
            $this->name
        );
    }
}
