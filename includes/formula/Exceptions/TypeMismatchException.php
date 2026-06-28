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
 * TypeMismatchException — Incompatible operand types in an operation.
 *
 * Thrown when an operator is applied to incompatible operand types.
 * Examples: "ABC" + 5 (string + number), TRUE / 3 (boolean arithmetic).
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_TypeMismatchException extends Formula_Exceptions_FormulaException
{
    /** @var string The operator that caused the mismatch */
    protected $operator;

    /** @var string The type of the left operand */
    protected $leftType;

    /** @var string The type of the right operand */
    protected $rightType;

    /**
     * @param string $message  Error message
     * @param string $operator The operator (e.g., '+', '/')
     * @param string $leftType The left operand type
     * @param string $rightType The right operand type
     * @param int    $line     Source line
     * @param int    $column   Source column
     */
    public function __construct($message, $operator, $leftType, $rightType, $line = 0, $column = 0)
    {
        parent::__construct($message, $line, $column);
        $this->operator  = (string)$operator;
        $this->leftType  = (string)$leftType;
        $this->rightType = (string)$rightType;
    }

    /**
     * @return string
     */
    public function getOperator()
    {
        return $this->operator;
    }

    /**
     * @return string
     */
    public function getLeftType()
    {
        return $this->leftType;
    }

    /**
     * @return string
     */
    public function getRightType()
    {
        return $this->rightType;
    }
}
