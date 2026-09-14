<?php
/**
 * HRM-FND-007 explicit Assignment Work Authorization jurisdiction custody.
 *
 * Jurisdiction is never inferred from nationality, passport, address, work
 * location, legal entity, employer text, or authorization data. The only
 * production binding source released by this slice is an explicit Employee
 * Transfer request. Binding is immutable per Assignment and must occur inside
 * the caller transaction so a successor Assignment cannot commit without its
 * governed jurisdiction custody.
 */
if (!function_exists('hrm_work_authorization_validate_jurisdiction'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

if (!defined('HRM_ASSIGNMENT_WA_JURISDICTION_SOURCE_TRANSFER'))
    define('HRM_ASSIGNMENT_WA_JURISDICTION_SOURCE_TRANSFER', 'employee_transfer_explicit');

function hrm_assignment_work_authorization_jurisdiction_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

function hrm_assignment_work_authorization_jurisdiction_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_EMPTRANSFER');
}

function hrm_assignment_work_authorization_jurisdiction_source_valid($source)
{
    return is_string($source) && $source === HRM_ASSIGNMENT_WA_JURISDICTION_SOURCE_TRANSFER;
}

function get_hrm_assignment_work_authorization_jurisdiction($assignment_id, $for_update = false)
{
    $assignment_id = (int)$assignment_id;
    if ($assignment_id <= 0 || !hrm_assignment_work_authorization_jurisdiction_permission())
        return false;
    global $transaction_level;
    if ($for_update && empty($transaction_level))
        return false;
    $sql = 'SELECT assignment_id,issuing_jurisdiction,binding_source,row_version,created_at,created_by '
        .'FROM '.TB_PREF.'hrm_assignment_work_authorization_jurisdictions WHERE assignment_id='.$assignment_id
        .' LIMIT 1'.($for_update ? ' FOR UPDATE' : '');
    $result = db_query($sql, 'could not read Assignment Work Authorization jurisdiction custody');
    if (!$result || db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row || (int)$row['assignment_id'] !== $assignment_id
        || !hrm_work_authorization_validate_jurisdiction((string)$row['issuing_jurisdiction'])
        || !hrm_assignment_work_authorization_jurisdiction_source_valid((string)$row['binding_source'])
        || (int)$row['row_version'] !== 1 || (int)$row['created_by'] <= 0)
        return false;
    return array(
        'assignment_id'=>$assignment_id,
        'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],
        'binding_source'=>(string)$row['binding_source'],
        'row_version'=>1,
        'created_at'=>(string)$row['created_at'],
        'created_by'=>(int)$row['created_by'],
    );
}

/**
 * Bind exactly one current primary Assignment to an explicit jurisdiction.
 * Exact retries are accepted; rebinding an Assignment is denied.
 */
function bind_hrm_assignment_work_authorization_jurisdiction($assignment_id, $jurisdiction, $source)
{
    global $transaction_level;
    $assignment_id = (int)$assignment_id;
    $actor = hrm_assignment_work_authorization_jurisdiction_actor_id();
    if (empty($transaction_level) || $assignment_id <= 0 || $actor <= 0
        || !hrm_assignment_work_authorization_jurisdiction_permission()
        || !hrm_work_authorization_validate_jurisdiction($jurisdiction)
        || !hrm_assignment_work_authorization_jurisdiction_source_valid($source))
        return false;

    $r = db_query('SELECT a.assignment_id,a.employment_id,a.legacy_employee_number,e.worker_id,e.legal_entity_id '
        .'FROM '.TB_PREF.'hrm_assignments a INNER JOIN '.TB_PREF.'hrm_employments e '
        .'ON e.employment_id=a.employment_id WHERE a.assignment_id='.$assignment_id
        ." AND a.legacy_current=1 AND a.primary_flag=1 AND a.assignment_type_code='legacy_primary'"
        .' AND e.legacy_current=1 LIMIT 2 FOR UPDATE',
        'could not lock Assignment jurisdiction predecessor custody');
    if (!$r || db_num_rows($r) !== 1)
        return false;
    $assignment = db_fetch_assoc($r);
    if (!$assignment || (int)$assignment['employment_id'] <= 0 || (int)$assignment['worker_id'] <= 0
        || (int)$assignment['legacy_employee_number'] <= 0 || (int)$assignment['legal_entity_id'] < 0)
        return false;

    $existing = get_hrm_assignment_work_authorization_jurisdiction($assignment_id, true);
    if (is_array($existing))
        return (string)$existing['issuing_jurisdiction'] === (string)$jurisdiction
            && (string)$existing['binding_source'] === (string)$source;

    $check = db_query('SELECT COUNT(*) n FROM '.TB_PREF.'hrm_assignment_work_authorization_jurisdictions '
        .'WHERE assignment_id='.$assignment_id, 'could not verify Assignment jurisdiction custody absence');
    $check_row = $check ? db_fetch_assoc($check) : false;
    if (!$check_row || (int)$check_row['n'] !== 0)
        return false;

    if (!db_query('INSERT INTO '.TB_PREF.'hrm_assignment_work_authorization_jurisdictions '
        .'(assignment_id,issuing_jurisdiction,binding_source,row_version,created_by) VALUES ('
        .$assignment_id.','.db_escape($jurisdiction).','.db_escape($source).',1,'.$actor.')',
        'could not bind Assignment Work Authorization jurisdiction'))
        return false;

    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$assignment['legal_entity_id'],
        'entity_type'=>'hrm_assignment_work_authorization_jurisdiction',
        'entity_key'=>array('assignment_id'=>$assignment_id,'employment_id'=>(int)$assignment['employment_id'],
            'worker_id'=>(int)$assignment['worker_id']),
        'action'=>'hrm.assignment.work_authorization_jurisdiction.bound',
        'reason_code'=>'hrm_fnd_007_assignment_work_authorization_jurisdiction_binding',
        'before'=>array(),
        'after'=>array('row_version'=>1,'binding_source'=>(string)$source),
        'restricted_diff'=>array(
            'explicit_jurisdiction_supplied'=>true,
            'jurisdiction_inferred_from_nationality'=>false,
            'jurisdiction_inferred_from_passport'=>false,
            'jurisdiction_inferred_from_work_location'=>false,
            'jurisdiction_inferred_from_legal_entity'=>false,
            'sensitive_plaintext_logged'=>false,
            'payroll_authority_changed'=>false,
            'historical_payroll_accounting_rows_rewritten'=>0,
        ),
    )) !== false;
}
