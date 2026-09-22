<?php
/** HRM-CMP-001 legacy salary compatibility guard; deliberately dependency-light. */
function hrm_cmp_001_legacy_tables_ready()
{
    return function_exists('employee_table_exists') && employee_table_exists('hrm_comp_cutover_receipts');
}
function hrm_cmp_001_projection_context_matches($employee_id)
{
    if (empty($GLOBALS['hrm_cmp_001_projection_context']) || !is_array($GLOBALS['hrm_cmp_001_projection_context'])) return false;
    $c = $GLOBALS['hrm_cmp_001_projection_context'];
    return isset($c['employee_id']) && (string)$c['employee_id'] === (string)$employee_id && !empty($c['authorized']);
}
function hrm_cmp_001_employee_is_cutover($employee_id)
{
    if (!hrm_cmp_001_legacy_tables_ready()) return false;
    $q = db_query('SELECT cutover_receipt_id FROM '.TB_PREF.'hrm_comp_cutover_receipts WHERE employee_id='.db_escape((string)$employee_id).' LIMIT 2');
    if (!$q) return true; // Once CMP custody exists, unreadable cutover state denies legacy mutation.
    return db_num_rows($q) > 0;
}
function hrm_cmp_001_legacy_salary_write_allowed($employee_id)
{
    return !hrm_cmp_001_employee_is_cutover($employee_id) || hrm_cmp_001_projection_context_matches($employee_id);
}
function hrm_cmp_001_legacy_salary_revision_allowed($employee_id)
{
    return !hrm_cmp_001_employee_is_cutover($employee_id);
}
function hrm_cmp_001_with_projection_context($employee_id, $assignment_id, $callable)
{
    $old = isset($GLOBALS['hrm_cmp_001_projection_context']) ? $GLOBALS['hrm_cmp_001_projection_context'] : null;
    $GLOBALS['hrm_cmp_001_projection_context'] = array('authorized'=>true,'employee_id'=>(string)$employee_id,'assignment_id'=>(int)$assignment_id);
    try { return $callable(); }
    finally { if ($old === null) unset($GLOBALS['hrm_cmp_001_projection_context']); else $GLOBALS['hrm_cmp_001_projection_context'] = $old; }
}
function hrm_cmp_001_employee_projection_authority($before, $candidate, $assignment_change = null)
{
    if (empty($GLOBALS['hrm_cmp_001_projection_context']) || !is_array($GLOBALS['hrm_cmp_001_projection_context']) || $assignment_change !== null) return false;
    $c = $GLOBALS['hrm_cmp_001_projection_context'];
    $employee_id = isset($before['employee_id']) ? (string)$before['employee_id'] : '';
    if ($employee_id === '' || !hrm_cmp_001_projection_context_matches($employee_id)) return false;
    if (!isset($candidate['personal_salary']) || (int)$candidate['personal_salary'] !== 1) return false;
    foreach ($candidate as $k=>$v) {
        if ($k === 'personal_salary') continue;
        if (!array_key_exists($k, $before) || (string)$before[$k] !== (string)$v) return false;
    }
    return true;
}
?>
