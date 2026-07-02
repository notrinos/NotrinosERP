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
 * DesignerEditor — in-memory editor state for the Phase 2 canvas.
 *
 * The editor stores the visual expression model and a dirty flag while the
 * canonical textarea synchronization remains deferred to Phase 4.
 *
 * @package FormulaDesigner\Editor
 * @since   2.0.0
 */
class FormulaDesigner_Editor_DesignerEditor
{
    /** @var array */
    private $expression = array();

    /** @var bool */
    private $dirty = false;

    /** @var string */
    private $formula = '';

    /**
     * @param array|string $expression
     */
    public function __construct($expression = array())
    {
        if (is_string($expression)) {
            $this->formula = trim($expression);
            $this->expression = $this->normalizeExpression(
                $this->deserializeExpression($this->formula)
            );
            return;
        }

        if (empty($expression)) {
            $expression = self::createEmptyExpression();
        }

        $this->expression = $this->normalizeExpression($expression);
        $this->formula = $this->serialize();
    }

    /**
     * Get the normalized expression payload.
     *
     * @return array
     */
    public function getExpression()
    {
        return $this->expression;
    }

    /**
     * Replace the active expression.
     *
     * @param array $expression
     * @return void
     */
    public function setExpression(array $expression)
    {
        $this->expression = $this->normalizeExpression($expression);
        $this->formula = $this->serialize();
        $this->dirty = true;
    }

    /**
     * Get the ordered token list.
     *
     * @return array
     */
    public function getTokens()
    {
        return isset($this->expression['tokens']) ? $this->expression['tokens'] : array();
    }

    /**
     * Replace the token list while keeping the current expression id.
     *
     * @param array $tokens
     * @return void
     */
    public function setTokens(array $tokens)
    {
        $expression = $this->expression;
        $expression['tokens'] = $tokens;

        $this->setExpression($expression);
    }

    /**
     * Get the canonical formula text.
     *
     * @return string
     */
    public function getFormula()
    {
        if ($this->formula === '' && !empty($this->expression['tokens'])) {
            $this->formula = $this->serialize();
        }

        return $this->formula;
    }

    /**
     * Replace the active expression from a formula string.
     *
     * @param string $formula
     * @return void
     */
    public function setFormula($formula)
    {
        $this->formula = trim((string)$formula);
        $expression = $this->deserializeExpression($this->formula);
        $expression['id'] = $this->expression['id'];
        $expression['type'] = $this->expression['type'];
        $this->expression = $this->normalizeExpression($expression);
        $this->dirty = true;
    }

    /**
     * Mark the editor as dirty.
     *
     * @return void
     */
    public function markDirty()
    {
        $this->dirty = true;
    }

    /**
     * Mark the editor as clean.
     *
     * @return void
     */
    public function markClean()
    {
        $this->dirty = false;
    }

    /**
     * Check whether the expression changed since last clean mark.
     *
     * @return bool
     */
    public function isDirty()
    {
        return $this->dirty;
    }

    /**
     * Convert the editor state into a serializable array.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'expression' => $this->expression,
            'formula' => $this->getFormula(),
            'dirty' => $this->dirty,
        );
    }

    /**
     * Serialize the current token list to canonical NFX text.
     *
     * @return string
     */
    public function serialize()
    {
        $output = '';
        $tokens = $this->getTokens();
        $token_count = count($tokens);

        foreach ($tokens as $index => $token) {
            $output .= $this->serializeToken($token, $index, $token_count > ($index + 1) ? $tokens[$index + 1] : null);
        }

        $this->formula = trim($output);

        return $this->formula;
    }

    /**
     * Deserialize a formula string into the designer expression payload.
     *
     * @param string $formula
     * @return array
     */
    public function deserialize($formula)
    {
        return $this->deserializeExpression($formula);
    }

    /**
     * Create an empty expression payload.
     *
     * @return array
     */
    public static function createEmptyExpression()
    {
        return array(
            'id' => 'expr-root',
            'type' => 'expression',
            'tokens' => array(),
        );
    }

    /**
     * Create the roadmap-defined Phase 2 verification expression.
     *
     * @return array
     */
    public static function createPhaseTwoTestExpression()
    {
        return array(
            'id' => 'expr-root',
            'type' => 'expression',
            'tokens' => array(
                array(
                    'id' => 't1',
                    'type' => 'variable',
                    'value' => 'Employee.BasicSalary',
                    'label' => 'Basic Salary',
                    'metadata' => array(
                        'namespace' => 'Employee',
                        'name' => 'BasicSalary',
                        'dataType' => 'number',
                    ),
                ),
                array(
                    'id' => 't2',
                    'type' => 'operator',
                    'value' => '*',
                    'label' => '×',
                ),
                array(
                    'id' => 't3',
                    'type' => 'literal',
                    'value' => '0.12',
                    'label' => '0.12',
                    'metadata' => array(
                        'dataType' => 'number',
                        'rawValue' => 0.12,
                    ),
                ),
                array(
                    'id' => 't4',
                    'type' => 'operator',
                    'value' => '+',
                    'label' => '+',
                ),
                array(
                    'id' => 't5',
                    'type' => 'function',
                    'value' => 'ROUND(',
                    'label' => 'ROUND',
                    'metadata' => array(
                        'name' => 'ROUND',
                        'minArgs' => 1,
                        'maxArgs' => 2,
                    ),
                ),
                array(
                    'id' => 't6',
                    'type' => 'variable',
                    'value' => 'Employee.Overtime',
                    'label' => 'Overtime',
                    'metadata' => array(
                        'namespace' => 'Employee',
                        'name' => 'Overtime',
                        'dataType' => 'number',
                    ),
                ),
                array(
                    'id' => 't7',
                    'type' => 'literal',
                    'value' => '2',
                    'label' => '2',
                    'metadata' => array(
                        'dataType' => 'number',
                        'rawValue' => 2,
                    ),
                ),
                array(
                    'id' => 't8',
                    'type' => 'group',
                    'value' => ')',
                    'label' => '',
                ),
            ),
        );
    }

    /**
     * Normalize the expression payload for predictable rendering.
     *
     * @param array $expression
     * @return array
     */
    private function normalizeExpression(array $expression)
    {
        $normalized = array(
            'id' => isset($expression['id']) ? (string)$expression['id'] : 'expr-root',
            'type' => isset($expression['type']) ? (string)$expression['type'] : 'expression',
            'tokens' => array(),
        );

        $tokens = isset($expression['tokens']) && is_array($expression['tokens'])
            ? $expression['tokens']
            : array();

        foreach ($tokens as $index => $token) {
            if (!is_array($token)) {
                continue;
            }

            $normalized['tokens'][] = $this->normalizeToken($token, $index);
        }

        return $normalized;
    }

    /**
     * Normalize one token entry.
     *
     * @param array $token
     * @param int   $index
     * @return array
     */
    private function normalizeToken(array $token, $index)
    {
        $metadata = isset($token['metadata']) && is_array($token['metadata'])
            ? $token['metadata']
            : array();

        $type = isset($token['type']) ? (string)$token['type'] : 'literal';
        $value = isset($token['value']) ? (string)$token['value'] : '';
        $label = isset($token['label']) ? (string)$token['label'] : '';

        if ($label === '') {
            $label = $this->buildDefaultLabel($type, $value, $metadata);
        }

        return array(
            'id' => isset($token['id']) ? (string)$token['id'] : 'tok-' . (int)$index,
            'type' => $type,
            'value' => $value,
            'label' => $label,
            'metadata' => $metadata,
        );
    }

    /**
     * Convert one token into its canonical formula fragment.
     *
     * @param array      $token
     * @param int        $index
     * @param array|null $next_token
     * @return string
     */
    private function serializeToken(array $token, $index, $next_token = null)
    {
        $type = isset($token['type']) ? (string)$token['type'] : 'literal';
        $value = isset($token['value']) ? (string)$token['value'] : '';
        $metadata = isset($token['metadata']) && is_array($token['metadata']) ? $token['metadata'] : array();

        if ($type === 'function') {
            return $this->normalizeFunctionValue($token);
        }

        if ($type === 'group') {
            return $value !== '' ? $value : ')';
        }

        if ($type === 'literal') {
            return $this->normalizeLiteralValue($token);
        }

        if ($type === 'operator') {
            $operator = strtoupper($value);
            $is_unary = $this->isUnaryOperatorToken($token, $index);

            if ($value === ',') {
                return ', ';
            }

            if ($is_unary) {
                if (ctype_alpha(substr($operator, 0, 1))) {
                    return $operator . ' ';
                }

                return $value;
            }

            if ($next_token !== null && $this->isClosingGroupToken($next_token) && $value === ':') {
                return $value;
            }

            return ' ' . $value . ' ';
        }

        if ($type === 'variable') {
            return $value;
        }

        return $value;
    }

    /**
     * Deserialize a formula string into the expression payload.
     *
     * @param string $formula
     * @return array
     */
    private function deserializeExpression($formula)
    {
        $formula = trim((string)$formula);

        if ($formula === '') {
            return self::createEmptyExpression();
        }

        return array(
            'id' => 'expr-root',
            'type' => 'expression',
            'tokens' => $this->tokenizeFormula($formula),
        );
    }

    /**
     * Tokenize an NFX formula into the designer token model.
     *
     * @param string $formula
     * @return array
     */
    private function tokenizeFormula($formula)
    {
        $length = strlen($formula);
        $position = 0;
        $token_index = 0;
        $tokens = array();

        while ($position < $length) {
            $character = $formula[$position];

            if (preg_match('/\s/', $character)) {
                $position += 1;
                continue;
            }

            if ($character === '"' || $character === "'") {
                $tokens[] = $this->readStringToken($formula, $position, $token_index);
                $token_index += 1;
                continue;
            }

            if (preg_match('/[0-9]/', $character) || ($character === '.' && $position + 1 < $length && preg_match('/[0-9]/', $formula[$position + 1]))) {
                $tokens[] = $this->readNumberToken($formula, $position, $token_index);
                $token_index += 1;
                continue;
            }

            if ($this->isIdentifierStart($character)) {
                $tokens[] = $this->readIdentifierToken($formula, $position, $token_index);
                $token_index += 1;
                continue;
            }

            $operator = $this->matchOperator($formula, $position);
            if ($operator !== null) {
                $tokens[] = $this->buildOperatorToken($operator, $token_index);
                $token_index += 1;
                $position += strlen($operator);
                continue;
            }

            $position += 1;
        }

        return $tokens;
    }

    /**
     * Read a string literal token.
     *
     * @param string $formula
     * @param int    $position
     * @param int    $token_index
     * @return array
     */
    private function readStringToken($formula, &$position, $token_index)
    {
        $quote = $formula[$position];
        $position += 1;
        $length = strlen($formula);
        $value = '';

        while ($position < $length) {
            $character = $formula[$position];
            if ($character === $quote) {
                $position += 1;
                break;
            }

            $value .= $character;
            $position += 1;
        }

        return array(
            'id' => 'tok-' . (int)$token_index,
            'type' => 'literal',
            'value' => $quote . $value . $quote,
            'label' => $value,
            'metadata' => array(
                'dataType' => 'string',
                'rawValue' => $value,
                'quote' => $quote,
            ),
        );
    }

    /**
     * Read a numeric literal token.
     *
     * @param string $formula
     * @param int    $position
     * @param int    $token_index
     * @return array
     */
    private function readNumberToken($formula, &$position, $token_index)
    {
        $length = strlen($formula);
        $value = '';
        $has_decimal = false;

        while ($position < $length) {
            $character = $formula[$position];

            if ($character === '.') {
                if ($has_decimal) {
                    break;
                }
                $has_decimal = true;
                $value .= $character;
                $position += 1;
                continue;
            }

            if (!preg_match('/[0-9]/', $character)) {
                break;
            }

            $value .= $character;
            $position += 1;
        }

        return array(
            'id' => 'tok-' . (int)$token_index,
            'type' => 'literal',
            'value' => $value,
            'label' => $value,
            'metadata' => array(
                'dataType' => 'number',
                'rawValue' => $has_decimal ? (float)$value : (int)$value,
            ),
        );
    }

    /**
     * Read an identifier, variable, function, or keyword token.
     *
     * @param string $formula
     * @param int    $position
     * @param int    $token_index
     * @return array
     */
    private function readIdentifierToken($formula, &$position, $token_index)
    {
        $length = strlen($formula);
        $value = '';

        while ($position < $length) {
            $character = $formula[$position];
            if ($this->isIdentifierPart($character)) {
                $value .= $character;
                $position += 1;
                continue;
            }

            if ($character === '.' && $position + 1 < $length && $this->isIdentifierStart($formula[$position + 1])) {
                $value .= $character;
                $position += 1;
                continue;
            }

            break;
        }

        $keyword = strtoupper($value);
        if (in_array($keyword, array('TRUE', 'FALSE', 'NULL'), true)) {
            return $this->buildKeywordLiteralToken($keyword, $token_index);
        }

        if (in_array($keyword, array('AND', 'OR', 'NOT', 'XOR'), true)) {
            return $this->buildOperatorToken($keyword, $token_index);
        }

        $next_non_whitespace = $this->peekNextNonWhitespaceCharacter($formula, $position);
        if ($next_non_whitespace === '(') {
            $position = $this->consumeWhitespace($formula, $position);
            if ($position < $length && $formula[$position] === '(') {
                $position += 1;
            }

            return array(
                'id' => 'tok-' . (int)$token_index,
                'type' => 'function',
                'value' => $keyword . '(',
                'label' => $keyword,
                'metadata' => array(
                    'name' => $keyword,
                ),
            );
        }

        return array(
            'id' => 'tok-' . (int)$token_index,
            'type' => 'variable',
            'value' => $value,
            'label' => $value,
            'metadata' => $this->buildVariableMetadata($value),
        );
    }

    /**
     * Build a keyword literal token.
     *
     * @param string $keyword
     * @param int    $token_index
     * @return array
     */
    private function buildKeywordLiteralToken($keyword, $token_index)
    {
        $metadata = array();

        if ($keyword === 'NULL') {
            $metadata['dataType'] = 'null';
            $metadata['rawValue'] = null;
        } else {
            $metadata['dataType'] = 'boolean';
            $metadata['rawValue'] = $keyword === 'TRUE';
        }

        return array(
            'id' => 'tok-' . (int)$token_index,
            'type' => 'literal',
            'value' => $keyword,
            'label' => $keyword,
            'metadata' => $metadata,
        );
    }

    /**
     * Build an operator or group token.
     *
     * @param string $operator
     * @param int    $token_index
     * @return array
     */
    private function buildOperatorToken($operator, $token_index)
    {
        if ($operator === '(' || $operator === ')') {
            return array(
                'id' => 'tok-' . (int)$token_index,
                'type' => 'group',
                'value' => $operator,
                'label' => $operator,
                'metadata' => array(),
            );
        }

        $metadata = array();
        if (in_array(strtoupper($operator), array('NOT'), true)) {
            $metadata['operatorKind'] = 'unary';
        }

        if ($operator === ',') {
            $metadata['operatorKind'] = 'separator';
        }

        return array(
            'id' => 'tok-' . (int)$token_index,
            'type' => 'operator',
            'value' => $operator,
            'label' => $operator,
            'metadata' => $metadata,
        );
    }

    /**
     * Match the next operator lexeme.
     *
     * @param string $formula
     * @param int    $position
     * @return string|null
     */
    private function matchOperator($formula, $position)
    {
        foreach (array('??', '<=', '>=', '==', '!=', '<>') as $operator) {
            if (substr($formula, $position, strlen($operator)) === $operator) {
                return $operator;
            }
        }

        $character = isset($formula[$position]) ? $formula[$position] : '';
        if ($character !== '' && strpos('+-*/%^(),:!<>=$', $character) !== false) {
            return $character;
        }

        return null;
    }

    /**
     * Build variable metadata from a qualified name.
     *
     * @param string $value
     * @return array
     */
    private function buildVariableMetadata($value)
    {
        $metadata = array();
        if (strpos($value, '.') !== false) {
            list($namespace, $name) = explode('.', $value, 2);
            $metadata['namespace'] = $namespace;
            $metadata['name'] = $name;
        } else {
            $metadata['name'] = $value;
        }

        return $metadata;
    }

    /**
     * Normalize a literal token back to canonical formula text.
     *
     * @param array $token
     * @return string
     */
    private function normalizeLiteralValue(array $token)
    {
        $value = isset($token['value']) ? (string)$token['value'] : '';
        $metadata = isset($token['metadata']) && is_array($token['metadata']) ? $token['metadata'] : array();
        $data_type = isset($metadata['dataType']) ? strtolower((string)$metadata['dataType']) : '';

        if ($data_type === 'string') {
            $raw_value = isset($metadata['rawValue']) ? (string)$metadata['rawValue'] : $this->stripLiteralQuotes($value);
            $quote = isset($metadata['quote']) && $metadata['quote'] !== '' ? (string)$metadata['quote'] : '"';
            return $quote . $raw_value . $quote;
        }

        if ($value !== '') {
            return $value;
        }

        if (array_key_exists('rawValue', $metadata) && $metadata['rawValue'] === null) {
            return 'NULL';
        }

        return isset($token['label']) ? (string)$token['label'] : '';
    }

    /**
     * Normalize a function token to NAME( form.
     *
     * @param array $token
     * @return string
     */
    private function normalizeFunctionValue(array $token)
    {
        $metadata = isset($token['metadata']) && is_array($token['metadata']) ? $token['metadata'] : array();
        $name = isset($metadata['name']) && $metadata['name'] !== ''
            ? strtoupper((string)$metadata['name'])
            : strtoupper(rtrim(isset($token['value']) ? (string)$token['value'] : '', '('));

        return $name . '(';
    }

    /**
     * Determine whether an operator token is unary in its current position.
     *
     * @param array $token
     * @param int   $index
     * @return bool
     */
    private function isUnaryOperatorToken(array $token, $index)
    {
        $metadata = isset($token['metadata']) && is_array($token['metadata']) ? $token['metadata'] : array();
        if (isset($metadata['operatorKind']) && $metadata['operatorKind'] === 'unary') {
            return true;
        }

        $value = strtoupper(isset($token['value']) ? (string)$token['value'] : '');
        if ($value === 'NOT') {
            return true;
        }

        if ($value !== '+' && $value !== '-') {
            return false;
        }

        if ($index === 0) {
            return true;
        }

        $previous = isset($this->expression['tokens'][$index - 1]) ? $this->expression['tokens'][$index - 1] : null;
        if ($previous === null) {
            return true;
        }

        return $this->isOperatorToken($previous) || $this->isOpeningGroupToken($previous) || $this->isFunctionToken($previous);
    }

    /**
     * Build a fallback label when one is not supplied.
     *
     * @param string $type
     * @param string $value
     * @param array  $metadata
     * @return string
     */
    private function buildDefaultLabel($type, $value, array $metadata)
    {
        if ($type === 'function') {
            return strtoupper(rtrim($value, '('));
        }

        if ($type === 'literal' && isset($metadata['dataType']) && strtolower((string)$metadata['dataType']) === 'string') {
            return isset($metadata['rawValue']) ? (string)$metadata['rawValue'] : $this->stripLiteralQuotes($value);
        }

        return $value;
    }

    /**
     * Remove a wrapping quote pair from a literal value.
     *
     * @param string $value
     * @return string
     */
    private function stripLiteralQuotes($value)
    {
        $length = strlen($value);
        if ($length >= 2) {
            $first = $value[0];
            $last = $value[$length - 1];
            if (($first === '"' || $first === "'") && $last === $first) {
                return substr($value, 1, -1);
            }
        }

        return $value;
    }

    /**
     * Find the next non-whitespace character.
     *
     * @param string $formula
     * @param int    $position
     * @return string|null
     */
    private function peekNextNonWhitespaceCharacter($formula, $position)
    {
        $length = strlen($formula);
        while ($position < $length) {
            if (!preg_match('/\s/', $formula[$position])) {
                return $formula[$position];
            }
            $position += 1;
        }

        return null;
    }

    /**
     * Advance a position to the next non-whitespace character.
     *
     * @param string $formula
     * @param int    $position
     * @return int
     */
    private function consumeWhitespace($formula, $position)
    {
        $length = strlen($formula);
        while ($position < $length && preg_match('/\s/', $formula[$position])) {
            $position += 1;
        }

        return $position;
    }

    /**
     * Check whether a character can start an identifier.
     *
     * @param string $character
     * @return bool
     */
    private function isIdentifierStart($character)
    {
        return (bool)preg_match('/[A-Za-z_]/', $character);
    }

    /**
     * Check whether a character can appear inside an identifier.
     *
     * @param string $character
     * @return bool
     */
    private function isIdentifierPart($character)
    {
        return (bool)preg_match('/[A-Za-z0-9_]/', $character);
    }

    /**
     * Check whether the token is a group token for '('.
     *
     * @param array $token
     * @return bool
     */
    private function isOpeningGroupToken(array $token)
    {
        return isset($token['type'], $token['value']) && $token['type'] === 'group' && $token['value'] === '(';
    }

    /**
     * Check whether the token is a group token for ')'.
     *
     * @param array $token
     * @return bool
     */
    private function isClosingGroupToken(array $token)
    {
        return isset($token['type'], $token['value']) && $token['type'] === 'group' && $token['value'] === ')';
    }

    /**
     * Check whether the token is an operator token.
     *
     * @param array $token
     * @return bool
     */
    private function isOperatorToken(array $token)
    {
        return isset($token['type']) && $token['type'] === 'operator';
    }

    /**
     * Check whether the token is a function token.
     *
     * @param array $token
     * @return bool
     */
    private function isFunctionToken(array $token)
    {
        return isset($token['type']) && $token['type'] === 'function';
    }
}