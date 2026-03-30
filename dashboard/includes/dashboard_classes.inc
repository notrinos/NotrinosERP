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
define('DA_HRM', 'hrm');
define('DA_SETUP', 'system');

define('WIDGET_FULL', 'widget_full');
define('WIDGET_HALF', 'widget_half');
define('WIDGET_THIRD', 'widget_third');
define('WIDGET_QUARTER', 'widget_quarter');

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

	function addWidget($dashboard, $widget_id, $width=WIDGET_FULL, $extra_class='') {

		unset($this->ar_widgets[$dashboard][$widget_id]);
		$this->ar_widgets[$dashboard][$widget_id] = array('width'=>$width, 'class'=>$extra_class);
	}

	/**
	 * Add widget with an extra CSS class for presentation variants.
	 *
	 * @param string $dashboard Dashboard id
	 * @param int|string $widget_id Widget id
	 * @param string $width Width constant
	 * @param string $extra_class Additional CSS class
	 * @return void
	 */
	function addWidgetWithClass($dashboard, $widget_id, $width=WIDGET_FULL, $extra_class='') {
		$this->addWidget($dashboard, $widget_id, $width, $extra_class);
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

				foreach($this->ar_widgets[$dashboard_id] as $widget_id => $widget_settings) {
					$widg = preg_replace('/[^a-z_0-9]/i', '', $widget_id);
					$widget_file = find_custom_file("/dashboard/widget$widg.php");

					if($widget_settings['width'] == WIDGET_FULL)
						$class = 'widget_full';
					elseif($widget_settings['width'] == WIDGET_HALF)
						$class = 'widget_half';
					elseif($widget_settings['width'] == WIDGET_THIRD)
						$class = 'widget_third';
					elseif($widget_settings['width'] == WIDGET_QUARTER)
						$class = 'widget_quarter';

					$widget_markup = '';
					$widget_type_class = '';
					if($widget_file !== null) {
						ob_start();
						require($widget_file);
						$widget_markup = ob_get_clean();

						if (stripos($widget_markup, '<table') !== false || stripos($widget_markup, 'class="form-table"') !== false || stripos($widget_markup, "class='form-table'") !== false)
							$widget_type_class = ' widget-has-table';
						elseif (stripos($widget_markup, '<canvas') !== false || stripos($widget_markup, '<svg') !== false || stripos($widget_markup, '<img') !== false)
							$widget_type_class = ' widget-has-chart';
					} else
						$widget_markup = "<br/><br/><div class = 'err_msg'>"._('Cannot find widget file').'&nbsp;' . "'$widg'" . "</div>";

					$extra_class = '';
					if (isset($widget_settings['class']) && $widget_settings['class'] != '')
						$extra_class = ' '.preg_replace('/[^a-z_0-9\-\s]/i', '', $widget_settings['class']);

					echo "<div class = 'widget $class$widget_type_class$extra_class'>";
					echo $widget_markup;
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

/**
 * Execute scalar SQL and return first column as numeric value.
 *
 * @param string $sql SQL query returning one row and one value.
 * @param string $error_message Query error message.
 * @return float
 */
function get_dashboard_scalar_value($sql, $error_message) {
	$result = db_query($sql, $error_message);
	$row = db_fetch_row($result);
	if (!$row)
		return 0;

	return (float)$row[0];
}

/**
 * Check whether table exists for the current company database prefix.
 *
 * @param string $table_name Table name including TB_PREF prefix.
 * @return bool
 */
function dashboard_table_exists($table_name) {
	$table_name = db_escape($table_name);
	$sql = "SHOW TABLES LIKE ".$table_name;
	$result = db_query($sql, _('Could not check table existence'));
	return db_num_rows($result) > 0;
}

/**
 * Check whether a given column exists in a dashboard table.
 *
 * @param string $table_name Table name including TB_PREF prefix.
 * @param string $column_name Column name to check.
 * @return bool
 */
function dashboard_table_has_column($table_name, $column_name) {
	$table_name = db_escape($table_name);
	$column_name = db_escape($column_name);
	$sql = "SHOW COLUMNS FROM ".$table_name." LIKE ".$column_name;
	$result = db_query($sql, _('Could not check table column existence'));
	return $result && db_num_rows($result) > 0;
}

/**
 * Render a compact dashboard stat card used by small widgets.
 *
 * @param string $label Metric label.
 * @param mixed $value Numeric or text value.
 * @param string $icon_name Theme icon name.
 * @param string $tone Visual tone class.
 * @return void
 */
function render_dashboard_small_stat_card($label, $value, $icon_name='database', $tone='default') {
	$label = htmlspecialchars($label, ENT_QUOTES, 'UTF-8');
	$value_text = is_numeric($value) ? number_format((float)$value, 0) : htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
	$tone = preg_replace('/[^a-z_\-0-9]/i', '', $tone);

	echo "<div class='dashboard-small-stat dashboard-small-stat-".$tone."'>";
	echo "<div class='dashboard-small-stat-icon'>".default_theme_icon($icon_name)."</div>";
	echo "<div class='dashboard-small-stat-value'>".$value_text."</div>";
	echo "<div class='dashboard-small-stat-label'>".$label."</div>";
	echo "</div>";
}

/**
 * Count active customers.
 *
 * @return int
 */
function dashboard_count_customers() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."debtors_master WHERE inactive=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count customers'));
}

/**
 * Count active customer branches.
 *
 * @return int
 */
function dashboard_count_customer_branches() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."cust_branch WHERE inactive=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count branches'));
}

/**
 * Count active salesmen.
 *
 * @return int
 */
function dashboard_count_salesmen() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."salesman WHERE inactive=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count salesmen'));
}

/**
 * Count overdue customer invoices with outstanding balances.
 *
 * @return int
 */
function dashboard_count_overdue_sales_invoices() {
	$today = date2sql(Today());
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."debtor_trans trans
		LEFT JOIN ".TB_PREF."voided v ON v.type=trans.type AND v.id=trans.trans_no
		WHERE trans.type=".ST_SALESINVOICE."
			AND v.id IS NULL
			AND trans.due_date < '".$today."'
			AND (trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount - trans.alloc) > ".FLOAT_COMP_DELTA;

	return (int)get_dashboard_scalar_value($sql, _('Could not count overdue sales invoices'));
}

/**
 * Count active suppliers.
 *
 * @return int
 */
function dashboard_count_suppliers() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."suppliers WHERE inactive=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count suppliers'));
}

/**
 * Count open purchase orders that still have unreceived quantities.
 *
 * @return int
 */
function dashboard_count_open_purchase_orders() {
	$sql = "SELECT COUNT(DISTINCT d.order_no)
		FROM ".TB_PREF."purch_order_details d
		INNER JOIN ".TB_PREF."purch_orders p ON p.order_no=d.order_no
		WHERE d.quantity_received < d.quantity_ordered";

	return (int)get_dashboard_scalar_value($sql, _('Could not count open purchase orders'));
}

/**
 * Count purchase invoices.
 *
 * @return int
 */
function dashboard_count_purchase_invoices() {
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."supp_trans trans
		LEFT JOIN ".TB_PREF."voided v ON v.type=trans.type AND v.id=trans.trans_no
		WHERE trans.type=".ST_SUPPINVOICE." AND v.id IS NULL";

	return (int)get_dashboard_scalar_value($sql, _('Could not count purchase invoices'));
}

/**
 * Count overdue supplier invoices with outstanding balances.
 *
 * @return int
 */
function dashboard_count_overdue_purchase_invoices() {
	$today = date2sql(Today());
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."supp_trans trans
		LEFT JOIN ".TB_PREF."voided v ON v.type=trans.type AND v.id=trans.trans_no
		WHERE trans.type=".ST_SUPPINVOICE."
			AND v.id IS NULL
			AND trans.due_date < '".$today."'
			AND (ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount) - trans.alloc) > ".FLOAT_COMP_DELTA;

	return (int)get_dashboard_scalar_value($sql, _('Could not count overdue purchase invoices'));
}

/**
 * Count active inventory items (excluding fixed assets).
 *
 * @return int
 */
function dashboard_count_inventory_items() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE inactive=0 AND mb_flag <> 'F'";
	return (int)get_dashboard_scalar_value($sql, _('Could not count inventory items'));
}

/**
 * Count active inventory locations.
 *
 * @return int
 */
function dashboard_count_inventory_locations() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."locations WHERE inactive=0 AND fixed_asset=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count inventory locations'));
}

/**
 * Count item kits (BOM parent records).
 *
 * @return int
 */
function dashboard_count_item_kits() {
	$sql = "SELECT COUNT(DISTINCT b.parent)
		FROM ".TB_PREF."bom b
		INNER JOIN ".TB_PREF."stock_master s ON s.stock_id=b.parent
		WHERE s.inactive=0";

	return (int)get_dashboard_scalar_value($sql, _('Could not count kits'));
}

/**
 * Count stock/location rows below reorder level.
 *
 * @return int
 */
function dashboard_count_below_reorder_level_items() {
	$sql = "SELECT COUNT(*)
		FROM (
			SELECT sm.stock_id, ls.loc_code
			FROM ".TB_PREF."stock_master sm
			INNER JOIN ".TB_PREF."loc_stock ls ON sm.stock_id=ls.stock_id
			LEFT JOIN ".TB_PREF."stock_moves mv ON mv.stock_id=sm.stock_id AND mv.loc_code=ls.loc_code
			WHERE sm.mb_flag IN ('B','M')
				AND ls.reorder_level > 0
			GROUP BY sm.stock_id, ls.loc_code, ls.reorder_level
			HAVING COALESCE(SUM(mv.qty), 0) < ls.reorder_level
		) a";

	return (int)get_dashboard_scalar_value($sql, _('Could not count below reorder level items'));
}

/**
 * Count assembled stock items.
 *
 * @return int
 */
function dashboard_count_assembled_items() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE inactive=0 AND mb_flag='A'";
	return (int)get_dashboard_scalar_value($sql, _('Could not count assembled items'));
}

/**
 * Count manufactured stock items.
 *
 * @return int
 */
function dashboard_count_manufactured_items() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE inactive=0 AND mb_flag='M'";
	return (int)get_dashboard_scalar_value($sql, _('Could not count manufactured items'));
}

/**
 * Count active work centres.
 *
 * @return int
 */
function dashboard_count_work_centres() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."workcentres WHERE inactive=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count work centres'));
}

/**
 * Count open workorders.
 *
 * @return int
 */
function dashboard_count_open_workorders() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."workorders WHERE closed=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count open workorders'));
}

/**
 * Count active fixed assets.
 *
 * @return int
 */
function dashboard_count_fixed_assets() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE inactive=0 AND mb_flag='F'";
	return (int)get_dashboard_scalar_value($sql, _('Could not count fixed assets'));
}

/**
 * Count active fixed asset locations.
 *
 * @return int
 */
function dashboard_count_fixed_asset_locations() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."locations WHERE inactive=0 AND fixed_asset=1";
	return (int)get_dashboard_scalar_value($sql, _('Could not count fixed asset locations'));
}

/**
 * Count fixed asset categories.
 *
 * @return int
 */
function dashboard_count_fixed_asset_categories() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."stock_category WHERE inactive=0 AND dflt_mb_flag='F'";
	return (int)get_dashboard_scalar_value($sql, _('Could not count fixed asset categories'));
}

/**
 * Count active fixed asset classes.
 *
 * @return int
 */
function dashboard_count_fixed_asset_classes() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."stock_fa_class WHERE inactive=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count fixed asset classes'));
}

/**
 * Count dimensions of type 1.
 *
 * @return int
 */
function dashboard_count_dimensions_type1() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."dimensions WHERE type_=1";
	return (int)get_dashboard_scalar_value($sql, _('Could not count dimensions'));
}

/**
 * Count dimensions of type 2.
 *
 * @return int
 */
function dashboard_count_dimensions_type2() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."dimensions WHERE type_=2";
	return (int)get_dashboard_scalar_value($sql, _('Could not count type 2 dimensions'));
}

/**
 * Sum dimension-related balance-sheet amounts for current fiscal year.
 *
 * @return string
 */
function dashboard_dimensions_total_balance_text() {
	$begin = date2sql(begin_fiscalyear());
	$today = date2sql(Today());
	$sql = "SELECT COALESCE(SUM(t.amount), 0)
		FROM ".TB_PREF."gl_trans t
		INNER JOIN ".TB_PREF."chart_master a ON a.account_code=t.account
		INNER JOIN ".TB_PREF."chart_types ct ON ct.id=a.account_type
		INNER JOIN ".TB_PREF."chart_class cc ON cc.cid=ct.class_id
		WHERE (t.dimension_id <> 0 OR t.dimension2_id <> 0)
			AND cc.ctype <= 3
			AND t.tran_date >= '".$begin."'
			AND t.tran_date <= '".$today."'";

	return number_format2(get_dashboard_scalar_value($sql, _('Could not calculate dimensions total balance')), user_price_dec());
}

/**
 * Sum dimension-related P/L amounts for current fiscal year.
 *
 * @return string
 */
function dashboard_dimensions_total_result_text() {
	$begin = date2sql(begin_fiscalyear());
	$today = date2sql(Today());
	$sql = "SELECT COALESCE(SUM(t.amount), 0)
		FROM ".TB_PREF."gl_trans t
		INNER JOIN ".TB_PREF."chart_master a ON a.account_code=t.account
		INNER JOIN ".TB_PREF."chart_types ct ON ct.id=a.account_type
		INNER JOIN ".TB_PREF."chart_class cc ON cc.cid=ct.class_id
		WHERE (t.dimension_id <> 0 OR t.dimension2_id <> 0)
			AND cc.ctype > 3
			AND t.tran_date >= '".$begin."'
			AND t.tran_date <= '".$today."'";

	return number_format2(get_dashboard_scalar_value($sql, _('Could not calculate dimensions total result')), user_price_dec());
}

/**
 * Count open customer invoices (receivables).
 *
 * @return int
 */
function dashboard_count_receivables() {
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."debtor_trans trans
		LEFT JOIN ".TB_PREF."voided v ON v.type=trans.type AND v.id=trans.trans_no
		WHERE trans.type=".ST_SALESINVOICE."
			AND v.id IS NULL
			AND (trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount - trans.alloc) > ".FLOAT_COMP_DELTA;

	return (int)get_dashboard_scalar_value($sql, _('Could not count receivables'));
}

/**
 * Count open supplier invoices (payables).
 *
 * @return int
 */
function dashboard_count_payables() {
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."supp_trans trans
		LEFT JOIN ".TB_PREF."voided v ON v.type=trans.type AND v.id=trans.trans_no
		WHERE trans.type=".ST_SUPPINVOICE."
			AND v.id IS NULL
			AND (ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount) - trans.alloc) > ".FLOAT_COMP_DELTA;

	return (int)get_dashboard_scalar_value($sql, _('Could not count payables'));
}

/**
 * Count today's bank deposits.
 *
 * @return int
 */
function dashboard_count_todays_deposits() {
	$today = date2sql(Today());
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."bank_trans bt
		LEFT JOIN ".TB_PREF."voided v ON v.type=bt.type AND v.id=bt.trans_no
		WHERE bt.type=".ST_BANKDEPOSIT." AND bt.trans_date='".$today."' AND v.id IS NULL";

	return (int)get_dashboard_scalar_value($sql, _('Could not count today deposits'));
}

/**
 * Count today's bank payments.
 *
 * @return int
 */
function dashboard_count_todays_payments() {
	$today = date2sql(Today());
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."bank_trans bt
		LEFT JOIN ".TB_PREF."voided v ON v.type=bt.type AND v.id=bt.trans_no
		WHERE bt.type=".ST_BANKPAYMENT." AND bt.trans_date='".$today."' AND v.id IS NULL";

	return (int)get_dashboard_scalar_value($sql, _('Could not count today payments'));
}

/**
 * Count active employees.
 *
 * @return int
 */
function dashboard_count_employees() {
	$table_name = TB_PREF.'employees';
	if (!dashboard_table_exists($table_name))
		return 0;

	$sql = "SELECT COUNT(*) FROM ".TB_PREF."employees";

	return (int)get_dashboard_scalar_value($sql, _('Could not count employees'));
}

/**
 * Count documents expiring within 30 days.
 *
 * @return int
 */
function dashboard_count_doc_expiry_soon() {
	$table_name = TB_PREF.'employee_documents';
	if (!dashboard_table_exists($table_name))
		return 0;
	if (!dashboard_table_has_column($table_name, 'expiry_date'))
		return 0;

	$today = date2sql(Today());
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."employee_documents
		WHERE expiry_date IS NOT NULL
			AND expiry_date <> '0000-00-00'
			AND expiry_date >= '".$today."'
			AND expiry_date <= DATE_ADD('".$today."', INTERVAL 30 DAY)";

	return (int)get_dashboard_scalar_value($sql, _('Could not count expiring documents'));
}

/**
 * Count active departments.
 *
 * @return int
 */
function dashboard_count_departments() {
	$table_name = TB_PREF.'departments';
	if (!dashboard_table_exists($table_name))
		return 0;

	$sql = "SELECT COUNT(*) FROM ".TB_PREF."departments";

	return (int)get_dashboard_scalar_value($sql, _('Could not count departments'));
}

/**
 * Count payroll periods approved/posted but not paid.
 *
 * @return int
 */
function dashboard_count_unpaid_salaries_periods() {
	$table_name = TB_PREF.'payroll_periods';
	if (!dashboard_table_exists($table_name))
		return 0;
	if (!dashboard_table_has_column($table_name, 'status'))
		return 0;

	$sql = "SELECT COUNT(*) FROM ".TB_PREF."payroll_periods
		WHERE status IN (1,2,3)
			OR status IN ('open','locked','posted','approved')";
	return (int)get_dashboard_scalar_value($sql, _('Could not count unpaid salary periods'));
}

/**
 * Count active users.
 *
 * @return int
 */
function dashboard_count_users() {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."users WHERE inactive=0";
	return (int)get_dashboard_scalar_value($sql, _('Could not count users'));
}

/**
 * Count active extensions from installed list.
 *
 * @return int
 */
function dashboard_count_active_extensions() {
	global $installed_extensions;

	if (!isset($installed_extensions) || !is_array($installed_extensions))
		return 0;

	$count = 0;
	foreach ($installed_extensions as $extension) {
		if (!empty($extension['active']))
			$count++;
	}

	return $count;
}

/**
 * Count installed language packs.
 *
 * @return int
 */
function dashboard_count_installed_languages() {
	global $installed_languages;

	if (!isset($installed_languages) || !is_array($installed_languages))
		return 0;

	return count($installed_languages);
}

/**
 * Get formatted current database size.
 *
 * @return string
 */
function dashboard_get_database_size_text() {
	$sql = "SELECT COALESCE(SUM(data_length + index_length), 0)
		FROM information_schema.tables
		WHERE table_schema = DATABASE()";

	$size_bytes = get_dashboard_scalar_value($sql, _('Could not calculate database size'));
	if ($size_bytes <= 0)
		return '0 B';

	$units = array('B', 'KB', 'MB', 'GB', 'TB');
	$unit_index = 0;
	while ($size_bytes >= 1024 && $unit_index < count($units) - 1) {
		$size_bytes = $size_bytes / 1024;
		$unit_index++;
	}

	return number_format($size_bytes, $unit_index == 0 ? 0 : 2).' '.$units[$unit_index];
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

/**
 * Get the active employee count grouped by department.
 *
 * @param int|null $limit Optional number of departments to return.
 * @return resource
 */
function hrm_employees_by_department($limit=null) {
	$sql = "SELECT d.department_name, COUNT(e.employee_id) AS total
		FROM ".TB_PREF."employees e
		LEFT JOIN ".TB_PREF."departments d ON e.department_id = d.department_id
		WHERE e.inactive = 0
		GROUP BY d.department_id, d.department_name
		ORDER BY total DESC, d.department_name";

	if (!empty($limit)) {
		$limit = (int) $limit;
		$sql .= " LIMIT ".$limit;
	}

	return db_query($sql, _('Could not get employee summary by department'));
}

/**
 * Get active employee counts grouped into age buckets.
 *
 * @return array
 */
function hrm_employee_age_distribution() {
	$sql = "SELECT
		SUM(CASE WHEN birth_date IS NOT NULL AND birth_date != '0000-00-00' AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) < 20 THEN 1 ELSE 0 END) AS under_20,
		SUM(CASE WHEN birth_date IS NOT NULL AND birth_date != '0000-00-00' AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) >= 20 AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) < 30 THEN 1 ELSE 0 END) AS age_20_29,
		SUM(CASE WHEN birth_date IS NOT NULL AND birth_date != '0000-00-00' AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) >= 30 AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) < 40 THEN 1 ELSE 0 END) AS age_30_39,
		SUM(CASE WHEN birth_date IS NOT NULL AND birth_date != '0000-00-00' AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) >= 40 AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) < 50 THEN 1 ELSE 0 END) AS age_40_49,
		SUM(CASE WHEN birth_date IS NOT NULL AND birth_date != '0000-00-00' AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) >= 50 AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) < 60 THEN 1 ELSE 0 END) AS age_50_59,
		SUM(CASE WHEN birth_date IS NOT NULL AND birth_date != '0000-00-00' AND TIMESTAMPDIFF(YEAR, birth_date, CURDATE()) >= 60 THEN 1 ELSE 0 END) AS age_60_plus,
		SUM(CASE WHEN birth_date IS NULL OR birth_date = '0000-00-00' THEN 1 ELSE 0 END) AS unknown_age
		FROM ".TB_PREF."employees
		WHERE inactive = 0";

	$result = db_query($sql, _('Could not get employee age distribution'));
	$row = db_fetch_assoc($result);

	return array(
		_('Under 20') => (int) $row['under_20'],
		_('20-29') => (int) $row['age_20_29'],
		_('30-39') => (int) $row['age_30_39'],
		_('40-49') => (int) $row['age_40_49'],
		_('50-59') => (int) $row['age_50_59'],
		_('60+') => (int) $row['age_60_plus'],
		_('Unknown') => (int) $row['unknown_age'],
	);
}

function get_reorder_stock($today, $limit=null) {
	
	$sql = "SELECT sm.stock_id, sm.description, l.location_name, ls.reorder_level, COALESCE(SUM(sm2.qty), 0) AS QtyOnHand
		FROM ".TB_PREF."stock_master sm
		INNER JOIN ".TB_PREF."loc_stock ls ON sm.stock_id = ls.stock_id
		INNER JOIN ".TB_PREF."locations l ON ls.loc_code = l.loc_code
		LEFT JOIN ".TB_PREF."stock_moves sm2 ON sm.stock_id = sm2.stock_id
		WHERE sm.mb_flag IN ('B', 'M')
		AND ls.reorder_level > 0
		GROUP BY sm.stock_id, sm.description, l.location_name, ls.reorder_level
		HAVING COALESCE(SUM(sm2.qty), 0) < ls.reorder_level
		ORDER BY sm.stock_id, l.location_name";

	if (!empty($limit)) {
		$limit = (int)$limit;
		$sql .= " LIMIT $limit";
	}

	return db_query($sql, _('Could not get Items details'));
}

function get_low_stock($today, $limit=null) {
	
	$sql = "SELECT sm.stock_id, sm.description, l.location_name, ls.reorder_level, COALESCE(SUM(sm2.qty), 0) AS QtyOnHand,
		(COALESCE(SUM(sm2.qty), 0) / ls.reorder_level) AS stock_ratio
		FROM ".TB_PREF."stock_master sm
		INNER JOIN ".TB_PREF."loc_stock ls ON sm.stock_id = ls.stock_id
		INNER JOIN ".TB_PREF."locations l ON ls.loc_code = l.loc_code
		LEFT JOIN ".TB_PREF."stock_moves sm2 ON sm.stock_id = sm2.stock_id
		WHERE sm.mb_flag IN ('B', 'M')
		AND ls.reorder_level > 0
		GROUP BY sm.stock_id, sm.description, l.location_name, ls.reorder_level
		HAVING COALESCE(SUM(sm2.qty), 0) > ls.reorder_level
		ORDER BY stock_ratio DESC, sm.stock_id, l.location_name";

	if (!empty($limit)) {
		$limit = (int)$limit;
		$sql .= " LIMIT $limit";
	}
	
	return db_query($sql, _('Could not get Items details'));
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

function source_graphic($title, $x_axis, $pg, $graphic1, $graphic2='', $type=2) {
	global $SysPrefs;

	$pg->axis_x    = $x_axis;
	$pg->axis_y    = _('Amount');
	$pg->graphic_1 = empty($graphic1) ? '' : $graphic1;
	$pg->graphic_2 = empty($graphic2) ? '' : $graphic2;
	$pg->type      = $type;
	$pg->skin      = 1;
	$pg->built_in  = false;
	$pg->latin_notation = ($SysPrefs->decseps[user_dec_sep()] != '.');

	echo "<center>";
	$pg->display();
	echo "</center>";
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

function supplier_unpaid_invoices($today) {
	$today = date2sql($today);
	$sql = "SELECT trans.trans_no, trans.reference, trans.tran_date, trans.due_date, s.supplier_id, s.supp_name, s.curr_code, (trans.ov_amount + trans.ov_gst + trans.ov_discount) AS total, (trans.ov_amount + trans.ov_gst + trans.ov_discount - trans.alloc) AS remainder, DATEDIFF(trans.due_date, '$today') AS days FROM ".TB_PREF."supp_trans as trans, ".TB_PREF."suppliers as s WHERE s.supplier_id = trans.supplier_id AND trans.type = ".ST_SUPPINVOICE." AND (ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount) - trans.alloc) > ".FLOAT_COMP_DELTA." AND DATEDIFF('$today', trans.due_date) <= 0 ORDER BY days ASC";

	return db_query($sql, 'could not get supplier transactions');
}
