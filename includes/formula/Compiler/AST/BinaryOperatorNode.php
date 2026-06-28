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
 * BinaryOperatorNode — AST node for binary operators.
 *
 * Represents: +, -, *, /, %, ^, ?? (binary operators with left and right operands).
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_BinaryOperatorNode extends Formula_Compiler_AST_Node
{
    /** @var string Operator: '+', '-', '*', '/', '%', '^', '??' */
    public $operator;

    /** @var Formula_Compiler_AST_Node */
    public $left;

    /** @var Formula_Compiler_AST_Node */
    public $right;

    /**
     * @param string $operator
     * @param Formula_Compiler_AST_Node $left
     * @param Formula_Compiler_AST_Node $right
     * @param int $line
     * @param int $column
     */
    public function __construct($operator, Formula_Compiler_AST_Node $left, Formula_Compiler_AST_Node $right, $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->operator = (string)$operator;
        $this->left     = $left;
        $this->right    = $right;
    }

    /** @return mixed */
    public function accept(Formula_Compiler_AST_NodeVisitor $visitor)
    {
        return $visitor->visitBinary($this);
    }

    /** @return Formula_Compiler_AST_Node[] */
    public function getChildren()
    {
        return array($this->left, $this->right);
    }

    /** @return array */
    public function serialize()
    {
        $data = parent::serialize();
        $data['operator'] = $this->operator;
        $data['left']     = $this->left->serialize();
        $data['right']    = $this->right->serialize();
        return $data;
    }
}
