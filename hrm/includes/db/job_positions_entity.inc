<?php
/**
 * Entity class for job_positions (table: `positions`).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class job_positions_entity extends Entity {
	function __construct() {
		parent::__construct(
			'positions',
			array(
				'position_id',
				'position_code',
				'position_name',
				'job_class_id',
				'department_id',
				'reports_to_position_id',
				'basic_amount',
				'min_salary',
				'max_salary',
				'budgeted_headcount',
				'is_manager',
				'description',
				'inactive'
			),
			'position_id'
		);
	}
}
