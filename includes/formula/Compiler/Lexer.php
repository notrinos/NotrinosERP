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
 * Lexer — Character-by-character scanner that converts a formula string
 * into a stream of Token objects.
 *
 * The lexer performs single-pass, left-to-right scanning of the formula
 * source. It is modeled after Spreadsheet_Excel_Writer_Parser::_advance()
 * (reporting/includes/Workbook.php ~line 3310) but enhanced with precise
 * line/column position tracking, keyword recognition, namespace-qualified
 * identifier support, and comprehensive error reporting.
 *
 * ## Token Production
 *
 * The main entry point is tokenize(), which produces an array of
 * Formula_Compiler_Token objects terminated by T_EOF. Each token carries
 * its type, the raw source text (lexeme), and exact source location
 * (line, column, byte offset, and byte length).
 *
 * ## Character Dispatch
 *
 * The scanner dispatches based on the current character:
 *   - Digit (0-9)           → readNumber()
 *   - Quote (" or ')        → readString()
 *   - Letter or underscore  → readIdentifier() / keyword check
 *   - Operator/delimiter    → readOperator()
 *   - Whitespace            → skipped (spaces, tabs, newlines, comments)
 *   - Anything else         → SyntaxErrorException
 *
 * ## Keywords Recognized
 *
 *   AND, OR, NOT, XOR, TRUE, FALSE, NULL
 *
 * Keywords are case-insensitive; the lexer normalizes to uppercase.
 *
 * ## Namespace-Qualified Identifiers
 *
 * When an identifier is followed by a dot and another identifier
 * (e.g., Employee.BasicSalary), the lexer produces a single T_NAMESPACE
 * token containing the full dotted path.
 *
 * ## Comments
 *
 * Line comments (// to end of line) and block comments (/* ... * /)
 * are treated as whitespace — stripped during lexing, never produce
 * tokens.
 *
 * ## Performance Characteristics
 *
 *   - O(n) single-pass scanning
 *   - No backtracking
 *   - Direct string indexing ($source[$position]) for speed
 *   - Token array built incrementally
 *
 * @package Formula\Compiler
 * @since   2.0.0
 * @see     Formula_Compiler_Token
 * @see     Formula_Compiler_TokenType
 */
class Formula_Compiler_Lexer
{
    /** @var string The formula source code */
    private $source;

    /** @var int Current byte position in source (0-based) */
    private $position;

    /** @var int Current line number (1-based) */
    private $line;

    /** @var int Current column number (1-based) */
    private $column;

    /** @var int Total length of the source string */
    private $length;

    /** @var Formula_Compiler_Token[] Accumulated token list */
    private $tokens;

    /** @var int Maximum formula source length */
    private $maxSourceLength;

    /**
     * Construct a lexer for the given formula string.
     *
     * The source is stored as-is. No preprocessing (trim, normalization)
     * is performed — the caller is responsible for any desired preprocessing.
     *
     * @param string $source The raw formula string to tokenize
     */
    public function __construct($source)
    {
        $this->source          = (string)$source;
        $this->position        = 0;
        $this->line            = 1;
        $this->column          = 1;
        $this->length          = strlen($this->source);
        $this->tokens          = array();
        $this->maxSourceLength = defined('FORMULA_MAX_SOURCE_LENGTH')
            ? FORMULA_MAX_SOURCE_LENGTH
            : 10000;
    }

    // -----------------------------------------------------------------------
    //  Public API
    // -----------------------------------------------------------------------

    /**
     * Tokenize the entire formula source into an array of Token objects.
     *
     * The returned array is always terminated by a T_EOF token.
     * An empty or whitespace-only formula produces [T_EOF].
     *
     * @return Formula_Compiler_Token[] Array of tokens, last is always T_EOF
     * @throws Formula_Exceptions_SyntaxErrorException  On unrecognized characters
     * @throws Formula_Exceptions_ResourceExhaustedException If source exceeds max length
     */
    public function tokenize()
    {
        $this->tokens   = array();
        $this->position = 0;
        $this->line     = 1;
        $this->column   = 1;

        // Security: reject oversized formulas before any processing
        if ($this->length > $this->maxSourceLength) {
            throw new Formula_Exceptions_ResourceExhaustedException(
                sprintf(
                    'Formula exceeds maximum length of %d characters (actual: %d)',
                    $this->maxSourceLength,
                    $this->length
                ),
                'source_length',
                $this->maxSourceLength
            );
        }

        // Main scanning loop
        while (!$this->isAtEnd()) {
            $this->skipWhitespace();

            if ($this->isAtEnd()) {
                break;
            }

            $char = $this->peek();
            $startOffset = $this->position;

            // Dispatch based on current character
            if ($this->isDigit($char)) {
                $token = $this->readNumber();
            } elseif ($char === '"' || $char === "'") {
                $token = $this->readString($char);
            } elseif ($this->isAlpha($char) || $char === '_') {
                $token = $this->readIdentifier();
            } else {
                $token = $this->readOperator();
            }

            // Record byte offset and length
            $token->offset = $startOffset;
            $token->length = $this->position - $startOffset;

            $this->tokens[] = $token;
        }

        // Always terminate with EOF
        $this->tokens[] = new Formula_Compiler_Token(
            Formula_Compiler_TokenType::T_EOF,
            '',
            $this->line,
            $this->column,
            $this->position,
            0
        );

        return $this->tokens;
    }

    // -----------------------------------------------------------------------
    //  Character-level helpers
    // -----------------------------------------------------------------------

    /**
     * Look at the current character without consuming it.
     *
     * @return string|null The character at current position, or null if at end
     */
    private function peek()
    {
        if ($this->position >= $this->length) {
            return null;
        }
        return $this->source[$this->position];
    }

    /**
     * Peek at a character at an offset relative to current position.
     *
     * @param int $offset Offset from current position (0 = current, 1 = next)
     * @return string|null The character, or null if beyond source length
     */
    private function peekAhead($offset)
    {
        $index = $this->position + (int)$offset;
        if ($index >= $this->length) {
            return null;
        }
        return $this->source[$index];
    }

    /**
     * Consume and return the current character, advancing position.
     *
     * Updates line and column tracking: \n increments line and resets column;
     * \r\n is handled as a single newline. All other characters increment column.
     *
     * @return string The consumed character
     */
    private function advance()
    {
        if ($this->position >= $this->length) {
            return '';
        }

        $char = $this->source[$this->position];
        $this->position++;

        // Handle Windows-style line endings (\r\n) as a single newline
        if ($char === "\r") {
            // Peek ahead for \n to consume it together
            if ($this->position < $this->length && $this->source[$this->position] === "\n") {
                $this->position++;
            }
            $this->line++;
            $this->column = 1;
        } elseif ($char === "\n") {
            $this->line++;
            $this->column = 1;
        } else {
            $this->column++;
        }

        return $char;
    }

    /**
     * Check whether the scanner has reached the end of the source.
     *
     * @return bool True if position >= source length
     */
    private function isAtEnd()
    {
        return $this->position >= $this->length;
    }

    /**
     * Skip whitespace, including spaces, tabs, newlines, and comments.
     *
     * Comments:
     *   // ... to end of line
     *   /* ... block comment ... *​/
     *
     * This method is called at the start of each token-scanning iteration
     * in the main tokenize() loop.
     *
     * @return void
     */
    private function skipWhitespace()
    {
        while (!$this->isAtEnd()) {
            $char = $this->peek();

            // Standard whitespace
            if ($char === ' ' || $char === "\t" || $char === "\r" || $char === "\n") {
                $this->advance();
                continue;
            }

            // Line comment: // to end of line
            if ($char === '/' && $this->peekAhead(1) === '/') {
                // Consume both slashes
                $this->advance();
                $this->advance();
                // Skip until newline or EOF
                while (!$this->isAtEnd()) {
                    $c = $this->peek();
                    if ($c === "\n" || $c === "\r") {
                        break;
                    }
                    $this->advance();
                }
                continue;
            }

            // Block comment: /* ... */
            if ($char === '/' && $this->peekAhead(1) === '*') {
                // Consume /*
                $this->advance();
                $this->advance();
                // Skip until */ or EOF
                $closed = false;
                while (!$this->isAtEnd()) {
                    $c = $this->peek();
                    if ($c === '*' && $this->peekAhead(1) === '/') {
                        $this->advance(); // consume *
                        $this->advance(); // consume /
                        $closed = true;
                        break;
                    }
                    $this->advance();
                }
                if (!$closed) {
                    throw new Formula_Exceptions_SyntaxErrorException(
                        'Unterminated block comment — expected */',
                        $this->line,
                        $this->column,
                        '/*',
                        '*/'
                    );
                }
                continue;
            }

            // Not whitespace or comment — stop skipping
            break;
        }
    }

    // -----------------------------------------------------------------------
    //  Character classifiers
    // -----------------------------------------------------------------------

    /**
     * Check whether a character is a decimal digit.
     *
     * @param string $char Single character
     * @return bool
     */
    private function isDigit($char)
    {
        return $char >= '0' && $char <= '9';
    }

    /**
     * Check whether a character is a letter (A-Z, a-z).
     *
     * @param string $char Single character
     * @return bool
     */
    private function isAlpha($char)
    {
        return ($char >= 'A' && $char <= 'Z') || ($char >= 'a' && $char <= 'z');
    }

    /**
     * Check whether a character is alphanumeric or underscore.
     *
     * @param string $char Single character
     * @return bool
     */
    private function isAlphaNumeric($char)
    {
        return $this->isAlpha($char) || $this->isDigit($char) || $char === '_';
    }

    // -----------------------------------------------------------------------
    //  Token readers
    // -----------------------------------------------------------------------

    /**
     * Read a numeric literal: integer or decimal.
     *
     * Called when peek() returns a digit (0-9).
     * Parses greedy: consumes all consecutive digits, then optionally
     * a decimal point followed by more digits.
     *
     * Rules:
     *   - Leading digits: [0-9]+
     *   - Optional: \.[0-9]+ for decimal portion
     *   - No scientific notation in v1
     *   - Leading sign (+/-) is NOT consumed here (handled by parser as unary op)
     *   - If decimal point is NOT followed by a digit, stop before the dot
     *     (the dot is a namespace separator, handled by identifier reader)
     *
     * @return Formula_Compiler_Token Either T_INTEGER or T_DECIMAL
     */
    private function readNumber()
    {
        $startLine   = $this->line;
        $startColumn = $this->column;
        $startPos    = $this->position;
        $value       = '';

        // Consume integer part
        while (!$this->isAtEnd() && $this->isDigit($this->peek())) {
            $value .= $this->advance();
        }

        $isDecimal = false;

        // Check for decimal point — only if followed by a digit
        if (!$this->isAtEnd() && $this->peek() === '.') {
            $nextChar = $this->peekAhead(1);
            if ($nextChar !== null && $this->isDigit($nextChar)) {
                $isDecimal = true;
                $value .= $this->advance(); // consume the dot
                // Consume fractional part
                while (!$this->isAtEnd() && $this->isDigit($this->peek())) {
                    $value .= $this->advance();
                }
            }
            // If dot is NOT followed by a digit, leave it for the
            // identifier reader (namespace separator) or operator reader.
        }

        // Determine token type and convert value
        if ($isDecimal) {
            $type       = Formula_Compiler_TokenType::T_DECIMAL;
            $tokenValue = (float)$value;
        } else {
            $type       = Formula_Compiler_TokenType::T_INTEGER;
            $tokenValue = (int)$value;
        }

        return new Formula_Compiler_Token(
            $type,
            (string)$value,     // raw lexeme
            $startLine,
            $startColumn,
            $startPos,
            0                   // set by tokenize() after construction
        );
    }

    /**
     * Read a string literal: double-quoted or single-quoted.
     *
     * Called when peek() returns " or '.
     * Consumes until the matching closing quote is found.
     * Empty strings are valid: "" produces T_STRING with empty value.
     *
     * Edge cases:
     *   - Unterminated string (EOF before closing quote) → SyntaxErrorException
     *   - Multi-line strings are supported (preserve newlines)
     *   - No escape sequences in v1 (literal content only)
     *
     * @param string $quoteChar The opening quote character (" or ')
     * @return Formula_Compiler_Token T_STRING token with the string content
     * @throws Formula_Exceptions_SyntaxErrorException If the string is unterminated
     */
    private function readString($quoteChar)
    {
        $startLine   = $this->line;
        $startColumn = $this->column;
        $startPos    = $this->position;

        // Consume the opening quote
        $this->advance();

        $value = '';

        while (!$this->isAtEnd()) {
            $char = $this->peek();

            // Found closing quote
            if ($char === $quoteChar) {
                $this->advance(); // consume closing quote
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_STRING,
                    $value,
                    $startLine,
                    $startColumn,
                    $startPos,
                    0
                );
            }

            // Consume this character (newlines update line/column via advance())
            $value .= $this->advance();
        }

        // EOF reached without closing quote
        throw new Formula_Exceptions_SyntaxErrorException(
            sprintf(
                'Unterminated string literal — expected closing %s',
                $quoteChar
            ),
            $startLine,
            $startColumn,
            $quoteChar . $value,
            $quoteChar,
            sprintf('Add closing %s at the end of the string', $quoteChar)
        );
    }

    /**
     * Read an identifier, keyword, or namespace-qualified identifier.
     *
     * Called when peek() returns a letter (A-Z, a-z), underscore (_),
     * OR a dollar sign ($) that starts an absolute cell reference.
     *
     * Identifiers: [A-Za-z_][A-Za-z0-9_]*
     *
     * After reading the base identifier, the lexer checks:
     *   1. If the identifier matches a keyword (AND, OR, NOT, XOR, TRUE,
     *      FALSE, NULL), produce the corresponding keyword token.
     *   2. If followed by a dot and another identifier (e.g., Employee.BasicSalary),
     *      continue reading to produce a single T_NAMESPACE token.
     *   3. Otherwise, produce a T_IDENTIFIER token.
     *
     * Cell references like "A1" are matched by the pattern [A-Z]+[0-9]+.
     * If this identifier matches, a T_CELL_REF token is produced.
     *
     * Absolute cell references ($A$1, $A1, A$1) have dollar signs that
     * are consumed as prefix/postfix markers by the parser. The lexer
     * produces T_DOLLAR tokens for each $ sign.
     *
     * @return Formula_Compiler_Token T_IDENTIFIER, T_NAMESPACE, T_CELL_REF,
     *                                or keyword token
     */
    private function readIdentifier()
    {
        $startLine   = $this->line;
        $startColumn = $this->column;
        $startPos    = $this->position;
        $value       = '';

        // Consume the first character (letter or underscore)
        $value .= $this->advance();

        // Consume remaining alphanumeric characters
        while (!$this->isAtEnd() && $this->isAlphaNumeric($this->peek())) {
            $value .= $this->advance();
        }

        // Check for cell reference pattern: one or more letters followed
        // by one or more digits (e.g., A1, AB12). This must NOT be followed
        // by an alphanumeric character (which would make it a longer identifier).
        if ($this->isCellReference($value)) {
            // Ensure it's not part of a longer identifier
            $next = $this->peek();
            if ($next === null || !$this->isAlphaNumeric($next)) {
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_CELL_REF,
                    $value,
                    $startLine,
                    $startColumn,
                    $startPos,
                    0
                );
            }
        }

        // Check for namespace-qualified identifier: Identifier.Identifier...
        // The dot MUST be followed by a letter or underscore.
        // Multiple segments are supported: Employee.BasicSalary, Payroll.Tax.Rate
        $hasNamespace = false;

        while (!$this->isAtEnd() && $this->peek() === '.') {
            $nextChar = $this->peekAhead(1);
            if ($nextChar !== null && ($this->isAlpha($nextChar) || $nextChar === '_')) {
                $hasNamespace = true;
                $value .= $this->advance(); // consume the dot

                // Consume the next identifier segment
                $value .= $this->advance(); // first char of next segment
                while (!$this->isAtEnd() && $this->isAlphaNumeric($this->peek())) {
                    $value .= $this->advance();
                }
            } else {
                // Dot not followed by an identifier — stop here
                break;
            }
        }

        // If we accumulated namespace segments, produce T_NAMESPACE
        if ($hasNamespace) {
            return new Formula_Compiler_Token(
                Formula_Compiler_TokenType::T_NAMESPACE,
                $value,
                $startLine,
                $startColumn,
                $startPos,
                0
            );
        }

        // Check keywords (case-insensitive)
        $upperValue = strtoupper($value);
        $keywordType = $this->getKeywordType($upperValue);

        if ($keywordType !== null) {
            return new Formula_Compiler_Token(
                $keywordType,
                $upperValue,        // normalize keywords to uppercase
                $startLine,
                $startColumn,
                $startPos,
                0
            );
        }

        // Regular identifier
        return new Formula_Compiler_Token(
            Formula_Compiler_TokenType::T_IDENTIFIER,
            $value,                 // preserve original case
            $startLine,
            $startColumn,
            $startPos,
            0
        );
    }

    /**
     * Read an operator, delimiter, or unrecognized character.
     *
     * Called when peek() returns something that is not whitespace, a digit,
     * a letter, an underscore, or a quote.
     *
     * Multi-character operators (>=, <=, !=, <>, ??) are handled by
     * looking ahead before deciding the token type. The longest match
     * always wins.
     *
     * @return Formula_Compiler_Token An operator or delimiter token
     * @throws Formula_Exceptions_SyntaxErrorException For unrecognized characters
     */
    private function readOperator()
    {
        $startLine   = $this->line;
        $startColumn = $this->column;
        $startPos    = $this->position;
        $char        = $this->advance();

        // Multi-character operators — check longest match first
        if ($char === '?' && $this->peek() === '?') {
            $this->advance(); // consume second ?
            return new Formula_Compiler_Token(
                Formula_Compiler_TokenType::T_NULL_COALESCE,
                '??',
                $startLine,
                $startColumn,
                $startPos,
                2
            );
        }

        if ($char === '<') {
            if ($this->peek() === '=') {
                $this->advance();
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_LE,
                    '<=',
                    $startLine,
                    $startColumn,
                    $startPos,
                    2
                );
            }
            if ($this->peek() === '>') {
                $this->advance();
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_NE,
                    '<>',
                    $startLine,
                    $startColumn,
                    $startPos,
                    2
                );
            }
            return new Formula_Compiler_Token(
                Formula_Compiler_TokenType::T_LT,
                '<',
                $startLine,
                $startColumn,
                $startPos,
                1
            );
        }

        if ($char === '>') {
            if ($this->peek() === '=') {
                $this->advance();
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_GE,
                    '>=',
                    $startLine,
                    $startColumn,
                    $startPos,
                    2
                );
            }
            return new Formula_Compiler_Token(
                Formula_Compiler_TokenType::T_GT,
                '>',
                $startLine,
                $startColumn,
                $startPos,
                1
            );
        }

        if ($char === '=' && $this->peek() === '=') {
            $this->advance();
            return new Formula_Compiler_Token(
                Formula_Compiler_TokenType::T_EQ,
                '==',
                $startLine,
                $startColumn,
                $startPos,
                2
            );
        }

        if ($char === '!' && $this->peek() === '=') {
            $this->advance();
            return new Formula_Compiler_Token(
                Formula_Compiler_TokenType::T_NE,
                '!=',
                $startLine,
                $startColumn,
                $startPos,
                2
            );
        }

        // Single-character operators and delimiters
        switch ($char) {
            case '+':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_PLUS, '+',
                    $startLine, $startColumn, $startPos, 1
                );

            case '-':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_MINUS, '-',
                    $startLine, $startColumn, $startPos, 1
                );

            case '*':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_MULTIPLY, '*',
                    $startLine, $startColumn, $startPos, 1
                );

            case '/':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_DIVIDE, '/',
                    $startLine, $startColumn, $startPos, 1
                );

            case '%':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_MODULO, '%',
                    $startLine, $startColumn, $startPos, 1
                );

            case '^':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_POWER, '^',
                    $startLine, $startColumn, $startPos, 1
                );

            case '(':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_LPAREN, '(',
                    $startLine, $startColumn, $startPos, 1
                );

            case ')':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_RPAREN, ')',
                    $startLine, $startColumn, $startPos, 1
                );

            case ',':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_COMMA, ',',
                    $startLine, $startColumn, $startPos, 1
                );

            case ':':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_COLON, ':',
                    $startLine, $startColumn, $startPos, 1
                );

            case '$':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_DOLLAR, '$',
                    $startLine, $startColumn, $startPos, 1
                );

            case '!':
                return new Formula_Compiler_Token(
                    Formula_Compiler_TokenType::T_EXCLAMATION, '!',
                    $startLine, $startColumn, $startPos, 1
                );

            // Standalone = is not a valid NFX operator (no assignment)
            case '=':
                throw new Formula_Exceptions_SyntaxErrorException(
                    "Unexpected '=' — did you mean '==' for comparison?",
                    $startLine,
                    $startColumn,
                    '=',
                    '==',
                    'Use == for equality comparison'
                );

            default:
                // Unrecognized character
                throw new Formula_Exceptions_SyntaxErrorException(
                    sprintf(
                        "Unrecognized character '%s' at line %d, column %d",
                        $char,
                        $startLine,
                        $startColumn
                    ),
                    $startLine,
                    $startColumn,
                    $char,
                    'valid formula character',
                    'Remove this character from the formula'
                );
        }
    }

    // -----------------------------------------------------------------------
    //  Keyword & cell reference helpers
    // -----------------------------------------------------------------------

    /**
     * Map of keyword strings to their TokenType constants.
     *
     * All keywords are case-insensitive; the lookup is performed with
     * the uppercase form of the identifier.
     *
     * @var array<string, int>
     */
    private static $keywords = array(
        'AND'   => null,  // set below
        'OR'    => null,
        'NOT'   => null,
        'XOR'   => null,
        'TRUE'  => null,
        'FALSE' => null,
        'NULL'  => null,
    );

    /**
     * Get the keyword type for a given uppercase identifier.
     *
     * Returns null if the identifier is not a keyword.
     *
     * @param string $upperValue Uppercase identifier value
     * @return int|null TokenType constant or null
     */
    private function getKeywordType($upperValue)
    {
        // Initialize the static keyword map
        if (self::$keywords['AND'] === null) {
            self::$keywords = array(
                'AND'   => Formula_Compiler_TokenType::T_AND,
                'OR'    => Formula_Compiler_TokenType::T_OR,
                'NOT'   => Formula_Compiler_TokenType::T_NOT,
                'XOR'   => Formula_Compiler_TokenType::T_XOR,
                'TRUE'  => Formula_Compiler_TokenType::T_TRUE,
                'FALSE' => Formula_Compiler_TokenType::T_FALSE,
                'NULL'  => Formula_Compiler_TokenType::T_NULL,
            );
        }

        return isset(self::$keywords[$upperValue]) ? self::$keywords[$upperValue] : null;
    }

    /**
     * Check whether a string matches the cell reference pattern.
     *
     * Cell references: one or more UPPERCASE letters followed by
     * one or more digits (e.g., A1, AB12, ZZ999).
     *
     * Per Excel convention, cell references use uppercase column letters.
     * Lowercase letter-digit sequences (e.g., x1, var123) are treated as
     * regular identifiers to avoid ambiguity with variable names.
     *
     * The identifier being tested must have already been consumed
     * by the identifier reader (i.e., it starts with a letter or underscore,
     * followed by alphanumeric characters).
     *
     * @param string $value The identifier value
     * @return bool True if the value matches the cell reference pattern
     */
    private function isCellReference($value)
    {
        $len = strlen($value);
        if ($len < 2) {
            return false;
        }

        // Pattern: one or more uppercase letters followed by one or more digits
        // Must start with an uppercase letter (A-Z only)
        $c0 = $value[0];
        if ($c0 < 'A' || $c0 > 'Z') {
            return false;
        }

        $hasDigit = false;
        $inLetters = true;

        for ($i = 0; $i < $len; $i++) {
            $c = $value[$i];
            if ($inLetters) {
                if ($c >= '0' && $c <= '9') {
                    $inLetters = false;
                    $hasDigit  = true;
                } elseif ($c < 'A' || $c > 'Z') {
                    // Allow only uppercase letters in the letter portion
                    return false;
                }
            } else {
                if ($c < '0' || $c > '9') {
                    return false;
                }
            }
        }

        return $hasDigit;
    }

    // -----------------------------------------------------------------------
    //  Public accessors
    // -----------------------------------------------------------------------

    /**
     * Get the current line number (1-based).
     *
     * @return int
     */
    public function getLine()
    {
        return $this->line;
    }

    /**
     * Get the current column number (1-based).
     *
     * @return int
     */
    public function getColumn()
    {
        return $this->column;
    }

    /**
     * Get the current byte position in the source (0-based).
     *
     * @return int
     */
    public function getPosition()
    {
        return $this->position;
    }

    /**
     * Get the original formula source string.
     *
     * @return string
     */
    public function getSource()
    {
        return $this->source;
    }
}
