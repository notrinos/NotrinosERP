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
 * TokenType — Constants for all NFX token types.
 *
 * Defines every token type the lexer can produce. Types are integer
 * constants with large spacing to allow future insertion without
 * renumbering existing types.
 *
 * @package Formula\Compiler
 * @since   2.0.0
 */
class Formula_Compiler_TokenType
{
    // ---- Structural ----
    const T_EOF          = 1000;

    // ---- Literals ----
    const T_INTEGER      = 1100;
    const T_DECIMAL      = 1101;
    const T_STRING       = 1102;

    // ---- Identifiers ----
    const T_IDENTIFIER   = 1200;
    const T_NAMESPACE    = 1201;

    // ---- Arithmetic ----
    const T_PLUS         = 1300;
    const T_MINUS        = 1301;
    const T_MULTIPLY     = 1302;
    const T_DIVIDE       = 1303;
    const T_MODULO       = 1304;
    const T_POWER        = 1305;

    // ---- Comparison ----
    const T_EQ           = 1400;
    const T_NE           = 1401;
    const T_LT           = 1402;
    const T_LE           = 1403;
    const T_GT           = 1404;
    const T_GE           = 1405;

    // ---- Logical ----
    const T_AND          = 1500;
    const T_OR           = 1501;
    const T_NOT          = 1502;
    const T_XOR          = 1503;

    // ---- Boolean & Null ----
    const T_TRUE         = 1600;
    const T_FALSE        = 1601;
    const T_NULL         = 1602;

    // ---- Delimiters ----
    const T_LPAREN       = 1700;
    const T_RPAREN       = 1701;
    const T_COMMA        = 1702;
    const T_COLON        = 1703;
    const T_DOLLAR       = 1704;
    const T_EXCLAMATION  = 1705;

    // ---- Special ----
    const T_NULL_COALESCE = 1800;
    const T_CELL_REF      = 1900;

    /**
     * Get the human-readable name for a token type.
     *
     * @param int $type The token type constant
     * @return string The human-readable name (e.g., 'T_PLUS', 'T_IDENTIFIER')
     */
    public static function getName($type)
    {
        $map = self::getTypeMap();
        return isset($map[$type]) ? $map[$type] : 'T_UNKNOWN';
    }

    /**
     * Check whether a token type value is valid.
     *
     * @param int $type
     * @return bool
     */
    public static function isValid($type)
    {
        $map = self::getTypeMap();
        return isset($map[$type]);
    }

    /**
     * Get the complete type-to-name map.
     *
     * @return array<int, string>
     */
    public static function all()
    {
        return self::getTypeMap();
    }

    /**
     * Build the type map (lazy-loaded via static cache).
     *
     * @return array<int, string>
     */
    private static function getTypeMap()
    {
        static $map = null;
        if ($map === null) {
            $reflection = new ReflectionClass(__CLASS__);
            $map = array();
            foreach ($reflection->getConstants() as $name => $value) {
                $map[$value] = $name;
            }
        }
        return $map;
    }
}
