<?php
/** DIM-LFC-001B1: minimized master-metadata observations, never delete authority. */
require_once __DIR__.'/dimension_lifecycle_db.inc';
require_once __DIR__.'/dimension_reference_schema.inc';

/**
 * Observe only ST_DIMENSION metadata and typed tag associations.
 * Authorization precedes every query. Missing tables, failed queries and dangling
 * tag identities remain unknown. No content, file path or actor is returned.
 * This snapshot does not cover scalar, payroll, session or extension owners and
 * must never be substituted for their adapters or a concurrent writer fence.
 */
function dimension_reference_metadata_observe($id)
{
    $result = array('state'=>'unknown', 'used_owners'=>array(),
        'unknown_owners'=>array(), 'deletion_authorized'=>false);
    $names = array('comments','attachments','references','audit','tags');
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)
        || !defined('ST_DIMENSION') || !defined('TAG_DIMENSION')) {
        $result['unknown_owners'] = $names;
        return $result;
    }
    $identity = db_escape((string)$id);
    $type = db_escape(ST_DIMENSION);
    $queries = array(
        'comments'=>'SELECT 1 FROM '.TB_PREF.'comments WHERE type='.$type.' AND id='.$identity.' LIMIT 1',
        'attachments'=>'SELECT 1 FROM '.TB_PREF.'attachments WHERE type_no='.$type.' AND trans_no='.$identity.' LIMIT 1',
        'references'=>'SELECT 1 FROM '.TB_PREF.'refs WHERE type='.$type.' AND id='.$identity.' LIMIT 1',
        'audit'=>'SELECT 1 FROM '.TB_PREF.'audit_trail WHERE type='.$type.' AND trans_no='.$identity.' LIMIT 1',
        // LEFT JOIN retains ambiguous associations instead of declaring them unused.
        'tags'=>'SELECT tags.type FROM '.TB_PREF.'tag_associations ta LEFT JOIN '.TB_PREF.'tags tags'
            .' ON tags.id=ta.tag_id WHERE ta.record_id='.$identity);
    $tables = array('comments'=>array('comments'), 'attachments'=>array('attachments'),
        'references'=>array('refs'), 'audit'=>array('audit_trail'),
        'tags'=>array('tag_associations','tags'));
    foreach ($queries as $owner=>$sql) {
        $status = 'unused';
        try {
            foreach ($tables[$owner] as $table) if (!dimension_reference_schema_available($table)) {
                $status = 'unknown';
                break;
            }
            if ($status === 'unknown') {
                $result['unknown_owners'][] = $owner;
                continue;
            }
            $rows = db_query($sql);
            if (!$rows) $status = 'unknown';
            else while ($row = db_fetch_row($rows)) {
                if ($owner !== 'tags' || ($row[0] !== null && (int)$row[0] === TAG_DIMENSION)) {
                    $status = 'used';
                    break;
                }
                if ($row[0] === null) $status = 'unknown';
            }
        } catch (Exception $exception) {
            $status = 'unknown';
        }
        if ($status === 'used') $result['used_owners'][] = $owner;
        elseif ($status === 'unknown') $result['unknown_owners'][] = $owner;
    }
    $result['state'] = count($result['used_owners']) ? 'used'
        : (count($result['unknown_owners']) ? 'unknown' : 'unused');
    return $result;
}
