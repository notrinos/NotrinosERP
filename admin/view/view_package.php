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
$page_security = 'SA_OPEN';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/packages.inc');

page(_($help_context = 'Package Details'), true);

include_once($path_to_root . '/includes/ui.inc');

if (!isset($_GET['id']))  {
	/*Script was not passed the correct parameters */
	display_note(_('The script must be called with a valid package id to review the info for.'));
	end_page();
}

$field_labels = array(
	'Package' => _('Package id'),
	'Version' => _('Available version'),
	'Type' => _('Package type'),
	'Name' => _('Package content'),
	'Description' => _('Description'),
	'Content' => _('Content information'),
	'Price' => _('Price'),
	'Author' => _('Author'),
	'Homepage' => _('Home page'),
	'Maintenance' => _('Package maintainer'),
	'InstallPath' => _('Installation path'),
	'Depends' => _('Minimal software versions'),
	'RTLDir' => _('Right to left'),
	'Encoding' => _('Charset encoding')
);
$field_order = array('Package', 'Version', 'Type', 'Name', 'Description', 'Content', 'Price', 'Author', 'Homepage', 'Maintenance', 'InstallPath', 'Depends', 'RTLDir', 'Encoding');

$pkg = get_package_info($_GET['id']);
if (!$pkg) {
	display_note(_('Package details are not available for the selected package.'));
	end_page();
}

$pkg['Price'] = get_package_price_label($pkg, '');
$package_images = get_package_image_urls($pkg);

display_heading(sprintf(_("Content information for package '%s'"), $_GET['id']));
br();
start_table(TABLESTYLE2, "width='80%'");
$th = array(_('Property'), _('Value'));
table_header($th);

foreach ($field_order as $field) {
	if (!isset($field_labels[$field]))
		continue;
	$value = isset($pkg[$field]) ? $pkg[$field] : '';
	if ($field == 'Homepage' && $value == '')
		continue;
	if (package_meta_text($value) == '')
		continue;
	start_row();
	label_cells($field_labels[$field], nl2br(html_specials_encode(is_array($value) ? implode("\n", $value) : $value)),
		 "class='tableheader2'");
	end_row();
}
end_table(1);

if (count($package_images)) {
	display_heading2(_('Package pictures'));
	echo "<div style='display:flex;flex-wrap:wrap;gap:16px;margin-top:12px;'>";
	foreach ($package_images as $image_url)
		echo "<div style='border:1px solid #d7d7d7;padding:6px;background:#fff;'><a href='".html_specials_encode($image_url)."' target='_blank' rel='noopener noreferrer' title='"._('Open full size picture')."'><img src='".html_specials_encode($image_url)."' alt='"._('Package picture')."' style='display:block;max-width:260px;max-height:180px;cursor:zoom-in;' /></a><div style='margin-top:6px;text-align:center;'><a href='".html_specials_encode($image_url)."' target='_blank' rel='noopener noreferrer'>"._('Open full size')."</a></div></div>";
	echo "</div>";
}

end_page(true);
