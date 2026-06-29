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
 * UnaryOperatorNode — AST node for unary operators: -, +, NOT.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_UnaryOperatorNode extends Formula_Compiler_AST_Node
{
    /** @var string Operator: '-', '+', 'NOT' */
    public $operator;

    /** @var Formula_Compiler_AST_Node */
    public $operand;

    public function __construct($operator, Formula_Compiler_AST_Node $operand, $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->operator = (string)$operator;
        $this->operand  = $operand;
    }

    /** @return string */
    public function getNodeType()
    {
        return Formula_Compiler_AST_NodeType::UNARY_OP;
    }

    public function accept(Formula_Compiler_AST_NodeVisitor $visitor) { return $visitor->visitUnary($this); }
    public function getChildren() { return array($this->operand); }

    public function serialize()
    {
        $data = parent::serialize();
        $data['operator'] = $this->operator;
        $data['operand']  = $this->operand->serialize();
        return $data;
    }

    /** @return Formula_Compiler_AST_NodeMetadata */
    protected function computeMetadata()
    {
        $childMeta = $this->operand->getMetadata();
        $type = $this->operator === 'NOT' ? 'boolean' : $childMeta->returnType;
        $metadata = Formula_Compiler_AST_NodeMetadata::leaf($type, 2, false, $childMeta->isDeterministic);
        $metadata->mergeChild($childMeta);
        return $metadata;
    }
}
