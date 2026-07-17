<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM sensitive-field authorization policy helpers.
 *
 * These containment policies keep the current security-area model and add no
 * parallel role framework. Record-level access remains the responsibility of
 * the calling HRM page or repository.
 */

define('HRM_FIELD_RESTRICTED_COMPENSATION', 'restricted_compensation');
define('HRM_FIELD_RESTRICTED_FINANCIAL', 'restricted_financial');
define('HRM_FIELD_RESTRICTED_IDENTITY', 'restricted_identity');
define('HRM_FIELD_ACTION_VIEW', 'view');
define('HRM_FIELD_ACTION_MANAGE', 'manage');

/**
 * Return the approved permission matrix for implemented sensitive fields.
 *
 * The legacy salary-structure permission implies compensation view during the
 * rolling migration so code deployment cannot lock out existing salary
 * maintainers before role rows receive SA_HRM_VIEW_SALARY.
 *
 * @return array
 */
function hrm_sensitive_field_permission_matrix() {
	return array(
		HRM_FIELD_RESTRICTED_COMPENSATION => array(
			HRM_FIELD_ACTION_VIEW => array(
				'all' => array('SA_EMPLOYEE'),
				'any' => array('SA_HRM_VIEW_SALARY', 'SA_SALARYSTRUCTURE'),
			),
			HRM_FIELD_ACTION_MANAGE => array(
				'all' => array('SA_EMPLOYEE', 'SA_SALARYSTRUCTURE'),
				'any' => array(),
			),
		),
		HRM_FIELD_RESTRICTED_FINANCIAL => array(
			HRM_FIELD_ACTION_VIEW => array(
				'all' => array('SA_EMPLOYEE', 'SA_HRM_VIEW_BANK_DETAILS'),
				'any' => array(),
			),
			// Full reveal and mutation intentionally have no policy entry. They
			// remain denied until step-up and maker/checker controls exist.
		),
		HRM_FIELD_RESTRICTED_IDENTITY => array(
			HRM_FIELD_ACTION_VIEW => array(
				'all' => array('SA_EMPLOYEE', 'SA_HRM_VIEW_IDENTITY_DETAILS'),
				'any' => array(),
			),
			// Full reveal and mutation intentionally have no policy entry. They
			// remain denied until verification and maker/checker controls exist.
		),
	);
}

/**
 * Check the current principal against one sensitive-field action.
 *
 * Unknown field classes and actions fail closed.
 *
 * @param string $field_class
 * @param string $action
 * @return bool
 */
function hrm_user_can_access_sensitive_field($field_class, $action) {
	$matrix = hrm_sensitive_field_permission_matrix();
	if (!isset($matrix[$field_class]) || !isset($matrix[$field_class][$action]))
		return false;

	$policy = $matrix[$field_class][$action];
	foreach ($policy['all'] as $area) {
		if (!user_check_access($area))
			return false;
	}

	if (empty($policy['any']))
		return true;

	foreach ($policy['any'] as $area) {
		if (user_check_access($area))
			return true;
	}

	return false;
}

/**
 * Build a PII-free security-log message for a denied field decision.
 *
 * @param string $field_class
 * @param string $action
 * @return string
 */
function hrm_sensitive_field_denial_audit_message($field_class, $action) {
	return hrm_sensitive_field_audit_message($field_class, $action, 'denied');
}

/**
 * Build a PII-free security-log message for a sensitive-field decision.
 *
 * @param string $field_class
 * @param string $action
 * @param string $outcome
 * @return string
 */
function hrm_sensitive_field_audit_message($field_class, $action, $outcome) {
	$actor_id = isset($_SESSION['wa_current_user']->user)
		? (int)$_SESSION['wa_current_user']->user : 0;
	$company = isset($_SESSION['wa_current_user']->company)
		? (int)$_SESSION['wa_current_user']->company : -1;
	$method = isset($_SERVER['REQUEST_METHOD'])
		? strtoupper((string)$_SERVER['REQUEST_METHOD']) : 'UNKNOWN';

	$safe_outcome = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$outcome));
	if ($safe_outcome === '')
		$safe_outcome = 'unknown';

	return sprintf(
		'[HRM_SECURITY] access_%s actor_id=%d company=%d field_class=%s action=%s method=%s',
		$safe_outcome,
		$actor_id,
		$company,
		preg_replace('/[^a-z0-9_]/', '', strtolower((string)$field_class)),
		preg_replace('/[^a-z0-9_]/', '', strtolower((string)$action)),
		preg_replace('/[^A-Z]/', '', $method)
	);
}

/**
 * Record a PII-free sensitive-field decision in the server security log.
 *
 * @param string $field_class
 * @param string $action
 * @param string $outcome
 * @return void
 */
function hrm_log_sensitive_field_access($field_class, $action, $outcome) {
	error_log(hrm_sensitive_field_audit_message($field_class, $action, $outcome));
}

/**
 * Audit exclusion of all classified employee fields from an output channel.
 *
 * The channel is normalized before logging and must not contain an employee
 * identifier or any field value. One event per implemented field class makes
 * the data-minimization decision independently reviewable.
 *
 * @param string $channel Stable output-channel name.
 * @return void
 */
function hrm_log_restricted_employee_projection($channel) {
	$safe_channel = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$channel));
	if (!in_array($safe_channel, array(
		'employee_card',
		'employee_profile_report',
		'employee_directory',
		'salary_component_routing',
		'salary_revision_context',
		'employee_transfer_context',
		'employee_separation_context',
		'leave_entitlement_context',
		'employee_summary',
		'attendance_roster',
	), true))
		$safe_channel = 'unknown';

	$field_classes = array(
		HRM_FIELD_RESTRICTED_COMPENSATION,
		HRM_FIELD_RESTRICTED_FINANCIAL,
		HRM_FIELD_RESTRICTED_IDENTITY,
	);
	foreach ($field_classes as $field_class) {
		hrm_log_sensitive_field_access(
			$field_class,
			HRM_FIELD_ACTION_VIEW,
			'excluded_'.$safe_channel
		);
	}
}

/**
 * Return a field-specific denial message without disclosing record data.
 *
 * @param string $field_class
 * @return string
 */
function hrm_sensitive_field_denial_message($field_class) {
	if ($field_class === HRM_FIELD_RESTRICTED_FINANCIAL)
		return _('You do not have permission to change restricted employee bank details.');
	if ($field_class === HRM_FIELD_RESTRICTED_IDENTITY)
		return _('You do not have permission to change restricted employee identity or tax details.');

	return _('You do not have permission to access restricted employee compensation.');
}

/**
 * Enforce one sensitive-field action and record a denied security event.
 *
 * @param string $field_class
 * @param string $action
 * @return bool
 */
function hrm_require_sensitive_field_access($field_class, $action) {
	if (hrm_user_can_access_sensitive_field($field_class, $action))
		return true;

	hrm_log_sensitive_field_access($field_class, $action, 'denied');
	display_error(hrm_sensitive_field_denial_message($field_class));
	return false;
}

/**
 * Return the legacy employee columns covered by bank-detail containment.
 *
 * @return array
 */
function hrm_restricted_bank_detail_fields() {
	return array('bank_name', 'bank_branch', 'bank_account', 'bank_routing', 'payment_method');
}

/**
 * Normalize one bank-detail value for change detection only.
 *
 * @param string $field
 * @param mixed $value
 * @return string|int
 */
function hrm_normalize_restricted_bank_detail($field, $value) {
	if ($field === 'payment_method')
		return (int)$value;

	return trim((string)$value);
}

/**
 * Detect a submitted change against the current legacy employee row.
 *
 * Only allowlisted keys are considered. Missing keys mean "not submitted" so
 * unrelated employee updates do not appear to clear protected bank values.
 *
 * @param array $submitted
 * @param array|null $existing
 * @return bool
 */
function hrm_restricted_bank_details_changed($submitted, $existing = null) {
	$existing = is_array($existing) ? $existing : array();
	foreach (hrm_restricted_bank_detail_fields() as $field) {
		if (!array_key_exists($field, $submitted))
			continue;

		$default = $field === 'payment_method' ? 0 : '';
		$current = array_key_exists($field, $existing) ? $existing[$field] : $default;
		if (hrm_normalize_restricted_bank_detail($field, $submitted[$field])
			!== hrm_normalize_restricted_bank_detail($field, $current)) {
			return true;
		}
	}

	return false;
}

/**
 * Restore protected bank fields before an employee helper or hook receives data.
 *
 * @param array $data
 * @param array|null $existing
 * @return void
 */
function hrm_preserve_restricted_bank_details(&$data, $existing = null) {
	$existing = is_array($existing) ? $existing : array();
	foreach (hrm_restricted_bank_detail_fields() as $field) {
		$default = $field === 'payment_method' ? 0 : '';
		$data[$field] = array_key_exists($field, $existing) ? $existing[$field] : $default;
	}
}

/**
 * Enforce the fail-closed legacy bank-write boundary.
 *
 * Values are restored before the decision is returned so a denied forged value
 * cannot leak into downstream hooks. Manage remains unregistered by design.
 *
 * @param array $data
 * @param array $submitted
 * @param array|null $existing
 * @return bool
 */
function hrm_enforce_restricted_bank_write(&$data, $submitted, $existing = null) {
	$changed = hrm_restricted_bank_details_changed($submitted, $existing);
	hrm_preserve_restricted_bank_details($data, $existing);

	if (!$changed)
		return true;

	return hrm_require_sensitive_field_access(
		HRM_FIELD_RESTRICTED_FINANCIAL,
		HRM_FIELD_ACTION_MANAGE
	);
}

/**
 * Return the legacy employee columns covered by identity/tax containment.
 *
 * Passport expiry is protected with the passport identifier because changing
 * either value requires the same future verification workflow.
 *
 * @return array
 */
function hrm_restricted_identity_detail_fields() {
	return array('national_id', 'passport', 'passport_expiry', 'tax_number', 'social_security_no');
}

/**
 * Normalize one identity/tax value for exact change detection only.
 *
 * @param string $field
 * @param mixed $value
 * @return string
 */
function hrm_normalize_restricted_identity_detail($field, $value) {
	$value = trim((string)$value);
	if ($field !== 'passport_expiry' || $value === '')
		return $value;

	if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $value))
		return $value;

	return function_exists('date2sql') ? date2sql($value) : $value;
}

/**
 * Detect a submitted identity/tax change against a legacy row.
 *
 * Missing keys mean "not submitted". A restricted subset may be supplied for
 * child records such as a dependent's national identifier.
 *
 * @param array $submitted
 * @param array|null $existing
 * @param array|null $fields
 * @return bool
 */
function hrm_restricted_identity_details_changed($submitted, $existing = null, $fields = null) {
	$existing = is_array($existing) ? $existing : array();
	$fields = is_array($fields) ? $fields : hrm_restricted_identity_detail_fields();
	foreach ($fields as $field) {
		if (!in_array($field, hrm_restricted_identity_detail_fields(), true)
			|| !array_key_exists($field, $submitted)) {
			continue;
		}

		$current = array_key_exists($field, $existing) ? $existing[$field] : '';
		if (hrm_normalize_restricted_identity_detail($field, $submitted[$field])
			!== hrm_normalize_restricted_identity_detail($field, $current)) {
			return true;
		}
	}

	return false;
}

/**
 * Restore protected identity/tax values before downstream helpers or hooks.
 *
 * @param array $data
 * @param array|null $existing
 * @param array|null $fields
 * @return void
 */
function hrm_preserve_restricted_identity_details(&$data, $existing = null, $fields = null) {
	$existing = is_array($existing) ? $existing : array();
	$fields = is_array($fields) ? $fields : hrm_restricted_identity_detail_fields();
	foreach ($fields as $field) {
		if (!in_array($field, hrm_restricted_identity_detail_fields(), true))
			continue;
		$data[$field] = array_key_exists($field, $existing) ? $existing[$field] : '';
	}
}

/**
 * Enforce the fail-closed legacy identity/tax write boundary.
 *
 * Values are restored before the decision is returned. Manage remains
 * unregistered until a verified maker/checker change workflow exists.
 *
 * @param array $data
 * @param array $submitted
 * @param array|null $existing
 * @param array|null $fields
 * @return bool
 */
function hrm_enforce_restricted_identity_write(&$data, $submitted, $existing = null, $fields = null) {
	$changed = hrm_restricted_identity_details_changed($submitted, $existing, $fields);
	hrm_preserve_restricted_identity_details($data, $existing, $fields);

	if (!$changed)
		return true;

	return hrm_require_sensitive_field_access(
		HRM_FIELD_RESTRICTED_IDENTITY,
		HRM_FIELD_ACTION_MANAGE
	);
}

/**
 * Render a fixed-width masked token exposing at most the final characters.
 *
 * @param mixed $value
 * @param int $visible_characters
 * @return string
 */
function hrm_mask_restricted_value($value, $visible_characters = 4) {
	$value = preg_replace('/\s+/', '', trim((string)$value));
	if ($value === '')
		return '';

	$visible_characters = max(0, min(4, (int)$visible_characters));
	$suffix = $visible_characters > 0 ? substr($value, -$visible_characters) : '';
	return '********'.$suffix;
}
