<?php
/** S39 exact additive custody metadata and interrupted-DDL resume guards. */
include_once(dirname(__FILE__).'/migration_readiness_db.inc');

/** Return exact columns, indexes, restrictive FKs and CHECKs for six tables. */
function pay_glb_001_native_schema_definitions()
{
    return array(
        array('name'=>'hrm_pay_glb_001_native_executions','code'=>'e',
            'columns'=>array(
                array('name'=>'native_execution_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'auto_increment','default'=>null),
                array('name'=>'company_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'legal_entity_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'payroll_group_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'group_version_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'payroll_period_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'calendar_period_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'source_kind','type'=>'varchar(32)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'producer_contract_version','type'=>'varchar(48)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'producer_event_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'request_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'native_execution_key','type'=>'varchar(128)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'revision_no','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'predecessor_execution_id','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'joint_core009_run_id','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'joint_core009_state_version','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'joint_core009_state_sha256','type'=>'char(64)','nullable'=>true,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'joint_producer_sha256','type'=>'char(64)','nullable'=>true,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'member_count','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'member_set_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'core010_result_set_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'gross_8','type'=>'decimal(38,8)','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'deduction_8','type'=>'decimal(38,8)','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'net_8','type'=>'decimal(38,8)','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'currency_code','type'=>'char(3)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'maker_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'produced_at_utc','type'=>'datetime','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'execution_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'receipt_json','type'=>'longtext','nullable'=>false,'charset'=>'utf8mb4_bin','extra'=>'','default'=>null),
                array('name'=>'created_at','type'=>'timestamp','nullable'=>false,'charset'=>null,'extra'=>'','default'=>'current_timestamp()')
            ),
            'indexes'=>array(array('name'=>'PRIMARY','columns'=>array('native_execution_id'),'unique'=>true),array('name'=>'uq_pgn_e_key','columns'=>array('native_execution_key'),'unique'=>true),array('name'=>'uq_pgn_e_sha','columns'=>array('execution_sha256'),'unique'=>true),array('name'=>'uq_pgn_e_revision','columns'=>array('company_id','legal_entity_id','payroll_group_id','payroll_period_id','revision_no'),'unique'=>true),array('name'=>'uq_pgn_e_predecessor','columns'=>array('predecessor_execution_id'),'unique'=>true),array('name'=>'uq_pgn_e_joint','columns'=>array('joint_core009_run_id'),'unique'=>true)),
            'fks'=>array(array('name'=>'fk_pgn_e_predecessor','column'=>'predecessor_execution_id','table'=>'hrm_pay_glb_001_native_executions','target'=>'native_execution_id'),array('name'=>'fk_pgn_e_joint','column'=>'joint_core009_run_id','table'=>'hrm_pay_core_009_runs','target'=>'run_id')),
            'checks'=>array('member_count > 0 AND revision_no > 0 AND maker_id > 0','gross_8 - deduction_8 = net_8','source_kind = \'GOVERNED_NATIVE_V1\' AND joint_core009_run_id IS NULL AND joint_core009_state_version IS NULL AND joint_core009_state_sha256 IS NULL AND joint_producer_sha256 IS NULL OR source_kind = \'JOINT_CORE009_V1\' AND joint_core009_run_id IS NOT NULL AND joint_core009_state_version IS NOT NULL AND joint_core009_state_sha256 IS NOT NULL AND joint_producer_sha256 IS NOT NULL','JSON_VALID(receipt_json)')),
        array('name'=>'hrm_pay_glb_001_native_members','code'=>'m',
            'columns'=>array(
                array('name'=>'native_member_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'auto_increment','default'=>null),
                array('name'=>'native_execution_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'ordinal_no','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'payroll_relationship_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'assignment_id','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'core004_snapshot_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'core004_result_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'snapshot_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'result_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'line_set_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'trace_set_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'joint_core009_run_item_id','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'joint_core009_document_sha256','type'=>'char(64)','nullable'=>true,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'cross_domain_parity_sha256','type'=>'char(64)','nullable'=>true,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'member_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'receipt_json','type'=>'longtext','nullable'=>false,'charset'=>'utf8mb4_bin','extra'=>'','default'=>null),
                array('name'=>'created_at','type'=>'timestamp','nullable'=>false,'charset'=>null,'extra'=>'','default'=>'current_timestamp()')
            ),
            'indexes'=>array(array('name'=>'PRIMARY','columns'=>array('native_member_id'),'unique'=>true),array('name'=>'uq_pgn_m_0','columns'=>array('native_execution_id','ordinal_no'),'unique'=>true),array('name'=>'uq_pgn_m_1','columns'=>array('native_execution_id','payroll_relationship_id'),'unique'=>true),array('name'=>'uq_pgn_m_2','columns'=>array('native_execution_id','core004_snapshot_id'),'unique'=>true),array('name'=>'uq_pgn_m_3','columns'=>array('native_execution_id','core004_result_id'),'unique'=>true),array('name'=>'uq_pgn_m_4','columns'=>array('native_execution_id','joint_core009_run_item_id'),'unique'=>true),array('name'=>'uq_pgn_m_sha','columns'=>array('member_sha256'),'unique'=>true),array('name'=>'ix_pgn_m_snapshot','columns'=>array('core004_snapshot_id'),'unique'=>false),array('name'=>'ix_pgn_m_result','columns'=>array('core004_result_id'),'unique'=>false),array('name'=>'ix_pgn_m_item','columns'=>array('joint_core009_run_item_id'),'unique'=>false)),
            'fks'=>array(array('name'=>'fk_pgn_m_execution','column'=>'native_execution_id','table'=>'hrm_pay_glb_001_native_executions','target'=>'native_execution_id'),array('name'=>'fk_pgn_m_snapshot','column'=>'core004_snapshot_id','table'=>'hrm_pay_core_004_snapshots','target'=>'snapshot_id'),array('name'=>'fk_pgn_m_result','column'=>'core004_result_id','table'=>'hrm_pay_core_004_results','target'=>'result_id'),array('name'=>'fk_pgn_m_item','column'=>'joint_core009_run_item_id','table'=>'hrm_pay_core_009_items','target'=>'run_item_id')),
            'checks'=>array('ordinal_no > 0','joint_core009_run_item_id IS NULL AND joint_core009_document_sha256 IS NULL AND cross_domain_parity_sha256 IS NULL OR joint_core009_run_item_id IS NOT NULL AND joint_core009_document_sha256 IS NOT NULL AND cross_domain_parity_sha256 IS NOT NULL','JSON_VALID(receipt_json)')),
        array('name'=>'hrm_pay_glb_001_native_selection_events','code'=>'s',
            'columns'=>array(
                array('name'=>'selection_event_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'auto_increment','default'=>null),
                array('name'=>'company_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'legal_entity_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'payroll_group_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'group_version_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'payroll_period_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'calendar_period_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'sequence_no','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'predecessor_selection_event_id','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'predecessor_selection_sha256','type'=>'char(64)','nullable'=>true,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'event_kind','type'=>'varchar(16)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'native_execution_id','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'reason_code','type'=>'varchar(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'maker_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'checker_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'command_key_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'selection_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'occurred_at_utc','type'=>'datetime','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'receipt_json','type'=>'longtext','nullable'=>false,'charset'=>'utf8mb4_bin','extra'=>'','default'=>null),
                array('name'=>'created_at','type'=>'timestamp','nullable'=>false,'charset'=>null,'extra'=>'','default'=>'current_timestamp()')
            ),
            'indexes'=>array(array('name'=>'PRIMARY','columns'=>array('selection_event_id'),'unique'=>true),array('name'=>'uq_pgn_s_seq','columns'=>array('company_id','legal_entity_id','payroll_group_id','payroll_period_id','sequence_no'),'unique'=>true),array('name'=>'uq_pgn_s_sha','columns'=>array('selection_sha256'),'unique'=>true),array('name'=>'uq_pgn_s_cmd','columns'=>array('company_id','legal_entity_id','payroll_group_id','payroll_period_id','command_key_sha256'),'unique'=>true),array('name'=>'uq_pgn_s_pred','columns'=>array('predecessor_selection_event_id'),'unique'=>true),array('name'=>'ix_pgn_s_execution','columns'=>array('native_execution_id'),'unique'=>false)),
            'fks'=>array(array('name'=>'fk_pgn_s_pred','column'=>'predecessor_selection_event_id','table'=>'hrm_pay_glb_001_native_selection_events','target'=>'selection_event_id'),array('name'=>'fk_pgn_s_execution','column'=>'native_execution_id','table'=>'hrm_pay_glb_001_native_executions','target'=>'native_execution_id')),
            'checks'=>array('sequence_no > 0 AND maker_id > 0 AND checker_id > 0 AND maker_id <> checker_id','event_kind = \'select\' AND native_execution_id IS NOT NULL OR event_kind IN (\'suspend\',\'revoke\') AND native_execution_id IS NULL','sequence_no = 1 AND predecessor_selection_event_id IS NULL AND predecessor_selection_sha256 IS NULL OR sequence_no > 1 AND predecessor_selection_event_id IS NOT NULL AND predecessor_selection_sha256 IS NOT NULL','JSON_VALID(receipt_json)')),
        array('name'=>'hrm_pay_glb_001_native_approval_bindings','code'=>'a',
            'columns'=>array(
                array('name'=>'approval_binding_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'auto_increment','default'=>null),
                array('name'=>'selection_event_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'native_execution_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'approval_intent_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'approval_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'intent_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'approval_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'result_set_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'projection_set_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'cutover_transition_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'maker_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'approver_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'binder_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'command_key_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'binding_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'receipt_json','type'=>'longtext','nullable'=>false,'charset'=>'utf8mb4_bin','extra'=>'','default'=>null),
                array('name'=>'created_at','type'=>'timestamp','nullable'=>false,'charset'=>null,'extra'=>'','default'=>'current_timestamp()')
            ),
            'indexes'=>array(array('name'=>'PRIMARY','columns'=>array('approval_binding_id'),'unique'=>true),array('name'=>'uq_pgn_a_sel','columns'=>array('selection_event_id'),'unique'=>true),array('name'=>'uq_pgn_a_approval','columns'=>array('approval_id'),'unique'=>true),array('name'=>'uq_pgn_a_sha','columns'=>array('binding_sha256'),'unique'=>true),array('name'=>'ix_pgn_a_execution','columns'=>array('native_execution_id'),'unique'=>false),array('name'=>'ix_pgn_a_intent','columns'=>array('approval_intent_id'),'unique'=>false),array('name'=>'ix_pgn_a_cmd','columns'=>array('command_key_sha256'),'unique'=>false)),
            'fks'=>array(array('name'=>'fk_pgn_a_sel','column'=>'selection_event_id','table'=>'hrm_pay_glb_001_native_selection_events','target'=>'selection_event_id'),array('name'=>'fk_pgn_a_execution','column'=>'native_execution_id','table'=>'hrm_pay_glb_001_native_executions','target'=>'native_execution_id'),array('name'=>'fk_pgn_a_intent','column'=>'approval_intent_id','table'=>'hrm_pay_core_010_approval_intents','target'=>'approval_intent_id'),array('name'=>'fk_pgn_a_approval','column'=>'approval_id','table'=>'hrm_pay_core_010_approvals','target'=>'approval_id')),
            'checks'=>array('maker_id > 0 AND approver_id > 0 AND binder_id > 0 AND maker_id <> approver_id','JSON_VALID(receipt_json)')),
        array('name'=>'hrm_pay_glb_001_native_gl_bindings','code'=>'g',
            'columns'=>array(
                array('name'=>'gl_binding_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'auto_increment','default'=>null),
                array('name'=>'approval_binding_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'journal_intent_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'reconciliation_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'review_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'posting_authority_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'posting_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'journal_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'approval_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'posting_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'reconciliation_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'posting_kind','type'=>'varchar(8)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'predecessor_posting_id','type'=>'bigint unsigned','nullable'=>true,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'native_gl_effect_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'command_key_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'gl_binding_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'bound_by','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'receipt_json','type'=>'longtext','nullable'=>false,'charset'=>'utf8mb4_bin','extra'=>'','default'=>null),
                array('name'=>'created_at','type'=>'timestamp','nullable'=>false,'charset'=>null,'extra'=>'','default'=>'current_timestamp()')
            ),
            'indexes'=>array(array('name'=>'PRIMARY','columns'=>array('gl_binding_id'),'unique'=>true),array('name'=>'uq_pgn_g_posting','columns'=>array('posting_id'),'unique'=>true),array('name'=>'uq_pgn_g_sha','columns'=>array('gl_binding_sha256'),'unique'=>true),array('name'=>'ix_pgn_g_approval','columns'=>array('approval_binding_id','posting_id'),'unique'=>false),array('name'=>'ix_pgn_g_0','columns'=>array('journal_intent_id'),'unique'=>false),array('name'=>'ix_pgn_g_1','columns'=>array('reconciliation_id'),'unique'=>false),array('name'=>'ix_pgn_g_2','columns'=>array('review_id'),'unique'=>false),array('name'=>'ix_pgn_g_3','columns'=>array('posting_authority_id'),'unique'=>false),array('name'=>'ix_pgn_g_4','columns'=>array('predecessor_posting_id'),'unique'=>false),array('name'=>'ix_pgn_g_5','columns'=>array('command_key_sha256'),'unique'=>false)),
            'fks'=>array(array('name'=>'fk_pgn_g_approval','column'=>'approval_binding_id','table'=>'hrm_pay_glb_001_native_approval_bindings','target'=>'approval_binding_id'),array('name'=>'fk_pgn_g_intent','column'=>'journal_intent_id','table'=>'hrm_pay_gl_001_journal_intents','target'=>'journal_intent_id'),array('name'=>'fk_pgn_g_recon','column'=>'reconciliation_id','table'=>'hrm_pay_gl_001_reconciliations','target'=>'reconciliation_id'),array('name'=>'fk_pgn_g_review','column'=>'review_id','table'=>'hrm_pay_gl_001_reviews','target'=>'review_id'),array('name'=>'fk_pgn_g_authority','column'=>'posting_authority_id','table'=>'hrm_pay_gl_001_posting_authorities','target'=>'authority_id'),array('name'=>'fk_pgn_g_posting','column'=>'posting_id','table'=>'hrm_pay_gl_001_gl_postings','target'=>'posting_id'),array('name'=>'fk_pgn_g_pred','column'=>'predecessor_posting_id','table'=>'hrm_pay_gl_001_gl_postings','target'=>'posting_id')),
            'checks'=>array('bound_by > 0','posting_kind = \'post\' AND predecessor_posting_id IS NULL OR posting_kind IN (\'reverse\',\'repost\') AND predecessor_posting_id IS NOT NULL','JSON_VALID(receipt_json)')),
        array('name'=>'hrm_pay_glb_001_native_command_receipts','code'=>'c',
            'columns'=>array(
                array('name'=>'command_receipt_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'auto_increment','default'=>null),
                array('name'=>'company_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'command_domain','type'=>'varchar(48)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'scope_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'command_key_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'expected_predecessor_sha256','type'=>'char(64)','nullable'=>true,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'request_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'effect_kind','type'=>'varchar(48)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'effect_id','type'=>'bigint unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'effect_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'actor_id','type'=>'int unsigned','nullable'=>false,'charset'=>null,'extra'=>'','default'=>null),
                array('name'=>'purpose_code','type'=>'varchar(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'command_sha256','type'=>'char(64)','nullable'=>false,'charset'=>'ascii_bin','extra'=>'','default'=>null),
                array('name'=>'request_context_json','type'=>'longtext','nullable'=>false,'charset'=>'utf8mb4_bin','extra'=>'','default'=>null),
                array('name'=>'receipt_json','type'=>'longtext','nullable'=>false,'charset'=>'utf8mb4_bin','extra'=>'','default'=>null),
                array('name'=>'created_at','type'=>'timestamp','nullable'=>false,'charset'=>null,'extra'=>'','default'=>'current_timestamp()')
            ),
            'indexes'=>array(array('name'=>'PRIMARY','columns'=>array('command_receipt_id'),'unique'=>true),array('name'=>'uq_pgn_c_replay','columns'=>array('company_id','command_domain','scope_sha256','command_key_sha256'),'unique'=>true),array('name'=>'uq_pgn_c_sha','columns'=>array('command_sha256'),'unique'=>true),array('name'=>'ix_pgn_c_effect','columns'=>array('effect_kind','effect_id'),'unique'=>false)),
            'fks'=>array(),
            'checks'=>array('actor_id > 0 AND effect_id > 0','JSON_VALID(request_context_json)','JSON_VALID(receipt_json)'))
    );
}

/** Normalize SQL formatting outside literals, preserving inner grouping. */
function pay_glb_001_native_schema_sql_normalize($sql)
{
    $parts = preg_split("/('(?:''|[^'])*')/", (string)$sql, -1, PREG_SPLIT_DELIM_CAPTURE);
    foreach ($parts as $index=>$part)
        if ($part === '' || $part[0] !== "'")
            $parts[$index] = strtolower(preg_replace('/\s+/', '', str_replace(chr(96), '', $part)));
    $text = implode('', $parts);
    while (strlen($text)>1 && $text[0]==='(' && substr($text,-1)===')') {
        $depth=0; $quoted=false; $enclosed=true;
        for ($i=0;$i<strlen($text)-1;$i++) {
            if ($text[$i]==="'") $quoted=!$quoted;
            if ($quoted) continue;
            if ($text[$i]==='(') $depth++;
            if ($text[$i]===')') $depth--;
            if ($depth===0) { $enclosed=false; break; }
        }
        if (!$enclosed) break;
        $text=substr($text,1,-1);
    }
    return $text;
}

/** Validate the existing active-prefix helper and physical name capacity. */
function pay_glb_001_native_schema_prefix()
{
    $prefix=hrm_migration_active_table_prefix();
    return $prefix!==false && strlen($prefix)<=16 ? $prefix : false;
}

/** Read metadata with existing DB wrappers and no authority assertions. */
function pay_glb_001_native_schema_rows($sql)
{
    $query=db_query($sql); if (!$query) return false;
    $rows=array(); while ($row=db_fetch_assoc($query)) $rows[]=$row;
    return $rows;
}

/** Compare a table's complete column/index/FK/CHECK/engine projection. */
function pay_glb_001_native_schema_table_valid($model,&$error=null)
{
    $error=null; $prefix=pay_glb_001_native_schema_prefix(); $quote=chr(96);
    if ($prefix===false) { $error='schema_prefix_invalid'; return false; }
    $physical=hrm_migration_physical_table_name($model['name']);
    if ($physical===false || !hrm_migration_table_exists($model['name'])) {
        $error='schema_table_missing'; return false;
    }
    $rows=pay_glb_001_native_schema_rows('SHOW FULL COLUMNS FROM '.$quote.$physical.$quote);
    if ($rows===false || count($rows)!==count($model['columns'])) {
        $error='schema_column_count'; return false;
    }
    foreach ($model['columns'] as $index=>$column) {
        $actual=$rows[$index];
        $type=preg_replace('/\b(tinyint|smallint|mediumint|int|bigint)\([0-9]+\)/','$1',strtolower($actual['Type']));
        $default=$actual['Default'];
        if (is_string($default) && strtolower($default)==='current_timestamp') $default='current_timestamp()';
        if ($actual['Field']!==$column['name'] || $type!==$column['type']
            || ($actual['Null']==='YES')!==$column['nullable']
            || $actual['Collation']!==$column['charset']
            || strtolower($actual['Extra'])!==$column['extra']
            || $default!==$column['default']) {
            $error='schema_column_projection:'.$column['name']; return false;
        }
    }
    $table=pay_glb_001_native_schema_rows("SELECT ENGINE,TABLE_COLLATION FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical));
    if (!$table || count($table)!==1 || strtolower($table[0]['ENGINE'])!=='innodb'
        || $table[0]['TABLE_COLLATION']!=='utf8mb4_general_ci') {
        $error='schema_engine_collation'; return false;
    }
    $rows=pay_glb_001_native_schema_rows('SHOW INDEX FROM '.$quote.$physical.$quote);
    if ($rows===false) { $error='schema_index_metadata'; return false; }
    $actual_indexes=array();
    foreach ($rows as $row) {
        $name=$row['Key_name'];
        if (!isset($actual_indexes[$name]))
            $actual_indexes[$name]=array('name'=>$name,'columns'=>array(),'unique'=>(int)$row['Non_unique']===0);
        if ($row['Sub_part']!==null || strtoupper($row['Index_type'])!=='BTREE') {
            $error='schema_index_prefix_or_type'; return false;
        }
        $actual_indexes[$name]['columns'][(int)$row['Seq_in_index']]=$row['Column_name'];
    }
    foreach ($actual_indexes as &$index) { ksort($index['columns']); $index['columns']=array_values($index['columns']); }
    unset($index); $expected_indexes=array();
    foreach ($model['indexes'] as $index) $expected_indexes[$index['name']]=$index;
    ksort($actual_indexes); ksort($expected_indexes);
    if ($actual_indexes!==$expected_indexes) { $error='schema_index_projection'; return false; }
    // MariaDB materializes a costly metadata join even with exact table filters.
    // Read each table's columns/rules separately and reconcile the same projection.
    $rows=pay_glb_001_native_schema_rows("SELECT CONSTRAINT_NAME,COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical)." AND REFERENCED_TABLE_NAME IS NOT NULL ORDER BY CONSTRAINT_NAME,ORDINAL_POSITION");
    if ($rows===false || count($rows)!==count($model['fks'])) { $error='schema_fk_count'; return false; }
    $rules=pay_glb_001_native_schema_rows("SELECT CONSTRAINT_NAME,UPDATE_RULE,DELETE_RULE FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS WHERE CONSTRAINT_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical));
    if ($rules===false || count($rules)!==count($model['fks'])) { $error='schema_fk_count'; return false; }
    $constraint_rules=array();
    foreach ($rules as $rule) {
        if (isset($constraint_rules[$rule['CONSTRAINT_NAME']])) { $error='schema_fk_projection'; return false; }
        $constraint_rules[$rule['CONSTRAINT_NAME']]=$rule;
    }
    $expected=array();
    foreach ($model['fks'] as $fk)
        $expected[$prefix.$fk['name']]=array('CONSTRAINT_NAME'=>$prefix.$fk['name'],'COLUMN_NAME'=>$fk['column'],
            'REFERENCED_TABLE_NAME'=>$prefix.$fk['table'],'REFERENCED_COLUMN_NAME'=>$fk['target'],
            'UPDATE_RULE'=>'RESTRICT','DELETE_RULE'=>'RESTRICT');
    foreach ($rows as $row) {
        $name=$row['CONSTRAINT_NAME'];
        if (!isset($constraint_rules[$name])) { $error='schema_fk_projection'; return false; }
        $row['UPDATE_RULE']=$constraint_rules[$name]['UPDATE_RULE'];
        $row['DELETE_RULE']=$constraint_rules[$name]['DELETE_RULE'];
        if (!isset($expected[$name]) || $row!==$expected[$name]) {
            $error='schema_fk_projection'; return false;
        }
    }
    $rows=pay_glb_001_native_schema_rows("SELECT CONSTRAINT_NAME,CHECK_CLAUSE FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA=DATABASE() AND TABLE_NAME=".db_escape($physical));
    if ($rows===false || count($rows)!==count($model['checks'])) { $error='schema_check_count'; return false; }
    $expected=array();
    foreach ($model['checks'] as $index=>$check)
        $expected[$prefix.'ck_pgn_'.$model['code'].'_'.$index]=pay_glb_001_native_schema_sql_normalize($check);
    foreach ($rows as $row)
        if (!isset($expected[$row['CONSTRAINT_NAME']])
            || pay_glb_001_native_schema_sql_normalize($row['CHECK_CLAUSE'])!==$expected[$row['CONSTRAINT_NAME']]) {
            $error='schema_check_projection'; return false;
        }
    return true;
}

/** Verify exact denial trigger body, timing, event and absence of extra triggers. */
function pay_glb_001_native_schema_triggers_valid($model,&$error=null)
{
    $error=null; $prefix=pay_glb_001_native_schema_prefix();
    if ($prefix===false) { $error='schema_prefix_invalid'; return false; }
    $rows=pay_glb_001_native_schema_rows("SELECT TRIGGER_NAME,EVENT_MANIPULATION,ACTION_TIMING,ACTION_ORIENTATION,ACTION_STATEMENT FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND EVENT_OBJECT_TABLE=".db_escape($prefix.$model['name']));
    if ($rows===false || count($rows)!==2) { $error='schema_trigger_count'; return false; }
    $seen=array();
    foreach ($rows as $row) {
        $operation=$row['EVENT_MANIPULATION'];
        if (!in_array($operation,array('UPDATE','DELETE'),true) || isset($seen[$operation])) {
            $error='schema_trigger_event'; return false;
        }
        $seen[$operation]=true;
        $name=$prefix.'pgn_'.$model['code'].'_'.strtolower($operation[0]);
        $body="SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable ".$model['name'].' '.$operation." denied'";
        if ($row['TRIGGER_NAME']!==$name || $row['ACTION_TIMING']!=='BEFORE'
            || $row['ACTION_ORIENTATION']!=='ROW'
            || pay_glb_001_native_schema_sql_normalize($row['ACTION_STATEMENT'])!==pay_glb_001_native_schema_sql_normalize($body)) {
            $error='schema_trigger_projection'; return false;
        }
    }
    return true;
}

/** Require complete custody and optionally verify zero seed before activation. */
function pay_glb_001_native_require_schema($patch,$zero_seed=false)
{
    if($zero_seed===false && function_exists('pay_glb_001_native_reader_schema_cached')
        && pay_glb_001_native_reader_schema_cached('native'))return true;
    foreach (pay_glb_001_native_schema_definitions() as $model) {
        if (!pay_glb_001_native_schema_table_valid($model,$error)
            || !pay_glb_001_native_schema_triggers_valid($model,$error))
            return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,$model['name'].': '.$error);
        if ($zero_seed) {
            $rows=pay_glb_001_native_schema_rows('SELECT COUNT(*) n FROM '.TB_PREF.$model['name']);
            if (!$rows || (string)$rows[0]['n']!=='0')
                return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'native_custody_nonzero');
        }
    }
    return true;
}

/** Resume missing exact additive DDL and deny drift without overwriting history. */
function pay_glb_001_native_install_schema($patch)
{
    global $path_to_root;
    $prefix=pay_glb_001_native_schema_prefix();
    $sql=file_get_contents($path_to_root.'/sql/upgrade_pay_glb_001_native_custody.sql');
    if ($prefix===false || !is_string($sql))
        return hrm_migration_fail($patch,HRM_MIGRATION_MISSING_SCHEMA,'native_schema_source_missing');
    preg_match_all('/CREATE TABLE \x600_([^\x60]+)\x60.*?;(?=\s|$)/s',$sql,$tables,PREG_SET_ORDER);
    $models=pay_glb_001_native_schema_definitions();
    if (count($tables)!==count($models))
        return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'native_schema_source_count');
    foreach ($models as $index=>$model) {
        if ($tables[$index][1]!==$model['name'])
            return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'native_schema_source_order');
        if (!hrm_migration_table_exists($model['name'])
            && !db_query(str_replace(chr(96).'0_',chr(96).$prefix,$tables[$index][0])))
            return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'native_table_create_failed');
        if (!pay_glb_001_native_schema_table_valid($model,$error))
            return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,$error);
    }
    preg_match_all('/CREATE TRIGGER \x600_([^\x60]+)\x60.*?;(?=\s|$)/s',$sql,$triggers,PREG_SET_ORDER);
    if (count($triggers)!==12)
        return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'native_trigger_source_count');
    foreach ($triggers as $trigger) {
        if (!preg_match('/BEFORE (?:UPDATE|DELETE) ON \x600_([^\x60]+)\x60/',$trigger[0],$target))
            return hrm_migration_fail($patch,HRM_MIGRATION_PROJECTION_MISMATCH,'native_trigger_source_table');
        $rows=pay_glb_001_native_schema_rows("SELECT TRIGGER_NAME FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA=DATABASE() AND EVENT_OBJECT_TABLE=".db_escape($prefix.$target[1])." AND TRIGGER_NAME=".db_escape($prefix.$trigger[1]));
        if ($rows===false) return false;
        if (count($rows)===0 && !db_query(str_replace(chr(96).'0_',chr(96).$prefix,$trigger[0]))) return false;
    }
    return pay_glb_001_native_require_schema($patch,true);
}
