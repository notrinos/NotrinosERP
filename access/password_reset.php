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
	if (!isset($path_to_root) || isset($_GET['path_to_root']) || isset($_POST['path_to_root']))
		die(_("Restricted access"));
	include_once($path_to_root . "/includes/ui.inc");
	include_once($path_to_root . "/includes/page/header.inc");

	$js = "<script language='JavaScript' type='text/javascript'>
function defaultCompany()
{
	document.forms[0].company_login_name.options[".user_company()."].selected = true;
}
</script>";
	add_js_file('login.js');

	if (!isset($def_coy))
		$def_coy = 0;
	$def_theme = "default";

	$login_timeout = $_SESSION["wa_current_user"]->last_act;

	$title = $SysPrefs->app_title." ".$version." - "._("Password reset");
	$encoding = isset($_SESSION['language']->encoding) ? $_SESSION['language']->encoding : "iso-8859-1";
	$rtl = isset($_SESSION['language']->dir) ? $_SESSION['language']->dir : "ltr";
	$onload = !$login_timeout ? "onload='defaultCompany()'" : "";

	echo "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n";
	echo "<html dir='$rtl' >\n";
	echo "<head profile=\"http://www.w3.org/2005/10/profile\"><title>$title</title>\n";
   	echo "<meta http-equiv='Content-type' content='text/html; charset=$encoding' >\n";
	echo "<link href='$path_to_root/themes/$def_theme/default.css' rel='stylesheet' type='text/css'> \n";
 	echo "<link href='$path_to_root/themes/default/images/favicon.ico' rel='icon' type='image/x-icon'> \n";
	send_scripts();
	echo $js;
	echo "</head>\n";

	echo "<body id='loginscreen' $onload>\n";

	echo "<table class='titletext'><tr><td>$title</td></tr></table>\n";
	
	div_start('_page_body');
	br();br();
	start_form(false, false, @$_SESSION['timeout']['uri'], "resetform");
	start_table(false, "class='login'");
	start_row();
	echo "<td align='center' colspan=2>";
	echo "<a target='_blank' href='".$SysPrefs->power_url."'><img src='$path_to_root/themes/$def_theme/images/logo_frontaccounting.png' alt='FrontAccounting' height='50' onload='fixPNG(this)' border='0' ></a>";
	echo "</td>\n";
	end_row();

	echo "<input type='hidden' id=ui_mode name='ui_mode' value='".fallback_mode()."' >\n";
	table_section_title(_("Version")." $version   Build ".$SysPrefs->build_version." - "._("Password reset"));

	text_row(_("Email"), "email_entry_field", "", 20, 30);

    $coy =  user_company();
    if (!isset($coy))
        $coy = $def_coy;
    if (!@$SysPrefs->text_company_selection) {
        echo "<tr><td>"._("Company")."</td><td><select name='company_login_name'>\n";
        for ($i = 0; $i < count($db_connections); $i++)
            echo "<option value=$i ".($i==$coy ? 'selected':'') .">" . $db_connections[$i]["name"] . "</option>";
        echo "</select>\n";
        echo "</td></tr>";
    } else {
        text_row(_("Company"), "company_login_nickname", "", 20, 50);
    }
    start_row();
    label_cell("Please enter your e-mail", "colspan=2 align='center' id='log_msg'");
    end_row();
	end_table(1);
	echo "<center><input type='submit' value='&nbsp;&nbsp;"._("Send password -->")."&nbsp;&nbsp;' name='SubmitReset'
		 onclick='set_fullmode();'></center>\n";

	end_form(1);
	$Ajax->addScript(true, "document.forms[0].password.focus();");

    echo "<script language='JavaScript' type='text/javascript'>
    //<![CDATA[
            <!--
            document.forms[0].email_entry_field.select();
            document.forms[0].email_entry_field.focus();
            //-->
    //]]>
    </script>";
    div_end();
	echo "<table class='bottomBar'>\n";
	echo "<tr>";
	if (isset($_SESSION['wa_current_user'])) 
		$date = Today() . " | " . Now();
	else	
		$date = date("m/d/Y") . " | " . date("h.i am");
	echo "<td class='bottomBarCell'>$date</td>\n";
	echo "</tr></table>\n";
	echo "<table class='footer'>\n";
	echo "<tr>\n";
	echo "<td><a target='_blank' href='".$SysPrefs->power_url."' tabindex='-1'>".$SysPrefs->app_title." $version - " . _("Theme:") . " " . $def_theme . "</a></td>\n";
	echo "</tr>\n";
	echo "<tr>\n";
	echo "<td><a target='_blank' href='".$SysPrefs->power_url."' tabindex='-1'>".$SysPrefs->power_by."</a></td>\n";
	echo "</tr>\n";
	echo "</table><br><br>\n";
	echo "</body></html>\n";

