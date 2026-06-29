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
 * NodeType — String constants identifying AST node types.
 *
 * Every AST node carries a type identifier for:
 *   - Serialization/deserialization (type discriminator)
 *   - Visitor dispatch verification
 *   - Metadata computation (different node types contribute differently)
 *   - Debugging and diagnostics
 *
 * Types are string constants (not integers) for human-readable
 * serialized output. These match the type keys in serialized AST
 * JSON as described in Architecture Volume 7, Section 7.7.
 *
 * @package Formula\Compiler\AST
 * @since   2.0.0
 */
class Formula_Compiler_AST_NodeType
{
    /** Constant literal: integer, decimal, string, or boolean */
    const LITERAL     = 'literal';

    /** Variable reference (simple or namespace-qualified) */
    const VARIABLE    = 'variable';

    /** Function call with argument list */
    const FUNCTION    = 'function';

    /** Binary operator: +, -, *, /, %, ^, ?? */
    const BINARY_OP   = 'binary';

    /** Unary operator: -, +, NOT */
    const UNARY_OP    = 'unary';

    /** Conditional: IF(condition, trueBranch, falseBranch) */
    const CONDITIONAL = 'conditional';

    /** Comparison: >, <, >=, <=, ==, !=, <> */
    const COMPARISON  = 'comparison';

    /** Logical operator: AND, OR, XOR */
    const LOGICAL     = 'logical';

    /** Cell range reference: A1:B10 */
    const RANGE       = 'range';

    /** NULL literal */
    const NULL_NODE   = 'null';

    /**
     * All valid node type constants.
     *
     * Used for validation when deserializing.
     *
     * @var string[]
     */
    private static $allTypes = null;

    /**
     * Get all valid node type constants.
     *
     * @return string[]
     */
    public static function all()
    {
        if (self::$allTypes === null) {
            self::$allTypes = array(
                self::LITERAL,
                self::VARIABLE,
                self::FUNCTION,
                self::BINARY_OP,
                self::UNARY_OP,
                self::CONDITIONAL,
                self::COMPARISON,
                self::LOGICAL,
                self::RANGE,
                self::NULL_NODE,
            );
        }
        return self::$allTypes;
    }

    /**
     * Check whether a type string is a valid node type.
     *
     * @param string $type
     * @return bool
     */
    public static function isValid($type)
    {
        return in_array((string)$type, self::all(), true);
    }

    /**
     * Get the human-readable label for a node type.
     *
     * @param string $type
     * @return string
     */
    public static function getLabel($type)
    {
        $labels = array(
            self::LITERAL     => 'Literal',
            self::VARIABLE    => 'Variable',
            self::FUNCTION    => 'Function',
            self::BINARY_OP   => 'Binary Operator',
            self::UNARY_OP    => 'Unary Operator',
            self::CONDITIONAL => 'Conditional',
            self::COMPARISON  => 'Comparison',
            self::LOGICAL     => 'Logical',
            self::RANGE       => 'Range',
            self::NULL_NODE   => 'NULL',
        );
        return isset($labels[$type]) ? $labels[$type] : 'Unknown';
    }
}
