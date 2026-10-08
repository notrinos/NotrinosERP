<?php
/**
 * PAY-GLB-001 S03 — offline calendar projection, not legally authoritative.
 * Do not treat operational dates or unverified external reports as statutory
 * deadlines, or infer filing/submission/close/payment compliance.
 */
require_once __DIR__.'/pay_glb_001_rollup.inc';

/** Validate an operational event against the independently approved entity source. */
function pay_glb_001_calendar_event_valid($ctx, $event, $approved)
{
    if (!pay_glb_001_exact_keys($event, array('company_id','entity_code','country_code',
        'delivery_model','deadline_kind','deadline_at_utc','deadline_timezone',
        'source_observed_utc','source_expires_utc','evidence_sha256','provenance_state'))) return false;
    if (!is_int($event['company_id']) || $event['company_id']!==$ctx['company_id'] ||
        !pay_glb_001_code($event['entity_code']) || !isset($approved[$event['entity_code']])) return false;
    $row = $approved[$event['entity_code']];
    if ($event['country_code']!==$row['country_code'] ||
        $event['delivery_model']!==$row['delivery_model']) return false;
    if (!in_array($event['deadline_kind'], array('OPERATIONAL_CUTOFF','RECONCILIATION_REVIEW','COMPLIANCE_REVIEW'),true) ||
        !pay_glb_001_iso_utc($event['deadline_at_utc']) ||
        !is_string($event['deadline_timezone']) ||
        !in_array($event['deadline_timezone'], DateTimeZone::listIdentifiers(),true) ||
        !pay_glb_001_iso_utc($event['source_observed_utc']) ||
        !pay_glb_001_iso_utc($event['source_expires_utc']) ||
        !pay_glb_001_sha256($event['evidence_sha256']) ||
        !in_array($event['provenance_state'], array('REVIEWED_INTERNAL','UNVERIFIED_EXTERNAL'),true)) return false;
    if ($event['source_observed_utc'] > $ctx['as_of_utc'] ||
        $event['source_expires_utc'] <= $event['source_observed_utc']) return false;
    if (($event['delivery_model']==='NATIVE') !== ($event['provenance_state']==='REVIEWED_INTERNAL')) return false;
    // External provider source timestamps cannot certify an operative legal deadline.
    return true;
}

/** @return array|false Fully validated, deterministic and minimally scoped projection. */
function pay_glb_001_project_calendar($ctx, $rollup_rows, $events)
{
    if (!pay_glb_001_context_valid($ctx,'calendar') ||
        !is_array($events) || count($events)>64 ||
        array_keys($events)!==(count($events)?range(0,count($events)-1):array())) return false;
    $rollup = pay_glb_001_project_rollup($ctx,$rollup_rows);
    if ($rollup===false) return false;
    $approved=array();
    foreach ($rollup['country_entity_rows'] as $r) $approved[$r['entity_code']]=$r;
    $out=array();$seen=array();$stale=0;
    foreach ($events as $event) {
        if (!pay_glb_001_calendar_event_valid($ctx,$event,$approved)) return false;
        $key=$event['entity_code'].'|'.$event['deadline_kind'].'|'.$event['deadline_at_utc'];
        if (isset($seen[$key])) return false;
        $seen[$key]=true;
        $is_stale = $event['source_expires_utc'] <= $ctx['as_of_utc'] ||
            $approved[$event['entity_code']]['stale'];
        if ($is_stale) $stale++;
        $timezone = new DateTimeZone($event['deadline_timezone']);
        $utc = new DateTimeImmutable($event['deadline_at_utc']);
        $out[]=array(
            'entity_code'=>$event['entity_code'], 'country_code'=>$event['country_code'],
            'delivery_model'=>$event['delivery_model'], 'deadline_kind'=>$event['deadline_kind'],
            'deadline_at_utc'=>$event['deadline_at_utc'],
            'deadline_timezone'=>$event['deadline_timezone'],
            'deadline_local'=>$utc->setTimezone($timezone)->format('Y-m-d H:i:s P'),
            'time_relation'=>$event['deadline_at_utc']<=$ctx['as_of_utc']?'PAST_OR_NOW':'FUTURE',
            'evidence_sha256'=>$event['evidence_sha256'],
            'source_observed_utc'=>$event['source_observed_utc'],
            'source_expires_utc'=>$event['source_expires_utc'],
            'provenance'=>$is_stale?'STALE_UNVERIFIED':$event['provenance_state'],
            'legally_verified'=>false
        );
    }
    usort($out,function($a,$b){
        return strcmp($a['deadline_at_utc'].'|'.$a['entity_code'].'|'.$a['deadline_kind'],
                      $b['deadline_at_utc'].'|'.$b['entity_code'].'|'.$b['deadline_kind']);
    });
    return array('scope'=>'AUTHORIZED_OPERATIONAL_CALENDAR_ONLY',
        'as_of_utc'=>$ctx['as_of_utc'], 'entries'=>$out,
        'total_entries'=>count($out),'stale_or_unverified_source_age'=>$stale,
        'legal_deadline_or_submission_authority'=>false);
}
?>
