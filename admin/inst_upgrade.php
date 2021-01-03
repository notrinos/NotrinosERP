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
$page_security = 'SA_SOFTWAREUPGRADE';
$path_to_root="..";
include_once($path_to_root . "/includes/session.inc");

if ($SysPrefs->use_popup_windows) {
	$js = get_js_open_window(900, 500);
}
page(_($help_context = "Software Upgrade"), false, false, "", $js);

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/admin/db/company_db.inc");
include_once($path_to_root . "/admin/db/maintenance_db.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/admin/includes/fa_patch.class.inc");

$site_status = get_site_status($db_connections);
$installers = get_installers();

if (get_post('Upgrade')) 
{
	$comp = get_post('select_comp');

    if ($comp === '')
		display_error(_('Select company to be upgraded.'));
	else {
		$patch = @$installers[$site_status[$comp]['version']];
		if ($patch)
		{
			if (!$patch->upgrade_company($comp, check_value('force')))
				display_error(implode('<hr>', $patch->errors));
			else
				display_notification(_("Company upgraded successfully."));

			$site_status = get_site_status($db_connections); // update info
		    $Ajax->activate('_page_body');
		}
	}
}
$i = find_submit('Clear');
if ($i != -1)
{
  unlink(VARLOG_PATH.'/upgrade.'.$i.'.log');
  $Ajax->activate('_page_body');
}
if (get_post('_select_comp_update'))
{
  $Ajax->activate('_page_body');
}

start_form();

$th = array(_("Company"), _("Table set"), _("Current version"), _("Last log"), _('Upgrade'));
start_table(TABLESTYLE);
table_header($th);
$k = 0; //row colour counter

$uptodate = true;
foreach($site_status as $i => $comp)
{
	$status = $comp['version']==$db_version;

	alt_table_row_color($k);

	label_cell($comp['name']);
	label_cell($comp['table_set']);

	label_cell($comp['version'], 'align=center' .($status ? '':' class=redfg')/*, 'class='.( $status ? 'ok' : 'error')*/);

	$log = VARLOG_PATH.'/upgrade.'.$i.'.log';
	if (file_exists($log))
	{
		label_cell(viewer_link(_('View log'), "admin/view/view_upgrade_log.php?id=$i", null, $i, 'log.png')
		  .button('Clear'.$i, _('Clear'), _('Clear log'), ICON_DELETE), 'align=center');
		submit_js_confirm('Clear'.$i, _("Do you really want to clear this upgrade log?"));
	} else
		label_cell('-', 'align=center');


	if (!$status)
	{
		label_cell(radio(null, 'select_comp', $i, null, true), 'align=center');
		$uptodate = false;
	} else
		label_cell(_('Up to date'));
	end_row();
}

end_table();
br();

div_start('upgrade_args');
if (get_post('select_comp') !== '')
{
	$patch = @$installers[$site_status[get_post('select_comp')]['version']];
	if ($patch)
		$patch->show_params(get_post('select_comp'));
}
div_end();

if ($uptodate)
	display_note(_('All company database schemes are up to date.'));
else {
	if (get_post('select_comp') === '')
		display_note(_("Select company for incremental upgrade."), 0, 1, "class='stockmankofg'");
	submit_center('Upgrade', _('Upgrade'), true, _('Save database and perform upgrade'), 'nonajax');
}
end_form();

end_page();

