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
 * ConditionalNode — AST node for IF(condition, trueBranch, falseBranch).
 *
 * Short-circuit semantics: only the selected branch is evaluated at runtime.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_ConditionalNode extends Formula_Compiler_AST_Node
{
    /** @var Formula_Compiler_AST_Node */
    public $condition;
    /** @var Formula_Compiler_AST_Node */
    public $trueBranch;
    /** @var Formula_Compiler_AST_Node */
    public $falseBranch;

    public function __construct(Formula_Compiler_AST_Node $condition, Formula_Compiler_AST_Node $trueBranch, Formula_Compiler_AST_Node $falseBranch, $line = 0, $column = 0)
    {
        parent::__construct($line, $column);
        $this->condition   = $condition;
        $this->trueBranch  = $trueBranch;
        $this->falseBranch = $falseBranch;
    }

    public function accept(Formula_Compiler_AST_NodeVisitor $visitor) { return $visitor->visitConditional($this); }
    public function getChildren() { return array($this->condition, $this->trueBranch, $this->falseBranch); }

    public function serialize()
    {
        $data = parent::serialize();
        $data['condition']   = $this->condition->serialize();
        $data['trueBranch']  = $this->trueBranch->serialize();
        $data['falseBranch'] = $this->falseBranch->serialize();
        return $data;
    }
}
