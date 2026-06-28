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
 * FormulaException — Base exception for all Formula Framework errors.
 *
 * All exceptions thrown by the Formula Framework extend this class.
 * It provides source-location tracking (line, column, token, expected
 * token) enabling precise error diagnostic messages for formula authors.
 *
 * The exception hierarchy:
 *   FormulaException
 *   ├── SyntaxErrorException          (lexer/parser errors)
 *   ├── UnknownVariableException      (unresolved variable references)
 *   ├── UnknownFunctionException      (unresolved function calls)
 *   ├── TypeMismatchException         (incompatible operand types)
 *   ├── DivideByZeroException         (division or modulo by zero)
 *   ├── CircularReferenceException    (circular variable dependencies)
 *   ├── PermissionDeniedException     (insufficient security permissions)
 *   ├── ResourceExhaustedException    (time/memory/depth limit exceeded)
 *   └── RuntimeExecutionException     (unexpected runtime errors)
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_FormulaException extends Exception
{
    /** @var int Source line number (1-based) */
    protected $sourceLine;

    /** @var int Source column number (1-based) */
    protected $sourceColumn;

    /** @var string The token found at the error location */
    protected $errorToken;

    /** @var string What was expected at the error location */
    protected $expectedToken;

    /** @var string Optional fix suggestion for the formula author */
    protected $suggestion;

    /**
     * Construct a FormulaException with full source-location context.
     *
     * @param string      $message        Human-readable error description
     * @param int         $line           Source line number (1-based, 0 if unknown)
     * @param int         $column         Source column number (1-based, 0 if unknown)
     * @param string      $token          The token found at the error location
     * @param string      $expectedToken  What was expected
     * @param string      $suggestion     Optional fix suggestion
     * @param int         $code           Exception code
     * @param Throwable   $previous       Previous exception for chaining
     */
    public function __construct(
        $message = '',
        $line = 0,
        $column = 0,
        $token = '',
        $expectedToken = '',
        $suggestion = '',
        $code = 0,
        Throwable $previous = null
    ) {
        parent::__construct($message, (int)$code, $previous);

        $this->sourceLine   = (int)$line;
        $this->sourceColumn = (int)$column;
        $this->errorToken   = (string)$token;
        $this->expectedToken = (string)$expectedToken;
        $this->suggestion   = (string)$suggestion;
    }

    /**
     * Get the source line number where the error occurred.
     *
     * @return int Line number (1-based, 0 if unknown)
     */
    public function getSourceLine()
    {
        return $this->sourceLine;
    }

    /**
     * Get the source column number where the error occurred.
     *
     * @return int Column number (1-based, 0 if unknown)
     */
    public function getSourceColumn()
    {
        return $this->sourceColumn;
    }

    /**
     * Get the token found at the error location.
     *
     * @return string
     */
    public function getErrorToken()
    {
        return $this->errorToken;
    }

    /**
     * Get the expected token description.
     *
     * @return string
     */
    public function getExpectedToken()
    {
        return $this->expectedToken;
    }

    /**
     * Get the optional fix suggestion.
     *
     * @return string
     */
    public function getSuggestion()
    {
        return $this->suggestion;
    }

    /**
     * Get a detailed error string suitable for display to formula authors.
     *
     * @return string Formatted error with location, message, and suggestion
     */
    public function getFormattedMessage()
    {
        $parts = array();

        if ($this->sourceLine > 0) {
            $loc = 'Line ' . $this->sourceLine;
            if ($this->sourceColumn > 0) {
                $loc .= ', Column ' . $this->sourceColumn;
            }
            $parts[] = $loc;
        }

        $parts[] = $this->getMessage();

        if ($this->suggestion !== '') {
            $parts[] = 'Suggestion: ' . $this->suggestion;
        }

        return implode(': ', $parts);
    }

    /**
     * String representation for logging/debugging.
     *
     * @return string
     */
    public function __toString()
    {
        $str = __CLASS__ . ': ' . $this->getFormattedMessage();
        if ($this->getPrevious()) {
            $str .= "\nCaused by: " . $this->getPrevious()->__toString();
        }
        return $str;
    }
}
