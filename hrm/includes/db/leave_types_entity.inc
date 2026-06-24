<?php
/**
 * Entity class for leave_types.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class leave_types_entity extends Entity {
	function __construct() {
		parent::__construct(
			'leave_types',
			array('leave_name', 'leave_code', 'pay_rate', 'entitlement', 'inactive', 'leave_id'),
			'leave_id'
		);
	}
}
