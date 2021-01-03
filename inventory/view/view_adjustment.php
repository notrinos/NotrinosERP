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
$page_security = 'SA_ITEMSTRANSVIEW';
$path_to_root = "../..";

include($path_to_root . "/includes/session.inc");

page(_($help_context = "View Inventory Adjustment"), true);

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/inventory/includes/inventory_db.inc");

if (isset($_GET["trans_no"]))
{
	$trans_no = $_GET["trans_no"];
}

display_heading($systypes_array[ST_INVADJUST] . " #$trans_no");

br(1);
$adjustment_items = get_stock_adjustment_items($trans_no);
$k = 0;
$header_shown = false;
while ($adjustment = db_fetch($adjustment_items))
{

	if (!$header_shown)
	{

		start_table(TABLESTYLE2, "width='90%'");
		start_row();
		label_cells(_("At Location"), $adjustment['location_name'], "class='tableheader2'");
    	label_cells(_("Reference"), $adjustment['reference'], "class='tableheader2'", "colspan=6");
		label_cells(_("Date"), sql2date($adjustment['tran_date']), "class='tableheader2'");
		end_row();
		comments_display_row(ST_INVADJUST, $trans_no);

		end_table();
		$header_shown = true;

		echo "<br>";
		start_table(TABLESTYLE, "width='90%'");

    	$th = array(_("Item Code"), _("Description"), _("Quantity"),
    		_("Units"), _("Unit Cost"));
    	table_header($th);
	}

    alt_table_row_color($k);

    label_cell($adjustment['stock_id']);
    label_cell($adjustment['description']);
    qty_cell($adjustment['qty'], false, get_qty_dec($adjustment['stock_id']));
    label_cell($adjustment['units']);
    amount_decimal_cell($adjustment['standard_cost']);
    end_row();
}

end_table(1);

is_voided_display(ST_INVADJUST, $trans_no, _("This adjustment has been voided."));

end_page(true, false, false, ST_INVADJUST, $trans_no);
