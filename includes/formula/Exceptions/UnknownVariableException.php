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
 * UnknownVariableException — Unresolved variable reference.
 *
 * Thrown when a formula references a variable namespace or identifier
 * that has no registered VariableProvider.
 *
 * @package Formula\Exceptions
 * @since   2.0.0
 */
class Formula_Exceptions_UnknownVariableException extends Formula_Exceptions_FormulaException
{
    /** @var string The unresolvable namespace */
    protected $varNamespace;

    /** @var string The unresolvable identifier */
    protected $identifier;

    /**
     * Construct with namespace and identifier context.
     *
     * @param string      $message   Error message
     * @param string      $namespace The unresolvable namespace (empty for simple variables)
     * @param string      $identifier The unresolvable identifier
     * @param int         $line      Source line
     * @param int         $column    Source column
     */
    public function __construct($message, $namespace, $identifier, $line = 0, $column = 0)
    {
        parent::__construct($message, $line, $column);
        $this->varNamespace = (string)$namespace;
        $this->identifier   = (string)$identifier;
    }

    /**
     * @return string
     */
    public function getNamespace()
    {
        return $this->varNamespace;
    }

    /**
     * @return string
     */
    public function getIdentifier()
    {
        return $this->identifier;
    }
}
