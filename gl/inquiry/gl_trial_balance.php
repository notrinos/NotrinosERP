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
$page_security = 'SA_GLANALYTIC';
$path_to_root="../..";

include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/admin/db/fiscalyears_db.inc");
include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/gl/includes/gl_db.inc");

$js = "";
if (user_use_date_picker())
	$js = get_js_date_picker();

page(_($help_context = "Trial Balance"), false, false, "", $js);

$k = 0;
$pdeb = $pcre = $cdeb = $ccre = $tdeb = $tcre = $pbal = $cbal = $tbal = 0;

//----------------------------------------------------------------------------------------------------
// Ajax updates
//
if (get_post('Show'))
{
	$Ajax->activate('balance_tbl');
}


function gl_inquiry_controls()
{
	$dim = get_company_pref('use_dimension');
    start_form();

    start_table(TABLESTYLE_NOBORDER);

	$date = today();
	if (!isset($_POST['TransToDate']))
		$_POST['TransToDate'] = end_month($date);
	if (!isset($_POST['TransFromDate']))
		$_POST['TransFromDate'] = add_days(end_month($date), -user_transaction_days());
	start_row();	
    date_cells(_("From:"), 'TransFromDate');
	date_cells(_("To:"), 'TransToDate');
	if ($dim >= 1)
		dimensions_list_cells(_("Dimension")." 1:", 'Dimension', null, true, " ", false, 1);
	if ($dim > 1)
		dimensions_list_cells(_("Dimension")." 2:", 'Dimension2', null, true, " ", false, 2);
	check_cells(_("No zero values"), 'NoZero', null);
	check_cells(_("Only balances"), 'Balance', null);
	check_cells(_("Group totals only"), 'GroupTotalOnly', null);
	submit_cells('Show',_("Show"),'','', 'default');
	end_row();
    end_table();
    end_form();
}

//----------------------------------------------------------------------------------------------------

function display_trial_balance($type, $typename)
{
	global $path_to_root, $SysPrefs,
		 $k, $pdeb, $pcre, $cdeb, $ccre, $tdeb, $tcre, $pbal, $cbal, $tbal;

	$printtitle = 0; //Flag for printing type name

	$k = 0;

	//Get Accounts directly under this group/type
	$accounts = get_gl_accounts(null, null, $type);

	$begin = get_fiscalyear_begin_for_date($_POST['TransFromDate']);
	if (date1_greater_date2($begin, $_POST['TransFromDate']))
		$begin = $_POST['TransFromDate'];
	$begin = add_days($begin, -1);

	$Apdeb=$pdeb;
	$Apcre=$pcre;
	$Acdeb=$cdeb;
	$Accre=$ccre;
	$Atdeb=$tdeb;
	$Atcre=$tcre;
	$Apbal=$pbal;
	$Acbal=$cbal;
	$Atbal=$tbal;

	while ($account = db_fetch($accounts))
	{
		//Print Type Title if it has atleast one non-zero account
		if (!$printtitle)
		{
			if (!check_value('GroupTotalOnly'))
			{
				start_row("class='inquirybg' style='font-weight:bold'");
				label_cell(_("Group")." - ".$type ." - ".$typename, "colspan=8");
				end_row();
			}
			$printtitle = 1;
		}

		// FA doesn't really clear the closed year, therefore the brought forward balance includes all the transactions from the past, even though the balance is null.
		// If we want to remove the balanced part for the past years, this option removes the common part from from the prev and tot figures.
		if (@$SysPrefs->clear_trial_balance_opening)
		{
			$open = get_balance($account["account_code"], $_POST['Dimension'], $_POST['Dimension2'], $begin,  $begin, false, true);
			$offset = min($open['debit'], $open['credit']);
		} else
			$offset = 0;

		$prev = get_balance($account["account_code"], $_POST['Dimension'], $_POST['Dimension2'], $begin, $_POST['TransFromDate'], false, false);
		$curr = get_balance($account["account_code"], $_POST['Dimension'], $_POST['Dimension2'], $_POST['TransFromDate'], $_POST['TransToDate'], true, true);
		$tot = get_balance($account["account_code"], $_POST['Dimension'], $_POST['Dimension2'], $begin, $_POST['TransToDate'], false, true);
		if (check_value("NoZero") && !$prev['balance'] && !$curr['balance'] && !$tot['balance'])
			continue;
		if (!check_value('GroupTotalOnly'))
		{
			alt_table_row_color($k);

			$url = "<a href='$path_to_root/gl/inquiry/gl_account_inquiry.php?TransFromDate=" . $_POST["TransFromDate"] . "&TransToDate=" . $_POST["TransToDate"] . "&account=" . $account["account_code"] . "&Dimension=" . $_POST["Dimension"] . "&Dimension2=" . $_POST["Dimension2"] . "'>" . $account["account_code"] . "</a>";

			label_cell($url);
			label_cell($account["account_name"]);
		}
		if (check_value('Balance'))
		{
			if (!check_value('GroupTotalOnly'))
			{
				display_debit_or_credit_cells($prev['balance']);
				display_debit_or_credit_cells($curr['balance']);
				display_debit_or_credit_cells($tot['balance']);
			}
		}
		else
		{
			if (!check_value('GroupTotalOnly'))
			{
				amount_cell($prev['debit']-$offset);
				amount_cell($prev['credit']-$offset);
				amount_cell($curr['debit']);
				amount_cell($curr['credit']);
				amount_cell($tot['debit']-$offset);
				amount_cell($tot['credit']-$offset);
			}
			$pdeb += $prev['debit'];
			$pcre += $prev['credit'];
			$cdeb += $curr['debit'];
			$ccre += $curr['credit'];
			$tdeb += $tot['debit'];
			$tcre += $tot['credit'];
		}
		$pbal += $prev['balance'];
		$cbal += $curr['balance'];
		$tbal += $tot['balance'];
		end_row();
	}

	//Get Account groups/types under this group/type
	$result = get_account_types(false, false, $type);
	while ($accounttype=db_fetch($result))
	{
		//Print Type Title if has sub types and not previously printed
		if (!$printtitle)
		{
			start_row("class='inquirybg' style='font-weight:bold'");
			label_cell(_("Group")." - ".$type ." - ".$typename, "colspan=8");
			end_row();
			$printtitle = 1;

		}
		display_trial_balance($accounttype["id"], $accounttype["name"].' ('.$typename.')');
	}

	start_row("class='inquirybg' style='font-weight:bold'");
	if (!check_value('GroupTotalOnly'))
		label_cell(_("Total") ." - ".$typename, "colspan=2");
	else
		label_cell(" - ".$typename, "colspan=2");


	if (!check_value('Balance'))
	{
		amount_cell($pdeb-$Apdeb );
		amount_cell($pcre-$Apcre);
		amount_cell($cdeb-$Acdeb );
		amount_cell($ccre-$Accre );
		amount_cell($tdeb-$Atdeb );
		amount_cell($tcre-$Atcre);
	}
	else
	{
		display_debit_or_credit_cells($pbal-$Apbal);
		display_debit_or_credit_cells($cbal-$Acbal );
		display_debit_or_credit_cells($tbal-$Atbal);
	}
	end_row();
}

//----------------------------------------------------------------------------------------------------

gl_inquiry_controls();

if (isset($_POST['TransFromDate']))
{
	$row = get_current_fiscalyear();
	if (date1_greater_date2($_POST['TransFromDate'], sql2date($row['end'])))
	{
		display_error(_("The from date cannot be bigger than the fiscal year end."));
		set_focus('TransFromDate');
		return;
	}
}
div_start('balance_tbl');
if (!isset($_POST['Dimension']))
	$_POST['Dimension'] = 0;
if (!isset($_POST['Dimension2']))
	$_POST['Dimension2'] = 0;
start_table(TABLESTYLE);
$tableheader =  "<tr>
	<td rowspan=2 class='tableheader'>" . _("Account") . "</td>
	<td rowspan=2 class='tableheader'>" . _("Account Name") . "</td>
	<td colspan=2 class='tableheader'>" . _("Brought Forward") . "</td>
	<td colspan=2 class='tableheader'>" . _("This Period") . "</td>
	<td colspan=2 class='tableheader'>" . _("Balance") . "</td>
	</tr><tr>
	<td class='tableheader'>" . _("Debit") . "</td>
	<td class='tableheader'>" . _("Credit") . "</td>
	<td class='tableheader'>" . _("Debit") . "</td>
	<td class='tableheader'>" . _("Credit") . "</td>
	<td class='tableheader'>" . _("Debit") . "</td>
	<td class='tableheader'>" . _("Credit") . "</td>
	</tr>";

echo $tableheader;

//display_trial_balance();

$classresult = get_account_classes(false);
while ($class = db_fetch($classresult))
{
	start_row("class='inquirybg' style='font-weight:bold'");
	label_cell(_("Class")." - ".$class['cid'] ." - ".$class['class_name'], "colspan=8");
	end_row();

	//Get Account groups/types under this group/type with no parents
	$typeresult = get_account_types(false, $class['cid'], -1);
	while ($accounttype=db_fetch($typeresult))
	{
		display_trial_balance($accounttype["id"], $accounttype["name"]);
	}
}

if (!check_value('Balance'))
{
	start_row("class='inquirybg' style='font-weight:bold'");
	label_cell(_("Total") ." - ".$_POST['TransToDate'], "colspan=2");
	amount_cell($pdeb);
	amount_cell($pcre);
	amount_cell($cdeb);
	amount_cell($ccre);
	amount_cell($tdeb);
	amount_cell($tcre);
	end_row();
}
start_row("class='inquirybg' style='font-weight:bold'");
label_cell(_("Ending Balance") ." - ".$_POST['TransToDate'], "colspan=2");
display_debit_or_credit_cells($pbal);
display_debit_or_credit_cells($cbal);
display_debit_or_credit_cells($tbal);
end_row();

end_table(1);
if (($pbal = round2($pbal, user_price_dec())) != 0 && $_POST['Dimension'] == 0 && $_POST['Dimension2'] == 0)
	display_warning(_("The Opening Balance is not in balance, probably due to a non closed Previous Fiscalyear."));
div_end();

//----------------------------------------------------------------------------------------------------

end_page();

