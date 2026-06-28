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
 * UnknownFunctionException — Function not found in registry.
 *
 * Thrown when a formula calls a function name that is not registered
 * in the FunctionRegistry. The function name is captured for diagnostic
 * messages and "did you mean?" suggestions.
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_UnknownFunctionException extends Formula_Exceptions_FormulaException
{
    /** @var string The unresolvable function name */
    protected $functionName;

    /**
     * Construct with function name context.
     *
     * @param string $message      Error message
     * @param string $functionName The unresolvable function name
     * @param int    $line         Source line
     * @param int    $column       Source column
     */
    public function __construct($message, $functionName, $line = 0, $column = 0)
    {
        parent::__construct($message, $line, $column);
        $this->functionName = (string)$functionName;
    }

    /**
     * @return string
     */
    public function getFunctionName()
    {
        return $this->functionName;
    }
}
