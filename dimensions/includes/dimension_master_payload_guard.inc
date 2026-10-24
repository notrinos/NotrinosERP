<?php
/** B02 ordinary supplier/customer metadata custody; no protected payload decoder. */
require_once __DIR__.'/dimension_reference_registry.inc';

/**
 * Project native ordinary JSON references by exact structural path. Unknown
 * envelopes/governed axes remain opaque; never decode embedded strings or PHP
 * objects. Null/empty storage is the native absence of metadata. The existing
 * observer determines supported ordinary intent before any projection is used.
 *
 * @return array|false Path => [native identity, native axis], or unknown
 */
function dimension_master_payload_references($raw)
{
    if ($raw === null || $raw === '') return array();
    if (!is_string($raw)) return false;
    $payload = json_decode($raw, true);
    if (!is_array($payload) || json_last_error() !== JSON_ERROR_NONE
        || dimension_master_payload_duplicate_keys($raw)
        || dimension_reference_payload_status($payload, PHP_INT_MAX) === 'unknown') return false;
    $references = array();
    return dimension_master_payload_walk($payload, '', $references, 0) ? $references : false;
}

/**
 * Reject ambiguous object keys, including escaped spelling aliases. Native JSON
 * decoding otherwise discards earlier duplicate values and could hide retained
 * intent. Inspect lexical keys only after native JSON validation; never decode
 * strings as nested payloads. Repeated keys in distinct objects remain valid.
 */
function dimension_master_payload_duplicate_keys($raw)
{
    $count = preg_match_all('/"(?:[^"\\\\]|\\\\.)*"|[{}\[\]]/s', $raw, $tokens, PREG_OFFSET_CAPTURE);
    if ($count === false) return true; // Parser/resource failure is unknown.
    $stack = array();
    foreach ($tokens[0] as $token) {
        $text = $token[0];
        if ($text === '{' || $text === '[') { $stack[] = array('kind'=>$text, 'keys'=>array()); continue; }
        if ($text === '}' || $text === ']') { array_pop($stack); continue; }
        $is_key = preg_match('/\G\s*:/', $raw, $following, 0, $token[1]+strlen($text));
        if ($is_key === false) return true;
        if (!$is_key) continue;
        $index = count($stack)-1;
        if ($index < 0 || $stack[$index]['kind'] !== '{') return true;
        $key = json_decode($text, true);
        if (isset($stack[$index]['keys'][$key])) return true;
        $stack[$index]['keys'][$key] = true;
    }
    return false;
}

/** Collect only the ordinary native slot keys; independent HR/payroll axes deny. */
function dimension_master_payload_walk($payload, $path, &$references, $depth)
{
    if ($depth > 64) return false;
    $axes = array('dimension_id'=>1, 'dimension'=>1, 'dimension2_id'=>2, 'dimension2'=>2);
    $independent = array('cost_center_id', 'legacy_cost_center_id', 'legacy_dimension2_id',
        'core_dimension_id', 'core_dimension2_id', 'axis_value_key', 'core_slot_hint');
    foreach ($payload as $key=>$value) {
        // Length-prefixed keys cannot collide when names contain separators.
        $next_path = $path.'/'.strlen((string)$key).':'.$key;
        if (in_array($key, $independent, true)) return false;
        if (isset($axes[$key])) {
            $id = dimension_writer_reference_id($value);
            if ($id === false || $value === null || $value === '') return false;
            if ($id > 0) $references[$next_path] = array($id, $axes[$key]);
        } elseif (is_array($value)) {
            if (!dimension_master_payload_walk($value, $next_path, $references, $depth+1)) return false;
        } elseif (is_string($value) && (preg_match('/^(?:\s*[\{\[]|[aOs]:[0-9]+:)/', $value)
            || strpos($value, 'pay_glb_001_native_approval_schema') !== false
            || strpos($value, 'native-result-v2') !== false)) {
            return false;
        }
    }
    return true;
}

/**
 * Preserve retained references at their original paths. Opaque historical bytes
 * can be maintained exactly, but replacement/removal needs owner recovery. No
 * historical reference is silently cleared, moved or remapped. Return only new
 * references requiring current open/axis validation, plus all IDs to serialize.
 * This is a writer guard, never an unused certificate or deletion authority.
 */
function dimension_master_payload_transition($old, $next, $remove = false)
{
    $before = dimension_master_payload_references($old);
    $after = dimension_master_payload_references($next);
    if ($before === false || $after === false) {
        if (!$remove && $old === $next) return array('locks'=>array(), 'new'=>array());
        return false;
    }
    if ($remove && count($before)) return false;
    foreach ($before as $path=>$reference)
        if (!isset($after[$path]) || $after[$path] !== $reference) return false;
    $result = array('locks'=>array(), 'new'=>array());
    foreach (array($before, $after) as $references)
        foreach ($references as $reference) $result['locks'][$reference[0]] = $reference[0];
    foreach ($after as $path=>$reference)
        if (!isset($before[$path])) $result['new'][] = $reference;
    return $result;
}
