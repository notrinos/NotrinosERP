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
$page_security = 'SA_CREATELANGUAGE';
$path_to_root = '..';
include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/packages.inc');
include_once($path_to_root.'/admin/db/maintenance_db.inc');
include_once($path_to_root.'/includes/ui.inc');

if ($SysPrefs->use_popup_windows)
	$js = get_js_open_window(900, 500);

page(_($help_context = 'Install/Update Languages'), false, false, '', $js);

simple_page_mode(true);

//---------------------------------------------------------------------------------------------
//
// Display all packages - both already installed and available from repository
//
function display_languages() {
	global $installed_languages, $dflt_lang, $GetText;
	
	$th = array(_('Language'), _('Name'), _('Encoding'), _('Right To Left'), _('Installed'), _('Available'), _('Default'), '', '');
	$currlang = $_SESSION['language']->code;

	div_start('lang_tbl');
	start_form();
	
	// select/display system locales support for sites using native gettext
	if (function_exists('gettext')) {
		if (check_value('DisplayAll'))
			 array_insert($th, 7, _('Supported'));
		start_table();
		check_row(_('Display also languages not supported by server locales'), 'DisplayAll', null, true);
		end_table();
	}

	start_table(TABLESTYLE);
	table_header($th);

	$k = 0;

	// get list of all (available and installed) langauges
	$langs = get_languages_list();
	foreach ($langs as $pkg_name => $lng) {
		if ($lng == 'C') // skip default locale (aka no translation)
			continue;

		$lang = $lng['code'];
		$lang_name = $lng['name'];
		$charset = $lng['encoding'];
		$rtl = @$lng['rtl'] == 'yes' || @$lng['rtl'] === true;
		$available = isset($lng['available']) ? $lng['available'] : '';
		$installed = isset($lng['version']) ? $lng['version'] : '';
		$id = @$lng['local_id'];
		
		if ($lang == $currlang)
			start_row("class='stockmankobg'");
		else
			alt_table_row_color($k);

		$support = $GetText->check_support($lang, $charset);

		if (function_exists('gettext') && !$support && !get_post('DisplayAll') && $lang != 'C') continue;

		label_cell($lang);
		label_cell($available ? get_package_view_str($lang, $lang_name) : $lang_name);
		label_cell($charset);
		label_cell($rtl ? _('Yes') : _('No'));
		label_cell($id === null ? _('None') : ($available && $installed ? $installed : _('Unknown')));
		label_cell($available ? $available : _('None'));
		label_cell($id === null ? '' : radio(null, 'CurDflt', $id, $dflt_lang == $lang, true), "align='center'");
		
		if (function_exists('gettext') && check_value('DisplayAll'))
			label_cell($support ? _('Yes') : _('No'));

		if (!$available && ($lang != 'C'))	// manually installed language
			button_cell('Edit'.$id, _('Edit'), _('Edit non standard language configuration'), ICON_EDIT);
		elseif (check_pkg_upgrade($installed, $available)) // outdated or not installed language in repo
			button_cell('Update'.$pkg_name, $installed ? _('Update') : _('Install'), _('Upload and install latest language package'), ICON_DOWN);
		else
			label_cell('');

		if (($id !== null) && ($lang != $currlang) && ($lang != 'C')) {
			delete_button_cell('Delete'.$id, _('Delete'));
			submit_js_confirm('Delete'.$id, 
				sprintf(_("You are about to remove language \'%s\'.\nDo you want to continue ?"), 
					$lang_name));
		}
		else
			label_cell('');
		end_row();
	}
	end_table();
	display_note(_('The marked language is the current language which cannot be deleted.'), 0, 0, "class='currentfg'");
	br();
	submit_center_first('Refresh', _('Update default'), '', null);

	submit_center_last('Add', _('Add new language manually'), '', false);

	end_form();
	div_end();
}

//---------------------------------------------------------------------------------------------

// Non standard (manually entered) languages support.
function check_data() {
	global $installed_languages;

	if (get_post('code') == '' || get_post('name') == '' || get_post('encoding') == '') {
		display_error(_('Language name, code nor encoding cannot be empty'));
		return false;
	}
	$id = array_search_value($_POST['code'], $installed_languages, 'code');
	if ($id !== null && $installed_languages[$id]['package'] != null) {
		display_error(_('Standard package for this language is already installed. If you want to install this language manually, uninstall standard language package first.'));
		return false;
	}
	return true;
}

function handle_submit($id){
	global $path_to_root, $installed_languages, $dflt_lang, $Mode;

	if ($_POST['dflt'])
		$dflt_lang = $_POST['code'];
	
	$installed_languages[$id]['code'] = clean_file_name($_POST['code']);
	$installed_languages[$id]['name'] = $_POST['name'];
	$installed_languages[$id]['path'] = 'lang/' . clean_file_name(get_post('code'));
	$installed_languages[$id]['encoding'] = $_POST['encoding'];
	$installed_languages[$id]['rtl'] = (bool)$_POST['rtl'];
	$installed_languages[$id]['package'] = '';
	$installed_languages[$id]['version'] = '';
	if (!write_lang())
		return false;
	$directory = $path_to_root . "/lang/" . clean_file_name(get_post('code'));
	if (!file_exists($directory)) {
		mkdir($directory);
		mkdir($directory.'/LC_MESSAGES');
	}
	if (is_uploaded_file($_FILES['uploadfile']['tmp_name'])) {
		$file1 = $_FILES['uploadfile']['tmp_name'];
		$code = preg_replace('/[^a-zA-Z_]/', '', $_POST['code']);
		$file2 = $directory.'/LC_MESSAGES/'.$code.'.po';
		if (file_exists($file2))
			unlink($file2);
		move_uploaded_file($file1, $file2);
	}
	if (is_uploaded_file($_FILES['uploadfile2']['tmp_name'])) {
		$file1 = $_FILES['uploadfile2']['tmp_name'];
		$code = preg_replace('/[^a-zA-Z_]/', '', $_POST['code']);
		$file2 = $directory.'/LC_MESSAGES/'.$code.'.mo';
		if (file_exists($file2))
			unlink($file2);
		move_uploaded_file($file1, $file2);
	}
	return true;
}

//---------------------------------------------------------------------------------------------

function display_language_edit($selected_id) {
	global $installed_languages, $dflt_lang;

	$n = $selected_id == -1 ? count($installed_languages) : $selected_id;
	
	start_form(true);

	start_table(TABLESTYLE2);

	if ($selected_id != -1) {
		$lang = $installed_languages[$n];
		$_POST['code'] = $lang['code'];
		$_POST['name']  = $lang['name'];
		$_POST['encoding']  = $lang['encoding'];
		$_POST['rtl'] = (isset($lang['rtl']) && $lang['rtl'] === true) ? $lang['rtl'] : false;
		$_POST['dflt'] = $dflt_lang == $lang['code'];
		hidden('selected_id', $selected_id);
	}
	text_row_ex(_('Language Code'), 'code', 20);
	text_row_ex(_('Language Name'), 'name', 20);
	text_row_ex(_('Encoding'), 'encoding', 20);

	yesno_list_row(_('Right To Left'), 'rtl', null, '', '', false);
	yesno_list_row(_('Default Language'), 'dflt', null, '', '', false);

	file_row(_('Language File').' (PO)', 'uploadfile');
	file_row(_('Language File').' (MO)', 'uploadfile2');

	end_table(0);
	display_note(_('Select your language files from your local harddisk.'), 0, 1);

	submit_add_or_update_center(false, '', 'both');

	end_form();
}

function handle_delete($id) {
	global  $path_to_root, $installed_languages, $dflt_lang;

	$lang = $installed_languages[$id]['code'];
	if ($installed_languages[$id]['package'])
		if (!uninstall_package($installed_languages[$id]['package']))
			return;
			
	if ($lang == $dflt_lang ) // on delete set default to current.
		$dflt_lang = $_SESSION['language']->code;
	
	unset($installed_languages[$id]);
	$installed_languages = array_values($installed_languages);

	if (!write_lang())
		return;

	$dirname = $path_to_root.'/lang/'.$lang;
	if ($lang && is_dir($dirname)) { // remove nonstadard language dir
		flush_dir($dirname, true);
		rmdir($dirname);
	}
}

//---------------------------------------------------------------------------------------------

if ($Mode == 'Delete')
	handle_delete($selected_id);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM')
	if (check_data() && handle_submit($selected_id))
		$Mode = 'RESET';

if ($id = find_submit('Update', false))
	install_language($id);

if (get_post('_CurDflt_update') || (get_post('Refresh') && get_post('CurDflt', -1) != -1)) {
	$new_lang = $installed_languages[get_post('CurDflt', 0)]['code'];
	if ($new_lang != $dflt_lang) {
		$dflt_lang = $new_lang;
		write_lang();
		$Ajax->activate('lang_tbl');
	}
}
if (get_post('_DisplayAll_update'))
	$Ajax->activate('lang_tbl');
	
//---------------------------------------------------------------------------------------------

if (isset($_GET['popup']) || get_post('Add') || $Mode == 'Edit' || $Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM')
	display_language_edit($selected_id);
else
	display_languages();

end_page();
