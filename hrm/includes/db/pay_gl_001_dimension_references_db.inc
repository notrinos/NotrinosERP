<?php
/** PAY-GL-owned, presence-only native allocation observation (DIM-LFC-001B3c1). */
require_once dirname(dirname(dirname(__DIR__))).'/dimensions/includes/dimension_lifecycle_db.inc';

/**
 * Observe a retained native allocation without releasing payroll facts.
 * Projection uses the immutable line's mapping-rule hash and axis code, rather
 * than assuming an arbitrary numeric allocation key is a core Dimension ID.
 * A retained native slot hint also blocks, even with missing/corrupt parents.
 * Only SELECT 1 reaches PHP: no axes, salaries, result keys or people escape.
 * Absence cannot attest the other protected payload/custody owners, so remains
 * unknown. This never validates posting, certifies hashes or authorizes delete.
 *
 * @param int|string $id Exact native identity in the active company
 * @return string used or unknown, including authorization/schema failure
 */
function pay_gl_001_observe_dimension_references($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id)) return 'unknown';
    $sql = 'SELECT 1 FROM '.TB_PREF.'hrm_pay_gl_001_allocation_axes ax'
        .' LEFT JOIN '.TB_PREF.'hrm_pay_gl_001_allocations a ON a.allocation_id=ax.allocation_id'
        .' LEFT JOIN '.TB_PREF.'hrm_pay_gl_001_journal_lines l ON l.journal_line_id=a.journal_line_id'
        .' LEFT JOIN '.TB_PREF.'hrm_pay_gl_001_mapping_rules r ON r.rule_sha256=l.mapping_rule_sha256'
        .' WHERE BINARY ax.axis_value_key=BINARY '.db_escape((string)$id)
        .' AND (ax.core_slot_hint IN (1,2) OR ax.axis_code=r.dimension1_axis_code'
        .' OR ax.axis_code=r.dimension2_axis_code) LIMIT 1';
    try {
        $rows = db_query($sql);
        return $rows && db_num_rows($rows) ? 'used' : 'unknown';
    } catch (Exception $exception) {
        return 'unknown';
    }
}
