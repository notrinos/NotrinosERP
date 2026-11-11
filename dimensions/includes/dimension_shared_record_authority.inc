<?php
/**
 * DIM-LFC-001 B06: shared record authority (attachments, audit trail, comments,
 * references, reference lines, tags and tag associations).
 *
 * These stores are shared by every module and keyed by a transaction/record
 * type. A shared row is never judged by one generic scan: each record type is
 * assigned to the domain authority that owns it (ordinary transaction and
 * master families, protected HRM families, protected payroll families); every
 * unassigned, undecodable, ambiguous or dangling key stays unknown. Only a
 * decoded Dimension reference or the Dimension's own identity-bound evidence
 * is positive, and nothing here ever authorizes a deletion.
 *
 * Reader side: dimension_shared_record_observe() runs inside the B10 locked
 * recheck (current committed locking reads, owner rows first, master last).
 * Writer side: the two fence functions below keep new references out of the
 * shared custom_data and serialize every Dimension-bound record with the
 * Dimension master lock.
 */
require_once __DIR__.'/dimension_lifecycle_db.inc';
require_once __DIR__.'/dimension_master_payload_guard.inc';

/**
 * Shared stores that retain custom_data, with their record-type discriminator.
 *
 * @return array table => discriminator column
 */
function dimension_shared_record_stores()
{
    return array('attachments'=>'type_no', 'audit_trail'=>'type', 'reflines'=>'trans_type', 'tags'=>'type');
}

/**
 * Every owner name the shared authority reports to the B10 recheck.
 *
 * @return string[]
 */
function dimension_shared_record_owner_names()
{
    return array('attachments', 'audit_trail', 'comments', 'refs', 'reflines', 'tag_associations', 'tags');
}

/**
 * Tables whose live schema the shared authority needs.
 *
 * @return string[]
 */
function dimension_shared_record_tables()
{
    return dimension_shared_record_owner_names();
}

/**
 * Record types assigned to each domain authority. A type absent from every
 * family is unsupported and always unknown.
 *
 * @return array family => (table kind => constant names)
 */
function dimension_shared_record_family_names()
{
    return array(
        'ordinary'=>array('type'=>array('ST_JOURNAL','ST_BANKPAYMENT','ST_BANKDEPOSIT','ST_BANKTRANSFER','ST_SALESINVOICE',
            'ST_CUSTCREDIT','ST_CUSTPAYMENT','ST_CUSTDELIVERY','ST_LOCTRANSFER','ST_INVADJUST','ST_PURCHORDER','ST_SUPPINVOICE',
            'ST_SUPPCREDIT','ST_SUPPAYMENT','ST_SUPPRECEIVE','ST_WORKORDER','ST_MANUISSUE','ST_MANURECEIVE','ST_SALESORDER',
            'ST_SALESQUOTE','ST_COSTUPDATE','ST_DIMENSION','ST_CUSTOMER','ST_SUPPLIER','ST_ITEM','ST_FIXEDASSET','ST_BANKACCOUNT',
            'ST_WH_TRANSFER_ORDER','ST_WH_MATERIAL_REQUEST','ST_WH_SCRAP','ST_WH_RETURN_ORDER','ST_WH_CYCLE_COUNT','ST_WH_INTERNAL_MOVE',
            'ST_SALESAGREEMENT','ST_SALESRETURN','ST_PURCHREQUISITION','ST_PURCHRFQ','ST_PURCHAGREEMENT','ST_PROCUREMENTPLAN',
            'ST_PURCHMATCHEXCEPTION','ST_VENDOR_EVALUATION'),
            'tag'=>array('TAG_ACCOUNT','TAG_DIMENSION','TAG_CRM')),
        'hrm'=>array('type'=>array('ST_EMPLOYEE','ST_LEAVE_REQUEST','ST_OVERTIME_REQUEST','ST_LOAN_REQUEST','ST_EMPLOYEE_APPRAISAL',
            'ST_HRM_IDENTIFIER_VERIFICATION','ST_HRM_BANK_ACCOUNT_VERIFICATION','ST_HRM_BANK_PAYMENT_ELECTION',
            'ST_HRM_LIFECYCLE_TRANSFER','ST_HRM_LIFECYCLE_SEPARATION','ST_HRM_LIFECYCLE_SALARY_REVISION',
            'ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT','ST_HRM_LIFECYCLE_CONFIRMATION','ST_HRM_LIFECYCLE_PROBATION_SCHEDULE',
            'ST_HRM_LIFECYCLE_PROMOTION','ST_HRM_LIFECYCLE_SUSPENSION','ST_HRM_LIFECYCLE_REINSTATEMENT',
            'ST_HRM_LIFECYCLE_HIRE','ST_HRM_LIFECYCLE_REHIRE','ST_HRM_COMPENSATION_AWARD'), 'tag'=>array()),
        'payroll'=>array('type'=>array('ST_PAYSLIP','ST_EMPPAYMENT','ST_PAYROLL_PERIOD'), 'tag'=>array()));
}

/**
 * Assign one shared-store row to its domain authority.
 *
 * @param string $table Shared table (tags classify by tag type, the rest by transaction type)
 * @param mixed $type Raw discriminator value
 * @return string ordinary, hrm, payroll or unsupported
 */
function dimension_shared_record_family($table, $type)
{
    if (!is_string($type) && !is_int($type)) return 'unsupported';
    if (!preg_match('/^(?:0|[1-9][0-9]*)$/D', (string)$type)) return 'unsupported';
    $kind = $table === 'tags' ? 'tag' : 'type';
    foreach (dimension_shared_record_family_names() as $family=>$kinds)
        foreach ($kinds[$kind] as $name)
            if (defined($name) && (string)constant($name) === (string)$type) return $family;
    return 'unsupported';
}

/**
 * Does a decoded payload contain a coordinate-like key at any depth? Used for
 * the protected families, whose independent axes never map onto a core
 * Dimension through this generic authority.
 *
 * @param array $payload Decoded JSON
 * @param int $depth Recursion depth
 * @return bool|null True when present, false when absent, null when too deep
 */
function dimension_shared_record_has_coordinate_key($payload, $depth = 0)
{
    if ($depth > 64) return null;
    $keys = array('dimension_id', 'dimension', 'dimension2_id', 'dimension2', 'cost_center_id', 'legacy_cost_center_id',
        'legacy_dimension2_id', 'core_dimension_id', 'core_dimension2_id', 'axis_value_key', 'core_slot_hint');
    foreach ($payload as $key=>$value) {
        if (in_array($key, $keys, true)) return true;
        if (is_array($value)) {
            $nested = dimension_shared_record_has_coordinate_key($value, $depth + 1);
            if ($nested !== false) return $nested;
        }
    }
    return false;
}

/**
 * Classify one retained shared payload under its domain authority.
 *
 * @param string $family ordinary, hrm, payroll or unsupported
 * @param mixed $raw Raw custom_data column value
 * @param int $id Exact native Dimension identity
 * @return string used, unused or unknown
 */
function dimension_shared_record_payload_status($family, $raw, $id)
{
    if ($family === 'ordinary') {
        $references = dimension_master_payload_references($raw);
        if ($references === false) return 'unknown';
        foreach ($references as $reference) if ((int)$reference[0] === (int)$id) return 'used';
        return 'unused';
    }
    if ($family !== 'hrm' && $family !== 'payroll') return 'unknown';
    if ($raw === null || $raw === '') return 'unused';
    if (!is_string($raw)) return 'unknown';
    $payload = json_decode($raw, true);
    if (!is_array($payload) || json_last_error() !== JSON_ERROR_NONE) return 'unknown';
    return dimension_shared_record_has_coordinate_key($payload) === false ? 'unused' : 'unknown';
}

/**
 * Identity predicate of the Dimension's own evidence in one store.
 *
 * @return string|null SQL predicate, or null when the store has no Dimension-keyed rows
 */
function dimension_shared_record_identity_predicate($table, $id)
{
    if (!defined('ST_DIMENSION')) return false;
    $type = (int)ST_DIMENSION;
    $id = (int)$id;
    switch ($table) {
        case 'attachments': return 'type_no='.$type.' AND trans_no='.$id;
        case 'audit_trail': return 'type='.$type.' AND trans_no='.$id;
        case 'comments':
        case 'refs': return 'type='.$type.' AND id='.$id;
    }
    return null;
}

/**
 * Classify one retaining shared store: the Dimension's own evidence first, then
 * every retained custom_data row through its domain authority. Locking reads;
 * the caller owns the transaction.
 *
 * @return string used, unused or unknown
 */
function dimension_shared_record_store_state($table, $discriminator, $columns, $id)
{
    if (!dimension_reference_recheck_schema($columns, array(), true) || !isset($columns[$discriminator])) return 'unknown';
    $identity = dimension_shared_record_identity_predicate($table, $id);
    if ($identity === false) return 'unknown';
    if ($identity !== null) {
        $rows = db_query('SELECT 1 FROM '.TB_PREF.$table.' WHERE '.$identity.' LIMIT 1 LOCK IN SHARE MODE');
        if (!$rows) return 'unknown';
        if (db_num_rows($rows)) return 'used';
    }
    $rows = db_query('SELECT `'.$discriminator.'`, custom_data FROM '.TB_PREF.$table.' LOCK IN SHARE MODE');
    if (!$rows) return 'unknown';
    $state = 'unused';
    while ($row = db_fetch_row($rows)) {
        $status = dimension_shared_record_payload_status(dimension_shared_record_family($table, $row[0]), $row[1], $id);
        if ($status === 'used') return 'used';
        if ($status === 'unknown') $state = 'unknown';
    }
    return $state;
}

/**
 * Dimension-keyed evidence stores without retained JSON (comments, references).
 *
 * @return string used, unused or unknown
 */
function dimension_shared_record_keyed_state($table, $columns, $id)
{
    foreach (array('type', 'id') as $column) if (!isset($columns[$column])) return 'unknown';
    $identity = dimension_shared_record_identity_predicate($table, $id);
    if (!is_string($identity)) return 'unknown';
    $rows = db_query('SELECT 1 FROM '.TB_PREF.$table.' WHERE '.$identity.' LIMIT 1 LOCK IN SHARE MODE');
    if (!$rows) return 'unknown';
    return db_num_rows($rows) ? 'used' : 'unused';
}

/**
 * Tag associations of this Dimension. Only an exact canonical record identity
 * on a Dimension-typed tag is used; a dangling tag, a non-canonical numeric
 * spelling of the identity, or any unreadable row is unknown. Tags of other
 * types that share the numeric identity space are not this Dimension.
 *
 * @return string used, unused or unknown
 */
function dimension_shared_record_tag_state($columns_associations, $columns_tags, $id)
{
    if (!isset($columns_associations['record_id'], $columns_associations['tag_id'])
        || !isset($columns_tags['id'], $columns_tags['type']) || !defined('TAG_DIMENSION')) return 'unknown';
    $id = (int)$id;
    $rows = db_query('SELECT ta.record_id, tags.type FROM '.TB_PREF.'tag_associations ta LEFT JOIN '.TB_PREF.'tags tags'
        .' ON tags.id=ta.tag_id WHERE ta.record_id=\''.$id.'\' OR CAST(ta.record_id AS SIGNED)='.$id.' LOCK IN SHARE MODE');
    if (!$rows) return 'unknown';
    $state = 'unused';
    while ($row = db_fetch_row($rows)) {
        $canonical = (string)$row[0] === (string)$id;
        if ($row[1] === null) { $state = 'unknown'; continue; }
        if ((int)$row[1] !== (int)TAG_DIMENSION) continue;
        if ($canonical) return 'used';
        $state = 'unknown';
    }
    return $state;
}

/**
 * Observe every shared owner for one Dimension inside the caller's open
 * transaction (called from the B10 locked recheck, before the master lock).
 *
 * @param int|string $id Exact positive native Dimension identity
 * @param array $columns Live schema map from dimension_reference_recheck_columns()
 * @return array owner => used|unused|unknown; never any payload
 */
function dimension_shared_record_observe($id, $columns)
{
    $states = array();
    foreach (dimension_shared_record_owner_names() as $name) $states[$name] = 'unknown';
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id) || empty($GLOBALS['transaction_level'])
        || !is_array($columns)) return $states;
    try {
        foreach (dimension_shared_record_owner_names() as $name) {
            if ($name === 'tag_associations') {
                $states[$name] = isset($columns['tag_associations'], $columns['tags'])
                    ? dimension_shared_record_tag_state($columns['tag_associations'], $columns['tags'], $id) : 'unknown';
            } elseif (isset(dimension_shared_record_stores()[$name])) {
                $stores = dimension_shared_record_stores();
                $states[$name] = isset($columns[$name]) ? dimension_shared_record_store_state($name, $stores[$name], $columns[$name], $id) : 'unknown';
            } else {
                $states[$name] = isset($columns[$name]) ? dimension_shared_record_keyed_state($name, $columns[$name], $id) : 'unknown';
            }
            if ($states[$name] === 'unknown' && db_error_no()) return $states;
        }
    } catch (Exception $exception) {
        // Unavailable owners never prove unused state.
    }
    return $states;
}

/**
 * Writer fence for retained shared JSON: refuse a coordinate-like key anywhere
 * in a payload that would be stored in a shared custom_data column. Shared
 * records carry no Dimension coordinates (the owning domain does); a legacy row
 * that already holds one stays readable and keeps its owner unknown/used.
 *
 * @param mixed $custom_data Array about to be json_encode()d, or null
 * @return bool True when the payload may be stored
 */
function dimension_shared_record_custom_data_admit($custom_data)
{
    if ($custom_data === null || $custom_data === array()) return true;
    return is_array($custom_data) && dimension_shared_record_has_coordinate_key($custom_data) === false;
}

/**
 * Writer fence for Dimension-bound shared records: serialize the write with the
 * Dimension master lock and refuse a missing master. Other record types pass
 * through untouched. The caller must hold an open transaction, so the lock
 * lasts until its commit; otherwise a Dimension-bound write is refused.
 *
 * @param int|string $type Transaction type (or tag type when $tag is true)
 * @param int|string $record Record identity bound to the type
 * @param bool $tag True when $type is a tag type
 * @return bool True when the write may proceed
 */
function dimension_shared_record_identity_admit($type, $record, $tag = false)
{
    $constant = $tag ? 'TAG_DIMENSION' : 'ST_DIMENSION';
    if (!defined($constant) || !is_scalar($type) || (string)$type !== (string)constant($constant)) return true;
    if (empty($GLOBALS['transaction_level'])) return false;
    if (!is_scalar($record) || !preg_match('/^[1-9][0-9]*$/D', (string)$record)) return false;
    $rows = db_query('SELECT id FROM '.TB_PREF.'dimensions WHERE id='.(int)$record.' FOR UPDATE');
    return $rows && db_num_rows($rows) === 1;
}

/**
 * Run one Dimension-bound shared write under the Dimension master lock. Other
 * record types run the callback unchanged. A refused admission returns false
 * before the callback runs; the callback's own return value is preserved.
 *
 * @param int|string $type Transaction type (or tag type when $tag is true)
 * @param int|string $record Record identity bound to the type
 * @param callable $callback The unchanged native write
 * @param bool $tag True when $type is a tag type
 * @return mixed The callback result, or false when the fence refuses
 */
function dimension_shared_record_bound_write($type, $record, $callback, $tag = false)
{
    $constant = $tag ? 'TAG_DIMENSION' : 'ST_DIMENSION';
    if (!defined($constant) || !is_scalar($type) || (string)$type !== (string)constant($constant))
        return call_user_func($callback);
    if (!begin_transaction()) return false;
    if (!dimension_shared_record_identity_admit($type, $record, $tag)) { cancel_transaction(); return false; }
    $result = call_user_func($callback);
    if ($result === false || !commit_transaction()) { cancel_transaction(); return false; }
    return $result;
}
