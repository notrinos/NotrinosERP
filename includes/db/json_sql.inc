<?php
/**
 * Quote JSON as SQL without the HTML transformation used for display strings.
 * Legacy HTML-encoded JSON is accepted only when one decode makes it valid.
 *
 * @param string|array|object|null $value JSON text or a structured value.
 * @param bool $nullify Whether a PHP null should be stored as SQL NULL.
 * @return string SQL literal; malformed input raises InvalidArgumentException.
 */
function db_escape_json($value, $nullify = false)
{
    if ($value === null && $nullify)
        return 'NULL';
    if (!is_string($value) && !is_array($value) && !is_object($value) && $value !== null)
        throw new InvalidArgumentException('Expected JSON text or structured data.');
    $json = is_string($value) ? $value : json_encode($value);
    if (!is_string($json))
        throw new InvalidArgumentException('JSON encoding failed.');
    json_decode($json);
    if (json_last_error() !== JSON_ERROR_NONE) {
        $json = html_entity_decode($json, ENT_QUOTES, 'UTF-8');
        json_decode($json);
        if (json_last_error() !== JSON_ERROR_NONE)
            throw new InvalidArgumentException('Invalid JSON database value.');
    }
    return "'".db_escape_function($json)."'";
}
