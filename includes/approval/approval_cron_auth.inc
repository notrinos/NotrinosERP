<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * Parse the mandatory company selector used by the approval CLI worker.
 *
 * @param array $arguments Command-line arguments, including script name
 * @return array status, company and message_code
 */
function parse_approval_cron_company_argument($arguments)
{
    $company = null;
    foreach ((array)$arguments as $argument) {
        if (strpos((string)$argument, '--company=') !== 0)
            continue;
        if ($company !== null) {
            return array(
                'status' => 'invalid_company',
                'company' => null,
                'message_code' => 'duplicate_company',
            );
        }
        $value = substr((string)$argument, strlen('--company='));
        if ($value === '' || !ctype_digit($value)) {
            return array(
                'status' => 'invalid_company',
                'company' => null,
                'message_code' => 'invalid_company',
            );
        }
        $company = (int)$value;
    }

    if ($company === null) {
        return array(
            'status' => 'missing_company',
            'company' => null,
            'message_code' => 'missing_company',
        );
    }

    return array(
        'status' => 'parsed',
        'company' => $company,
        'message_code' => '',
    );
}

/**
 * Authorize one company-scoped approval CLI execution.
 *
 * The plaintext token is accepted only from process environment and compared
 * to the SHA-256 digest configured for the selected company. This avoids a
 * default company, avoids credentials in command history, and fails closed
 * before a database connection is opened.
 *
 * @param int    $company
 * @param string $provided_token
 * @param array  $connections
 * @return array status and message_code; never returns the credential
 */
function authorize_approval_cron_cli($company, $provided_token, $connections)
{
    $company = (int)$company;
    if (!isset($connections[$company]) || !is_array($connections[$company])) {
        return array('status' => 'denied', 'message_code' => 'unknown_company');
    }

    $configured_hash = isset($connections[$company]['approval_cron_token_hash'])
        ? strtolower(trim((string)$connections[$company]['approval_cron_token_hash'])) : '';
    if (!preg_match('/^[a-f0-9]{64}$/', $configured_hash)) {
        return array('status' => 'denied', 'message_code' => 'credential_not_configured');
    }

    $provided_token = (string)$provided_token;
    if ($provided_token === '') {
        return array('status' => 'denied', 'message_code' => 'credential_missing');
    }

    $provided_hash = hash('sha256', $provided_token);
    $matched = function_exists('hash_equals')
        ? hash_equals($configured_hash, $provided_hash)
        : approval_cron_constant_time_equals($configured_hash, $provided_hash);

    return $matched
        ? array('status' => 'authorized', 'message_code' => '')
        : array('status' => 'denied', 'message_code' => 'credential_invalid');
}

/**
 * PHP 5.6-compatible constant-time comparison fallback.
 *
 * @param string $known
 * @param string $provided
 * @return bool
 */
function approval_cron_constant_time_equals($known, $provided)
{
    $known = (string)$known;
    $provided = (string)$provided;
    if (strlen($known) !== strlen($provided))
        return false;

    $difference = 0;
    $length = strlen($known);
    for ($i = 0; $i < $length; $i++)
        $difference |= ord($known[$i]) ^ ord($provided[$i]);

    return $difference === 0;
}
