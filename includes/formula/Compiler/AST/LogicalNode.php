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
 * LogicalNode — AST node for logical operators: AND, OR, XOR.
 *
 * Short-circuit semantics: AND stops if left is FALSE; OR stops if left is TRUE.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_LogicalNode extends Formula_Compiler_AST_Node
{
    public $operator;
    public $left;
    public $right;

    public function __construct($operator, Formula_Compiler_AST_Node $left, Formula_Compiler_AST_Node $right, $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->operator = (string)$operator;
        $this->left     = $left;
        $this->right    = $right;
    }

    /** @return string */
    public function getNodeType()
    {
        return Formula_Compiler_AST_NodeType::LOGICAL;
    }

    public function accept(Formula_Compiler_AST_NodeVisitor $visitor) { return $visitor->visitLogical($this); }
    public function getChildren() { return array($this->left, $this->right); }

    public function serialize()
    {
        $data = parent::serialize();
        $data['operator'] = $this->operator;
        $data['left']     = $this->left->serialize();
        $data['right']    = $this->right->serialize();
        return $data;
    }

    /** @return Formula_Compiler_AST_NodeMetadata */
    protected function computeMetadata()
    {
        $leftMeta  = $this->left->getMetadata();
        $rightMeta = $this->right->getMetadata();
        $metadata = Formula_Compiler_AST_NodeMetadata::leaf('boolean', 3, false, true);
        $metadata->mergeChild($leftMeta);
        $metadata->mergeChild($rightMeta);
        return $metadata;
    }
}
