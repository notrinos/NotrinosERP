<?php
/**
 * Entity class for work_shifts.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class work_shifts_entity extends Entity {
	function __construct() {
		parent::__construct(
			'work_shifts',
			array(
				'shift_id',
				'shift_name',
				'start_time',
				'end_time',
				'break_duration',
				'work_hours',
				'is_night_shift',
				'inactive'
			),
			'shift_id'
		);
	}
}
