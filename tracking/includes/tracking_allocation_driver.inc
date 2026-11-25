<?php
/**
 * DIM-ALC-002: versioned measured drivers for overhead and labor allocation.
 *
 * A driver is an approved, effective-dated set of measured units per Dimension (floor area, machine hours, an
 * aggregate payroll headcount ...). It turns the fixed rule of DIM-ALC-001 into an explainable, financially
 * reconcilable one:
 *
 *  - a driver VERSION has a key, a version number, one Dimension category (axis), a unit label, an effective
 *    period, the measure source (`measured` or the minimized `payroll_aggregate`), the units of each Dimension
 *    and the total population measured. A maker proposes it, a DIFFERENT user approves or rejects it; approving a
 *    version supersedes the earlier version it fully covers (a partial overlap is refused);
 *  - a driver RUN reads the posted ledger rows of the ALC-001 source pool, converts the units to exact weights
 *    (parts per million, largest remainder, ties to the lowest Dimension) and posts the SAME balancing journal
 *    through the DIM-ALC-001 engine and the ordinary journal approval path, so amounts are conserved to the cent;
 *  - a zero or missing driver, a population that is not fully measured and a covered Dimension without a measure
 *    BLOCK the run unless the request names an explicit residual Dimension; a run is labelled "fully allocated"
 *    only when no residual and no gap remains;
 *  - a driver that is superseded, changed or out of period before the journal posts is STALE: the run is refused
 *    at dispatch and again inside the approval executor's posting transaction;
 *  - payroll measures are minimized aggregates only (headcount, FTE or hours of at least three persons per
 *    Dimension), tied to an approved payroll result-set digest, require the payroll evidence permission, and no
 *    person key, name or pay amount can be stored.
 *
 * No posted ledger row, budget, approval or HRM row is ever updated. PHP 5.6 compatible.
 */
require_once(__DIR__.'/../../includes/value_helpers.inc');

/** Largest measured unit count a driver may carry (keeps every weight product exact in 64-bit integers). */
function dimension_driver_max_units()
{
	return 1000000000;
}

/** Fewest persons a payroll aggregate cell may describe, so an aggregate never identifies one person. */
function dimension_driver_minimum_cohort()
{
	return 3;
}

/** Measure sources a driver may declare. */
function dimension_driver_source_kinds()
{
	return array('measured', 'payroll_aggregate');
}

/** Units a payroll aggregate driver may use (counts and hours only; never an amount). */
function dimension_driver_payroll_units()
{
	return array('headcount', 'fte', 'hours');
}

/** Residual policies of a driver run. */
function dimension_driver_residual_policies()
{
	return array('block', 'dimension');
}

/** Authorization check that denies when the session helper is unavailable. */
function dimension_driver_has_access($area)
{
	return function_exists('user_check_access') && user_check_access($area);
}

/**
 * Parse a non-negative integer (an integer or a canonical digit string).
 *
 * @param mixed $value Candidate
 * @param int $max Largest accepted value
 * @return int|false
 */
function dimension_driver_parse_uint($value, $max)
{
	if (is_string($value) && preg_match('/^(0|[1-9][0-9]{0,15})$/D', $value))
		$value = (int)$value;
	if (!is_int($value) || $value < 0 || $value > $max)
		return false;
	return $value;
}

/**
 * Normalize and validate one driver version proposal without touching the database.
 *
 * Only aggregate measures are accepted: any field outside the contract (an employee, person, name, salary or
 * pay field) is refused, so personal detail cannot enter a driver.
 *
 * @param array $spec driver_key, axis (1|2), source_kind, unit_label, period_from, period_to, optional
 *   population_units, aggregate_source_sha256 (payroll only), note and values (dimension_id, units, member_count)
 * @return array ok, errors, codes and the canonical `spec`
 */
function dimension_driver_normalize_spec($spec)
{
	$result = array('ok' => true, 'errors' => array(), 'codes' => array(), 'spec' => null);
	if (!is_array($spec))
		return dimension_allocation_fail($result, 'invalid_request', _('The driver proposal could not be read.'));
	$allowed = array('driver_key', 'axis', 'source_kind', 'unit_label', 'period_from', 'period_to', 'population_units',
		'aggregate_source_sha256', 'note', 'values');
	foreach (array_keys($spec) as $field)
		if (!in_array($field, $allowed, true))
			dimension_allocation_fail($result, 'privacy_field', sprintf(_('The field "%s" is not part of the driver contract; a driver carries aggregate measures only.'),
				preg_replace('/[^A-Za-z0-9_]/', '', (string)$field)));
	$key = isset($spec['driver_key']) && is_string($spec['driver_key']) ? $spec['driver_key'] : '';
	if (!preg_match('/^[a-z0-9][a-z0-9_-]{0,63}$/D', $key))
		dimension_allocation_fail($result, 'invalid_key', _('The driver key must be 1-64 lowercase letters, digits, hyphens or underscores.'));
	$axis = isset($spec['axis']) ? $spec['axis'] : 0;
	if (!in_array((string)$axis, array('1', '2'), true))
		dimension_allocation_fail($result, 'invalid_axis', _('Choose the tracking category the driver measures.'));
	$kind = isset($spec['source_kind']) ? $spec['source_kind'] : '';
	if (!in_array($kind, dimension_driver_source_kinds(), true))
		dimension_allocation_fail($result, 'invalid_source_kind', _('Choose measured data or a payroll aggregate as the driver source.'));
	$unit = isset($spec['unit_label']) && is_string($spec['unit_label']) ? trim($spec['unit_label']) : '';
	if ($unit === '' || strlen($unit) > 40 || !preg_match('/^[A-Za-z0-9][A-Za-z0-9 ._%\/-]*$/D', $unit))
		dimension_allocation_fail($result, 'invalid_unit', _('The unit label must be 1-40 plain characters (for example m2, hours or headcount).'));
	elseif ($kind === 'payroll_aggregate' && !in_array(strtolower($unit), dimension_driver_payroll_units(), true))
		dimension_allocation_fail($result, 'payroll_unit', _('A payroll aggregate driver may only use the units headcount, fte or hours.'));
	$from = isset($spec['period_from']) ? $spec['period_from'] : '';
	$to = isset($spec['period_to']) ? $spec['period_to'] : '';
	if (!is_iso_calendar_date($from) || !is_iso_calendar_date($to) || strcmp($from, $to) > 0)
		dimension_allocation_fail($result, 'invalid_period', _('The effective period needs a valid start date that is not after its end date.'));
	$note = isset($spec['note']) && is_string($spec['note']) ? trim($spec['note']) : '';
	if (strlen($note) > 200)
		dimension_allocation_fail($result, 'note_too_long', _('The note may not be longer than 200 characters.'));
	$source_sha = null;
	if ($kind === 'payroll_aggregate') {
		$source_sha = isset($spec['aggregate_source_sha256']) && is_string($spec['aggregate_source_sha256']) ? $spec['aggregate_source_sha256'] : '';
		if (!preg_match('/^[a-f0-9]{64}$/D', $source_sha))
			dimension_allocation_fail($result, 'payroll_source', _('A payroll aggregate driver must cite the approved payroll result-set digest it summarizes.'));
	} elseif (isset($spec['aggregate_source_sha256']) && $spec['aggregate_source_sha256'] !== '' && $spec['aggregate_source_sha256'] !== null)
		dimension_allocation_fail($result, 'payroll_source', _('Only a payroll aggregate driver may cite a payroll result-set digest.'));

	$values = array();
	$sum = 0;
	$raw_values = isset($spec['values']) && is_array($spec['values']) ? $spec['values'] : array();
	foreach ($raw_values as $row) {
		if (!is_array($row)) {
			dimension_allocation_fail($result, 'invalid_value', _('A driver value could not be read.'));
			continue;
		}
		foreach (array_keys($row) as $field)
			if (!in_array($field, array('dimension_id', 'units', 'member_count'), true)) {
				dimension_allocation_fail($result, 'privacy_field', sprintf(_('The field "%s" is not part of the driver contract; a driver carries aggregate measures only.'),
					preg_replace('/[^A-Za-z0-9_]/', '', (string)$field)));
				continue 2;
			}
		$dimension = dimension_driver_parse_uint(isset($row['dimension_id']) ? $row['dimension_id'] : null, 2147483647);
		if ($dimension === false || $dimension <= 0) {
			dimension_allocation_fail($result, 'invalid_value', _('Each driver value must name an existing tracking value.'));
			continue;
		}
		if (isset($values[$dimension])) {
			dimension_allocation_fail($result, 'duplicate_value', sprintf(_('Tracking value #%d is listed more than once.'), $dimension));
			continue;
		}
		$units = dimension_driver_parse_uint(isset($row['units']) ? $row['units'] : null, dimension_driver_max_units());
		if ($units === false) {
			dimension_allocation_fail($result, 'invalid_units', sprintf(_('The units of tracking value #%d must be a whole number from 0 to %d.'), $dimension, dimension_driver_max_units()));
			continue;
		}
		$members = null;
		if ($kind === 'payroll_aggregate') {
			$members = dimension_driver_parse_uint(isset($row['member_count']) ? $row['member_count'] : null, 100000000);
			if ($members === false || ($units > 0 && $members < dimension_driver_minimum_cohort())) {
				dimension_allocation_fail($result, 'cohort_too_small', sprintf(_('The payroll aggregate of tracking value #%d must describe at least %d persons so no person can be identified.'),
					$dimension, dimension_driver_minimum_cohort()));
				continue;
			}
		} elseif (isset($row['member_count']) && $row['member_count'] !== null && $row['member_count'] !== '') {
			dimension_allocation_fail($result, 'privacy_field', _('A member count belongs only to a payroll aggregate driver.'));
			continue;
		}
		$values[$dimension] = array('dimension_id' => $dimension, 'units' => $units, 'member_count' => $members);
		$sum += $units;
	}
	if (!count($values))
		dimension_allocation_fail($result, 'no_values', _('Add the measured units of at least one tracking value.'));
	elseif (count($values) > 200)
		dimension_allocation_fail($result, 'too_many_values', _('A driver may measure at most 200 tracking values.'));
	$population = $sum;
	if (isset($spec['population_units']) && $spec['population_units'] !== '' && $spec['population_units'] !== null) {
		$population = dimension_driver_parse_uint($spec['population_units'], dimension_driver_max_units());
		if ($population === false || $population < $sum)
			dimension_allocation_fail($result, 'invalid_population', _('The measured population must be a whole number that is not less than the listed units.'));
	}
	if (!$result['ok'])
		return $result;
	ksort($values);
	$result['spec'] = array('driver_key' => $key, 'axis' => (int)$axis, 'source_kind' => $kind, 'unit_label' => $unit,
		'period_from' => $from, 'period_to' => $to, 'population_units' => (int)$population, 'units_total' => (int)$sum,
		'aggregate_source_sha256' => $source_sha, 'note' => $note, 'values' => array_values($values));
	return $result;
}

/**
 * Content digest of a driver version: everything that decides an allocation, nothing volatile.
 *
 * @param array $spec Normalized spec or a loaded driver (same field names)
 * @return string 64 hex characters
 */
function dimension_driver_content_sha256($spec)
{
	$values = array();
	foreach ($spec['values'] as $row)
		$values[] = array((int)$row['dimension_id'], (int)$row['units'], $row['member_count'] === null ? null : (int)$row['member_count']);
	return hash('sha256', json_encode(array('alc2-driver', (string)$spec['driver_key'], (int)$spec['axis'], (string)$spec['source_kind'],
		(string)$spec['unit_label'], (string)$spec['period_from'], (string)$spec['period_to'], (int)$spec['population_units'],
		$spec['aggregate_source_sha256'] === null ? null : (string)$spec['aggregate_source_sha256'], $values)));
}

/**
 * Are the three driver tables installed in the active company?
 *
 * @return bool|string True, 'absent' or false when it cannot be answered
 */
function dimension_driver_schema_ready()
{
	$rows = db_query("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('"
		.TB_PREF."dimension_allocation_drivers','".TB_PREF."dimension_allocation_driver_values','".TB_PREF."dimension_allocation_driver_runs')");
	$row = $rows ? db_fetch_row($rows) : false;
	if (!$row)
		return false;
	return (int)$row[0] === 3 ? true : 'absent';
}

/**
 * Check that every named Dimension exists and belongs to the driver's category.
 *
 * @param int[] $ids Dimension identities
 * @param int $axis Category (1|2)
 * @return array ok, errors, codes
 */
function dimension_driver_check_dimensions($ids, $axis)
{
	$result = array('ok' => true, 'errors' => array(), 'codes' => array());
	if (!count($ids))
		return $result;
	$list = array();
	foreach ($ids as $id)
		$list[] = (int)$id;
	$rows = db_query('SELECT id, type_ FROM '.TB_PREF.'dimensions WHERE id IN ('.implode(',', $list).')');
	if ($rows === false)
		return dimension_allocation_fail($result, 'unavailable', _('The tracking values could not be read.'));
	$found = array();
	while ($row = db_fetch_assoc($rows))
		$found[(int)$row['id']] = (int)$row['type_'];
	foreach ($list as $id) {
		if (!isset($found[$id]))
			dimension_allocation_fail($result, 'dimension_missing', sprintf(_('Tracking value #%d does not exist.'), $id));
		elseif ($found[$id] !== (int)$axis)
			dimension_allocation_fail($result, 'dimension_axis', sprintf(_('Tracking value #%d does not belong to the chosen tracking category.'), $id));
	}
	return $result;
}

/**
 * Is the payroll result-set digest an approved PAY-CORE approval of this company?
 *
 * @param string $sha 64-hex digest
 * @return bool
 */
function dimension_driver_payroll_source_verified($sha)
{
	if (!is_string($sha) || !preg_match('/^[a-f0-9]{64}$/D', $sha))
		return false;
	$rows = db_query('SELECT approval_id FROM '.TB_PREF.'hrm_pay_core_010_approvals WHERE result_set_sha256='.db_escape($sha)
		." AND LOWER(decision_token)='approved' LIMIT 1");
	return $rows && db_num_rows($rows) === 1;
}

/**
 * Load one driver version with its measured units.
 *
 * @param int $driver_id Version identity
 * @param bool $lock Lock the rows for the owning transaction
 * @return array|false The driver row with `values` (list), or false
 */
function dimension_driver_load($driver_id, $lock = false)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_allocation_drivers WHERE driver_id='.(int)$driver_id.($lock ? ' FOR UPDATE' : ''));
	$driver = $rows ? db_fetch_assoc($rows) : false;
	if (!$driver)
		return false;
	$rows = db_query('SELECT target_id, units, member_count FROM '.TB_PREF.'dimension_allocation_driver_values WHERE driver_id='
		.(int)$driver_id.' ORDER BY target_id'.($lock ? ' FOR UPDATE' : ''));
	if ($rows === false)
		return false;
	$driver['values'] = array();
	while ($row = db_fetch_assoc($rows))
		$driver['values'][] = array('dimension_id' => (int)$row['target_id'], 'units' => (int)$row['units'],
			'member_count' => $row['member_count'] === null ? null : (int)$row['member_count']);
	return $driver;
}

/** Does the stored content of a loaded driver still match its digest and totals? */
function dimension_driver_intact($driver)
{
	if (!is_array($driver) || !isset($driver['values']))
		return false;
	$sum = 0;
	foreach ($driver['values'] as $row)
		$sum += (int)$row['units'];
	if ($sum !== (int)$driver['units_total'] || (int)$driver['population_units'] < $sum)
		return false;
	return hash_equals((string)$driver['content_sha256'], dimension_driver_content_sha256($driver));
}

/**
 * Propose a driver version (the maker step).
 *
 * @param array $spec See dimension_driver_normalize_spec
 * @return array ok, duplicate, driver_id, version_no, errors, codes
 */
function dimension_driver_submit($spec)
{
	$result = array('ok' => false, 'duplicate' => false, 'driver_id' => 0, 'version_no' => 0, 'errors' => array(), 'codes' => array());
	if (!dimension_driver_has_access('SA_DIMENSION'))
		return dimension_allocation_fail($result, 'permission_denied', _('You are not allowed to propose allocation drivers.'));
	$normal = dimension_driver_normalize_spec($spec);
	if (!$normal['ok'])
		return array_merge($result, array('errors' => $normal['errors'], 'codes' => $normal['codes']));
	$spec = $normal['spec'];
	if ($spec['source_kind'] === 'payroll_aggregate' && !dimension_driver_has_access('SA_HRM_VIEW_PAY_GL'))
		return dimension_allocation_fail($result, 'permission_denied', _('Payroll aggregates need the payroll evidence permission.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_allocation_fail($result, 'permission_denied', _('The proposing user could not be identified.'));
	$ids = array();
	foreach ($spec['values'] as $row)
		$ids[] = $row['dimension_id'];
	$dimensions = dimension_driver_check_dimensions($ids, $spec['axis']);
	if (!$dimensions['ok'])
		return array_merge($result, array('errors' => $dimensions['errors'], 'codes' => $dimensions['codes']));
	if ($spec['source_kind'] === 'payroll_aggregate' && !dimension_driver_payroll_source_verified($spec['aggregate_source_sha256']))
		return dimension_allocation_fail($result, 'payroll_source_unverified', _('The cited payroll result-set digest is not an approved payroll approval of this company.'));
	$sha = dimension_driver_content_sha256($spec);
	if (!begin_transaction())
		return dimension_allocation_fail($result, 'unavailable', _('The driver could not start a transaction.'));
	$rows = db_query('SELECT driver_id, version_no, status, content_sha256 FROM '.TB_PREF.'dimension_allocation_drivers WHERE driver_key='
		.db_escape($spec['driver_key']).' ORDER BY version_no FOR UPDATE');
	if ($rows === false) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'unavailable', _('The driver versions could not be read.'));
	}
	$next = 1;
	while ($row = db_fetch_assoc($rows)) {
		$next = max($next, (int)$row['version_no'] + 1);
		if ($row['content_sha256'] === $sha && in_array($row['status'], array('pending', 'approved'), true)) {
			cancel_transaction();
			$result['ok'] = true;
			$result['duplicate'] = true;
			$result['driver_id'] = (int)$row['driver_id'];
			$result['version_no'] = (int)$row['version_no'];
			return $result;
		}
	}
	$inserted = db_query('INSERT INTO '.TB_PREF.'dimension_allocation_drivers (driver_key, version_no, axis, source_kind, unit_label,'
		.' period_from, period_to, population_units, units_total, aggregate_source_sha256, status, content_sha256, note, maker_id, created_at)'
		.' VALUES ('.db_escape($spec['driver_key']).','.$next.','.(int)$spec['axis'].','.db_escape($spec['source_kind']).','
		.db_escape($spec['unit_label']).','.db_escape($spec['period_from']).','.db_escape($spec['period_to']).','
		.(int)$spec['population_units'].','.(int)$spec['units_total'].','
		.($spec['aggregate_source_sha256'] === null ? 'NULL' : db_escape($spec['aggregate_source_sha256'])).",'pending',"
		.db_escape($sha).','.dimension_allocation_quote($spec['note']).','.$actor.',NOW())');
	$driver_id = $inserted ? (int)db_insert_id() : 0;
	$stored = $driver_id > 0;
	if ($stored)
		foreach ($spec['values'] as $row)
			if (db_query('INSERT INTO '.TB_PREF.'dimension_allocation_driver_values (driver_id, target_id, units, member_count) VALUES ('
				.$driver_id.','.(int)$row['dimension_id'].','.(int)$row['units'].','.($row['member_count'] === null ? 'NULL' : (int)$row['member_count']).')') === false)
				$stored = false;
	if (!$stored || !commit_transaction()) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'unavailable', _('The driver could not be recorded.'));
	}
	$result['ok'] = true;
	$result['driver_id'] = $driver_id;
	$result['version_no'] = $next;
	return $result;
}

/**
 * Approve or reject a proposed driver version (the independent checker step).
 *
 * Approval supersedes every approved version of the same key whose period the new version fully covers; a
 * partial overlap is refused so no effective day is ever left without its measure.
 *
 * @param int $driver_id Proposed version
 * @param string $decision approve|reject
 * @return array ok, status, errors, codes
 */
function dimension_driver_decide($driver_id, $decision)
{
	$result = array('ok' => false, 'status' => '', 'errors' => array(), 'codes' => array());
	if (!dimension_driver_has_access('SA_DIMENSION'))
		return dimension_allocation_fail($result, 'permission_denied', _('You are not allowed to decide allocation drivers.'));
	if (!in_array($decision, array('approve', 'reject'), true))
		return dimension_allocation_fail($result, 'invalid_decision', _('The decision must be approve or reject.'));
	$actor = current_user_id_or_zero();
	if ($actor <= 0)
		return dimension_allocation_fail($result, 'permission_denied', _('The deciding user could not be identified.'));
	if (!begin_transaction())
		return dimension_allocation_fail($result, 'unavailable', _('The decision could not start a transaction.'));
	$driver = dimension_driver_load($driver_id, true);
	if (!$driver || $driver['status'] !== 'pending') {
		cancel_transaction();
		return dimension_allocation_fail($result, 'not_pending', _('Only a proposed driver version can be decided.'));
	}
	if ($driver['source_kind'] === 'payroll_aggregate' && !dimension_driver_has_access('SA_HRM_VIEW_PAY_GL')) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'permission_denied', _('Payroll aggregates need the payroll evidence permission.'));
	}
	if ((int)$driver['maker_id'] === $actor) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'maker_checker', _('The proposing user cannot decide the same driver version; ask an independent checker.'));
	}
	$status = $decision === 'approve' ? 'approved' : 'rejected';
	if ($decision === 'approve') {
		if (!dimension_driver_intact($driver)) {
			cancel_transaction();
			return dimension_allocation_fail($result, 'driver_tampered', _('The stored driver no longer matches its digest and cannot be approved.'));
		}
		if ($driver['source_kind'] === 'payroll_aggregate' && !dimension_driver_payroll_source_verified($driver['aggregate_source_sha256'])) {
			cancel_transaction();
			return dimension_allocation_fail($result, 'payroll_source_unverified', _('The cited payroll result-set digest is not an approved payroll approval of this company.'));
		}
		$rows = db_query('SELECT driver_id, period_from, period_to FROM '.TB_PREF.'dimension_allocation_drivers WHERE driver_key='
			.db_escape($driver['driver_key']).' AND status='."'approved'".' AND period_from<='.db_escape($driver['period_to'])
			.' AND period_to>='.db_escape($driver['period_from']).' FOR UPDATE');
		if ($rows === false) {
			cancel_transaction();
			return dimension_allocation_fail($result, 'unavailable', _('The earlier driver versions could not be read.'));
		}
		$superseded = array();
		while ($row = db_fetch_assoc($rows)) {
			if (strcmp($driver['period_from'], $row['period_from']) > 0 || strcmp($driver['period_to'], $row['period_to']) < 0) {
				cancel_transaction();
				return dimension_allocation_fail($result, 'driver_overlap', sprintf(_('The effective period overlaps the approved version #%d only in part; end the earlier version or choose a period that covers it.'),
					(int)$row['driver_id']));
			}
			$superseded[] = (int)$row['driver_id'];
		}
		foreach ($superseded as $old)
			if (db_query('UPDATE '.TB_PREF."dimension_allocation_drivers SET status='superseded', superseded_by=".(int)$driver_id.' WHERE driver_id='.$old." AND status='approved'") === false) {
				cancel_transaction();
				return dimension_allocation_fail($result, 'unavailable', _('The earlier version could not be superseded.'));
			}
	}
	if (db_query('UPDATE '.TB_PREF.'dimension_allocation_drivers SET status='.db_escape($status).', checker_id='.$actor
		.', decided_at=NOW() WHERE driver_id='.(int)$driver_id." AND status='pending'") === false || !commit_transaction()) {
		cancel_transaction();
		return dimension_allocation_fail($result, 'unavailable', _('The decision could not be recorded.'));
	}
	$result['ok'] = true;
	$result['status'] = $status;
	return $result;
}

/**
 * Exact weights in parts per million with largest-remainder rounding (ties to the lowest Dimension).
 *
 * @param array $units Dimension => units (positive), in ascending Dimension order
 * @param int $population Positive total the units are measured against (not less than their sum)
 * @return array Dimension => parts per million, totalling exactly 1000000 when the units sum to the population
 */
function dimension_driver_ppm($units, $population)
{
	$population = (int)$population;
	$ppm = array();
	$remainder = array();
	$given = 0;
	foreach ($units as $dimension => $count) {
		$product = (int)$count * 1000000;
		$whole = (int)(($product - ($product % $population)) / $population);
		$ppm[$dimension] = $whole;
		$remainder[$dimension] = $product - $whole * $population;
		$given += $whole;
	}
	$ranked = array_keys($remainder);
	usort($ranked, function ($left, $right) use ($remainder) {
		return $remainder[$left] === $remainder[$right] ? $left - $right : ($remainder[$left] > $remainder[$right] ? -1 : 1);
	});
	$left = 1000000 - $given;
	// The population may exceed the measured units (an unmeasured remainder): only the measured share is handed out.
	$limit = count($ranked);
	for ($index = 0; $index < $left && $index < $limit; $index++)
		$ppm[$ranked[$index]]++;
	return $ppm;
}

/**
 * Turn a driver and a residual policy into exact allocation weights.
 *
 * @param array $driver Loaded driver
 * @param string $policy block|dimension
 * @param int $residual_dimension Explicit residual Dimension (policy dimension)
 * @param int[] $cover Dimensions that must carry a measure
 * @return array ok, errors, codes, targets (dimension_id, units, ppm, role), population, units_total,
 *   residual_units, residual_ppm, residual_dimension_id, gaps, fully_allocated
 */
function dimension_driver_weights($driver, $policy, $residual_dimension, $cover)
{
	$result = array('ok' => true, 'errors' => array(), 'codes' => array(), 'targets' => array(), 'population' => (int)$driver['population_units'],
		'units_total' => (int)$driver['units_total'], 'residual_units' => 0, 'residual_ppm' => 0, 'residual_dimension_id' => null,
		'gaps' => array(), 'fully_allocated' => false);
	if (!in_array($policy, dimension_driver_residual_policies(), true))
		return dimension_allocation_fail($result, 'invalid_policy', _('Choose to block the run or to name an explicit residual tracking value.'));
	$units = array();
	$known = array();
	foreach ($driver['values'] as $row) {
		$known[(int)$row['dimension_id']] = true;
		if ((int)$row['units'] > 0)
			$units[(int)$row['dimension_id']] = (int)$row['units'];
	}
	ksort($units);
	foreach ($cover as $dimension)
		if (!isset($units[(int)$dimension]))
			$result['gaps'][] = (int)$dimension;
	sort($result['gaps']);
	$sum = array_sum($units);
	$population = max((int)$driver['population_units'], $sum);
	$unexplained = $population - $sum;
	if (!$sum) {
		if ($policy === 'block')
			return dimension_allocation_fail($result, 'driver_zero', _('The driver measures nothing for this period; nothing can be allocated. Name an explicit residual tracking value or correct the driver.'));
		$unexplained = max($population, 1);
	}
	if (count($result['gaps']) && $policy === 'block')
		return dimension_allocation_fail($result, 'driver_gap', sprintf(_('The driver has no measure for tracking value #%s; add it, remove it from the coverage or name an explicit residual tracking value.'),
			implode(', #', $result['gaps'])));
	if ($unexplained > 0) {
		if ($policy === 'block')
			return dimension_allocation_fail($result, 'driver_incomplete', sprintf(_('The driver measures %d of %d units; the unmeasured remainder needs an explicit residual tracking value.'), $sum, $population));
		$residual_dimension = (int)$residual_dimension;
		if ($residual_dimension <= 0)
			return dimension_allocation_fail($result, 'residual_missing', _('Name the tracking value that receives the unmeasured residual.'));
		if (isset($known[$residual_dimension]))
			return dimension_allocation_fail($result, 'residual_overlap', _('The residual tracking value must not be one of the measured tracking values.'));
		$check = dimension_driver_check_dimensions(array($residual_dimension), (int)$driver['axis']);
		if (!$check['ok']) {
			$result['ok'] = false;
			$result['errors'] = $check['errors'];
			$result['codes'] = $check['codes'];
			return $result;
		}
		$result['residual_units'] = $unexplained;
		$result['residual_dimension_id'] = $residual_dimension;
		$units[$residual_dimension] = $unexplained;
		ksort($units);
		$population = $sum ? $population : $unexplained;
	}
	$ppm = dimension_driver_ppm($units, $population);
	$total = 0;
	foreach ($units as $dimension => $count) {
		if ($ppm[$dimension] <= 0)
			return dimension_allocation_fail($result, 'driver_resolution', sprintf(_('The share of tracking value #%d is below the 0.0001%% resolution of an allocation; correct the driver.'), $dimension));
		$role = $dimension === $result['residual_dimension_id'] ? 'residual' : 'driver';
		$result['targets'][] = array('dimension_id' => (int)$dimension, 'units' => (int)$count, 'ppm' => (int)$ppm[$dimension], 'role' => $role);
		$total += $ppm[$dimension];
		if ($role === 'residual')
			$result['residual_ppm'] = (int)$ppm[$dimension];
	}
	if ($total !== 1000000)
		return dimension_allocation_fail($result, 'driver_weights', _('The driver weights do not total 100%; nothing was prepared.'));
	$result['fully_allocated'] = $result['residual_units'] === 0 && !count($result['gaps']);
	return $result;
}

/** Plain label of the completeness of a weight set: "fully allocated" only when nothing is left over. */
function dimension_driver_completeness_label($weights)
{
	if (!empty($weights['fully_allocated']))
		return _('Fully allocated');
	return _('Partly allocated: an explicit residual or an unmeasured tracking value remains');
}

/**
 * Normalize and validate one driver run request without touching the database.
 *
 * @param array $request driver_id, sources, date (Y-m-d), memo, residual_policy, residual_dimension_id, cover
 * @return array ok, errors, codes, run (canonical)
 */
function dimension_driver_normalize_run($request)
{
	$result = array('ok' => true, 'errors' => array(), 'codes' => array(), 'run' => null);
	if (!is_array($request))
		return dimension_allocation_fail($result, 'invalid_request', _('The allocation request could not be read.'));
	$driver_id = dimension_driver_parse_uint(isset($request['driver_id']) ? $request['driver_id'] : null, 2147483647);
	if (!$driver_id)
		dimension_allocation_fail($result, 'invalid_driver', _('Choose an approved driver version.'));
	$policy = isset($request['residual_policy']) ? $request['residual_policy'] : 'block';
	if (!in_array($policy, dimension_driver_residual_policies(), true))
		dimension_allocation_fail($result, 'invalid_policy', _('Choose to block the run or to name an explicit residual tracking value.'));
	$residual = 0;
	if ($policy === 'dimension') {
		$residual = dimension_driver_parse_uint(isset($request['residual_dimension_id']) ? $request['residual_dimension_id'] : null, 2147483647);
		if (!$residual)
			$residual = 0;
	}
	$cover = array();
	$raw_cover = isset($request['cover']) && is_array($request['cover']) ? $request['cover'] : array();
	foreach ($raw_cover as $dimension) {
		$id = dimension_driver_parse_uint($dimension, 2147483647);
		if (!$id) {
			dimension_allocation_fail($result, 'invalid_cover', _('A required tracking value is invalid.'));
			continue;
		}
		$cover[$id] = $id;
	}
	$cover = array_values($cover);
	sort($cover);
	// The shared engine validates the date, memo and source pool exactly as for a fixed rule.
	$shared = dimension_allocation_normalize_request(array('kind' => 'allocation', 'axis' => 1, 'mode' => 'percent',
		'date' => isset($request['date']) ? $request['date'] : '', 'memo' => isset($request['memo']) ? $request['memo'] : '',
		'sources' => isset($request['sources']) ? $request['sources'] : array(),
		'targets' => array(array('dimension_id' => 1, 'value' => 50), array('dimension_id' => 2, 'value' => 50))));
	// The shared engine validated the date, memo and source pool; the placeholder targets cannot fail.
	if (!$shared['ok'])
		foreach ($shared['errors'] as $index => $text)
			dimension_allocation_fail($result, isset($shared['codes'][$index]) ? $shared['codes'][$index] : 'invalid_request', $text);
	if (!$result['ok'])
		return $result;
	$result['run'] = array('driver_id' => (int)$driver_id, 'sources' => $shared['request']['sources'], 'date' => $shared['request']['date'],
		'memo' => $shared['request']['memo'], 'residual_policy' => $policy, 'residual_dimension_id' => (int)$residual, 'cover' => $cover);
	return $result;
}

/**
 * Is a driver usable for a run on a date? (approved, intact, effective on the date)
 *
 * @param array|false $driver Loaded driver
 * @param string $date Journal date (Y-m-d)
 * @return array ok, code
 */
function dimension_driver_usable($driver, $date)
{
	if (!$driver)
		return array('ok' => false, 'code' => 'driver_missing', 'text' => _('The driver version does not exist.'));
	if ($driver['status'] !== 'approved')
		return array('ok' => false, 'code' => 'driver_stale', 'text' => sprintf(_('Driver version #%d is %s; choose the current approved version.'),
			(int)$driver['driver_id'], $driver['status']));
	if (!dimension_driver_intact($driver))
		return array('ok' => false, 'code' => 'driver_tampered', 'text' => _('The stored driver no longer matches its digest.'));
	if (strcmp($date, $driver['period_from']) < 0 || strcmp($date, $driver['period_to']) > 0)
		return array('ok' => false, 'code' => 'driver_period', 'text' => _('The journal date is outside the effective period of the driver.'));
	return array('ok' => true, 'code' => '', 'text' => '');
}

/**
 * Prepare the shared allocation request, preview data and weights of a driver run (no write).
 *
 * @param array $run Canonical driver run
 * @return array ok, errors, codes, driver, weights, request (normalized shared ALC-001 request), raw_request
 */
function dimension_driver_prepare($run)
{
	$result = array('ok' => false, 'errors' => array(), 'codes' => array(), 'driver' => null, 'weights' => null, 'request' => null);
	$driver = dimension_driver_load($run['driver_id']);
	$usable = dimension_driver_usable($driver, $run['date']);
	if (!$usable['ok'])
		return dimension_allocation_fail($result, $usable['code'], $usable['text']);
	$weights = dimension_driver_weights($driver, $run['residual_policy'], $run['residual_dimension_id'], $run['cover']);
	if (!$weights['ok'])
		return array_merge($result, array('errors' => $weights['errors'], 'codes' => $weights['codes']));
	$targets = array();
	foreach ($weights['targets'] as $target)
		$targets[] = array('dimension_id' => $target['dimension_id'], 'value' => number_format($target['ppm'] / 10000, 4, '.', ''));
	$request = array('kind' => count($targets) === 1 ? 'reclassification' : 'allocation', 'axis' => (int)$driver['axis'], 'mode' => 'percent',
		'date' => $run['date'], 'memo' => $run['memo'], 'sources' => $run['sources'], 'targets' => $targets);
	$normal = dimension_allocation_normalize_request($request);
	if (!$normal['ok'])
		return array_merge($result, array('errors' => $normal['errors'], 'codes' => $normal['codes']));
	$result['ok'] = true;
	$result['driver'] = $driver;
	$result['weights'] = $weights;
	$result['request'] = $normal['request'];
	$result['raw_request'] = $request;
	return $result;
}

/**
 * Preview one driver run: weights plus the exact ALC-001 journal preview (nothing is written).
 *
 * @param array $request Raw driver run request
 * @return array ok, errors, codes, weights, driver, preview
 */
function dimension_driver_preview($request)
{
	$result = array('ok' => false, 'errors' => array(), 'codes' => array(), 'weights' => null, 'driver' => null, 'preview' => null);
	if (!dimension_driver_has_access('SA_JOURNALENTRY'))
		return dimension_allocation_fail($result, 'permission_denied', _('You are not allowed to preview allocation runs.'));
	$normal = dimension_driver_normalize_run($request);
	if (!$normal['ok'])
		return array_merge($result, array('errors' => $normal['errors'], 'codes' => $normal['codes']));
	$prepared = dimension_driver_prepare($normal['run']);
	if (!$prepared['ok'])
		return array_merge($result, array('errors' => $prepared['errors'], 'codes' => $prepared['codes']));
	$rows = dimension_allocation_load_sources($prepared['request']['sources']);
	$reserved = $rows === false ? false : dimension_allocation_reservations($prepared['request']['sources']);
	if ($rows === false || $reserved === false)
		return dimension_allocation_fail($result, 'unavailable', _('The source ledger rows could not be read.'));
	$check = dimension_allocation_check_sources($prepared['request'], $rows, $reserved);
	if (!$check['ok'])
		return array_merge($result, array('errors' => $check['errors'], 'codes' => $check['codes']));
	$preview = dimension_allocation_build_preview($prepared['request'], $rows);
	if (!$preview['ok'])
		return array_merge($result, array('errors' => $preview['errors'], 'codes' => $preview['codes']));
	$result['ok'] = true;
	$result['weights'] = $prepared['weights'];
	$result['driver'] = $prepared['driver'];
	$result['preview'] = $preview;
	return $result;
}

/**
 * Record, validate and dispatch one driver run through the DIM-ALC-001 engine.
 *
 * @param array $request Raw driver run request
 * @return array ok, state, duplicate, run_id, trans_no, fully_allocated, errors, codes
 */
function dimension_driver_submit_run($request)
{
	$result = array('ok' => false, 'state' => 'refused', 'duplicate' => false, 'run_id' => 0, 'trans_no' => 0, 'fully_allocated' => false,
		'errors' => array(), 'codes' => array());
	if (!dimension_driver_has_access('SA_JOURNALENTRY'))
		return dimension_allocation_fail($result, 'permission_denied', _('You are not allowed to run allocations.'));
	$normal = dimension_driver_normalize_run($request);
	if (!$normal['ok'])
		return array_merge($result, array('errors' => $normal['errors'], 'codes' => $normal['codes']));
	$run = $normal['run'];
	$prepared = dimension_driver_prepare($run);
	if (!$prepared['ok'])
		return array_merge($result, array('errors' => $prepared['errors'], 'codes' => $prepared['codes']));
	$driver = $prepared['driver'];
	$weights = $prepared['weights'];
	$extra = array('driver' => array('driver_id' => (int)$driver['driver_id'], 'driver_sha256' => $driver['content_sha256'],
		'run_request' => $run, 'fully_allocated' => $weights['fully_allocated'], 'targets' => $weights['targets']));
	$link = function ($run_id) use ($driver, $weights, $run, $extra) {
		return db_query('INSERT INTO '.TB_PREF.'dimension_allocation_driver_runs (run_id, driver_id, driver_sha256, residual_policy,'
			.' residual_target_id, residual_ppm, fully_allocated, weights_payload, created_at) VALUES ('.(int)$run_id.','
			.(int)$driver['driver_id'].','.db_escape($driver['content_sha256']).','.db_escape($run['residual_policy']).','
			.($weights['residual_dimension_id'] === null ? 'NULL' : (int)$weights['residual_dimension_id']).','.(int)$weights['residual_ppm'].','
			.($weights['fully_allocated'] ? 1 : 0).','.dimension_allocation_quote(json_encode($extra['driver'])).',NOW())') !== false;
	};
	// The shared engine normalizes its own input, so it receives the raw (not the normalized) request.
	$submitted = dimension_allocation_submit($prepared['raw_request'], $link, $extra);
	$result = array_merge($result, array('ok' => $submitted['ok'], 'state' => $submitted['state'], 'duplicate' => $submitted['duplicate'],
		'run_id' => $submitted['run_id'], 'trans_no' => $submitted['trans_no'], 'errors' => $submitted['errors'], 'codes' => $submitted['codes']));
	$result['fully_allocated'] = $weights['fully_allocated'];
	return $result;
}

/**
 * Guard a pending driver run against a stale, changed or out-of-period driver.
 *
 * Called when the run is dispatched and again inside the approval executor's posting transaction, so a driver
 * superseded while the journal waited for approval can never post.
 *
 * @param array $run Run row
 * @param array $payload Decoded run payload
 * @param bool $lock Lock the driver rows
 * @return array ok, code, text
 */
function dimension_driver_run_guard($run, $payload, $lock = false)
{
	$block = isset($payload['driver']) && is_array($payload['driver']) ? $payload['driver'] : null;
	if (!$block || !isset($block['driver_id'], $block['driver_sha256']))
		return array('ok' => false, 'code' => 'driver_payload', 'text' => _('The driver evidence of the run is missing.'));
	$driver = dimension_driver_load((int)$block['driver_id'], $lock);
	$usable = dimension_driver_usable($driver, $run['tran_date']);
	if (!$usable['ok'])
		return $usable;
	if ($driver['content_sha256'] !== $block['driver_sha256'])
		return array('ok' => false, 'code' => 'driver_stale', 'text' => _('The driver changed after the run was prepared.'));
	$link = db_query('SELECT driver_sha256 FROM '.TB_PREF.'dimension_allocation_driver_runs WHERE run_id='.(int)$run['run_id'].' AND driver_id='
		.(int)$driver['driver_id']);
	$link = $link ? db_fetch_assoc($link) : false;
	if (!$link || $link['driver_sha256'] !== $driver['content_sha256'])
		return array('ok' => false, 'code' => 'driver_unlinked', 'text' => _('The run is not linked to its driver version.'));
	return array('ok' => true, 'code' => '', 'text' => '');
}

/** Retry the stored driver request of a failed, rejected or cancelled run as a new run against the current driver state. */
function dimension_driver_retry($payload)
{
	$request = isset($payload['driver']['run_request']) ? $payload['driver']['run_request'] : null;
	if (!is_array($request))
		return array('ok' => false, 'state' => 'refused', 'duplicate' => false, 'run_id' => 0, 'trans_no' => 0, 'fully_allocated' => false,
			'errors' => array(_('The driver request of the run could not be read.')), 'codes' => array('not_retryable'));
	return dimension_driver_submit_run($request);
}

/**
 * Recent driver versions with their state.
 *
 * @param int $limit Maximum rows
 * @return array|false
 */
function dimension_driver_recent($limit = 50)
{
	$rows = db_query('SELECT * FROM '.TB_PREF.'dimension_allocation_drivers ORDER BY driver_id DESC LIMIT '.(int)$limit);
	if ($rows === false)
		return false;
	$drivers = array();
	while ($row = db_fetch_assoc($rows))
		$drivers[] = $row;
	return $drivers;
}
