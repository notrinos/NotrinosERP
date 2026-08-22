<?php
/**
 * Legacy positions entity with HRM-FND-003 normalized Position custody.
 *
 * The maintained SA_POSITION writer keeps only Position identity/status in
 * sync with hrm_positions. Job class, Department, reporting hierarchy,
 * compensation, headcount, manager and description fields remain legacy-only
 * metadata. Physical deletion is denied once normalized Position custody exists.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root . '/includes/db/domain_audit_db.inc');
include_once($path_to_root . '/hrm/includes/db/legal_entity_db.inc');
include_once($path_to_root . '/hrm/includes/db/position_db.inc');

function hrm_job_position_writer_authorized()
{
    return isset($_SESSION['wa_current_user'])
        && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'can_access')
        && $_SESSION['wa_current_user']->can_access('SA_POSITION');
}

class job_positions_entity extends Entity {
    function __construct() {
        parent::__construct(
            'positions',
            array(
                'position_id',
                'position_code',
                'position_name',
                'job_class_id',
                'department_id',
                'reports_to_position_id',
                'basic_amount',
                'min_salary',
                'max_salary',
                'budgeted_headcount',
                'is_manager',
                'description',
                'inactive'
            ),
            'position_id'
        );
    }

    /** Lock and verify every existing Position identity/status projection. */
    private static function lock_projection_state($verify_projection)
    {
        $result = db_query("SELECT position_id,position_code,position_name,inactive FROM "
            .TB_PREF."positions ORDER BY position_id FOR UPDATE", 'could not lock legacy Positions');
        if (!$result)
            return false;
        $rows = array();
        while ($row = db_fetch_assoc($result)) {
            $id = (int)$row['position_id'];
            if ($id <= 0 || isset($rows[$id]))
                return false;
            $rows[$id] = $row;
        }
        if (!$verify_projection || !hrm_position_table_ready())
            return $rows;
        $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
        if ($legal_entity_id === false)
            return false;
        foreach ($rows as $id=>$row) {
            $projection = hrm_position_projection_from_legacy_position($row, (int)$legal_entity_id);
            $position = get_hrm_position_by_legacy_position($id, true);
            if (!$projection || !is_array($position)
                || !hrm_position_row_matches_projection($position, $projection))
                return false;
        }
        return $rows;
    }

    /** Return normalized Position projection fields changed by this update. */
    private static function projection_changed_fields($before, $after)
    {
        $map = array(
            'position_code'=>'legacy_position_code',
            'position_name'=>'position_name',
            'inactive'=>'position_status',
        );
        $changed = array();
        foreach ($map as $legacy=>$normalized) {
            $left = array_key_exists($legacy, $before) ? (string)$before[$legacy] : '';
            $right = array_key_exists($legacy, $after) ? (string)$after[$legacy] : $left;
            if ($left !== $right)
                $changed[] = $normalized;
        }
        return $changed;
    }

    public static function create($data)
    {
        if (!hrm_job_position_writer_authorized())
            return false;
        begin_transaction();
        $e = new static();
        $existing = static::lock_projection_state(true);
        $result = is_array($existing) && $e->insert($data);
        $legacy_position_id = $result ? (int)db_insert_id() : 0;
        if ($result && hrm_position_table_ready())
            $result = synchronize_hrm_position_from_legacy_position(
                $legacy_position_id,
                array('legacy_position_code','position_name','position_status'),
                'create'
            );
        if ($result)
            commit_transaction();
        else
            cancel_transaction();
        return (bool)$result;
    }

    public static function modify($key, $data)
    {
        if (!hrm_job_position_writer_authorized() || (int)$key <= 0)
            return false;
        begin_transaction();
        $e = new static();
        $rows = static::lock_projection_state(true);
        $before = is_array($rows) && isset($rows[(int)$key]) ? $rows[(int)$key] : false;
        $changed = $before ? static::projection_changed_fields($before, $data) : array();
        $result = $before !== false && $e->update($key, $data);
        if ($result && hrm_position_table_ready() && !empty($changed))
            $result = synchronize_hrm_position_from_legacy_position((int)$key, $changed, 'update');
        if ($result && hrm_position_table_ready())
            $result = is_array(static::lock_projection_state(true));
        if ($result)
            commit_transaction();
        else
            cancel_transaction();
        return (bool)$result;
    }

    public static function set_inactive($position_id, $inactive)
    {
        if (!hrm_job_position_writer_authorized() || (int)$position_id <= 0)
            return false;
        begin_transaction();
        $rows = static::lock_projection_state(true);
        $current = is_array($rows) && isset($rows[(int)$position_id])
            ? (int)$rows[(int)$position_id]['inactive'] : -1;
        $inactive = $inactive ? 1 : 0;
        $result = $current !== -1;
        if ($result && $current !== $inactive)
            $result = db_query("UPDATE ".TB_PREF."positions SET inactive=".$inactive
                ." WHERE position_id=".(int)$position_id, 'could not update Position inactive status');
        if ($result && $current !== $inactive && hrm_position_table_ready())
            $result = synchronize_hrm_position_from_legacy_position(
                (int)$position_id, array('position_status'), 'inactive'
            );
        if ($result)
            commit_transaction();
        else
            cancel_transaction();
        return (bool)$result;
    }

    public static function remove($key)
    {
        if (!hrm_job_position_writer_authorized())
            return false;
        if (hrm_position_table_ready())
            return false;
        $e = new static();
        return $e->delete($key);
    }
}

/** Route inactive checkbox mutation through the synchronized Position writer. */
function hrm_job_position_inactive_control_cell($position_id, $value)
{
    $name = 'Inactive'.$position_id;
    $value = $value ? 1 : 0;
    if (check_value('show_inactive')) {
        if (isset($_POST['LInact'][$position_id])
            && (get_post('_Inactive'.$position_id.'_update') || get_post('Update'))
            && (check_value('Inactive'.$position_id) != $value)) {
            if (!job_positions_entity::set_inactive($position_id, !$value))
                display_error(_('The Position status could not be synchronized.'));
        }
        echo '<td align="center">'.checkbox(null, $name, $value, true, '')
            .hidden("LInact[$position_id]", $value, false).'</td>';
    }
}
