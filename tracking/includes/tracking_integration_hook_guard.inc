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
 * DIM-INT-001: versioned Dimension payload and extension-hook intent guard.
 *
 * Extension hooks (`db_prewrite` / `db_postwrite`) receive the whole document by reference, so an
 * installed extension can change a Dimension value or add a coordinate between the moment the
 * operator and the approval path decided the classification and the moment the shared writers post it.
 * This file seals the Dimension-bearing state of a document before the hooks run and proves it again
 * afterwards:
 *
 *  - a `db_prewrite` hook that changes, removes or adds a Dimension coordinate (including a third
 *    or later axis that this release cannot post) is refused with a stable code; the sealed values
 *    are restored first, so a caller that catches the refusal never continues with altered intent;
 *  - a `db_postwrite` hook cannot undo a committed document, so an alteration is restored and
 *    recorded but never throws;
 *  - the sealed state is a versioned payload (`dimension_integration_serialize()`); a reader of a
 *    payload with another version, an unknown axis, an unknown field or an invalid reference fails
 *    closed instead of dropping what it does not understand.
 *
 * The file is self-contained (no includes, no database) because `includes/hooks.inc` loads it before
 * the session and company connection exist. With no installed extension implementing the hook the
 * guard does nothing and costs nothing.
 */

/** Refusal raised when an extension hook altered the Dimension intent of a document being written. */
class DimensionIntegrationHookIntentException extends RuntimeException
{
	/** @var string[] Stable refusal codes */
	public $codes = array();
	/** @var string[] Dimension-bearing property paths that differ */
	public $paths = array();
}

/**
 * Version of the companion Dimension payload written and read by integration adapters.
 *
 * @return int Supported payload version
 */
function dimension_integration_payload_version()
{
	return 1;
}

/**
 * The native axes this release can post.
 *
 * @return array Axis number => native column
 */
function dimension_integration_axes()
{
	return array(1 => 'dimension_id', 2 => 'dimension2_id');
}

/**
 * Classify a property name as Dimension-bearing.
 *
 * @param mixed $name Property or array key
 * @return array|false kind (id|detail|intent) and axis (0 when the name carries no axis), or false
 */
function dimension_integration_property_kind($name)
{
	if (!is_string($name))
		return false;
	if (preg_match('/^(?:dimension|dim)([0-9]*)(?:_id)?$/Di', $name, $match))
		return array('kind' => 'id', 'axis' => $match[1] === '' ? 1 : (int)$match[1]);
	if (preg_match('/^dimension([0-9]*)_(?:intent|mode|origin)$/Di', $name, $match))
		return array('kind' => 'detail', 'axis' => $match[1] === '' ? 1 : (int)$match[1]);
	if (preg_match('/^(?:classification_intents|dimension_intents?)$/Di', $name))
		return array('kind' => 'intent', 'axis' => 0);
	return false;
}

/**
 * Normalize one Dimension-bearing value so a type change that keeps the meaning is not an alteration.
 *
 * @param array $kind Result of dimension_integration_property_kind()
 * @param mixed $value Stored value
 * @return string Comparable canonical form
 */
function dimension_integration_normalize_value($kind, $value)
{
	if ($kind['kind'] === 'id') {
		if ($value === null || $value === '' || $value === 0 || $value === '0' || $value === false)
			return '0';
		if (is_int($value) && $value > 0)
			return (string)$value;
		if (is_string($value) && preg_match('/^[1-9][0-9]{0,9}$/D', $value))
			return $value;
	}
	return 'raw:'.serialize($value);
}

/**
 * Collect every Dimension-bearing value of a document.
 *
 * @param mixed $node Document, line or array
 * @param array $path Path of the node from the document root
 * @param array $found Collected entries, by reference
 * @param int $nodes Visited-node counter, by reference
 * @param array $visited Object identities on the current path
 * @return bool False when the traversal bounds were exceeded
 */
function dimension_integration_collect($node, $path, &$found, &$nodes, $visited)
{
	if (++$nodes > 200000 || count($path) > 8)
		return false;
	if (is_object($node)) {
		$identity = spl_object_hash($node);
		if (isset($visited[$identity]))
			return true;
		$visited[$identity] = true;
		$items = get_object_vars($node);
	} elseif (is_array($node)) {
		$items = $node;
	} else {
		return true;
	}
	foreach ($items as $name => $value) {
		$kind = dimension_integration_property_kind($name);
		if ($kind !== false) {
			$found[] = array('path' => array_merge($path, array($name)), 'kind' => $kind,
				'value' => $value, 'canonical' => dimension_integration_normalize_value($kind, $value));
			if ($kind['kind'] !== 'intent')
				continue;
		}
		if (is_object($value) || is_array($value))
			if (!dimension_integration_collect($value, array_merge($path, array($name)), $found, $nodes, $visited))
				return false;
	}
	return true;
}

/**
 * Seal the Dimension-bearing state of a document.
 *
 * @param mixed $document Cart, argument object or array about to be handed to hooks
 * @return array Seal: complete flag, entries and the versioned payload
 */
function dimension_integration_hook_seal($document)
{
	$found = array();
	$nodes = 0;
	$complete = dimension_integration_collect($document, array(), $found, $nodes, array());
	$flat = array();
	foreach ($found as $entry)
		$flat[implode("\x1f", array_map('strval', $entry['path']))] = $entry['canonical'];
	ksort($flat);
	return array('complete' => $complete, 'entries' => $found, 'flat' => $flat,
		'payload' => dimension_integration_serialize_state($flat), 'version' => dimension_integration_payload_version());
}

/**
 * Compare a document with its seal.
 *
 * @param mixed $document Document after the hooks ran
 * @param array $seal Result of dimension_integration_hook_seal()
 * @return array ok flag, stable codes and differing paths
 */
function dimension_integration_hook_verify($document, $seal)
{
	$result = array('ok' => true, 'codes' => array(), 'paths' => array());
	if (!is_array($seal) || empty($seal['complete']) || !isset($seal['flat'], $seal['version'])
		|| (int)$seal['version'] !== dimension_integration_payload_version()) {
		$result['ok'] = false;
		$result['codes'][] = 'seal_unavailable';
		return $result;
	}
	$now = dimension_integration_hook_seal($document);
	if (!$now['complete']) {
		$result['ok'] = false;
		$result['codes'][] = 'seal_unavailable';
		return $result;
	}
	foreach ($seal['flat'] as $key => $canonical) {
		if (!array_key_exists($key, $now['flat'])) {
			$result['codes'][] = 'coordinate_removed';
			$result['paths'][] = str_replace("\x1f", '.', $key);
		} elseif ($now['flat'][$key] !== $canonical) {
			$result['codes'][] = 'coordinate_changed';
			$result['paths'][] = str_replace("\x1f", '.', $key);
		}
	}
	foreach ($now['entries'] as $entry) {
		$key = implode("\x1f", array_map('strval', $entry['path']));
		if (array_key_exists($key, $seal['flat']))
			continue;
		$axis = $entry['kind']['axis'];
		$result['codes'][] = $entry['kind']['kind'] === 'id' && !isset(dimension_integration_axes()[$axis])
			? 'unsupported_axis' : 'coordinate_added';
		$result['paths'][] = str_replace("\x1f", '.', $key);
	}
	$result['codes'] = array_values(array_unique($result['codes']));
	$result['ok'] = !count($result['codes']);
	return $result;
}

/**
 * Put every sealed Dimension value back and drop every coordinate a hook added.
 *
 * @param mixed $document Document, by reference
 * @param array $seal Result of dimension_integration_hook_seal()
 * @return bool True when the restoration was attempted from a complete seal
 */
function dimension_integration_hook_restore(&$document, $seal)
{
	if (!is_array($seal) || empty($seal['complete']))
		return false;
	$now = array();
	$nodes = 0;
	dimension_integration_collect($document, array(), $now, $nodes, array());
	$keep = array();
	foreach ($seal['entries'] as $entry)
		$keep[implode("\x1f", array_map('strval', $entry['path']))] = true;
	foreach ($now as $entry) {
		$key = implode("\x1f", array_map('strval', $entry['path']));
		if (!isset($keep[$key]))
			dimension_integration_path_unset($document, $entry['path']);
	}
	foreach ($seal['entries'] as $entry)
		dimension_integration_path_set($document, $entry['path'], $entry['value']);
	return true;
}

/**
 * Set one value at a path of a document.
 *
 * @param mixed $node Node, by reference
 * @param array $path Property/key path
 * @param mixed $value Value to store
 * @return void
 */
function dimension_integration_path_set(&$node, $path, $value)
{
	$head = array_shift($path);
	if (is_object($node)) {
		if (count($path)) {
			if (isset($node->$head))
				dimension_integration_path_set($node->$head, $path, $value);
		} else {
			$node->$head = $value;
		}
	} elseif (is_array($node)) {
		if (count($path)) {
			if (isset($node[$head]))
				dimension_integration_path_set($node[$head], $path, $value);
		} else {
			$node[$head] = $value;
		}
	}
}

/**
 * Remove one value at a path of a document.
 *
 * @param mixed $node Node, by reference
 * @param array $path Property/key path
 * @return void
 */
function dimension_integration_path_unset(&$node, $path)
{
	$head = array_shift($path);
	if (is_object($node)) {
		if (count($path)) {
			if (isset($node->$head))
				dimension_integration_path_unset($node->$head, $path);
		} else {
			unset($node->$head);
		}
	} elseif (is_array($node)) {
		if (count($path)) {
			if (isset($node[$head]))
				dimension_integration_path_unset($node[$head], $path);
		} else {
			unset($node[$head]);
		}
	}
}

/**
 * Whether any installed extension overrides a document hook (the base class method is a no-op).
 *
 * @param string $method Hook method (db_prewrite or db_postwrite)
 * @return bool
 */
function dimension_integration_hook_active($method)
{
	global $Hooks;
	if (!isset($Hooks) || !is_array($Hooks))
		return false;
	foreach ($Hooks as $hook) {
		if (!is_object($hook) || !method_exists($hook, $method))
			continue;
		// Every extension class inherits the no-op base method; only an override can alter a document.
		$reflection = new ReflectionMethod($hook, $method);
		if ($reflection->getDeclaringClass()->getName() !== 'hooks')
			return true;
	}
	return false;
}

/** @var array Last hook-intent failure of this request (codes and paths only) */
$GLOBALS['dimension_integration_last_failure'] = array();

/**
 * Prove a document still carries the sealed intent after its hooks ran.
 *
 * @param mixed $document Document, by reference
 * @param array $seal Seal taken before the hooks
 * @param string $method db_prewrite (refuse) or db_postwrite (restore and record)
 * @param mixed $type Transaction type, evidence only
 * @return void
 * @throws DimensionIntegrationHookIntentException On a db_prewrite alteration
 */
function dimension_integration_hook_enforce(&$document, $seal, $method, $type = null)
{
	$verdict = dimension_integration_hook_verify($document, $seal);
	if ($verdict['ok']) {
		$GLOBALS['dimension_integration_last_failure'] = array();
		return;
	}
	dimension_integration_hook_restore($document, $seal);
	$GLOBALS['dimension_integration_last_failure'] = array('method' => (string)$method, 'type' => $type,
		'codes' => $verdict['codes'], 'paths' => $verdict['paths']);
	if ($method !== 'db_prewrite')
		return;
	if (function_exists('cancel_transaction'))
		cancel_transaction();
	$exception = new DimensionIntegrationHookIntentException(
		'An extension changed the Dimension classification of the document before it was written ('.implode(', ', $verdict['codes']).'). The document was not posted.');
	$exception->codes = $verdict['codes'];
	$exception->paths = $verdict['paths'];
	throw $exception;
}

/**
 * Serialize a state as the versioned companion payload.
 *
 * @param array $state Canonical key => value map
 * @return string JSON payload
 */
function dimension_integration_serialize_state($state)
{
	return json_encode(array('v' => dimension_integration_payload_version(), 'state' => $state));
}

/**
 * Serialize axis intents as the versioned companion payload.
 *
 * @param array $axes Axis number => array(mode selected|unassigned|inherit, id)
 * @return string|false JSON payload, or false when an axis, mode or identity is not supported
 */
function dimension_integration_serialize($axes)
{
	if (!is_array($axes))
		return false;
	$payload = array('v' => dimension_integration_payload_version(), 'axes' => array());
	ksort($axes);
	foreach ($axes as $axis => $intent) {
		if (!isset(dimension_integration_axes()[$axis]) || !is_array($intent) || !isset($intent['mode']))
			return false;
		$id = isset($intent['id']) ? $intent['id'] : 0;
		if (!is_int($id) || $id < 0 || $id > 2147483647)
			return false;
		if (!in_array($intent['mode'], array('selected', 'unassigned', 'inherit'), true)
			|| ($intent['mode'] === 'selected') !== ($id > 0))
			return false;
		$payload['axes'][(string)$axis] = array('mode' => $intent['mode'], 'id' => $id);
	}
	return json_encode($payload);
}

/**
 * Read a versioned companion payload; anything this release does not understand fails closed.
 *
 * @param string $json Payload text
 * @return array ok flag, stable code and the axis intents
 */
function dimension_integration_deserialize($json)
{
	$result = array('ok' => false, 'code' => 'malformed', 'axes' => array());
	if (!is_string($json) || strlen($json) > 4096)
		return $result;
	$payload = json_decode($json, true);
	if (!is_array($payload) || !array_key_exists('v', $payload))
		return $result;
	if ($payload['v'] !== dimension_integration_payload_version()) {
		$result['code'] = 'unsupported_version';
		return $result;
	}
	if (array_diff(array_keys($payload), array('v', 'axes')) || !isset($payload['axes']) || !is_array($payload['axes'])) {
		$result['code'] = 'unknown_field';
		return $result;
	}
	foreach ($payload['axes'] as $axis => $intent) {
		if (!isset(dimension_integration_axes()[(int)$axis]) || (string)(int)$axis !== (string)$axis) {
			$result['code'] = 'unsupported_axis';
			$result['axes'] = array();
			return $result;
		}
		if (!is_array($intent) || array_diff(array_keys($intent), array('mode', 'id')) || !isset($intent['mode'], $intent['id'])) {
			$result['code'] = 'unknown_field';
			$result['axes'] = array();
			return $result;
		}
		if (!in_array($intent['mode'], array('selected', 'unassigned', 'inherit'), true) || !is_int($intent['id'])
			|| $intent['id'] < 0 || $intent['id'] > 2147483647 || ($intent['mode'] === 'selected') !== ($intent['id'] > 0)) {
			$result['code'] = 'invalid_reference';
			$result['axes'] = array();
			return $result;
		}
		$result['axes'][(int)$axis] = array('mode' => $intent['mode'], 'id' => $intent['id']);
	}
	$result['ok'] = true;
	$result['code'] = '';
	return $result;
}
