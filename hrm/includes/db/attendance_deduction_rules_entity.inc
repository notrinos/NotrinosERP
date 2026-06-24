<?php
/**
 * Entity class for attendance_deduction_rules.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 * PK: rule_id (int), fields match the actual DB schema.
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class attendance_deduction_rules_entity extends Entity {
	function __construct() {
		parent::__construct(
			'attendance_deduction_rules',
			array('rule_id', 'rule_type', 'from_value', 'to_value',
				'deduction_rate', 'day_of_week', 'work_hours', 'inactive'),
			'rule_id'
		);
	}
}
