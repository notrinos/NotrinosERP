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

define('DA_CUSTOMER', 'orders');
define('DA_SUPPLIER', 'AP');
define('DA_INVENTORY', 'stock');
define('DA_MANUFACTURE', 'manuf');
define('DA_DIMENSIONS', 'proj');
define('DA_GL', 'GL');
define('DA_FIXEDASSETS', 'assets');
define('DA_HRM', 'FrontHrm');
define('DA_SETUP', 'system');

define('WIDGET_FULL', 'widget_full');
define('WIDGET_HALF', 'widget_half');

class Dashboard {

	var $ar_dashboards;
	var $ar_widgets;

	function __construct() {
		$this->ar_dashboards = array();
	}

	function addDashboard($dashboard_name, $dashboard_id) {
		if (!$dashboard_id)
			$dashboard_id = 'orders';
		$this->ar_dashboards[$dashboard_id] = $dashboard_name;

		return $dashboard_id;
	}

	function addWidget($dashboard, $widget_id, $width=WIDGET_FULL) {

		unset($this->ar_widgets[$dashboard][$widget_id]);
		$this->ar_widgets[$dashboard][$widget_id] = array('width'=>$width);
	}

	function display($class=null) {
		global $path_to_root;

		if (isset($_GET['sel_app']))
			$cur_app = $_GET['sel_app'];
		elseif(isset($_GET['application']))
			$cur_app = $_GET['application'];
		else
			$cur_app = user_startup_tab();

		echo "<div class='widgets_container'>";
		foreach($this->ar_dashboards as $dashboard_id => $name) {

			if($cur_app == $dashboard_id) {

				foreach($this->ar_widgets[$dashboard_id] as $widget_id => $widget) {
					$widg = preg_replace('/[^a-z_0-9]/i', '', $widget_id);
					$widget_file = find_custom_file("/dashboard/widget$widg.php");

					if($widget['width'] == WIDGET_FULL)
						$class = 'widget_full';
					elseif($widget['width'] == WIDGET_HALF)
						$class = 'widget_half';

					echo "<div class = 'widget $class'>";

					if(file_exists($widget_file))
						require($widget_file);
					else
						echo "<br/><br/><div class = 'err_msg'>"._('Cannot find widget file').'&nbsp;'."'$widg'"."</div>";
					
					echo '</div>';
				}
			}
		}
		echo '</div>';
	}
}

class Widget {

	var $title;

	function __construct($title='') {
		$this->title = $title;
	}

	function setTitle($title) {
		$this->title = $title;
	}

	function checkSecurity($area) {

		if (!$_SESSION['wa_current_user']->can_access_page($area)) {
			echo '<center><br><br><br><b>';
			echo _('The security settings on your account do not permit you to access this function');
			echo '</b>';
			echo '<br><br><br><br></center>';

			return false;
		}
		return true;
	}

	function Start() {
		$title = $this->title;

		echo "<div class = 'widget_title'>";
		echo '<span>'.$title.'</span>';
		echo '</div>';
		echo "<div class = 'widget_content'>";
	}

	function End() {
		echo '</div>';
	}
}

function add_custom_dashboards($dashboard) {
	global $installed_extensions, $path_to_root;

	if (count($installed_extensions) > 0) {

		$extensions = $installed_extensions;
		foreach ($extensions as $ext) {
			if (($ext['active'] && $ext['type'] == 'extension')) {
				$file = $path_to_root.'/'.$ext['path'].'/dashboard/custom_dashboards.php';
				if (file_exists($file)) {
					set_ext_domain($ext['path']);
					include_once($file);
					set_ext_domain();
				}
			}
		}
	}
	$file = company_path().'/dashboard/custom_dashboards.php';
	if (file_exists($file))
		include_once($file);
}

function customer_top($today, $limit=10) {
	$begin = date2sql(begin_fiscalyear());
	$today = date2sql($today);
	$sql = "SELECT SUM((ov_amount + ov_discount) * rate * IF(trans.type = ".ST_CUSTCREDIT.", -1, 1)) AS total,d.debtor_no, d.name FROM ".TB_PREF."debtor_trans AS trans, ".TB_PREF."debtors_master AS d WHERE trans.debtor_no=d.debtor_no AND (trans.type = ".ST_SALESINVOICE." OR trans.type = ".ST_CUSTCREDIT.") AND tran_date >= '$begin' AND tran_date <= '$today' GROUP by d.debtor_no ORDER BY total DESC, d.debtor_no LIMIT $limit";

	$result = db_query($sql, 'could not get customer data');

	return $result;
}

function supplier_top($today, $limit=10) {

	$begin = date2sql(begin_fiscalyear());
	$today = date2sql($today);
	$sql = "SELECT SUM((trans.ov_amount + trans.ov_discount) * rate) AS total, s.supplier_id, s.supp_name FROM ".TB_PREF."supp_trans AS trans, ".TB_PREF."suppliers AS s WHERE trans.supplier_id=s.supplier_id AND (trans.type = ".ST_SUPPINVOICE." OR trans.type = ".ST_SUPPCREDIT.") AND tran_date >= '$begin' AND tran_date <= '$today' GROUP by s.supplier_id ORDER BY total DESC, s.supplier_id LIMIT $limit";

	$result = db_query($sql, 'could not get supplier data');
	
	return $result;
}

function stock_top($today, $limit=10, $type=0) {

	$begin = date2sql(begin_fiscalyear());
	$today = date2sql($today);
	if ($type == 0) {
		$sql = "SELECT SUM((trans.unit_price * trans.quantity) * d.rate) AS total, s.stock_id, s.description, SUM(trans.quantity) AS qty, SUM((s.material_cost + s.overhead_cost + s.labour_cost) * trans.quantity) AS costs FROM ".TB_PREF."debtor_trans_details AS trans, ".TB_PREF."stock_master AS s, ".TB_PREF."debtor_trans AS d WHERE trans.stock_id=s.stock_id AND trans.debtor_trans_type=d.type AND trans.debtor_trans_no=d.trans_no AND (d.type = ".ST_SALESINVOICE." OR d.type = ".ST_CUSTCREDIT.") ";
	}
	else {
		$sql = "SELECT SUM(m.qty * (s.material_cost + s.labour_cost + s.overhead_cost)) AS total, s.stock_id, s.description, SUM(qty) AS qty FROM ".TB_PREF."stock_master AS s, ".TB_PREF."stock_moves AS m WHERE s.stock_id=m.stock_id ";
		if ($type == 1)
			$sql .= "AND s.mb_flag='M' AND m.type <> ".ST_CUSTDELIVERY." AND m.type <> ".ST_CUSTCREDIT." ";
		elseif ($type == 2)	
			$sql .= "AND s.mb_flag='F' ";
	}
	if ($type != 2)
		$sql .= "AND tran_date >= '$begin' ";

	$sql .= "AND tran_date <= '$today' GROUP by s.stock_id ORDER BY total DESC, s.stock_id LIMIT $limit";

	$result = db_query($sql, 'could not get stock data');

	return $result;
}

function dimension_top($today, $limit=10) {

	$begin = date2sql(begin_fiscalyear());
	$today = date2sql($today);
	$sql = "SELECT SUM(-t.amount) AS total, d.reference, d.name FROM ".TB_PREF."gl_trans AS t,".TB_PREF."dimensions AS d WHERE (t.dimension_id = d.id OR t.dimension2_id = d.id) AND t.tran_date >= '$begin' AND t.tran_date <= '$today' GROUP BY d.id ORDER BY total DESC LIMIT $limit";
	$result = db_query($sql, 'Transactions could not be calculated');

	return $result;
}

function gl_top($today) {
	$begin = date2sql(begin_fiscalyear());
	$today = date2sql($today);
	$sql = "SELECT SUM(amount) AS total, c.class_name, c.ctype FROM ".TB_PREF."gl_trans,".TB_PREF."chart_master AS a, ".TB_PREF."chart_types AS t, ".TB_PREF."chart_class AS c WHERE account = a.account_code AND a.account_type = t.id AND t.class_id = c.cid AND IF(c.ctype > 3, tran_date >= '$begin', tran_date >= '0000-00-00') AND tran_date <= '$today' GROUP BY c.cid ORDER BY c.cid"; 

	$result = db_query($sql, 'Transactions could not be calculated');
	
	return $result;
}

function gl_performance($today, $weeks=5) {
	global $SysPrefs;

	$begin = date2sql(begin_fiscalyear());
	$today = date2sql($today);
	$sep = $SysPrefs->dateseps[user_date_sep()];
	$sql = "SELECT week_name, sales, costs FROM(SELECT DATE_FORMAT(tran_date, '%Y{$sep}%u') AS week_name, SUM(IF(c.ctype = 4, amount * -1, 0)) AS sales, SUM(IF(c.ctype = 6, amount, 0)) AS costs FROM ".TB_PREF."gl_trans, ".TB_PREF."chart_master AS a, ".TB_PREF."chart_types AS t, ".TB_PREF."chart_class AS c WHERE(c.ctype = 4 OR c.ctype = 6) AND account = a.account_code AND a.account_type = t.id AND t.class_id = c.cid AND tran_date >= '$begin' AND tran_date <= '$today' GROUP BY week_name ORDER BY week_name DESC LIMIT 0, $weeks) b GROUP BY week_name ORDER BY week_name ASC";

	$result = db_query($sql, 'Transactions could not be calculated');

	return $result;
}

function source_graphic($title, $x_axis, $pg, $graphic1, $graphic2=null, $type=2) {

	$pg->axis_x    = $x_axis;
	$pg->axis_y    = _('Amount');
	$pg->graphic_1 = $graphic1;
	if ($graphic2 != null)
		$pg->graphic_2 = $graphic2;
	$pg->type      = $type;
	$pg->skin      = 1;
	$pg->built_in  = false;
	$filename = company_path().'/pdf_files/'.random_id().'.png';
	$pg->display($filename, true);

	echo "<center><img src='$filename' border='0' alt='$title'></center>";
}

function customer_trans($today) {
	$today = date2sql($today);

	$sql = "SELECT trans.trans_no, trans.reference,	trans.tran_date, trans.due_date, debtor.debtor_no, debtor.name, branch.br_name, debtor.curr_code, (trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount) AS total, (trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount - trans.alloc) AS remainder, DATEDIFF('$today', trans.due_date) AS days FROM ".TB_PREF."debtor_trans as trans, ".TB_PREF."debtors_master as debtor, ".TB_PREF."cust_branch as branch WHERE debtor.debtor_no = trans.debtor_no AND trans.branch_code = branch.branch_code AND trans.type = ".ST_SALESINVOICE." AND (trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount - trans.alloc) > ".FLOAT_COMP_DELTA." AND DATEDIFF('$today', trans.due_date) > 0 ORDER BY days DESC";

	$result = db_query($sql, 'could not get customer transactions');

	return $result;
}

function calculate_next_invoice($myrow) {
	if ($myrow['last_sent'] == '0000-00-00')
		$next = sql2date($myrow['begin']);
	else
		$next = sql2date($myrow['last_sent']);
	$next = add_months($next, $myrow['monthly']);
	$next = add_days($next, $myrow['days']);
	return add_days($next, -1);
}

function supplier_trans($today) {
	$today = date2sql($today);
	$sql = "SELECT trans.trans_no, trans.reference, trans.tran_date, trans.due_date, s.supplier_id, s.supp_name, s.curr_code, (trans.ov_amount + trans.ov_gst + trans.ov_discount) AS total, (trans.ov_amount + trans.ov_gst + trans.ov_discount - trans.alloc) AS remainder, DATEDIFF('$today', trans.due_date) AS days FROM ".TB_PREF."supp_trans as trans, ".TB_PREF."suppliers as s WHERE s.supplier_id = trans.supplier_id AND trans.type = ".ST_SUPPINVOICE." AND (ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount) - trans.alloc) > ".FLOAT_COMP_DELTA." AND DATEDIFF('$today', trans.due_date) > 0 ORDER BY days DESC";

	$result = db_query($sql, 'could not get supplier transactions');
	
	return $result;
}
