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
 * DIM-APR-001: versioned approval classification snapshots, restoration and atomic execution.
 *
 * An approval draft is a delayed command. The classification a submitter reviewed must be the
 * classification the source executor posts, even after a customer/supplier/item default, a
 * Dimension category policy or a source revision changes while the draft waits. This file owns that
 * guarantee for the ordinary APR/SAL/PUR/GL/INV/MFG approval families:
 *
 *  - a draft payload may carry one additive `dimension_approval_snapshot` envelope (version 2)
 *    naming the exact DIM-FND-006 draft revisions the submitter bound; the envelope is sealed with
 *    a canonical SHA-256 digest and stores the exact axis evidence and revision seals;
 *  - a legacy payload that carries no envelope stays fully compatible. It is reported as a legacy
 *    compatibility outcome on review and in the approval message, is never recalculated and is
 *    never silently treated as if a snapshot had been verified;
 *  - before any approval action is recorded, and again inside the final execution transaction, the
 *    envelope is re-verified against the stored revisions: exact revision identity, exact revision
 *    number, exact payload seal, exact axes, still-current revision and current master/policy
 *    admission. A changed, closed, deleted, stale or tampered source refuses the whole action;
 *  - on execution the verified snapshot is restored onto the data the registered executor receives
 *    (native header/line identities plus the shared `classification_intents` carrier), so a stale
 *    payload value cannot win the posted classification;
 *  - a versioned execution records one immutable `snapshot` audit action with the verified revision
 *    set; if that receipt cannot be written the posting is rolled back with the approval action, the
 *    reserved number and the draft status.
 *
 * What it deliberately does not own:
 *  - no approval system replacement and no schema change: the envelope is additive draft JSON. The
 *    migration note of DIM-APR-001 is "additive draft JSON version only";
 *  - no privilege change: reject/cancel keep their original source authorities and are never gated
 *    by snapshot validity, because refusing a stale draft must stay possible for its owner;
 *  - no final source binding: binding a draft revision to the saved document stays with the owning
 *    module conversion (DIM-SAL-*, DIM-PUR-*, DIM-GL-001/002) and DIM-FND-006's
 *    `dimension_source_revision_bind()`.
 *
 * PHP 5.6 compatible syntax.
 */
require_once dirname(dirname(__DIR__)).'/dimensions/includes/dimension_source_revision.inc';
require_once dirname(dirname(__DIR__)).'/dimensions/includes/dimension_validation_contract.inc';

/**
 * Envelope schema name stored in the approval draft JSON.
 *
 * @return string
 */
function approval_dimension_snapshot_schema_name()
{
	return 'dimension-approval-snapshot';
}

/**
 * Current envelope version. Version 1 never shipped; version 2 is the first sealed envelope.
 *
 * @return int
 */
function approval_dimension_snapshot_schema_version()
{
	return 2;
}

/**
 * Additive draft JSON key that owns the envelope.
 *
 * @return string
 */
function approval_dimension_snapshot_payload_key()
{
	return 'dimension_approval_snapshot';
}

/**
 * Governed approval transaction types and their reviewed Dimension source family.
 *
 * The map is the intersection of the DIM-APR-001 module scope (APR SAL PUR GL INV MFG) with the
 * registered approval transaction types. A family is never accepted from the request: it is derived
 * from the stored transaction type, and a revision of another family is refused.
 *
 * @return array Transaction type => reviewed source family
 */
function approval_dimension_snapshot_types()
{
	static $types = null;
	if ($types !== null)
		return $types;
	$types = array();
	$sources = array(
		'gl'=>array('ST_JOURNAL','ST_BANKPAYMENT','ST_BANKDEPOSIT'),
		'purchasing'=>array('ST_PURCHORDER','ST_SUPPINVOICE','ST_PURCHREQUISITION',
			'ST_PURCHAGREEMENT','ST_PROCUREMENTPLAN','ST_PURCHMATCHEXCEPTION','ST_VENDOR_EVALUATION'),
		'sales'=>array('ST_SALESINVOICE','ST_SALESORDER','ST_CUSTDELIVERY','ST_SALESAGREEMENT',
			'ST_SALES_COMMISSION','ST_SALESRETURN'),
		'inventory'=>array('ST_INVADJUST','ST_LOCTRANSFER','ST_WH_MATERIAL_REQUEST',
			'ST_WH_TRANSFER_ORDER','ST_WH_SCRAP','ST_WH_CYCLE_COUNT'),
		'manufacturing'=>array('ST_WORKORDER'));
	foreach ($sources as $family => $constants)
		foreach ($constants as $constant)
			if (defined($constant))
				$types[(int)constant($constant)] = $family;
	return $types;
}

/**
 * Reviewed source family of one governed approval transaction type.
 *
 * @param mixed $trans_type Stored transaction type
 * @return string|false
 */
function approval_dimension_snapshot_family_for($trans_type)
{
	$type = dimension_source_revision_int($trans_type, 0);
	if ($type === false)
		return false;
	$types = approval_dimension_snapshot_types();
	return isset($types[$type]) ? $types[$type] : false;
}

/**
 * Is one approval transaction type governed by this authority?
 *
 * @param mixed $trans_type Stored transaction type
 * @return bool
 */
function approval_dimension_snapshot_governed($trans_type)
{
	return approval_dimension_snapshot_family_for($trans_type) !== false;
}

/**
 * Maximum sealed line entries. A bounded envelope cannot become an unbounded audit payload.
 *
 * @return int
 */
function approval_dimension_snapshot_line_limit()
{
	return 200;
}

/**
 * Normalize one axis evidence array from an envelope.
 *
 * @param mixed $entry value/mode/origin/basis/policy_version
 * @return array ok, axis, errors and codes
 */
function approval_dimension_snapshot_normalize_axis($entry)
{
	$result = array('ok' => false, 'axis' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($entry)) {
		$result['codes'][] = 'invalid_axis_evidence';
		$result['errors'][] = _('Every approval classification axis needs resolved evidence.');
		return $result;
	}
	$allowed = array('value','mode','origin','basis','policy_version');
	foreach ($entry as $key => $unused)
		if (!in_array($key, $allowed, true)) {
			$result['codes'][] = 'unknown_axis_field';
			$result['errors'][] = sprintf(_('Unsupported approval classification axis field: %s.'), (string)$key);
			return $result;
		}
	foreach ($allowed as $key)
		if (!array_key_exists($key, $entry)) {
			$result['codes'][] = 'missing_axis_field';
			$result['errors'][] = sprintf(_('The approval classification axis has no %s value.'), $key);
			return $result;
		}
	$normalized = dimension_source_revision_normalize_axis(1, $entry);
	if (!$normalized['ok']) {
		$result['codes'] = $normalized['codes'];
		$result['errors'] = $normalized['errors'];
		return $result;
	}
	$result['ok'] = true;
	$result['axis'] = $normalized['axis'];
	return $result;
}

/**
 * Normalize the two axis evidences of one envelope entry.
 *
 * @param mixed $axes Axis 1 and axis 2 evidence
 * @param bool $wrap Wrap a single axis evidence as axis 1
 * @return array ok, axes, errors and codes
 */
function approval_dimension_snapshot_normalize_axes($axes, $wrap = true)
{
	$result = array('ok' => false, 'axes' => array(), 'errors' => array(), 'codes' => array());
	if (!is_array($axes)) {
		$result['codes'][] = 'invalid_axes';
		$result['errors'][] = _('An approval classification snapshot needs both axis evidences.');
		return $result;
	}
	if (!isset($axes[1]) && !isset($axes[2]) && $wrap)
		$axes = array(1 => $axes);
	foreach (array(1, 2) as $axis) {
		if (!array_key_exists($axis, $axes)) {
			$result['codes'][] = 'missing_axis';
			$result['errors'][] = sprintf(_('The approval classification snapshot has no category %d evidence.'), $axis);
			return $result;
		}
		$normalized = approval_dimension_snapshot_normalize_axis($axes[$axis]);
		if (!$normalized['ok']) {
			$result['codes'] = array_merge($result['codes'], $normalized['codes']);
			foreach ($normalized['errors'] as $error)
				$result['errors'][] = sprintf(_('Category %d: %s'), $axis, $error);
			return $result;
		}
		$result['axes'][$axis] = $normalized['axis'];
	}
	foreach ($axes as $key => $unused)
		if ($key !== 1 && $key !== 2) {
			$result['codes'][] = 'unexpected_axis';
			$result['errors'][] = _('An approval classification snapshot carries exactly two categories.');
			return $result;
		}
	$result['ok'] = true;
	return $result;
}

/**
 * Build and normalize one revision reference of an envelope entry.
 *
 * @param mixed $reference source_revision_id/revision_no/payload_sha256/source_identity_sha256
 * @return array ok, reference, errors and codes
 */
function approval_dimension_snapshot_normalize_reference($reference)
{
	$result = array('ok' => false, 'reference' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($reference)) {
		$result['codes'][] = 'missing_revision_reference';
		$result['errors'][] = _('An approval classification entry must name its exact stored source revision.');
		return $result;
	}
	$allowed = array('source_revision_id','revision_no','payload_sha256','source_identity_sha256');
	foreach ($reference as $key => $unused)
		if (!in_array($key, $allowed, true)) {
			$result['codes'][] = 'unknown_revision_field';
			$result['errors'][] = sprintf(_('Unsupported approval revision reference field: %s.'), (string)$key);
			return $result;
		}
	$id = isset($reference['source_revision_id'])
		? dimension_source_revision_int($reference['source_revision_id'], 1) : false;
	$number = isset($reference['revision_no'])
		? dimension_source_revision_int($reference['revision_no'], 1) : false;
	$payload = isset($reference['payload_sha256']) ? (string)$reference['payload_sha256'] : '';
	$identity = isset($reference['source_identity_sha256']) ? (string)$reference['source_identity_sha256'] : '';
	if ($id === false || $number === false
		|| !preg_match('/^[a-f0-9]{64}$/D', $payload)
		|| !preg_match('/^[a-f0-9]{64}$/D', $identity)) {
		$result['codes'][] = 'invalid_revision_reference';
		$result['errors'][] = _('An approval classification revision reference is not canonical.');
		return $result;
	}
	$result['ok'] = true;
	$result['reference'] = array('source_revision_id' => $id, 'revision_no' => $number,
		'payload_sha256' => $payload, 'source_identity_sha256' => $identity);
	return $result;
}

/**
 * Normalize one header or line entry of an envelope.
 *
 * @param mixed $entry Envelope entry
 * @param string $line_kind Expected kind (`header` or `draft_line`)
 * @return array ok, entry, errors and codes
 */
function approval_dimension_snapshot_normalize_entry($entry, $line_kind)
{
	$result = array('ok' => false, 'entry' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($entry)) {
		$result['codes'][] = 'invalid_entry';
		$result['errors'][] = _('An approval classification entry must be a structured value.');
		return $result;
	}
	$allowed = array('line_kind','line_key','axes','revision','account');
	foreach ($entry as $key => $unused)
		if (!in_array($key, $allowed, true)) {
			$result['codes'][] = 'unknown_entry_field';
			$result['errors'][] = sprintf(_('Unsupported approval classification entry field: %s.'), (string)$key);
			return $result;
		}
	if (!isset($entry['line_kind']) || (string)$entry['line_kind'] !== $line_kind) {
		$result['codes'][] = 'wrong_line_kind';
		$result['errors'][] = _('An approval classification entry does not match its declared line kind.');
		return $result;
	}
	$line_key = isset($entry['line_key']) ? (string)$entry['line_key'] : '';
	if (!dimension_source_revision_valid_line_key($line_kind, $line_key)) {
		$result['codes'][] = 'invalid_line_key';
		$result['errors'][] = _('An approval classification entry needs its stable source line key.');
		return $result;
	}
	$axes = approval_dimension_snapshot_normalize_axes(isset($entry['axes']) ? $entry['axes'] : null);
	if (!$axes['ok']) {
		$result['codes'] = array_merge($result['codes'], $axes['codes']);
		$result['errors'] = $axes['errors'];
		return $result;
	}
	$reference = approval_dimension_snapshot_normalize_reference(
		isset($entry['revision']) ? $entry['revision'] : null);
	if (!$reference['ok']) {
		$result['codes'] = array_merge($result['codes'], $reference['codes']);
		$result['errors'] = $reference['errors'];
		return $result;
	}
	$account = '';
	if (array_key_exists('account', $entry) && $entry['account'] !== '' && $entry['account'] !== null) {
		$account = dimension_source_revision_clean_key($entry['account'], 15);
		if ($account === false) {
			$result['codes'][] = 'invalid_account';
			$result['errors'][] = _('An approval classification account rule reference is not canonical.');
			return $result;
		}
	}
	$result['ok'] = true;
	$result['entry'] = array('line_kind' => $line_kind, 'line_key' => $line_key,
		'axes' => $axes['axes'], 'revision' => $reference['reference'], 'account' => $account);
	return $result;
}

/**
 * Compare two normalized axis evidences exactly.
 *
 * @param array $left Normalized axis
 * @param array $right Normalized axis
 * @return bool
 */
function approval_dimension_snapshot_axis_equal($left, $right)
{
	if (!is_array($left) || !is_array($right))
		return false;
	foreach (array('value','mode','origin','basis','policy_version') as $key)
		if (!array_key_exists($key, $left) || !array_key_exists($key, $right)
			|| (string)$left[$key] !== (string)$right[$key])
			return false;
	return true;
}

/**
 * Compare two normalized entry axis sets exactly.
 *
 * @param array $left Axis set
 * @param array $right Axis set
 * @return bool
 */
function approval_dimension_snapshot_axes_equal($left, $right)
{
	if (!is_array($left) || !is_array($right) || !isset($left[1], $left[2], $right[1], $right[2]))
		return false;
	foreach (array(1, 2) as $axis)
		if (!approval_dimension_snapshot_axis_equal($left[$axis], $right[$axis]))
			return false;
	return true;
}

/**
 * Canonical SHA-256 seal of one envelope, excluding the seal field itself.
 *
 * @param mixed $snapshot Envelope
 * @return string|false
 */
function approval_dimension_snapshot_seal($snapshot)
{
	if (!is_array($snapshot))
		return false;
	unset($snapshot['snapshot_sha256']);
	$json = dimension_source_revision_payload_json($snapshot);
	return $json === false ? false : hash('sha256', $json);
}

/**
 * Verify one envelope structurally without any database read.
 *
 * A present-but-invalid envelope is never downgraded to "legacy": tampering must fail closed.
 *
 * @param mixed $snapshot Envelope
 * @return array ok, state, snapshot, errors and codes
 */
function approval_dimension_snapshot_verify($snapshot)
{
	$result = array('ok' => false, 'state' => 'tamper', 'snapshot' => null,
		'errors' => array(), 'codes' => array());
	if (!is_array($snapshot)) {
		$result['codes'][] = 'invalid_envelope';
		$result['errors'][] = _('The approval classification snapshot is not a structured envelope.');
		return $result;
	}
	$allowed = array('schema','version','trans_type','source_family','document_key','header','lines',
		'snapshot_sha256');
	foreach ($snapshot as $key => $unused)
		if (!in_array($key, $allowed, true)) {
			$result['codes'][] = 'unknown_envelope_field';
			$result['errors'][] = sprintf(_('Unsupported approval classification envelope field: %s.'), (string)$key);
			return $result;
		}
	foreach ($allowed as $key)
		if (!array_key_exists($key, $snapshot)) {
			$result['codes'][] = 'missing_envelope_field';
			$result['errors'][] = sprintf(_('The approval classification envelope has no %s value.'), $key);
			return $result;
		}
	if ((string)$snapshot['schema'] !== approval_dimension_snapshot_schema_name()) {
		$result['codes'][] = 'unknown_schema';
		$result['errors'][] = _('The approval classification envelope schema is not registered.');
		return $result;
	}
	$version = dimension_source_revision_int($snapshot['version'], 1);
	if ($version === false || $version !== approval_dimension_snapshot_schema_version()) {
		$result['codes'][] = 'unsupported_version';
		$result['errors'][] = _('The approval classification envelope version is not supported.');
		return $result;
	}
	$type = dimension_source_revision_int($snapshot['trans_type'], 0);
	$family = approval_dimension_snapshot_family_for($type);
	if ($type === false || $family === false) {
		$result['codes'][] = 'unsupported_type';
		$result['errors'][] = _('This transaction type is not governed by the approval classification snapshot.');
		return $result;
	}
	if ((string)$snapshot['source_family'] !== $family) {
		$result['codes'][] = 'family_mismatch';
		$result['errors'][] = _('The approval classification family does not match its transaction type.');
		return $result;
	}
	$document_key = isset($snapshot['document_key']) ? (string)$snapshot['document_key'] : '';
	if (!dimension_source_revision_is_draft_key($document_key)
		|| !preg_match('/^draft:[a-f0-9]{64}$/D', $document_key)) {
		$result['codes'][] = 'invalid_document_key';
		$result['errors'][] = _('An approval classification snapshot must name an unsaved draft identity.');
		return $result;
	}
	$header = approval_dimension_snapshot_normalize_entry($snapshot['header'], 'header');
	if (!$header['ok']) {
		$result['codes'] = array_merge($result['codes'], $header['codes']);
		foreach ($header['errors'] as $error)
			$result['errors'][] = sprintf(_('Header: %s'), $error);
		return $result;
	}
	if (!is_array($snapshot['lines'])) {
		$result['codes'][] = 'invalid_lines';
		$result['errors'][] = _('The approval classification snapshot line set is not a list.');
		return $result;
	}
	$lines = array_values($snapshot['lines']);
	if (count($lines) > approval_dimension_snapshot_line_limit()) {
		$result['codes'][] = 'too_many_lines';
		$result['errors'][] = _('The approval classification snapshot carries more lines than the sealed limit.');
		return $result;
	}
	$normalized_lines = array();
	$line_keys = array();
	$revision_ids = array();
	$revision_ids[(int)$header['entry']['revision']['source_revision_id']] = true;
	foreach ($lines as $index => $line) {
		$entry = approval_dimension_snapshot_normalize_entry($line, 'draft_line');
		if (!$entry['ok']) {
			$result['codes'] = array_merge($result['codes'], $entry['codes']);
			foreach ($entry['errors'] as $error)
				$result['errors'][] = sprintf(_('Line %d: %s'), $index + 1, $error);
			return $result;
		}
		if (isset($line_keys[$entry['entry']['line_key']])) {
			$result['codes'][] = 'duplicate_line_key';
			$result['errors'][] = _('Two approval classification entries name the same stable source line key.');
			return $result;
		}
		$line_keys[$entry['entry']['line_key']] = true;
		$revision_id = (int)$entry['entry']['revision']['source_revision_id'];
		if (isset($revision_ids[$revision_id])) {
			$result['codes'][] = 'duplicate_revision';
			$result['errors'][] = _('Two approval classification entries name the same stored source revision.');
			return $result;
		}
		$revision_ids[$revision_id] = true;
		$normalized_lines[] = $entry['entry'];
	}
	$normalized = array(
		'schema' => approval_dimension_snapshot_schema_name(),
		'version' => (int)approval_dimension_snapshot_schema_version(),
		'trans_type' => (int)$type,
		'source_family' => $family,
		'document_key' => $document_key,
		'header' => $header['entry'],
		'lines' => $normalized_lines);
	$seal = (string)$snapshot['snapshot_sha256'];
	if (!preg_match('/^[a-f0-9]{64}$/D', $seal)) {
		$result['codes'][] = 'invalid_seal';
		$result['errors'][] = _('The approval classification snapshot has no canonical seal.');
		return $result;
	}
	// The seal covers the stored envelope shape, never an internal normalization: otherwise an
	// omitted optional field would change the digest of unchanged evidence.
	$expected = approval_dimension_snapshot_seal($snapshot);
	if ($expected === false || $expected !== $seal) {
		$result['codes'][] = 'seal_mismatch';
		$result['errors'][] = _('The approval classification snapshot seal does not verify; the payload was changed after sealing.');
		return $result;
	}
	$normalized['snapshot_sha256'] = $seal;
	$result['ok'] = true;
	$result['state'] = 'available';
	$result['snapshot'] = $normalized;
	return $result;
}

/**
 * Read the envelope of one decoded draft payload without a database read.
 *
 * @param mixed $data Decoded draft payload
 * @return array state (absent|available|tamper), snapshot, errors and codes
 */
function approval_dimension_snapshot_read($data)
{
	$result = array('state' => 'absent', 'snapshot' => null, 'errors' => array(), 'codes' => array());
	$key = approval_dimension_snapshot_payload_key();
	if (!is_array($data) || !array_key_exists($key, $data)) {
		$result['codes'][] = 'legacy_payload';
		return $result;
	}
	$verified = approval_dimension_snapshot_verify($data[$key]);
	$result['state'] = $verified['ok'] ? 'available' : 'tamper';
	$result['snapshot'] = $verified['ok'] ? $verified['snapshot'] : null;
	$result['errors'] = $verified['errors'];
	$result['codes'] = $verified['codes'];
	return $result;
}

/**
 * Build the sealed snapshot of one governed approval submission from stored draft revisions.
 *
 * The caller names only stored revision identities; both axes and the revision seals are read from
 * the DIM-FND-006 authority, so a caller cannot forge the classification inside the envelope.
 *
 * @param mixed $trans_type Governed transaction type
 * @param array $request source_family, document_key, header (revision_id), lines (list of
 *                       revision_id and optional account)
 * @return array ok, state, snapshot, errors and codes
 */
function approval_dimension_snapshot_capture($trans_type, $request)
{
	$result = array('ok' => false, 'state' => 'invalid', 'snapshot' => null,
		'errors' => array(), 'codes' => array());
	$type = dimension_source_revision_int($trans_type, 0);
	$family = approval_dimension_snapshot_family_for($type);
	if ($type === false || $family === false) {
		$result['codes'][] = 'unsupported_type';
		$result['errors'][] = _('This transaction type cannot carry an approval classification snapshot.');
		return $result;
	}
	if (!is_array($request)) {
		$result['codes'][] = 'invalid_request';
		$result['errors'][] = _('An approval classification snapshot needs its revision evidence.');
		return $result;
	}
	foreach ($request as $key => $unused)
		if (!in_array($key, array('source_family','document_key','header','lines'), true)) {
			$result['codes'][] = 'unknown_request_field';
			$result['errors'][] = sprintf(_('Unsupported approval snapshot request field: %s.'), (string)$key);
			return $result;
		}
	if (!isset($request['source_family']) || (string)$request['source_family'] !== $family) {
		$result['codes'][] = 'family_mismatch';
		$result['errors'][] = _('The requested classification family does not match this transaction type.');
		return $result;
	}
	$document_key = isset($request['document_key']) ? (string)$request['document_key'] : '';
	if (!dimension_source_revision_is_draft_key($document_key)
		|| !preg_match('/^draft:[a-f0-9]{64}$/D', $document_key)) {
		$result['codes'][] = 'invalid_document_key';
		$result['errors'][] = _('An approval classification snapshot must name an unsaved draft identity.');
		return $result;
	}
	$header = approval_dimension_snapshot_capture_entry(isset($request['header']) ? $request['header'] : null,
		'header', $family, $type, $document_key);
	if (!$header['ok']) {
		$result['codes'] = array_merge($result['codes'], $header['codes']);
		foreach ($header['errors'] as $error)
			$result['errors'][] = sprintf(_('Header: %s'), $error);
		return $result;
	}
	$lines = array();
	if (isset($request['lines'])) {
		if (!is_array($request['lines'])) {
			$result['codes'][] = 'invalid_lines';
			$result['errors'][] = _('An approval classification line set must be a list.');
			return $result;
		}
		$request_lines = array_values($request['lines']);
		if (count($request_lines) > approval_dimension_snapshot_line_limit()) {
			$result['codes'][] = 'too_many_lines';
			$result['errors'][] = _('The approval classification snapshot carries more lines than the sealed limit.');
			return $result;
		}
		$revision_ids = array((int)$header['entry']['revision']['source_revision_id'] => true);
		$line_keys = array();
		foreach ($request_lines as $index => $line) {
			$entry = approval_dimension_snapshot_capture_entry($line, 'draft_line', $family, $type,
				$document_key);
			if (!$entry['ok']) {
				$result['codes'] = array_merge($result['codes'], $entry['codes']);
				foreach ($entry['errors'] as $error)
					$result['errors'][] = sprintf(_('Line %d: %s'), $index + 1, $error);
				return $result;
			}
			$revision_id = (int)$entry['entry']['revision']['source_revision_id'];
			if (isset($revision_ids[$revision_id])) {
				$result['codes'][] = 'duplicate_revision';
				$result['errors'][] = _('Two approval classification entries name the same stored source revision.');
				return $result;
			}
			$revision_ids[$revision_id] = true;
			if (isset($line_keys[$entry['entry']['line_key']])) {
				$result['codes'][] = 'duplicate_line_key';
				$result['errors'][] = _('Two approval classification entries name the same stable source line key.');
				return $result;
			}
			$line_keys[$entry['entry']['line_key']] = true;
			$lines[] = $entry['entry'];
		}
	}
	$snapshot = array(
		'schema' => approval_dimension_snapshot_schema_name(),
		'version' => (int)approval_dimension_snapshot_schema_version(),
		'trans_type' => (int)$type,
		'source_family' => $family,
		'document_key' => $document_key,
		'header' => $header['entry'],
		'lines' => $lines,
		'snapshot_sha256' => '');
	$seal = approval_dimension_snapshot_seal($snapshot);
	if ($seal === false) {
		$result['codes'][] = 'seal_failed';
		$result['errors'][] = _('The approval classification snapshot could not be sealed.');
		return $result;
	}
	$snapshot['snapshot_sha256'] = $seal;
	$verified = approval_dimension_snapshot_verify($snapshot);
	if (!$verified['ok']) {
		$result['codes'] = array_merge($result['codes'], $verified['codes']);
		$result['errors'] = $verified['errors'];
		return $result;
	}
	$result['ok'] = true;
	$result['state'] = 'versioned';
	$result['snapshot'] = $verified['snapshot'];
	return $result;
}

/**
 * Read and bind one stored draft revision into one envelope entry.
 *
 * @param mixed $request Entry request with revision_id and optional account
 * @param string $line_kind Expected stored line kind
 * @param string $family Reviewed family
 * @param int $trans_type Stored transaction type
 * @param string $document_key Canonical draft document key
 * @return array ok, entry, errors and codes
 */
function approval_dimension_snapshot_capture_entry($request, $line_kind, $family, $trans_type,
	$document_key)
{
	$result = array('ok' => false, 'entry' => null, 'errors' => array(), 'codes' => array());
	if (!is_array($request)) {
		$result['codes'][] = 'invalid_entry';
		$result['errors'][] = _('An approval classification entry must name one stored source revision.');
		return $result;
	}
	foreach ($request as $key => $unused)
		if (!in_array($key, array('revision_id','account'), true)) {
			$result['codes'][] = 'unknown_entry_field';
			$result['errors'][] = sprintf(_('Unsupported approval classification entry field: %s.'), (string)$key);
			return $result;
		}
	$revision_id = isset($request['revision_id'])
		? dimension_source_revision_int($request['revision_id'], 1) : false;
	if ($revision_id === false) {
		$result['codes'][] = 'invalid_revision_identity';
		$result['errors'][] = _('An approval classification entry needs a stored source revision identity.');
		return $result;
	}
	$read = dimension_source_revision_read($revision_id);
	if (!is_array($read) || $read['state'] !== 'available') {
		$result['codes'][] = 'revision_unavailable';
		$result['errors'][] = _('The named source revision is missing or does not verify.');
		if (is_array($read) && !empty($read['codes']))
			$result['codes'] = array_merge($result['codes'], $read['codes']);
		return $result;
	}
	$revision = $read['revision'];
	if ($revision['source_family'] !== $family || (int)$revision['trans_type'] !== (int)$trans_type
		|| $revision['document_key'] !== $document_key
		|| $revision['line_kind'] !== $line_kind) {
		$result['codes'][] = 'revision_identity_mismatch';
		$result['errors'][] = _('The named source revision does not belong to this approval document, family, type and line kind.');
		return $result;
	}
	if (!dimension_source_revision_policy_verified($revision)) {
		$result['codes'][] = 'policy_version_unverified';
		$result['errors'][] = _('A policy version named by the source revision is missing or does not verify.');
		return $result;
	}
	$current = dimension_source_revision_find_current($revision['source_identity_sha256']);
	if ($current === false) {
		$result['codes'][] = 'revision_not_current';
		$result['errors'][] = _('The named source revision is not the current revision of its source identity.');
		return $result;
	}
	if (!is_array($current) || isset($current['read_error'])) {
		$result['codes'][] = 'revision_unavailable';
		$result['errors'][] = _('The current source revision cannot be verified.');
		return $result;
	}
	if ((int)$current['source_revision_id'] !== (int)$revision_id) {
		$result['codes'][] = 'revision_stale';
		$result['errors'][] = sprintf(_('Revision %d is no longer current; revision %d supersedes it.'),
			(int)$revision['revision_no'], (int)$current['revision_no']);
		return $result;
	}
	$account = '';
	if (array_key_exists('account', $request) && $request['account'] !== ''
		&& $request['account'] !== null) {
		$account = dimension_source_revision_clean_key($request['account'], 15);
		if ($account === false) {
			$result['codes'][] = 'invalid_account';
			$result['errors'][] = _('An approval classification account rule reference is not canonical.');
			return $result;
		}
	}
	$result['ok'] = true;
	$result['entry'] = array('line_kind' => $line_kind, 'line_key' => $revision['line_key'],
		'axes' => $revision['axes'],
		'revision' => array('source_revision_id' => (int)$revision_id,
			'revision_no' => (int)$revision['revision_no'],
			'payload_sha256' => (string)$revision['payload_sha256'],
			'source_identity_sha256' => (string)$revision['source_identity_sha256']),
		'account' => $account);
	return $result;
}

/**
 * Attach or verify the snapshot envelope of one submission.
 *
 * Absence of both a snapshot request and an envelope is a legitimate legacy payload; it is reported
 * as `legacy` and never silently upgraded. An optional request is server-side context, never raw
 * request input; the source context that builds it owns its authority.
 *
 * @param mixed $trans_type Stored transaction type
 * @param mixed $data Draft payload
 * @param mixed $request Optional snapshot request
 * @return array ok, state (not_governed|legacy|versioned), version, data, snapshot, errors and codes
 */
function approval_dimension_snapshot_attach_submission($trans_type, $data, $request = null)
{
	$result = array('ok' => true, 'state' => 'not_governed', 'version' => 0, 'data' => $data,
		'snapshot' => null, 'errors' => array(), 'codes' => array());
	$type = dimension_source_revision_int($trans_type, 0);
	if ($type === false || !approval_dimension_snapshot_governed($type))
		return $result;
	$key = approval_dimension_snapshot_payload_key();
	$has_envelope = is_array($data) && array_key_exists($key, $data);
	if ($request === null && !$has_envelope) {
		$result['state'] = 'legacy';
		$result['codes'][] = 'legacy_payload';
		return $result;
	}
	if (!is_array($data)) {
		$result['ok'] = false;
		$result['state'] = 'invalid';
		$result['codes'][] = 'invalid_payload';
		$result['errors'][] = _('A classification snapshot can only be bound to a structured draft payload.');
		return $result;
	}
	if ($has_envelope) {
		$existing = approval_dimension_snapshot_verify($data[$key]);
		if (!$existing['ok']) {
			$result['ok'] = false;
			$result['state'] = 'tamper';
			$result['codes'] = $existing['codes'];
			$result['errors'] = $existing['errors'];
			return $result;
		}
		if ($request === null) {
			$result['state'] = 'versioned';
			$result['version'] = approval_dimension_snapshot_schema_version();
			$result['snapshot'] = $existing['snapshot'];
			return $result;
		}
	}
	$capture = approval_dimension_snapshot_capture($type, $request);
	if (!$capture['ok']) {
		$result['ok'] = false;
		$result['state'] = 'invalid';
		$result['codes'] = $capture['codes'];
		$result['errors'] = $capture['errors'];
		return $result;
	}
	if ($has_envelope && $capture['snapshot']['snapshot_sha256'] !== $data[$key]['snapshot_sha256']) {
		$result['ok'] = false;
		$result['state'] = 'conflict';
		$result['codes'][] = 'snapshot_conflict';
		$result['errors'][] = _('The draft already carries a different classification snapshot; an exact retry must reproduce it.');
		return $result;
	}
	$data[$key] = $capture['snapshot'];
	$result['state'] = 'versioned';
	$result['version'] = approval_dimension_snapshot_schema_version();
	$result['data'] = $data;
	$result['snapshot'] = $capture['snapshot'];
	return $result;
}

/**
 * Revalidate one sealed envelope against its exact stored revisions and the current policy.
 *
 * @param mixed $snapshot Envelope
 * @param array $options lock: use locking reads inside the caller's transaction
 * @return array ok, state (valid|tamper|unavailable|stale|invalid), entries, snapshot, errors and codes
 */
function approval_dimension_snapshot_revalidate($snapshot, $options = array())
{
	$result = array('ok' => false, 'state' => 'tamper', 'entries' => array(), 'snapshot' => null,
		'errors' => array(), 'codes' => array());
	$verified = approval_dimension_snapshot_verify($snapshot);
	if (!$verified['ok']) {
		$result['codes'] = $verified['codes'];
		$result['errors'] = $verified['errors'];
		return $result;
	}
	$snapshot = $verified['snapshot'];
	$result['snapshot'] = $snapshot;
	$lock = !empty($options['lock']) && !empty($GLOBALS['transaction_level']);
	$ready = dimension_source_revision_schema_ready();
	if ($ready === 'absent') {
		$result['state'] = 'unavailable';
		$result['codes'][] = 'schema_unavailable';
		$result['errors'][] = _('The durable Dimension source revision store is not installed, so the snapshot cannot be verified.');
		return $result;
	}
	if ($ready !== true) {
		$result['codes'][] = 'schema_unknown';
		$result['errors'][] = _('The durable Dimension source revision store cannot be verified.');
		return $result;
	}
	$entries = array_merge(array($snapshot['header']), $snapshot['lines']);
	$stale = false;
	$invalid = false;
	$revision_ids = array();
	foreach ($entries as $index => $entry) {
		$label = $index === 0 ? _('Header') : sprintf(_('Line %s'), $entry['line_key']);
		$reference = $entry['revision'];
		if (isset($revision_ids[(int)$reference['source_revision_id']])) {
			$result['state'] = 'tamper';
			$result['codes'][] = 'duplicate_revision';
			$result['errors'][] = _('Two approval classification entries name the same stored source revision.');
			return $result;
		}
		$revision_ids[(int)$reference['source_revision_id']] = true;
		$read = dimension_source_revision_read($reference['source_revision_id'], $lock);
		if (!is_array($read) || $read['state'] !== 'available') {
			$result['state'] = is_array($read) && $read['state'] === 'unavailable' ? 'unavailable' : 'tamper';
			$result['codes'][] = 'revision_unavailable';
			$result['errors'][] = sprintf(_('%s: the named source revision is missing or does not verify.'), $label);
			return $result;
		}
		$revision = $read['revision'];
		if (!dimension_source_revision_policy_verified($revision)) {
			$result['state'] = 'tamper';
			$result['codes'][] = 'policy_version_unverified';
			$result['errors'][] = sprintf(_('%s: a policy version named by the source revision is missing or does not verify.'), $label);
			return $result;
		}
		if ((int)$revision['source_revision_id'] !== (int)$reference['source_revision_id']
			|| (int)$revision['revision_no'] !== (int)$reference['revision_no']
			|| (string)$revision['payload_sha256'] !== (string)$reference['payload_sha256']
			|| (string)$revision['source_identity_sha256'] !== (string)$reference['source_identity_sha256']) {
			$result['state'] = 'tamper';
			$result['codes'][] = 'revision_seal_mismatch';
			$result['errors'][] = sprintf(_('%s: the stored source revision no longer matches the sealed snapshot.'), $label);
			return $result;
		}
		if ($revision['source_family'] !== $snapshot['source_family']
			|| (int)$revision['trans_type'] !== (int)$snapshot['trans_type']
			|| $revision['document_key'] !== $snapshot['document_key']
			|| $revision['line_kind'] !== $entry['line_kind']
			|| $revision['line_key'] !== $entry['line_key']) {
			$result['state'] = 'tamper';
			$result['codes'][] = 'revision_identity_mismatch';
			$result['errors'][] = sprintf(_('%s: the stored source revision identity does not match the sealed snapshot.'), $label);
			return $result;
		}
		if (!approval_dimension_snapshot_axes_equal($revision['axes'], $entry['axes'])) {
			$result['state'] = 'tamper';
			$result['codes'][] = 'revision_axis_mismatch';
			$result['errors'][] = sprintf(_('%s: the stored source revision classification no longer matches the sealed snapshot.'), $label);
			return $result;
		}
		$current = dimension_source_revision_find_current($revision['source_identity_sha256'], 0, $lock);
		if ($current === false) {
			$result['state'] = 'tamper';
			$result['codes'][] = 'revision_absent';
			$result['errors'][] = sprintf(_('%s: the source identity has no current revision.'), $label);
			return $result;
		}
		if (!is_array($current) || isset($current['read_error'])) {
			$result['state'] = 'unavailable';
			$result['codes'][] = 'revision_unavailable';
			$result['errors'][] = sprintf(_('%s: the current source revision cannot be read.'), $label);
			return $result;
		}
		if ((int)$current['source_revision_id'] !== (int)$revision['source_revision_id']) {
			$stale = true;
			$result['codes'][] = 'revision_stale';
			$result['errors'][] = sprintf(_('%s: revision %d has been superseded by revision %d.'),
				$label, (int)$revision['revision_no'], (int)$current['revision_no']);
		}
		$classification = dimension_validation_classification($entry['axes'][1]['value'],
			$entry['axes'][2]['value'], 'new_event', array('account' => $entry['account']));
		if (!$classification['ok']) {
			$invalid = true;
			$result['codes'] = array_merge($result['codes'], $classification['codes']);
			$reason = !empty($classification['errors']) ? $classification['errors'][0]
				: _('the classification is no longer admitted');
			$result['errors'][] = sprintf(_('%s: %s'), $label, $reason);
		}
		$result['entries'][] = array('label' => $label, 'line_kind' => $entry['line_kind'],
			'line_key' => $entry['line_key'], 'account' => $entry['account'],
			'axes' => $entry['axes'], 'revision' => $reference,
			'revision_no' => (int)$revision['revision_no'],
			'created_at' => isset($revision['created_at']) ? (string)$revision['created_at'] : '',
			'actor_id' => (int)$revision['actor_id'],
			'classification_ok' => $classification['ok']);
	}
	if ($stale) {
		$result['state'] = 'stale';
		return $result;
	}
	if ($invalid) {
		$result['state'] = 'invalid';
		return $result;
	}
	$result['ok'] = true;
	$result['state'] = 'valid';
	return $result;
}

/**
 * Verify one decoded draft payload before an approval decision.
 *
 * A legacy payload stays admissible; the outcome is visible to the approver and on review. A
 * versioned payload must be exact-match current, or the action is refused before anything is written.
 *
 * @param array $draft Approval draft row (with id, trans_type)
 * @param mixed $data Decoded draft payload
 * @return array ok, state (not_governed|legacy|versioned), validation, notice, errors and codes
 */
function approval_dimension_snapshot_preflight($draft, $data)
{
	$result = array('ok' => true, 'state' => 'not_governed', 'validation' => null,
		'notice' => '', 'errors' => array(), 'codes' => array());
	$trans_type = isset($draft['trans_type']) ? (int)$draft['trans_type'] : 0;
	if (!approval_dimension_snapshot_governed($trans_type))
		return $result;
	$read = approval_dimension_snapshot_read($data);
	if ($read['state'] === 'absent') {
		$result['state'] = 'legacy';
		$result['notice'] = approval_dimension_snapshot_notice('legacy');
		$result['codes'][] = 'legacy_payload';
		return $result;
	}
	if ($read['state'] === 'tamper') {
		$result['ok'] = false;
		$result['state'] = 'tamper';
		$result['codes'] = $read['codes'];
		$result['errors'] = $read['errors'];
		return $result;
	}
	$validation = approval_dimension_snapshot_revalidate($read['snapshot'],
		array('lock' => !empty($GLOBALS['transaction_level'])));
	if (!$validation['ok']) {
		$result['ok'] = false;
		$result['state'] = $validation['state'];
		$result['validation'] = $validation;
		$result['codes'] = $validation['codes'];
		$result['errors'] = $validation['errors'];
		return $result;
	}
	$result['state'] = 'versioned';
	$result['validation'] = $validation;
	$result['notice'] = approval_dimension_snapshot_notice('versioned');
	return $result;
}

/**
 * Prepare one approved execution: revalidate, guard duplicate execution and restore the snapshot.
 *
 * @param array $draft Approval draft row
 * @param mixed $data Decoded draft payload
 * @return array ok, state, data, validation, notice, receipt, errors and codes
 */
function approval_dimension_snapshot_execution($draft, $data)
{
	$result = array('ok' => false, 'state' => 'not_governed', 'data' => $data, 'validation' => null,
		'notice' => '', 'errors' => array(), 'codes' => array());
	$preflight = approval_dimension_snapshot_preflight($draft, $data);
	if (!$preflight['ok']) {
		$result['state'] = $preflight['state'];
		$result['validation'] = $preflight['validation'];
		$result['errors'] = $preflight['errors'];
		$result['codes'] = $preflight['codes'];
		return $result;
	}
	if ($preflight['state'] !== 'versioned') {
		$result['ok'] = true;
		$result['state'] = $preflight['state'];
		$result['notice'] = $preflight['notice'];
		return $result;
	}
	$draft_id = isset($draft['id']) ? (int)$draft['id'] : 0;
	if ($draft_id > 0 && function_exists('get_last_approval_action')
		&& get_last_approval_action($draft_id, 'snapshot')) {
		$result['state'] = 'already_executed';
		$result['codes'][] = 'duplicate_execution';
		$result['errors'][] = _('This draft already recorded a verified classification execution receipt.');
		return $result;
	}
	$restore = approval_dimension_snapshot_restore($data, $preflight['validation']['snapshot'],
		$preflight['validation']);
	if (!$restore['ok']) {
		$result['state'] = 'mapping_failed';
		$result['errors'] = $restore['errors'];
		$result['codes'] = $restore['codes'];
		return $result;
	}
	$result['ok'] = true;
	$result['state'] = 'versioned';
	$result['data'] = $restore['data'];
	$result['validation'] = $preflight['validation'];
	$result['notice'] = $preflight['notice'];
	return $result;
}

/**
 * Build the shared axis intents of one sealed entry.
 *
 * @param array $axes Normalized axis set
 * @return array ok, intents, errors and codes
 */
function approval_dimension_snapshot_entry_intents($axes)
{
	$result = array('ok' => false, 'intents' => array(), 'errors' => array(), 'codes' => array());
	if (!is_array($axes) || !isset($axes[1], $axes[2])) {
		$result['codes'][] = 'invalid_axes';
		$result['errors'][] = _('The sealed classification has no axis pair to restore.');
		return $result;
	}
	foreach (array(1, 2) as $axis) {
		$entry = $axes[$axis];
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
		$result['intents'][$axis] = $intent['intent'];
	}
	$result['ok'] = true;
	return $result;
}

/**
 * Restore one verified snapshot onto the data the registered executor reconstructs.
 *
 * The header identity is applied through both legacy spellings (`dimension_id`/`dimension2_id` and
 * `dimension`/`dimension2`) and the shared `classification_intents` carrier. Line entries are mapped
 * by their sealed stable line key when the payload carries it, otherwise by the single carrier order
 * the payload declares; an ambiguous or incomplete mapping refuses execution rather than partially
 * restoring a classification.
 *
 * @param mixed $data Decoded draft payload
 * @param array $snapshot Verified envelope from a successful revalidation
 * @param array $validation Successful revalidation result
 * @return array ok, data, errors and codes
 */
function approval_dimension_snapshot_restore($data, $snapshot, $validation)
{
	$result = array('ok' => false, 'data' => $data, 'errors' => array(), 'codes' => array());
	if (!is_array($data) || !is_array($snapshot) || !is_array($validation)
		|| empty($validation['ok'])) {
		$result['codes'][] = 'invalid_restore';
		$result['errors'][] = _('Only a verified classification snapshot can be restored onto a draft payload.');
		return $result;
	}
	$header_intents = approval_dimension_snapshot_entry_intents($snapshot['header']['axes']);
	if (!$header_intents['ok']) {
		$result['codes'] = $header_intents['codes'];
		$result['errors'] = $header_intents['errors'];
		return $result;
	}
	$restored = $data;
	$restored['dimension_id'] = (int)$snapshot['header']['axes'][1]['value'];
	$restored['dimension2_id'] = (int)$snapshot['header']['axes'][2]['value'];
	$restored['dimension'] = (int)$snapshot['header']['axes'][1]['value'];
	$restored['dimension2'] = (int)$snapshot['header']['axes'][2]['value'];
	$restored['classification_intents'] = $header_intents['intents'];
	$line_entries = $snapshot['lines'];
	if (empty($line_entries))
		return array('ok' => true, 'data' => $restored, 'errors' => array(), 'codes' => array());
	$gl_items = isset($restored['gl_items']) && is_array($restored['gl_items']) ? $restored['gl_items'] : null;
	$line_items = isset($restored['line_items']) && is_array($restored['line_items']) ? $restored['line_items'] : null;
	if ($gl_items !== null && $line_items !== null && count($gl_items) > 0 && count($line_items) > 0) {
		$result['codes'][] = 'ambiguous_carrier';
		$result['errors'][] = _('The draft payload declares two line carriers; the sealed classification cannot be restored unambiguously.');
		return $result;
	}
	$carrier = $gl_items !== null && count($gl_items) > 0 ? 'gl_items'
		: ($line_items !== null && count($line_items) > 0 ? 'line_items' : '');
	if ($carrier === '') {
		$result['codes'][] = 'missing_carrier';
		$result['errors'][] = _('The draft payload carries sealed line classifications but no line carrier to restore them onto.');
		return $result;
	}
	$mapped = array();
	foreach ($restored[$carrier] as $index => $item) {
		if (!is_array($item))
			continue;
		if (isset($item['draft_line_key'])) {
			$key = (string)$item['draft_line_key'];
			foreach ($line_entries as $entry_index => $entry)
				if ($entry['line_key'] === $key) {
					$mapped[$entry_index] = $index;
					break;
				}
			continue;
		}
		$mapped[$index] = $index;
	}
	$complete = count($mapped) === count($line_entries);
	if ($complete) {
		foreach ($line_entries as $entry_index => $entry)
			if (!isset($mapped[$entry_index])) {
				$complete = false;
				break;
			}
	}
	if (!$complete || count($restored[$carrier]) !== count($line_entries)) {
		$result['codes'][] = 'line_mapping_failed';
		$result['errors'][] = sprintf(_('The sealed classification names %d line(s) but the draft payload carries %d line(s) that cannot be mapped exactly.'),
			count($line_entries), count($restored[$carrier]));
		return $result;
	}
	foreach ($line_entries as $entry_index => $entry) {
		$carrier_index = $mapped[$entry_index];
		$intents = approval_dimension_snapshot_entry_intents($entry['axes']);
		if (!$intents['ok']) {
			$result['codes'] = $intents['codes'];
			$result['errors'] = $intents['errors'];
			return $result;
		}
		$restored[$carrier][$carrier_index]['dimension_id'] = (int)$entry['axes'][1]['value'];
		$restored[$carrier][$carrier_index]['dimension2_id'] = (int)$entry['axes'][2]['value'];
		$restored[$carrier][$carrier_index]['classification_intents'] = $intents['intents'];
	}
	return array('ok' => true, 'data' => $restored, 'errors' => array(), 'codes' => array());
}

/**
 * Visible compatibility / verification notice of one classification state.
 *
 * @param string $state legacy, versioned or another verified state
 * @return string
 */
function approval_dimension_snapshot_notice($state)
{
	if ($state === 'legacy')
		return _('Legacy compatibility: this draft carries no versioned classification snapshot. The source executor keeps its existing behavior and no classification is recalculated.');
	if ($state === 'versioned' || $state === 'valid')
		return _('Classification snapshot version 2 was verified against its exact durable source revisions.');
	return '';
}

/**
 * Actionable refusal text for one failed snapshot decision.
 *
 * @param array $result Failed preflight/execution result
 * @param string $phase decision or execution
 * @return string
 */
function approval_dimension_snapshot_refusal_text($result, $phase = 'decision')
{
	$reason = '';
	if (is_array($result) && !empty($result['errors']))
		$reason = (string)$result['errors'][0];
	elseif (is_array($result) && !empty($result['codes']))
		$reason = (string)$result['codes'][0];
	if ($reason === '')
		$reason = _('the classification snapshot does not verify against its durable source revisions');
	$suffix = $phase === 'execution'
		? _(' No transaction was posted and the draft stays pending; correct the source or reject/cancel the draft under its original authority.')
		: _(' No approval decision was recorded.');
	return sprintf(_('The classification snapshot cannot proceed: %s.%s'), rtrim($reason, '.'), $suffix);
}

/**
 * Immutable audit receipt text of one verified execution.
 *
 * @param array $validation Successful revalidation result
 * @param int $posted_trans_no Posted transaction number
 * @return string
 */
function approval_dimension_snapshot_receipt($validation, $posted_trans_no)
{
	$count = is_array($validation) && isset($validation['entries'])
		? count($validation['entries']) : 0;
	$seal = is_array($validation) && isset($validation['snapshot']['snapshot_sha256'])
		? substr($validation['snapshot']['snapshot_sha256'], 0, 16) : '';
	return sprintf(_('Classification snapshot v%d verified and restored before execution; %d sealed revision(s); draft seal %s; posted trans #%d.'),
		approval_dimension_snapshot_schema_version(), $count, $seal, (int)$posted_trans_no);
}

/**
 * Build the review projection of one draft without changing it.
 *
 * @param array $draft Approval draft row
 * @param mixed $data Optional decoded payload; decoded through the workflow decoder when omitted
 * @return array state (not_governed|legacy|valid|stale|invalid|tamper|unavailable), snapshot,
 *               validation, errors and codes
 */
function approval_dimension_snapshot_review($draft, $data = null)
{
	$result = array('state' => 'not_governed', 'snapshot' => null, 'validation' => null,
		'errors' => array(), 'codes' => array());
	$trans_type = isset($draft['trans_type']) ? (int)$draft['trans_type'] : 0;
	if (!approval_dimension_snapshot_governed($trans_type))
		return $result;
	if ($data === null && isset($draft['draft_data'])) {
		try {
			if (function_exists('get_approval_workflow_service'))
				$data = get_approval_workflow_service()->decodeStoredDraftData($draft['draft_data'], $ok);
			if ($data === null) {
				$data = json_decode((string)$draft['draft_data'], true);
				if ($data === null)
					$data = json_decode(stripslashes((string)$draft['draft_data']), true);
			}
		} catch (Exception $exception) {
			$data = null;
		} catch (Throwable $exception) {
			$data = null;
		}
	}
	$read = approval_dimension_snapshot_read($data);
	if ($read['state'] === 'absent') {
		$result['state'] = 'legacy';
		$result['codes'][] = 'legacy_payload';
		return $result;
	}
	$result['snapshot'] = $read['snapshot'];
	if ($read['state'] === 'tamper') {
		$result['state'] = 'tamper';
		$result['errors'] = $read['errors'];
		$result['codes'] = $read['codes'];
		return $result;
	}
	try {
		$validation = approval_dimension_snapshot_revalidate($read['snapshot']);
	} catch (Exception $exception) {
		$result['state'] = 'unavailable';
		$result['errors'][] = _('The classification snapshot could not be revalidated on this request.');
		return $result;
	} catch (Throwable $exception) {
		$result['state'] = 'unavailable';
		$result['errors'][] = _('The classification snapshot could not be revalidated on this request.');
		return $result;
	}
	$result['validation'] = $validation;
	$result['errors'] = $validation['errors'];
	$result['codes'] = $validation['codes'];
	$result['state'] = $validation['ok'] ? 'valid' : $validation['state'];
	return $result;
}

/**
 * Human label of one review state.
 *
 * @param string $state Review state
 * @return string
 */
function approval_dimension_snapshot_review_label($state)
{
	$labels = array(
		'legacy' => _('Legacy payload - no classification snapshot'),
		'valid' => _('Version 2 verified'),
		'stale' => _('Source revision superseded'),
		'invalid' => _('Classification no longer valid'),
		'tamper' => _('Snapshot failed verification'),
		'unavailable' => _('Revision store unavailable'),
		'not_governed' => _('Not governed'));
	return isset($labels[$state]) ? $labels[$state] : _('Unknown state');
}

/**
 * Render the read-only classification snapshot review panel.
 *
 * @param array $review approval_dimension_snapshot_review() result
 * @return string HTML
 */
function approval_dimension_snapshot_render_review($review)
{
	$state = is_array($review) && isset($review['state']) ? $review['state'] : 'not_governed';
	if ($state === 'not_governed')
		return '';
	$colors = array('valid' => '#198754', 'legacy' => '#6c757d', 'stale' => '#fd7e14',
		'invalid' => '#dc3545', 'tamper' => '#dc3545', 'unavailable' => '#fd7e14');
	$color = isset($colors[$state]) ? $colors[$state] : '#6c757d';
	$html = "<div style='border-left:4px solid ".$color.";padding:8px 12px;margin:8px 0;background:#f8f9fa;'>\n";
	$html .= "<strong style='color:".$color.";'>"
		.htmlspecialchars(approval_dimension_snapshot_review_label($state), ENT_QUOTES, 'UTF-8')
		."</strong>\n";
	if ($state === 'legacy') {
		$html .= "<p style='margin:6px 0 0 0;'>"
			.htmlspecialchars(approval_dimension_snapshot_notice('legacy'), ENT_QUOTES, 'UTF-8')
			."</p>\n";
	}
	if ($state === 'valid')
		$html .= "<p style='margin:6px 0 0 0;'>"
			.htmlspecialchars(approval_dimension_snapshot_notice('versioned'), ENT_QUOTES, 'UTF-8')
			."</p>\n";
	if (!empty($review['errors'])) {
		$html .= "<ul style='margin:6px 0 0 18px;'>\n";
		foreach ($review['errors'] as $error)
			$html .= "<li>".htmlspecialchars((string)$error, ENT_QUOTES, 'UTF-8')."</li>\n";
		$html .= "</ul>\n";
	}
	$entries = is_array($review) && isset($review['validation']['entries'])
		? $review['validation']['entries'] : array();
	if (!empty($entries)) {
		$html .= "<table class='tablestyle' style='width:100%;margin-top:8px;'>\n";
		$html .= "<tr><th>"._('Entry')."</th><th>"._('Category 1')."</th><th>"._('Category 2')."</th>"
			."<th>"._('Revision')."</th><th>"._('State')."</th></tr>\n";
		foreach ($entries as $entry) {
			$axis1 = $entry['axes'][1];
			$axis2 = $entry['axes'][2];
			$html .= "<tr>"
				."<td>".htmlspecialchars((string)$entry['label'], ENT_QUOTES, 'UTF-8')."</td>"
				."<td>".htmlspecialchars(sprintf(_('#%d %s'), (int)$axis1['value'], $axis1['mode']), ENT_QUOTES, 'UTF-8')."</td>"
				."<td>".htmlspecialchars(sprintf(_('#%d %s'), (int)$axis2['value'], $axis2['mode']), ENT_QUOTES, 'UTF-8')."</td>"
				."<td>".htmlspecialchars(sprintf(_('#%d'), (int)$entry['revision_no']), ENT_QUOTES, 'UTF-8')."</td>"
				."<td>".htmlspecialchars($entry['classification_ok'] ? _('current') : _('refused'), ENT_QUOTES, 'UTF-8')."</td>"
				."</tr>\n";
		}
		$html .= "</table>\n";
	}
	$html .= "</div>\n";
	return $html;
}
