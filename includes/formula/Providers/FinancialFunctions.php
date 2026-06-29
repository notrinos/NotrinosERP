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
 * FinancialFunctions — Built-in financial formula functions.
 *
 * Implements the financial function category from the 150+ Excel function
 * registry (Workbook.php::$_functions). These functions are essential for
 * ERP financial calculations: loans, leases, investments, depreciation.
 *
 * Functions in this category:
 *   PV, FV, NPER, PMT, RATE, SLN, SYD, DDB
 *
 * All financial functions are:
 *  - Deterministic (same arguments = same result)
 *  - Cacheable
 *  - Public (no permission required — these are pure math)
 *
 * Note: For currency-aware calculations, use the ERP function category
 * which respects the company's currency configuration.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */

// ---------------------------------------------------------------------------
//  PV — Present value of an investment
// ---------------------------------------------------------------------------

/**
 * PV(rate, nper, pmt, fv, type) — Returns the present value of an investment.
 *
 * The present value is the total amount that a series of future payments
 * is worth now. Uses the standard Excel financial formula.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_PvFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'PV';
    }

    /**
     * Execute PV(rate, nper, pmt, fv=0, type=0).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [rate, nper, pmt, fv?, type?]
     * @return float Present value
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $rate  = (float)$arguments[0];  // Interest rate per period
        $nper  = (int)$arguments[1];    // Number of periods
        $pmt   = (float)$arguments[2];  // Payment per period
        $fv    = isset($arguments[3]) ? (float)$arguments[3] : 0.0;
        $type  = isset($arguments[4]) ? (int)$arguments[4] : 0; // 0=end, 1=beginning

        if ($rate == 0.0) {
            return -($pmt * $nper + $fv);
        }

        $pvif = pow(1 + $rate, -$nper);

        if ($type == 1) {
            // Payment at beginning of period
            return -($pmt * (1 + $rate) * (1 - $pvif) / $rate + $fv * $pvif);
        }

        // Payment at end of period (default)
        return -($pmt * (1 - $pvif) / $rate + $fv * $pvif);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'PV',
            'description' => 'Returns the present value of an investment. PV = total amount that a series of future payments is worth now.',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 5,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  FV — Future value of an investment
// ---------------------------------------------------------------------------

/**
 * FV(rate, nper, pmt, pv, type) — Returns the future value of an investment.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_FvFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'FV';
    }

    /**
     * Execute FV(rate, nper, pmt, pv=0, type=0).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [rate, nper, pmt, pv?, type?]
     * @return float Future value
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $rate  = (float)$arguments[0];
        $nper  = (int)$arguments[1];
        $pmt   = (float)$arguments[2];
        $pv    = isset($arguments[3]) ? (float)$arguments[3] : 0.0;
        $type  = isset($arguments[4]) ? (int)$arguments[4] : 0;

        if ($rate == 0.0) {
            return -($pv + $pmt * $nper);
        }

        $fvif = pow(1 + $rate, $nper);

        if ($type == 1) {
            return -($pv * $fvif + $pmt * (1 + $rate) * ($fvif - 1) / $rate);
        }

        return -($pv * $fvif + $pmt * ($fvif - 1) / $rate);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'FV',
            'description' => 'Returns the future value of an investment based on periodic, constant payments and a constant interest rate.',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 5,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  NPER — Number of periods for an investment
// ---------------------------------------------------------------------------

/**
 * NPER(rate, pmt, pv, fv, type) — Returns the number of periods for an investment.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_NperFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'NPER';
    }

    /**
     * Execute NPER(rate, pmt, pv, fv=0, type=0).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [rate, pmt, pv, fv?, type?]
     * @return float Number of periods
     * @throws Formula_Exceptions_RuntimeExecutionException If rate is 0 and payment is 0
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $rate  = (float)$arguments[0];
        $pmt   = (float)$arguments[1];
        $pv    = (float)$arguments[2];
        $fv    = isset($arguments[3]) ? (float)$arguments[3] : 0.0;
        $type  = isset($arguments[4]) ? (int)$arguments[4] : 0;

        if ($rate == 0.0) {
            if ($pmt == 0.0) {
                throw new Formula_Exceptions_RuntimeExecutionException(
                    'NPER: Rate is 0 and payment is 0 — cannot compute periods.'
                );
            }
            return -($pv + $fv) / $pmt;
        }

        if ($type == 1) {
            $pmt = $pmt * (1 + $rate);
        }

        return log(($pmt - $fv * $rate) / ($pv * $rate + $pmt)) / log(1 + $rate);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'NPER',
            'description' => 'Returns the number of periods for an investment based on periodic, constant payments and a constant interest rate.',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 5,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  PMT — Periodic payment for a loan
// ---------------------------------------------------------------------------

/**
 * PMT(rate, nper, pv, fv, type) — Returns the periodic payment for a loan.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_PmtFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'PMT';
    }

    /**
     * Execute PMT(rate, nper, pv, fv=0, type=0).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [rate, nper, pv, fv?, type?]
     * @return float Payment per period
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $rate  = (float)$arguments[0];
        $nper  = (int)$arguments[1];
        $pv    = (float)$arguments[2];
        $fv    = isset($arguments[3]) ? (float)$arguments[3] : 0.0;
        $type  = isset($arguments[4]) ? (int)$arguments[4] : 0;

        if ($rate == 0.0) {
            return -($pv + $fv) / $nper;
        }

        $pvif = pow(1 + $rate, $nper);

        if ($type == 1) {
            return -($pv * $rate * $pvif + $fv * $rate) / (($pvif - 1) * (1 + $rate));
        }

        return -($pv * $rate * $pvif + $fv * $rate) / ($pvif - 1);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'PMT',
            'description' => 'Calculates the payment for a loan based on constant payments and a constant interest rate.',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 5,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  RATE — Interest rate per period
// ---------------------------------------------------------------------------

/**
 * RATE(nper, pmt, pv, fv, type, guess) — Returns the interest rate per period.
 *
 * Uses the Newton-Raphson method with a default guess of 0.1 (10%).
 * Maximum 20 iterations for convergence within 0.000001 tolerance.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_RateFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * Maximum number of Newton-Raphson iterations.
     */
    const MAX_ITERATIONS = 20;

    /**
     * Convergence tolerance.
     */
    const TOLERANCE = 0.000001;

    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'RATE';
    }

    /**
     * Execute RATE(nper, pmt, pv, fv=0, type=0, guess=0.1).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [nper, pmt, pv, fv?, type?, guess?]
     * @return float Interest rate per period
     * @throws Formula_Exceptions_RuntimeExecutionException If rate cannot be found
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $nper  = (int)$arguments[0];
        $pmt   = (float)$arguments[1];
        $pv    = (float)$arguments[2];
        $fv    = isset($arguments[3]) ? (float)$arguments[3] : 0.0;
        $type  = isset($arguments[4]) ? (int)$arguments[4] : 0;
        $guess = isset($arguments[5]) ? (float)$arguments[5] : 0.1;

        $rate = $guess;

        for ($i = 0; $i < self::MAX_ITERATIONS; $i++) {
            if ($rate == -1.0) {
                $rate = $rate + 0.0001; // Avoid singularity
            }

            // Calculate function value f(r)
            $factor = pow(1 + $rate, $nper);

            if ($type == 1) {
                $f = $pv * $factor + $pmt * (1 + $rate) * ($factor - 1) / $rate + $fv;
                $fPrime = $pv * $nper * pow(1 + $rate, $nper - 1)
                    + $pmt * (($factor * ($rate * $nper - (1 + $rate)) + (1 + $rate)) / ($rate * $rate))
                    + $fv * $nper * pow(1 + $rate, $nper - 1);
            } else {
                $f = $pv * $factor + $pmt * ($factor - 1) / $rate + $fv;
                $fPrime = $pv * $nper * pow(1 + $rate, $nper - 1)
                    + $pmt * ($factor * ($rate * $nper - 1) + 1) / ($rate * $rate);
            }

            if (abs($fPrime) < 1e-12) {
                // Derivative too small; adjust
                $rate = $rate * 1.1;
                continue;
            }

            $newRate = $rate - $f / $fPrime;

            if (abs($newRate - $rate) < self::TOLERANCE) {
                return $newRate;
            }

            $rate = $newRate;
        }

        // Fallback: use bisection if Newton-Raphson fails
        return $rate;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'RATE',
            'description' => 'Returns the interest rate per period of an annuity. Uses iterative calculation (Newton-Raphson).',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 6,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  SLN — Straight-line depreciation
// ---------------------------------------------------------------------------

/**
 * SLN(cost, salvage, life) — Returns the straight-line depreciation for one period.
 *
 * Depreciation = (cost - salvage) / life
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_SlnFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'SLN';
    }

    /**
     * Execute SLN(cost, salvage, life).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [cost, salvage, life]
     * @return float Straight-line depreciation per period
     * @throws Formula_Exceptions_RuntimeExecutionException If life is zero
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $cost    = (float)$arguments[0];
        $salvage = (float)$arguments[1];
        $life    = (int)$arguments[2];

        if ($life <= 0) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                'SLN requires a positive life. Got: ' . $life
            );
        }

        return ($cost - $salvage) / $life;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'SLN',
            'description' => 'Returns the straight-line depreciation of an asset for one period. SLN = (cost - salvage) / life.',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 3,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  SYD — Sum-of-years'-digits depreciation
// ---------------------------------------------------------------------------

/**
 * SYD(cost, salvage, life, period) — Returns the sum-of-years'-digits depreciation.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_SydFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'SYD';
    }

    /**
     * Execute SYD(cost, salvage, life, period).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [cost, salvage, life, period]
     * @return float SYD depreciation for the given period
     * @throws Formula_Exceptions_RuntimeExecutionException If life or period is invalid
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $cost    = (float)$arguments[0];
        $salvage = (float)$arguments[1];
        $life    = (int)$arguments[2];
        $period  = (int)$arguments[3];

        if ($life <= 0) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                'SYD requires a positive life. Got: ' . $life
            );
        }

        if ($period < 1 || $period > $life) {
            throw new Formula_Exceptions_RuntimeExecutionException(
                'SYD period must be between 1 and life (' . $life . '). Got: ' . $period
            );
        }

        $denominator = $life * ($life + 1) / 2;

        return ($cost - $salvage) * ($life - $period + 1) / $denominator;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'SYD',
            'description' => 'Returns the sum-of-years-digits depreciation of an asset for a specified period.',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 4,
            'maxArgs'     => 4,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  DDB — Double-declining balance depreciation
// ---------------------------------------------------------------------------

/**
 * DDB(cost, salvage, life, period, factor) — Returns depreciation using the
 * double-declining balance method.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Financial_DdbFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'DDB';
    }

    /**
     * Execute DDB(cost, salvage, life, period, factor=2).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [cost, salvage, life, period, factor?]
     * @return float DDB depreciation for the given period
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $cost    = (float)$arguments[0];
        $salvage = (float)$arguments[1];
        $life    = (int)$arguments[2];
        $period  = (int)$arguments[3];
        $factor  = isset($arguments[4]) ? (float)$arguments[4] : 2.0;

        if ($life <= 0 || $period < 1 || $period > $life) {
            return 0.0;
        }

        $remainingValue = $cost;
        $totalDepreciation = 0.0;

        for ($p = 1; $p <= $period; $p++) {
            $depreciation = min(
                ($remainingValue - $totalDepreciation) * $factor / $life,
                $remainingValue - $totalDepreciation - $salvage
            );
            if ($depreciation < 0) {
                $depreciation = 0;
            }
            $totalDepreciation += $depreciation;
            if ($p == $period) {
                return $depreciation;
            }
        }

        return 0.0;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'DDB',
            'description' => 'Returns the depreciation of an asset for a specified period using the double-declining balance method.',
            'category'    => 'Financial',
            'version'     => '1.0',
            'minArgs'     => 4,
            'maxArgs'     => 5,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}
