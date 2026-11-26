<?php
/** PAY-CTRY-007 exact zero-seed append-only catalog, recipient lifecycle and delivery-outbox schema contract. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');
include_once($path_to_root.'/includes/pay_ctry_007_contract.inc');
function pay_ctry_007_schema_columns(){return array(
'pay_ctry_catalog_releases'=>array('catalog_release_id','catalog_code','release_no','predecessor_catalog_release_id','predecessor_release_sha256','catalog_root_sha256','released_at','publisher_id','signer_id','trust_key_id','signature_algorithm','signature_payload_sha256','signature_base64','release_sha256','received_by','created_at'),
'pay_ctry_catalog_release_admissions'=>array('catalog_admission_id','catalog_release_id','decision','signature_verified','trust_snapshot_sha256','evidence_sha256','maker_id','checker_id','decided_at','admission_sha256','created_at'),
'pay_ctry_update_notices'=>array('update_notice_id','notice_key','catalog_release_id','pack_code','current_pack_version','current_semantic_root','current_evidence_root','successor_pack_version','successor_semantic_root','successor_evidence_root','severity','deadline_at','required_action','applicability_sha256','impact_sha256','dedupe_key','notice_sha256','issued_at','created_by','created_at'),
'pay_ctry_update_notice_applicability'=>array('notice_applicability_id','update_notice_id','company_id','scope_type','legal_entity_id','payroll_scope_key','current_binding_sha256','run_snapshot_sha256','affected_open_run_count','affected_future_run_count','impact_sha256','applicability_sha256','created_at'),
'pay_ctry_update_notice_audiences'=>array('notice_audience_id','update_notice_id','notice_applicability_id','audience_key','audience_type','audience_scope_sha256','recipient_set_sha256','audience_sha256','selected_by','selected_at','created_at'),
'pay_ctry_update_notice_recipient_events'=>array('recipient_event_id','notice_audience_id','update_notice_id','notice_applicability_id','recipient_user_id','recipient_event_key','event_no','predecessor_recipient_event_id','predecessor_recipient_event_sha256','event_type','reason_code','recipient_event_sha256','acted_by','occurred_at','created_at'),
'pay_ctry_update_notice_acknowledgements'=>array('notice_acknowledgement_id','recipient_event_id','update_notice_id','notice_applicability_id','recipient_user_id','acknowledgement_type','acknowledged_at','acknowledgement_sha256','actor_id','created_at'),
'pay_ctry_update_notice_lifecycle_events'=>array('notice_lifecycle_event_id','update_notice_id','notice_applicability_id','recipient_event_id','lifecycle_event_key','event_type','due_at','reason_code','evidence_sha256','lifecycle_sha256','actor_id','occurred_at','created_at'),
'pay_ctry_update_notice_delivery_outbox'=>array('delivery_outbox_id','recipient_event_id','update_notice_id','notice_applicability_id','recipient_user_id','outbox_key','channel','payload_sha256','outbox_sha256','queued_by','queued_at','created_at'),
'pay_ctry_update_notice_delivery_attempts'=>array('delivery_attempt_id','delivery_outbox_id','attempt_no','provider_code','delivery_status','result_sha256','error_code','delivery_attempt_sha256','attempted_by','attempted_at','created_at'));
}
function pay_ctry_007_required_indexes(){return array(
'pay_ctry_catalog_releases'=>array('uq_pct7_cr_release'=>array(1,array('catalog_code','release_no')),'uq_pct7_cr_sha'=>array(1,
	array('release_sha256')),'uq_pct7_cr_pred'=>array(1,array('predecessor_catalog_release_id')),'ix_pct7_cr_root'=>array(0,
	array('catalog_root_sha256','released_at')),'fk_pct7_cr_publisher'=>array(0,array('publisher_id')),'fk_pct7_cr_signer'=>array(0,
	array('signer_id')),'fk_pct7_cr_key'=>array(0,array('trust_key_id'))),
'pay_ctry_catalog_release_admissions'=>array('uq_pct7_ca_release'=>array(1,array('catalog_release_id')),'uq_pct7_ca_sha'=>array(1,array('admission_sha256')),'ix_pct7_ca_checker'=>array(0,array('checker_id','decided_at'))),
'pay_ctry_update_notices'=>array('uq_pct7_un_key'=>array(1,array('notice_key')),'uq_pct7_un_dedupe'=>array(1,array('dedupe_key')),'uq_pct7_un_sha'=>array(1,array('notice_sha256')),'ix_pct7_un_pack'=>array(0,array('pack_code','current_pack_version','successor_pack_version')),'ix_pct7_un_deadline'=>array(0,array('severity','deadline_at')),'fk_pct7_un_release'=>array(0,array('catalog_release_id'))),
'pay_ctry_update_notice_applicability'=>array('uq_pct7_na_sha'=>array(1,array('applicability_sha256')),'ix_pct7_na_notice'=>array(0,array('update_notice_id','company_id')),'ix_pct7_na_scope'=>array(0,array('company_id','scope_type','legal_entity_id','payroll_scope_key'))),
'pay_ctry_update_notice_audiences'=>array('uq_pct7_au_key'=>array(1,array('audience_key')),'uq_pct7_au_sha'=>array(1,array('audience_sha256')),'ix_pct7_au_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'ix_pct7_au_type'=>array(0,array('audience_type','selected_at')),'fk_pct7_au_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_recipient_events'=>array('uq_pct7_re_key'=>array(1,array('recipient_event_key')),'uq_pct7_re_stream'=>array(1,
	array('notice_audience_id','recipient_user_id','event_no')),'uq_pct7_re_pred'=>array(1,array('predecessor_recipient_event_id')),
	'uq_pct7_re_sha'=>array(1,array('recipient_event_sha256')),'ix_pct7_re_user'=>array(0,array('recipient_user_id','occurred_at')),
	'ix_pct7_re_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'fk_pct7_re_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_acknowledgements'=>array('uq_pct7_ack_recipient'=>array(1,array('recipient_event_id')),'uq_pct7_ack_sha'=>array(1,array('acknowledgement_sha256')),'ix_pct7_ack_user'=>array(0,array('recipient_user_id','acknowledged_at')),'ix_pct7_ack_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'fk_pct7_ack_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_lifecycle_events'=>array('uq_pct7_le_key'=>array(1,array('lifecycle_event_key')),'uq_pct7_le_sha'=>array(1,
	array('lifecycle_sha256')),'ix_pct7_le_notice'=>array(0,array('update_notice_id','notice_applicability_id','occurred_at')),
	'ix_pct7_le_recipient'=>array(0,array('recipient_event_id','event_type','occurred_at')),'fk_pct7_le_app'=>array(0,array('notice_applicability_id'))),
'pay_ctry_update_notice_delivery_outbox'=>array('uq_pct7_ob_key'=>array(1,array('outbox_key')),'uq_pct7_ob_recipient_channel'=>array(1,
	array('recipient_event_id','channel')),'uq_pct7_ob_sha'=>array(1,array('outbox_sha256')),'ix_pct7_ob_user'=>array(0,array('recipient_user_id',
	'channel','queued_at')),'ix_pct7_ob_notice'=>array(0,array('update_notice_id','notice_applicability_id')),'fk_pct7_ob_app'=>array(0,
	array('notice_applicability_id'))),
'pay_ctry_update_notice_delivery_attempts'=>array('uq_pct7_da_attempt'=>array(1,array('delivery_outbox_id','attempt_no')),'uq_pct7_da_sha'=>array(1,array('delivery_attempt_sha256')),'ix_pct7_da_status'=>array(0,array('delivery_outbox_id','delivery_status','attempted_at'))));}
/**
 * Normalize information-schema rows to stable PAY-CTRY-007 index attributes.
 *
 * @param array $rows Rows containing index metadata.
 * @return array|false Index-name map with uniqueness, type, ordered columns and prefix lengths.
 */
function pay_ctry_007_canonical_index_projection(array $rows)
{
    $indexes = array();
    foreach ($rows as $row) {
        if (!isset($row['INDEX_NAME'], $row['NON_UNIQUE'], $row['SEQ_IN_INDEX'], $row['COLUMN_NAME'], $row['INDEX_TYPE'])) {
            if (isset($row['INDEX_NAME']) && strcasecmp((string)$row['INDEX_NAME'], 'PRIMARY') === 0) {
                continue;
            }
            return false;
        }
        $name = (string)$row['INDEX_NAME'];
        if (strcasecmp($name, 'PRIMARY') === 0) {
            continue;
        }
        $sequence = (int)$row['SEQ_IN_INDEX'];
        $nonUnique = (int)$row['NON_UNIQUE'];
        $column = (string)$row['COLUMN_NAME'];
        $type = strtoupper(trim((string)$row['INDEX_TYPE']));
        $prefix = $row['SUB_PART'] === null || $row['SUB_PART'] === '' ? null : (int)$row['SUB_PART'];
        if ($name === '' || $sequence < 1 || ($nonUnique !== 0 && $nonUnique !== 1)
            || $column === '' || $type === '' || ($prefix !== null && $prefix < 1)) {
            return false;
        }
        $unique = $nonUnique === 0 ? 1 : 0;
        if (!isset($indexes[$name])) {
            $indexes[$name] = array('unique'=>$unique, 'type'=>$type, 'columns'=>array(), 'prefixes'=>array());
        } elseif ($indexes[$name]['unique'] !== $unique || $indexes[$name]['type'] !== $type) {
            return false;
        }
        if (isset($indexes[$name]['columns'][$sequence])) {
            return false;
        }
        $indexes[$name]['columns'][$sequence] = $column;
        $indexes[$name]['prefixes'][$sequence] = $prefix;
    }

    ksort($indexes, SORT_STRING);
    $projection = array();
    foreach ($indexes as $name=>$index) {
        ksort($index['columns'], SORT_NUMERIC);
        ksort($index['prefixes'], SORT_NUMERIC);
        $expectedSequence = 1;
        foreach ($index['columns'] as $sequence=>$column) {
            if ((int)$sequence !== $expectedSequence) {
                return false;
            }
            $expectedSequence++;
        }
        $projection[$name] = array(
            $index['unique'],
            $index['type'],
            array_values($index['columns']),
            array_values($index['prefixes'])
        );
    }
    return $projection;
}

?>
