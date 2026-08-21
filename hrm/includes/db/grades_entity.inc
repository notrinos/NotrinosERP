<?php
/**
 * Entity class for legacy pay_grades with HRM-FND-003 normalized Grade custody.
 *
 * Legacy salary ranges remain authoritative compensation metadata. Once the
 * normalized Grade foundation exists, identity/level/status writes are kept in
 * one SA_PAYGRADE-authorized transaction with hrm_grades and restricted audit
 * evidence. Physical deletion is denied to preserve Assignment/history custody.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root . '/includes/db/domain_audit_db.inc');
include_once($path_to_root . '/hrm/includes/db/legal_entity_db.inc');
include_once($path_to_root . '/hrm/includes/db/grade_db.inc');

function hrm_pay_grade_writer_authorized()
{
    return isset($_SESSION['wa_current_user'])
        && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'can_access')
        && $_SESSION['wa_current_user']->can_access('SA_PAYGRADE');
}

class grades_entity extends Entity {
    function __construct() {
        parent::__construct(
            'pay_grades',
            array('grade_id','grade_code','grade_name','grade_level','min_salary',
                'mid_salary','max_salary','description','inactive'),
            'grade_id'
        );
    }

    /** Lock and verify every existing Pay Grade projection before mutation. */
    private static function lock_projection_state($verify_projection) {
        $result = db_query("SELECT grade_id,grade_code,grade_name,grade_level,inactive FROM "
            .TB_PREF."pay_grades ORDER BY grade_id FOR UPDATE", 'could not lock legacy Pay Grades');
        if (!$result)
            return false;
        $rows = array();
        while ($row = db_fetch_assoc($result)) {
            $id = (int)$row['grade_id'];
            if ($id <= 0 || isset($rows[$id]))
                return false;
            $rows[$id] = $row;
        }
        if (!$verify_projection || !hrm_grade_table_ready())
            return $rows;
        $legal_entity_id = get_hrm_default_legal_entity_binding_id(true);
        if ($legal_entity_id === false)
            return false;
        foreach ($rows as $id=>$row) {
            $projection = hrm_grade_projection_from_pay_grade($row, (int)$legal_entity_id);
            $grade = get_hrm_grade_by_legacy_grade($id, true);
            if (!$projection || !is_array($grade)
                || !hrm_grade_row_matches_projection($grade, $projection))
                return false;
        }
        return $rows;
    }

    /** Return normalized projection field names changed by this legacy update. */
    private static function projection_changed_fields($before, $after) {
        $map = array(
            'grade_code'=>'legacy_grade_code',
            'grade_name'=>'grade_name',
            'grade_level'=>'grade_level',
            'inactive'=>'grade_status',
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

    public static function create($data) {
        if (!hrm_pay_grade_writer_authorized())
            return false;
        begin_transaction();
        $e = new static();
        $existing = static::lock_projection_state(true);
        $result = is_array($existing) && $e->insert($data);
        $legacy_grade_id = $result ? (int)db_insert_id() : 0;
        if ($result && hrm_grade_table_ready())
            $result = synchronize_hrm_grade_from_pay_grade(
                $legacy_grade_id,
                array('grade_name','legacy_grade_code','grade_level','grade_status'),
                'create'
            );
        if ($result)
            commit_transaction();
        else
            cancel_transaction();
        return (bool)$result;
    }

    public static function modify($key, $data) {
        if (!hrm_pay_grade_writer_authorized() || (int)$key <= 0)
            return false;
        begin_transaction();
        $e = new static();
        $rows = static::lock_projection_state(true);
        $before = is_array($rows) && isset($rows[(int)$key]) ? $rows[(int)$key] : false;
        $changed = $before ? static::projection_changed_fields($before, $data) : array();
        $result = $before !== false && $e->update($key, $data);
        if ($result && hrm_grade_table_ready() && !empty($changed))
            $result = synchronize_hrm_grade_from_pay_grade((int)$key, $changed, 'update');
        if ($result && hrm_grade_table_ready())
            $result = is_array(static::lock_projection_state(true));
        if ($result)
            commit_transaction();
        else
            cancel_transaction();
        return (bool)$result;
    }

    public static function set_inactive($grade_id, $inactive) {
        if (!hrm_pay_grade_writer_authorized() || (int)$grade_id <= 0)
            return false;
        begin_transaction();
        $rows = static::lock_projection_state(true);
        $current = is_array($rows) && isset($rows[(int)$grade_id])
            ? (int)$rows[(int)$grade_id]['inactive'] : -1;
        $inactive = $inactive ? 1 : 0;
        $result = $current !== -1;
        if ($result && $current !== $inactive)
            $result = db_query("UPDATE ".TB_PREF."pay_grades SET inactive=".$inactive
                ." WHERE grade_id=".(int)$grade_id, 'could not update Pay Grade inactive status');
        if ($result && $current !== $inactive && hrm_grade_table_ready())
            $result = synchronize_hrm_grade_from_pay_grade(
                (int)$grade_id, array('grade_status'), 'inactive'
            );
        if ($result)
            commit_transaction();
        else
            cancel_transaction();
        return (bool)$result;
    }

    public static function remove($key) {
        if (!hrm_pay_grade_writer_authorized())
            return false;
        if (hrm_grade_table_ready())
            return false;
        $e = new static();
        return $e->delete($key);
    }
}

/** Route inactive checkbox mutation through the synchronized Grade writer. */
function hrm_pay_grade_inactive_control_cell($grade_id, $value) {
    $name = 'Inactive'.$grade_id;
    $value = $value ? 1 : 0;
    if (check_value('show_inactive')) {
        if (isset($_POST['LInact'][$grade_id])
            && (get_post('_Inactive'.$grade_id.'_update') || get_post('Update'))
            && (check_value('Inactive'.$grade_id) != $value)) {
            if (!grades_entity::set_inactive($grade_id, !$value))
                display_error(_('The Pay Grade status could not be synchronized.'));
        }
        echo '<td align="center">'.checkbox(null, $name, $value, true, '')
            .hidden("LInact[$grade_id]", $value, false).'</td>';
    }
}
