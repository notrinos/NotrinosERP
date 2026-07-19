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
 * Token — Immutable value object representing a single lexed token.
 *
 * Each token carries its type, the raw source text (lexeme), and precise
 * source location for error reporting. Tokens are immutable after construction.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_Token
{
    /** @var int Token type (from TokenType constants) */
    public $type;

    /** @var string The raw source text (lexeme) */
    public $value;

    /** @var int Source line number (1-based) */
    public $line;

    /** @var int Source column number (1-based) */
    public $column;

    /** @var int Byte offset in the source string */
    public $offset;

    /** @var int Length in bytes of the token in source */
    public $length;

    /**
     * Construct a token.
     *
     * @param int    $type   Token type constant from TokenType
     * @param string $value  The raw lexeme value
     * @param int    $line   Source line (1-based)
     * @param int    $column Source column (1-based)
     * @param int    $offset Byte offset in source
     * @param int    $length Length in bytes
     */
    public function __construct($type, $value, $line, $column, $offset = 0, $length = 0)
    {
        $this->type   = (int)$type;
        $this->value  = (string)$value;
        $this->line   = (int)$line;
        $this->column = (int)$column;
        $this->offset = (int)$offset;
        $this->length = (int)$length;
    }

    /**
     * Check whether this token matches a specific type.
     *
     * @param int $type The TokenType constant to check
     * @return bool
     */
    public function isType($type)
    {
        return $this->type === (int)$type;
    }

    /**
     * Check whether this token is an operator.
     *
     * @return bool
     */
    public function isOperator()
    {
        return in_array($this->type, array(
            Formula_Compiler_TokenType::T_PLUS,
            Formula_Compiler_TokenType::T_MINUS,
            Formula_Compiler_TokenType::T_MULTIPLY,
            Formula_Compiler_TokenType::T_DIVIDE,
            Formula_Compiler_TokenType::T_MODULO,
            Formula_Compiler_TokenType::T_POWER,
            Formula_Compiler_TokenType::T_EQ,
            Formula_Compiler_TokenType::T_NE,
            Formula_Compiler_TokenType::T_LT,
            Formula_Compiler_TokenType::T_LE,
            Formula_Compiler_TokenType::T_GT,
            Formula_Compiler_TokenType::T_GE,
            Formula_Compiler_TokenType::T_AND,
            Formula_Compiler_TokenType::T_OR,
            Formula_Compiler_TokenType::T_NOT,
            Formula_Compiler_TokenType::T_XOR,
            Formula_Compiler_TokenType::T_NULL_COALESCE,
        ));
    }

    /**
     * Check whether this token is a literal value.
     *
     * @return bool
     */
    public function isLiteral()
    {
        return in_array($this->type, array(
            Formula_Compiler_TokenType::T_INTEGER,
            Formula_Compiler_TokenType::T_DECIMAL,
            Formula_Compiler_TokenType::T_STRING,
            Formula_Compiler_TokenType::T_TRUE,
            Formula_Compiler_TokenType::T_FALSE,
            Formula_Compiler_TokenType::T_NULL,
        ));
    }

    /**
     * Check whether this token is a comparison operator.
     *
     * @return bool
     */
    public function isComparison()
    {
        return in_array($this->type, array(
            Formula_Compiler_TokenType::T_EQ,
            Formula_Compiler_TokenType::T_NE,
            Formula_Compiler_TokenType::T_LT,
            Formula_Compiler_TokenType::T_LE,
            Formula_Compiler_TokenType::T_GT,
            Formula_Compiler_TokenType::T_GE,
        ));
    }

    /**
     * Check equality with another token.
     *
     * @param Formula_Compiler_Token $other
     * @return bool
     */
    public function equals(Formula_Compiler_Token $other)
    {
        return $this->type === $other->type
            && $this->value === $other->value
            && $this->line === $other->line
            && $this->column === $other->column;
    }

    /**
     * Convert to array for serialization/caching.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'type'   => $this->type,
            'value'  => $this->value,
            'line'   => $this->line,
            'column' => $this->column,
            'offset' => $this->offset,
            'length' => $this->length,
        );
    }

    /**
     * Create a token from a serialized array.
     *
     * @param array $data
     * @return Formula_Compiler_Token
     */
    public static function fromArray(array $data)
    {
        return new self(
            isset($data['type']) ? $data['type'] : 0,
            isset($data['value']) ? $data['value'] : '',
            isset($data['line']) ? $data['line'] : 0,
            isset($data['column']) ? $data['column'] : 0,
            isset($data['offset']) ? $data['offset'] : 0,
            isset($data['length']) ? $data['length'] : 0
        );
    }

    /**
     * Human-readable representation for debugging.
     *
     * @return string
     */
    public function __toString()
    {
        $typeName = Formula_Compiler_TokenType::getName($this->type);
        return sprintf(
            '%s "%s" at %d:%d',
            $typeName,
            $this->value,
            $this->line,
            $this->column
        );
    }

    // -----------------------------------------------------------------------
    // Factory Methods
    // -----------------------------------------------------------------------

    /**
     * Create an integer literal token.
     *
     * @param int $value The integer value
     * @param int $line  Source line (1-based)
     * @param int $col   Source column (1-based)
     * @param int $offset Byte offset
     * @return self
     */
    public static function integer($value, $line, $col, $offset = 0)
    {
        return new self(
            Formula_Compiler_TokenType::T_INTEGER,
            (string)$value,
            $line,
            $col,
            $offset,
            strlen((string)$value)
        );
    }

    /**
     * Create a decimal literal token.
     *
     * @param float $value The decimal value
     * @param int   $line  Source line (1-based)
     * @param int   $col   Source column (1-based)
     * @param int   $offset Byte offset
     * @return self
     */
    public static function decimal($value, $line, $col, $offset = 0)
    {
        $str = (string)$value;
        return new self(
            Formula_Compiler_TokenType::T_DECIMAL,
            $str,
            $line,
            $col,
            $offset,
            strlen($str)
        );
    }

    /**
     * Create a string literal token.
     *
     * @param string $value  The string value (quotes stripped)
     * @param int    $line   Source line (1-based)
     * @param int    $col    Source column (1-based)
     * @param int    $offset Byte offset
     * @return self
     */
    public static function stringLiteral($value, $line, $col, $offset = 0)
    {
        return new self(
            Formula_Compiler_TokenType::T_STRING,
            (string)$value,
            $line,
            $col,
            $offset,
            strlen((string)$value)
        );
    }

    /**
     * Create an identifier token.
     *
     * @param string $name   The identifier name
     * @param int    $line   Source line (1-based)
     * @param int    $col    Source column (1-based)
     * @param int    $offset Byte offset
     * @return self
     */
    public static function identifier($name, $line, $col, $offset = 0)
    {
        return new self(
            Formula_Compiler_TokenType::T_IDENTIFIER,
            (string)$name,
            $line,
            $col,
            $offset,
            strlen((string)$name)
        );
    }

    /**
     * Create an operator token.
     *
     * @param int    $type   The TokenType constant for the operator
     * @param string $symbol The operator symbol (e.g., '+', '-*')
     * @param int    $line   Source line (1-based)
     * @param int    $col    Source column (1-based)
     * @param int    $offset Byte offset
     * @return self
     */
    public static function operatorSymbol($type, $symbol, $line, $col, $offset = 0)
    {
        return new self(
            $type,
            $symbol,
            $line,
            $col,
            $offset,
            strlen($symbol)
        );
    }

    /**
     * Create a boolean token (TRUE or FALSE).
     *
     * @param bool $value  The boolean value
     * @param int  $line   Source line (1-based)
     * @param int  $col    Source column (1-based)
     * @param int  $offset Byte offset
     * @return self
     */
    public static function booleanLiteral($value, $line, $col, $offset = 0)
    {
        $boolStr = $value ? 'TRUE' : 'FALSE';
        return new self(
            $value ? Formula_Compiler_TokenType::T_TRUE : Formula_Compiler_TokenType::T_FALSE,
            $boolStr,
            $line,
            $col,
            $offset,
            strlen($boolStr)
        );
    }

    /**
     * Create a NULL literal token.
     *
     * @param int $line   Source line (1-based)
     * @param int $col    Source column (1-based)
     * @param int $offset Byte offset
     * @return self
     */
    public static function nullLiteral($line, $col, $offset = 0)
    {
        return new self(
            Formula_Compiler_TokenType::T_NULL,
            'NULL',
            $line,
            $col,
            $offset,
            4
        );
    }

    /**
     * Create an EOF token.
     *
     * @param int $line   Source line (1-based)
     * @param int $col    Source column (1-based)
     * @param int $offset Byte offset
     * @return self
     */
    public static function eof($line, $col, $offset = 0)
    {
        return new self(
            Formula_Compiler_TokenType::T_EOF,
            '',
            $line,
            $col,
            $offset,
            0
        );
    }
}
