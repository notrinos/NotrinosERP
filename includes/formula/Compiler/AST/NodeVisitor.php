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
 * NodeVisitor — Contract for AST traversal (Visitor Pattern).
 *
 * Every operation on the Abstract Syntax Tree (validation, optimization,
 * evaluation, serialization, explanation) is implemented as a visitor.
 * Each visitor implements this interface with one visit method per
 * concrete AST node type.
 *
 * All visit methods accept the typed node and return mixed (the result
 * of visiting that node). Visitors must NOT mutate AST nodes — they
 * produce new values or new nodes.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
interface Formula_Compiler_AST_NodeVisitor
{
    /**
     * Visit a literal node (number, string, boolean).
     *
     * @param Formula_Compiler_AST_LiteralNode $node
     * @return mixed
     */
    public function visitLiteral(Formula_Compiler_AST_LiteralNode $node);

    /**
     * Visit a variable reference node.
     *
     * @param Formula_Compiler_AST_VariableNode $node
     * @return mixed
     */
    public function visitVariable(Formula_Compiler_AST_VariableNode $node);

    /**
     * Visit a function call node.
     *
     * @param Formula_Compiler_AST_FunctionNode $node
     * @return mixed
     */
    public function visitFunction(Formula_Compiler_AST_FunctionNode $node);

    /**
     * Visit a binary operator node (+, -, *, /, ^, AND, OR, ??).
     *
     * @param Formula_Compiler_AST_BinaryOperatorNode $node
     * @return mixed
     */
    public function visitBinary(Formula_Compiler_AST_BinaryOperatorNode $node);

    /**
     * Visit a unary operator node (-, +, NOT).
     *
     * @param Formula_Compiler_AST_UnaryOperatorNode $node
     * @return mixed
     */
    public function visitUnary(Formula_Compiler_AST_UnaryOperatorNode $node);

    /**
     * Visit a conditional node (IF).
     *
     * @param Formula_Compiler_AST_ConditionalNode $node
     * @return mixed
     */
    public function visitConditional(Formula_Compiler_AST_ConditionalNode $node);

    /**
     * Visit a comparison node (>, <, >=, <=, ==, !=, <>).
     *
     * @param Formula_Compiler_AST_ComparisonNode $node
     * @return mixed
     */
    public function visitComparison(Formula_Compiler_AST_ComparisonNode $node);

    /**
     * Visit a logical node (AND, OR, XOR).
     *
     * @param Formula_Compiler_AST_LogicalNode $node
     * @return mixed
     */
    public function visitLogical(Formula_Compiler_AST_LogicalNode $node);

    /**
     * Visit a range node (A1:B10 cell range).
     *
     * @param Formula_Compiler_AST_RangeNode $node
     * @return mixed
     */
    public function visitRange(Formula_Compiler_AST_RangeNode $node);

    /**
     * Visit a null literal node.
     *
     * @param Formula_Compiler_AST_NullNode $node
     * @return mixed
     */
    public function visitNull(Formula_Compiler_AST_NullNode $node);
}
