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
/*
	Retrieve value of POST variable(s).
	For $name passed as array $dflt is not used, 
	default values can be passed as values with non-numeric keys instead.
	If some field have user formatted numeric value, pass float default value to
	convert automatically to POSIX.
*/
function get_post($name, $dflt='')
{
	if (is_array($name)) {
		$ret = array();
		foreach($name as $key => $dflt)
			if (!is_numeric($key)) {
				$ret[$key] = is_numeric($dflt) ? input_num($key, $dflt) : get_post($key, $dflt);
			} else {
				$ret[$dflt] = get_post($dflt, null);
			}
		return $ret;
	} else
		return is_float($dflt) ? input_num($name, $dflt) :
				((!isset($_POST[$name]) /*|| $_POST[$name] === ''*/) ? $dflt : $_POST[$name]);
}
//---------------------------------------------------------------------------------
$form_nested = -1;

function start_form($multi=false, $dummy=false, $action="", $name="")
{
	// $dummy - leaved for compatibility with 2.0 API
	global $form_nested;

	if (++$form_nested) return;

	if ($name != "")
		$name = "name='$name'";
	if ($action == "")
		$action = $_SERVER['PHP_SELF'];

	if ($multi)
		echo "<form enctype='multipart/form-data' method='post' action='$action' $name>\n";
	else
		echo "<form method='post' action='$action' $name>\n";

}

/*
	Flush hidden fields buffer.
*/
function output_hidden()
{
	global $hidden_fields;

	if (is_array($hidden_fields))
		echo implode('', $hidden_fields);
	$hidden_fields = array();
}
//---------------------------------------------------------------------------------

function end_form($breaks=0)
{
	global $Ajax, $form_nested, $hidden_fields;

	if ($form_nested-- > 0) return;

	$_SESSION['csrf_token'] = random_id();
	if ($breaks)
		br($breaks);
	hidden('_focus');
	hidden('_modified', get_post('_modified', 0));
	hidden('_confirmed'); // helper for final form confirmation
	hidden('_token', $_SESSION['csrf_token']);

	output_hidden();
	echo "</form>\n";
	$Ajax->activate('_token');
	$Ajax->activate('_confirmed');
}

function check_csrf_token()
{
	if ($_SESSION['csrf_token'] != @$_POST['_token'])
	{
		display_error(_("Request from outside of this page is forbidden."));
		error_log(_("CSRF attack detected from: ").@$_SERVER['HTTP_HOST'].' ('.@$_SERVER['HTTP_REFERER'].')');
		return false;
	}
	return true;
}

function start_table($class=false, $extra="", $padding='2', $spacing='0')
{
	echo "<center><table";
	if ($class == TABLESTYLE_NOBORDER)
		echo " class='tablestyle_noborder'";
	elseif ($class == TABLESTYLE2)
		echo " class='tablestyle2'";
	elseif ($class == TABLESTYLE)
		echo " class='tablestyle'";
	if ($extra != "")
		echo " $extra";
	echo " cellpadding='$padding' cellspacing='$spacing'>\n";
}

function end_table($breaks=0)
{
	echo "</table></center>\n";
	output_hidden();
	if ($breaks)
		br($breaks);
}

function start_outer_table($class=false, $extra="", $padding='2', $spacing='0', $br=false)
{
	if ($br)
		br();
	start_table($class, $extra, $padding, $spacing);
	echo "<tr valign=top><td>\n"; // outer table
}

function table_section($number=1, $width=false)
{
	if ($number > 1)
	{
		echo "</table>\n";
		output_hidden();
		$width = ($width ? "width='$width'" : "");
		echo "</td><td style='border-left:1px solid #cccccc;' $width>\n"; // outer table
	}
	echo "<table class='tablestyle_inner'>\n";
}	

function end_outer_table($breaks=0, $close_table=true)
{
	if ($close_table)
	{
		echo "</table>\n";
		output_hidden();
	}
	echo "</td></tr>\n";
	end_table($breaks);
}
//
//  outer table spacer
//
function vertical_space($params='')
{
	echo "</td></tr><tr><td valign=center $params>";
}

function meta_forward($forward_to, $params="", $timeout=0, $return=false)
{
    global $Ajax;
	echo "<meta http-equiv='Refresh' content='".$timeout."; url=$forward_to?$params'>\n";
	echo "<center><br>" . _("You should automatically be forwarded.");
	echo " " . _("If this does not happen") . " " . "<a href='$forward_to?$params'>" . _("click here") . "</a> " . _("to continue") . ".<br><br></center>\n";
	if ($params !='') $params = '?'.$params;
	$Ajax->redirect($forward_to.$params);
	if (!$return) exit;
}

//-----------------------------------------------------------------------------------
// Find and replace hotkey marker.
// if $clean == true marker is removed and clean label is returned 
// (for use in wiki help system), otherwise result is array of label 
// with underlined hotkey letter and access property string.
//
function access_string($label, $clean=false)
{
	$access = '';
	$slices = array();

	if (preg_match('/(.*)&([a-zA-Z0-9])(.*)/', $label, $slices))	
	{
		$label = $clean ? $slices[1].$slices[2].$slices[3] :
			$slices[1].'<u>'.$slices[2].'</u>'.$slices[3];
		$access = " accesskey='".strtoupper($slices[2])."'";
	}
	
	$label = str_replace( '&&', '&', $label);

	return $clean ? $label : array($label, $access);
}

function hyperlink_back($center=true, $no_menu=true, $type_no=0, $trans_no=0, $final=false)
{
	global $path_to_root;

	if ($center)
		echo "<center>";
	$id = 0;	
	if ($no_menu && $trans_no != 0)
	{
		include_once($path_to_root."/admin/db/attachments_db.inc");
		$id = has_attachment($type_no, $trans_no);
		$attach = get_attachment_string($type_no, $trans_no);
    	echo $attach;
	}
	$width = ($id != 0 ? "30%" : "20%");	
	start_table(false, "width='$width'");
	start_row();
	if ($no_menu)
	{
		echo "<td align=center><a href='javascript:window.print();'>"._("Print")."</a></td>\n";
	}	
	echo "<td align=center><a href='javascript:goBack(".($final ? '-2' : '').");'>".($no_menu ? _("Close") : _("Back"))."</a></td>\n";
	end_row();
	end_table();
	if ($center)
		echo "</center>";
	echo "<br>";
}

function hyperlink_no_params($target, $label, $center=true)
{
	$id = default_focus();
	$pars = access_string($label);
	if ($target == '')
		$target = $_SERVER['PHP_SELF'];
	if ($center)
		echo "<br><center>";
	echo "<a href='$target' id='$id' $pars[1]>$pars[0]</a>\n";
	if ($center)
		echo "</center>";
}

function hyperlink_no_params_td($target, $label)
{
	echo "<td>";
	hyperlink_no_params($target, $label);
	echo "</td>\n";
}

function viewer_link($label, $url='', $class='', $id='',  $icon=null)
{
	global $path_to_root;
	
	if ($class != '')
		$class = " class='$class'";

	if ($id != '')
		$class = " id='$id'";

	if ($url != "")
	{
		$pars = access_string($label);
		if (user_graphic_links() && $icon)
			$pars[0] = set_icon($icon, $pars[0]);
		$preview_str = "<a target='_blank' $class $id href='$path_to_root/$url' onclick=\"javascript:openWindow(this.href,this.target); return false;\"$pars[1]>$pars[0]</a>";
	}
	else
		$preview_str = $label;
 return $preview_str;
}

function menu_link($url, $label, $id=null)
{
	global $path_to_root;

	$id = default_focus($id);
	$pars = access_string($label);

	if ($url[0] != '/')
		$url = '/'.$url;
	$url = $path_to_root.$url;

	return "<a href='$url' class='menu_option' id='$id' $pars[1]>$pars[0]</a>";
}

function submenu_option($title, $url, $id=null)
{
	display_note( menu_link($url, $title, $id), 0, 1);
}

function submenu_view($title, $type, $number, $id=null)
{
	display_note(get_trans_view_str($type, $number, $title, false, 'viewlink', $id), 0, 1);
}

function submenu_print($title, $type, $number, $id=null, $email=0, $extra=0)
{
	display_note(print_document_link($number, $title, true, $type, false, 'printlink', $id, $email, $extra), 0, 1);
}
//-----------------------------------------------------------------------------------

function hyperlink_params($target, $label, $params, $center=true)
{
	$id = default_focus();
	
	$pars = access_string($label);
	if ($target == '')
		$target = $_SERVER['PHP_SELF'];
	if ($center)
		echo "<br><center>";
	echo "<a id='$id' href='$target?$params'$pars[1]>$pars[0]</a>\n";
	if ($center)
		echo "</center>";
}

function hyperlink_params_td($target, $label, $params)
{
	echo "<td>";
	hyperlink_params($target, $label, $params, false);
	echo "</td>\n";
}

//-----------------------------------------------------------------------------------

function hyperlink_params_separate($target, $label, $params, $center=false)
{
	$id = default_focus();

	$pars = access_string($label);
	if ($center)
		echo "<br><center>";
	echo "<a target='_blank' id='$id' href='$target?$params' $pars[1]>$pars[0]</a>\n";
	if ($center)
		echo "</center>";
}

function hyperlink_params_separate_td($target, $label, $params)
{
	echo "<td>";
	hyperlink_params_separate($target, $label, $params);
	echo "</td>\n";
}

//--------------------------------------------------------------------------------------------------

function alt_table_row_color(&$k, $extra_class=null)
{
	$classes = $extra_class ? array($extra_class) : array();
	if ($k == 1)
	{
		array_push($classes, 'oddrow');
		$k = 0;
	}
	else
	{
		array_push($classes, 'evenrow');
		$k++;
	}
	echo "<tr class='".implode(' ', $classes)."'>\n";
}

function table_section_title($msg, $colspan=2)
{
	echo "<tr><td colspan=$colspan class='tableheader'>$msg</td></tr>\n";
}

function table_header($labels, $params='')
{
	start_row();
	foreach ($labels as $label)
		labelheader_cell($label, $params);
	end_row();
}
//-----------------------------------------------------------------------------------

function start_row($param="")
{
	if ($param != "")
		echo "<tr $param>\n";
	else
		echo "<tr>\n";
}

function end_row()
{
	echo "</tr>\n";
}

function br($num=1)
{
	for ($i = 0; $i < $num; $i++)
		echo "<br>";
}

$ajax_divs = array();

function div_start($id='', $trigger=null, $non_ajax=false)
{
    global $ajax_divs;

	if ($non_ajax) { // div for non-ajax elements
   		array_push($ajax_divs, array($id, null));
   		echo "<div style='display:none' class='js_only' ".($id !='' ? "id='$id'" : '').">";
	} else { // ajax ready div
   		array_push($ajax_divs, array($id, $trigger===null ? $id : $trigger));
   		echo "<div ". ($id !='' ? "id='$id'" : '').">";
   		ob_start();
	}
}

function div_end()
{
    global $ajax_divs, $Ajax;

	output_hidden();
    if (count($ajax_divs))
    {
		$div = array_pop($ajax_divs);
		if ($div[1] !== null)
			$Ajax->addUpdate($div[1], $div[0], ob_get_flush());
    }
	echo "</div>";
}

//-----------------------------------------------------------------------------
//	Tabbed area:
// 	$name - prefix for widget internal elements:
//		Nth tab submit name:  {$name}_N
//		div id:	_{$name}_div
//		sel (hidden) name: _{$name}_sel
// $tabs - array of tabs; string: tab title or array(tab_title, enabled_status)

function tabbed_content_start($name, $tabs, $dft='') {
    global $Ajax;

    $selname = '_'.$name.'_sel';
	$div = '_'.$name.'_div';

	$sel = find_submit($name.'_', false);
	if($sel==null)
		$sel = get_post($selname, (string)($dft==='' ? key($tabs) : $dft));

	if ($sel!==@$_POST[$selname])
		$Ajax->activate($name);

	$_POST[$selname] = $sel;

	div_start($name);
	$str = "<ul class='ajaxtabs' rel='$div'>\n";
	foreach($tabs as $tab_no => $tab) {
		
		$acc = access_string(is_array($tab) ? $tab[0] : $tab);
		$disabled = (is_array($tab) && !$tab[1])  ? 'disabled ' : '';
		$str .= ( "<li>"
			."<button type='submit' name='{$name}_".$tab_no
			."' class='".((string)$tab_no===$sel ? 'current':'ajaxbutton')."' $acc[1] $disabled>"
			."<span>$acc[0]</span>"
			."</button>\n"
			."</li>\n" );
	}

	$str .= "</ul>\n";
	$str .= "<div class='spaceBox'></div>\n";
	$str .= "<input type='hidden' name='$selname' value='$sel'>\n";
	$str .= "<div class='contentBox' id='$div'>\n";
	echo $str;
}

function tabbed_content_end() {
	output_hidden();
	echo "</div>"; // content box (don't change to div_end() unless div_start() is used above)
	div_end(); // tabs widget
}

function tab_changed($name)
{
	$to = find_submit("{$name}_", false);
	if (!$to) return null;

	return array('from' => $from = get_post("_{$name}_sel"),
		'to' => $to);
}
/*
	Check whether tab has been just switched on
*/
function tab_opened($name, $tab)
{
	return (get_post('_'.$name.'_sel') != $tab) && (find_submit($name.'_', false) == $tab);
}
/*
	Check whether tab has been just switched off
*/
function tab_closed($name, $tab)
{
	return (get_post('_'.$name.'_sel') == $tab) && (find_submit($name.'_', false) != $tab);
}
/*
	Check whether tab is visible on current page
*/
function tab_visible($name, $tab)
{
	$new = find_submit($name.'_', false);
	return (get_post('_'.$name.'_sel') == $tab && !$new) || $new==$tab;
}

/* Table editor interfaces. Key is editor type
	0 => url of editor page
	1 => hotkey code
	2 => context help
*/
$popup_editors = array(
	'customer' => array('/sales/manage/customers.php?debtor_no=', 
		113,	_("Customers"), 900, 550),
	'branch' => array('/sales/manage/customer_branches.php?SelectedBranch=', 
		114, _("Branches"), 900, 650),
	'supplier' => array('/purchasing/manage/suppliers.php?supplier_id=', 
		113, _("Suppliers"), 1150, 550),
	'item' => array('/inventory/manage/items.php?stock_id=', 
		115, _("Items"), 1000, 500),
	'fa_item' => array('/inventory/manage/items.php?FixedAsset=1&stock_id=', 
		115, _("Items"), 1000, 500)
);
/*
	Bind editors for various selectors.
	$type - type of editor
	$input - name of related input field
	$caller - optional function key code (available values F1-F12: 112-123,
		true: default)
*/
function set_editor($type, $input, $caller=true)
{
	global $path_to_root, $Editors, $popup_editors, $Pagehelp;

	$key = $caller===true ? $popup_editors[$type][1] : $caller;

	$Editors[$key] = array( $path_to_root . $popup_editors[$type][0], $input, 
		$popup_editors[$type][3], $popup_editors[$type][4]);
	
	$help = 'F' . ($key - 111) . ' - ';
	$help .= $popup_editors[$type][2];
	$Pagehelp[] = $help;
}
//------------------------------------------------------------------------------
// Procedures below are now obsolete. Preserved for eventual future use.

/*
	External page call with saving current context.
	$call - url of external page
	$ctx - optional. name of SESSION context object or array of names of POST 
		variables saved on call
*/
function context_call($call, $ctx='')
{
	if (is_array($ctx)) 
	{
		foreach($ctx as $postname)
		{
		 	$context[$postname] = get_post($postname);
		}
	} else 
		$context = isset($_SESSION[$ctx]) ? $_SESSION[$ctx] : null;

	array_unshift($_SESSION['Context'], array('name' => $ctx, 
		'ctx' => $context,
		'caller' => $_SERVER['PHP_SELF'],
		'ret' => array()));
	meta_forward($call);
}
/*
	Restores context after external page call and
	returns array of data passed by external page.
*/
function context_restore()
{
	if ( count($_SESSION['Context'])) {
		if ($_SERVER['PHP_SELF'] == $_SESSION['Context'][0]['caller']) {
			$ctx = array_shift($_SESSION['Context']);
			if ($ctx) {
				if (is_array($ctx['ctx'])) {
					foreach($ctx['ctx'] as $name => $val) 
					{
						$_POST[$name] = $val;
					}
				} else
					if ($ctx['name']!='')
						$_SESSION[$ctx['name']] = $ctx['ctx'];
				return $ctx['ret'];
			}
		}
	}
	return false;
}

/*
	Return to caller page if the page was called from external context.
*/
function context_return($ret)
{
	if ( count($_SESSION['Context'])) {
		$ctx = &$_SESSION['Context'][0];
		$ctx['ret'] = $ret;
		meta_forward( $ctx['caller'] );
	}
}
/*
	Clearing context stack after page cancel.
*/
function context_reset()
{
	$_SESSION['Context'] = array();
}
/*
	Context stack initialization
*/
if (!isset($_SESSION['Context'])) {
		context_reset();
}
/*
	Redirector for selector F4 calls.
	$sel_editors is array of selname=>editor_page
*/
function editor_redirect($sel_editors, $save_fun='') {
	foreach ($sel_editors as $selname=>$editor)
		if (isset($_POST['_'.$selname.'_editor'])) {
			if (function_exists($save_fun))
				$save_fun();
			unset($_POST['_'.$selname.'_editor']);
			context_call($editor, array_keys($_POST));
		}
}
/*
	Return procedure for selector F4 calls
*/
function editor_return($vars, $restore_fun='') {
	if (function_exists($restore_fun))
		$restore_fun();

	if ($ret = context_restore()) {
		foreach ($vars as $postname=>$retname)
			if (isset($ret[$retname])) {
				$_POST[$postname] = $ret[$retname];
				set_focus($postname);
			}
	}
}

function confirm_dialog($submit, $msg) {
	if (find_post($submit)) {
		display_warning($msg);
		br();
		submit_center_first('DialogConfirm', _("Proceed"), '', true);
		submit_center_last('DialogCancel', _("Cancel"), '', 'cancel');
		return 0;
	} else
		return get_post('DialogConfirm', 0);
}
/*
	Confirm dialog to be used optionally in final form checking routine.
	Displays warning conditionally unless it was displayed
*/
function display_confirmation($msg)
{
	global $Ajax;

	if (!get_post('_confirmed'))
	{
		$_POST['_confirmed'] = 1;
		display_warning($msg);
		return false;
	} else
		return true;
}
/*
	Block menu/shortcut links during transaction procesing.
*/
function page_processing($msg = false)
{
	global $Ajax;

	if ($msg === true)
		$msg = _("Entered data has not been saved yet.\nDo you want to abandon changes?");

	$js = "_validate._processing=" . (
		$msg ? '\''.strtr($msg, array("\n"=>'\\n')) . '\';' : 'null;');
	if (in_ajax()) {
		$Ajax->addScript(true, $js);
	} else
		add_js_source($js);
}

function page_modified($status = true)
{
	global $Ajax;

	$js = "_validate._modified=" . ($status ? 1:0).';';
	if (in_ajax()) {
		$Ajax->addScript(true, $js);
	} else
		add_js_source($js);
}
