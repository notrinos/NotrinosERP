<?php
/**
 * Entity class for pay_grades (table: `pay_grades`).
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class grades_entity extends Entity {
	function __construct() {
		parent::__construct(
			'pay_grades',
			array(
				'grade_id',
				'grade_code',
				'grade_name',
				'grade_level',
				'min_salary',
				'mid_salary',
				'max_salary',
				'description',
				'inactive'
			),
			'grade_id'
		);
	}
}
