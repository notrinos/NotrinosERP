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
		global $path_to_root;

		$stats = $this->get_hrm_alert_counts();
		$items = array(
			array(
				'id'      => 'hrm-leave-pending',
				'title'   => _('Pending Leave Approvals'),
				'summary' => sprintf(_('%d leave requests awaiting approval'), (int)$stats['pending_leave_requests']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/inquiry/leave_requests.php',
				'type'    => 'pending',
				'is_read' => false,
			),
			array(
				'id'      => 'hrm-overtime-pending',
				'title'   => _('Pending Overtime Approvals'),
				'summary' => sprintf(_('%d overtime requests awaiting approval'), (int)$stats['pending_overtime_requests']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/inquiry/overtime_requests.php',
				'type'    => 'pending',
				'is_read' => false,
			),
			array(
				'id'      => 'hrm-loan-pending',
				'title'   => _('Pending Loan Approvals'),
				'summary' => sprintf(_('%d loan applications awaiting approval'), (int)$stats['pending_loan_applications']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/inquiry/employee_loans.php',
				'type'    => 'pending',
				'is_read' => false,
			),
			array(
				'id'      => 'hrm-payslip-draft',
				'title'   => _('Draft Payslips'),
				'summary' => sprintf(_('%d payslips pending finalization'), (int)$stats['draft_payslips']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/manage/payslips.php',
				'type'    => 'pending',
				'is_read' => false,
			),
			array(
				'id'      => 'hrm-attendance-missing',
				'title'   => _('Missing Attendance Today'),
				'summary' => sprintf(_('%d employees have not recorded attendance today'), (int)$stats['missing_attendance_today']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/inquiry/attendance.php',
				'type'    => 'warning',
				'is_read' => false,
			),
			array(
				'id'      => 'hrm-documents-expiring',
				'title'   => _('Expiring Employee Documents'),
				'summary' => sprintf(_('%d employee documents expire in next 30 days'), (int)$stats['documents_expiring']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/inquiry/employee_documents.php',
				'type'    => 'warning',
				'is_read' => false,
			),
			array(
				'id'      => 'hrm-probation-ending',
				'title'   => _('Probation Periods Ending'),
				'summary' => sprintf(_('%d employees reach end of probation in next 30 days'), (int)$stats['probation_ending']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/inquiry/employee_inquiry.php',
				'type'    => 'warning',
				'is_read' => false,
			),
			array(
				'id'      => 'hrm-appraisal-pending',
				'title'   => _('Pending Appraisals'),
				'summary' => sprintf(_('%d employee appraisals awaiting approval'), (int)$stats['pending_appraisals']),
				'time'    => '',
				'url'     => $path_to_root . '/hrm/inquiry/appraisals.php',
				'type'    => 'pending',
				'is_read' => false,
			),
		);

		$filtered = array();
		foreach ($items as $item) {
			if (preg_match('/(\d+)/', $item['summary'], $matches) && (int)$matches[1] <= 0)
				continue;
			$filtered[] = $item;
		}

		if ((int)$limit > 0)
			$filtered = array_slice($filtered, 0, (int)$limit);

		return $filtered;
	}

	/**
	 * {@inheritdoc}
	 */
	public function getViewAllUrl()
	{
		global $path_to_root;
		return $path_to_root . '/hrm/dashboard/hrm_dashboard.php';
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
		if ($this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
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
		if ($this->table_exists(TB_PREF . 'leave_requests')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "leave_requests
				WHERE status = 0";
			$direct = (int)$this->scalar($sql, 'could not count pending leave requests');
			if ($direct > $counts['pending_leave_requests'])
				$counts['pending_leave_requests'] = $direct;
		}

		// ---- Pending Overtime Requests ----
		if ($this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
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

		if ($this->table_exists(TB_PREF . 'overtime_requests')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "overtime_requests
				WHERE status = 0";
			$direct = (int)$this->scalar($sql, 'could not count pending overtime requests');
			if ($direct > $counts['pending_overtime_requests'])
				$counts['pending_overtime_requests'] = $direct;
		}

		// ---- Pending Loan Applications ----
		if ($this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
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

		if ($this->table_exists(TB_PREF . 'employee_loans')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "employee_loans
				WHERE status = 0";
			$direct = (int)$this->scalar($sql, 'could not count pending loan applications');
			if ($direct > $counts['pending_loan_applications'])
				$counts['pending_loan_applications'] = $direct;
		}

		// ---- Draft Payslips ----
		if ($this->table_exists(TB_PREF . 'payslips')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "payslips
				WHERE status = 0";
			$counts['draft_payslips'] = (int)$this->scalar($sql, 'could not count draft payslips');
		}

		// ---- Missing Attendance Today ----
		if ($this->table_exists(TB_PREF . 'employees') && $this->table_exists(TB_PREF . 'attendance')) {
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
		if ($this->table_exists(TB_PREF . 'employee_documents')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "employee_documents
				WHERE expiry_date IS NOT NULL
					AND expiry_date >= " . db_escape($today) . "
					AND expiry_date <= " . db_escape($thirty_days);
			$counts['documents_expiring'] = (int)$this->scalar($sql, 'could not count expiring documents');
		}

		// ---- Probation Periods Ending ----
		if ($this->table_exists(TB_PREF . 'employees') && $this->column_exists(TB_PREF . 'employees', 'probation_end_date')) {
			$sql = "SELECT COUNT(*)
				FROM " . TB_PREF . "employees
				WHERE inactive = 0
					AND probation_end_date IS NOT NULL
					AND probation_end_date >= " . db_escape($today) . "
					AND probation_end_date <= " . db_escape($thirty_days);
			$counts['probation_ending'] = (int)$this->scalar($sql, 'could not count probation endings');
		}

		// ---- Pending Appraisals ----
		if ($this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
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
