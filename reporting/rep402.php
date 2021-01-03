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
$page_security = 'SA_BOMREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Work Order Listing
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/includes/banking.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");
include_once($path_to_root . "/inventory/includes/db/items_category_db.inc");

//----------------------------------------------------------------------------------------------------

print_work_order_listing();

function getTransactions($items, $open_only, $location)
{
	$sql = "SELECT
		workorder.id,
		workorder.wo_ref,
		workorder.type,
		location.location_name,
		item.description,
		workorder.units_reqd,
		workorder.units_issued,
		workorder.date_,
		workorder.required_by,
		workorder.closed,
		workorder.stock_id
		FROM ".TB_PREF."workorders as workorder
			LEFT JOIN ".TB_PREF."voided v ON v.id=workorder.id and v.type=".ST_WORKORDER.","
 			.TB_PREF."stock_master as item,"
			.TB_PREF."locations as location
		WHERE ISNULL(v.id)
			AND workorder.stock_id=item.stock_id 
			AND workorder.loc_code=location.loc_code";

	if ($open_only != 0)
		$sql .= " AND workorder.closed=0";

	if ($location != '')
		$sql .= " AND workorder.loc_code=".db_escape($location);

	if ($items != '')
		$sql .= " AND workorder.stock_id=".db_escape($items);
	
	$sql .=" ORDER BY workorder.id";	

    return db_query($sql,"No transactions were returned");

}

function print_gl_rows(&$rep, $result, $title)
{
	global $systypes_array;

   	$dec = user_price_dec();

    if (db_num_rows($result))
    {
		$rep->Line($rep->row -= 4);
		$rep->NewLine();
		$rep->Font('italic');
		$rep->TextCol(3, 11, $title);
		$rep->Font();
		$rep->Line($rep->row -= 4);
		while($myrow = db_fetch($result)) {
			$rep->NewLine();
			$rep->TextCol(0, 2, $systypes_array[$myrow['type']] . ' ' . $myrow['type_no'], -2);
			$rep->TextCol(2, 3, sql2date($myrow["tran_date"]), -2);
			$rep->TextCol(3, 4, $myrow['account'], -2);
			$rep->TextCol(4, 5, $myrow['account_name'], -2);
			if ($myrow['amount'] > 0.0)
				$rep->AmountCol(5, 6, $myrow['amount'], $dec);
			else	
				$rep->AmountCol(6, 7, $myrow['amount'] * -1, $dec, -1);
			$rep->TextCol(8, 11, $myrow['memo_']);
		}
	}
}

//----------------------------------------------------------------------------------------------------

function print_work_order_listing()
{
    global $path_to_root, $wo_types_array;

    $item = $_POST['PARAM_0'];
    $location = $_POST['PARAM_1'];
    $open_only = $_POST['PARAM_2'];
    $show_gl = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];
	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");

	$orientation = ($orientation ? 'L' : 'P');

	if ($item == '')
		$items = _('All');
	else
	{
		$row = get_item($item);
		$items = $row['description']; 
	}

	if ($location == '')
		$loc = _('All');
	else
		$loc = get_location_name($location);

	$open = $open_only == 1 ? _('Yes') : _('No');
	$show = $show_gl == 1 ? _('Yes') : _('No');
	
	$cols = array(0, 100, 120, 165, 210, 275, 315, 375, 385, 440, 495, 555);

	$headers = array(_('Type'), '#', ('Reference'), _('Location'), _('Item'), _('Required'), _('Manufactured'), ' ', _('Date'), _('Required By'), _('Closed'));

	if ($show_gl)
	{
		$cols2 = $cols;
		$headers2 = array(_("Transaction"), ' ', _("Date"), _("Account Code"),' ' . _("Account Name"), _("Debit"), _("Credit"), ' ', _("Memo"));
	}	
	else
	{
		$cols2 = null;
		$headers2 = null;
	}	

	$aligns = array('left',	'left',	'left', 'left', 'left', 'right', 'right', 'left', 'left', 'left', 'left');

    $params =   array( 	0 => $comments,
    				    1 => array('text' => _('Items'), 'from' => $items, 'to' => ''),
    				    2 => array('text' => _('Location'), 'from' => $loc, 'to' => ''),
    				    3 => array('text' => _('Open Only'), 'from' => $open, 'to' => ''),
    				    4 => array('text' => _('Show GL Rows'), 'from' => $show, 'to' => ''),
    				    );

    $rep = new FrontReport(_('Work Order Listing'), "WorkOrderListing", user_pagesize(), 9, $orientation);
   	if ($orientation == 'L')
    	recalculate_cols($cols);

    $rep->Font();
    if ($show_gl)
    	$rep->Info($params, $cols2, $headers2, $aligns, $cols, $headers);
    else	
    	$rep->Info($params, $cols, $headers, $aligns);
    $rep->NewPage();

	$res = getTransactions($item, $open_only, $location);
	while ($trans=db_fetch($res))
	{
		$rep->TextCol(0, 1, $wo_types_array[$trans['type']]);
		$rep->TextCol(1, 2, $trans['id'], -1);
		$rep->TextCol(2, 3, $trans['wo_ref'], -1);
		$rep->TextCol(3, 4, $trans['location_name'], -1);
		$rep->TextCol(4, 5, $trans['description'], -1);
		$dec = get_qty_dec($trans['stock_id']);
		$rep->AmountCol(5, 6, $trans['units_reqd'], $dec);
		$rep->AmountCol(6, 7, $trans['units_issued'], $dec);
		$rep->TextCol(7, 8, '', -1);
		$rep->TextCol(8, 9, sql2date($trans['date_']), -1);
		$rep->TextCol(9, 10, sql2date($trans['required_by']), -1);
		$rep->TextCol(10, 11, $trans['closed'] ? ' ' : _('No'), -1);
		if ($show_gl)
		{
			$rep->NewLine();
			$productions = get_gl_wo_productions($trans['id'], true);
			print_gl_rows($rep, $productions, _("Finished Product Requirements"));

			$issues = get_gl_wo_issue_trans($trans['id'], -1, true);
			print_gl_rows($rep, $issues, _("Additional Material Issues"));

		    $costs = get_gl_wo_cost_trans($trans['id'], -1, true);
			print_gl_rows($rep, $costs, _("Additional Costs"));

			$wo = get_gl_trans(ST_WORKORDER, $trans['id']);
			print_gl_rows($rep, $wo, _("Finished Product Receival"));
			$rep->Line($rep->row - 2);
			$rep->NewLine();
		}
		$rep->NewLine();
	}
	$rep->Line($rep->row);
    $rep->End();
}

