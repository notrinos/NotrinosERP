<?php
/**********************************************************************
	Shared value helpers.

	One copy of small checks that modules used to repeat under their own
	names: the signed-in user's id, the security session id, whole-number
	and calendar-date input parsing, today and entered dates for SQL, the
	SHA-256 digest format and the hash of a client idempotency key. Load this file with require_once from every
	file that calls one of these functions.
***********************************************************************/

/**
 * Return the id of the signed-in user, or 0 when no user is signed in.
 *
 * @return int
 */
function current_user_id_or_zero()
{
	return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
		&& isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

/**
 * Return the id of the current security session registry row, or 0.
 *
 * @return int
 */
function current_security_session_id_or_zero()
{
	return isset($_SESSION['security_session_registry_id'])
		? (int)$_SESSION['security_session_registry_id'] : 0;
}

/**
 * Parse a whole number greater than zero.
 *
 * Accepts an int, or a string of decimal digits without a leading zero and
 * with optional surrounding spaces. Anything else, including values beyond
 * PHP_INT_MAX, is refused.
 *
 * @param mixed $value
 * @return int|false
 */
function parse_positive_integer($value)
{
	if (is_int($value))
		return $value > 0 ? $value : false;
	$value = trim((string)$value);
	if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
		return false;
	$number = (int)$value;
	return $number > 0 && (string)$number === $value ? $number : false;
}

/**
 * Parse a whole number greater than zero, without trimming.
 *
 * Accepts an int, or a string of decimal digits without a leading zero and
 * without spaces. Any other type (null, float, bool) is refused. When
 * $nullable is true, null and '' give null.
 *
 * @param mixed $value
 * @param bool $nullable
 * @return int|false|null
 */
function parse_positive_integer_strict($value, $nullable = false)
{
	if (($value === null || $value === '') && $nullable)
		return null;
	if (is_int($value))
		return $value > 0 ? $value : false;
	if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
		return false;
	$number = (int)$value;
	return $number > 0 && (string)$number === $value ? $number : false;
}

/**
 * Parse a whole number that is zero or greater.
 *
 * Same rules as parse_positive_integer(), but "0" is accepted.
 *
 * @param mixed $value
 * @return int|false
 */
function parse_non_negative_integer($value)
{
	if (is_int($value))
		return $value >= 0 ? $value : false;
	$value = trim((string)$value);
	if (preg_match('/^(?:0|[1-9][0-9]*)$/D', $value) !== 1)
		return false;
	$number = (int)$value;
	return $number >= 0 && (string)$number === $value ? $number : false;
}

/**
 * Check a real calendar date written as YYYY-MM-DD.
 *
 * Returns the trimmed date string, or false for any other format or for a
 * date that does not exist (for example 2026-02-30).
 *
 * @param mixed $value
 * @return string|false
 */
function parse_iso_calendar_date($value)
{
	$value = trim((string)$value);
	if (preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1)
		return false;
	$date = DateTime::createFromFormat('!Y-m-d', $value);
	$errors = DateTime::getLastErrors();
	if (!$date || (is_array($errors)
		&& ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
		|| $date->format('Y-m-d') !== $value)
		return false;
	return $value;
}

/**
 * Hash a client idempotency key for storage.
 *
 * The key must be a string of 16 to 128 characters from A-Z, a-z, 0-9 and
 * ". _ : -". Returns its SHA-256 as 64 lowercase hex characters, or false.
 *
 * @param mixed $key
 * @return string|false
 */
function idempotency_key_hash($key)
{
	if (!is_string($key) || strlen($key) < 16 || strlen($key) > 128
		|| preg_match('/^[A-Za-z0-9._:-]+$/D', $key) !== 1)
		return false;
	return hash('sha256', $key);
}

/**
 * Convert an entered date to YYYY-MM-DD for SQL.
 *
 * Trims the value. An empty value gives ''. A YYYY-MM-DD value is returned
 * as it is. A date in the user's date format is converted with date2sql().
 * Any other text is returned unchanged, so the caller's own check refuses it.
 *
 * @param mixed $date_value
 * @return string
 */
function input_date_to_sql_date($date_value)
{
	$date_value = trim((string)$date_value);
	if ($date_value === '')
		return '';
	if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_value))
		return $date_value;
	if (is_date($date_value))
		return date2sql($date_value);
	return $date_value;
}

/**
 * Check a SHA-256 digest written as 64 lowercase hex characters.
 *
 * @param mixed $value
 * @return bool
 */
function is_sha256_hex($value)
{
	return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

/**
 * Return today's date as YYYY-MM-DD.
 *
 * Uses the application's Today() and date2sql() when they are loaded and
 * give a real calendar date, otherwise the server date.
 *
 * @return string
 */
function today_as_sql_date()
{
	if (function_exists('Today') && function_exists('date2sql')) {
		$today = date2sql(Today());
		if (parse_iso_calendar_date($today) !== false)
			return $today;
	}
	return date('Y-m-d');
}
