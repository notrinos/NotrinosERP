<?php
/**
 * Active HRM migration-head registry plus immutable reviewed edges.
 *
 * Historical migration tests validate their own edge from this registry.
 * Only the active_database_version and active custody fields move when a new
 * accepted successor is added; historical edge entries never move.
 */

/** @return array */
function hrm_migration_head_registry()
{
    return array(
        'active_database_version'=>'1.0.237',
        'active_legacy_payroll_contract'=>'1.0.191',
        'next_backlog_item'=>'read-only HRM-FND-004 identifier/bank governance-metadata review only',
        'next_slice_changed_files'=>array(),
        'next_slice_scope'=>'reconcile accepted HRM-FND-001 identifier effective history and HRM-FND-004 encrypted bank foundation against approval/version requirements before choosing any additive mutation or payment-binding successor; make no verification promotion, maker-checker, full-reveal, payment-election/allocation or payment-authority change during that review',
        'edges'=>array(
            'hrm_fnd_002_employment_foundation'=>array('previous'=>'1.0.200','version'=>'1.0.201',
                'file'=>'sql/upgrade_hrm_fnd_002_employment_foundation.php'),
            'hrm_fnd_002_contract_foundation'=>array('previous'=>'1.0.201','version'=>'1.0.202',
                'file'=>'sql/upgrade_hrm_fnd_002_contract_foundation.php'),
            'hrm_fnd_002_assignment_foundation'=>array('previous'=>'1.0.202','version'=>'1.0.203',
                'file'=>'sql/upgrade_hrm_fnd_002_assignment_foundation.php'),
            'hrm_fnd_002_employment_rehire'=>array('previous'=>'1.0.203','version'=>'1.0.204',
                'file'=>'sql/upgrade_hrm_fnd_002_employment_rehire.php'),
            'hrm_fnd_002_assignment_effective_change'=>array('previous'=>'1.0.204','version'=>'1.0.205',
                'file'=>'sql/upgrade_hrm_fnd_002_assignment_effective_change.php'),
            'hrm_fnd_002_employment_termination'=>array('previous'=>'1.0.205','version'=>'1.0.206',
                'file'=>'sql/upgrade_hrm_fnd_002_employment_termination.php'),
            'hrm_fnd_002_concurrent_interval_policy'=>array('previous'=>'1.0.206','version'=>'1.0.207',
                'file'=>'sql/upgrade_hrm_fnd_002_concurrent_interval_policy.php'),
            'hrm_fnd_002_completion'=>array('previous'=>'1.0.207','version'=>'1.0.208',
                'file'=>'sql/upgrade_hrm_fnd_002_completion.php'),
            'hrm_fnd_003_legal_entity_foundation'=>array('previous'=>'1.0.208','version'=>'1.0.209',
                'file'=>'sql/upgrade_hrm_fnd_003_legal_entity_foundation.php'),
            'hrm_fnd_003_employment_legal_entity_binding'=>array('previous'=>'1.0.209','version'=>'1.0.210',
                'file'=>'sql/upgrade_hrm_fnd_003_employment_legal_entity_binding.php'),
            'hrm_fnd_003_organization_unit_foundation'=>array('previous'=>'1.0.210','version'=>'1.0.211',
                'file'=>'sql/upgrade_hrm_fnd_003_organization_unit_foundation.php'),
            'hrm_fnd_003_assignment_organization_unit_binding'=>array('previous'=>'1.0.211','version'=>'1.0.212',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_organization_unit_binding.php'),
            'hrm_fnd_003_grade_foundation'=>array('previous'=>'1.0.212','version'=>'1.0.213',
                'file'=>'sql/upgrade_hrm_fnd_003_grade_foundation.php'),
            'hrm_fnd_003_assignment_grade_binding'=>array('previous'=>'1.0.213','version'=>'1.0.214',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_grade_binding.php'),
            'hrm_fnd_003_position_foundation'=>array('previous'=>'1.0.214','version'=>'1.0.215',
                'file'=>'sql/upgrade_hrm_fnd_003_position_foundation.php'),
            'hrm_fnd_003_assignment_position_binding'=>array('previous'=>'1.0.215','version'=>'1.0.216',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_position_binding.php'),
            'hrm_fnd_003_job_foundation'=>array('previous'=>'1.0.216','version'=>'1.0.217',
                'file'=>'sql/upgrade_hrm_fnd_003_job_foundation.php'),
            'hrm_fnd_003_establishment_foundation'=>array('previous'=>'1.0.217','version'=>'1.0.218',
                'file'=>'sql/upgrade_hrm_fnd_003_establishment_foundation.php'),
            'hrm_fnd_003_work_location_foundation'=>array('previous'=>'1.0.218','version'=>'1.0.219',
                'file'=>'sql/upgrade_hrm_fnd_003_work_location_foundation.php'),
            'hrm_fnd_003_work_location_writer'=>array('previous'=>'1.0.219','version'=>'1.0.220',
                'file'=>'sql/upgrade_hrm_fnd_003_work_location_writer.php'),
            'hrm_fnd_003_assignment_work_location_binding'=>array('previous'=>'1.0.220','version'=>'1.0.221',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_work_location_binding.php'),
            'hrm_fnd_003_assignment_work_location_writer'=>array('previous'=>'1.0.221','version'=>'1.0.222',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_work_location_writer.php'),
            'hrm_fnd_003_establishment_writer'=>array('previous'=>'1.0.222','version'=>'1.0.223',
                'file'=>'sql/upgrade_hrm_fnd_003_establishment_writer.php'),
            'hrm_fnd_003_work_location_establishment_binding'=>array('previous'=>'1.0.223','version'=>'1.0.224',
                'file'=>'sql/upgrade_hrm_fnd_003_work_location_establishment_binding.php'),
            'hrm_fnd_003_work_location_establishment_writer'=>array('previous'=>'1.0.224','version'=>'1.0.225',
                'file'=>'sql/upgrade_hrm_fnd_003_work_location_establishment_writer.php'),
            'hrm_fnd_003_job_writer'=>array('previous'=>'1.0.225','version'=>'1.0.226',
                'file'=>'sql/upgrade_hrm_fnd_003_job_writer.php'),
            'hrm_fnd_003_position_job_binding'=>array('previous'=>'1.0.226','version'=>'1.0.227',
                'file'=>'sql/upgrade_hrm_fnd_003_position_job_binding.php'),
            'hrm_fnd_003_position_job_writer'=>array('previous'=>'1.0.227','version'=>'1.0.228',
                'file'=>'sql/upgrade_hrm_fnd_003_position_job_writer.php'),
            'hrm_fnd_003_assignment_job_binding'=>array('previous'=>'1.0.228','version'=>'1.0.229',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_job_binding.php'),
            'hrm_fnd_003_assignment_job_writer'=>array('previous'=>'1.0.229','version'=>'1.0.230',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_job_writer.php'),
            'hrm_fnd_003_assignment_manager_binding'=>array('previous'=>'1.0.230','version'=>'1.0.231',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_manager_binding.php'),
            'hrm_fnd_003_assignment_manager_writer'=>array('previous'=>'1.0.231','version'=>'1.0.232',
                'file'=>'sql/upgrade_hrm_fnd_003_assignment_manager_writer.php'),
            'hrm_fnd_003_position_hierarchy_foundation'=>array('previous'=>'1.0.232','version'=>'1.0.233',
                'file'=>'sql/upgrade_hrm_fnd_003_position_hierarchy_foundation.php'),
            'hrm_fnd_003_position_hierarchy_writer'=>array('previous'=>'1.0.233','version'=>'1.0.234',
                'file'=>'sql/upgrade_hrm_fnd_003_position_hierarchy_writer.php'),
            'hrm_fnd_003_position_headcount_foundation'=>array('previous'=>'1.0.234','version'=>'1.0.235',
                'file'=>'sql/upgrade_hrm_fnd_003_position_headcount_foundation.php'),
            'hrm_fnd_003_position_headcount_writer'=>array('previous'=>'1.0.235','version'=>'1.0.236',
                'file'=>'sql/upgrade_hrm_fnd_003_position_headcount_writer.php'),
            'hrm_fnd_004_person_bank_account_foundation'=>array('previous'=>'1.0.236','version'=>'1.0.237',
                'file'=>'sql/upgrade_hrm_fnd_004_person_bank_account_foundation.php'),
        ),
    );
}

/**
 * Verify one immutable migration edge without consulting moving HEAD markers.
 *
 * @param string $name Registry edge key.
 * @param string $previous Expected predecessor.
 * @param string $version Expected successor.
 * @return bool
 */
function hrm_migration_registered_edge_matches($name, $previous, $version)
{
    $registry = hrm_migration_head_registry();
    return isset($registry['edges'][$name])
        && (string)$registry['edges'][$name]['previous'] === (string)$previous
        && (string)$registry['edges'][$name]['version'] === (string)$version;
}
