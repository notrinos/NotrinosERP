<?php
/** DIM-LFC-001B3b: authorized observations through fixed, source-owned readers. */
require_once __DIR__.'/dimension_reference_registry.inc';
require_once __DIR__.'/dimension_reference_metadata.inc';

/**
 * Observe scalar, metadata and characterized approval owners in the active company.
 * Deny before any reader/query; never accept caller attestations or cache across
 * requests/companies. Protected payroll, retained/session and extension domains
 * remain unknown until their own complete authority and writer fences exist.
 * This is an advisory snapshot, never an unused certificate or delete authority.
 *
 * @param int|string $id Exact positive native identity
 * @return array Owner names/states only; deletion_authorized is always false
 */
function dimension_reference_db_observe($id)
{
    if (!dimension_lifecycle_authorized() || !dimension_lifecycle_valid_id($id))
        return dimension_reference_observe(0, null);
    $approval = array('approval_drafts'=>'unknown', 'approval_actions'=>'unknown');
    if (function_exists('get_approval_workflow_service')) {
        $service = get_approval_workflow_service();
        if (is_object($service) && method_exists($service, 'observeDimensionReferences'))
            $approval = $service->observeDimensionReferences($id);
    }
    $metadata = dimension_reference_metadata_observe($id);
    return dimension_reference_observe((int)$id, function($name, $owner, $identity) use ($approval, $metadata) {
        if ($owner['kind'] === 'scalar') {
            $sql = dimension_reference_owner_sql($name, $identity);
            if ($sql === false) return 'unknown';
            $rows = db_query($sql);
            return $rows ? (db_num_rows($rows) ? 'used' : 'unused') : 'unknown';
        }
        if (isset($approval[$name])) return $approval[$name];
        if ($name === 'tags') return $metadata['state'];
        return 'unknown';
    });
}
