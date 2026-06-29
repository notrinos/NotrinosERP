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
 * LogicalFunctions — Built-in logical and information formula functions.
 *
 * Implements the logical function category from the 150+ Excel function
 * registry (Workbook.php::$_functions). Each function is registered
 * as an independent class implementing FormulaFunctionInterface.
 *
 * Functions in this category:
 *   IF, AND, OR, NOT, ISNA, ISERROR
 *
 * Note: IF is handled specially by the parser as a ConditionalNode;
 * its implementation here ensures it is also available as a registered
 * function for backward compatibility.
 *
 * All logical functions are:
 *  - Deterministic (same arguments = same result)
 *  - Cacheable
 *  - Public (no permission required)
 *
 * @package Formula\Providers
 * @since   2.0.0
 */

// ---------------------------------------------------------------------------
//  IF — Conditional evaluation
// ---------------------------------------------------------------------------

/**
 * IF(condition, true_value, false_value) — Returns one value if a condition
 * evaluates to TRUE and another value if it evaluates to FALSE.
 *
 * The parser converts IF() to a ConditionalNode for short-circuit evaluation.
 * This implementation is used when IF is called via the function registry
 * (e.g., when an extension registers it).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Logical_IfFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'IF';
    }

    /**
     * Execute IF(condition, true_value, false_value).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [condition, trueVal, falseVal]
     * @return mixed The selected value based on condition truthiness
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $condition = $arguments[0];
        $trueVal   = isset($arguments[1]) ? $arguments[1] : 0;
        $falseVal  = isset($arguments[2]) ? $arguments[2] : 0;

        return $condition ? $trueVal : $falseVal;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'IF',
            'description' => 'Returns one value if a condition is TRUE and another if FALSE. Short-circuit: only the selected branch is evaluated.',
            'category'    => 'Logical',
            'version'     => '1.0',
            'minArgs'     => 2,
            'maxArgs'     => 3,
            'returnType'  => 'mixed',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  AND — Logical conjunction
// ---------------------------------------------------------------------------

/**
 * AND(condition1, condition2, ...) — Returns TRUE if ALL arguments evaluate to TRUE.
 *
 * Short-circuit: evaluation stops at the first FALSE argument.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Logical_AndFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'AND';
    }

    /**
     * Execute AND(condition1, condition2, ...).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments
     * @return bool TRUE if all arguments are truthy, FALSE otherwise
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        if (empty($arguments)) {
            return true; // Vacuous truth — AND with no arguments
        }

        foreach ($arguments as $arg) {
            if (!$arg) {
                return false;
            }
        }
        return true;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'AND',
            'description' => 'Returns TRUE if all of its arguments are TRUE. Returns FALSE if one or more arguments are FALSE.',
            'category'    => 'Logical',
            'version'     => '1.0',
            'minArgs'     => -1,
            'maxArgs'     => -1,
            'returnType'  => 'boolean',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  OR — Logical disjunction
// ---------------------------------------------------------------------------

/**
 * OR(condition1, condition2, ...) — Returns TRUE if ANY argument evaluates to TRUE.
 *
 * Short-circuit: evaluation stops at the first TRUE argument.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Logical_OrFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'OR';
    }

    /**
     * Execute OR(condition1, condition2, ...).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments
     * @return bool TRUE if any argument is truthy, FALSE otherwise
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        if (empty($arguments)) {
            return false; // OR with no arguments
        }

        foreach ($arguments as $arg) {
            if ($arg) {
                return true;
            }
        }
        return false;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'OR',
            'description' => 'Returns TRUE if any argument is TRUE. Returns FALSE if all arguments are FALSE.',
            'category'    => 'Logical',
            'version'     => '1.0',
            'minArgs'     => -1,
            'maxArgs'     => -1,
            'returnType'  => 'boolean',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  NOT — Logical negation
// ---------------------------------------------------------------------------

/**
 * NOT(logical) — Reverses the logical value of its argument.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Logical_NotFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'NOT';
    }

    /**
     * Execute NOT(logical).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [value]
     * @return bool The negation of the argument
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return !$arguments[0];
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'NOT',
            'description' => 'Reverses the value of its argument. NOT(TRUE) = FALSE. NOT(FALSE) = TRUE.',
            'category'    => 'Logical',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'boolean',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  ISNA — Test for #N/A error value
// ---------------------------------------------------------------------------

/**
 * ISNA(value) — Returns TRUE if the value is the #N/A error value.
 *
 * In the NotrinosERP formula framework, ISNA checks whether a value
 * equals the sentinel Formula_Providers_Logical_NA singleton.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Logical_IsnaFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ISNA';
    }

    /**
     * Execute ISNA(value).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [value]
     * @return bool TRUE if the value represents #N/A
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $value = $arguments[0];
        return ($value instanceof Formula_Providers_Logical_NA);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ISNA',
            'description' => 'Returns TRUE if the value is the #N/A error value. Used to test for lookup failures.',
            'category'    => 'Logical',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'boolean',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  ISERROR — Test for any error value
// ---------------------------------------------------------------------------

/**
 * ISERROR(value) — Returns TRUE if the value is any error value.
 *
 * Checks whether the value is an instance of FormulaException or the
 * NA sentinel, indicating an error occurred during evaluation.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Logical_IsErrorFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ISERROR';
    }

    /**
     * Execute ISERROR(value).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [value]
     * @return bool TRUE if the value is an error
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $value = $arguments[0];
        return ($value instanceof Formula_Exceptions_FormulaException)
            || ($value instanceof Formula_Providers_Logical_NA);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ISERROR',
            'description' => 'Returns TRUE if the value is any error value (#N/A, #VALUE!, #DIV/0!, etc.).',
            'category'    => 'Logical',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'boolean',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  NA — Sentinel value for #N/A
// ---------------------------------------------------------------------------

/**
 * NA — Sentinel class representing the #N/A (Not Available) error value.
 *
 * Functions like ISNA and ISERROR check for instances of this class.
 * It is NOT a function — it is a helper sentinel used by lookup
 * functions (VLOOKUP, HLOOKUP, MATCH) to signal "not found."
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Logical_NA
{
    /**
     * Singleton instance.
     *
     * @var Formula_Providers_Logical_NA|null
     */
    private static $instance = null;

    /**
     * Private constructor — use ::value() to get the singleton.
     */
    private function __construct()
    {
    }

    /**
     * Get the singleton #N/A sentinel value.
     *
     * @return Formula_Providers_Logical_NA
     */
    public static function value()
    {
        if (self::$instance === null) {
            self::$instance = new self();
        }
        return self::$instance;
    }

    /**
     * String representation for display.
     *
     * @return string
     */
    public function __toString()
    {
        return '#N/A';
    }
}
