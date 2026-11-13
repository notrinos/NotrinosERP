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
 * DIM-GL-001: honest document-level classification for multi-line GL/bank documents.
 *
 * A GL journal, bank payment, bank deposit or bank transfer may carry a different Dimension pair
 * on every line. A single header row cannot reproduce that intent, and the legacy bank writer
 * either omitted the bank axes or copied the last cart line's pair onto the whole balancing amount.
 * This file owns the one summary rule those writers use:
 *
 *  - `single`      every line carries the same non-zero classification, so the header may name it;
 *  - `mixed`       the lines disagree (a non-zero pair beside an unassigned line also disagrees),
 *                  so the header records that fact and never a whole-amount attribution;
 *  - `unassigned`  no line carries a classification.
 *
 * The per-line ledger rows and the approval draft stay authoritative for the exact pairs. The bank
 * source row only *declares* the document-level state in the existing `custom_data` column, because
 * `bank_trans.dimension_id`/`dimension2_id` are 0 both for a deliberate mixed/unassigned document
 * and for a legacy writer that omitted the axes. A reader must therefore never trust a zero bank
 * axis as a classification: a missing declaration is reported as `legacy_unrecorded`, not as
 * "unassigned". This module is a pure contract; it writes nothing, reads no database and grants no
 * authority.
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_validation_contract.inc';

/**
 * States a maintained writer may declare for one document or one source line.
 *
 * @return string[] Stable state identifiers
 */
function dimension_document_classification_states()
{
	return array('single', 'mixed', 'unassigned');
}

/**
 * States a reader of a stored source row may report.
 *
 * The last two are explicit historical dispositions, never a classification.
 *
 * @return string[]
 */
function dimension_document_classification_read_states()
{
	return array('single', 'mixed', 'unassigned', 'legacy_unrecorded', 'legacy_axes', 'invalid');
}

/**
 * Versioned contract identifier stored inside the retained declaration.
 *
 * It deliberately contains no slash so PHP 5.6 and PHP 8.x produce byte-identical JSON without
 * depending on JSON_UNESCAPED_SLASHES.
 *
 * @return string
 */
function dimension_document_classification_contract()
{
	return 'dimension-document-classification-1';
}

/**
 * Retained `custom_data` key that owns the declaration.
 *
 * @return string
 */
function dimension_document_classification_payload_key()
{
	return 'dimension_classification';
}

/**
 * Canonical JSON declaration of one document-level state.
 *
 * @param string $state One of dimension_document_classification_states()
 * @return string|false Exact retained JSON, or false for an unknown state
 */
function dimension_document_classification_payload($state)
{
	if (!in_array($state, dimension_document_classification_states(), true))
		return false;
	$payload = array(
		dimension_document_classification_payload_key() => array(
			'contract' => dimension_document_classification_contract(),
			'state' => $state));
	$json = json_encode($payload);
	return is_string($json) ? $json : false;
}

/**
 * Read the exact pair of one documented line without trusting an object/array shape.
 *
 * @param mixed $line Cart line object or associative array
 * @return array|false dimension_id/dimension2_id as normalized integers, or false when invalid
 */
function dimension_document_classification_line_pair($line)
{
	if (is_object($line)) {
		$dimension_id = isset($line->dimension_id) ? $line->dimension_id : 0;
		$dimension2_id = isset($line->dimension2_id) ? $line->dimension2_id : 0;
	} elseif (is_array($line)) {
		$dimension_id = array_key_exists('dimension_id', $line) ? $line['dimension_id'] : 0;
		$dimension2_id = array_key_exists('dimension2_id', $line) ? $line['dimension2_id'] : 0;
	} else {
		return false;
	}
	$id = dimension_validation_legacy_id($dimension_id);
	$id2 = dimension_validation_legacy_id($dimension2_id);
	if ($id === false || $id2 === false)
		return false;
	return array('dimension_id' => $id, 'dimension2_id' => $id2);
}

/**
 * Summarize the exact classification of the lines that make up one document.
 *
 * Zero/blank pairs are "no Dimension on this axis" and are compared as a pair like any other, so a
 * document with one classified line and one deliberately unassigned line is `mixed`, not `single`.
 * The result is descriptive: master lifecycle and company policy remain the DIM-FND-004/007 owners.
 *
 * @param array $lines Cart lines (gl_item objects or arrays) or plain pair arrays
 * @return array ok, state, pair, distinct, line_count, errors and codes
 */
function dimension_document_classification_summary($lines)
{
	$result = array('ok' => false, 'state' => 'unassigned',
		'pair' => array('dimension_id' => 0, 'dimension2_id' => 0), 'distinct' => array(),
		'line_count' => 0, 'errors' => array(), 'codes' => array());
	if (!is_array($lines)) {
		$result['codes'][] = 'invalid_lines';
		$result['errors'][] = _('The document lines could not be read for Dimension classification.');
		return $result;
	}
	$distinct = array();
	$number = 0;
	foreach ($lines as $line) {
		$number++;
		$pair = dimension_document_classification_line_pair($line);
		if ($pair === false) {
			$result['codes'][] = 'invalid_line';
			$result['errors'][] = sprintf(_('Line %d does not carry a stored Dimension pair.'), $number);
			return $result;
		}
		$key = $pair['dimension_id'].'/'.$pair['dimension2_id'];
		if (!isset($distinct[$key]))
			$distinct[$key] = array('dimension_id' => $pair['dimension_id'],
				'dimension2_id' => $pair['dimension2_id'], 'lines' => 0);
		$distinct[$key]['lines']++;
		$result['line_count']++;
	}
	ksort($distinct, SORT_STRING);
	$result['distinct'] = array_values($distinct);
	if (count($distinct) === 0) {
		$result['state'] = 'unassigned';
	} elseif (count($distinct) === 1) {
		$only = $result['distinct'][0];
		$result['pair'] = array('dimension_id' => $only['dimension_id'],
			'dimension2_id' => $only['dimension2_id']);
		$result['state'] = ($only['dimension_id'] === 0 && $only['dimension2_id'] === 0)
			? 'unassigned' : 'single';
	} else {
		$result['state'] = 'mixed';
	}
	$result['ok'] = true;
	return $result;
}

/**
 * Project one summary onto the exact retained source-row declaration.
 *
 * Only `single` carries the pair. `mixed` and `unassigned` carry explicit zero axes plus a marker,
 * so a later reader can distinguish a deliberate mixed document from a legacy writer that simply
 * left the columns at their default. The caller must persist all three returned values together.
 *
 * @param array $summary dimension_document_classification_summary() result
 * @return array|false state, dimension_id, dimension2_id and custom_data
 */
function dimension_document_classification_row($summary)
{
	if (!is_array($summary) || empty($summary['ok'])
		|| !isset($summary['state'], $summary['pair'])
		|| !in_array($summary['state'], dimension_document_classification_states(), true))
		return false;
	$payload = dimension_document_classification_payload($summary['state']);
	if ($payload === false)
		return false;
	$dimension_id = $summary['state'] === 'single'
		? dimension_validation_legacy_id($summary['pair']['dimension_id']) : 0;
	$dimension2_id = $summary['state'] === 'single'
		? dimension_validation_legacy_id($summary['pair']['dimension2_id']) : 0;
	if ($dimension_id === false || $dimension2_id === false
		|| ($summary['state'] === 'single' && ($dimension_id < 1 || $dimension2_id < 1))
		|| ($summary['state'] !== 'single' && ($dimension_id !== 0 || $dimension2_id !== 0)))
		return false;
	return array('state' => $summary['state'], 'dimension_id' => $dimension_id,
		'dimension2_id' => $dimension2_id, 'custom_data' => $payload);
}

/**
 * Encode one single source line (used for a per-line destination bank row).
 *
 * @param mixed $dimension_id Axis-1 value of the line
 * @param mixed $dimension2_id Axis-2 value of the line
 * @return array|false Retained row declaration, or false for an invalid pair
 */
function dimension_document_classification_line($dimension_id, $dimension2_id)
{
	$summary = dimension_document_classification_summary(array(array(
		'dimension_id' => $dimension_id, 'dimension2_id' => $dimension2_id)));
	return $summary['ok'] ? dimension_document_classification_row($summary) : false;
}

/**
 * Read a stored source-row declaration without trusting a legacy zero axis.
 *
 * @param mixed $custom_data Retained bank/source payload
 * @param mixed $dimension_id Stored axis-1 value
 * @param mixed $dimension2_id Stored axis-2 value
 * @return array ok, state, trusted, pair, errors and codes
 */
function dimension_document_classification_read($custom_data, $dimension_id, $dimension2_id)
{
	$result = array('ok' => false, 'state' => 'invalid', 'trusted' => false,
		'pair' => array('dimension_id' => 0, 'dimension2_id' => 0),
		'errors' => array(), 'codes' => array());
	$id = dimension_validation_legacy_id($dimension_id);
	$id2 = dimension_validation_legacy_id($dimension2_id);
	if ($id === false || $id2 === false) {
		$result['codes'][] = 'invalid_stored_pair';
		$result['errors'][] = _('The stored source row carries an unusable Dimension pair.');
		return $result;
	}
	$result['pair'] = array('dimension_id' => $id, 'dimension2_id' => $id2);
	if ($custom_data === null || $custom_data === '')
		$custom_data = '{}';
	$payload = is_string($custom_data) ? json_decode($custom_data, true) : false;
	if (!is_array($payload) || json_last_error() !== JSON_ERROR_NONE) {
		$result['codes'][] = 'invalid_payload';
		$result['errors'][] = _('The retained source payload is not a readable JSON object.');
		return $result;
	}
	$field = dimension_document_classification_payload_key();
	if (!array_key_exists($field, $payload)) {
		$result['ok'] = true;
		$result['state'] = ($id === 0 && $id2 === 0) ? 'legacy_unrecorded' : 'legacy_axes';
		return $result;
	}
	$declaration = $payload[$field];
	if (!is_array($declaration) || !isset($declaration['contract'], $declaration['state'])
		|| $declaration['contract'] !== dimension_document_classification_contract()
		|| !in_array($declaration['state'], dimension_document_classification_states(), true)) {
		$result['codes'][] = 'invalid_declaration';
		$result['errors'][] = _('The retained document classification declaration is not valid.');
		return $result;
	}
	$state = $declaration['state'];
	if (($state === 'single' && ($id < 1 || $id2 < 1))
		|| ($state !== 'single' && ($id !== 0 || $id2 !== 0))) {
		$result['codes'][] = 'declaration_mismatch';
		$result['errors'][] = _('The retained document classification does not match the stored axes.');
		return $result;
	}
	$result['ok'] = true;
	$result['state'] = $state;
	$result['trusted'] = true;
	return $result;
}

/**
 * Is one read state a classification recorded by a maintained writer?
 *
 * @param string $state dimension_document_classification_read() state
 * @return bool
 */
function dimension_document_classification_is_recorded($state)
{
	return in_array($state, dimension_document_classification_states(), true);
}

/**
 * Translate one state into an honest review label; legacy states are never called "unassigned".
 *
 * @param string $state Read or writer state
 * @return string
 */
function dimension_document_classification_label($state)
{
	switch ((string)$state) {
		case 'single':
			return _('Classified');
		case 'mixed':
			return _('Mixed classifications');
		case 'unassigned':
			return _('Explicitly unassigned');
		case 'legacy_unrecorded':
			return _('Legacy source row without a recorded classification');
		case 'legacy_axes':
			return _('Legacy source row with unverified axes');
		default:
			return _('Unreadable classification');
	}
}
