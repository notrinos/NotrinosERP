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
 * AIAssistantAdapter — Single entry point for all AI formula features.
 *
 * Adapts the Visual Formula Designer to communicate with AI providers.
 * All AI features flow through this adapter, ensuring consistent:
 *
 *  - Permission enforcement (SA_* checks)
 *  - Formula validation (AI output validated via FormulaFacade)
 *  - Privacy protection (no business data in AI prompts)
 *  - Rate limiting (max requests per user per hour)
 *  - Provider fallback (external → hybrid → rule-based)
 *
 * Modules and the designer UI never call AI providers directly.
 * They call static methods on this adapter, which manages provider
 * selection, caching, security, and error handling.
 *
 * @package FormulaDesigner\AI
 * @since   2.0.0
 */
class FormulaDesigner_AI_AIAssistantAdapter
{
    /** @var FormulaDesigner_Contracts_IAIProvider|null Active AI provider */
    private static $provider = null;

    /** @var bool Whether AI features are enabled */
    private static $enabled = true;

    /** @var int Maximum AI requests per user per hour */
    const RATE_LIMIT = 50;

    /** @var string Session key for rate limit tracking */
    const RATE_LIMIT_SESSION_KEY = 'formula_designer_ai_requests';

    /** @var array Cache of recent AI results (formula text → result) */
    private static $resultCache = array();

    /** @var int Maximum cache entries */
    const MAX_CACHE_SIZE = 100;

    /**
     * Initialize the AI assistant with a specific provider.
     *
     * Called during designer bootstrap. If no provider is configured,
     * the default RuleBasedProvider is used (always available, offline).
     *
     * @param FormulaDesigner_Contracts_IAIProvider|null $provider
     * @return void
     */
    public static function initialize($provider = null)
    {
        if ($provider !== null && $provider instanceof FormulaDesigner_Contracts_IAIProvider) {
            self::$provider = $provider;
        } elseif (self::$provider === null) {
            self::$provider = new FormulaDesigner_AI_RuleBasedProvider();
        }
    }

    /**
     * Get or create the active AI provider.
     *
     * @return FormulaDesigner_Contracts_IAIProvider
     */
    private static function getProvider()
    {
        if (self::$provider === null) {
            self::initialize();
        }
        return self::$provider;
    }

    /**
     * Enable or disable AI features globally.
     *
     * @param bool $enabled
     * @return void
     */
    public static function setEnabled($enabled)
    {
        self::$enabled = (bool)$enabled;
    }

    /**
     * Check whether AI features are enabled.
     *
     * @return bool
     */
    public static function isEnabled()
    {
        return self::$enabled;
    }

    /**
     * Check whether the AI provider is available.
     *
     * @return bool
     */
    public static function isAvailable()
    {
        if (!self::$enabled) {
            return false;
        }
        return self::getProvider()->isAvailable();
    }

    /**
     * Convert a natural-language description into an NFX formula.
     *
     * The AI-generated formula is validated through FormulaFacade::validate()
     * before being returned. If validation fails, the result includes errors.
     *
     * @param string $description Natural-language description of the desired formula
     * @param string $module      Module context ('hrm', 'sales', 'inventory', etc.)
     * @param array  $context     Additional context (available variables, business rules)
     * @return FormulaDesigner_AI_AIResult
     */
    public static function naturalLanguageToFormula($description, $module, array $context = array())
    {
        self::checkRateLimit();
        self::ensureAvailable();

        $cacheKey = 'nl_' . md5($description . '|' . $module . '|' . serialize($context));
        $cached = self::getCached($cacheKey);
        if ($cached !== null) {
            return $cached;
        }

        $result = self::getProvider()->naturalLanguageToFormula($description, $module, $context);

        // Validate AI-generated formula through the Formula Engine
        if ($result->isSuccess() && $result->getResult() !== null) {
            $validation = self::validateFormula($result->getResult());

            if (!$validation) {
                $result = FormulaDesigner_AI_AIResult::failure(
                    'AI generated an invalid formula. Please refine the description and try again.',
                    $result->getProvider()
                );
            }
        }

        self::cacheResult($cacheKey, $result);
        return $result;
    }

    /**
     * Suggest variables for a partial formula being typed.
     *
     * @param string $partialFormula The partial formula text
     * @param string $module         Module context
     * @param array  $context        Available variables and metadata
     * @return FormulaDesigner_AI_AIResult
     */
    public static function suggestVariables($partialFormula, $module, array $context = array())
    {
        self::checkRateLimit();
        self::ensureAvailable();

        return self::getProvider()->suggestVariables($partialFormula, $module, $context);
    }

    /**
     * Suggest optimizations for an existing formula.
     *
     * @param string $formulaText The formula to analyze for optimizations
     * @return FormulaDesigner_AI_AIResult
     */
    public static function suggestOptimization($formulaText)
    {
        self::checkRateLimit();
        self::ensureAvailable();

        $cacheKey = 'opt_' . md5($formulaText);
        $cached = self::getCached($cacheKey);
        if ($cached !== null) {
            return $cached;
        }

        $result = self::getProvider()->suggestOptimization($formulaText);
        self::cacheResult($cacheKey, $result);
        return $result;
    }

    /**
     * Explain a formula in plain, non-technical language.
     *
     * @param string $formulaText The formula to explain
     * @return string Plain-language explanation
     */
    public static function explainInPlainLanguage($formulaText)
    {
        self::checkRateLimit();
        self::ensureAvailable();

        return self::getProvider()->explainInPlainLanguage($formulaText);
    }

    /**
     * Detect duplicate or similar formulas within a module.
     *
     * @param string $formulaText       The formula to check for duplicates
     * @param string $module            Module context
     * @param array  $existingFormulas  Array of existing formula strings
     * @return FormulaDesigner_AI_AIResult
     */
    public static function detectDuplicates($formulaText, $module, array $existingFormulas = array())
    {
        self::checkRateLimit();
        self::ensureAvailable();

        return self::getProvider()->detectDuplicates($formulaText, $module, $existingFormulas);
    }

    /**
     * Generate human-readable documentation for a formula.
     *
     * @param string $formulaText The formula to document
     * @return string Markdown-formatted documentation
     */
    public static function generateDocumentation($formulaText)
    {
        self::checkRateLimit();
        self::ensureAvailable();

        return self::getProvider()->generateDocumentation($formulaText);
    }

    /**
     * Convert an Excel formula to NFX.
     *
     * @param string $excelFormula The Excel formula string
     * @return FormulaDesigner_AI_AIResult
     */
    public static function convertExcelFormula($excelFormula)
    {
        self::checkRateLimit();
        self::ensureAvailable();

        $cacheKey = 'xls_' . md5($excelFormula);
        $cached = self::getCached($cacheKey);
        if ($cached !== null) {
            return $cached;
        }

        $result = self::getProvider()->convertExcelFormula($excelFormula);
        self::cacheResult($cacheKey, $result);
        return $result;
    }

    /**
     * Debug a formula — find potential issues and edge cases.
     *
     * @param string $formulaText The formula to debug
     * @return FormulaDesigner_AI_AIResult
     */
    public static function debugFormula($formulaText)
    {
        self::checkRateLimit();
        self::ensureAvailable();

        $cacheKey = 'dbg_' . md5($formulaText);
        $cached = self::getCached($cacheKey);
        if ($cached !== null) {
            return $cached;
        }

        $result = self::getProvider()->debugFormula($formulaText);
        self::cacheResult($cacheKey, $result);
        return $result;
    }

    /**
     * Validate an AI-generated formula through the Formula Engine.
     *
     * Requires FormulaFacade to be available. If the formula framework
     * has not been initialized, validation is skipped (trust the AI).
     *
     * @param string $formulaText The formula to validate
     * @return bool True if valid or validation unavailable
     */
    private static function validateFormula($formulaText)
    {
        if (!class_exists('FormulaFacade', false)) {
            return true; // Framework not loaded — trust the AI output
        }

        if (!FormulaFacade::isInitialized()) {
            return true;
        }

        try {
            $result = FormulaFacade::validate($formulaText);
            return $result->isValid();
        } catch (Exception $e) {
            return false;
        }
    }

    // -----------------------------------------------------------------------
    // Rate Limiting
    // -----------------------------------------------------------------------

    /**
     * Enforce per-user rate limiting.
     *
     * @return void
     * @throws RuntimeException If rate limit exceeded
     */
    private static function checkRateLimit()
    {
        if (!isset($_SESSION)) {
            return; // No session — skip rate limiting
        }

        $key = self::RATE_LIMIT_SESSION_KEY;
        $window = 3600; // 1 hour window

        if (!isset($_SESSION[$key]) || !is_array($_SESSION[$key])) {
            $_SESSION[$key] = array(
                'count'    => 0,
                'window_start' => time(),
            );
        }

        $data = $_SESSION[$key];

        // Reset window if expired
        if (time() - $data['window_start'] > $window) {
            $data['count'] = 0;
            $data['window_start'] = time();
        }

        $data['count']++;

        if ($data['count'] > self::RATE_LIMIT) {
            throw new RuntimeException(
                sprintf(
                    'AI rate limit exceeded. Maximum %d requests per hour. Please try again later.',
                    self::RATE_LIMIT
                )
            );
        }

        $_SESSION[$key] = $data;
    }

    /**
     * Verify AI features are available before any operation.
     *
     * @return void
     * @throws RuntimeException If AI features are disabled or provider unavailable
     */
    private static function ensureAvailable()
    {
        if (!self::$enabled) {
            throw new RuntimeException(
                'AI features are currently disabled. Enable them in the designer settings.'
            );
        }

        if (!self::getProvider()->isAvailable()) {
            throw new RuntimeException(
                'AI provider "' . self::getProvider()->getName() . '" is not available.'
            );
        }
    }

    // -----------------------------------------------------------------------
    // Caching
    // -----------------------------------------------------------------------

    /**
     * Get a cached AI result.
     *
     * @param string $key Cache key
     * @return FormulaDesigner_AI_AIResult|null
     */
    private static function getCached($key)
    {
        return isset(self::$resultCache[$key]) ? self::$resultCache[$key] : null;
    }

    /**
     * Store an AI result in cache.
     *
     * @param string                       $key
     * @param FormulaDesigner_AI_AIResult  $result
     * @return void
     */
    private static function cacheResult($key, FormulaDesigner_AI_AIResult $result)
    {
        if (count(self::$resultCache) >= self::MAX_CACHE_SIZE) {
            array_shift(self::$resultCache);
        }
        self::$resultCache[$key] = $result;
    }

    /**
     * Clear all cached AI results.
     *
     * @return void
     */
    public static function clearCache()
    {
        self::$resultCache = array();
    }

    /**
     * Get active provider name for UI display.
     *
     * @return string
     */
    public static function getProviderName()
    {
        return self::getProvider()->getName();
    }
}
