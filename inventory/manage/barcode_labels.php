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
 * Barcode Label Printing & Scanner Test Page
 *
 * Features:
 *   - Generate and preview barcode images for items, serial numbers, batch numbers
 *   - Select barcode type (Code128, Code39, EAN-13, QR Code, GS1-128)
 *   - Print barcode labels as PDF (single or batch)
 *   - GS1 barcode builder (GTIN + Batch + Expiry + Serial)
 *   - Barcode scanner test area (hardware scanner keyboard wedge detection)
 *   - Barcode scan lookup (find item/serial/batch by scanning)
 */
$page_security = 'SA_BARCODELABELS';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Barcode Labels & Scanner');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

add_js_file('barcode_scanner.js');

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/includes/barcode_generator.inc');
include_once($path_to_root . '/inventory/includes/gs1_standards.inc');
include_once($path_to_root . '/inventory/includes/db/items_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');

//-------------------------------------------------------------------------------------
// Handle PDF label generation
//-------------------------------------------------------------------------------------

if (isset($_POST['print_labels'])) {
	$label_type = get_post('label_source');
	$barcode_type = get_post('barcode_type', 'C128B');
	$label_size = get_post('label_size', 'medium');
	$labels = array();

	if ($label_type === 'item') {
		$stock_id = get_post('stock_id');
		if ($stock_id) {
			$copies = max(1, min(100, (int)get_post('label_copies', 1)));
			$lbl = get_item_label_data($stock_id);
			if ($lbl) {
				for ($i = 0; $i < $copies; $i++) {
					$labels[] = $lbl;
				}
			}
		}
	} elseif ($label_type === 'serial') {
		// Get selected serial IDs
		$serial_ids = array();
		foreach ($_POST as $key => $val) {
			if (strpos($key, 'sel_serial_') === 0 && $val) {
				$serial_ids[] = (int)str_replace('sel_serial_', '', $key);
			}
		}
		// If no checkboxes, try single serial
		if (empty($serial_ids) && get_post('serial_id')) {
			$serial_ids[] = (int)get_post('serial_id');
		}
		foreach ($serial_ids as $sid) {
			$lbl = get_serial_label_data($sid);
			if ($lbl) {
				$labels[] = $lbl;
			}
		}
	} elseif ($label_type === 'batch') {
		$batch_ids = array();
		foreach ($_POST as $key => $val) {
			if (strpos($key, 'sel_batch_') === 0 && $val) {
				$batch_ids[] = (int)str_replace('sel_batch_', '', $key);
			}
		}
		if (empty($batch_ids) && get_post('batch_id')) {
			$batch_ids[] = (int)get_post('batch_id');
		}
		foreach ($batch_ids as $bid) {
			$lbl = get_batch_label_data($bid);
			if ($lbl) {
				$labels[] = $lbl;
			}
		}
	} elseif ($label_type === 'gs1') {
		$gtin = get_post('gs1_gtin', '');
		$serial = get_post('gs1_serial', '');
		$batch = get_post('gs1_batch', '');
		$expiry = get_post('gs1_expiry', '');

		if ($gtin) {
			$gs1_string = build_gs1_barcode_string($gtin, $serial, $batch, $expiry);
			$copies = max(1, min(100, (int)get_post('label_copies', 1)));

			$subtitle_parts = array();
			if ($batch) $subtitle_parts[] = 'Batch: ' . $batch;
			if ($serial) $subtitle_parts[] = 'SN: ' . $serial;
			if ($expiry && $expiry !== '0000-00-00') $subtitle_parts[] = 'EXP: ' . $expiry;

			for ($i = 0; $i < $copies; $i++) {
				$labels[] = array(
					'code'     => $gs1_string,
					'title'    => 'GTIN: ' . $gtin,
					'subtitle' => implode(' | ', $subtitle_parts),
				);
			}
		}
	} elseif ($label_type === 'custom') {
		$custom_code = get_post('custom_code', '');
		$custom_title = get_post('custom_title', '');
		$custom_subtitle = get_post('custom_subtitle', '');
		$copies = max(1, min(100, (int)get_post('label_copies', 1)));

		if ($custom_code) {
			for ($i = 0; $i < $copies; $i++) {
				$labels[] = array(
					'code'     => $custom_code,
					'title'    => $custom_title,
					'subtitle' => $custom_subtitle,
				);
			}
		}
	}

	if (!empty($labels)) {
		generate_barcode_labels_pdf($labels, $barcode_type, $label_size, 'barcode_labels.pdf');
		// generate_barcode_labels_pdf() calls exit()
	} else {
		display_error(_('No label data available. Please select items to print.'));
	}
}

//-------------------------------------------------------------------------------------
// Handle barcode preview (AJAX)
//-------------------------------------------------------------------------------------

if (isset($_POST['preview_barcode'])) {
	// Just re-render the page with preview data set in POST
	$_POST['show_preview'] = 1;
	$Ajax->activate('_page_body');
}

//-------------------------------------------------------------------------------------
// Handle GS1 parse test
//-------------------------------------------------------------------------------------
$gs1_parse_result = null;
if (isset($_POST['parse_gs1'])) {
	$gs1_input = get_post('gs1_test_input', '');
	if ($gs1_input) {
		$gs1_parse_result = parse_gs1_barcode($gs1_input);
	}
	$Ajax->activate('_page_body');
}

//-------------------------------------------------------------------------------------
// Main page rendering
//-------------------------------------------------------------------------------------

echo '<style>
.barcode-section { background: #f9f9f9; border: 1px solid #ddd; border-radius: 4px; padding: 12px; margin-bottom: 16px; }
.barcode-section h3 { margin: 0 0 10px 0; font-size: 14px; color: #333; border-bottom: 1px solid #ddd; padding-bottom: 6px; }
.barcode-preview { text-align: center; padding: 16px; background: #fff; border: 1px solid #eee; border-radius: 4px; margin: 8px 0; }
.scanner-test-area { border: 2px dashed #28a745; border-radius: 8px; padding: 20px; text-align: center; background: #f0fff0; margin: 12px 0; cursor: pointer; }
.scanner-test-area.active { border-color: #007bff; background: #f0f0ff; }
.scan-result { background: #fff; border: 1px solid #ddd; border-radius: 4px; padding: 12px; margin-top: 8px; display: none; }
.scan-result.visible { display: block; }
.gs1-field { display: inline-block; margin: 2px 4px; padding: 3px 8px; background: #e9ecef; border-radius: 3px; font-family: monospace; }
.gs1-ai { font-weight: bold; color: #0056b3; }
.gs1-value { color: #333; }
</style>';

start_form();

//-------------------------------------------------------------------------------------
// Section 1: Barcode Preview Generator
//-------------------------------------------------------------------------------------

div_start('barcode_preview_section');
echo '<div class="barcode-section">';
echo '<h3><i class="fa fa-barcode"></i> ' . _('Barcode Preview') . '</h3>';

start_table(TABLESTYLE2);

// Barcode type selector
$barcode_types = get_barcode_types();
array_selector_row(_('Barcode Type:'), 'barcode_type', get_post('barcode_type', 'C128B'), $barcode_types);

// Code to encode
text_row(_('Data to Encode:'), 'preview_code', get_post('preview_code', ''), 40, 100);

submit_center_first('preview_barcode', _('Preview'), _('Generate barcode preview'), 'default');

end_table();

// Show preview if barcode type and code are set
$preview_code = get_post('preview_code', '');
$preview_type = get_post('barcode_type', 'C128B');
if ($preview_code) {
	echo '<div class="barcode-preview">';
	echo display_barcode_image($preview_code, $preview_type, 400, 100);
	echo '<div style="margin-top:8px; font-family:monospace; color:#666;">' . htmlspecialchars($preview_code, ENT_QUOTES, 'UTF-8') . '</div>';
	echo '<div style="color:#888; font-size:11px;">' . htmlspecialchars($barcode_types[$preview_type], ENT_QUOTES, 'UTF-8') . '</div>';
	echo '</div>';
}

echo '</div>';
div_end();

//-------------------------------------------------------------------------------------
// Section 2: Label Printing
//-------------------------------------------------------------------------------------

echo '<div class="barcode-section">';
echo '<h3><i class="fa fa-print"></i> ' . _('Print Barcode Labels') . '</h3>';

start_table(TABLESTYLE2);

// Label source selector
$label_sources = array(
	'item'   => _('Item'),
	'serial' => _('Serial Number'),
	'batch'  => _('Batch / Lot'),
	'gs1'    => _('GS1 Composite'),
	'custom' => _('Custom Code'),
);
array_selector_row(_('Label Source:'), 'label_source', get_post('label_source', 'item'), $label_sources,
	array('select_submit' => true));

// Barcode type for labels
array_selector_row(_('Barcode Format:'), 'barcode_type_label', get_post('barcode_type_label', 'C128B'), $barcode_types);

// Label size
$label_sizes = array(
	'small'  => _('Small (2" × 1")'),
	'medium' => _('Medium (3" × 1.5")'),
	'large'  => _('Large (4" × 2")'),
);
array_selector_row(_('Label Size:'), 'label_size', get_post('label_size', 'medium'), $label_sizes);

end_table();

$label_source = get_post('label_source', 'item');

echo '<div style="padding: 8px 0;">';

if ($label_source === 'item') {
	start_table(TABLESTYLE2);
	stock_items_list_cells(_('Item:'), 'stock_id', get_post('stock_id'), false, true);
	end_row();
	start_row();
	small_amount_row(_('Number of Copies:'), 'label_copies', get_post('label_copies', 1), null, null, 0);
	end_table();

} elseif ($label_source === 'serial') {
	start_table(TABLESTYLE2);
	// Filter by item
	$serial_items = get_serial_tracked_items();
	if (db_num_rows($serial_items) > 0) {
		echo '<tr><td class="label">' . _('Filter by Item:') . '</td><td>';
		echo '<select name="serial_filter_item">';
		echo '<option value="">' . _('All Serial-Tracked Items') . '</option>';
		while ($sitem = db_fetch_assoc($serial_items)) {
			$sel = (get_post('serial_filter_item') === $sitem['stock_id']) ? ' selected' : '';
			echo '<option value="' . htmlspecialchars($sitem['stock_id'], ENT_QUOTES, 'UTF-8') . '"' . $sel . '>'
				. htmlspecialchars($sitem['stock_id'] . ' - ' . $sitem['description'], ENT_QUOTES, 'UTF-8') . '</option>';
		}
		echo '</select></td></tr>';
	}
	end_table();

	// Show serial numbers for selection
	$filter_item = get_post('serial_filter_item', '');
	$serials_result = get_serial_numbers($filter_item, 'available', '', '', false, '', '', 1, 50);
	if (db_num_rows($serials_result) > 0) {
		start_table(TABLESTYLE);
		$th = array('', _('Serial #'), _('Item'), _('Status'), _('Location'));
		table_header($th);
		$k = 0;
		while ($sn = db_fetch_assoc($serials_result)) {
			alt_table_row_color($k);
			echo '<td>';
			check_value('sel_serial_' . $sn['id']);
			echo '<input type="checkbox" name="sel_serial_' . (int)$sn['id'] . '" value="1"';
			if (get_post('sel_serial_' . $sn['id'])) echo ' checked';
			echo '></td>';
			label_cell($sn['serial_no']);
			label_cell($sn['stock_id']);
			label_cell(get_serial_status_label($sn['status']));
			label_cell($sn['loc_code']);
			echo '</tr>';
		}
		end_table();
	} else {
		display_note(_('No serial numbers found.'));
	}

} elseif ($label_source === 'batch') {
	start_table(TABLESTYLE2);
	$batch_items = get_batch_tracked_items();
	if (db_num_rows($batch_items) > 0) {
		echo '<tr><td class="label">' . _('Filter by Item:') . '</td><td>';
		echo '<select name="batch_filter_item">';
		echo '<option value="">' . _('All Batch-Tracked Items') . '</option>';
		while ($bitem = db_fetch_assoc($batch_items)) {
			$sel = (get_post('batch_filter_item') === $bitem['stock_id']) ? ' selected' : '';
			echo '<option value="' . htmlspecialchars($bitem['stock_id'], ENT_QUOTES, 'UTF-8') . '"' . $sel . '>'
				. htmlspecialchars($bitem['stock_id'] . ' - ' . $bitem['description'], ENT_QUOTES, 'UTF-8') . '</option>';
		}
		echo '</select></td></tr>';
	}
	end_table();

	$filter_batch_item = get_post('batch_filter_item', '');
	$batches_result = get_stock_batches($filter_batch_item, 'active', '', false, '', '', 1, 50);
	if (db_num_rows($batches_result) > 0) {
		start_table(TABLESTYLE);
		$th = array('', _('Batch #'), _('Item'), _('Status'), _('Expiry'));
		table_header($th);
		$k = 0;
		while ($bt = db_fetch_assoc($batches_result)) {
			alt_table_row_color($k);
			echo '<td>';
			echo '<input type="checkbox" name="sel_batch_' . (int)$bt['id'] . '" value="1"';
			if (get_post('sel_batch_' . $bt['id'])) echo ' checked';
			echo '></td>';
			label_cell($bt['batch_no']);
			label_cell($bt['stock_id']);
			label_cell(get_batch_status_label($bt['status']));
			label_cell(($bt['expiry_date'] && $bt['expiry_date'] !== '0000-00-00') ? sql2date($bt['expiry_date']) : '-');
			echo '</tr>';
		}
		end_table();
	} else {
		display_note(_('No batch numbers found.'));
	}

} elseif ($label_source === 'gs1') {
	start_table(TABLESTYLE2);
	text_row(_('GTIN (up to 14 digits):'), 'gs1_gtin', get_post('gs1_gtin', ''), 20, 14);
	text_row(_('Batch / Lot Number:'), 'gs1_batch', get_post('gs1_batch', ''), 25, 20);
	text_row(_('Serial Number:'), 'gs1_serial', get_post('gs1_serial', ''), 25, 20);
	date_row(_('Expiry Date:'), 'gs1_expiry', get_post('gs1_expiry', ''), false, 0, 0, 1001);
	small_amount_row(_('Number of Copies:'), 'label_copies', get_post('label_copies', 1), null, null, 0);
	end_table();

	// Show GS1 preview
	$gs1_gtin = get_post('gs1_gtin', '');
	if ($gs1_gtin) {
		$gs1_preview = build_gs1_barcode_string(
			$gs1_gtin,
			get_post('gs1_serial', ''),
			get_post('gs1_batch', ''),
			get_post('gs1_expiry', '')
		);
		echo '<div class="barcode-preview">';
		echo '<div style="font-family:monospace; margin-bottom:8px;">' . htmlspecialchars($gs1_preview, ENT_QUOTES, 'UTF-8') . '</div>';
		echo display_barcode_image($gs1_preview, 'C128B', 500, 80);
		echo '</div>';
	}

} elseif ($label_source === 'custom') {
	start_table(TABLESTYLE2);
	text_row(_('Barcode Data:'), 'custom_code', get_post('custom_code', ''), 40, 100);
	text_row(_('Title (top line):'), 'custom_title', get_post('custom_title', ''), 40, 60);
	text_row(_('Subtitle (bottom):'), 'custom_subtitle', get_post('custom_subtitle', ''), 40, 60);
	small_amount_row(_('Number of Copies:'), 'label_copies', get_post('label_copies', 1), null, null, 0);
	end_table();
}

echo '</div>';

submit_center('print_labels', _('Print Labels (PDF)'), true, _('Generate PDF with barcode labels'), 'default');

echo '</div>';

//-------------------------------------------------------------------------------------
// Section 3: GS1 Barcode Parser / Tester
//-------------------------------------------------------------------------------------

echo '<div class="barcode-section">';
echo '<h3><i class="fa fa-tags"></i> ' . _('GS1 Barcode Parser') . '</h3>';
echo '<p style="color:#666; font-size:12px;">' . _('Test GS1 barcode parsing. Enter a GS1 barcode string in either parenthesized format (01)12345678901234(10)BATCH(17)261231(21)SERIAL or raw format.') . '</p>';

start_table(TABLESTYLE2);
text_row(_('GS1 Barcode Data:'), 'gs1_test_input', get_post('gs1_test_input', ''), 60, 200);
end_table();

submit_center('parse_gs1', _('Parse GS1'), true, _('Parse GS1 barcode and show Application Identifiers'), 'default');

if ($gs1_parse_result !== null && count($gs1_parse_result) > 1) {
	echo '<div style="margin-top:12px; padding:12px; background:#fff; border:1px solid #ddd; border-radius:4px;">';
	echo '<strong>' . _('Parsed Application Identifiers:') . '</strong><br><br>';

	$ai_defs = get_gs1_application_identifiers();
	foreach ($gs1_parse_result as $ai => $value) {
		if ($ai === '_raw' || strpos($ai, '_formatted') !== false) {
			continue;
		}
		$name = isset($ai_defs[$ai]) ? $ai_defs[$ai]['name'] : _('AI') . ' ' . $ai;
		$formatted = isset($gs1_parse_result[$ai . '_formatted']) ? ' → ' . $gs1_parse_result[$ai . '_formatted'] : '';

		echo '<span class="gs1-field">';
		echo '<span class="gs1-ai">(' . htmlspecialchars($ai, ENT_QUOTES, 'UTF-8') . ')</span> ';
		echo '<span class="gs1-value">' . htmlspecialchars($value, ENT_QUOTES, 'UTF-8') . htmlspecialchars($formatted, ENT_QUOTES, 'UTF-8') . '</span>';
		echo '</span> ';
		echo '<span style="color:#888; font-size:11px;">' . htmlspecialchars($name, ENT_QUOTES, 'UTF-8') . '</span><br>';
	}
	echo '</div>';
} elseif ($gs1_parse_result !== null) {
	display_warning(_('No GS1 Application Identifiers found in the input.'));
}

echo '</div>';

//-------------------------------------------------------------------------------------
// Section 4: Scanner Test Area
//-------------------------------------------------------------------------------------

echo '<div class="barcode-section">';
echo '<h3><i class="fa fa-wifi"></i> ' . _('Hardware Scanner Test') . '</h3>';
echo '<p style="color:#666; font-size:12px;">'
	. _('Click the area below and scan a barcode with your hardware scanner. The scanner must be in keyboard wedge mode (sends keystrokes followed by Enter).')
	. '</p>';

echo '<div id="scanner_test_area" class="scanner-test-area" tabindex="0">';
echo '<i class="fa fa-barcode" style="font-size:48px; color:#28a745; margin-bottom:12px; display:block;"></i>';
echo '<span id="scanner_status">' . _('Click here, then scan a barcode...') . '</span>';
echo '</div>';

echo '<div id="scan_result" class="scan-result">';
echo '<strong>' . _('Last Scan Result:') . '</strong>';
echo '<div id="scan_result_content"></div>';
echo '</div>';

// Scanner test JS
echo '<script type="text/javascript">
(function() {
	var area = document.getElementById("scanner_test_area");
	var resultDiv = document.getElementById("scan_result");
	var resultContent = document.getElementById("scan_result_content");
	var statusSpan = document.getElementById("scanner_status");

	if (!area || typeof BarcodeScanner === "undefined") return;

	var scanner = new BarcodeScanner({
		ajaxUrl: "' . $path_to_root . '/inventory/includes/barcode_ajax.inc",
		minLength: 3,
		maxDelay: 80,
		autoLookup: true,
		onScan: function(result) {
			resultDiv.className = "scan-result visible";
			var html = "<div style=\"margin:8px 0;\">";
			html += "<strong>' . _('Scanned Code:') . '</strong> <code>" + escapeHtml(result.code) + "</code><br>";
			html += "<strong>' . _('Type:') . '</strong> " + escapeHtml(result.type) + "<br>";

			if (result.gs1 && result.gs1.type === "gs1") {
				html += "<strong>' . _('GS1 Data:') . '</strong><br>";
				if (result.gs1.gtin) html += " &nbsp; GTIN: <code>" + escapeHtml(result.gs1.gtin) + "</code><br>";
				if (result.gs1.serial) html += " &nbsp; ' . _('Serial:') . ' <code>" + escapeHtml(result.gs1.serial) + "</code><br>";
				if (result.gs1.batch) html += " &nbsp; ' . _('Batch:') . ' <code>" + escapeHtml(result.gs1.batch) + "</code><br>";
				if (result.gs1.expiry_date) html += " &nbsp; ' . _('Expiry:') . ' " + escapeHtml(result.gs1.expiry_date) + "<br>";
			}

			if (result.matches && result.matches.length > 0) {
				html += "<br><strong>' . _('Database Matches:') . '</strong><br>";
				for (var i = 0; i < result.matches.length; i++) {
					var m = result.matches[i];
					html += "<div style=\"padding:4px 8px; margin:4px 0; background:#e8f5e9; border-radius:3px;\">";
					html += "<strong>" + escapeHtml(m.match_type) + ":</strong> ";
					if (m.stock_id) html += escapeHtml(m.stock_id) + " - ";
					if (m.description) html += escapeHtml(m.description);
					if (m.serial_no) html += " (SN: " + escapeHtml(m.serial_no) + ")";
					if (m.batch_no) html += " (Batch: " + escapeHtml(m.batch_no) + ")";
					if (m.status) html += " [" + escapeHtml(m.status) + "]";
					html += "</div>";
				}
			} else if (result.success === false) {
				html += "<br><span style=\"color:#dc3545;\">' . _('No matches found in database.') . '</span>";
			}

			html += "</div>";
			resultContent.innerHTML = html;

			scanner.showNotification("' . _('Barcode scanned:') . ' " + result.code, result.success ? "success" : "info");
			statusSpan.textContent = "' . _('Last scan:') . ' " + result.code;
		},
		onError: function(msg) {
			scanner.showNotification(msg, "error");
		}
	});

	// Enable scanner when area is focused
	area.addEventListener("focus", function() {
		scanner.enable();
		area.className = "scanner-test-area active";
		statusSpan.textContent = "' . _('Scanner active — waiting for scan...') . '";
	});

	area.addEventListener("blur", function() {
		scanner.disable();
		area.className = "scanner-test-area";
		statusSpan.textContent = "' . _('Click here, then scan a barcode...') . '";
	});

	// Also add manual scan button
	var manualBtn = document.createElement("button");
	manualBtn.type = "button";
	manualBtn.className = "btn btn-default";
	manualBtn.style.marginTop = "8px";
	manualBtn.innerHTML = "<i class=\"fa fa-keyboard-o\"></i> ' . _('Manual Entry') . '";
	manualBtn.addEventListener("click", function(e) {
		e.preventDefault();
		var code = prompt("' . _('Enter barcode data:') . '");
		if (code && code.trim()) {
			scanner.enable();
			scanner.processCode(code.trim());
		}
	});
	area.parentNode.insertBefore(manualBtn, resultDiv);

	function escapeHtml(text) {
		if (!text) return "";
		var div = document.createElement("div");
		div.appendChild(document.createTextNode(text));
		return div.innerHTML;
	}
})();
</script>';

echo '</div>';

//-------------------------------------------------------------------------------------
// Section 5: Quick Reference
//-------------------------------------------------------------------------------------

echo '<div class="barcode-section">';
echo '<h3><i class="fa fa-info-circle"></i> ' . _('Quick Reference') . '</h3>';

echo '<table class="tablestyle" style="width:100%;">';
echo '<tr class="tableheader"><th>' . _('Barcode Type') . '</th><th>' . _('Best For') . '</th><th>' . _('Max Length') . '</th><th>' . _('Notes') . '</th></tr>';

$ref = array(
	array('Code 128',  _('Serial numbers, internal labels'), _('Variable'),  _('Most versatile 1D barcode. Recommended default.')),
	array('Code 39',   _('Industrial, defense'),              _('43 chars'),  _('Alphanumeric. Wider than Code 128.')),
	array('EAN-13',    _('Retail products'),                  _('13 digits'), _('Requires valid check digit. Numeric only.')),
	array('EAN-8',     _('Small retail products'),            _('8 digits'),  _('Compact version of EAN-13.')),
	array('QR Code',   _('Mobile scanning, URLs, large data'),_('~2000 chars'), _('2D code. Scannable by smartphones.')),
	array('GS1-128',   _('Supply chain, shipping'),          _('Variable'),  _('Encodes Application Identifiers (GTIN, batch, expiry, serial).')),
);

$k = 0;
foreach ($ref as $row) {
	alt_table_row_color($k);
	foreach ($row as $cell) {
		label_cell($cell);
	}
	echo '</tr>';
}
echo '</table>';

echo '<div style="margin-top:8px; font-size:12px; color:#666;">';
echo '<strong>' . _('GS1 Application Identifiers:') . '</strong> ';
echo '(01) GTIN &nbsp;|&nbsp; (10) Batch/Lot &nbsp;|&nbsp; (17) Expiry Date &nbsp;|&nbsp; (21) Serial Number &nbsp;|&nbsp; (11) Production Date &nbsp;|&nbsp; (37) Count';
echo '</div>';

echo '</div>';

end_form();

end_page();
