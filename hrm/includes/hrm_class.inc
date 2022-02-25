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

class hrm_cart {
	public $trans_type;
	public $line_items;
	public $Comments;
	public $employee_id;
	public $empployee_name;
	public $tran_date;
	public $trans_no;
	public $cash_account;

	public $reference;

	function __construct($type, $trans_no=0) {
		$this->line_items = array();
		$this->trans_type = $type;
		$this->trans_no = $trans_no;
		$this->employee_id = 0;
	}

	function add_gl_item($code_id, $dimension_id, $dimension2_id, $amount, $memo='', $act_descr=null, $person_id=null, $date=null) {
		if (isset($code_id) && $code_id != '' && isset($amount) && isset($dimension_id) && isset($dimension2_id)) {
			$this->gl_items[] = new gl_item($code_id, $dimension_id, $dimension2_id, $amount, $memo, $act_descr, $person_id, $date);
			return true;
		}
		else // shouldn't come here under normal circumstances
			display_error("unexpected - invalid parameters in add_gl_item($code_id, $dimension_id, $dimension2_id, $amount,...)", '', true);

		return false;
	}

	function get_elements() {

		$items = $this->line_items;
		return $items;
	}

	function set_employee($employee_id, $employee_name) {
		$this->employee_id = $employee_id;
		$this->employee_name = $employee_name;
	}
}