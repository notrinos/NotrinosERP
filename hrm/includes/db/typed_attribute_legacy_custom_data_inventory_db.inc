<?php
/**
 * HRM-FND-007 restricted legacy custom_data inventory/compatibility guard.
 *
 * Values are never returned. Unknown/nested JSON remains opaque compatibility
 * custody and no Typed Attribute row is created by this boundary.
 */
if (!function_exists('hrm_compatibility_profile_current'))
    include_once($path_to_root.'/hrm/includes/db/employee_compatibility_profile_db.inc');
if (!function_exists('get_hrm_typed_attribute_legacy_mapping_as_of'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_mapping_db.inc');

function hrm_typed_attribute_legacy_custom_data_parity($stored, $retained)
{
    if (($stored === null || $stored === '') && ($retained === null || $retained === '')) return true;
    return is_scalar($stored) && is_scalar($retained) && (string)$stored === (string)$retained;
}

/** Return key/type metadata only, with a hard 100-key bound and no values. */
function hrm_typed_attribute_legacy_custom_data_inventory_decode($raw)
{
    if ($raw === null || $raw === '') return array();
    if (!is_string($raw) || strlen($raw) > 1048576) return false;
    $trim = ltrim($raw);
    if ($trim === '' || $trim[0] !== '{') return false;
    $decoded = json_decode($raw, true);
    if (!is_array($decoded) || json_last_error() !== JSON_ERROR_NONE || count($decoded) > 100) return false;
    $out = array();
    foreach ($decoded as $key=>$value) {
        $canonical = hrm_typed_attribute_legacy_mapping_key((string)$key);
        if ($canonical === false) return false;
        $kind = is_array($value) || is_object($value) ? 'opaque_nested' : 'scalar';
        $out[$canonical] = $kind;
    }
    ksort($out, SORT_STRING);
    return $out;
}

/** Inventory exactly one current legacy Employee/Worker pair without exposing values. */
function get_hrm_typed_attribute_legacy_custom_data_inventory($employee_number, $as_of)
{
    if (!hrm_typed_attribute_legacy_mapping_user_can_view() || (int)$employee_number <= 0)
        return false;
    $as_of = hrm_typed_attribute_legacy_mapping_date($as_of, false);
    if ($as_of === false) return false;
    $binding = function_exists('get_hrm_person_worker_binding')
        ? get_hrm_person_worker_binding((int)$employee_number) : false;
    if (!$binding || empty($binding['worker_id']) || $binding['retired_at'] !== null) return false;
    $profile = hrm_compatibility_profile_current((int)$binding['worker_id']);
    $retained = $profile ? hrm_compatibility_profile_open($profile) : false;
    if ($retained === false || !array_key_exists('custom_data',$retained)) return false;
    $r = db_query('SELECT custom_data FROM '.TB_PREF.'employees WHERE employee_number='.(int)$employee_number.' LIMIT 2',
        'could not read legacy Employee custom_data compatibility state');
    if (!$r || db_num_rows($r) !== 1) return false;
    $stored = db_fetch_assoc($r);
    if (!hrm_typed_attribute_legacy_custom_data_parity($stored['custom_data'], $retained['custom_data'])) return false;
    $keys = hrm_typed_attribute_legacy_custom_data_inventory_decode($retained['custom_data']);
    if ($keys === false) return false;
    $out = array();
    foreach ($keys as $key=>$kind) {
        $mapping = $kind === 'scalar' ? get_hrm_typed_attribute_legacy_mapping_as_of($key, $as_of) : null;
        if ($mapping === false) return false;
        $out[] = array(
            'legacy_key'=>$key,
            'legacy_kind'=>$kind,
            'mapping_state'=>$mapping === null ? 'unmapped' : 'approved',
            'attribute_code'=>$mapping === null ? null : (string)$mapping['attribute_code'],
            'definition_status'=>$mapping === null ? null : (string)$mapping['definition_status'],
            'mapping_ready_for_future_execution'=>$mapping === null ? false : (bool)$mapping['mapping_ready_for_future_execution'],
        );
    }
    return array('employee_number'=>(int)$employee_number,'worker_id'=>(int)$binding['worker_id'],
        'key_count'=>count($out),'value_bytes_exposed'=>0,'rows'=>$out);
}
