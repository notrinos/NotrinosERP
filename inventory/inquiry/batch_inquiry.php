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
 * Batch/Lot Inquiry — lookup a batch and display its complete detail,
 * location breakdown, movement timeline, and related documents.
 *
 * Access via:
 *   - Direct URL: batch_inquiry.php?batch_id=123
 *   - Search: type batch number in search field
 */
$page_security = 'SA_BATCHINQUIRY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Batch / Lot Inquiry');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);

page($_SESSION['page_title'], isset($_GET['batch_id']), false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');

//----------------------------------------------------------------------
// Handle GET parameter
//----------------------------------------------------------------------
if (isset($_GET['batch_id']))
	$_POST['batch_id'] = (int)$_GET['batch_id'];

if (isset($_GET['batch_no']) && isset($_GET['stock_id'])) {
	$found = get_stock_batch_by_code($_GET['batch_no'], $_GET['stock_id']);
	if ($found)
		$_POST['batch_id'] = $found['id'];
}

//----------------------------------------------------------------------
// Handle search
//----------------------------------------------------------------------
if (isset($_POST['search_batch'])) {
	$search = trim(get_post('search_text'));
	if ($search !== '') {
		$result = get_stock_batches('', '', $search, false, '', 0, 1, 0);
		$row = db_fetch($result);
		if ($row) {
			$_POST['batch_id'] = $row['id'];
			$Ajax->activate('_page_body');
		} else {
			display_error(sprintf(_('No batch found matching "%s".'), htmlspecialchars($search)));
		}
	}
}

//======================================================================
//  S E A R C H   F O R M
//======================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

text_cells(_('Search Batch:'), 'search_text', get_post('search_text'), 30, 100,
	_('Enter batch number to search'));
submit_cells('search_batch', _('Find'), '', _('Search for batch number'), 'default');

end_row();
end_table();

//======================================================================
//  D I S P L A Y   B A T C H   D E T A I L S
//======================================================================

$batch_id = get_post('batch_id', 0);

if ($batch_id > 0) {
	$batch = get_stock_batch($batch_id);

	if (!$batch) {
		display_error(_('Batch record not found.'));
	} else {

		// Header
		echo "<div style='margin:15px 0;'>";
		echo "<h3 style='margin:0 0 5px 0;'>"
			. sprintf(_('Batch: %s'), '<strong>' . $batch['batch_no'] . '</strong>')
			. " &mdash; " . batch_status_badge($batch['status']);
		if ($batch['expiry_date'])
			echo " &mdash; " . batch_expiry_badge($batch['expiry_date']);
		echo "</h3>";
		echo "<div style='color:#666;font-size:12px;'>"
			. sprintf(_('Item: %s — %s'), $batch['stock_id'], $batch['item_description'])
			. "</div>";
		echo "</div>";

		//--------------------------------------------------------------
		// Detail Card
		//--------------------------------------------------------------
		echo "<h4 style='margin:15px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
			. _('Batch Details') . "</h4>";

		display_batch_detail_card($batch);

		//--------------------------------------------------------------
		// Location Breakdown
		//--------------------------------------------------------------
		echo "<h4 style='margin:15px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
			. _('Stock by Location') . "</h4>";

		display_batch_location_breakdown($batch_id);

		//--------------------------------------------------------------
		// Quick Links
		//--------------------------------------------------------------
		echo "<h4 style='margin:15px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
			. _('Quick Links') . "</h4>";

		echo "<div style='margin:5px 0;'>";

		echo "<a href='" . $path_to_root . "/inventory/manage/stock_batches.php?stock_id="
			. urlencode($batch['stock_id']) . "'>"
			. _('Manage Batches for this Item') . "</a>";

		echo " &nbsp;|&nbsp; <a href='" . $path_to_root . "/inventory/inquiry/stock_movements.php?stock_id="
			. urlencode($batch['stock_id']) . "'>"
			. _('Item Stock Movements') . "</a>";

		echo " &nbsp;|&nbsp; <a href='" . $path_to_root . "/inventory/inquiry/expiry_dashboard.php'>"
			. _('Expiry Dashboard') . "</a>";

		echo "</div>";

		//--------------------------------------------------------------
		// Movement Timeline
		//--------------------------------------------------------------
		echo "<h4 style='margin:20px 0 5px 0;border-bottom:1px solid #ddd;padding-bottom:3px;'>"
			. _('Movement History') . "</h4>";

		display_batch_movement_timeline($batch_id);
	}
}

end_form();
end_page();
