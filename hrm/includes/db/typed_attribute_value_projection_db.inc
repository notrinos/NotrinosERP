<?php
/**
 * HRM-FND-007 bounded current-value projection for one normalized subject.
 *
 * This layer never enumerates subjects and never writes. Definition owner_scope
 * remains classification metadata; explicit no-auto-grant object-scope access
 * is required before metadata or plaintext is returned.
 */
if (!function_exists('hrm_typed_attribute_value_read_as_of'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_value_reader_db.inc');

function hrm_typed_attribute_value_projection_limit($limit)
{
    if (is_int($limit)) return $limit >= 1 && $limit <= 50 ? $limit : false;
    if (!is_string($limit) || preg_match('/^[1-9][0-9]*$/D', $limit) !== 1) return false;
    $n = (int)$limit;
    return $n >= 1 && $n <= 50 && (string)$n === $limit ? $n : false;
}

/**
 * Resolve accessible effective definitions for exactly one normalized subject.
 * At most 200 effective codes are inspected; larger configuration sets fail
 * closed instead of silently becoming an accidental bulk-discovery surface.
 */
function hrm_typed_attribute_value_projection_candidates($subject_type, $subject_id, $as_of, $for_manage = false)
{
    $subject_id = hrm_typed_attribute_value_positive_id($subject_id, false);
    $as_of = hrm_typed_attribute_value_datetime($as_of, false);
    if ($subject_id === false || $as_of === false || !is_bool($for_manage)
        || !in_array($subject_type, hrm_typed_attribute_subject_types(), true)
        || !hrm_typed_attribute_value_subject_exists($subject_type, $subject_id, false))
        return false;

    $date = substr($as_of, 0, 10);
    $sql = 'SELECT d.attribute_code FROM '.TB_PREF.'hrm_attribute_definitions d '
        .'WHERE EXISTS (SELECT 1 FROM '.TB_PREF.'hrm_attribute_definition_versions v '
        .'WHERE v.definition_id=d.definition_id AND v.allowed_subject='.db_escape($subject_type)
        ." AND v.definition_status IN ('active','deprecated','retired')"
        .' AND v.effective_from<='.db_escape($date).') '
        .'ORDER BY d.attribute_code LIMIT 201';
    $r = db_query($sql, 'could not inventory current Typed Attribute definitions');
    if (!$r || db_num_rows($r) > 200) return false;

    $out = array(); $seen = array();
    while ($row = db_fetch_assoc($r)) {
        $code = isset($row['attribute_code']) ? (string)$row['attribute_code'] : '';
        if ($code === '' || isset($seen[$code])) continue;
        $seen[$code] = true;
        $definition = hrm_typed_attribute_definition_resolve_state($code, $as_of, false);
        if (!is_array($definition) || (string)$definition['allowed_subject'] !== $subject_type)
            return false;
        $status=(string)$definition['definition_status'];
        if ($status==='retired') continue;
        if ($for_manage && $status!=='active') continue;
        if (!$for_manage && !in_array($status,array('active','deprecated'),true)) return false;
        if ($definition['effective_to']!==null && $date>(string)$definition['effective_to']) continue;
        $authorized = $for_manage
            ? hrm_typed_attribute_value_user_can_manage((string)$definition['sensitivity'])
            : hrm_typed_attribute_value_user_can_view((string)$definition['sensitivity']);
        if (!$authorized
            || !hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope']))
            continue;
        $out[] = array(
            'definition_id'=>(int)$definition['definition_id'],
            'attribute_code'=>(string)$definition['attribute_code'],
            'definition_version_id'=>(int)$definition['definition_version_id'],
            'value_type'=>(string)$definition['value_type'],
            'sensitivity'=>(string)$definition['sensitivity'],
            'owner_scope'=>(string)$definition['owner_scope'],
            'effective_from'=>(string)$definition['effective_from'],
            'effective_to'=>$definition['effective_to']===null?null:(string)$definition['effective_to'],
        );
    }
    return $out;
}

function get_hrm_typed_attribute_current_definition_choices($subject_type, $subject_id, $as_of, $limit = 50, $for_manage = false)
{
    $limit = hrm_typed_attribute_value_projection_limit($limit);
    if ($limit === false) return false;
    $rows = hrm_typed_attribute_value_projection_candidates($subject_type, $subject_id, $as_of, $for_manage);
    return is_array($rows) ? array_slice($rows, 0, $limit) : false;
}

/** Return at most 50 authorized current plaintext values for one exact subject. */
function get_hrm_typed_attribute_current_values($subject_type, $subject_id, $as_of, $limit = 50)
{
    $limit = hrm_typed_attribute_value_projection_limit($limit);
    if ($limit === false) return false;
    $definitions = hrm_typed_attribute_value_projection_candidates($subject_type, $subject_id, $as_of, false);
    if (!is_array($definitions)) return false;
    $out = array();
    foreach ($definitions as $definition) {
        $row = hrm_typed_attribute_value_read_as_of(
            (string)$definition['attribute_code'], $subject_type, $subject_id, $as_of);
        if ($row === false) return false;
        if ($row === null) continue;
        $out[] = $row;
        if (count($out) >= $limit) break;
    }
    return $out;
}

/**
 * Return the one immutable value-chain leaf for server-owned predecessor
 * custody. No caller-supplied predecessor is accepted by the browser route.
 */
function hrm_typed_attribute_value_current_leaf_id($definition_id, $subject_type, $subject_id)
{
    $definition_id = hrm_typed_attribute_value_positive_id($definition_id, false);
    $subject_id = hrm_typed_attribute_value_positive_id($subject_id, false);
    if ($definition_id === false || $subject_id === false
        || !in_array($subject_type, hrm_typed_attribute_subject_types(), true)
        || !hrm_typed_attribute_value_subject_exists($subject_type, $subject_id, false))
        return false;
    $sql = 'SELECT a.attribute_value_id FROM '.TB_PREF.'hrm_attribute_values a '
        .'INNER JOIN '.TB_PREF.'hrm_attribute_definition_versions v ON v.definition_version_id=a.definition_version_id '
        .'LEFT JOIN '.TB_PREF.'hrm_attribute_values s ON s.predecessor_id=a.attribute_value_id '
        .'AND s.subject_type=a.subject_type AND s.subject_id=a.subject_id '
        .'WHERE v.definition_id='.(int)$definition_id.' AND a.subject_type='.db_escape($subject_type)
        .' AND a.subject_id='.(int)$subject_id.' AND s.attribute_value_id IS NULL '
        .'ORDER BY a.attribute_value_id LIMIT 2';
    $r = db_query($sql, 'could not resolve Typed Attribute value chain leaf');
    if (!$r) return false;
    $count = db_num_rows($r);
    if ($count === 0) return null;
    if ($count !== 1) return false;
    $row = db_fetch_assoc($r);
    return isset($row['attribute_value_id']) && (int)$row['attribute_value_id'] > 0
        ? (int)$row['attribute_value_id'] : false;
}
