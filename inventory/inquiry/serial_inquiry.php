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

/**
 * Serial Number Inquiry — lookup a serial number and display its complete
 * movement timeline, current status, and related documents.
 *
 * Access via:
 *   - Direct URL: serial_inquiry.php?serial_id=123
 *   - Search: type serial number in search field
 */
$page_security = 'SA_SERIALINQUIRY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Serial Number Inquiry');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);

page($_SESSION['page_title'], isset($_GET['serial_id']), false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');

//----------------------------------------------------------------------
// Handle GET parameter
//----------------------------------------------------------------------
if (isset($_GET['serial_id']))
	$_POST['serial_id'] = (int)$_GET['serial_id'];

if (isset($_GET['serial_no']) && isset($_GET['stock_id'])) {
	$found = get_serial_number_by_code($_GET['serial_no'], $_GET['stock_id']);
	if ($found)
		$_POST['serial_id'] = $found['id'];
}

//----------------------------------------------------------------------
// Handle search
//----------------------------------------------------------------------
if (isset($_POST['search_serial'])) {
	$search = trim(get_post('search_text'));
	if ($search !== '') {
		// Search by serial number across all items
		$result = get_serial_numbers('', '', '', $search, false, '', '', 1, 0);
		$row = db_fetch($result);
		if ($row) {
			$_POST['serial_id'] = $row['id'];
			$Ajax->activate('_page_body');
		} else {
			display_error(sprintf(_('No serial number found matching "%s".'), htmlspecialchars($search)));
		}
	}
}

// Handle serial selector change
if (list_updated('serial_id_select'))
	$_POST['serial_id'] = get_post('serial_id_select');

//======================================================================
//  S E A R C H   F O R M
//======================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

text_cells(_('Search Serial:'), 'search_text', get_post('search_text'), 30, 100,
	_('Enter serial number to search'));
submit_cells('search_serial', _('Find'), '', _('Search for serial number'), 'default');

end_row();
end_table();

//======================================================================
//  D I S P L A Y   S E R I A L   D E T A I L S
//======================================================================

$serial_id = get_post('serial_id', 0);

if ($serial_id > 0) {
	$serial = get_serial_number($serial_id);

	if (!$serial) {
		display_error(_('Serial number record not found.'));
	} else {

		// Header
		echo "<div style='margin:15px 0;'>";
		echo "<h3 style='margin:0 0 5px 0;'>"
			. sprintf(_('Serial Number: %s'), '<strong>' . $serial['serial_no'] . '</strong>')
			. " &mdash; " . serial_status_badge($serial['status'])
			. "</h3>";
		echo "<div style='color:#666;font-size:12px;'>"
			. sprintf(_('Item: %s — %s'), $serial['stock_id'], $serial['item_description'])
			. "</div>";
		echo "</div>";

		//--------------------------------------------------------------
		// Detail Card
		//--------------------------------------------------------------
		echo "<h4 style='margin:15px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
			. _('Serial Number Details') . "</h4>";

		display_serial_detail_card($serial);

		//--------------------------------------------------------------
		// Status Summary
		//--------------------------------------------------------------
		echo "<h4 style='margin:15px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
			. _('Quick Links') . "</h4>";

		echo "<div style='margin:5px 0;'>";

		// Link to edit
		echo "<a href='" . $path_to_root . "/inventory/manage/serial_numbers.php?stock_id="
			. urlencode($serial['stock_id']) . "'>"
			. _('Manage Serial Numbers for this Item') . "</a>";

		// Link to item movements if stock_id exists
		echo " &nbsp;|&nbsp; <a href='" . $path_to_root . "/inventory/inquiry/stock_movements.php?stock_id="
			. urlencode($serial['stock_id']) . "'>"
			. _('Item Stock Movements') . "</a>";

		echo "</div>";

		//--------------------------------------------------------------
		// Movement Timeline
		//--------------------------------------------------------------
		echo "<h4 style='margin:20px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
			. _('Movement History') . "</h4>";

		display_serial_movement_timeline($serial_id);
	}
}

end_form();
end_page();
