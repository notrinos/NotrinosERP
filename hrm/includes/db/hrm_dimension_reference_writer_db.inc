<?php
/**
 * DIM-LFC-001 B04: serialized Dimension-reference writes for HRM-owned stores.
 *
 * Every HRM write that stores a native Dimension coordinate (employees,
 * departments, organization units, assignments) must hold the referenced
 * Dimension masters FOR UPDATE, in ascending identity order, inside the
 * caller's open transaction before the owner row changes. Former references
 * are locked too. A NEW reference written from outside HRM (Hire, Department
 * maintenance) must be on its correct axis and open; a projection that copies
 * a value from an already locked HRM owner row only takes the lock, so legacy
 * closed, orphan and negative "irregular" references stay maintainable and are
 * never silently remapped.
 *
 * This is NOT a deletion certificate: the unused authority is the locked
 * recheck in dimensions/includes/dimension_reference_recheck.inc. Payloads are
 * never read or returned here; only numeric coordinates.
 */
if (!function_exists('dimension_writer_reference_id'))
    require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_reference_writer_fence.inc';

/**
 * Normalize one stored or submitted coordinate. Blank means no reference;
 * negative whole numbers are preserved legacy "irregular" values that no
 * Dimension master can match.
 *
 * @param mixed $value Candidate coordinate
 * @return int|false Canonical integer, or false for anything else
 */
function hrm_dimension_writer_coordinate($value)
{
    if ($value === null || $value === '') return 0;
    if (is_int($value)) return $value;
    if (!is_string($value) || !preg_match('/^(?:0|-?[1-9][0-9]*)$/D', $value)) return false;
    $integer = (int)$value;
    return (string)$integer === $value ? $integer : false;
}

/**
 * Lock and admit a set of HRM Dimension coordinates.
 *
 * Each coordinate is array('axis'=>1|2, 'old'=>mixed, 'new'=>mixed,
 * 'validate'=>bool). With validate (the default) a changed positive new value
 * must name an existing open master of the given axis. Without it the value is
 * a projection copy of an already locked HRM owner and only the lock is taken.
 * Fails closed on any malformed value, lock failure or deadlock; the caller
 * must then roll back. Outside an open transaction a locking read could not
 * hold anything, so a coordinate set that validates anything fails closed,
 * while a pure projection copy (every coordinate has validate=false) returns
 * true without locks: that is the offline upgrade/reconciliation re-projection
 * of existing HRM state, never a runtime writer, which always runs inside its
 * own transaction and therefore takes the locks.
 *
 * @param array $coordinates List of coordinate descriptors
 * @return bool True only when every master is locked and admitted
 */
function hrm_dimension_writer_admit($coordinates)
{
    if (!is_array($coordinates)) return false;
    $owned = !empty($GLOBALS['transaction_level']);
    if (!$owned) {
        foreach ($coordinates as $coordinate)
            if (!is_array($coordinate) || !array_key_exists('validate', $coordinate) || $coordinate['validate'] !== false) return false;
        return true;
    }
    $locks = array();
    $normalized = array();
    foreach ($coordinates as $coordinate) {
        if (!is_array($coordinate) || !isset($coordinate['axis']) || !in_array($coordinate['axis'], array(1, 2), true)
            || !array_key_exists('old', $coordinate) || !array_key_exists('new', $coordinate)) return false;
        $old = hrm_dimension_writer_coordinate($coordinate['old']);
        $new = hrm_dimension_writer_coordinate($coordinate['new']);
        if ($old === false || $new === false) return false;
        $normalized[] = array($coordinate['axis'], $old, $new, !isset($coordinate['validate']) || $coordinate['validate'] !== false);
        foreach (array($old, $new) as $id) if ($id > 0) $locks[$id] = $id;
    }
    sort($locks, SORT_NUMERIC);
    $masters = array();
    foreach ($locks as $id) {
        $rows = db_query('SELECT id, type_, closed FROM '.TB_PREF.'dimensions WHERE id='.$id.' FOR UPDATE');
        if (!$rows) return false;
        $masters[$id] = db_fetch_assoc($rows); // False is allowed only for an existing orphan reference.
    }
    foreach ($normalized as $coordinate) {
        list($axis, $old, $new, $validate) = $coordinate;
        if (!$validate || $new === $old) continue;
        if ($new < 0) return false; // A new reference never adopts a legacy irregular value.
        if ($new === 0) continue;
        $master = $masters[$new];
        if (!is_array($master) || (int)$master['type_'] !== $axis || (int)$master['closed'] !== 0) return false;
    }
    return true;
}
