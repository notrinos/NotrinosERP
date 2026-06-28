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
 * NullNode — AST node representing the NULL literal.
 *
 * The simplest AST node. Represents the NULL keyword in formulas,
 * and is also used as the root node for empty formula strings
 * (which evaluate to 0.0 for backward compatibility with the
 * legacy payroll_formula_engine).
 *
 * This is a leaf node with no children.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_NullNode extends Formula_Compiler_AST_Node
{
    /**
     * Construct a NULL node.
     *
     * @param int $line   Source line (1-based)
     * @param int $column Source column (1-based)
     */
    public function __construct($line = 0, $column = 0)
    {
        parent::__construct($line, $column);
    }

    /**
     * Accept a visitor.
     *
     * @param Formula_Compiler_AST_NodeVisitor $visitor
     * @return mixed
     */
    public function accept(Formula_Compiler_AST_NodeVisitor $visitor)
    {
        return $visitor->visitNull($this);
    }

    /**
     * NULL has no children.
     *
     * @return Formula_Compiler_AST_Node[]
     */
    public function getChildren()
    {
        return array();
    }

    /**
     * Serialize to array for cache storage.
     *
     * @return array
     */
    public function serialize()
    {
        $data = parent::serialize();
        $data['value'] = null;
        return $data;
    }

    /**
     * Human-readable representation.
     *
     * @return string
     */
    public function __toString()
    {
        return 'NULL';
    }
}
