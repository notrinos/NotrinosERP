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
 * VariableNode — AST node for variable references.
 *
 * Represents a variable reference in a formula. Variables can be
 * simple (e.g., "Salary") or namespace-qualified (e.g., "Employee.BasicSalary").
 * Resolution is performed lazily at runtime through registered
 * Variable Providers.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_VariableNode extends Formula_Compiler_AST_Node
{
    /** @var string Variable namespace (e.g., 'Employee'), or empty for simple variables */
    public $namespace;

    /** @var string Variable identifier (e.g., 'BasicSalary', 'Salary') */
    public $identifier;

    /**
     * Construct a variable node.
     *
     * @param string $namespace  The namespace ('' for simple variables)
     * @param string $identifier The variable name
     * @param int    $line       Source line
     * @param int    $column     Source column
     */
    public function __construct($namespace, $identifier, $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->namespace  = (string)$namespace;
        $this->identifier = (string)$identifier;
    }

    /**
     * Accept a visitor.
     *
     * @param Formula_Compiler_AST_NodeVisitor $visitor
     * @return mixed
     */
    public function accept(Formula_Compiler_AST_NodeVisitor $visitor)
    {
        return $visitor->visitVariable($this);
    }

    /**
     * Variable nodes have no children.
     *
     * @return Formula_Compiler_AST_Node[]
     */
    public function getChildren()
    {
        return array();
    }

    /**
     * Get the fully qualified variable name.
     *
     * @return string e.g., "Employee.BasicSalary" or "Salary"
     */
    public function getQualifiedName()
    {
        if ($this->namespace !== '') {
            return $this->namespace . '.' . $this->identifier;
        }
        return $this->identifier;
    }

    /**
     * Serialize for cache.
     *
     * @return array
     */
    public function serialize()
    {
        $data = parent::serialize();
        $data['namespace']  = $this->namespace;
        $data['identifier'] = $this->identifier;
        return $data;
    }
}
