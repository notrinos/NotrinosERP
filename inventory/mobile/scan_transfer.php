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
 * Mobile Scan Transfer — Bin-to-bin transfer workflow.
 *
 * Steps: 1. Scan Item → 2. Scan Source Bin → 3. Scan Dest Bin → 4. Confirm
 */

$page_security = 'SA_LOCATIONTRANSFER';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Mobile Transfer');

$js = '';
add_js_file('barcode_scanner.js');
add_js_ufile($path_to_root . '/inventory/mobile/mobile_helper.js?v=' . filemtime(dirname(__FILE__) . '/mobile_helper.js'));

page($_SESSION['page_title'], false, false, '', $js, false,
	$path_to_root . '/inventory/mobile/mobile.css');

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');

$wms_locs = get_wms_enabled_locations();
$loc_options = '';
while ($loc = db_fetch($wms_locs)) {
	$loc_options .= '<option value="' . htmlspecialchars($loc['loc_code']) . '">'
		. htmlspecialchars($loc['loc_code'] . ' — ' . $loc['location_name']) . '</option>';
}
?>

<div class="mobile-page" id="mobile-transfer">

	<div class="mobile-header">
		<a href="<?php echo $path_to_root; ?>/inventory/mobile/" class="mobile-back">&#8592; Back</a>
		<h2><?php echo _('Bin Transfer'); ?></h2>
	</div>

	<div class="mobile-steps">
		<div class="mobile-step active" id="step-ind-1">
			<span class="mobile-step-num">1</span> <?php echo _('Item'); ?>
		</div>
		<div class="mobile-step" id="step-ind-2">
			<span class="mobile-step-num">2</span> <?php echo _('From'); ?>
		</div>
		<div class="mobile-step" id="step-ind-3">
			<span class="mobile-step-num">3</span> <?php echo _('To'); ?>
		</div>
		<div class="mobile-step" id="step-ind-4">
			<span class="mobile-step-num">4</span> <?php echo _('Done'); ?>
		</div>
	</div>

	<div class="mobile-field">
		<label><?php echo _('Warehouse'); ?></label>
		<select id="loc_code"><?php echo $loc_options; ?></select>
	</div>

	<!-- Step 1: Scan Item -->
	<div id="step1">
		<div class="mobile-scan-area">
			<label><?php echo _('Scan Item to Transfer'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_item"
				placeholder="<?php echo _('Scan barcode...'); ?>" autocomplete="off" autofocus>
		</div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="lookupItem()">
			<?php echo _('Look Up'); ?>
		</button>
		<div id="item-result"></div>
	</div>

	<!-- Step 2: Qty + Source Bin + Tracking -->
	<div id="step2" style="display:none;">
		<div class="mobile-card" id="item-card"></div>

		<div class="mobile-field">
			<label><?php echo _('Quantity'); ?></label>
			<input type="number" id="xfer_qty" value="1" min="0.01" step="any"
				style="font-size:20px; text-align:center;">
		</div>

		<div id="serial-field" style="display:none;">
			<div class="mobile-field">
				<label><?php echo _('Serial Number'); ?></label>
				<input type="text" class="mobile-scan-input" id="xfer_serial"
					placeholder="<?php echo _('Serial #'); ?>" autocomplete="off">
			</div>
		</div>

		<div id="batch-field" style="display:none;">
			<div class="mobile-field">
				<label><?php echo _('Batch Number'); ?></label>
				<input type="text" class="mobile-scan-input" id="xfer_batch"
					placeholder="<?php echo _('Batch #'); ?>" autocomplete="off">
			</div>
		</div>

		<div class="mobile-scan-area">
			<label><?php echo _('Scan Source Bin'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_from_bin"
				placeholder="<?php echo _('Source bin code...'); ?>" autocomplete="off">
		</div>

		<div class="mobile-btn-row">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(1)"><?php echo _('Back'); ?></button>
			<button type="button" class="mobile-btn mobile-btn-primary" onclick="goStep(3)"><?php echo _('Next: Dest Bin'); ?></button>
		</div>
		<div id="from-result"></div>
	</div>

	<!-- Step 3: Destination Bin -->
	<div id="step3" style="display:none;">
		<div class="mobile-card" id="transfer-summary"></div>

		<div class="mobile-scan-area">
			<label><?php echo _('Scan Destination Bin'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_to_bin"
				placeholder="<?php echo _('Destination bin code...'); ?>" autocomplete="off">
		</div>

		<div class="mobile-btn-row">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(2)"><?php echo _('Back'); ?></button>
			<button type="button" class="mobile-btn mobile-btn-success" onclick="confirmTransfer()">
				<?php echo _('Confirm Transfer'); ?>
			</button>
		</div>
		<div id="to-result"></div>
	</div>

	<!-- Step 4: Done -->
	<div id="step4" style="display:none;">
		<div id="final-result"></div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="resetForm()">
			<?php echo _('Transfer Another'); ?>
		</button>
	</div>
</div>

<script>
var mh = new MobileHelper({ ajaxUrl: 'mobile_ajax.php' });
var currentItem = null;

function goStep(n) {
	for (var i = 1; i <= 4; i++)
		document.getElementById('step' + i).style.display = (i === n) ? '' : 'none';
	mh.setStep(n, 4);

	if (n === 1) mh.focusScan('scan_item');
	if (n === 2) mh.focusScan('scan_from_bin');
	if (n === 3) {
		var summary = '<div class="mobile-card-title">' + mh.escapeHtml(currentItem ? currentItem.stock_id : '') + '</div>';
		summary += mh.cardRow('Qty', document.getElementById('xfer_qty').value);
		summary += mh.cardRow('From Bin', document.getElementById('scan_from_bin').value);
		var sn = document.getElementById('xfer_serial').value;
		var bn = document.getElementById('xfer_batch').value;
		if (sn) summary += mh.cardRow('Serial', sn);
		if (bn) summary += mh.cardRow('Batch', bn);
		document.getElementById('transfer-summary').innerHTML = summary;
		mh.focusScan('scan_to_bin');
	}
}

function lookupItem() {
	var code = document.getElementById('scan_item').value.trim();
	if (!code) return;

	mh.showLoading('item-result');
	mh.post('scan_lookup', { scan: code }, function(resp) {
		if (resp.success && resp.matches && resp.matches.length > 0) {
			var match = resp.matches[0];
			currentItem = match;

			var html = '<div class="mobile-card-title">' + mh.escapeHtml(match.stock_id || '') + '</div>';
			if (match.description) html += mh.cardRow('Description', match.description);
			document.getElementById('item-card').innerHTML = html;

			var tb = match.track_by || 'none';
			document.getElementById('serial-field').style.display =
				(tb === 'serial' || tb === 'both') ? '' : 'none';
			document.getElementById('batch-field').style.display =
				(tb === 'batch' || tb === 'both') ? '' : 'none';

			mh.clearResult('item-result');
			goStep(2);
		} else {
			mh.showResult('item-result', resp.error || 'No match found', 'error');
		}
	});
}

function confirmTransfer() {
	var fromBin = document.getElementById('scan_from_bin').value.trim();
	var toBin = document.getElementById('scan_to_bin').value.trim();
	if (!fromBin || !toBin) { mh.toast('Both bins required', 'error'); return; }

	// Resolve both bins
	mh.showLoading('to-result');
	mh.post('scan_lookup', { scan: fromBin }, function(fromResp) {
		if (!fromResp.success || !fromResp.matches || !fromResp.matches[0] || fromResp.matches[0].type !== 'bin') {
			mh.showResult('to-result', 'Source bin not found: ' + fromBin, 'error');
			return;
		}
		var fromBinId = fromResp.matches[0].loc_id;

		mh.post('scan_lookup', { scan: toBin }, function(toResp) {
			if (!toResp.success || !toResp.matches || !toResp.matches[0] || toResp.matches[0].type !== 'bin') {
				mh.showResult('to-result', 'Destination bin not found: ' + toBin, 'error');
				return;
			}
			var toBinId = toResp.matches[0].loc_id;

			mh.post('confirm_transfer', {
				stock_id: currentItem ? currentItem.stock_id : '',
				qty: document.getElementById('xfer_qty').value,
				from_bin_id: fromBinId,
				to_bin_id: toBinId,
				serial_no: document.getElementById('xfer_serial').value.trim(),
				batch_no: document.getElementById('xfer_batch').value.trim(),
				loc_code: document.getElementById('loc_code').value
			}, function(resp) {
				if (resp.success) {
					mh.showResult('final-result', resp.message, 'success');
					mh.toast(resp.message, 'success');
					goStep(4);
				} else {
					mh.showResult('to-result', resp.error, 'error');
					mh.toast(resp.error, 'error');
				}
			});
		});
	});
}

function resetForm() {
	currentItem = null;
	document.getElementById('scan_item').value = '';
	document.getElementById('xfer_qty').value = '1';
	document.getElementById('xfer_serial').value = '';
	document.getElementById('xfer_batch').value = '';
	document.getElementById('scan_from_bin').value = '';
	document.getElementById('scan_to_bin').value = '';
	mh.clearResult('item-result');
	mh.clearResult('from-result');
	mh.clearResult('to-result');
	mh.clearResult('final-result');
	goStep(1);
}

mh.initScanner(function(code) {
	if (document.getElementById('step1').style.display !== 'none') {
		document.getElementById('scan_item').value = code;
		lookupItem();
	} else if (document.getElementById('step2').style.display !== 'none') {
		document.getElementById('scan_from_bin').value = code;
	} else if (document.getElementById('step3').style.display !== 'none') {
		document.getElementById('scan_to_bin').value = code;
		confirmTransfer();
	}
});

document.getElementById('scan_item').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); lookupItem(); }
});
document.getElementById('scan_from_bin').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); goStep(3); }
});
document.getElementById('scan_to_bin').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); confirmTransfer(); }
});
</script>

<?php end_page(); ?>
