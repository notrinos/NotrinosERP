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

include_once($path_to_root.'/includes/ui/items_cart.inc');

/**
 * HRM transaction cart.
 *
 * Extends core `items_cart` to reuse built-in GL line operations and provides
 * employee-aware posting for HRM transactions.
 */
class hrm_cart extends items_cart {
	/** @var int */
	public $employee_id = 0;

	/** @var string */
	public $employee_name = '';

	/** @var string */
	public $Comments = '';

	/** @var int */
	public $trans_no = 0;

	/** @var mixed */
	public $cash_account;

	/**
	 * Constructor.
	 *
	 * @param int $type
	 * @param int $trans_no
	 */
	function __construct($type, $trans_no=0) {
		parent::__construct($type, $trans_no);
		$this->trans_no = $trans_no;
		$this->order_id = $trans_no;
	}

	/**
	 * Backward compatible element getter.
	 *
	 * @return array
	 */
	function get_elements() {
		return $this->line_items;
	}

	/**
	 * Set employee context for sub-ledger posting.
	 *
	 * @param int $employee_id
	 * @param string $employee_name
	 * @return void
	 */
	function set_employee($employee_id, $employee_name) {
		$this->employee_id = (int)$employee_id;
		$this->employee_name = $employee_name;
	}

	/**
	 * Write GL entries and force employee person type for HRM transactions.
	 *
	 * @param bool $check_balance
	 * @return int
	 */
	function write_gl($check_balance=true) {
		$journal_id = 0;
		$trans_no = $this->trans_no ? $this->trans_no : $this->order_id;

		foreach ($this->gl_items as $line) {
			$line_date = isset($line->date) && $line->date ? $line->date : $this->tran_date;
			$journal_id += add_gl_trans(
				$this->trans_type,
				$trans_no,
				$line_date,
				$line->code_id,
				$line->dimension_id,
				$line->dimension2_id,
				$line->reference,
				$line->amount,
				null,
				PT_EMPLOYEE,
				$this->employee_id
			);
		}

		return $journal_id;
	}
}

/**
 * Payroll calculation document.
 *
 * Holds all calculated values and line details for one employee payslip.
 */
class payslip_doc {
	/** @var int */
	public $payslip_id = 0;

	/** @var int */
	public $payroll_period_id = 0;

	/** @var int */
	public $employee_id = 0;

	/** @var array */
	public $employee = array();

	/** @var string */
	public $from_date = '';

	/** @var string */
	public $to_date = '';

	/** @var string */
	public $tran_date = '';

	/** @var string */
	public $reference = '';

	/** @var float */
	public $basic_salary = 0;

	/** @var float */
	public $total_earnings = 0;

	/** @var float */
	public $gross_salary = 0;

	/** @var float */
	public $total_deductions = 0;

	/** @var float */
	public $net_salary = 0;

	/** @var float */
	public $employer_contributions = 0;

	/** @var float */
	public $total_cost = 0;

	/** @var float */
	public $working_days = 0;

	/** @var float */
	public $worked_days = 0;

	/** @var float */
	public $overtime_hours = 0;

	/** @var float */
	public $leave_days = 0;

	/** @var float */
	public $absent_days = 0;

	/** @var float */
	public $loan_deduction = 0;

	/** @var array */
	public $lines = array();

	/** @var array */
	public $gl_entries = array();

	/**
	 * Add one payslip element line.
	 *
	 * @param int $element_id
	 * @param string $element_name
	 * @param int $element_category
	 * @param bool $is_deduction
	 * @param int $amount_type
	 * @param float $base_amount
	 * @param float $rate
	 * @param float $calculated_amount
	 * @param float $final_amount
	 * @param string $account_code
	 * @param string|null $formula_used
	 * @param bool $is_taxable
	 * @param int $display_order
	 * @return void
	 */
	function add_line($element_id, $element_name, $element_category,
		$is_deduction, $amount_type, $base_amount, $rate,
		$calculated_amount, $final_amount, $account_code,
		$formula_used=null, $is_taxable=true, $display_order=0) {
		$this->lines[] = new payslip_line(
			$element_id,
			$element_name,
			$element_category,
			$is_deduction,
			$amount_type,
			$base_amount,
			$rate,
			$calculated_amount,
			$final_amount,
			$account_code,
			$formula_used,
			$is_taxable,
			$display_order
		);
	}

	/**
	 * Get only earning lines.
	 *
	 * @return array
	 */
	function get_earnings() {
		$earnings = array();
		foreach ($this->lines as $line) {
			if (!$line->is_deduction)
				$earnings[] = $line;
		}
		return $earnings;
	}

	/**
	 * Get only deduction lines.
	 *
	 * @return array
	 */
	function get_deductions() {
		$deductions = array();
		foreach ($this->lines as $line) {
			if ($line->is_deduction)
				$deductions[] = $line;
		}
		return $deductions;
	}

	/**
	 * Recalculate aggregate totals from current lines.
	 *
	 * @return void
	 */
	function recalculate_totals() {
		$this->total_earnings = 0;
		$this->total_deductions = 0;

		foreach ($this->lines as $line) {
			if ($line->is_deduction)
				$this->total_deductions += $line->final_amount;
			else
				$this->total_earnings += $line->final_amount;
		}

		$this->gross_salary = $this->basic_salary + $this->total_earnings;
		$this->net_salary = $this->gross_salary - $this->total_deductions;
		$this->total_cost = $this->net_salary + $this->employer_contributions;
	}
}

/**
 * Single payslip element row.
 */
class payslip_line {
	/** @var int */
	public $element_id;

	/** @var string */
	public $element_name;

	/** @var int */
	public $element_category;

	/** @var bool */
	public $is_deduction;

	/** @var int */
	public $amount_type;

	/** @var float */
	public $base_amount;

	/** @var float */
	public $rate;

	/** @var float */
	public $calculated_amount;

	/** @var float */
	public $final_amount;

	/** @var string */
	public $account_code;

	/** @var string|null */
	public $formula_used;

	/** @var bool */
	public $is_taxable;

	/** @var int */
	public $display_order;

	/**
	 * Constructor.
	 *
	 * @param int $element_id
	 * @param string $element_name
	 * @param int $element_category
	 * @param bool $is_deduction
	 * @param int $amount_type
	 * @param float $base_amount
	 * @param float $rate
	 * @param float $calculated_amount
	 * @param float $final_amount
	 * @param string $account_code
	 * @param string|null $formula_used
	 * @param bool $is_taxable
	 * @param int $display_order
	 */
	function __construct($element_id, $element_name, $element_category,
		$is_deduction, $amount_type, $base_amount, $rate,
		$calculated_amount, $final_amount, $account_code,
		$formula_used=null, $is_taxable=true, $display_order=0) {
		$this->element_id = (int)$element_id;
		$this->element_name = $element_name;
		$this->element_category = (int)$element_category;
		$this->is_deduction = (bool)$is_deduction;
		$this->amount_type = (int)$amount_type;
		$this->base_amount = (float)$base_amount;
		$this->rate = (float)$rate;
		$this->calculated_amount = (float)$calculated_amount;
		$this->final_amount = (float)$final_amount;
		$this->account_code = $account_code;
		$this->formula_used = $formula_used;
		$this->is_taxable = (bool)$is_taxable;
		$this->display_order = (int)$display_order;
	}
}
