<?php
/**
 * Entity class for eos_rules.
 *
 * Single-table CRUD via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */
include_once($path_to_root . '/includes/db/class.entity.inc');

class eos_rules_entity extends Entity {
	function __construct() {
		parent::__construct(
			'eos_rules',
			array('name', 'description', 'calculation_method', 'inactive', 'id'),
			'id'
		);
	}
}
