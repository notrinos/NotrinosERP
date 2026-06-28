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
 * CircularReferenceException — Circular variable dependency detected.
 *
 * Thrown by the dependency validator when formula variables form a
 * circular reference chain (e.g., A depends on B, B depends on C,
 * C depends on A).
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_CircularReferenceException extends Formula_Exceptions_FormulaException
{
    /** @var string[] The circular reference path (list of variable names) */
    protected $cyclePath;

    /**
     * @param string   $message   Error message
     * @param string[] $cyclePath Ordered list of variable names forming the cycle
     * @param int      $line      Source line
     * @param int      $column    Source column
     */
    public function __construct($message, array $cyclePath, $line = 0, $column = 0)
    {
        parent::__construct($message, $line, $column);
        $this->cyclePath = $cyclePath;
    }

    /**
     * @return string[]
     */
    public function getCyclePath()
    {
        return $this->cyclePath;
    }
}
