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
if (!isset($path_to_root) || isset($_GET['path_to_root']) || isset($_POST['path_to_root']))
	die(_('Restricted access'));
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/page/header.inc');

$js = "<script>
	function defaultCompany() {
		document.forms[0].company_login_name.options[".user_company()."].selected = true;
	}
</script>";

add_js_file('login.js');
// Display demo user name and password within login form if allow_demo_mode option is true
if ($SysPrefs->allow_demo_mode == true)
	$demo_text = _('Login as user: demouser and password: password');
else {
	$demo_text = _('Please login here');
	if (@$SysPrefs->allow_password_reset)
		$demo_text .= ' '._('or')." <a href='".$path_to_root."/index.php?reset=1'>"._('request new password')."</a>";
}

if (check_faillog()) {
	$blocked = true;

	$js .= "<script>setTimeout(function() {
		document.getElementsByName('SubmitUser')[0].disabled=0;
		document.getElementById('log_msg').innerHTML='$demo_text'}, 1000*".$SysPrefs->login_delay.");</script>";
	$demo_text = '<span class="redfg">'._('Too many failed login attempts.<br>Please wait a while or try later.').'</span>';
}
elseif ($_SESSION['wa_current_user']->login_attempt > 1)
	$demo_text = '<span class="redfg">'._('Invalid password or username. Please, try again.').'</span>';

flush_dir(user_js_cache());
if (!isset($def_coy))
	$def_coy = 0;

$def_theme = 'default';
$login_timeout = $_SESSION['wa_current_user']->last_act;
$title = $login_timeout ? _('Authorization timeout') : $SysPrefs->app_title.' '.$version.' - '._('Login');
$encoding = isset($_SESSION['language']->encoding) ? $_SESSION['language']->encoding : 'iso-8859-1';
$rtl = isset($_SESSION['language']->dir) ? $_SESSION['language']->dir : 'ltr';
$onload = !$login_timeout ? "onload='defaultCompany()'" : '';

echo "<!DOCTYPE html>\n";
echo "<html dir='".$rtl."' >\n";
echo "<head profile=\"http://www.w3.org/2005/10/profile\"><title>".$title."</title>\n";
echo "<meta charset='".$encoding."' >\n";
echo "<meta name='viewport' content='width=device-width,initial-scale=1'>";
echo "<link href='".$path_to_root.'/themes/'.$def_theme."/default.css' rel='stylesheet'> \n";
echo "<link href='".$path_to_root.'/themes/'.$def_theme."/local_style/access.css' rel='stylesheet'> \n";
echo "<link href='".$path_to_root."/libraries/fontawesome/css/all.min.css' rel='stylesheet'> \n";
echo "<link href='".$path_to_root."/themes/default/images/favicon.ico' rel='icon' type='image/x-icon'> \n";
send_scripts();

if (!$login_timeout)
	echo $js;

echo "</head>\n";
echo "<body id='loginscreen' ".$onload.">\n";
echo "<table class='titletext'><tr><td>".$title."</td></tr></table>\n";
	
div_start('_page_body');
br(2);
start_form(false, $_SESSION['timeout']['uri'], 'loginform');
start_table(false, "class='login'");
start_row();
echo "<td align='center' colspan=2>";
if (!$login_timeout) // logo
	echo "<a target='_blank' href='".$SysPrefs->power_url."'><img src='".$path_to_root."/themes/".$def_theme."/images/notrinos_erp.png' alt='NotrinosERP' height='50' onload='fixPNG(this)' border='0' ></a>";
else
	echo "<font size=5>"._('Authorization timeout')."</font>";
echo "</td>\n";
end_row();

if (!$login_timeout)
	table_section_title(_('Version').' '.$version.'   Build '.$SysPrefs->build_version.' - '._('Login'));

echo "<tr><td colspan='2'></td></tr>";

$value = $login_timeout ? $_SESSION['wa_current_user']->loginname : ($SysPrefs->allow_demo_mode ? 'demouser' : 'phuong');

echo "<tr><td class='login_input'><div class='input_container'><i class='fas fa-user' title='"._('User')."'></i>";
echo "<input required class='input' id='user' name='user_name_entry_field' type='text' placeholder='"._('User name:')."' value='$value'></div></td></tr>";

$password = $SysPrefs->allow_demo_mode ? 'password' : '12345';

echo "<tr><td class='login_input'><div class='input_container'><i class='fas fa-key' title='"._('Password')."'></i>";
echo "<input required class='input' id='pass' name='password' type='password' placeholder='"._('Password:')."' value='$password'></div></td></tr>";

if ($login_timeout)
	hidden('company_login_name', user_company());
else {
	$coy =  user_company();
	if (!isset($coy))
		$coy = $def_coy;
	echo "<tr><td class='login_input'><div class='input_container'><i class='fas fa-building' title='"._('Company')."'></i>";
	if (!@$SysPrefs->text_company_selection) {
		echo "<select name='company_login_name'>\n";
		for ($i = 0; $i < count($db_connections); $i++)
			echo "<option value=".$i.' '.($i==$coy ? 'selected' : '').'>'.$db_connections[$i]['name'].'</option>';
		echo "</select>\n";
	}
	else
		echo "<input required type='text' name='company_login_nickname' placeholder='"._('Company')."'>";
	echo '</div></td></tr>';
}; 
start_row();
label_cell($demo_text, "colspan=2 align='center' id='log_msg'");
end_row();

start_row();
echo "<input type='hidden' id=ui_mode name='ui_mode' value='".!fallback_mode()."' >\n";
echo "<td colspan='2'><center><input type='submit' value='&nbsp;&nbsp;"._('Login')."&nbsp;&nbsp;&#8250;' name='SubmitUser'"." onclick='".(in_ajax() ? 'retry();': 'set_fullmode();')."'".(isset($blocked) ? " disabled" : '')." ></center></td>\n";
end_row();

end_table(1);

foreach($_SESSION['timeout']['post'] as $p => $val) {
	// add all request variables to be resend together with login data
	if (!in_array($p, array('ui_mode', 'user_name_entry_field', 'password', 'SubmitUser', 'company_login_name')))
		if (!is_array($val))
			echo "<input type='hidden' name='".$p."' value='".$val."'>";
		else
			foreach($val as $i => $v)
				echo "<input type='hidden' name='{$p}[$i]' value='$v'>";
}
end_form(1);
$Ajax->addScript(true, "if (document.forms.length) document.forms[0].password.focus();");

echo "<script>
	//<![CDATA[
		<!--
		document.forms[0].user_name_entry_field.select();
		document.forms[0].user_name_entry_field.focus();
		//-->
	//]]>
</script>";
div_end();
echo "<table class='bottomBar'>\n";
echo '<tr>';
if (isset($_SESSION['wa_current_user'])) 
	$date = Today().' | '.Now();
else	
	$date = date('m/d/Y').' | '.date('h.i am');
echo "<td class='bottomBarCell'>".$date."</td>\n";
echo "</tr></table>\n";
echo "<table class='footer'>\n";
echo "<tr>\n";
echo "<td><a target='_blank' href='".$SysPrefs->power_url."' tabindex='-1'>".$SysPrefs->app_title.' '.$version.' - '._('Theme:').' '.$def_theme."</a></td>\n";
echo "</tr>\n";
echo "<tr>\n";
echo "<td><a target='_blank' href='".$SysPrefs->power_url."' tabindex='-1'>".$SysPrefs->power_by."</a></td>\n";
echo "</tr>\n";
echo "</table><br><br>\n";
echo "</body></html>\n";
