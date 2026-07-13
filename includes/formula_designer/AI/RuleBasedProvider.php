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
 * RuleBasedProvider — Deterministic, offline AI formula assistance.
 *
 * Provides formula assistance without requiring an external LLM API.
 * Uses pattern matching, keyword extraction, and rule-based templates
 * to convert natural language descriptions into NFX formulas.
 *
 * This is the DEFAULT provider — always available, zero network cost,
 * and keeps all business data on-premise. It handles common payroll,
 * sales, and inventory formula patterns out of the box.
 *
 * @package FormulaDesigner\AI
 * @since   2.0.0
 */
class FormulaDesigner_AI_RuleBasedProvider implements FormulaDesigner_Contracts_IAIProvider
{
    /** @var array Pattern → formula templates (lowercase key → formula) */
    private $patterns;

    /** @var array Excel function name → NFX function name mapping */
    private $excelMap;

    /**
     * Construct the rule-based provider with built-in pattern library.
     */
    public function __construct()
    {
        $this->patterns = $this->buildPatternLibrary();
        $this->excelMap = $this->buildExcelMap();
    }

    /** @return string */
    public function getName()
    {
        return 'RuleBased';
    }

    /** @return bool */
    public function isAvailable()
    {
        return true;
    }

    /**
     * Convert natural language to NFX using keyword extraction.
     *
     * @param string $description
     * @param string $module
     * @param array  $context
     * @return FormulaDesigner_AI_AIResult
     */
    public function naturalLanguageToFormula($description, $module, array $context = array())
    {
        $start = microtime(true);
        $lower = mb_strtolower(trim($description));

        if ($lower === '') {
            return FormulaDesigner_AI_AIResult::failure(
                'No description provided. Please describe the formula in plain language.',
                'RuleBased'
            );
        }

        $bestScore = -1.0;
        $bestFormula = null;
        $bestExplanation = '';

        foreach ($this->patterns as $pattern => $entry) {
            $score = $this->matchScore($lower, $pattern, $entry['keywords']);

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestFormula = $entry['formula'];
                $bestExplanation = $entry['explanation'];
            }
        }

        if ($bestScore < 0.2) {
            return FormulaDesigner_AI_AIResult::failure(
                'Could not determine a formula pattern from the description. '
                . 'Try describing the calculation more specifically, e.g., '
                . '"Calculate net salary by deducting 5% pension from basic salary."',
                'RuleBased'
            );
        }

        $durationMs = (microtime(true) - $start) * 1000;

        return new FormulaDesigner_AI_AIResult(
            true,
            $bestFormula,
            array(),
            $bestExplanation,
            $bestScore,
            $bestScore > 0.7 ? 'beginner' : 'intermediate',
            substr_count($bestFormula, '*') + substr_count($bestFormula, '+') + substr_count($bestFormula, '-') + 2,
            'RuleBased',
            $durationMs
        );
    }

    /** @inheritDoc */
    public function suggestVariables($partialFormula, $module, array $context = array())
    {
        $start = microtime(true);

        $availableVars = isset($context['availableVariables']) ? $context['availableVariables'] : array();

        if (empty($availableVars)) {
            return FormulaDesigner_AI_AIResult::failure(
                'No variables available for module "' . $module . '".',
                'RuleBased'
            );
        }

        $partial = mb_strtolower(trim($partialFormula));
        $suggestions = array();

        foreach ($availableVars as $var) {
            $varName = is_array($var) ? (isset($var['qualifiedName']) ? $var['qualifiedName'] : '') : (string)$var;
            $varLower = mb_strtolower($varName);

            if ($partial === '' || mb_strpos($varLower, $partial) !== false || mb_strpos($partial, $varLower) !== false) {
                $suggestions[] = $varName;
                if (count($suggestions) >= 10) {
                    break;
                }
            }
        }

        $durationMs = (microtime(true) - $start) * 1000;

        return new FormulaDesigner_AI_AIResult(
            true,
            implode(', ', $suggestions),
            $suggestions,
            'Suggested variables matching "' . $partialFormula . '" in module ' . $module . '.',
            count($suggestions) > 0 ? 0.9 : 0.1,
            'beginner',
            0,
            'RuleBased',
            $durationMs
        );
    }

    /** @inheritDoc */
    public function suggestOptimization($formulaText)
    {
        $start = microtime(true);
        $suggestions = array();
        $explanation = '';

        $trimmed = trim($formulaText);

        // Detect common optimizable patterns
        if (preg_match('/\b(\w+)\s*\*\s*1\b/', $trimmed, $m)) {
            $suggestions[] = str_replace('* 1', '', $trimmed);
            $explanation .= 'Removed identity multiplication (' . $m[1] . ' * 1 = ' . $m[1] . ").\n";
        }

        if (preg_match('/\b(\w+)\s*\+\s*0\b/', $trimmed, $m)) {
            $suggestions[] = str_replace('+ 0', '', $trimmed);
            $explanation .= 'Removed zero addition (' . $m[1] . ' + 0 = ' . $m[1] . ").\n";
        }

        if (preg_match('/\b(\w+)\s*\*\s*0\b/', $trimmed, $m)) {
            $suggestions[] = '0';
            $explanation .= 'Detected zero multiplication — result is always 0. Consider removing this term or ensuring the variable is non-zero when needed.' . "\n";
        }

        if (empty($suggestions)) {
            return FormulaDesigner_AI_AIResult::failure(
                'No obvious optimizations found. The formula appears well-structured.',
                'RuleBased'
            );
        }

        $durationMs = (microtime(true) - $start) * 1000;

        return new FormulaDesigner_AI_AIResult(
            true,
            $suggestions[0],
            $suggestions,
            $explanation,
            0.7,
            'intermediate',
            0,
            'RuleBased',
            $durationMs
        );
    }

    /** @inheritDoc */
    public function explainInPlainLanguage($formulaText)
    {
        $trimmed = trim($formulaText);

        if ($trimmed === '') {
            return 'The formula is empty — no calculation is performed.';
        }

        $parts = array();
        $parts[] = 'This formula calculates:';

        // Detect basic patterns
        if (mb_strpos($trimmed, '+') !== false) {
            $parts[] = '- Addition of multiple values';
        }
        if (mb_strpos($trimmed, '-') !== false) {
            $parts[] = '- Subtraction of one value from another';
        }
        if (mb_strpos($trimmed, '*') !== false) {
            $parts[] = '- Multiplication (scaling or rate application)';
        }
        if (mb_strpos($trimmed, '/') !== false) {
            $parts[] = '- Division (proration or ratio calculation)';
        }
        if (preg_match('/\bIF\s*\(/i', $trimmed)) {
            $parts[] = '- Conditional logic (different results based on conditions)';
        }
        if (preg_match('/\bROUND\s*\(/i', $trimmed)) {
            $parts[] = '- Rounding to a specified number of decimal places';
        }
        if (preg_match('/\bMAX\s*\(/i', $trimmed)) {
            $parts[] = '- Uses a maximum value threshold';
        }
        if (preg_match('/\bMIN\s*\(/i', $trimmed)) {
            $parts[] = '- Uses a minimum value threshold';
        }
        if (preg_match('/\bABS\s*\(/i', $trimmed)) {
            $parts[] = '- Uses absolute (positive) values';
        }

        $regex = '/\b([A-Z][A-Za-z]*(?:\.[A-Z][A-Za-z]*)*)\b/';
        if (preg_match_all($regex, $trimmed, $m)) {
            $vars = array_unique($m[1]);
            $filtered = array();
            foreach ($vars as $v) {
                if (
                    !in_array(strtoupper($v), array('IF', 'AND', 'OR', 'NOT', 'ABS', 'ROUND', 'MAX', 'MIN', 'SUM', 'AVG', 'TRUE', 'FALSE'))
                ) {
                    $filtered[] = $v;
                }
            }
            if (!empty($filtered)) {
                $parts[] = '- References variables: ' . implode(', ', $filtered);
            }
        }

        if (count($parts) === 1) {
            $parts[] = '- Simple value or variable reference';
        }

        return implode("\n", $parts);
    }

    /** @inheritDoc */
    public function detectDuplicates($formulaText, $module, array $existingFormulas = array())
    {
        $start = microtime(true);
        $trimmed = trim($formulaText);

        if ($trimmed === '' || empty($existingFormulas)) {
            return FormulaDesigner_AI_AIResult::failure(
                'No existing formulas available for comparison.',
                'RuleBased'
            );
        }

        $normalized = $this->normalizeFormula($trimmed);
        $duplicates = array();

        foreach ($existingFormulas as $existing) {
            $existingNormalized = $this->normalizeFormula(trim($existing));

            if ($normalized === $existingNormalized) {
                $duplicates[] = $existing;
            } elseif ($this->levenshteinSimilarity($normalized, $existingNormalized) > 0.9) {
                $duplicates[] = $existing;
            }
        }

        $durationMs = (microtime(true) - $start) * 1000;

        if (empty($duplicates)) {
            return new FormulaDesigner_AI_AIResult(
                true,
                'No duplicates found.',
                array(),
                'No identical or highly similar formulas were detected.',
                0.95,
                'beginner',
                0,
                'RuleBased',
                $durationMs
            );
        }

        return new FormulaDesigner_AI_AIResult(
            true,
            count($duplicates) . ' similar formula(s) found.',
            $duplicates,
            'Found ' . count($duplicates) . ' formula(s) that are identical or highly similar. '
            . 'Consider reusing an existing formula instead of creating duplicates.',
            0.9,
            'beginner',
            0,
            'RuleBased',
            $durationMs
        );
    }

    /** @inheritDoc */
    public function generateDocumentation($formulaText)
    {
        $trimmed = trim($formulaText);

        if ($trimmed === '') {
            return '## Empty Formula' . "\n\n" . 'No calculation is defined.';
        }

        $doc = "## Formula Documentation\n\n";
        $doc .= "### NFX Expression\n\n```\n" . $trimmed . "\n```\n\n";

        $doc .= "### Plain Language Explanation\n\n";
        $doc .= $this->explainInPlainLanguage($formulaText) . "\n\n";

        $doc .= "### Components\n\n";

        $regex = '/\b([A-Z][A-Za-z]*(?:\.[A-Z][A-Za-z]*)*)\b/';
        if (preg_match_all($regex, $trimmed, $m)) {
            $vars = array_unique($m[1]);
            $known = array('IF', 'AND', 'OR', 'NOT', 'ABS', 'ROUND', 'MAX', 'MIN', 'SUM', 'AVG', 'TRUE', 'FALSE');
            foreach ($vars as $v) {
                if (in_array(strtoupper($v), $known)) {
                    $doc .= "- **" . $this->escape($v) . "()** — Built-in function\n";
                } else {
                    $doc .= "- **" . $this->escape($v) . "** — Variable reference\n";
                }
            }
        }

        $doc .= "\n### Complexity\n\n";
        $ops = substr_count($trimmed, '+') + substr_count($trimmed, '-') + substr_count($trimmed, '*') + substr_count($trimmed, '/');
        $funcs = preg_match_all('/\b(IF|AND|OR|ABS|ROUND|MAX|MIN|SUM|AVG)\s*\(/i', $trimmed, $m2);
        $doc .= "- Operators: " . $ops . "\n";
        $doc .= "- Functions: " . (int)$funcs . "\n";

        if ($ops < 3 && (int)$funcs < 2) {
            $doc .= "- Level: **Beginner**\n";
        } elseif ($ops < 8 && (int)$funcs < 5) {
            $doc .= "- Level: **Intermediate**\n";
        } else {
            $doc .= "- Level: **Advanced**\n";
        }

        return $doc;
    }

    /** @inheritDoc */
    public function convertExcelFormula($excelFormula)
    {
        $start = microtime(true);
        $nfx = trim($excelFormula);

        if ($nfx === '') {
            return FormulaDesigner_AI_AIResult::failure('No Excel formula provided.', 'RuleBased');
        }

        // Strip leading = (Excel convention)
        if (isset($nfx[0]) && $nfx[0] === '=') {
            $nfx = substr($nfx, 1);
        }

        // Map Excel-specific function names
        foreach ($this->excelMap as $excelFn => $nfxFn) {
            $nfx = preg_replace('/\b' . preg_quote($excelFn, '/') . '\s*\(/i', $nfxFn . '(', $nfx);
        }

        // Replace Excel range references (A1:B10) with placeholder
        $nfx = preg_replace('/\b[A-Z]+\d+\s*:\s*[A-Z]+\d+\b/i', '1', $nfx);

        // Replace Excel cell references (A1, B2) with placeholder variable
        $nfx = preg_replace('/\b([A-Z]+\d+)\b/', 'Cell_$1', $nfx);

        $durationMs = (microtime(true) - $start) * 1000;

        return new FormulaDesigner_AI_AIResult(
            true,
            $nfx,
            array(),
            'Converted Excel formula to NFX. Cell references were mapped to variable names (Cell_A1, etc.). '
            . 'Range references were replaced with placeholder values.',
            0.85,
            'beginner',
            0,
            'RuleBased',
            $durationMs
        );
    }

    /** @inheritDoc */
    public function debugFormula($formulaText)
    {
        $start = microtime(true);
        $trimmed = trim($formulaText);
        $issues = array();

        if ($trimmed === '') {
            return FormulaDesigner_AI_AIResult::failure(
                'Empty formula — nothing to debug.',
                'RuleBased'
            );
        }

        // Check for divide-by-zero risks
        if (preg_match('/(\w+)\s*\/\s*(\w+)/', $trimmed, $m)) {
            $issues[] = array(
                'type' => 'warning',
                'message' => 'Potential divide-by-zero risk: ' . $m[1] . ' / ' . $m[2] . '. '
                    . 'Consider wrapping in IF(' . $m[2] . ' != 0, ' . $m[1] . ' / ' . $m[2] . ', 0).',
            );
        }

        // Check for unbalanced parentheses
        $opens = substr_count($trimmed, '(');
        $closes = substr_count($trimmed, ')');
        if ($opens !== $closes) {
            $issues[] = array(
                'type' => 'error',
                'message' => 'Unbalanced parentheses: ' . $opens . ' opening vs ' . $closes . ' closing.',
            );
        }

        // Check for very long formulas
        $length = strlen($trimmed);
        if ($length > 500) {
            $issues[] = array(
                'type' => 'info',
                'message' => 'Formula is ' . $length . ' characters long. Consider breaking into smaller sub-formulas for readability.',
            );
        }

        // Check for nested IFs exceeding 3 levels
        if (substr_count(strtoupper($trimmed), 'IF(') > 3) {
            $issues[] = array(
                'type' => 'info',
                'message' => 'Formula has ' . substr_count(strtoupper($trimmed), 'IF(') . ' nested IF conditions. Consider using a lookup table or SWITCH function.',
            );
        }

        $durationMs = (microtime(true) - $start) * 1000;

        if (empty($issues)) {
            return new FormulaDesigner_AI_AIResult(
                true,
                'No issues found.',
                array(),
                'No structural issues detected in the formula.',
                0.95,
                'beginner',
                0,
                'RuleBased',
                $durationMs
            );
        }

        return new FormulaDesigner_AI_AIResult(
            true,
            count($issues) . ' issue(s) found.',
            $issues,
            'Found ' . count($issues) . ' potential issue(s) to review.',
            0.8,
            'intermediate',
            0,
            'RuleBased',
            $durationMs
        );
    }

    // -----------------------------------------------------------------------
    // Internal Helpers
    // -----------------------------------------------------------------------

    /**
     * Build the keyword-to-formula pattern library.
     *
     * @return array
     */
    private function buildPatternLibrary()
    {
        return array(
            // ---- Payroll / HRM ----
            'salary-net-deduction' => array(
                'keywords'    => array('net', 'salary', 'deduct', 'deduction', 'pension', 'tax', 'basic'),
                'formula'     => 'Employee.BasicSalary - Employee.BasicSalary * 0.05',
                'explanation' => 'Net salary = Basic salary minus 5% pension deduction.',
            ),
            'salary-days-worked' => array(
                'keywords'    => array('salary', 'days', 'worked', 'prorate', 'prorated', 'monthly'),
                'formula'     => 'Employee.BasicSalary * (Payroll.DaysWorked / Payroll.WorkingDays)',
                'explanation' => 'Prorated salary = Basic salary × (Days worked ÷ Total working days).',
            ),
            'overtime-calculation' => array(
                'keywords'    => array('overtime', 'rate', 'hours', 'extra'),
                'formula'     => 'Employee.OvertimeHours * Employee.OvertimeRate',
                'explanation' => 'Overtime pay = Overtime hours × Overtime rate per hour.',
            ),
            'tax-above-threshold' => array(
                'keywords'    => array('tax', 'threshold', 'above', 'income', 'taxable'),
                'formula'     => 'MAX(Employee.TaxableIncome - 5000, 0) * 0.20',
                'explanation' => 'Tax = 20% of taxable income above 5000. Only positive amounts are taxed.',
            ),
            'bonus-percentage' => array(
                'keywords'    => array('bonus', 'percentage', 'percent'),
                'formula'     => 'Employee.BasicSalary * 0.10',
                'explanation' => 'Bonus = 10% of basic salary.',
            ),
            // ---- Sales / Pricing ----
            'discount-tiered' => array(
                'keywords'    => array('discount', 'tier', 'tiered', 'quantity', 'volume'),
                'formula'     => 'IF(Item.Quantity >= 100, Item.Price * 0.8, IF(Item.Quantity >= 50, Item.Price * 0.9, Item.Price))',
                'explanation' => 'Tiered discount: 20% off for 100+ units, 10% off for 50+, full price otherwise.',
            ),
            'commission-percentage' => array(
                'keywords'    => array('commission', 'sales', 'percentage', 'revenue'),
                'formula'     => 'Sales.Revenue * 0.05',
                'explanation' => 'Commission = 5% of sales revenue.',
            ),
            'margin-calculation' => array(
                'keywords'    => array('margin', 'profit', 'cost', 'price', 'selling'),
                'formula'     => '(Item.SellPrice - Item.CostPrice) / Item.SellPrice * 100',
                'explanation' => 'Profit margin percentage = (Sell price − Cost) ÷ Sell price × 100.',
            ),
            // ---- Inventory / Manufacturing ----
            'reorder-level' => array(
                'keywords'    => array('reorder', 'stock', 'level', 'inventory', 'minimum'),
                'formula'     => 'Stock.AvgDailyUsage * Stock.LeadTimeDays + Stock.SafetyStock',
                'explanation' => 'Reorder level = Average daily usage × Lead time (days) + Safety stock buffer.',
            ),
            'landed-cost' => array(
                'keywords'    => array('landed', 'cost', 'freight', 'duty', 'import', 'shipping'),
                'formula'     => 'Item.UnitCost + (Item.FreightCost + Item.DutyCost) / Item.Quantity',
                'explanation' => 'Landed cost per unit = Unit cost + (Freight + Duty) ÷ Quantity.',
            ),
            // ---- General / Utility ----
            'percentage-of' => array(
                'keywords'    => array('percentage', 'of', 'percent'),
                'formula'     => 'Amount * 0.10',
                'explanation' => '10% of the given amount. Adjust the multiplier for different percentages.',
            ),
            'conditional-if' => array(
                'keywords'    => array('if', 'greater', 'than', 'less', 'condition', 'conditional'),
                'formula'     => 'IF(Value > 100, ResultA, ResultB)',
                'explanation' => 'Conditional formula: if Value > 100 use ResultA, otherwise use ResultB.',
            ),
            'round-to-decimals' => array(
                'keywords'    => array('round', 'to', 'decimal', 'places'),
                'formula'     => 'ROUND(Value, 2)',
                'explanation' => 'Rounds Value to 2 decimal places.',
            ),
            'absolute-value' => array(
                'keywords'    => array('absolute', 'abs', 'positive', 'magnitude'),
                'formula'     => 'ABS(Value)',
                'explanation' => 'Returns the absolute (positive) value of the input.',
            ),
        );
    }

    /**
     * Build the Excel function name → NFX function name mapping.
     *
     * @return array
     */
    private function buildExcelMap()
    {
        return array(
            'SUM'       => 'SUM',
            'AVERAGE'   => 'AVG',
            'IF'        => 'IF',
            'IFS'       => 'IFS',
            'AND'       => 'AND',
            'OR'        => 'OR',
            'NOT'       => 'NOT',
            'ABS'       => 'ABS',
            'ROUND'     => 'ROUND',
            'ROUNDUP'   => 'CEIL',
            'ROUNDDOWN' => 'FLOOR',
            'MAX'       => 'MAX',
            'MIN'       => 'MIN',
            'COUNT'     => 'COUNT',
            'COUNTA'    => 'COUNTA',
            'CONCATENATE' => 'CONCAT',
            'LEFT'      => 'LEFT',
            'RIGHT'     => 'RIGHT',
            'MID'       => 'MID',
            'UPPER'     => 'UPPER',
            'LOWER'     => 'LOWER',
            'TRIM'      => 'TRIM',
            'LEN'       => 'LEN',
            'TODAY'     => 'TODAY',
            'NOW'       => 'NOW',
            'YEAR'      => 'YEAR',
            'MONTH'     => 'MONTH',
            'DAY'       => 'DAY',
            'DATE'      => 'DATE',
            'PMT'       => 'PMT',
            'FV'        => 'FV',
            'PV'        => 'PV',
            'NPV'       => 'NPV',
            'SUMIF'     => 'SUMIF',
            'VLOOKUP'   => 'VLOOKUP',
            'HLOOKUP'   => 'HLOOKUP',
        );
    }

    /**
     * Score a natural language description against a pattern.
     *
     * @param string $lowerDescription Lowercase description
     * @param string $patternKey       Pattern identifier
     * @param array  $keywords         Expected keywords
     * @return float Score between 0.0 and 1.0
     */
    private function matchScore($lowerDescription, $patternKey, array $keywords)
    {
        $matchCount = 0;

        foreach ($keywords as $kw) {
            if (mb_strpos($lowerDescription, $kw) !== false) {
                $matchCount++;
            }
        }

        if (empty($keywords)) {
            return 0.0;
        }

        $score = $matchCount / count($keywords);

        // Bonus for pattern key word appearing in description
        $patternWords = explode('-', $patternKey);
        foreach ($patternWords as $pw) {
            if (mb_strpos($lowerDescription, $pw) !== false) {
                $score += 0.1;
            }
        }

        return min(1.0, $score);
    }

    /**
     * Normalize a formula for comparison.
     *
     * @param string $formula
     * @return string
     */
    private function normalizeFormula($formula)
    {
        $normalized = strtoupper(trim($formula));
        $normalized = preg_replace('/\s+/', '', $normalized);
        return $normalized;
    }

    /**
     * Compute Levenshtein similarity between two strings.
     *
     * @param string $a
     * @param string $b
     * @return float Similarity between 0.0 and 1.0
     */
    private function levenshteinSimilarity($a, $b)
    {
        if ($a === '' && $b === '') {
            return 1.0;
        }
        $maxLen = max(strlen($a), strlen($b));
        if ($maxLen === 0) {
            return 1.0;
        }

        $dist = 0;
        $len = strlen($a);
        $lenB = strlen($b);
        $i = 0;

        // Simple character-level comparison
        $minLen = min($len, $lenB);
        for ($i = 0; $i < $minLen; $i++) {
            if ($a[$i] !== $b[$i]) {
                $dist++;
            }
        }
        $dist += abs($len - $lenB);

        return 1.0 - ($dist / $maxLen);
    }

    /**
     * Escape text for Markdown output.
     *
     * @param string $text
     * @return string
     */
    private function escape($text)
    {
        return htmlspecialchars($text, ENT_QUOTES, 'UTF-8');
    }
}
