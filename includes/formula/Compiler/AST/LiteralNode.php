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
 * LiteralNode — AST node for constant literal values.
 *
 * Represents inline constant values: integers (42), decimals (3.14),
 * strings ("Hello"), booleans (TRUE, FALSE). Leaf node — no children.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_LiteralNode extends Formula_Compiler_AST_Node
{
    /** @var mixed The literal value */
    public $value;

    /** @var string The data type: 'integer', 'decimal', 'string', 'boolean' */
    public $dataType;

    /**
     * Construct a literal node.
     *
     * @param mixed  $value    The literal value
     * @param string $dataType The type of literal
     * @param int    $line     Source line
     * @param int    $column   Source column
     */
    public function __construct($value, $dataType = 'decimal', $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->value    = $value;
        $this->dataType = (string)$dataType;
    }

    /**
     * Accept a visitor.
     *
     * @param Formula_Compiler_AST_NodeVisitor $visitor
     * @return mixed
     */
    public function accept(Formula_Compiler_AST_NodeVisitor $visitor)
    {
        return $visitor->visitLiteral($this);
    }

    /**
     * Literals have no children.
     *
     * @return Formula_Compiler_AST_Node[]
     */
    public function getChildren()
    {
        return array();
    }

    /**
     * Serialize for cache.
     *
     * @return array
     */
    public function serialize()
    {
        $data = parent::serialize();
        $data['value']    = $this->value;
        $data['dataType'] = $this->dataType;
        return $data;
    }
}
