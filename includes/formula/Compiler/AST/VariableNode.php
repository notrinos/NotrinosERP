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
     * @return string The NodeType constant for this node class
     */
    public function getNodeType()
    {
        return Formula_Compiler_AST_NodeType::VARIABLE;
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
     * Serialize for cache storage.
     *
     * @return array
     */
    public function serialize()
    {
        $data = parent::serialize();
        $data['namespace']     = $this->namespace;
        $data['identifier']    = $this->identifier;
        $data['qualifiedName'] = $this->getQualifiedName();
        return $data;
    }

    /**
     * Compute per-node metadata: variables are non-constant but deterministic.
     *
     * @return Formula_Compiler_AST_NodeMetadata
     */
    protected function computeMetadata()
    {
        $name = $this->getQualifiedName();
        $metadata = Formula_Compiler_AST_NodeMetadata::leaf(
            'mixed', // Type unknown at compile time
            2,        // Complexity: 2 (requires runtime resolution)
            false,    // NOT constant-foldable
            true      // Deterministic (same context → same value)
        );
        $metadata->referencedVariables = array($name);
        if ($this->namespace !== '') {
            $metadata->referencedNamespaces = array($this->namespace);
        }
        return $metadata;
    }
}
