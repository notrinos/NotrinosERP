<?php
/**
 * PAY-GLB-001 S09: non-routable orchestration from a verified current session,
 * an independently supplied per-object/residency decision and a strictly
 * normalized server-origin aggregate batch. NO ROUTE OR COLLECTOR IS WIRED.
 * Safe to integrate ONLY after independent approval of actual source and ACL.
 */
require_once __DIR__.'/pay_glb_001_authority.inc';
require_once __DIR__.'/pay_glb_001_collector_boundary.inc';
require_once __DIR__.'/pay_glb_001_dashboard.inc';

/** Render only after session, trusted grant scope and complete aggregate validation. */
function pay_glb_001_guarded_preview($trusted_entity_grants, $as_of_utc, $server_batch)
{
    $scope = pay_glb_001_server_scope($trusted_entity_grants, $as_of_utc);
    if ($scope === false ||
        pay_glb_001_validate_collected_batch($scope, $server_batch) === false) return false;
    // Recalculate (rather than render a caller-supplied snapshot) under the
    // exact checked scope. All dynamic HTML remains escaped by the renderer.
    return pay_glb_001_render_dashboard($scope, $server_batch['rollup_rows'],
        $server_batch['calendar_events'], $server_batch['exception_rows']);
}
?>
