<?php
/**
 * Active HRM database-head registry.
 *
 * Records the active database version and legacy-payroll contract marker that
 * the source, fresh-install and demo schemas must agree on.
 */

/** @return array */
function hrm_migration_head_registry()
{
    return array(
        'active_database_version'=>'1.0.1089',
        'active_legacy_payroll_contract'=>'1.0.780',
        'next_backlog_item'=>'HRM-FND-007',
        'next_slice_changed_files'=>array(),
        'next_slice_scope'=>'HRM-FND-007 integrated local acceptance is the first pending development verification in the reconciled 60-workstream matrix at current source/DB target 1.0.1080 and custody 1.0.564. Five integrated acceptance records remain pending: HRM-FND-007 (1.0.433), HRM-ESS-001 (1.0.459), PAY-RULE-003 (1.0.485), PAY-RULE-004 (1.0.496), PAY-CORE-002 (1.0.546). Current source closure checks pass, but exact historical source/DB/activation prerequisites must be genuinely verified on disposable pairs before recording integrated acceptance. PAY-CON-001 documentation/mock acceptance remains CLOSED; PAY-REC-001, PAY-FIL-001 TT80 development acceptance and PAY-GLB-001 native-v1 pre-production closure remain accepted. Production activation remains denied; live provider admission, credentials, statutory submission and PAY-GLB-002 rollout remain separate.',
    );
}
