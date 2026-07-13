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
 * IAIProvider — Contract for pluggable AI providers.
 *
 * AI providers implement this interface to offer formula assistance
 * capabilities. The Visual Formula Designer Framework supports three
 * provider strategies:
 *
 *  - RuleBasedProvider — Offline, deterministic, no external API.
 *  - ExternalLLMProvider — Cloud LLM API (OpenAI, Anthropic, etc.).
 *  - HybridProvider — Rule-based first, LLM fallback for complex cases.
 *
 * All AI-generated formulas pass through FormulaFacade::validate()
 * before being displayed to the user. No business data leaves the
 * server through AI prompts — only formula text and metadata.
 *
 * @package FormulaDesigner\Contracts
 * @since   2.0.0
 */
interface FormulaDesigner_Contracts_IAIProvider
{
    /**
     * Get the provider name.
     *
     * @return string Human-readable provider name (e.g., 'RuleBased', 'OpenAI GPT-5')
     */
    public function getName();

    /**
     * Check if the provider is available and configured.
     *
     * Rule-based providers always return true. External providers
     * check API key configuration and network connectivity.
     *
     * @return bool
     */
    public function isAvailable();

    /**
     * Convert a natural-language formula description into NFX.
     *
     * @param string $description The natural-language description of the desired formula
     * @param string $module      The module context (e.g., 'hrm', 'sales')
     * @param array  $context     Associative array with keys:
     *                             - availableVariables: array of qualified variable names
     *                             - availableFunctions: array of function names
     *                             - businessRules: array of string descriptions
     * @return FormulaDesigner_AI_AIResult The AI result with candidate formulas
     */
    public function naturalLanguageToFormula($description, $module, array $context = array());

    /**
     * Suggest variable references for a partial formula.
     *
     * @param string $partialFormula The partial formula text typed so far
     * @param string $module         The module context
     * @param array  $context        Available variables and metadata
     * @return FormulaDesigner_AI_AIResult
     */
    public function suggestVariables($partialFormula, $module, array $context = array());

    /**
     * Suggest formula optimizations.
     *
     * Analyzes an existing formula and suggests improvements:
     * constant folding, algebraic simplification, dead code elimination.
     *
     * @param string $formulaText The full formula text to analyze
     * @return FormulaDesigner_AI_AIResult
     */
    public function suggestOptimization($formulaText);

    /**
     * Explain a formula in plain, non-technical language.
     *
     * @param string $formulaText The formula to explain
     * @return string Plain-language explanation
     */
    public function explainInPlainLanguage($formulaText);

    /**
     * Detect duplicate or similar formulas within a module.
     *
     * @param string $formulaText  The formula to check
     * @param string $module       The module context
     * @param array  $existingFormulas Array of existing formula strings
     * @return FormulaDesigner_AI_AIResult
     */
    public function detectDuplicates($formulaText, $module, array $existingFormulas = array());

    /**
     * Generate human-readable documentation for a formula.
     *
     * Produces a structured description including variable explanations,
     * function references, business logic breakdown, and example usage.
     *
     * @param string $formulaText The formula to document
     * @return string Markdown-formatted documentation
     */
    public function generateDocumentation($formulaText);

    /**
     * Convert an Excel formula string to NFX.
     *
     * Handles Excel function name mapping (e.g., SUMIF → NFX equivalent),
     * range conversion, and syntax differences.
     *
     * @param string $excelFormula The Excel formula to convert
     * @return FormulaDesigner_AI_AIResult
     */
    public function convertExcelFormula($excelFormula);

    /**
     * Analyze a formula and provide debugging hints.
     *
     * Identifies potential issues: type mismatches, divide-by-zero risks,
     * circular references, deprecated function usage, and missing variables.
     *
     * @param string $formulaText The formula to debug
     * @return FormulaDesigner_AI_AIResult
     */
    public function debugFormula($formulaText);
}
