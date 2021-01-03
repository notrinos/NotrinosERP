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
$page_security = 'SA_SALESBULKREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Order Status List
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");
include_once($path_to_root . "/inventory/includes/db/items_category_db.inc");

//----------------------------------------------------------------------------------------------------

print_order_status_list();

//----------------------------------------------------------------------------------------------------

function GetSalesOrders($from, $to, $category=0, $location=null, $backorder=0)
{
	$fromdate = date2sql($from);
	$todate = date2sql($to);

	$sql= "SELECT sorder.order_no,
				sorder.debtor_no,
                sorder.branch_code,
                sorder.customer_ref,
                sorder.ord_date,
                sorder.from_stk_loc,
                sorder.delivery_date,
                sorder.total,
                line.stk_code,
                item.description,
                item.units,
                line.quantity,
                line.qty_sent
            FROM ".TB_PREF."sales_orders sorder
	           	INNER JOIN ".TB_PREF."sales_order_details line
            	    ON sorder.order_no = line.order_no
            	    AND sorder.trans_type = line.trans_type
            	    AND sorder.trans_type = ".ST_SALESORDER."
            	INNER JOIN ".TB_PREF."stock_master item
            	    ON line.stk_code = item.stock_id
            WHERE sorder.ord_date >='$fromdate'
                AND sorder.ord_date <='$todate'";
	if ($category > 0)
		$sql .= " AND item.category_id=".db_escape($category);
	if ($location != null)
		$sql .= " AND sorder.from_stk_loc=".db_escape($location);
	if ($backorder)
		$sql .= " AND line.quantity - line.qty_sent > 0";
	$sql .= " ORDER BY sorder.order_no";

	return db_query($sql, "Error getting order details");
}

//----------------------------------------------------------------------------------------------------

function print_order_status_list()
{
	global $path_to_root;

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$category = $_POST['PARAM_2'];
	$location = $_POST['PARAM_3'];
	$backorder = $_POST['PARAM_4'];
	$comments = $_POST['PARAM_5'];
	$orientation = $_POST['PARAM_6'];
	$destination = $_POST['PARAM_7'];
	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");
	$orientation = ($orientation ? 'L' : 'P');

	if ($category == ALL_NUMERIC)
		$category = 0;
	if ($location == ALL_TEXT)
		$location = null;
	if ($category == 0)
		$cat = _('All');
	else
		$cat = get_category_name($category);
	if ($location == null)
		$loc = _('All');
	else
		$loc = get_location_name($location);
	if ($backorder == 0)
		$back = _('All Orders');
	else
		$back = _('Back Orders Only');

	$cols = array(0, 60, 150, 260, 325,	385, 450, 515);

	$headers2 = array(_('Order'), _('Customer'), _('Branch'), _('Customer Ref'),
		_('Ord Date'),	_('Del Date'),	_('Loc'));

	$aligns = array('left',	'left',	'right', 'right', 'right', 'right',	'right');

	$headers = array(_('Code'),	_('Description'), _('Ordered'),	_('Delivered'),
		_('Outstanding'), '', _('Total Amount'));

    $params =   array( 	0 => $comments,
	    				1 => array(  'text' => _('Period'), 'from' => $from, 'to' => $to),
	    				2 => array(  'text' => _('Category'), 'from' => $cat,'to' => ''),
	    				3 => array(  'text' => _('Location'), 'from' => $loc, 'to' => ''),
	    				4 => array(  'text' => _('Selection'),'from' => $back,'to' => ''));

	$aligns2 = $aligns;

	$rep = new FrontReport(_('Order Status Listing'), "OrderStatusListing", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    	recalculate_cols($cols);
	$cols2 = $cols;
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns, $cols2, $headers2, $aligns2);

	$rep->NewPage();
	$orderno = 0;
	$grand_total = 0;

	$result = GetSalesOrders($from, $to, $category, $location, $backorder);
	while ($myrow=db_fetch($result))
	{
		$rep->NewLine(0, 2, false, $orderno);
		if ($orderno != $myrow['order_no'])
		{
			if ($orderno != 0)
			{
				$rep->Line($rep->row);
				$rep->NewLine();
			}
			$rep->TextCol(0, 1,	$myrow['order_no']);
			$rep->TextCol(1, 2,	get_customer_name($myrow['debtor_no']));
			$rep->TextCol(2, 3,	get_branch_name($myrow['branch_code']));
			$rep->TextCol(3, 4,	$myrow['customer_ref']);
			$rep->DateCol(4, 5,	$myrow['ord_date'], true);
			$rep->DateCol(5, 6,	$myrow['delivery_date'], true);
			$rep->TextCol(6, 7,	$myrow['from_stk_loc']);
			$rep->NewLine(1);
			$rep->AmountCol(6, 7, $myrow['total']);
			$rep->NewLine(1);
			$grand_total += $myrow['total'];		
			$orderno = $myrow['order_no'];
		}
		$rep->TextCol(0, 1,	$myrow['stk_code']);
		$rep->TextCol(1, 2,	$myrow['description']);
		$dec = get_qty_dec($myrow['stk_code']);
		$rep->AmountCol(2, 3, $myrow['quantity'], $dec);
		$rep->AmountCol(3, 4, $myrow['qty_sent'], $dec);
		$rep->AmountCol(4, 5, $myrow['quantity'] - $myrow['qty_sent'], $dec);
		if ($myrow['quantity'] - $myrow['qty_sent'] > 0)
		{
			$rep->Font('italic');
			$rep->TextCol(5, 6,	_('Outstanding'));
			$rep->Font();
		}
		$rep->NewLine();
	}
	$rep->Line($rep->row);
	$rep->NewLine();
	$rep->TextCol(1, 6, _("Grand Total")); 
	$rep->AmountCol(6, 7, $grand_total);
	$rep->Line($rep->row - 5);
	$rep->End();
}

