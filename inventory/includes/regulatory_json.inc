<?php
/** SQL-only JSON serialization for retained regulatory metadata. */

/**
 * Encode metadata without HTML transformation. Reject encoding failures before
 * a writer executes SQL; never repair or reinterpret previously retained rows.
 * Use the selected driver's escaping, as the native attachment JSON writer does.
 *
 * @param mixed $value JSON-serializable regulatory metadata
 * @return string|false Quoted SQL JSON literal, or false on encoding failure
 */
function regulatory_json_sql($value)
{
    $json = json_encode($value);
    if ($json === false) return false;
    return "'".db_escape_function($json)."'";
}
