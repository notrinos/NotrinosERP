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
$page_security = 'SA_TRACKINGSETTINGS';
$path_to_root = '../..';
include_once($path_to_root.'/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Item Tracking Settings');

page($_SESSION['page_title']);

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/admin/db/company_db.inc');
include_once($path_to_root.'/inventory/includes/db/serial_batch_db.inc');

//-------------------------------------------------------------------------------------

if (isset($_POST['update']) && $_POST['update'] != '') {

	$input_error = 0;

	// Validate serial number format
	$serial_fmt = get_post('serial_number_format');
	if ($serial_fmt !== '' && strpos($serial_fmt, '{SEQ:') === false) {
		$input_error = 1;
		display_error(_('Serial number format must contain a {SEQ:n} token for sequential numbering.'));
		set_focus('serial_number_format');
	}

	// Validate batch number format
	$batch_fmt = get_post('batch_number_format');
	if ($batch_fmt !== '' && strpos($batch_fmt, '{SEQ:') === false) {
		$input_error = 1;
		display_error(_('Batch number format must contain a {SEQ:n} token for sequential numbering.'));
		set_focus('batch_number_format');
	}

	// Validate expiry warning days
	$expiry_warn = get_post('expiry_warning_days');
	if ($expiry_warn !== '' && (!is_numeric($expiry_warn) || (int)$expiry_warn < 0)) {
		$input_error = 1;
		display_error(_('Expiry warning days must be a positive number.'));
		set_focus('expiry_warning_days');
	}

	if ($input_error == 0) {
		set_tracking_setting('serial_number_format', get_post('serial_number_format'));
		set_tracking_setting('batch_number_format', get_post('batch_number_format'));
		set_tracking_setting('expiry_warning_days', get_post('expiry_warning_days'));
		set_tracking_setting('enforce_fefo', check_value('enforce_fefo') ? '1' : '0');
		set_tracking_setting('barcode_format', get_post('barcode_format'));
		set_tracking_setting('auto_generate_serial', check_value('auto_generate_serial') ? '1' : '0');
		set_tracking_setting('auto_generate_batch', check_value('auto_generate_batch') ? '1' : '0');

		display_notification(_('Tracking settings have been updated.'));
	}
}

//-------------------------------------------------------------------------------------

// Load current values
$serial_number_format = get_tracking_setting('serial_number_format', '{PREFIX}{YYYY}{MM}{SEQ:6}');
$batch_number_format = get_tracking_setting('batch_number_format', '{PREFIX}{YYYY}{MM}{SEQ:5}');
$expiry_warning_days = get_tracking_setting('expiry_warning_days', '30');
$enforce_fefo = get_tracking_setting('enforce_fefo', '0');
$barcode_format = get_tracking_setting('barcode_format', 'code128');
$auto_generate_serial = get_tracking_setting('auto_generate_serial', '1');
$auto_generate_batch = get_tracking_setting('auto_generate_batch', '1');

start_form();

start_outer_table(TABLESTYLE2);

table_section(1);

table_section_title(_('Serial Number Settings'));

text_row(_('Serial Number Format:'), 'serial_number_format', $serial_number_format, 40, 100);
label_row(_('Preview:'), '<em>' . htmlspecialchars(preview_number_format($serial_number_format, 'SN', 1)) . '</em>');
label_row(_('Format tokens:'), '<small>{PREFIX} {YYYY} {YY} {MM} {DD} {SEQ:n}</small>');

check_row(_('Auto-generate serial numbers on receive:'), 'auto_generate_serial', $auto_generate_serial);

table_section_title(_('Batch/Lot Number Settings'));

text_row(_('Batch Number Format:'), 'batch_number_format', $batch_number_format, 40, 100);
label_row(_('Preview:'), '<em>' . htmlspecialchars(preview_number_format($batch_number_format, 'LOT', 1)) . '</em>');

check_row(_('Auto-generate batch numbers on receive:'), 'auto_generate_batch', $auto_generate_batch);

table_section(2);

table_section_title(_('Expiry & Compliance'));

text_row(_('Expiry Warning Days:'), 'expiry_warning_days', $expiry_warning_days, 10, 10);
label_row('', '<small>' . _('Number of days before expiry to show warnings') . '</small>');

check_row(_('Enforce FEFO (First Expiry First Out):'), 'enforce_fefo', $enforce_fefo);
label_row('', '<small>' . _('When enabled, system will enforce dispatching items with earliest expiry first') . '</small>');

table_section_title(_('Barcode Settings'));

$barcode_formats = array(
	'code128' => _('Code 128'),
	'code39' => _('Code 39'),
	'ean13' => _('EAN-13'),
	'ean8' => _('EAN-8'),
	'qrcode' => _('QR Code'),
	'datamatrix' => _('Data Matrix (GS1)'),
);
array_selector_row(_('Default Barcode Format:'), 'barcode_format', $barcode_format, $barcode_formats);

end_outer_table(1);

submit_center('update', _('Update Settings'), true, '', 'default');

end_form();

end_page();
