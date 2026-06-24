<?php
/**
 * Entity class for job_classes.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class job_classes_entity extends Entity {
	function __construct() {
		parent::__construct(
			'job_classes',
			array('class_code', 'class_name', 'pay_basis', 'description', 'inactive', 'job_class_id'),
			'job_class_id'
		);
	}
}

/**
 * Check whether any job classes exist in the system.
 *
 * @return bool True if at least one job class record exists.
 */
function db_has_job_classes() {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."job_classes");
}
