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
$page_security = 'SA_RECONCILE';
$path_to_root = "..";
include($path_to_root . "/includes/db_pager.inc");
include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/gl/includes/gl_db.inc");
include_once($path_to_root . "/includes/banking.inc");

$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

add_js_file('reconcile.js');

page(_($help_context = "Reconcile Bank Account"), false, false, "", $js);

check_db_has_bank_accounts(_("There are no bank accounts defined in the system."));

function check_date() {
	if (!is_date(get_post('reconcile_date'))) {
		display_error(_("Invalid reconcile date format"));
		set_focus('reconcile_date');
		return false;
	}
	return true;
}
//
//	This function can be used directly in table pager 
//	if we would like to change page layout.
//
function rec_checkbox($row)
{
	$name = "rec_" .$row['id'];
	$hidden = 'last['.$row['id'].']';
	$value = $row['reconciled'] != '';

// save also in hidden field for testing during 'Reconcile'
	return is_closed_trans($row['type'], $row['trans_no']) ? "--" : checkbox(null, $name, $value, true, _('Reconcile this transaction'))
 		. hidden($hidden, $value, false);
}

function systype_name($dummy, $type)
{
	global $systypes_array;
	
	return $systypes_array[$type];
}

function trans_view($trans)
{
	return get_trans_view_str($trans["type"], $trans["trans_no"]);
}

function gl_view($row)
{
	return get_gl_view_str($row["type"], $row["trans_no"]);
}

function fmt_debit($row)
{
	$value = $row["amount"];
	return $value>=0 ? price_format($value) : '';
}

function fmt_credit($row)
{
	$value = -$row["amount"];
	return $value>0 ? price_format($value) : '';
}

function fmt_person($trans)
{
	return get_counterparty_name($trans["type"], $trans["trans_no"]);
}

function fmt_memo($row)
{
	$value = $row["memo_"];
	return $value;
}

function update_data()
{
	global $Ajax;
	
	unset($_POST["beg_balance"]);
	unset($_POST["end_balance"]);
	$Ajax->activate('summary');
}
//---------------------------------------------------------------------------------------------
// Update db record if respective checkbox value has changed.
//
function change_tpl_flag($reconcile_id)
{
	global	$Ajax;

	if (!check_date() 
		&& check_value("rec_".$reconcile_id)) // temporary fix
		return false;

	if (get_post('bank_date')=='')	// new reconciliation
		$Ajax->activate('bank_date');

	$_POST['bank_date'] = date2sql(get_post('reconcile_date'));
	$reconcile_value = check_value("rec_".$reconcile_id) 
						? ("'".$_POST['bank_date'] ."'") : 'NULL';
	
	update_reconciled_values($reconcile_id, $reconcile_value, $_POST['reconcile_date'],
		input_num('end_balance'), $_POST['bank_account']);
		
	$Ajax->activate('reconciled');
	$Ajax->activate('difference');
	return true;
}

function set_tpl_flag($reconcile_id)
{
	global	$Ajax;

	if (check_value("rec_".$reconcile_id))
		return;

	if (get_post('bank_date')=='')	// new reconciliation
		$Ajax->activate('bank_date');

	$_POST['bank_date'] = date2sql(get_post('reconcile_date'));
	$reconcile_value =  ("'".$_POST['bank_date'] ."'");
	
	update_reconciled_values($reconcile_id, $reconcile_value, $_POST['reconcile_date'],
		input_num('end_balance'), $_POST['bank_account']);
		
	$Ajax->activate('reconciled');
	$Ajax->activate('difference');
}

if (!isset($_POST['reconcile_date'])) { // init page
	$_POST['reconcile_date'] = new_doc_date();
//	$_POST['bank_date'] = date2sql(Today());
}

if (list_updated('bank_account')) {
    $Ajax->activate('bank_date');
	update_data();
}
if (list_updated('bank_date')) {
	$_POST['reconcile_date'] = 
		get_post('bank_date')=='' ? Today() : sql2date($_POST['bank_date']);
	update_data();
}
if (get_post('_reconcile_date_changed')) {
	$_POST['bank_date'] = check_date() ? date2sql(get_post('reconcile_date')) : '';
    $Ajax->activate('bank_date');
	update_data();
}

$id = find_submit('_rec_');
if ($id != -1) 
	change_tpl_flag($id);


if (isset($_POST['Reconcile'])) {
	set_focus('bank_date');
	foreach($_POST['last'] as $id => $value)
		if ($value != check_value('rec_'.$id))
			if(!change_tpl_flag($id)) break;

    $Ajax->activate('_page_body');
}

if (isset($_POST['ReconcileAll'])) {
	set_focus('bank_date');
	foreach($_POST['last'] as $id => $value)
		set_tpl_flag($id);

    $Ajax->activate('_page_body');
}

//------------------------------------------------------------------------------------------------
start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
bank_accounts_list_cells(_("Account:"), 'bank_account', null, true);

bank_reconciliation_list_cells(_("Bank Statement:"), get_post('bank_account'),
	'bank_date', null, true, _("New"));
end_row();
end_table();

$result = get_max_reconciled(get_post('reconcile_date'), $_POST['bank_account']);

if ($row = db_fetch($result)) {
	$_POST["reconciled"] = price_format($row["end_balance"]-$row["beg_balance"]);
	$total = $row["total"];
	if (!isset($_POST["beg_balance"])) { // new selected account/statement
		$_POST["last_date"] = sql2date($row["last_date"]);
		$_POST["beg_balance"] = price_format($row["beg_balance"]);
		$_POST["end_balance"] = price_format($row["end_balance"]);
		if (get_post('bank_date')) {
			// if it is the last updated bank statement retrieve ending balance

			$row = get_ending_reconciled($_POST['bank_account'], $_POST['bank_date']);
			if($row) {
				$_POST["end_balance"] = price_format($row["ending_reconcile_balance"]);
			}
		}
	} 
}

echo "<hr>";

div_start('summary');

start_table(TABLESTYLE);
$th = array(_("Reconcile Date"), _("Beginning<br>Balance"), 
	_("Ending<br>Balance"), _("Account<br>Total"),_("Reconciled<br>Amount"), _("Difference"));
table_header($th);
start_row();

date_cells("", "reconcile_date", _('Date of bank statement to reconcile'), 
	get_post('bank_date')=='', 0, 0, 0, null, true);

amount_cells_ex("", "beg_balance", 15);

amount_cells_ex("", "end_balance", 15);

$reconciled = input_num('reconciled');
$difference = input_num("end_balance") - input_num("beg_balance") - $reconciled;

amount_cell($total);
amount_cell($reconciled, false, '', "reconciled");
amount_cell($difference, false, '', "difference");

end_row();
end_table();
div_end();
echo "<hr>";
//------------------------------------------------------------------------------------------------

if (!isset($_POST['bank_account']))
    $_POST['bank_account'] = "";

$sql = get_sql_for_bank_account_reconcile(get_post('bank_account'), get_post('reconcile_date'));

$act = get_bank_account($_POST["bank_account"]);
display_heading($act['bank_account_name']." - ".$act['bank_curr_code']);

	$cols =
	array(
		_("Type") => array('fun'=>'systype_name', 'ord'=>''),
		_("#") => array('fun'=>'trans_view', 'ord'=>''),
		_("Reference"), 
		_("Date") => 'date',
		_("Debit") => array('align'=>'right', 'fun'=>'fmt_debit'), 
		_("Credit") => array('align'=>'right','insert'=>true, 'fun'=>'fmt_credit'), 
	    _("Person/Item") => array('fun'=>'fmt_person'), 
		_("Memo") => array('fun'=>'fmt_memo'),
		array('insert'=>true, 'fun'=>'gl_view'),
		"X"=>array('insert'=>true, 'fun'=>'rec_checkbox')
	   );
	$table =& new_db_pager('trans_tbl', $sql, $cols);

	$table->width = "80%";
	display_db_pager($table);

br(1);
echo '<center>';
submit('Reconcile', _("Reconcile"), true, '', null);
submit('ReconcileAll', _("Reconcile All"), true, '');
echo '</center>';
end_form();

//------------------------------------------------------------------------------------------------

end_page();

