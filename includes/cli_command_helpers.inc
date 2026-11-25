<?php
/**********************************************************************
	Shared command-line helpers.

	One copy of the two steps that the admin/ command-line scripts used to
	repeat under their own names: reading fixed --name=value options and
	ending the script with one JSON result line. Load this file with
	require_once from every script that calls one of these functions.
***********************************************************************/

/**
 * Write one JSON result line and stop the script.
 *
 * The line holds 'ok' and 'code' followed by $details. It goes to STDOUT
 * with exit status 0 when $ok is true, otherwise to STDERR with status 1.
 *
 * @param bool $ok
 * @param string $code
 * @param array $details
 * @return void
 */
function cli_command_exit($ok, $code, $details = array())
{
	$result = array_merge(array('ok' => (bool)$ok, 'code' => (string)$code), $details);
	fwrite($ok ? STDOUT : STDERR, json_encode($result)."\n");
	exit($ok ? 0 : 1);
}

/**
 * Read command-line arguments written as --name=value.
 *
 * Names are lower-case letters and hyphens. A positional argument, an
 * option given twice or a value with a line break is refused.
 *
 * @param array $arguments
 * @return array|false Option name => value, or false.
 */
function cli_command_options($arguments)
{
	$options = array();
	foreach ($arguments as $argument) {
		if (!preg_match('/^--([a-z-]+)=(.*)$/D', (string)$argument, $matches) || isset($options[$matches[1]]))
			return false;
		$options[$matches[1]] = $matches[2];
	}
	return $options;
}
