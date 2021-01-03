<?php
/**********************************************************************
    Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
$page_security = 'SA_VIEWPRINTTRANSACTION';
$path_to_root = "..";

include($path_to_root . "/includes/db_pager.inc");
include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/admin/db/transactions_db.inc");

include_once($path_to_root . "/reporting/includes/reporting.inc");
$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
page(_($help_context = "View or Print Transactions"), false, false, "", $js);

//----------------------------------------------------------------------------------------
function view_link($trans)
{
	if (!isset($trans['type']))
		$trans['type'] = $_POST['filterType'];
	return get_trans_view_str($trans["type"], $trans["trans_no"]);
}

function prt_link($row)
{
	if (!isset($row['type']))
		$row['type'] = $_POST['filterType'];
  	if ($row['type'] == ST_PURCHORDER || $row['type'] == ST_SALESORDER || $row['type'] == ST_SALESQUOTE || 
  		$row['type'] == ST_WORKORDER)
 		return print_document_link($row['trans_no'], _("Print"), true, $row['type'], ICON_PRINT);
 	else	
		return print_document_link($row['trans_no']."-".$row['type'], _("Print"), true, $row['type'], ICON_PRINT);
}

function gl_view($row)
{
	if (!isset($row['type']))
		$row['type'] = $_POST['filterType'];
	return get_gl_view_str($row["type"], $row["trans_no"]);
}

function date_view($row)
{
	return $row['trans_date'];
}

function ref_view($row)
{
	return $row['ref'];
}

function viewing_controls()
{
	display_note(_("Only documents can be printed."));

    start_table(TABLESTYLE_NOBORDER);
	start_row();

	systypes_list_cells(_("Type:"), 'filterType', null, true, array(ST_CUSTOMER, ST_SUPPLIER));

	if (!isset($_POST['FromTransNo']))
		$_POST['FromTransNo'] = "1";
	if (!isset($_POST['ToTransNo']))
		$_POST['ToTransNo'] = "999999";

    ref_cells(_("from #:"), 'FromTransNo');

    ref_cells(_("to #:"), 'ToTransNo');

    submit_cells('ProcessSearch', _("Search"), '', '', 'default');

	end_row();
    end_table(1);

}

//----------------------------------------------------------------------------------------

function check_valid_entries()
{
	if (!is_numeric($_POST['FromTransNo']) OR $_POST['FromTransNo'] <= 0)
	{
		display_error(_("The starting transaction number is expected to be numeric and greater than zero."));
		return false;
	}

	if (!is_numeric($_POST['ToTransNo']) OR $_POST['ToTransNo'] <= 0)
	{
		display_error(_("The ending transaction number is expected to be numeric and greater than zero."));
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------

function handle_search()
{
	if (check_valid_entries()==true)
	{
		$trans_ref = false;
		$sql = get_sql_for_view_transactions(get_post('filterType'), get_post('FromTransNo'), get_post('ToTransNo'), $trans_ref);
		if ($sql == "")
			return;

		$print_type = get_post('filterType');
		$print_out = ($print_type == ST_SALESINVOICE || $print_type == ST_CUSTCREDIT || $print_type == ST_CUSTDELIVERY ||
			$print_type == ST_PURCHORDER || $print_type == ST_SALESORDER || $print_type == ST_SALESQUOTE ||
			$print_type == ST_CUSTPAYMENT || $print_type == ST_SUPPAYMENT || $print_type == ST_WORKORDER);

		$cols = array(
			_("#") => array('insert'=>true, 'fun'=>'view_link'), 
			_("Reference") => array('fun'=>'ref_view'), 
			_("Date") => array('type'=>'date', 'fun'=>'date_view'),
			_("Print") => array('insert'=>true, 'fun'=>'prt_link'), 
			_("GL") => array('insert'=>true, 'fun'=>'gl_view')
		);
		if(!$print_out) {
			array_remove($cols, 3);
		}
		if(!$trans_ref) {
			array_remove($cols, 1);
		}

		$table =& new_db_pager('transactions', $sql, $cols);
		$table->width = "40%";
		display_db_pager($table);
	}

}

//----------------------------------------------------------------------------------------

if (isset($_POST['ProcessSearch']))
{
	if (!check_valid_entries())
		unset($_POST['ProcessSearch']);
	$Ajax->activate('transactions');
}

//----------------------------------------------------------------------------------------

start_form(false);
	viewing_controls();
	handle_search();
end_form(2);

end_page();

