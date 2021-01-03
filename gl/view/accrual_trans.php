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
$page_security = 'SA_ACCRUALS';
$path_to_root = "../..";
include_once($path_to_root . "/includes/session.inc");

$_SESSION['page_title'] = _($help_context = _("Search General Ledger Transactions for account: ").$_GET['act']);

page($_SESSION['page_title'], true);

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");

include_once($path_to_root . "/gl/includes/gl_db.inc");

$js ="\n<script type='text/javascript'>\n"
		. "<!--\n"
		. " function WindowClose(amount_, date__)\n"
		. "{\n"
		. " opener.document.getElementsByName('amount')[0].value = amount_; \n"
		. " opener.document.getElementsByName('date_')[0].value = date__; \n"
		. " window.close();\n"
		. " return true;\n"
		. "}\n"
		. "-->\n"
		. "</script>\n";
echo $js;

if (!isset($_GET['act']) || !isset($_GET['date']))
{ /*Script was not passed the correct parameters */

	echo "<p>" . _("The script must be called with a valid transaction type and transaction number to review the general ledger postings for.") . "</p>";
	exit;
}

display_heading($_SESSION['page_title']. " ".get_gl_account_name($_GET['act']));

br();

start_table(TABLESTYLE);
$dim = get_company_pref('use_dimension');

$first_cols = array(_("Type"), "#", _("Date"));
if ($dim == 2)
	$dim_cols = array(_("Dimension")." 1", _("Dimension")." 2");
elseif ($dim == 1)
	$dim_cols = array(_("Dimension"));
else
	$dim_cols = array();

$remaining_cols = array(_("Person/Item"), _("Debit"), _("Credit"), _("Memo"));

$th = array_merge($first_cols, $dim_cols, $remaining_cols);

table_header($th);
$end = $_GET['date'];
$account = $_GET['act'];
$begin = add_days($end, -user_transaction_days());

$result = get_gl_transactions($begin, $end, -1,	$account, 0, 0, null);
$j = 0;
$k = 1;
while ($myrow = db_fetch($result))
{
	alt_table_row_color($k);

	$trandate = sql2date($myrow["tran_date"]);

	label_cell($systypes_array[$myrow["type"]]);
	$amount = price_format($myrow["amount"]);
	$str = "<a href='#' onclick='return WindowClose(\"$amount\", \"$trandate\");' >".$myrow['type_no']."</a>";
	label_cell($str);
	label_cell($trandate);

	if ($dim >= 1)
		label_cell(get_dimension_string($myrow['dimension_id'], true));
	if ($dim > 1)
		label_cell(get_dimension_string($myrow['dimension2_id'], true));
	label_cell(payment_person_name($myrow["person_type_id"],$myrow["person_id"]));
	display_debit_or_credit_cells($myrow["amount"]);
	label_cell($myrow['memo_']);
	end_row();

	$j++;
	if ($j == 12)
	{
		$j = 1;
		table_header($th);
	}
}
//end of while loop


//end of while loop
end_table(1);

end_page(true);

