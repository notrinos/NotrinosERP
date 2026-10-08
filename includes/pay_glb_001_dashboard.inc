<?php
/**
 * PAY-GLB-001 S06 — escaped, bounded OFFLINE read-only HTML preview.
 * It has no route/session/SQL/IO. A future route MUST authorize company,
 * legal entities and residency independently BEFORE gathering input.
 */
require_once __DIR__.'/pay_glb_001_snapshot.inc';

/** Escape one normalized display value for UTF-8 HTML text. */
function pay_glb_001_html($value)
{
    return htmlspecialchars((string)$value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

/**
 * Recompute from strict normalized, independently-authorized source inputs;
 * do not trust/render a caller-supplied snapshot or an arbitrary HTML field.
 * @return string|false Escaped HTML fragment (no user controls or script).
 */
function pay_glb_001_render_dashboard($ctx, $rollup_rows, $events, $exception_rows)
{
    $snapshot = pay_glb_001_project_snapshot($ctx, $rollup_rows, $events, $exception_rows);
    if ($snapshot === false) return false;
    $esc = 'pay_glb_001_html';
    $out = '<section aria-label="Payroll operations offline preview">';
    $out .= '<h2>Multi-country payroll operations — offline preview</h2>';
    $out .= '<p>Read-only internal aggregate preview. Not live provider evidence or legal compliance certification.</p>';
    $out .= '<p>As of UTC: '.$esc($snapshot['as_of_utc']).' | Snapshot change detector: '.
        $esc($snapshot['projection_sha256']).' (not a signature)</p>';
    $out .= '<table><caption>Scoped entity aggregates</caption><thead><tr>'.
        '<th>Entity</th><th>Country</th><th>Delivery model</th><th>Operational signal</th>'.
        '<th>Reconciliation signal</th><th>Source age</th></tr></thead><tbody>';
    foreach ($snapshot['rollup']['country_entity_rows'] as $row) {
        $cells = array($row['entity_code'],$row['country_code'],$row['delivery_model'],
            $row['status'],$row['reconciliation'],$row['stale'] ? 'STALE' : 'FRESH');
        $out .= '<tr>';
        foreach ($cells as $value) $out .= '<td>'.$esc($value).'</td>';
        $out .= '</tr>';
    }
    $out .= '</tbody></table><table><caption>Reported aggregate exceptions — no case drilldown</caption>'.
        '<thead><tr><th>Entity</th><th>Provenance</th><th>Open</th><th>Under review</th>'.
        '<th>Reported resolved</th></tr></thead><tbody>';
    foreach ($snapshot['exceptions']['entries'] as $row) {
        $cells = array($row['entity_code'],$row['source_state'],
            $row['reported_open']===null?'NOT CURRENT':$row['reported_open'],
            $row['reported_review']===null?'NOT CURRENT':$row['reported_review'],
            $row['reported_resolved']===null?'NOT CURRENT':$row['reported_resolved']);
        $out .= '<tr>';
        foreach ($cells as $value) $out .= '<td>'.$esc($value).'</td>';
        $out .= '</tr>';
    }
    $out .= '</tbody></table><table><caption>Informational operational review calendar — not statutory deadlines</caption>'.
        '<thead><tr><th>Entity</th><th>Kind</th><th>Local timestamp with offset</th>'.
        '<th>Provenance</th></tr></thead><tbody>';
    foreach ($snapshot['calendar']['entries'] as $event) {
        $cells = array($event['entity_code'],$event['deadline_kind'],
            $event['deadline_local'],$event['provenance']);
        $out .= '<tr>';
        foreach ($cells as $value) $out .= '<td>'.$esc($value).'</td>';
        $out .= '</tr>';
    }
    return $out.'</tbody></table></section>';
}
?>
