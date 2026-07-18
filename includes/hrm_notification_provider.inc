<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * Notification provider for HRM-focused alerts.
 *
 * Provides pending leave/overtime/loan/payslip approvals, expiring documents,
 * probation endings, missing attendance, and pending appraisals for the
 * topbar notification bell.
 */
class HrmNotificationProvider implements NotificationProvider
{
	/** @var int */
	private $user_id;

	/** @var int */
	private $role_id;

	/** @var int|null */
	private $count_cache = null;

	/**
	 * Constructor.
	 *
	 * @param int $user_id
	 * @param int $role_id
	 */
	public function __construct($user_id, $role_id)
	{
		$this->user_id = (int)$user_id;
		$this->role_id = (int)$role_id;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionId()
	{
		return 'hrm';
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionLabel()
	{
		return _('HRM Alerts');
	}

	/**
	 * {@inheritdoc}
	 */
	public function getSectionIcon()
	{
		return 'users';
	}

	/**
	 * {@inheritdoc}
	 */
	public function getCount()
	{
		if ($this->count_cache !== null)
			return $this->count_cache;

		$stats = $this->get_hrm_alert_counts();
		$this->count_cache = array_sum($stats);
		return $this->count_cache;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getItems($limit)
	{
		$stats = $this->get_hrm_alert_counts();
		$items = array();
		foreach ($this->get_allowed_category_definitions() as $key => $definition) {
			$count = isset($stats[$key]) ? (int)$stats[$key] : 0;
			if ($count <= 0)
				continue;
			$items[] = array(
				'id' => $definition['id'],
				'title' => $definition['title'],
				'summary' => sprintf($definition['summary'], $count),
				'time' => '',
				'url' => $definition['url'],
				'type' => $definition['type'],
				'is_read' => false,
			);
		}

		$limit = max(1, min(8, (int)$limit));
		return array_slice($items, 0, $limit);
	}

	/**
	 * {@inheritdoc}
	 */
	public function getViewAllUrl()
	{
		global $path_to_root;
		$definitions = $this->get_allowed_category_definitions();
		if (!empty($definitions)) {
			$first = reset($definitions);
			return $first['url'];
		}
		return $path_to_root . '/index.php';
	}

	/**
	 * Return the exact page-capability and valid-route contract for HRM alerts.
	 *
	 * @return array
	 */
	private function get_category_definitions()
	{
		global $path_to_root;
		return array(
			'pending_leave_requests' => array('id' => 'hrm-leave-pending', 'area' => 'SA_LEAVEAPPROVE', 'title' => _('Pending Leave Approvals'), 'summary' => _('%d leave requests awaiting approval'), 'url' => $path_to_root.'/hrm/transactions/leave_approval.php?sel_app=hrm', 'type' => 'pending'),
			'pending_overtime_requests' => array('id' => 'hrm-overtime-pending', 'area' => 'SA_OVERTIMEAPPROVE', 'title' => _('Pending Overtime Approvals'), 'summary' => _('%d overtime requests awaiting approval'), 'url' => $path_to_root.'/hrm/transactions/overtime_approval.php?sel_app=hrm', 'type' => 'pending'),
			'pending_loan_applications' => array('id' => 'hrm-loan-pending', 'area' => 'SA_LOAN', 'title' => _('Pending Loan Applications'), 'summary' => _('%d loan applications awaiting review'), 'url' => $path_to_root.'/hrm/transactions/loan_request.php?sel_app=hrm', 'type' => 'pending'),
			'draft_payslips' => array('id' => 'hrm-payslip-draft', 'area' => 'SA_PAYSLIP', 'title' => _('Draft Payslips'), 'summary' => _('%d payslips pending finalization'), 'url' => $path_to_root.'/hrm/transactions/payslip.php?NewPayslip=Yes&sel_app=hrm', 'type' => 'pending'),
			'missing_attendance_today' => array('id' => 'hrm-attendance-missing', 'area' => 'SA_ATTINQUIRY', 'title' => _('Missing Attendance Today'), 'summary' => _('%d employees have not recorded attendance today'), 'url' => $path_to_root.'/hrm/inquiry/attendance_inquiry.php?sel_app=hrm', 'type' => 'warning'),
			'documents_expiring' => array('id' => 'hrm-documents-expiring', 'area' => 'SA_EMPLOYEE', 'title' => _('Expiring Employee Documents'), 'summary' => _('%d employee documents expire in next 30 days'), 'url' => $path_to_root.'/hrm/manage/employees.php?_tabs_sel=tab_documents&sel_app=hrm', 'type' => 'warning'),
			'probation_ending' => array('id' => 'hrm-probation-ending', 'area' => 'SA_EMPLOYEEREP', 'title' => _('Probation Periods Ending'), 'summary' => _('%d employees reach end of probation in next 30 days'), 'url' => $path_to_root.'/hrm/inquiry/employee_directory.php?sel_app=hrm', 'type' => 'warning'),
			'pending_appraisals' => array('id' => 'hrm-appraisal-pending', 'area' => 'SA_EMPLOYEE', 'title' => _('Pending Appraisals'), 'summary' => _('%d employee appraisals awaiting approval'), 'url' => $path_to_root.'/hrm/manage/appraisals.php?sel_app=hrm', 'type' => 'pending'),
		);
	}

	/**
	 * Check the provider actor and current session capability for one category.
	 *
	 * @param string $area
	 * @return bool
	 */
	private function can_access_area($area)
	{
		return isset($_SESSION['wa_current_user'])
			&& $_SESSION['wa_current_user']->logged_in()
			&& (int)$_SESSION['wa_current_user']->user === $this->user_id
			&& $_SESSION['wa_current_user']->can_access_page($area);
	}

	/**
	 * Filter alert definitions before any category count query runs.
	 *
	 * @return array
	 */
	private function get_allowed_category_definitions()
	{
		$allowed = array();
		foreach ($this->get_category_definitions() as $key => $definition) {
			if ($this->can_access_area($definition['area']))
				$allowed[$key] = $definition;
		}
		return $allowed;
	}

	/**
	 * Record the capability-filtered notification category set without counts or PII.
	 *
	 * @param array $allowed
	 * @return void
	 */
	private function log_scope_decision($allowed)
	{
		$company = isset($_SESSION['wa_current_user']->company)
			? (int)$_SESSION['wa_current_user']->company : -1;
		error_log(sprintf(
			'[HRM_NOTIFICATION_SECURITY] actor_id=%d company=%d allowed_categories=%d outcome=filtered',
			$this->user_id,
			$company,
			count($allowed)
		));
	}

	/**
	 * Check table existence.
	 *
	 * @param  string $table_name
	 * @return bool
	 */
	private function table_exists($table_name)
	{
		$sql = "SHOW TABLES LIKE " . db_escape($table_name);
		$result = db_query($sql, 'could not check HRM notification table');
		return db_num_rows($result) > 0;
	}

	/**
	 * Check if a column exists in a table.
	 *
	 * @param  string $table_name  Table name including TB_PREF prefix.
	 * @param  string $column_name Column name.
	 * @return bool
	 */
	private function column_exists($table_name, $column_name)
	{
		$sql = "SHOW COLUMNS FROM `" . str_replace('`', '', $table_name) . "` LIKE " . db_escape($column_name);
		$result = @db_query($sql, 'could not check column existence');
		return ($result && db_num_rows($result) > 0);
	}

	/**
	 * Count HRM alert categories.
	 *
	 * @return array
	 */
	private function get_hrm_alert_counts()
	{
		$today         = date2sql(Today());
		$thirty_days   = date('Y-m-d', strtotime($today . ' +30 days'));
		$allowed       = $this->get_allowed_category_definitions();
		$this->log_scope_decision($allowed);

		$counts = array(
			'pending_leave_requests'     => 0,
			'pending_overtime_requests'  => 0,
			'pending_loan_applications'  => 0,
			'draft_payslips'             => 0,
			'missing_attendance_today'   => 0,
			'documents_expiring'         => 0,
			'probation_ending'           => 0,
			'pending_appraisals'         => 0,
		);

		// ---- Pending Leave Requests ----
		// Count from approval_drafts (approval workflow) + direct table (no workflow)
		if (isset($allowed['pending_leave_requests'])
			&& $this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
			$sql = "SELECT COUNT(DISTINCT d.id)
				FROM " . TB_PREF . "approval_drafts d
				INNER JOIN " . TB_PREF . "approval_levels al
					ON al.workflow_id = d.workflow_id
					AND al.level = d.current_level
					AND al.role_id = " . db_escape($this->role_id) . "
					AND al.is_active = 1
				WHERE d.status = 0
					AND d.trans_type = " . ST_LEAVE_REQUEST;
			$counts['pending_leave_requests'] = (int)$this->scalar($sql, 'could not count pending leave approvals');
		}

		// Also count from leave_requests table directly (no-approval-workflow case)
		if (isset($allowed['pending_leave_requests']) && $this->table_exists(TB_PREF . 'leave_requests')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "leave_requests
				WHERE status = 0";
			$direct = (int)$this->scalar($sql, 'could not count pending leave requests');
			if ($direct > $counts['pending_leave_requests'])
				$counts['pending_leave_requests'] = $direct;
		}

		// ---- Pending Overtime Requests ----
		if (isset($allowed['pending_overtime_requests'])
			&& $this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
			$sql = "SELECT COUNT(DISTINCT d.id)
				FROM " . TB_PREF . "approval_drafts d
				INNER JOIN " . TB_PREF . "approval_levels al
					ON al.workflow_id = d.workflow_id
					AND al.level = d.current_level
					AND al.role_id = " . db_escape($this->role_id) . "
					AND al.is_active = 1
				WHERE d.status = 0
					AND d.trans_type = " . ST_OVERTIME_REQUEST;
			$counts['pending_overtime_requests'] = (int)$this->scalar($sql, 'could not count pending overtime approvals');
		}

		if (isset($allowed['pending_overtime_requests']) && $this->table_exists(TB_PREF . 'overtime_requests')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "overtime_requests
				WHERE status = 0";
			$direct = (int)$this->scalar($sql, 'could not count pending overtime requests');
			if ($direct > $counts['pending_overtime_requests'])
				$counts['pending_overtime_requests'] = $direct;
		}

		// ---- Pending Loan Applications ----
		if (isset($allowed['pending_loan_applications'])
			&& $this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
			$sql = "SELECT COUNT(DISTINCT d.id)
				FROM " . TB_PREF . "approval_drafts d
				INNER JOIN " . TB_PREF . "approval_levels al
					ON al.workflow_id = d.workflow_id
					AND al.level = d.current_level
					AND al.role_id = " . db_escape($this->role_id) . "
					AND al.is_active = 1
				WHERE d.status = 0
					AND d.trans_type = " . ST_LOAN_REQUEST;
			$counts['pending_loan_applications'] = (int)$this->scalar($sql, 'could not count pending loan approvals');
		}

		if (isset($allowed['pending_loan_applications']) && $this->table_exists(TB_PREF . 'employee_loans')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "employee_loans
				WHERE status = 0";
			$direct = (int)$this->scalar($sql, 'could not count pending loan applications');
			if ($direct > $counts['pending_loan_applications'])
				$counts['pending_loan_applications'] = $direct;
		}

		// ---- Draft Payslips ----
		if (isset($allowed['draft_payslips']) && $this->table_exists(TB_PREF . 'payslips')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "payslips
				WHERE status = 0";
			$counts['draft_payslips'] = (int)$this->scalar($sql, 'could not count draft payslips');
		}

		// ---- Missing Attendance Today ----
		if (isset($allowed['missing_attendance_today'])
			&& $this->table_exists(TB_PREF . 'employees') && $this->table_exists(TB_PREF . 'attendance')) {
			$sql = "SELECT COUNT(DISTINCT e.employee_id)
				FROM " . TB_PREF . "employees e
				WHERE e.inactive = 0
					AND e.employee_id NOT IN (
						SELECT a.employee_id
						FROM " . TB_PREF . "attendance a
						WHERE a.date = " . db_escape($today) . "
					)";
			$counts['missing_attendance_today'] = (int)$this->scalar($sql, 'could not count missing attendance');
		}

		// ---- Expiring Employee Documents ----
		if (isset($allowed['documents_expiring']) && $this->table_exists(TB_PREF . 'employee_documents')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "employee_documents
				WHERE expiry_date IS NOT NULL
					AND expiry_date >= " . db_escape($today) . "
					AND expiry_date <= " . db_escape($thirty_days);
			$counts['documents_expiring'] = (int)$this->scalar($sql, 'could not count expiring documents');
		}

		// ---- Probation Periods Ending ----
		if (isset($allowed['probation_ending'])
			&& $this->table_exists(TB_PREF . 'employees') && $this->column_exists(TB_PREF . 'employees', 'probation_end_date')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "employees
				WHERE inactive = 0
					AND probation_end_date IS NOT NULL
					AND probation_end_date >= " . db_escape($today) . "
					AND probation_end_date <= " . db_escape($thirty_days);
			$counts['probation_ending'] = (int)$this->scalar($sql, 'could not count probation endings');
		}

		// ---- Pending Appraisals ----
		if (isset($allowed['pending_appraisals'])
			&& $this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
			$sql = "SELECT COUNT(DISTINCT d.id)
				FROM " . TB_PREF . "approval_drafts d
				INNER JOIN " . TB_PREF . "approval_levels al
					ON al.workflow_id = d.workflow_id
					AND al.level = d.current_level
					AND al.role_id = " . db_escape($this->role_id) . "
					AND al.is_active = 1
				WHERE d.status = 0
					AND d.trans_type = " . ST_EMPLOYEE_APPRAISAL;
			$counts['pending_appraisals'] = (int)$this->scalar($sql, 'could not count pending appraisals');
		}

		return $counts;
	}

	/**
	 * Execute scalar query helper.
	 *
	 * @param  string $sql
	 * @param  string $error_message
	 * @return float
	 */
	private function scalar($sql, $error_message)
	{
		$result = db_query($sql, $error_message);
		$row = db_fetch_row($result);
		if (!$row)
			return 0;
		return (float)$row[0];
	}
}
