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
 * Mobile Scan Receive — Guided inbound receipt workflow.
 *
 * Steps: 1. Scan/enter Item → 2. Enter Serial/Batch → 3. Scan Bin → 4. Confirm
 *
 * Uses hardware barcode scanner (keyboard wedge) or manual text entry.
 * Responsive layout for tablets and handheld scanner devices.
 */

$page_security = 'SA_WAREHOUSE_OPERATIONS';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Mobile Receive');

$js = '';
if (file_exists($path_to_root . '/libraries/barcode_scanner.js'))
	$js .= get_js_open_window(0, 0); // dummy to include js infrastructure
add_js_file('barcode_scanner.js');

page($_SESSION['page_title'], false, false, '', $js, false,
	$path_to_root . '/inventory/mobile/mobile.css');

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');

// Get WMS-enabled locations for dropdown
$wms_locs = get_wms_enabled_locations();
$loc_options = '';
while ($loc = db_fetch($wms_locs)) {
	$loc_options .= '<option value="' . htmlspecialchars($loc['loc_code']) . '">'
		. htmlspecialchars($loc['loc_code'] . ' — ' . $loc['location_name']) . '</option>';
}
?>

<div class="mobile-page" id="mobile-receive">

	<div class="mobile-header">
		<a href="<?php echo $path_to_root; ?>/inventory/mobile/" class="mobile-back">&#8592; Back</a>
		<h2><?php echo _('Scan Receive'); ?></h2>
	</div>

	<!-- Step Indicator -->
	<div class="mobile-steps">
		<div class="mobile-step active" id="step-ind-1">
			<span class="mobile-step-num">1</span> <?php echo _('Item'); ?>
		</div>
		<div class="mobile-step" id="step-ind-2">
			<span class="mobile-step-num">2</span> <?php echo _('Track'); ?>
		</div>
		<div class="mobile-step" id="step-ind-3">
			<span class="mobile-step-num">3</span> <?php echo _('Bin'); ?>
		</div>
		<div class="mobile-step" id="step-ind-4">
			<span class="mobile-step-num">4</span> <?php echo _('Done'); ?>
		</div>
	</div>

	<!-- Warehouse selector -->
	<div class="mobile-field">
		<label><?php echo _('Warehouse'); ?></label>
		<select id="loc_code"><?php echo $loc_options; ?></select>
	</div>

	<!-- Step 1: Scan Item -->
	<div id="step1">
		<div class="mobile-scan-area">
			<label><?php echo _('Scan or enter Item / Barcode'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_item"
				placeholder="<?php echo _('Scan barcode...'); ?>"
				autocomplete="off" autofocus>
			<div class="mobile-scan-hint"><?php echo _('Scan item barcode or type stock ID'); ?></div>
		</div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="lookupItem()">
			<?php echo _('Look Up Item'); ?>
		</button>
		<div id="item-result"></div>
	</div>

	<!-- Step 2: Serial/Batch + Qty -->
	<div id="step2" style="display:none;">
		<div class="mobile-card" id="item-card"></div>

		<div class="mobile-field">
			<label><?php echo _('Quantity'); ?></label>
			<input type="number" id="recv_qty" value="1" min="0.01" step="any"
				style="font-size:20px; text-align:center;">
		</div>

		<div id="serial-field" style="display:none;">
			<div class="mobile-scan-area">
				<label><?php echo _('Scan / Enter Serial Number'); ?></label>
				<input type="text" class="mobile-scan-input" id="recv_serial"
					placeholder="<?php echo _('Serial #'); ?>" autocomplete="off">
			</div>
		</div>

		<div id="batch-field" style="display:none;">
			<div class="mobile-scan-area">
				<label><?php echo _('Scan / Enter Batch Number'); ?></label>
				<input type="text" class="mobile-scan-input" id="recv_batch"
					placeholder="<?php echo _('Batch #'); ?>" autocomplete="off">
			</div>
		</div>

		<div class="mobile-btn-row">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(1)"><?php echo _('Back'); ?></button>
			<button type="button" class="mobile-btn mobile-btn-primary" onclick="goStep(3)"><?php echo _('Next: Bin'); ?></button>
		</div>
	</div>

	<!-- Step 3: Scan Bin -->
	<div id="step3" style="display:none;">
		<div class="mobile-card" id="item-summary"></div>

		<div class="mobile-scan-area">
			<label><?php echo _('Scan Destination Bin'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_bin"
				placeholder="<?php echo _('Bin code...'); ?>" autocomplete="off">
			<div class="mobile-scan-hint"><?php echo _('Scan bin barcode or type bin code'); ?></div>
		</div>
		<div id="bin-result"></div>

		<div class="mobile-btn-row">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(2)"><?php echo _('Back'); ?></button>
			<button type="button" class="mobile-btn mobile-btn-success" onclick="confirmReceive()"
				id="btn-confirm"><?php echo _('Confirm Receipt'); ?></button>
		</div>
	</div>

	<!-- Step 4: Done -->
	<div id="step4" style="display:none;">
		<div id="final-result"></div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="resetForm()">
			<?php echo _('Receive Next Item'); ?>
		</button>
	</div>

</div>

<script>
var mh = new MobileHelper({ ajaxUrl: 'mobile_ajax.php' });
var currentItem = null;
var currentBin = null;

function goStep(n) {
	for (var i = 1; i <= 4; i++)
		document.getElementById('step' + i).style.display = (i === n) ? '' : 'none';
	mh.setStep(n, 4);

	if (n === 3) {
		// Build summary
		var summary = '<div class="mobile-card-title">' + mh.escapeHtml(currentItem.stock_id) + '</div>';
		summary += mh.cardRow('Qty', document.getElementById('recv_qty').value);
		var sn = document.getElementById('recv_serial').value;
		var bn = document.getElementById('recv_batch').value;
		if (sn) summary += mh.cardRow('Serial', sn);
		if (bn) summary += mh.cardRow('Batch', bn);
		document.getElementById('item-summary').innerHTML = summary;
		mh.focusScan('scan_bin');
	} else if (n === 1) {
		mh.focusScan('scan_item');
	}
}

function lookupItem() {
	var code = document.getElementById('scan_item').value.trim();
	if (!code) return;

	mh.showLoading('item-result');
	mh.post('scan_lookup', { scan: code }, function(resp) {
		if (resp.success && resp.matches && resp.matches.length > 0) {
			var match = resp.matches[0];
			if (match.type === 'item' || match.type === 'serial' || match.type === 'batch') {
				currentItem = match;
				// Show item card
				var html = '<div class="mobile-card-title">' + mh.escapeHtml(match.stock_id || '') + '</div>';
				html += mh.cardRow('Description', match.description || '');
				html += mh.cardRow('Track By', match.track_by || 'none');
				document.getElementById('item-card').innerHTML = html;

				// Toggle serial/batch fields
				var tb = match.track_by || 'none';
				document.getElementById('serial-field').style.display =
					(tb === 'serial' || tb === 'both') ? '' : 'none';
				document.getElementById('batch-field').style.display =
					(tb === 'batch' || tb === 'both') ? '' : 'none';

				mh.clearResult('item-result');
				goStep(2);
			} else {
				mh.showResult('item-result', 'Scanned: ' + match.type + ' — ' + (match.stock_id || match.serial_no || match.batch_no), 'info');
				currentItem = match;
				// For serial/batch scans, populate tracking fields
				if (match.type === 'serial') {
					currentItem = { stock_id: match.stock_id, track_by: 'serial' };
					document.getElementById('item-card').innerHTML =
						'<div class="mobile-card-title">' + mh.escapeHtml(match.stock_id) + '</div>' +
						mh.cardRow('Serial', match.serial_no);
					document.getElementById('serial-field').style.display = '';
					document.getElementById('batch-field').style.display = 'none';
					document.getElementById('recv_serial').value = match.serial_no;
					goStep(2);
				} else if (match.type === 'batch') {
					currentItem = { stock_id: match.stock_id, track_by: 'batch' };
					document.getElementById('item-card').innerHTML =
						'<div class="mobile-card-title">' + mh.escapeHtml(match.stock_id) + '</div>' +
						mh.cardRow('Batch', match.batch_no);
					document.getElementById('serial-field').style.display = 'none';
					document.getElementById('batch-field').style.display = '';
					document.getElementById('recv_batch').value = match.batch_no;
					goStep(2);
				}
			}
		} else {
			mh.showResult('item-result', resp.error || 'No match found', 'error');
		}
	});
}

function confirmReceive() {
	var binCode = document.getElementById('scan_bin').value.trim();
	if (!binCode) { mh.toast('Scan destination bin first', 'error'); return; }

	// Resolve bin first
	mh.showLoading('bin-result');
	mh.post('scan_lookup', { scan: binCode }, function(resp) {
		if (resp.success && resp.matches && resp.matches.length > 0 && resp.matches[0].type === 'bin') {
			currentBin = resp.matches[0];
			doReceive();
		} else {
			mh.showResult('bin-result', 'Bin not found: ' + binCode, 'error');
		}
	});
}

function doReceive() {
	var data = {
		stock_id: currentItem.stock_id || '',
		qty: document.getElementById('recv_qty').value,
		bin_loc_id: currentBin.loc_id,
		serial_no: document.getElementById('recv_serial').value.trim(),
		batch_no: document.getElementById('recv_batch').value.trim(),
		loc_code: document.getElementById('loc_code').value
	};

	mh.post('confirm_receive', data, function(resp) {
		if (resp.success) {
			mh.showResult('final-result', resp.message, 'success');
			mh.toast(resp.message, 'success');
			goStep(4);
		} else {
			mh.showResult('bin-result', resp.error, 'error');
			mh.toast(resp.error, 'error');
		}
	});
}

function resetForm() {
	currentItem = null;
	currentBin = null;
	document.getElementById('scan_item').value = '';
	document.getElementById('recv_qty').value = '1';
	document.getElementById('recv_serial').value = '';
	document.getElementById('recv_batch').value = '';
	document.getElementById('scan_bin').value = '';
	mh.clearResult('item-result');
	mh.clearResult('bin-result');
	mh.clearResult('final-result');
	goStep(1);
}

// Hardware scanner: auto-detect if on step 1 (item) or step 3 (bin)
mh.initScanner(function(code) {
	if (document.getElementById('step1').style.display !== 'none') {
		document.getElementById('scan_item').value = code;
		lookupItem();
	} else if (document.getElementById('step3').style.display !== 'none') {
		document.getElementById('scan_bin').value = code;
		confirmReceive();
	}
});

// Enter key handling on scan inputs
document.getElementById('scan_item').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); lookupItem(); }
});
document.getElementById('scan_bin').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); confirmReceive(); }
});
</script>

<?php end_page(); ?>
