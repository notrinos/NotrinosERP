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
 * DIM-FND-006: durable ADR-03 source classification revisions.
 *
 * A transient selector or approval payload is not a durable source snapshot. This file is the one
 * authority that records what a source document and each of its lines was classified as, in an
 * append-only company-prefixed companion table, without touching the legacy document, its native
 * `dimension_id`/`dimension2_id` fields or any ledger row.
 *
 * What it owns:
 *  - a canonical source identity: family, transaction type, document key, line kind and a stable
 *    line key (never a mutable cart-array index);
 *  - an append-only revision chain with a predecessor and a canonical SHA-256 payload seal;
 *  - both native identities plus the DIM-FND-005 modes/origins/bases and the exact published policy
 *    versions that decided them;
 *  - one-time binding of an unsaved draft identity to the final source identity, with retry
 *    protection and no re-binding;
 *  - the read-side snapshot adapters the DIM-FND-005 resolver consumes, so a saved classification
 *    wins a changed customer/supplier/item default after reload.
 *
 * What it deliberately does not own:
 *  - no historical backfill or inferred intent for an unsnapshotted legacy document: a missing
 *    revision is reported `absent` (lineage unavailable), never reconstructed from a current
 *    default;
 *  - no posting: monetary effects stay with the maintained GL writer and DIM-FND-007; the
 *    posting-link lineage lives in dimension_posting_lineage.inc;
 *  - no UI, approval envelope or module writer conversion (DIM-APR-001 and the Sales/Purchasing
 *    module slices own those).
 *
 * PHP 5.6 compatible syntax.
 */
require_once __DIR__.'/dimension_resolution.inc';

/**
 * Reviewed source families that may persist a classification revision.
 *
 * A family is a document owner, not a report or a free label. Adding one requires its own writer
 * contract; an unknown family is refused rather than stored ambiguously.
 *
 * @return string[]
 */
function dimension_source_revision_families()
{
	return array('sales', 'purchasing', 'gl', 'inventory', 'manufacturing');
}

/**
 * Reviewed line kinds.
 *
 * `header` is the document-level classification; `line` is a persisted document line; `draft_line`
 * is a line that has no final source identity yet and must be bound once before execution.
 *
 * @return string[]
 */
function dimension_source_revision_line_kinds()
{
	return array('header', 'line', 'draft_line');
}

/**
 * Physical companion tables owned by this authority.
 *
 * @return string[]
 */
function dimension_source_revision_tables()
{
	return array('dimension_source_revisions');
}

/**
 * Expected column inventory of the revision table.
 *
 * Used by the migration patch, the contract suite and the retention observer so a partial or
 * drifted table fails closed instead of being treated as absence.
 *
 * @return array Column name => true
 */
function dimension_source_revision_schema_columns()
{
	return array(
		'source_revision_id'=>true, 'source_identity_sha256'=>true, 'source_family'=>true,
		'trans_type'=>true, 'document_key'=>true, 'line_kind'=>true, 'line_key'=>true,
		'revision_no'=>true, 'predecessor_revision_id'=>true, 'draft_binding_sha256'=>true,
		'dimension_id'=>true, 'dimension2_id'=>true, 'mode1'=>true, 'mode2'=>true,
		'origin1'=>true, 'origin2'=>true, 'basis1'=>true, 'basis2'=>true,
		'policy1_version'=>true, 'policy2_version'=>true, 'effective_date'=>true,
		'override_reason'=>true, 'line_quantity'=>true, 'resolution_payload'=>true,
		'payload_sha256'=>true, 'actor_id'=>true, 'created_at'=>true);}

/**
 * Active company index of the running request, or 0 when no authenticated session is present.
 *
 * The companion table is company-prefixed, so this value is normally 0 in the physical database it
 * addresses; it is part of the identity digest so a copied database cannot silently reuse another
 * company's identity. There is deliberately no caller-supplied override.
 *
 * @return int
 */
function dimension_source_revision_company()
{
	return isset($_SESSION['wa_current_user']->cur_con)
		? (int)$_SESSION['wa_current_user']->cur_con : 0;
}

/**
 * Authenticated actor identity, or 0 when the request is not authenticated.
 *
 * @return int
 */
function dimension_source_revision_actor()
{
	return isset($_SESSION['wa_current_user']->user)
		? (int)$_SESSION['wa_current_user']->user : 0;
}

/**
 * Check one physical table of the active company prefix.
 *
 * @param string|null $table Unprefixed table name, or null to reset the request cache
 * @param bool $refresh Re-read the schema instead of reusing this request's answer
 * @return string available|absent|unknown
 */
function dimension_source_revision_table_state($table = null, $refresh = false)
{
	static $states = array();
	if ($refresh)
		$states = array();
	if (!is_string($table))
		return 'unknown';
	if (!preg_match('/^[a-z][a-z0-9_]*$/D', $table))
		return 'unknown';
	if (isset($states[$table]))
		return $states[$table];
	$result = db_query('SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE()'
		." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."' AND TABLE_TYPE='BASE TABLE'");
	if (!$result)
		return $states[$table] = 'unknown';
	$row = db_fetch_row($result);
	return $states[$table] = ($row && (int)$row[0] === 1) ? 'available' : 'absent';
}

/**
 * Check that one exact physical column exists in the active company schema.
 *
 * @param string $table Unprefixed table name
 * @param string $column Exact column name
 * @return bool
 */
function dimension_source_revision_column_available($table, $column)
{
	if (dimension_source_revision_table_state($table) !== 'available')
		return false;
	if (!is_string($column) || !preg_match('/^[a-z][a-z0-9_]*$/D', $column))
		return false;
	$result = db_query('SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE()'
		." AND BINARY TABLE_NAME=BINARY '".db_escape_function(TB_PREF.$table)."'"
		." AND BINARY COLUMN_NAME=BINARY '".db_escape_function($column)."'");
	if (!$result)
		return false;
	$row = db_fetch_row($result);
	return $row && (int)$row[0] === 1;
}

/**
 * Is the complete revision schema installed for the active company?
 *
 * @param bool $refresh Re-read the schema instead of reusing this request's answer
 * @return bool|string True when ready, 'absent' when the table is missing, 'partial' when an
 *                     installed table lacks a registered column, false when unreadable
 */
function dimension_source_revision_schema_ready($refresh = false)
{
	if ($refresh)
		dimension_source_revision_table_state(null, true);
	$state = dimension_source_revision_table_state('dimension_source_revisions');
	if ($state === 'unknown')
		return false;
	if ($state === 'absent')
		return 'absent';
	foreach (array_keys(dimension_source_revision_schema_columns()) as $column)
		if (!dimension_source_revision_column_available('dimension_source_revisions', $column))
			return 'partial';
	return true;
}

/**
 * Parse an exact non-negative whole number from an int or canonical digit string.
 *
 * @param mixed $value Raw value
 * @param int $minimum Inclusive lower bound
 * @param int $maximum Inclusive upper bound
 * @return int|false
 */
function dimension_source_revision_int($value, $minimum = 0, $maximum = 2147483647)
{
	if (is_int($value)) {
		$integer = $value;
	} elseif (is_string($value) && preg_match('/^(?:0|[1-9][0-9]{0,9})$/D', $value)) {
		$integer = (int)$value;
		if ((string)$integer !== $value)
			return false;
	} else {
		return false;
	}
	if ($integer < $minimum || $integer > $maximum)
		return false;
	return $integer;
}

/**
 * Validate one bounded identifier used as a family, kind, origin or basis token.
 *
 * @param mixed $value Raw value
 * @param int $maximum Maximum length
 * @return string|false
 */
function dimension_source_revision_clean_token($value, $maximum)
{
	if (!is_string($value) || $value === '')
		return false;
	if (strlen($value) > (int)$maximum || !preg_match('/^[a-z][a-z0-9_]{0,63}$/D', $value))
		return false;
	return $value;
}

/**
 * Validate one stored key (document, line or account) without control or separator characters.
 *
 * @param mixed $value Raw value
 * @param int $maximum Maximum length
 * @param bool $allow_empty Whether an empty string is a valid "no key"
 * @return string|false
 */
function dimension_source_revision_clean_key($value, $maximum, $allow_empty = false)
{
	if (!is_string($value) && !is_int($value))
		return false;
	$value = (string)$value;
	if ($value === '')
		return $allow_empty ? '' : false;
	if (strlen($value) > (int)$maximum)
		return false;
	if (!preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]*$/D', $value))
		return false;
	return $value;
}

/**
 * Validate free text that will be stored and displayed (override reason).
 *
 * @param mixed $value Raw value
 * @param int $maximum Maximum characters
 * @return string|null Trimmed text, or null for a control character, angle bracket or overlong value
 */
function dimension_source_revision_clean_text($value, $maximum)
{
	if ($value === null)
		return '';
	if (!is_scalar($value))
		return null;
	$value = trim((string)$value);
	if (preg_match('/[\x00-\x1F\x7F<>]/', $value))
		return null;
	$length = function_exists('mb_strlen') ? mb_strlen($value, 'UTF-8') : strlen($value);
	return $length > (int)$maximum ? null : $value;
}

/**
 * Canonical final document key of a persisted source number.
 *
 * @param mixed $document_no Positive stored transaction number
 * @return string|false
 */
function dimension_source_revision_document_key($document_no)
{
	$identity = dimension_source_revision_int($document_no, 1);
	return $identity === false ? false : (string)$identity;
}

/**
 * Canonical draft identity of an unsaved document.
 *
 * The caller supplies an opaque draft token that must survive save/reload (a session draft key or a
 * registered approval draft key). The returned key names no business row and cannot be parsed back
 * into one, so it can never masquerade as a final source identity.
 *
 * @param mixed $draft_token Bounded opaque token
 * @return string|false
 */
function dimension_source_revision_draft_key($draft_token)
{
	$token = dimension_source_revision_clean_key($draft_token, 64);
	if ($token === false || strlen($token) < 8)
		return false;
	return 'draft:'.hash('sha256', 'dimension-draft|'.$token);
}

/**
 * Is a document key an unsaved draft identity?
 *
 * @param string $document_key Canonical key
 * @return bool
 */
function dimension_source_revision_is_draft_key($document_key)
{
	return is_string($document_key) && strpos($document_key, 'draft:') === 0;
}

/**
 * Stable line key of one persisted source line or header.
 *
 * A header key is the constant `header` (the document key already identifies it). A persisted line
 * must supply its stored identity, never the current position in a cart array. A draft line derives
 * its key from the opaque draft token and a caller-supplied line token.
 *
 * @param string $line_kind header, line or draft_line
 * @param mixed $identity Stored line identity or opaque draft line token
 * @return string|false
 */
function dimension_source_revision_line_key($line_kind, $identity)
{
	if ($line_kind === 'header')
		return 'header';
	if ($line_kind === 'line') {
		$stored = dimension_source_revision_int($identity, 1);
		if ($stored !== false)
			return 'line:'.$stored;
		$token = dimension_source_revision_clean_key($identity, 64);
		return $token === false ? false : 'line:'.$token;
	}
	if ($line_kind === 'draft_line') {
		$token = dimension_source_revision_clean_key($identity, 64);
		if ($token === false || strlen($token) < 4)
			return false;
		return 'draft:'.hash('sha256', 'dimension-draft-line|'.$token);
	}
	return false;
}

/**
 * Is one canonical stable line key consistent with its declared kind?
 *
 * @param string $line_kind header, line or draft_line
 * @param string $line_key Canonical key
 * @return bool
 */
function dimension_source_revision_valid_line_key($line_kind, $line_key)
{
	if (!is_string($line_key))
		return false;
	if ($line_kind === 'header')
		return $line_key === 'header';
	if ($line_kind === 'line')
		return preg_match('/^line:(?:[1-9][0-9]{0,9}|[A-Za-z0-9][A-Za-z0-9_.:-]{0,63})$/D', $line_key) === 1;
	if ($line_kind === 'draft_line')
		return preg_match('/^draft:[a-f0-9]{64}$/D', $line_key) === 1;
	return false;
}

/**
 * Quote one exact text value for a legacy SQL statement without HTML entity encoding.
 *
 * The generic `db_escape()` helper HTML-encodes quotes and ampersands for page safety, which would
 * corrupt a canonical JSON payload and its digest. Stored evidence therefore uses the driver's
 * string escaping only; callers never build a query from unvalidated input.
 *
 * @param mixed $value Exact text value
 * @return string Quoted SQL literal
 */
function dimension_source_revision_quote($value)
{
	if ($value === null || $value === '')
		return "''";
	return "'".db_escape_function((string)$value)."'";
}

/**
 * Canonical length-prefixed field encoding used by identity and payload digests.
 *
 * Length prefixes make the encoding unambiguous even when a stored text value contains the
 * separator (`|`) or a colon.
 *
 * @param array $fields Name => scalar value, in the declared order
 * @return string
 */
function dimension_source_revision_canonical($fields)
{
	$canonical = '';
	foreach ($fields as $name => $value)
		$canonical .= $name.'='.strlen((string)$value).':'.(string)$value.';';
	return $canonical;
}

/**
 * Recursively canonicalise a resolution payload for deterministic JSON storage and sealing.
 *
 * Only JSON-safe scalars and arrays are accepted. Floats, objects and resources are refused so two
 * runs cannot produce different bytes for the same logical evidence on different runtimes.
 *
 * @param mixed $value Raw payload value
 * @param int $depth Current depth
 * @return mixed Canonical value, or false for an unsupported or over-deep value
 */
function dimension_source_revision_canonical_payload($value, $depth = 0)
{
	if ($depth > 8)
		return false;
	if ($value === null || is_bool($value) || is_int($value) || is_string($value))
		return $value;
	if (is_float($value) || is_object($value) || is_resource($value))
		return false;
	if (!is_array($value))
		return false;
	if (!count($value))
		return array();
	$keys = array_keys($value);
	$list = true;
	foreach ($keys as $index => $key)
		if ($key !== $index) {
			$list = false;
			break;
		}
	$canonical = array();
	if ($list) {
		foreach ($value as $entry) {
			$normalized = dimension_source_revision_canonical_payload($entry, $depth + 1);
			if ($normalized === false && $entry !== false) return false;
			$canonical[] = $normalized;
		}
		return $canonical;
	}
	ksort($value, SORT_STRING);
	foreach ($value as $key => $entry) {
		$normalized = dimension_source_revision_canonical_payload($entry, $depth + 1);
		if ($normalized === false && $entry !== false) return false;
		$canonical[(string)$key] = $normalized;
	}
	return $canonical;
}

/**
 * Canonical JSON text of a resolution payload.
 *
 * @param mixed $value Resolution evidence
 * @return string|false
 */
function dimension_source_revision_payload_json($value)
{
	$canonical = dimension_source_revision_canonical_payload($value);
	if ($canonical === false)
		return false;
	$json = json_encode($canonical, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
	return is_string($json) ? $json : false;
}

/**
 * Parse one non-negative canonical decimal with at most eight fraction digits.
 *
 * The 16-digit integer bound fits the declared decimal(24,8). A float is refused on purpose: a
 * binary float cannot prove an exact recorded quantity.
 *
 * @param mixed $value Int or decimal string
 * @param bool $allow_negative Whether a signed contribution is admitted
 * @param int $max_integer_digits Maximum integer digits
 * @return string|false Canonical `integer.fraction` with exactly eight fraction digits
 */
function dimension_source_revision_decimal($value, $allow_negative = false, $max_integer_digits = 16)
{
	if (is_int($value))
		$value = (string)$value;
	if (!is_string($value))
		return false;
	$value = trim($value);
	if (!preg_match('/^(-?)(0|[1-9][0-9]*)(?:\.([0-9]{1,8}))?$/D', $value, $match))
		return false;
	$negative = $match[1] === '-';
	$integer = $match[2];
	if (strlen($integer) > (int)$max_integer_digits)
		return false;
	$fraction = isset($match[3]) ? $match[3] : '';
	$fraction = str_pad($fraction, 8, '0');
	$zero = $integer === '0' && trim($fraction, '0') === '';
	if ($negative && !$allow_negative && !$zero)
		return false;
	return ($negative && !$zero ? '-' : '').$integer.'.'.$fraction;
}

/**
 * Compare two canonical non-negative decimals without floating point.
 *
 * @param string $left Canonical decimal
 * @param string $right Canonical decimal
 * @return int|false -1, 0 or 1, or false when either value is not canonical
 */
function dimension_source_revision_decimal_compare($left, $right)
{
	if (dimension_source_revision_decimal($left, false) !== $left
		|| dimension_source_revision_decimal($right, false) !== $right)
		return false;
	$left_parts = explode('.', $left);
	$right_parts = explode('.', $right);
	if (strlen($left_parts[0]) !== strlen($right_parts[0]))
		return strlen($left_parts[0]) < strlen($right_parts[0]) ? -1 : 1;
	$comparison = strcmp($left_parts[0], $right_parts[0]);
	if ($comparison !== 0)
		return $comparison < 0 ? -1 : 1;
	$comparison = strcmp($left_parts[1], $right_parts[1]);
	return $comparison === 0 ? 0 : ($comparison < 0 ? -1 : 1);
}

/**
 * Compare the maintained GL writer's stored double with a recorded 8-decimal contribution.
 *
 * `gl_trans.amount` is a legacy double; the rounding the writer already applied is what a posting
 * link records, so the comparison is made at the recorded eight-decimal scale rather than with a
 * binary float equality.
 *
 * @param mixed $stored Stored GL amount
 * @param string $canonical Recorded signed contribution
 * @return bool
 */
function dimension_source_revision_amount_matches($stored, $canonical)
{
	$expected = dimension_source_revision_decimal($canonical, true);
	if ($expected === false)
		return false;
	$actual = dimension_source_revision_decimal(sprintf('%.8F', (float)$stored), true);
	return $actual !== false && $actual === $expected;
}

/**
 * Validate one exact ISO calendar date.
 *
 * @param mixed $value Raw date
 * @return string|false
 */
function dimension_source_revision_date($value)
{
	if (!is_string($value) || !preg_match('/^([0-9]{4})-([0-9]{2})-([0-9]{2})$/D', $value, $match))
		return false;
	if (!checkdate((int)$match[2], (int)$match[3], (int)$match[1]))
		return false;
	return $value;
}

/**
 * Normalize one resolved axis into its durable evidence.
 *
 * Only a final decision is durable: `selected`, `unassigned` or policy-derived `not_applicable`.
 * `inherit` with no winner and the adapter-only `legacy_unspecified` are not intent and are refused.
 *
 * @param int $axis Native axis 1 or 2
 * @param array $entry value/mode/origin/basis/policy_version
 * @return array ok, axis, errors and codes
 */
function dimension_source_revision_normalize_axis($axis, $entry)
{
	$result = array('ok' => false, 'axis' => (int)$axis, 'errors' => array(), 'codes' => array());
	if ($axis !== 1 && $axis !== 2) {
		$result['codes'][] = 'invalid_axis';
		$result['errors'][] = _('A Dimension source revision axis must be category 1 or 2.');
		return $result;
	}
	if (!is_array($entry)) {
		$result['codes'][] = 'invalid_axis_evidence';
		$result['errors'][] = _('Every Dimension source revision axis needs resolved evidence.');
		return $result;
	}
	$value = array_key_exists('value', $entry) ? dimension_source_revision_int($entry['value']) : false;
	$mode = isset($entry['mode']) ? (string)$entry['mode'] : '';
	if ($value === false) {
		$result['codes'][] = 'invalid_identity';
		$result['errors'][] = _('A Dimension source revision identity must be the stored whole identity or zero.');
		return $result;
	}
	if (!in_array($mode, array('selected', 'unassigned', 'not_applicable'), true)) {
		$result['codes'][] = 'unresolved_axis';
		$result['errors'][] = _('A source revision can only record a final classification, not inheritance or an unknown legacy disposition.');
		return $result;
	}
	if ($mode === 'selected' && $value < 1) {
		$result['codes'][] = 'missing_identity';
		$result['errors'][] = _('A selected Dimension source revision needs a positive stored identity.');
		return $result;
	}
	if ($mode !== 'selected' && $value !== 0) {
		$result['codes'][] = 'unexpected_identity';
		$result['errors'][] = _('This Dimension source revision decision must not carry an identity.');
		return $result;
	}
	$origin = isset($entry['origin']) ? (string)$entry['origin'] : '';
	if ($mode === 'not_applicable') {
		$origin = 'policy';
		$basis = isset($entry['basis']) ? (string)$entry['basis'] : '';
		if (!in_array($basis, dimension_resolution_not_applicable_bases(), true)) {
			$result['codes'][] = 'invalid_basis';
			$result['errors'][] = _('Not Applicable is durable only with a registered policy basis.');
			return $result;
		}
		$policy_version = array_key_exists('policy_version', $entry)
			? dimension_source_revision_int($entry['policy_version']) : false;
		if ($policy_version === false || $policy_version < 1) {
			$result['codes'][] = 'missing_policy_version';
			$result['errors'][] = _('Not Applicable must name the published policy version that declares it.');
			return $result;
		}
	} else {
		$origin = $origin === '' ? 'resolution' : dimension_resolution_clean_origin($origin);
		if ($origin === false) {
			$result['codes'][] = 'invalid_origin';
			$result['errors'][] = _('The Dimension source revision origin is not a bounded identifier.');
			return $result;
		}
		$basis = '';
		$policy_version = array_key_exists('policy_version', $entry)
			? dimension_source_revision_int($entry['policy_version']) : 0;
		if ($policy_version === false) {
			$result['codes'][] = 'invalid_policy_version';
			$result['errors'][] = _('A Dimension source revision policy version must be a non-negative whole number.');
			return $result;
		}
	}
	$result['ok'] = true;
	$result['axis'] = array('value' => $value, 'mode' => $mode, 'origin' => $origin,
		'basis' => $basis, 'policy_version' => (int)$policy_version);
	return $result;
}

/**
 * Canonical identity digest of one source header or line.
 *
 * @param string $family Reviewed source family
 * @param mixed $trans_type Stored transaction type
 * @param string $document_key Canonical final or draft document key
 * @param string $line_kind header, line or draft_line
 * @param string $line_key Canonical stable line key
 * @param int|null $company Active company index, or null to read the request
 * @return string|false Lower-case SHA-256, or false for invalid evidence
 */
function dimension_source_revision_identity($family, $trans_type, $document_key, $line_kind,
	$line_key, $company = null)
{
	if (!in_array($family, dimension_source_revision_families(), true))
		return false;
	$type = dimension_source_revision_int($trans_type, 0);
	if ($type === false)
		return false;
	if (!is_string($document_key) || dimension_source_revision_clean_key($document_key, 128) !== $document_key)
		return false;
	if (!in_array($line_kind, dimension_source_revision_line_kinds(), true))
		return false;
	if (!dimension_source_revision_valid_line_key($line_kind, $line_key))
		return false;
	$company = $company === null ? dimension_source_revision_company() : dimension_source_revision_int($company, 0);
	if ($company === false)
		return false;
	return hash('sha256', dimension_source_revision_canonical(array(
		'schema' => 'dimension-source-identity/1', 'company' => $company, 'family' => $family,
		'trans_type' => $type, 'document_key' => $document_key, 'line_kind' => $line_kind,
		'line_key' => $line_key)));
}

/**
 * Canonical sealed payload of one revision, including its identity and predecessor.
 *
 * The digest covers the actor and every decision field. It deliberately excludes the auto-increment
 * identity and the creation timestamp so an exact retry of the same logical revision converges on
 * the stored row instead of creating a duplicate.
 *
 * @param array $fields Prepared revision fields
 * @return string|false Lower-case SHA-256, or false when a field is not canonical
 */
function dimension_source_revision_payload_digest($fields)
{
	if (!is_array($fields))
		return false;
	$required = array('company', 'source_family', 'trans_type', 'document_key', 'line_kind',
		'line_key', 'source_identity_sha256', 'revision_no', 'predecessor_revision_id',
		'draft_binding_sha256', 'dimension_id', 'dimension2_id', 'mode1', 'mode2', 'origin1',
		'origin2', 'basis1', 'basis2', 'policy1_version', 'policy2_version', 'effective_date',
		'override_reason', 'line_quantity', 'resolution_sha256', 'actor_id');
	foreach ($required as $name)
		if (!array_key_exists($name, $fields))
			return false;
	if (dimension_source_revision_identity($fields['source_family'], $fields['trans_type'],
		$fields['document_key'], $fields['line_kind'], $fields['line_key'],
		$fields['company']) !== $fields['source_identity_sha256'])
		return false;
	if (dimension_source_revision_decimal($fields['line_quantity']) !== $fields['line_quantity'])
		return false;
	if (dimension_source_revision_date($fields['effective_date']) !== $fields['effective_date'])
		return false;
	$canonical = array('schema' => 'dimension-source-revision/1');
	foreach ($required as $name)
		$canonical[$name] = (string)$fields[$name];
	return hash('sha256', dimension_source_revision_canonical($canonical));
}

/**
 * Build the durable evidence of one resolved axis pair.
 *
 * This is the shared adapter between the DIM-FND-005 resolver and durable storage. It refuses an
 * unresolved axis, recovers the exact Not Applicable basis/version from the resolver, and records
 * the applicable policy version on each axis.
 *
 * @param string $family Reviewed source family
 * @param mixed $trans_type Stored transaction type
 * @param string $document_key Final or draft document key
 * @param string $line_kind header, line or draft_line
 * @param string $line_key Stable line key
 * @param array $pair dimension_resolution_pair() result
 * @param array $options effective_date, override_reason, line_quantity, resolution, basis,
 *                       policy_version, predecessor_revision_id
 * @return array ok, fields, errors and codes
 */
function dimension_source_revision_input($family, $trans_type, $document_key, $line_kind, $line_key,
	$pair, $options = array())
{
	$result = array('ok' => false, 'fields' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($pair) || empty($pair['ok']) || empty($pair['axes'])) {
		$result['codes'][] = 'unresolved_classification';
		$result['errors'][] = _('Only a complete Dimension resolution can become a durable source revision.');
		if (is_array($pair) && !empty($pair['errors']))
			$result['errors'] = array_merge($result['errors'], $pair['errors']);
		return $result;
	}
	$axes = array();
	foreach (array(1, 2) as $axis) {
		$resolved = isset($pair['axes'][$axis]) && is_array($pair['axes'][$axis])
			? $pair['axes'][$axis] : array();
		$entry = array(
			'value' => isset($resolved['value']) ? $resolved['value'] : 0,
			'mode' => isset($resolved['mode']) ? $resolved['mode'] : '',
			'origin' => isset($resolved['origin']) ? $resolved['origin'] : '',
			'basis' => isset($resolved['basis']) ? $resolved['basis'] : '',
			'policy_version' => isset($resolved['policy_version']) ? $resolved['policy_version'] : 0);
		if (isset($options['basis']) && is_array($options['basis']) && isset($options['basis'][$axis]))
			$entry['basis'] = $options['basis'][$axis];
		if (isset($options['policy_version']) && is_array($options['policy_version'])
			&& array_key_exists($axis, $options['policy_version']))
			$entry['policy_version'] = $options['policy_version'][$axis];		if (($entry['mode'] === 'selected' || $entry['mode'] === 'unassigned')
			&& (int)$entry['policy_version'] === 0)
			$entry['policy_version'] = isset($pair['policy_version']) ? (int)$pair['policy_version'] : 0;
		$normalized = dimension_source_revision_normalize_axis($axis, $entry);
		if (!$normalized['ok']) {
			$result['codes'] = array_merge($result['codes'], $normalized['codes']);
			foreach ($normalized['errors'] as $error)
				$result['errors'][] = sprintf(_('Category %d: %s'), $axis, $error);
			return $result;
		}
		$axes[$axis] = $normalized['axis'];
	}
	if (!isset($options['resolution'])) {
		$summary = array('contract' => 'dimension-resolution/1',
			'values' => array(1 => $axes[1]['value'], 2 => $axes[2]['value']),
			'policy_version' => isset($pair['policy_version']) ? (int)$pair['policy_version'] : 0);
		foreach (array(1, 2) as $axis) {
			$resolved = isset($pair['axes'][$axis]) && is_array($pair['axes'][$axis])
				? $pair['axes'][$axis] : array();
			$summary['provenance'][$axis] = isset($resolved['provenance'])
				? $resolved['provenance'] : array();
		}
		$options['resolution'] = $summary;
	}
	// basis/policy_version are input-adapter evidence; the prepared revision stores them per axis.
	unset($options['basis'], $options['policy_version']);
	return dimension_source_revision_prepare($family, $trans_type, $document_key, $line_kind,
		$line_key, $axes, $options);
}

/**
 * Validate and seal one revision before it is stored.
 *
 * @param string $family Reviewed source family
 * @param mixed $trans_type Stored transaction type
 * @param string $document_key Final or draft document key
 * @param string $line_kind header, line or draft_line
 * @param string $line_key Stable line key
 * @param array $axes Axis => value/mode/origin/basis/policy_version
 * @param array $options effective_date, override_reason, line_quantity, resolution,
 *                       predecessor_revision_id, draft_binding_revision_id
 * @return array ok, fields, errors and codes
 */
function dimension_source_revision_prepare($family, $trans_type, $document_key, $line_kind, $line_key,
	$axes, $options = array())
{
	$result = array('ok' => false, 'fields' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($options)) {
		$result['codes'][] = 'invalid_options';
		$result['errors'][] = _('The Dimension source revision evidence must be an array.');
		return $result;
	}
	foreach ($options as $key => $unused) {
		if (!in_array($key, array('effective_date', 'override_reason', 'line_quantity', 'resolution',
			'predecessor_revision_id', 'draft_binding_revision_id'), true)) {
			$result['codes'][] = 'invalid_option';
			$result['errors'][] = sprintf(_('Unsupported Dimension source revision evidence: %s.'), (string)$key);
			return $result;
		}
	}
	if (!in_array($family, dimension_source_revision_families(), true)) {
		$result['codes'][] = 'invalid_family';
		$result['errors'][] = _('The source family is not registered for durable Dimension revisions.');
		return $result;
	}
	$type = dimension_source_revision_int($trans_type, 0);
	if ($type === false) {
		$result['codes'][] = 'invalid_trans_type';
		$result['errors'][] = _('A source revision needs a whole stored transaction type.');
		return $result;
	}
	if (!is_string($document_key)
		|| (dimension_source_revision_is_draft_key($document_key)
			? !preg_match('/^draft:[a-f0-9]{64}$/D', $document_key)
			: dimension_source_revision_document_key($document_key) !== (string)$document_key)) {
		$result['codes'][] = 'invalid_document_key';
		$result['errors'][] = _('A source revision needs a canonical document key.');
		return $result;
	}
	if (!in_array($line_kind, dimension_source_revision_line_kinds(), true)) {
		$result['codes'][] = 'invalid_line_kind';
		$result['errors'][] = _('A source revision line kind is not registered.');
		return $result;
	}
	if (!dimension_source_revision_valid_line_key($line_kind, $line_key)) {
		$result['codes'][] = 'invalid_line_key';
		$result['errors'][] = _('A source revision needs a stable line key that is not a cart position.');
		return $result;
	}
	$company = dimension_source_revision_company();
	$actor = dimension_source_revision_actor();
	if ($actor < 1) {
		$result['codes'][] = 'unauthenticated';
		$result['errors'][] = _('A durable Dimension source revision needs the authenticated actor who recorded it.');
		return $result;
	}
	if (!isset($options['effective_date'])) {
		$result['codes'][] = 'missing_effective_date';
		$result['errors'][] = _('A source revision must name its effective date.');
		return $result;
	}
	$effective_date = dimension_source_revision_date($options['effective_date']);
	if ($effective_date === false) {
		$result['codes'][] = 'invalid_effective_date';
		$result['errors'][] = _('The source revision effective date is not a real ISO calendar date.');
		return $result;
	}
	$override_reason = dimension_source_revision_clean_text(
		isset($options['override_reason']) ? $options['override_reason'] : '', 255);
	if ($override_reason === null) {
		$result['codes'][] = 'invalid_override_reason';
		$result['errors'][] = _('The source revision override reason contains a control character or is too long.');
		return $result;
	}
	$line_quantity = dimension_source_revision_decimal(
		isset($options['line_quantity']) ? $options['line_quantity'] : 0);
	if ($line_quantity === false) {
		$result['codes'][] = 'invalid_line_quantity';
		$result['errors'][] = _('A source revision line quantity must be an exact non-negative decimal.');
		return $result;
	}
	if ($line_kind === 'header' && $line_quantity !== '0.00000000') {
		$result['codes'][] = 'header_quantity';
		$result['errors'][] = _('A header classification carries no line quantity; classify the line instead.');
		return $result;
	}
	if (!is_array($axes) || !isset($axes[1], $axes[2])) {
		$result['codes'][] = 'missing_axes';
		$result['errors'][] = _('A source revision needs both resolved Dimension axes.');
		return $result;
	}
	$normalized = array();
	foreach (array(1, 2) as $axis) {
		$axis_result = dimension_source_revision_normalize_axis($axis, $axes[$axis]);
		if (!$axis_result['ok']) {
			$result['codes'] = array_merge($result['codes'], $axis_result['codes']);
			foreach ($axis_result['errors'] as $error)
				$result['errors'][] = sprintf(_('Category %d: %s'), $axis, $error);
			return $result;
		}
		$normalized[$axis] = $axis_result['axis'];
	}
	$predecessor = isset($options['predecessor_revision_id'])
		? dimension_source_revision_int($options['predecessor_revision_id'], 0) : 0;
	if ($predecessor === false) {
		$result['codes'][] = 'invalid_predecessor';
		$result['errors'][] = _('A source revision predecessor must be a stored positive identity or zero.');
		return $result;
	}
	$binding = isset($options['draft_binding_revision_id'])
		? dimension_source_revision_int($options['draft_binding_revision_id'], 0) : 0;
	if ($binding === false) {
		$result['codes'][] = 'invalid_draft_binding';
		$result['errors'][] = _('A pending draft binding must name the stored draft revision.');
		return $result;
	}
	if ($binding > 0) {
		if (dimension_source_revision_is_draft_key($document_key)) {
			$result['codes'][] = 'draft_binding_to_draft';
			$result['errors'][] = _('A pending draft revision cannot be bound to another draft identity.');
			return $result;
		}
		if ($predecessor > 0 && $predecessor !== $binding) {
			$result['codes'][] = 'conflicting_binding';
			$result['errors'][] = _('A pending draft binding is the direct predecessor of the final revision.');
			return $result;
		}
		$predecessor = $binding;
	}
	$resolution_json = dimension_source_revision_payload_json(
		isset($options['resolution']) ? $options['resolution'] : array());
	if ($resolution_json === false || strlen($resolution_json) > 65535) {
		$result['codes'][] = 'invalid_resolution_payload';
		$result['errors'][] = _('The resolution evidence is not a bounded JSON-safe payload.');
		return $result;
	}
	$identity = dimension_source_revision_identity($family, $type, $document_key, $line_kind, $line_key,
		$company);
	if ($identity === false) {
		$result['codes'][] = 'invalid_identity';
		$result['errors'][] = _('The Dimension source identity could not be derived.');
		return $result;
	}
	$fields = array(
		'company' => $company,
		'source_family' => $family,
		'trans_type' => $type,
		'document_key' => $document_key,
		'line_kind' => $line_kind,
		'line_key' => $line_key,
		'source_identity_sha256' => $identity,
		'revision_no' => 1,
		'predecessor_revision_id' => $predecessor,
		'draft_binding_sha256' => $binding > 0
			? hash('sha256', 'dimension-draft-binding/1|'.$binding) : '',
		'dimension_id' => $normalized[1]['value'],
		'dimension2_id' => $normalized[2]['value'],
		'mode1' => $normalized[1]['mode'],
		'mode2' => $normalized[2]['mode'],
		'origin1' => $normalized[1]['origin'],
		'origin2' => $normalized[2]['origin'],
		'basis1' => $normalized[1]['basis'],
		'basis2' => $normalized[2]['basis'],
		'policy1_version' => $normalized[1]['policy_version'],
		'policy2_version' => $normalized[2]['policy_version'],
		'effective_date' => $effective_date,
		'override_reason' => (string)$override_reason,
		'line_quantity' => $line_quantity,
		'resolution_payload' => $resolution_json,
		'resolution_sha256' => hash('sha256', $resolution_json),
		'actor_id' => $actor);
	$fields['payload_sha256'] = dimension_source_revision_payload_digest($fields);
	if ($fields['payload_sha256'] === false) {
		$result['codes'][] = 'invalid_payload';
		$result['errors'][] = _('The Dimension source revision payload could not be sealed.');
		return $result;
	}
	$result['ok'] = true;
	$result['fields'] = $fields;
	if ($binding > 0)
		$result['draft_binding_revision_id'] = $binding;
	return $result;
}

/**
 * Insert one prepared revision, converging on an exact retry and refusing a conflicting one.
 *
 * @param array $prepared dimension_source_revision_prepare()/input() result
 * @return array ok, created, existing, revision_id, revision, errors and codes
 */
function dimension_source_revision_persist($prepared)
{
	$failed = array('ok' => false, 'created' => false, 'existing' => false, 'revision_id' => 0,
		'revision' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($prepared) || empty($prepared['ok']) || empty($prepared['fields'])
		|| !is_array($prepared['fields'])) {
		$failed['codes'][] = 'invalid_prepared_revision';
		$failed['errors'][] = _('Only a validated Dimension source revision can be stored.');
		return $failed;
	}
	$fields = $prepared['fields'];
	if (dimension_source_revision_payload_digest($fields) !== $fields['payload_sha256']) {
		$failed['codes'][] = 'tamper';
		$failed['errors'][] = _('The prepared Dimension source revision payload does not verify.');
		return $failed;
	}
	$ready = dimension_source_revision_schema_ready();
	if ($ready === 'absent') {
		$failed['codes'][] = 'schema_unavailable';
		$failed['errors'][] = _('The Dimension source revision schema is not installed for this company.');
		return $failed;
	}
	if ($ready !== true) {
		$failed['codes'][] = 'schema_unknown';
		$failed['errors'][] = _('The Dimension source revision schema could not be verified.');
		return $failed;
	}
	foreach (array(1, 2) as $axis) {
		$version = (int)$fields['policy'.$axis.'_version'];
		if ($version < 1)
			continue;
		if (!function_exists('dimension_category_policy_version')
			|| dimension_category_policy_version($version) === false) {
			$failed['codes'][] = 'policy_version_unverified';
			$failed['errors'][] = _('A policy version named by this Dimension source revision is missing or does not verify.');
			return $failed;
		}
	}
	if (!begin_transaction()) {
		$failed['codes'][] = 'transaction_unavailable';
		$failed['errors'][] = _('The Dimension source revision could not be stored; try again.');
		return $failed;
	}
	try {
		$binding = $fields['draft_binding_sha256'] !== '';
		if ($binding) {
			$draft_id = isset($prepared['draft_binding_revision_id'])
				? dimension_source_revision_int($prepared['draft_binding_revision_id'], 1) : false;
			if ($draft_id === false
				|| hash('sha256', 'dimension-draft-binding/1|'.$draft_id) !== $fields['draft_binding_sha256']) {
				cancel_transaction();
				$failed['codes'][] = 'invalid_draft_binding';
				$failed['errors'][] = _('The pending draft binding evidence is not canonical.');
				return $failed;
			}
			$existing = dimension_source_revision_find_binding($fields['draft_binding_sha256'], true);
			if ($existing !== false) {
				if (!is_array($existing)) {
					cancel_transaction();
					$failed['codes'][] = 'schema_unknown';
					$failed['errors'][] = _('The pending draft binding could not be verified.');
					return $failed;
				}
				if ($existing['payload_sha256'] !== $fields['payload_sha256']) {
					cancel_transaction();
					$failed['codes'][] = 'draft_already_bound';
					$failed['errors'][] = _('This pending Dimension draft is already bound to a different final source.');
					return $failed;
				}
				commit_transaction();
				$failed['ok'] = true;
				$failed['existing'] = true;
				$failed['revision_id'] = (int)$existing['source_revision_id'];
				$failed['revision'] = $existing;
				return $failed;
			}
			$draft = dimension_source_revision_read_locked($draft_id);
			if (!is_array($draft) || $draft['state'] !== 'available'
				|| !dimension_source_revision_is_draft_key($draft['revision']['document_key'])
				|| $draft['revision']['draft_binding_sha256'] !== '') {
				cancel_transaction();
				$failed['codes'][] = 'invalid_draft_binding';
				$failed['errors'][] = _('The pending Dimension draft revision is missing, already bound or not a draft.');
				return $failed;
			}
			$fields['revision_no'] = 1;
			$fields['predecessor_revision_id'] = $draft_id;
		} elseif ((int)$fields['predecessor_revision_id'] > 0) {
			$predecessor = dimension_source_revision_read_locked((int)$fields['predecessor_revision_id']);
			if (!is_array($predecessor) || $predecessor['state'] !== 'available'
				|| $predecessor['revision']['source_identity_sha256'] !== $fields['source_identity_sha256']) {
				cancel_transaction();
				$failed['codes'][] = 'invalid_predecessor';
				$failed['errors'][] = _('A source revision predecessor must be a readable revision of the same source identity.');
				return $failed;
			}
			$fields['revision_no'] = (int)$predecessor['revision']['revision_no'] + 1;
		} else {
			$fields['revision_no'] = 1;
		}
		$fields['payload_sha256'] = dimension_source_revision_payload_digest($fields);
		if ($fields['payload_sha256'] === false) {
			cancel_transaction();
			$failed['codes'][] = 'invalid_payload';
			$failed['errors'][] = _('The Dimension source revision payload could not be sealed.');
			return $failed;
		}
		$current = dimension_source_revision_find_current($fields['source_identity_sha256'],
			(int)$fields['revision_no'], true);
		if ($current !== false) {
			if (!is_array($current) || $current['payload_sha256'] !== $fields['payload_sha256']) {
				cancel_transaction();
				$failed['codes'][] = 'conflicting_revision';
				$failed['errors'][] = _('A different Dimension revision already exists for this source identity and revision number.');
				return $failed;
			}
			commit_transaction();
			$failed['ok'] = true;
			$failed['existing'] = true;
			$failed['revision_id'] = (int)$current['source_revision_id'];
			$failed['revision'] = $current;
			return $failed;
		}
		$inserted = db_query('INSERT INTO '.TB_PREF.'dimension_source_revisions ('
			.'source_identity_sha256,source_family,trans_type,document_key,line_kind,line_key,revision_no,'
			.'predecessor_revision_id,draft_binding_sha256,dimension_id,dimension2_id,mode1,mode2,origin1,'
			.'origin2,basis1,basis2,policy1_version,policy2_version,effective_date,override_reason,'
			.'line_quantity,resolution_payload,payload_sha256,actor_id,created_at) VALUES ('
			.db_escape($fields['source_identity_sha256']).','.db_escape($fields['source_family']).','
			.(int)$fields['trans_type'].','.dimension_source_revision_quote($fields['document_key']).','
			.dimension_source_revision_quote($fields['line_kind']).','
			.dimension_source_revision_quote($fields['line_key']).','
			.(int)$fields['revision_no'].','
			.((int)$fields['predecessor_revision_id'] > 0
				? (int)$fields['predecessor_revision_id'] : 'NULL').','
			.($fields['draft_binding_sha256'] === '' ? 'NULL' : db_escape($fields['draft_binding_sha256']))
			.','.(int)$fields['dimension_id'].','.(int)$fields['dimension2_id'].','
			.db_escape($fields['mode1']).','.db_escape($fields['mode2']).','
			.db_escape($fields['origin1']).','.db_escape($fields['origin2']).','
			.db_escape($fields['basis1']).','.db_escape($fields['basis2']).','
			.(int)$fields['policy1_version'].','.(int)$fields['policy2_version'].','
			.dimension_source_revision_quote($fields['effective_date']).','
			.dimension_source_revision_quote($fields['override_reason']).','
			.dimension_source_revision_quote($fields['line_quantity']).','
			.dimension_source_revision_quote($fields['resolution_payload']).','
			.db_escape($fields['payload_sha256']).','.(int)$fields['actor_id'].',UTC_TIMESTAMP())');
		$revision_id = $inserted ? (int)db_insert_id() : 0;
		if ($revision_id < 1) {
			// A concurrent identical insert can win the unique key; the committed row is the retry.
			$concurrent = dimension_source_revision_find_current($fields['source_identity_sha256'],
				(int)$fields['revision_no'], true);
			if (is_array($concurrent) && $concurrent['payload_sha256'] === $fields['payload_sha256']) {
				commit_transaction();
				$failed['ok'] = true;
				$failed['existing'] = true;
				$failed['revision_id'] = (int)$concurrent['source_revision_id'];
				$failed['revision'] = $concurrent;
				return $failed;
			}
			cancel_transaction();
			$failed['codes'][] = 'store_failed';
			$failed['errors'][] = _('The Dimension source revision could not be stored; try again.');
			return $failed;
		}
		$read = dimension_source_revision_read($revision_id);
		if (!is_array($read) || $read['state'] !== 'available') {
			cancel_transaction();
			$failed['codes'][] = 'store_verification_failed';
			$failed['errors'][] = _('The stored Dimension source revision did not verify.')
				.' ['.(is_array($read) ? $read['state'].':'.implode(',', $read['codes']) : 'unknown').']';
			return $failed;
		}
		if (!commit_transaction()) {
			$failed['codes'][] = 'commit_failed';
			$failed['errors'][] = _('The Dimension source revision could not be committed; try again.');
			return $failed;
		}
		$failed['ok'] = true;
		$failed['created'] = true;
		$failed['revision_id'] = $revision_id;
		$failed['revision'] = $read['revision'];
		return $failed;
	} catch (Exception $exception) {
		cancel_transaction();
		$failed['codes'][] = 'store_failed';
		$failed['errors'][] = _('The Dimension source revision could not be stored; try again.');
		return $failed;
	}
}

/**
 * Read one stored revision and verify its identity, payload seal and axis evidence.
 *
 * @param mixed $revision_id Stored identity
 * @param bool $lock Take an exclusive row lock inside the caller's transaction
 * @return array state (available|absent|unavailable|unknown|tamper), revision, errors and codes
 */
function dimension_source_revision_read($revision_id, $lock = false)
{
	$result = array('state' => 'unknown', 'revision' => null, 'errors' => array(), 'codes' => array());
	$identity = dimension_source_revision_int($revision_id, 1);
	if ($identity === false) {
		$result['codes'][] = 'invalid_identity';
		$result['errors'][] = _('A Dimension source revision identity must be a stored positive number.');
		return $result;
	}
	$ready = dimension_source_revision_schema_ready();
	if ($ready === 'absent') {
		$result['state'] = 'unavailable';
		$result['codes'][] = 'schema_unavailable';
		return $result;
	}
	if ($ready !== true) {
		$result['codes'][] = 'schema_unknown';
		return $result;
	}
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_source_revisions WHERE source_revision_id='
		.(int)$identity.($lock ? ' FOR UPDATE' : ''));
	if (!$rows)
		return $result;
	$row = db_fetch_assoc($rows);
	if (!$row) {
		$result['state'] = 'absent';
		$result['codes'][] = 'revision_absent';
		return $result;
	}
	return dimension_source_revision_row_state($row);
}

/**
 * Locked read used inside the persist transaction.
 *
 * @param int $revision_id Stored identity
 * @return array|false
 */
function dimension_source_revision_read_locked($revision_id)
{
	$read = dimension_source_revision_read($revision_id, true);
	return $read['state'] === 'available' ? $read : false;
}

/**
 * Read the latest (or one exact) revision of a source identity.
 *
 * @param string $source_identity_sha256 Canonical identity digest
 * @param int $revision_no Exact revision number, or 0 for the latest
 * @param bool $lock Lock the selected row
 * @return array|false Verified row, false for absence, or an error array for a failed/unknown read
 */
function dimension_source_revision_find_current($source_identity_sha256, $revision_no = 0, $lock = false)
{
	if (!is_string($source_identity_sha256) || !preg_match('/^[a-f0-9]{64}$/D', $source_identity_sha256))
		return false;
	$revision_no = (int)$revision_no;
	$ready = dimension_source_revision_schema_ready();
	if ($ready !== true)
		return array('read_error' => $ready === 'absent' ? 'schema_unavailable' : 'schema_unknown');
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_source_revisions WHERE source_identity_sha256='
		.db_escape($source_identity_sha256).($revision_no > 0 ? ' AND revision_no='.$revision_no : '')
		.' ORDER BY revision_no DESC LIMIT 1'.($lock ? ' FOR UPDATE' : ''));
	if (!$rows)
		return array('read_error' => 'query_failed');
	$row = db_fetch_assoc($rows);
	if (!$row)
		return false;
	$state = dimension_source_revision_row_state($row);
	return $state['state'] === 'available' ? $state['revision']
		: array('read_error' => $state['state'], 'row_state' => $state);
}

/**
 * Find the final revision that adopted a pending draft.
 *
 * @param string $draft_binding_sha256 Canonical binding digest
 * @param bool $lock Lock the selected row
 * @return array|false Verified row, false for absence, or an error array for a failed/unknown read
 */
function dimension_source_revision_find_binding($draft_binding_sha256, $lock = false)
{
	if (!is_string($draft_binding_sha256) || !preg_match('/^[a-f0-9]{64}$/D', $draft_binding_sha256))
		return false;
	$ready = dimension_source_revision_schema_ready();
	if ($ready !== true)
		return array('read_error' => $ready === 'absent' ? 'schema_unavailable' : 'schema_unknown');
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_source_revisions WHERE draft_binding_sha256='
		.db_escape($draft_binding_sha256).' LIMIT 1'.($lock ? ' FOR UPDATE' : ''));
	if (!$rows)
		return array('read_error' => 'query_failed');
	$row = db_fetch_assoc($rows);
	if (!$row)
		return false;
	$state = dimension_source_revision_row_state($row);
	return $state['state'] === 'available' ? $state['revision']
		: array('read_error' => $state['state'], 'row_state' => $state);
}

/**
 * Verify one stored (or prepared) revision row from its own bytes.
 *
 * This is pure source logic: no database access. A tampered row is reported tamper, never repaired
 * and never treated as absence.
 *
 * @param array $row Stored row
 * @return array state, revision, axes, errors and codes
 */
function dimension_source_revision_row_state($row)
{
	$result = array('state' => 'tamper', 'revision' => null, 'axes' => array(),
		'errors' => array(), 'codes' => array());
	if (!is_array($row)) {
		$result['codes'][] = 'invalid_row';
		$result['errors'][] = _('A Dimension source revision row must be an array.');
		return $result;
	}
	foreach (array_keys(dimension_source_revision_schema_columns()) as $column)
		if (!array_key_exists($column, $row)) {
			$result['codes'][] = 'missing_column';
			$result['errors'][] = sprintf(_('The stored Dimension source revision row has no %s column.'), $column);
			return $result;
		}
	$company = dimension_source_revision_company();
	$revision_id = dimension_source_revision_int($row['source_revision_id'], 1);
	$revision_no = dimension_source_revision_int($row['revision_no'], 1);
	$trans_type = dimension_source_revision_int($row['trans_type'], 0);
	$dimension_id = dimension_source_revision_int($row['dimension_id'], 0);
	$dimension2_id = dimension_source_revision_int($row['dimension2_id'], 0);
	$policy1 = dimension_source_revision_int($row['policy1_version'], 0);
	$policy2 = dimension_source_revision_int($row['policy2_version'], 0);
	$actor = dimension_source_revision_int($row['actor_id'], 1);
	$predecessor = $row['predecessor_revision_id'] === null ? 0
		: dimension_source_revision_int($row['predecessor_revision_id'], 1);
	$draft_binding = $row['draft_binding_sha256'] === null ? '' : (string)$row['draft_binding_sha256'];
	if ($revision_id === false || $revision_no === false || $trans_type === false
		|| $dimension_id === false || $dimension2_id === false || $policy1 === false
		|| $policy2 === false || $actor === false || $predecessor === false) {
		$result['codes'][] = 'invalid_row';
		return $result;
	}
	$family = (string)$row['source_family'];
	$line_kind = (string)$row['line_kind'];
	$document_key = (string)$row['document_key'];
	$line_key = (string)$row['line_key'];
	$identity_sha = (string)$row['source_identity_sha256'];
	$payload_sha = (string)$row['payload_sha256'];
	$resolution_sha = hash('sha256', (string)$row['resolution_payload']);
	if (!in_array($family, dimension_source_revision_families(), true)
		|| !in_array($line_kind, dimension_source_revision_line_kinds(), true)
		|| !dimension_source_revision_valid_line_key($line_kind, $line_key)
		|| !preg_match('/^[a-f0-9]{64}$/D', $identity_sha)
		|| !preg_match('/^[a-f0-9]{64}$/D', $payload_sha)
		|| (dimension_source_revision_is_draft_key($document_key)
			? !preg_match('/^draft:[a-f0-9]{64}$/D', $document_key)
			: dimension_source_revision_document_key($document_key) !== $document_key)) {
		$result['codes'][] = 'invalid_row';
		return $result;
	}
	if ($draft_binding !== '' && !preg_match('/^[a-f0-9]{64}$/D', $draft_binding)) {
		$result['codes'][] = 'invalid_binding';
		return $result;
	}
	if ($draft_binding !== '' && $predecessor < 1) {
		$result['codes'][] = 'binding_without_predecessor';
		return $result;
	}
	if ($draft_binding !== '' && dimension_source_revision_is_draft_key($document_key)) {
		$result['codes'][] = 'bound_draft';
		return $result;
	}
	$effective_date = dimension_source_revision_date($row['effective_date']);
	$line_quantity = dimension_source_revision_decimal($row['line_quantity']);
	$origin1 = dimension_resolution_clean_origin($row['origin1']);
	$origin2 = dimension_resolution_clean_origin($row['origin2']);
	$override_reason = dimension_source_revision_clean_text($row['override_reason'], 255);
	if ($effective_date === false || $line_quantity === false || $origin1 === false || $origin2 === false
		|| $override_reason === null || $override_reason !== (string)$row['override_reason']) {
		$result['codes'][] = 'invalid_row';
		return $result;
	}
	if ($line_kind === 'header' && $line_quantity !== '0.00000000') {
		$result['codes'][] = 'invalid_row';
		return $result;
	}
	$axes = array();
	foreach (array(1, 2) as $axis) {
		$entry = array(
			'value' => $axis === 1 ? $dimension_id : $dimension2_id,
			'mode' => (string)$row['mode'.$axis],
			'origin' => $axis === 1 ? $origin1 : $origin2,
			'basis' => (string)$row['basis'.$axis],
			'policy_version' => $axis === 1 ? $policy1 : $policy2);
		$normalized = dimension_source_revision_normalize_axis($axis, $entry);
		if (!$normalized['ok']) {
			$result['codes'] = array_merge($result['codes'], $normalized['codes']);
			return $result;
		}
		$axes[$axis] = $normalized['axis'];
	}
	$resolution_payload = (string)$row['resolution_payload'];
	$decoded_payload = strlen($resolution_payload) > 65535 ? false : json_decode($resolution_payload, true);
	if ($resolution_payload === '' || !is_array($decoded_payload)
		|| dimension_source_revision_payload_json($decoded_payload) !== $resolution_payload) {
		$result['codes'][] = 'resolution_tamper';
		return $result;
	}
	$fields = array(
		'company' => $company,
		'source_family' => $family,
		'trans_type' => $trans_type,
		'document_key' => $document_key,
		'line_kind' => $line_kind,
		'line_key' => $line_key,
		'source_identity_sha256' => $identity_sha,
		'revision_no' => $revision_no,
		'predecessor_revision_id' => $predecessor,
		'draft_binding_sha256' => $draft_binding,
		'dimension_id' => $dimension_id,
		'dimension2_id' => $dimension2_id,
		'mode1' => $axes[1]['mode'],
		'mode2' => $axes[2]['mode'],
		'origin1' => $axes[1]['origin'],
		'origin2' => $axes[2]['origin'],
		'basis1' => $axes[1]['basis'],
		'basis2' => $axes[2]['basis'],
		'policy1_version' => $policy1,
		'policy2_version' => $policy2,
		'effective_date' => $effective_date,
		'override_reason' => $override_reason,
		'line_quantity' => $line_quantity,
		'resolution_sha256' => $resolution_sha,
		'actor_id' => $actor);
	if (dimension_source_revision_identity($family, $trans_type, $document_key, $line_kind, $line_key,
		$company) !== $identity_sha) {
		$result['codes'][] = 'identity_tamper';
		return $result;
	}
	if (dimension_source_revision_payload_digest($fields) !== $payload_sha) {
		$result['codes'][] = 'payload_tamper';
		return $result;
	}
	$revision = $row;
	foreach ($fields as $name => $value)
		$revision[$name] = $value;
	$revision['source_revision_id'] = $revision_id;
	$revision['axes'] = $axes;
	$result['state'] = 'available';
	$result['revision'] = $revision;
	$result['axes'] = $axes;
	return $result;
}

/**
 * Build the four-axis snapshot intents of one verified revision.
 *
 * @param array $revision Verified revision (a row_state()['revision'])
 * @return array ok, intents axis => builder result, errors and codes
 */
function dimension_source_revision_row_intents($revision)
{
	$result = array('ok' => false, 'intents' => array(), 'errors' => array(), 'codes' => array());
	if (!is_array($revision) || !isset($revision['axes'][1], $revision['axes'][2])) {
		$result['codes'][] = 'invalid_revision';
		$result['errors'][] = _('Only a verified Dimension source revision can produce snapshot intents.');
		return $result;
	}
	foreach (array(1, 2) as $axis) {
		$entry = $revision['axes'][$axis];
		if ($entry['mode'] === 'not_applicable') {
			$intent = dimension_resolution_intent('not_applicable', 0, 'policy', array(
				'persisted' => true, 'basis' => $entry['basis'],
				'policy_version' => $entry['policy_version']));
		} else {
			$intent = dimension_resolution_intent($entry['mode'], $entry['value'], $entry['origin'],
				array('persisted' => true));
		}
		if (!$intent['ok']) {
			$result['codes'] = array_merge($result['codes'], $intent['codes']);
			foreach ($intent['errors'] as $error)
				$result['errors'][] = sprintf(_('Category %d: %s'), $axis, $error);
			return $result;
		}
		$result['intents'][$axis] = $intent;
	}
	$result['ok'] = true;
	return $result;
}

/**
 * Read the durable snapshot of one final or draft source header/line.
 *
 * @param string $family Reviewed source family
 * @param mixed $trans_type Stored transaction type
 * @param string $document_key Final or draft document key
 * @param string $line_kind header, line or draft_line
 * @param string $line_key Stable line key
 * @param array $options revision_no (0 = latest)
 * @return array state (available|absent|unavailable|unknown|tamper), revision, intents, errors
 */
function dimension_source_revision_snapshot($family, $trans_type, $document_key, $line_kind,
	$line_key, $options = array())
{
	$result = array('state' => 'unknown', 'revision' => null, 'intents' => array(),
		'errors' => array(), 'codes' => array());
	if (!is_array($options))
		$options = array();
	$revision_no = isset($options['revision_no']) ? dimension_source_revision_int($options['revision_no'], 0) : 0;
	if ($revision_no === false) {
		$result['codes'][] = 'invalid_revision_number';
		$result['errors'][] = _('A snapshot revision number must be a whole non-negative number.');
		return $result;
	}
	$identity = dimension_source_revision_identity($family, $trans_type, $document_key, $line_kind,
		$line_key);
	if ($identity === false) {
		$result['codes'][] = 'invalid_identity';
		$result['errors'][] = _('The Dimension source identity is not canonical.');
		return $result;
	}
	$ready = dimension_source_revision_schema_ready();
	if ($ready === 'absent') {
		$result['state'] = 'unavailable';
		$result['codes'][] = 'schema_unavailable';
		return $result;
	}
	if ($ready !== true) {
		$result['codes'][] = 'schema_unknown';
		return $result;
	}
	$current = dimension_source_revision_find_current($identity, $revision_no);
	if ($current === false) {
		$result['state'] = 'absent';
		$result['codes'][] = 'lineage_unavailable';
		return $result;
	}
	if (!is_array($current) || isset($current['read_error'])) {
		$error = is_array($current) && isset($current['read_error']) ? $current['read_error'] : 'read_failed';
		$result['state'] = $error === 'tamper' ? 'tamper' : 'unknown';
		$result['codes'][] = $error;
		if (is_array($current) && isset($current['row_state']['codes']))
			$result['codes'] = array_merge($result['codes'], $current['row_state']['codes']);
		return $result;
	}
	// find_current() already returned a row_state()-verified revision; re-normalizing it would read
	// the normalized predecessor 0/binding '' back as if they were raw nullable columns.
	$revision = $current;
	if (!dimension_source_revision_policy_verified($revision)) {
		$result['state'] = 'tamper';
		$result['codes'][] = 'policy_version_unverified';
		$result['errors'][] = _('A policy version named by this source revision is missing or does not verify.');
		return $result;
	}
	$intents = dimension_source_revision_row_intents($revision);
	if (!$intents['ok']) {
		$result['state'] = 'tamper';
		$result['codes'] = array_merge($result['codes'], $intents['codes']);
		$result['errors'] = $intents['errors'];
		return $result;
	}
	$result['state'] = 'available';
	$result['revision'] = $revision;
	$result['intents'] = $intents['intents'];
	$result['lineage'] = dimension_source_revision_master_states($revision);
	return $result;
}

/**
 * Verify the published policy versions named by one revision.
 *
 * @param array $revision Verified revision
 * @return bool
 */
function dimension_source_revision_policy_verified($revision)
{
	foreach (array(1, 2) as $axis) {
		$version = isset($revision['policy'.$axis.'_version'])
			? (int)$revision['policy'.$axis.'_version'] : 0;
		if ($version < 1)
			continue;
		if (!function_exists('dimension_category_policy_version'))
			return false;
		if (dimension_category_policy_version($version) === false)
			return false;
	}
	return true;
}

/**
 * Current master state of the classified identities (informational; closed is a valid historical read).
 *
 * @param array $revision Verified revision
 * @return array axis => absent|current|closed|wrong_axis|missing|unknown
 */
function dimension_source_revision_master_states($revision)
{
	$states = array(1 => 'unknown', 2 => 'unknown');
	if (dimension_source_revision_table_state('dimensions') !== 'available')
		return $states;
	foreach (array(1, 2) as $axis) {
		$value = isset($revision['axes'][$axis]['value'])
			? (int)$revision['axes'][$axis]['value'] : 0;
		if ($value < 1) {
			$states[$axis] = 'absent';
			continue;
		}
		$rows = db_query('SELECT type_,closed FROM '.TB_PREF.'dimensions WHERE id='.$value);
		if (!$rows) {
			$states[$axis] = 'unknown';
			continue;
		}
		$row = db_fetch_assoc($rows);
		if (!$row)
			$states[$axis] = 'missing';
		elseif ((int)$row['type_'] !== $axis)
			$states[$axis] = 'wrong_axis';
		elseif ((int)$row['closed'] === 1)
			$states[$axis] = 'closed';
		else
			$states[$axis] = 'current';
	}
	return $states;
}

/**
 * One axis of a verified snapshot keyed by its resolver source kind.
 *
 * The DIM-FND-005 document adapter accepts one snapshot descriptor per axis:
 * `array('header_snapshot' => $intent)` or `array('line_snapshot' => $intent)`.
 *
 * @param array $snapshot Verified snapshot result
 * @param int $axis Native axis 1 or 2
 * @return array|false
 */
function dimension_source_revision_snapshot_source($snapshot, $axis)
{
	if (!is_array($snapshot) || $snapshot['state'] !== 'available'
		|| ($axis !== 1 && $axis !== 2) || !isset($snapshot['intents'][$axis]))
		return false;
	$source = $snapshot['revision']['line_kind'] === 'header' ? 'header_snapshot' : 'line_snapshot';
	return array($source => $snapshot['intents'][$axis]);
}

/**
 * Apply a verified snapshot to a fresh document carrier for reload.
 *
 * The snapshot intents carry persisted evidence, so they are attached through the same
 * `classification_intents` carrier the module classes already expose and mirrored into the native
 * field. No value is re-derived from a current default during reload.
 *
 * @param object $document Source cart/order/transaction
 * @param array $snapshot Verified snapshot result
 * @return bool
 */
function dimension_source_revision_apply($document, $snapshot)
{
	if (!is_object($document) || !is_array($snapshot) || $snapshot['state'] !== 'available')
		return false;
	if (!property_exists($document, 'classification_intents') && get_class($document) !== 'stdClass')
		return false;
	$intents = array();
	foreach (array(1, 2) as $axis) {
		if (!isset($snapshot['intents'][$axis]['intent']))
			return false;
		$intents[$axis] = $snapshot['intents'][$axis]['intent'];
	}
	$document->classification_intents = $intents;
	foreach ($intents as $axis => $intent)
		dimension_resolution_mirror_native_intent($document, (int)$axis, $intent);
	return true;
}

/**
 * Bind one pending draft revision to its final source identity exactly once.
 *
 * @param mixed $draft_revision_id Stored draft revision
 * @param array $identity document_key, line_kind, line_key and optional effective_date,
 *                        override_reason, resolution, line_quantity
 * @return array persist result with codes binding_mismatch/missing_draft added when refused
 */
function dimension_source_revision_bind($draft_revision_id, $identity)
{
	$failed = array('ok' => false, 'created' => false, 'existing' => false, 'revision_id' => 0,
		'revision' => null, 'errors' => array(), 'codes' => array());
	$draft_id = dimension_source_revision_int($draft_revision_id, 1);
	if ($draft_id === false || !is_array($identity)) {
		$failed['codes'][] = 'invalid_binding';
		$failed['errors'][] = _('A pending draft binding needs the stored draft revision and the final source identity.');
		return $failed;
	}
	foreach ($identity as $key => $unused)
		if (!in_array($key, array('document_key', 'line_kind', 'line_key', 'effective_date',
			'override_reason', 'resolution', 'line_quantity'), true)) {
			$failed['codes'][] = 'invalid_option';
			$failed['errors'][] = sprintf(_('Unsupported Dimension draft binding evidence: %s.'), (string)$key);
			return $failed;
		}
	$draft = dimension_source_revision_read($draft_id);
	if (!is_array($draft) || $draft['state'] !== 'available') {
		$failed['codes'][] = 'missing_draft';
		$failed['errors'][] = _('The pending Dimension draft revision is missing or does not verify.');
		return $failed;
	}
	$revision = $draft['revision'];
	if (!dimension_source_revision_is_draft_key($revision['document_key'])) {
		$failed['codes'][] = 'not_a_draft';
		$failed['errors'][] = _('Only an unsaved draft revision can be bound to a final source identity.');
		return $failed;
	}
	$final_kind = $revision['line_kind'] === 'header' ? 'header' : 'line';
	$requested_kind = isset($identity['line_kind']) ? (string)$identity['line_kind'] : $final_kind;
	if ($requested_kind !== $final_kind) {
		$failed['codes'][] = 'binding_mismatch';
		$failed['errors'][] = _('The final line kind does not match the pending draft line kind.');
		return $failed;
	}
	$options = array(
		'effective_date' => isset($identity['effective_date'])
			? $identity['effective_date'] : $revision['effective_date'],
		'override_reason' => isset($identity['override_reason'])
			? $identity['override_reason'] : $revision['override_reason'],
		'resolution' => isset($identity['resolution'])
			? $identity['resolution'] : json_decode($revision['resolution_payload'], true),
		'line_quantity' => isset($identity['line_quantity'])
			? $identity['line_quantity'] : $revision['line_quantity'],
		'draft_binding_revision_id' => $draft_id);
	$prepared = dimension_source_revision_prepare($revision['source_family'], $revision['trans_type'],
		isset($identity['document_key']) ? $identity['document_key'] : '',
		$final_kind, isset($identity['line_key']) ? $identity['line_key'] : '', $revision['axes'],
		$options);
	if (!$prepared['ok']) {
		$failed['codes'] = array_merge($failed['codes'], $prepared['codes']);
		$failed['errors'] = $prepared['errors'];
		return $failed;
	}
	return dimension_source_revision_persist($prepared);
}

/**
 * Observe every stored source revision bound to one Dimension identity for the locked recheck.
 *
 * A missing schema cannot hold evidence, so it reports unused (like an uninstalled optional owner);
 * a present-but-unreadable or partial schema reports unknown, never unused. Must run inside the
 * caller's transaction so the shared locks hold until the decision completes.
 *
 * @param int $id Exact positive native Dimension identity
 * @return array source_revisions => used|unused|unknown
 */
function dimension_source_revision_observe($id)
{
	if (dimension_source_revision_int($id, 1) === false || empty($GLOBALS['transaction_level']))
		return array('source_revisions' => 'unknown');
	$ready = dimension_source_revision_schema_ready();
	if ($ready === 'absent')
		return array('source_revisions' => 'unused');
	if ($ready !== true)
		return array('source_revisions' => 'unknown');
	try {
		$id = (int)$id;
		$rows = db_query('SELECT 1 FROM '.TB_PREF.'dimension_source_revisions WHERE dimension_id='.$id
			.' OR dimension2_id='.$id.' LIMIT 1 LOCK IN SHARE MODE');
		if (!$rows)
			return array('source_revisions' => 'unknown');
		return array('source_revisions' => db_num_rows($rows) ? 'used' : 'unused');
	} catch (Exception $exception) {
		return array('source_revisions' => 'unknown');
	}
}
