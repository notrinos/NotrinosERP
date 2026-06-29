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
 * MathFunctions — Built-in mathematical formula functions.
 *
 * Implements the math function category from the 150+ Excel function
 * registry (Workbook.php::$_functions). Each function is registered
 * as an independent class implementing FormulaFunctionInterface.
 *
 * Functions in this category:
 *   ABS, ROUND, INT, SQRT, EXP, LN, LOG10, SIN, COS, TAN, ATAN, SIGN, MOD
 *
 * All math functions are:
 *  - Deterministic (same arguments = same result)
 *  - Cacheable (result can be memoized within a session)
 *  - Public (no permission required)
 *
 * @package Formula\Providers
 * @since   2.0.0
 */

// ---------------------------------------------------------------------------
//  ABS — Absolute value
// ---------------------------------------------------------------------------

/**
 * ABS(number) — Returns the absolute (non-negative) value of a number.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_AbsFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ABS';
    }

    /**
     * Execute ABS(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return float The absolute value
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return abs((float)$arguments[0]);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ABS',
            'description' => 'Returns the absolute value of a number. The absolute value of a number is the number without its sign.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  ROUND — Round to specified decimal places
// ---------------------------------------------------------------------------

/**
 * ROUND(number, num_digits) — Rounds a number to a specified number of digits.
 *
 * Uses PHP_ROUND_HALF_UP (Excel-compatible rounding).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_RoundFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ROUND';
    }

    /**
     * Execute ROUND(number, num_digits).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number, num_digits]
     * @return float The rounded value
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $number    = (float)$arguments[0];
        $numDigits = (int)$arguments[1];

        return round($number, $numDigits, PHP_ROUND_HALF_UP);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ROUND',
            'description' => 'Rounds a number to a specified number of digits. Uses standard rounding (half up).',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 2,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  INT — Integer part (truncate toward zero)
// ---------------------------------------------------------------------------

/**
 * INT(number) — Rounds a number down to the nearest integer.
 *
 * For positive numbers, INT is equivalent to floor().
 * For negative numbers, INT rounds away from zero (toward negative infinity).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_IntFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'INT';
    }

    /**
     * Execute INT(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return float The integer part
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return (float)intval((float)$arguments[0]);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'INT',
            'description' => 'Rounds a number down to the nearest integer. INT(8.9) = 8, INT(-8.9) = -9.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  SQRT — Square root
// ---------------------------------------------------------------------------

/**
 * SQRT(number) — Returns the positive square root of a number.
 *
 * Throws RuntimeExecutionException for negative inputs (matching Excel #NUM! error).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_SqrtFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'SQRT';
    }

    /**
     * Execute SQRT(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return float The square root
     * @throws Formula_Exceptions_RuntimeExecutionException If number is negative
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $number = (float)$arguments[0];

        if ($number < 0) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                'SQRT expects a non-negative number. Got: ' . $number
            );
        }

        return sqrt($number);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'SQRT',
            'description' => 'Returns the positive square root of a number. The number must be non-negative.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  EXP — e raised to a power
// ---------------------------------------------------------------------------

/**
 * EXP(number) — Returns e raised to the power of a given number.
 *
 * The constant e (Euler's number) equals approximately 2.71828182845904.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_ExpFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'EXP';
    }

    /**
     * Execute EXP(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return float e^number
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return exp((float)$arguments[0]);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'EXP',
            'description' => 'Returns e raised to the power of number. The constant e equals 2.71828182845904, the base of the natural logarithm.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  LN — Natural logarithm (base e)
// ---------------------------------------------------------------------------

/**
 * LN(number) — Returns the natural logarithm of a number.
 *
 * Natural logarithms are based on the constant e (2.71828182845904).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_LnFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'LN';
    }

    /**
     * Execute LN(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return float The natural logarithm
     * @throws Formula_Exceptions_RuntimeExecutionException If number is negative or zero
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $number = (float)$arguments[0];

        if ($number <= 0) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                'LN expects a positive number. Got: ' . $number
            );
        }

        return log($number);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'LN',
            'description' => 'Returns the natural logarithm of a number. Natural logarithms are based on the constant e (2.71828182845904).',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  LOG10 — Base-10 logarithm
// ---------------------------------------------------------------------------

/**
 * LOG10(number) — Returns the base-10 logarithm of a number.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_Log10Function implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'LOG10';
    }

    /**
     * Execute LOG10(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return float The base-10 logarithm
     * @throws Formula_Exceptions_RuntimeExecutionException If number is negative or zero
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $number = (float)$arguments[0];

        if ($number <= 0) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                'LOG10 expects a positive number. Got: ' . $number
            );
        }

        return log10($number);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'LOG10',
            'description' => 'Returns the base-10 logarithm of a number.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  SIN — Sine (radians)
// ---------------------------------------------------------------------------

/**
 * SIN(angle) — Returns the sine of an angle given in radians.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_SinFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'SIN';
    }

    /**
     * Execute SIN(angle).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [angle_in_radians]
     * @return float The sine
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return sin((float)$arguments[0]);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'SIN',
            'description' => 'Returns the sine of an angle. The angle must be in radians.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  COS — Cosine (radians)
// ---------------------------------------------------------------------------

/**
 * COS(angle) — Returns the cosine of an angle given in radians.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_CosFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'COS';
    }

    /**
     * Execute COS(angle).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [angle_in_radians]
     * @return float The cosine
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return cos((float)$arguments[0]);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'COS',
            'description' => 'Returns the cosine of an angle. The angle must be in radians.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  TAN — Tangent (radians)
// ---------------------------------------------------------------------------

/**
 * TAN(angle) — Returns the tangent of an angle given in radians.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_TanFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'TAN';
    }

    /**
     * Execute TAN(angle).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [angle_in_radians]
     * @return float The tangent
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return tan((float)$arguments[0]);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'TAN',
            'description' => 'Returns the tangent of an angle. The angle must be in radians.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  ATAN — Arctangent (returns radians)
// ---------------------------------------------------------------------------

/**
 * ATAN(number) — Returns the arctangent of a number in radians.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_AtanFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ATAN';
    }

    /**
     * Execute ATAN(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return float The arctangent in radians
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return atan((float)$arguments[0]);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ATAN',
            'description' => 'Returns the arctangent of a number. The result is in radians between -PI/2 and PI/2.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  SIGN — Sign of a number (-1, 0, or 1)
// ---------------------------------------------------------------------------

/**
 * SIGN(number) — Determines the sign of a number.
 *
 * Returns 1 if positive, 0 if zero, -1 if negative.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_SignFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'SIGN';
    }

    /**
     * Execute SIGN(number).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number]
     * @return int -1, 0, or 1
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $number = (float)$arguments[0];

        if ($number > 0) {
            return 1;
        }
        if ($number < 0) {
            return -1;
        }
        return 0;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'SIGN',
            'description' => 'Determines the sign of a number. Returns 1 if positive, 0 if zero, -1 if negative.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  MOD — Remainder after division (Excel-compatible)
// ---------------------------------------------------------------------------

/**
 * MOD(number, divisor) — Returns the remainder after division.
 *
 * The result has the same sign as the divisor (Excel-compatible behavior).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Math_ModFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'MOD';
    }

    /**
     * Execute MOD(number, divisor).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [number, divisor]
     * @return float The remainder
     * @throws Formula_Exceptions_RuntimeExecutionException If divisor is zero
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $number  = (float)$arguments[0];
        $divisor = (float)$arguments[1];

        if ($divisor == 0.0) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                'MOD requires a non-zero divisor.'
            );
        }

        // Excel-compatible MOD: result has the same sign as the divisor
        $result = fmod($number, $divisor);
        if (($result < 0 && $divisor > 0) || ($result > 0 && $divisor < 0)) {
            $result += $divisor;
        }

        return $result;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'MOD',
            'description' => 'Returns the remainder after a number is divided by a divisor. The result has the same sign as the divisor.',
            'category'    => 'Math',
            'version'     => '1.0',
            'minArgs'     => 2,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}
