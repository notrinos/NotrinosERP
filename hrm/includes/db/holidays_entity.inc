<?php
/**
 * Entity class for holidays.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class holidays_entity extends Entity {
	function __construct() {
		parent::__construct(
			'holidays',
			array('holiday_id', 'holiday_name', 'holiday_date', 'to_date', 'recurring', 'is_paid', 'description'),
			'holiday_id'
		);
	}
}