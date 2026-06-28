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
 * TokenStream — Navigable wrapper around a token array.
 *
 * Provides push/pop navigation, lookahead, and typed expectation
 * methods for the recursive descent parser. The parser interacts
 * with tokens exclusively through this stream interface.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_TokenStream
{
    /** @var Formula_Compiler_Token[] */
    private $tokens;

    /** @var int Current position */
    private $position;

    /**
     * @param Formula_Compiler_Token[] $tokens
     */
    public function __construct(array $tokens)
    {
        $this->tokens   = $tokens;
        $this->position = 0;
    }

    /**
     * Get the current token without consuming it.
     *
     * @return Formula_Compiler_Token|null
     */
    public function current()
    {
        return isset($this->tokens[$this->position])
            ? $this->tokens[$this->position]
            : null;
    }

    /**
     * Get the previous token (already consumed).
     *
     * @return Formula_Compiler_Token|null
     */
    public function previous()
    {
        return isset($this->tokens[$this->position - 1])
            ? $this->tokens[$this->position - 1]
            : null;
    }

    /**
     * Peek at the next token without consuming.
     *
     * @return Formula_Compiler_Token|null
     */
    public function peek()
    {
        return isset($this->tokens[$this->position + 1])
            ? $this->tokens[$this->position + 1]
            : null;
    }

    /**
     * Peek ahead k tokens without consuming.
     *
     * @param int $k Number of tokens to look ahead (1-based)
     * @return Formula_Compiler_Token|null
     */
    public function peekAhead($k)
    {
        $k = (int)$k;
        if ($k < 1) {
            return $this->current();
        }
        $index = $this->position + $k - 1;
        return isset($this->tokens[$index]) ? $this->tokens[$index] : null;
    }

    /**
     * Consume and return the current token, advancing position.
     *
     * @return Formula_Compiler_Token|null
     */
    public function advance()
    {
        $token = $this->current();
        if ($token !== null) {
            $this->position++;
        }
        return $token;
    }

    /**
     * Check if the current token matches a type, without consuming.
     *
     * @param int $type TokenType constant
     * @return bool
     */
    public function check($type)
    {
        $token = $this->current();
        return $token !== null && $token->type === (int)$type;
    }

    /**
     * If current token matches type, consume it and return true.
     * Otherwise, return false (no error).
     *
     * @param int $type TokenType constant
     * @return bool True if matched and consumed
     */
    public function match($type)
    {
        if ($this->check($type)) {
            $this->advance();
            return true;
        }
        return false;
    }

    /**
     * Consume the current token and assert it matches the expected type.
     *
     * @param int $type TokenType constant
     * @return Formula_Compiler_Token The consumed token
     * @throws Formula_Exceptions_SyntaxErrorException If type doesn't match
     */
    public function expect($type)
    {
        $token = $this->current();

        if ($token === null) {
            throw new Formula_Exceptions_SyntaxErrorException(
                'Unexpected end of formula',
                0,
                0,
                'EOF',
                Formula_Compiler_TokenType::getName($type)
            );
        }

        if ($token->type !== (int)$type) {
            throw new Formula_Exceptions_SyntaxErrorException(
                sprintf(
                    "Unexpected %s '%s'",
                    Formula_Compiler_TokenType::getName($token->type),
                    $token->value
                ),
                $token->line,
                $token->column,
                $token->value,
                Formula_Compiler_TokenType::getName($type),
                sprintf('Expected %s', Formula_Compiler_TokenType::getName($type))
            );
        }

        return $this->advance();
    }

    /**
     * Check if we've reached the end of the token stream.
     *
     * @return bool
     */
    public function isAtEnd()
    {
        return $this->current() === null
            || $this->current()->type === Formula_Compiler_TokenType::T_EOF;
    }

    /**
     * Get the raw token array.
     *
     * @return Formula_Compiler_Token[]
     */
    public function getTokens()
    {
        return $this->tokens;
    }

    /**
     * Get the current position.
     *
     * @return int
     */
    public function getPosition()
    {
        return $this->position;
    }

    /**
     * Reset position to a saved state (for backtracking/error recovery).
     *
     * @param int $position
     * @return void
     */
    public function seek($position)
    {
        $this->position = max(0, min((int)$position, count($this->tokens) - 1));
    }
}
