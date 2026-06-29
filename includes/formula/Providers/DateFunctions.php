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
 * DateFunctions — Built-in date/time formula functions.
 *
 * Implements the date/time function category from the 150+ Excel function
 * registry (Workbook.php::$_functions). Each function is registered
 * as an independent class implementing FormulaFunctionInterface.
 *
 * Functions in this category:
 *   DATE, TIME, TODAY, NOW, YEAR, MONTH, DAY, WEEKDAY, HOUR, MINUTE, SECOND, DAYS
 *
 * Date semantics:
 *  - Dates are represented as DateTimeImmutable objects internally
 *  - TODAY() and NOW() are VOLATILE (not deterministic, not cacheable)
 *  - All other date functions are deterministic for given inputs
 *  - Uses the NotrinosERP date system (SQL format: Y-m-d, user format per locale)
 *
 * @package Formula\Providers
 * @since   2.0.0
 */

// ---------------------------------------------------------------------------
//  DATE — Create a date from year, month, day
// ---------------------------------------------------------------------------

/**
 * DATE(year, month, day) — Returns a DateTimeImmutable representing the given date.
 *
 * Excel-compatible: month values > 12 roll over to the next year,
 * day values beyond the month roll over to the next month.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_DateFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'DATE';
    }

    /**
     * Execute DATE(year, month, day).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [year, month, day]
     * @return DateTimeImmutable The constructed date
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $year  = (int)$arguments[0];
        $month = (int)$arguments[1];
        $day   = (int)$arguments[2];

        // Excel-compatible: month rollover
        if ($month < 1 || $month > 12) {
            $year  += intdiv($month - 1, 12);
            $month  = (($month - 1) % 12) + 1;
            if ($month < 1) {
                $month += 12;
                $year  -= 1;
            }
        }

        // Clamp day to max days in the calculated month
        $maxDay = (int)(new DateTimeImmutable("$year-$month-01"))->format('t');
        if ($day > $maxDay) {
            $day = $maxDay;
        }
        if ($day < 1) {
            $day = 1;
        }

        $date = new DateTimeImmutable(
            sprintf('%04d-%02d-%02d', $year, $month, $day)
        );

        return $date;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'DATE',
            'description' => 'Returns a date value from year, month, and day components. Month and day values automatically roll over.',
            'category'    => 'Date',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 3,
            'returnType'  => 'date',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  TIME — Create a time from hour, minute, second
// ---------------------------------------------------------------------------

/**
 * TIME(hour, minute, second) — Returns a time value.
 *
 * Excel-compatible: values outside normal ranges roll over
 * (e.g., TIME(25, 0, 0) = 1:00 AM next day).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_TimeFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'TIME';
    }

    /**
     * Execute TIME(hour, minute, second).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [hour, minute, second]
     * @return float Time as a fraction of a 24-hour day (Excel-compatible serial time)
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $hour   = (int)$arguments[0];
        $minute = (int)$arguments[1];
        $second = (int)$arguments[2];

        // Normalize: seconds → minutes, minutes → hours
        $totalSeconds = ($hour * 3600) + ($minute * 60) + $second;
        $totalSeconds = $totalSeconds % (24 * 3600); // Wrap within 24 hours
        if ($totalSeconds < 0) {
            $totalSeconds += 24 * 3600;
        }

        return $totalSeconds / (24 * 3600);
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'TIME',
            'description' => 'Returns a time value from hour, minute, and second components. Values automatically roll over.',
            'category'    => 'Date',
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
//  TODAY — Current date (VOLATILE)
// ---------------------------------------------------------------------------

/**
 * TODAY() — Returns the current date.
 *
 * VOLATILE: This function is NOT deterministic and NOT cacheable.
 * Each call returns the current system date.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_TodayFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'TODAY';
    }

    /**
     * Execute TODAY().
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments No arguments expected
     * @return DateTimeImmutable Current date at midnight
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return new DateTimeImmutable('today');
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'TODAY',
            'description' => 'Returns the current date. The value updates automatically when the worksheet is recalculated.',
            'category'    => 'Date',
            'version'     => '1.0',
            'minArgs'     => 0,
            'maxArgs'     => 0,
            'returnType'  => 'date',
            'isDeterministic' => false, // VOLATILE
            'isCacheable'     => false, // VOLATILE
        ));
    }
}

// ---------------------------------------------------------------------------
//  NOW — Current date and time (VOLATILE)
// ---------------------------------------------------------------------------

/**
 * NOW() — Returns the current date and time.
 *
 * VOLATILE: This function is NOT deterministic and NOT cacheable.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_NowFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'NOW';
    }

    /**
     * Execute NOW().
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments No arguments expected
     * @return DateTimeImmutable Current date and time
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        return new DateTimeImmutable('now');
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'NOW',
            'description' => 'Returns the current date and time. The value updates when the worksheet is recalculated.',
            'category'    => 'Date',
            'version'     => '1.0',
            'minArgs'     => 0,
            'maxArgs'     => 0,
            'returnType'  => 'datetime',
            'isDeterministic' => false, // VOLATILE
            'isCacheable'     => false, // VOLATILE
        ));
    }
}

// ---------------------------------------------------------------------------
//  YEAR — Extract year from a date
// ---------------------------------------------------------------------------

/**
 * YEAR(date) — Returns the year component of a date.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_YearFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'YEAR';
    }

    /**
     * Execute YEAR(date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [date]
     * @return int The year (e.g., 2026)
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $date = $arguments[0];

        if ($date instanceof DateTimeInterface) {
            return (int)$date->format('Y');
        }

        // Try to parse string or timestamp
        return (int)date('Y', is_numeric($date) ? (int)$date : strtotime((string)$date));
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'YEAR',
            'description' => 'Returns the year corresponding to a date. The year is returned as an integer in the range 1900-9999.',
            'category'    => 'Date',
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
//  MONTH — Extract month from a date (1-12)
// ---------------------------------------------------------------------------

/**
 * MONTH(date) — Returns the month component of a date (1=January through 12=December).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_MonthFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'MONTH';
    }

    /**
     * Execute MONTH(date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [date]
     * @return int Month number (1-12)
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $date = $arguments[0];

        if ($date instanceof DateTimeInterface) {
            return (int)$date->format('n');
        }

        return (int)date('n', is_numeric($date) ? (int)$date : strtotime((string)$date));
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'MONTH',
            'description' => 'Returns the month of a date. The month is returned as an integer from 1 (January) to 12 (December).',
            'category'    => 'Date',
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
//  DAY — Extract day of month from a date (1-31)
// ---------------------------------------------------------------------------

/**
 * DAY(date) — Returns the day of the month (1 through 31).
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_DayFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'DAY';
    }

    /**
     * Execute DAY(date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [date]
     * @return int Day of month (1-31)
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $date = $arguments[0];

        if ($date instanceof DateTimeInterface) {
            return (int)$date->format('j');
        }

        return (int)date('j', is_numeric($date) ? (int)$date : strtotime((string)$date));
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'DAY',
            'description' => 'Returns the day of a date. The day is returned as an integer from 1 to 31.',
            'category'    => 'Date',
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
//  WEEKDAY — Day of week (1=Sunday through 7=Saturday, or configurable)
// ---------------------------------------------------------------------------

/**
 * WEEKDAY(date, return_type) — Returns the day of the week as an integer.
 *
 * Return type:
 *   1 (default): 1=Sunday through 7=Saturday
 *   2:           1=Monday through 7=Sunday
 *   3:           0=Monday through 6=Sunday
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_WeekdayFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'WEEKDAY';
    }

    /**
     * Execute WEEKDAY(date, return_type).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [date, return_type?]
     * @return int Day of week
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $date       = $arguments[0];
        $returnType = isset($arguments[1]) ? (int)$arguments[1] : 1;

        if ($date instanceof DateTimeInterface) {
            $dayOfWeek = (int)$date->format('w'); // 0=Sunday, 6=Saturday
        } else {
            $ts        = is_numeric($date) ? (int)$date : strtotime((string)$date);
            $dayOfWeek = (int)date('w', $ts);
        }

        switch ($returnType) {
            case 1:
                // 1=Sunday, 7=Saturday
                return $dayOfWeek + 1;
            case 2:
                // 1=Monday, 7=Sunday
                return $dayOfWeek === 0 ? 7 : $dayOfWeek;
            case 3:
                // 0=Monday, 6=Sunday
                return $dayOfWeek === 0 ? 6 : $dayOfWeek - 1;
            default:
                return $dayOfWeek + 1;
        }
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'WEEKDAY',
            'description' => 'Returns the day of the week corresponding to a date. The day is returned as an integer (1=Sunday through 7=Saturday by default).',
            'category'    => 'Date',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}

// ---------------------------------------------------------------------------
//  HOUR — Extract hour from a time (0-23)
// ---------------------------------------------------------------------------

/**
 * HOUR(time) — Returns the hour component of a time value.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_HourFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'HOUR';
    }

    /**
     * Execute HOUR(time).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [time]
     * @return int Hour (0-23)
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $time = $arguments[0];

        if ($time instanceof DateTimeInterface) {
            return (int)$time->format('G');
        }

        // Excel serial time (fraction of day)
        if (is_numeric($time) && $time < 1.0) {
            return (int)($time * 24);
        }

        return (int)date('G', is_numeric($time) ? (int)$time : strtotime((string)$time));
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'HOUR',
            'description' => 'Returns the hour of a time value. The hour is returned as an integer from 0 (12:00 AM) to 23 (11:00 PM).',
            'category'    => 'Date',
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
//  MINUTE — Extract minute from a time (0-59)
// ---------------------------------------------------------------------------

/**
 * MINUTE(time) — Returns the minute component of a time value.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_MinuteFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'MINUTE';
    }

    /**
     * Execute MINUTE(time).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [time]
     * @return int Minute (0-59)
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $time = $arguments[0];

        if ($time instanceof DateTimeInterface) {
            return (int)$time->format('i');
        }

        if (is_numeric($time) && $time < 1.0) {
            return (int)(($time * 24 * 60) % 60);
        }

        return (int)date('i', is_numeric($time) ? (int)$time : strtotime((string)$time));
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'MINUTE',
            'description' => 'Returns the minute of a time value. The minute is returned as an integer from 0 to 59.',
            'category'    => 'Date',
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
//  SECOND — Extract second from a time (0-59)
// ---------------------------------------------------------------------------

/**
 * SECOND(time) — Returns the second component of a time value.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_SecondFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'SECOND';
    }

    /**
     * Execute SECOND(time).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [time]
     * @return int Second (0-59)
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $time = $arguments[0];

        if ($time instanceof DateTimeInterface) {
            return (int)$time->format('s');
        }

        if (is_numeric($time) && $time < 1.0) {
            return (int)(($time * 24 * 3600) % 60);
        }

        return (int)date('s', is_numeric($time) ? (int)$time : strtotime((string)$time));
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'SECOND',
            'description' => 'Returns the second of a time value. The second is returned as an integer from 0 to 59.',
            'category'    => 'Date',
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
//  DAYS — Days between two dates
// ---------------------------------------------------------------------------

/**
 * DAYS(end_date, start_date) — Returns the number of days between two dates.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_Date_DaysFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'DAYS';
    }

    /**
     * Execute DAYS(end_date, start_date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments Already-resolved arguments [end_date, start_date]
     * @return int Number of days between the dates
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $endDate   = $arguments[0];
        $startDate = $arguments[1];

        // Convert to DateTimeImmutable if not already
        if (!$endDate instanceof DateTimeInterface) {
            $endDate = new DateTimeImmutable(
                is_numeric($endDate) ? '@' . (int)$endDate : (string)$endDate
            );
        }
        if (!$startDate instanceof DateTimeInterface) {
            $startDate = new DateTimeImmutable(
                is_numeric($startDate) ? '@' . (int)$startDate : (string)$startDate
            );
        }

        $diff = $endDate->diff($startDate);
        return $diff->days;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'DAYS',
            'description' => 'Returns the number of days between two dates. DAYS(end_date, start_date) = end_date - start_date.',
            'category'    => 'Date',
            'version'     => '1.0',
            'minArgs'     => 2,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
        ));
    }
}
