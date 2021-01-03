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
$page_security = 'SA_PRICEREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	price Listing
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui/ui_input.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");
include_once($path_to_root . "/sales/includes/db/sales_types_db.inc");
include_once($path_to_root . "/inventory/includes/inventory_db.inc");

//----------------------------------------------------------------------------------------------------

print_price_listing();

function fetch_items($category=0)
{
		$sql = "SELECT item.stock_id, item.description AS name,
				item.material_cost AS Standardcost,
				item.category_id,item.units,
				category.description
			FROM ".TB_PREF."stock_master item,
				".TB_PREF."stock_category category
			WHERE item.category_id=category.category_id AND NOT item.inactive";
		if ($category != 0)
			$sql .= " AND category.category_id = ".db_escape($category);
		$sql .= " AND item.mb_flag<> 'F' ORDER BY item.category_id,
				item.stock_id";

    return db_query($sql,"No transactions were returned");
}

function get_kits($category=0)
{
	$sql = "SELECT i.item_code AS kit_code, i.description AS kit_name, c.category_id AS cat_id, c.description AS cat_name, count(*)>1 AS kit
			FROM
				".TB_PREF."item_codes i
				LEFT JOIN ".TB_PREF."stock_category c ON i.category_id=c.category_id
			WHERE !i.is_foreign AND i.item_code!=i.stock_id";
	if ($category != 0)
		$sql .= " AND c.category_id = ".db_escape($category);
	$sql .= " GROUP BY i.item_code";
    return db_query($sql,"No kits were returned");
}

//----------------------------------------------------------------------------------------------------

function print_price_listing()
{
    global $path_to_root, $SysPrefs;

    $currency = $_POST['PARAM_0'];
    $category = $_POST['PARAM_1'];
    $salestype = $_POST['PARAM_2'];
    $pictures = $_POST['PARAM_3'];
    $showGP = $_POST['PARAM_4'];
    $comments = $_POST['PARAM_5'];
	$orientation = $_POST['PARAM_6'];
	$destination = $_POST['PARAM_7'];
	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");

	$orientation = ($orientation ? 'L' : 'P');
    $dec = user_price_dec();

	$home_curr = get_company_pref('curr_default');
	if ($currency == ALL_TEXT)
		$currency = $home_curr;
	$curr = get_currency($currency);
	$curr_sel = $currency . " - " . $curr['currency'];
	if ($category == ALL_NUMERIC)
		$category = 0;
	if ($salestype == ALL_NUMERIC)
		$salestype = 0;
	if ($category == 0)
		$cat = _('All');
	else
		$cat = get_category_name($category);
	if ($salestype == 0)
		$stype = _('All');
	else
		$stype = get_sales_type_name($salestype);
	if ($showGP == 0)
		$GP = _('No');
	else
		$GP = _('Yes');

	$cols = array(0, 100, 360, 385, 450, 515);

	$headers = array(_('Category/Items'), _('Description'),	_('UOM'), _('Price'),	_('GP %'));

	$aligns = array('left',	'left',	'left', 'right', 'right');

    $params =   array( 	0 => $comments,
    				    1 => array('text' => _('Currency'), 'from' => $curr_sel, 'to' => ''),
    				    2 => array('text' => _('Category'), 'from' => $cat, 'to' => ''),
    				    3 => array('text' => _('Sales Type'), 'from' => $stype, 'to' => ''),
    				    4 => array(  'text' => _('Show GP %'),'from' => $GP,'to' => ''));

	if ($pictures)
		$user_comp = user_company();
	else
		$user_comp = "";

    $rep = new FrontReport(_('Price Listing'), "PriceListing", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    	recalculate_cols($cols);

    $rep->Font();
    $rep->Info($params, $cols, $headers, $aligns);
    $rep->NewPage();

	$result = fetch_items($category);

	$catgor = '';
	$_POST['sales_type_id'] = $salestype;
	while ($myrow=db_fetch($result))
	{
		if ($catgor != $myrow['description'])
		{
			$rep->Line($rep->row  - $rep->lineHeight);
			$rep->NewLine(2);
			$rep->fontSize += 2;
			$rep->TextCol(0, 3, $myrow['category_id'] . " - " . $myrow['description']);
			$catgor = $myrow['description'];
			$rep->fontSize -= 2;
			$rep->NewLine();
		}
		$rep->NewLine();
		$rep->TextCol(0, 1,	$myrow['stock_id']);
		$rep->TextCol(1, 2, $myrow['name']);
		$rep->TextCol(2, 3, $myrow['units']);
		$price = get_price($myrow['stock_id'], $currency, $salestype);
		$rep->AmountCol(3, 4, $price, $dec);
		if ($showGP)
		{
			$price2 = get_price($myrow['stock_id'], $home_curr, $salestype);
			if ($price2 != 0.0)
				$disp = ($price2 - $myrow['Standardcost']) * 100 / $price2;
			else
				$disp = 0.0;
			$rep->TextCol(4, 5,	number_format2($disp, user_percent_dec()) . " %");
		}
		if ($pictures)
		{
			$image = company_path(). "/images/"
				. item_img_name($myrow['stock_id']) . ".jpg";
			if (file_exists($image))
			{
				$rep->NewLine();
				if ($rep->row - $SysPrefs->pic_height < $rep->bottomMargin)
					$rep->NewPage();
				$rep->AddImage($image, $rep->cols[1], $rep->row - $SysPrefs->pic_height, 0, $SysPrefs->pic_height);
				$rep->row -= $SysPrefs->pic_height;
				$rep->NewLine();
			}
		}
		else
			$rep->NewLine(0, 1);
	}
	$rep->Line($rep->row  - 4);

	$result = get_kits($category);

	$catgor = '';
	while ($myrow=db_fetch($result))
	{
		if ($catgor != $myrow['cat_name'])
		{
			if ($catgor == '')
			{
				$rep->NewLine(2);
				$rep->fontSize += 2;
				$rep->TextCol(0, 3, _("Sales Kits"));
				$rep->fontSize -= 2;
			}
			$rep->Line($rep->row  - $rep->lineHeight);
			$rep->NewLine(2);
			$rep->fontSize += 2;
			$rep->TextCol(0, 3, $myrow['cat_id'] . " - " . $myrow['cat_name']);
			$catgor = $myrow['cat_name'];
			$rep->fontSize -= 2;
			$rep->NewLine();
		}
		$rep->NewLine();
		$rep->TextCol(0, 1,	$myrow['kit_code']);
		$rep->TextCol(1, 3, $myrow['kit_name']);
		$price = get_kit_price($myrow['kit_code'], $currency, $salestype);
		$rep->AmountCol(3, 4, $price, $dec);
		$rep->NewLine(0, 1);
	}
	$rep->Line($rep->row  - 4);
	$rep->NewLine();
    $rep->End();
}

