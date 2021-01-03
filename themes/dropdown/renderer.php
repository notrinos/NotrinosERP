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
// Author: Joe Hunt, 17/11/2015. Upgraded to release 2.4. 10/11/2015.

	class renderer
	{
		function wa_get_apps($title, $applications, $sel_app)
		{
			foreach($applications as $app)
			{
				foreach ($app->modules as $module)
				{
					$apps = array();
					foreach ($module->lappfunctions as $appfunction)
						$apps[] = $appfunction;
					foreach ($module->rappfunctions as $appfunction)
						$apps[] = $appfunction;
					$application = array();	
					foreach ($apps as $application)	
					{
						$url = explode('?', $application->link);
						$app_lnk = $url[0];					
						$pos = strrpos($app_lnk, "/");
						if ($pos > 0)
						{
							$app_lnk = substr($app_lnk, $pos + 1);
							$lnk = $_SERVER['REQUEST_URI'];
							$url = explode('?', $lnk);
							$asset = false;
							if (isset($url[1]))
								$asset = strstr($url[1], "FixedAsset");
							$lnk = $url[0];					
							$pos = strrpos($lnk, "/");
							$lnk = substr($lnk, $pos + 1);
							if ($app_lnk == $lnk)  
							{
								$acc = access_string($app->name);
								$app_id = ($asset != false ? "assets" : $app->id);
								return array($acc[0], $module->name, $application->label, $app_id);
							}	
						}	
					}
				}
			}
			return array("", "", "", $sel_app);
		}
		
		function wa_header()
		{
			page(_($help_context = "Main Menu"), false, true);
		}

		function wa_footer()
		{
			end_page(false, true);
		}
		function shortcut($url, $label) 
		{
			echo "<li>";
			echo menu_link($url, $label);
			echo "</li>";
		}
		function menu_header($title, $no_menu, $is_index)
		{
			global $path_to_root, $SysPrefs, $version;

			$sel_app = $_SESSION['sel_app'];
			echo "<div class='fa-main'>\n";
			if (!$no_menu)
			{
				$applications = $_SESSION['App']->applications;
				$local_path_to_root = $path_to_root;
				$pimg = "<img src='$local_path_to_root/themes/".user_theme()."/images/preferences.gif' style='width:14px;height:14px;border:0;vertical-align:middle;padding-bottom:3px;' alt='"._('Preferences')."'>&nbsp;&nbsp;";
				$limg = "<img src='$local_path_to_root/themes/".user_theme()."/images/lock.gif' style='width:14px;height:14px;border:0;vertical-align:middle;padding-bottom:3px;' alt='"._('Change Password')."'>&nbsp;&nbsp;";
				$img = "<img src='$local_path_to_root/themes/".user_theme()."/images/on_off.png' style='width:14px;height:14px;border:0;vertical-align:middle;padding-bottom:3px;' alt='"._('Logout')."'>&nbsp;&nbsp;";
				$himg = "<img src='$local_path_to_root/themes/".user_theme()."/images/help.gif' style='width:14px;height:14px;border:0;vertical-align:middle;padding-bottom:3px;' alt='"._('Help')."'>&nbsp;&nbsp;";
				echo "<div id='header'>\n";
				echo "<ul>\n";
				echo "  <li><a href='$local_path_to_root/admin/display_prefs.php?'>$pimg" . _("Preferences") . "</a></li>\n";
				echo "  <li><a href='$local_path_to_root/admin/change_current_user_password.php?selected_id=" . $_SESSION["wa_current_user"]->username . "'>$limg" . _("Change password") . "</a></li>\n";
				if ($SysPrefs->help_base_url != null)
					echo "  <li><a target = '_blank' onclick=" .'"'."javascript:openWindow(this.href,this.target); return false;".'" '. "href='". 
						help_url()."'>$himg" . _("Help") . "</a></li>";
				echo "  <li><a href='$path_to_root/access/logout.php?'>$img" . _("Logout") . "</a></li>";
				echo "</ul>\n";
				$indicator = "$path_to_root/themes/".user_theme(). "/images/ajax-loader.gif";
				echo "<h1>$SysPrefs->power_by $version<span style='padding-left:300px;'><img id='ajaxmark' src='$indicator' align='center' style='visibility:hidden;'></span></h1>\n";
				echo "</div>\n"; // header
								
				echo "<div id='cssmenu'>\n";
				echo "<ul>\n";
				$i = 0;
				$account = $this->wa_get_apps($title, $applications, $sel_app);
				foreach($applications as $app)
				{
                    if ($_SESSION["wa_current_user"]->check_application_access($app))
                    {
						$acc = access_string($app->name);
						$class = ($account[3] == $app->id ? "active" : "");
						$n = count($app->modules);
						if ($n)
							$class .= " has-sub";
						$dashboard = "";	
					    $u_agent = $_SERVER['HTTP_USER_AGENT']; 
    					if (preg_match('/android/i', $u_agent) && preg_match('/mobile/i', $u_agent)) {
    						$link = "#'";
							$dashboard = "$local_path_to_root/index.php?application=$app->id";
						}
    					else
    						$link = "$local_path_to_root/index.php?application=$app->id '$acc[1]";
						echo "  <li class ='$class'><a href='$link><span>" . $acc[0] . "</span></a>\n";
						if (!$n)
						{
							echo "  </li>\n";
							continue;
						}	
						echo "    <ul>\n";
   						if ($dashboard !="")
							echo "      <li><a href='$dashboard'><span><font color='red'>"._("Dashboard")."</font></span></a></li>\n";
						foreach ($app->modules as $module)
						{
	    					if (!$_SESSION["wa_current_user"]->check_module_access($module))
        						continue;
 							echo "      <li class='has-sub'><a href='#'><span>$module->name</span></a>\n"; 
 							$apps2 = array();
 							foreach ($module->lappfunctions as $appfunction)
								$apps2[] = $appfunction;
							foreach ($module->rappfunctions as $appfunction)
								$apps2[] = $appfunction;
							$application = array();	
       						$n = count($apps2);
       						$class = "";
       						if ($i > 5)
       							$class = "class='align_right'";
							if ($n)
								echo "        <ul $class>\n";
							else
							{
								echo "      </li>\n";
								continue;
							}	
							foreach ($apps2 as $application)	
							{
								$lnk = access_string($application->label);
								if ($_SESSION["wa_current_user"]->can_access_page($application->access))
								{
									if ($application->label != "")
									{
										echo "          <li><a href='$path_to_root/$application->link'><span>$lnk[0]</span></a></li>\n";
									}
								}
								elseif (!$_SESSION["wa_current_user"]->hide_inaccessible_menu_items())	
									echo "          <li><a href='#'><span><font color='gray'>$lnk[0]</font></span></a></li>\n";
							}
							if ($n)
								echo "        </ul>\n";	
							echo "      </li>\n";
						}
						echo "    </ul>\n"; // menu
					}
					echo"  </li>\n";
					$i++;
				}	
				echo "</ul>\n"; 
				echo "</div>\n"; // menu
			}
			echo "<div class='fa-body'>\n";
			if ($no_menu)
				echo "<br>";
			elseif ($title && !$no_menu && !$is_index)
			{
				echo "<div class='fa-content'>\n";
				echo "<center><table id='title'><tr><td width='100%' class='titletext'>$title</td>"
				."<td align=right>"
				.(user_hints() ? "<span id='hints'></span>" : '')
				."</td>"
				."</tr></table></center>";
			}
		}

		function menu_footer($no_menu, $is_index)
		{
			global $path_to_root, $SysPrefs, $version, $db_connections;
			include_once($path_to_root . "/includes/date_functions.inc");

			if (!$no_menu && !$is_index)
				echo "</div>\n"; // fa-content
			echo "</div>\n"; // fa-body
			if (!$no_menu)
			{
   				echo "<script type='text/javascript'>if( /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent))
   				       {document.getElementById('cssmenu').style.position = 'fixed';}</script>\n";
				echo "<div class='fa-footer'>\n";
				if (isset($_SESSION['wa_current_user']))
				{
					echo "<span class='power'><a target='_blank' href='$SysPrefs->power_url'>$SysPrefs->power_by $version</a></span>\n";
					echo "<span class='date'>".Today() . "&nbsp;" . Now()."</span>\n";
					echo "<span class='date'>" . $db_connections[$_SESSION["wa_current_user"]->company]["name"] . "</span>\n";
					echo "<span class='date'>" . $_SERVER['SERVER_NAME'] . "</span>\n";
					echo "<span class='date'>" . $_SESSION["wa_current_user"]->name . "</span>\n";
					echo "<span class='date'>" . _("Theme:") . " " . user_theme() . "</span>\n";
					echo "<span class='date'>".show_users_online()."</span>\n";
				}
				echo "</div>\n"; // footer
			}
			echo "</div>\n"; // fa-main
		}

		function display_applications(&$waapp)
		{
			global $path_to_root;

			$sel = $waapp->get_selected_application();
			meta_forward("$path_to_root/admin/dashboard.php", "sel_app=$sel->id");	
        	end_page();
        	exit;
		}	
	}
	
