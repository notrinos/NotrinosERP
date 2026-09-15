<?php
/**
 * HRM-FND-007 legacy employees.custom_data mapping-policy custody.
 *
 * This file is read-only. It does not parse legacy values into Typed Attribute
 * rows and it never grants authority from a key name, owner_scope or definition.
 */

if (!function_exists('hrm_typed_attribute_definition_resolve_state'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_command_db.inc');

function hrm_typed_attribute_legacy_mapping_user_can_view()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING');
}

function hrm_typed_attribute_legacy_mapping_key($value)
{
    if (!is_string($value) || strlen($value) < 1 || strlen($value) > 128)
        return false;
    return preg_match('/^[A-Za-z][A-Za-z0-9_.:-]{0,127}$/D', $value) === 1 ? $value : false;
}

function hrm_typed_attribute_legacy_mapping_date($value, $nullable = false)
{
    if ($nullable && $value === null) return null;
    if (!is_string($value) || preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1)
        return false;
    $d = DateTime::createFromFormat('!Y-m-d', $value);
    return $d && $d->format('Y-m-d') === $value ? $value : false;
}

/** Validate immutable mapping metadata. Legacy employee custom_data is Worker-owned compatibility custody only. */
function hrm_typed_attribute_legacy_mapping_validate_metadata($row)
{
    if (!is_array($row)) return false;
    $required = array('legacy_key','attribute_code','subject_type','mapping_status','effective_from',
        'effective_to','qualified_review_ref','approved_at','approved_by');
    if (array_diff($required, array_keys($row))) return false;
    $key = hrm_typed_attribute_legacy_mapping_key($row['legacy_key']);
    $code = isset($row['attribute_code']) && is_string($row['attribute_code'])
        && preg_match('/^[a-z][a-z0-9_.-]{0,63}$/D', $row['attribute_code']) === 1
        ? $row['attribute_code'] : false;
    $from = hrm_typed_attribute_legacy_mapping_date($row['effective_from'], false);
    $to = hrm_typed_attribute_legacy_mapping_date($row['effective_to'], true);
    if ($key === false || $code === false || $row['subject_type'] !== 'worker'
        || !in_array($row['mapping_status'], array('draft','approved','retired'), true)
        || $from === false || $to === false || ($to !== null && $to < $from)) return false;
    if ($row['mapping_status'] === 'approved') {
        if (!is_string($row['qualified_review_ref']) || strlen($row['qualified_review_ref']) < 1
            || strlen($row['qualified_review_ref']) > 128
            || !is_string($row['approved_at'])
            || preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$/D', $row['approved_at']) !== 1
            || (int)$row['approved_by'] <= 0) return false;
    }
    return true;
}

/** Resolve one explicit approved mapping. Ambiguous policy or definition state fails closed. */
function get_hrm_typed_attribute_legacy_mapping_as_of($legacy_key, $as_of)
{
    if (!hrm_typed_attribute_legacy_mapping_user_can_view()) return false;
    $legacy_key = hrm_typed_attribute_legacy_mapping_key($legacy_key);
    $as_of = hrm_typed_attribute_legacy_mapping_date($as_of, false);
    if ($legacy_key === false || $as_of === false) return false;
    $sql = 'SELECT m.mapping_version_id,m.legacy_key,m.version_no,m.predecessor_id,m.attribute_code,m.subject_type,'
        .'m.mapping_status,m.effective_from,m.effective_to,m.qualified_review_ref,m.approved_at,m.approved_by '
        .'FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions m '
        .'WHERE m.legacy_key='.db_escape($legacy_key)." AND m.mapping_status='approved'"
        .' AND m.effective_from<='.db_escape($as_of)
        .' AND (m.effective_to IS NULL OR m.effective_to>='.db_escape($as_of).') '
        .'ORDER BY m.effective_from DESC,m.version_no DESC,m.mapping_version_id DESC LIMIT 2';
    $r = db_query($sql, 'could not resolve legacy Typed Attribute mapping policy');
    if (!$r || db_num_rows($r) > 1) return false;
    if (db_num_rows($r) === 0) return null;
    $row = db_fetch_assoc($r);
    if (!hrm_typed_attribute_legacy_mapping_validate_metadata($row)) return false;

    $definition = hrm_typed_attribute_definition_resolve_state(
        (string)$row['attribute_code'], $as_of.' 00:00:00', false);
    if (!is_array($definition) || $definition['allowed_subject'] !== 'worker'
        || ($definition['effective_to'] !== null && $as_of > (string)$definition['effective_to'])) return false;
    $row['definition_id'] = (int)$definition['definition_id'];
    $row['definition_version_id'] = (int)$definition['definition_version_id'];
    $row['definition_status'] = (string)$definition['definition_status'];
    $row['mapping_ready_for_future_execution'] = $definition['definition_status'] === 'active';
    return $row;
}
