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
 * Mobile Bin Putaway — Guided putaway confirmation workflow.
 *
 * Steps: 1. Scan Item → 2. Enter Qty → 3. Scan Bin → 4. Confirm
 *
 * Completes putaway operations from the inbound flow.
 */

$page_security = 'SA_WAREHOUSE_OPERATIONS';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Mobile Putaway');

$js = '';
add_js_file('barcode_scanner.js');

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

<div class="mobile-page" id="mobile-putaway">

	<div class="mobile-header">
		<a href="<?php echo $path_to_root; ?>/inventory/mobile/" class="mobile-back">&#8592; Back</a>
		<h2><?php echo _('Putaway'); ?></h2>
	</div>

	<div class="mobile-steps">
		<div class="mobile-step active" id="step-ind-1">
			<span class="mobile-step-num">1</span> <?php echo _('Item'); ?>
		</div>
		<div class="mobile-step" id="step-ind-2">
			<span class="mobile-step-num">2</span> <?php echo _('Bin'); ?>
		</div>
		<div class="mobile-step" id="step-ind-3">
			<span class="mobile-step-num">3</span> <?php echo _('Done'); ?>
		</div>
	</div>

	<div class="mobile-field">
		<label><?php echo _('Warehouse'); ?></label>
		<select id="loc_code"><?php echo $loc_options; ?></select>
	</div>

	<!-- Step 1: Scan Item -->
	<div id="step1">
		<div class="mobile-scan-area">
			<label><?php echo _('Scan Item for Putaway'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_item"
				placeholder="<?php echo _('Scan barcode...'); ?>" autocomplete="off" autofocus>
		</div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="lookupItem()">
			<?php echo _('Look Up'); ?>
		</button>
		<div id="item-result"></div>
	</div>

	<!-- Step 2: Qty + Tracking + Bin -->
	<div id="step2" style="display:none;">
		<div class="mobile-card" id="item-card"></div>

		<div class="mobile-field">
			<label><?php echo _('Quantity'); ?></label>
			<input type="number" id="put_qty" value="1" min="0.01" step="any"
				style="font-size:20px; text-align:center;">
		</div>

		<div id="serial-field" style="display:none;">
			<div class="mobile-field">
				<label><?php echo _('Serial Number'); ?></label>
				<input type="text" class="mobile-scan-input" id="put_serial"
					placeholder="<?php echo _('Serial #'); ?>" autocomplete="off">
			</div>
		</div>

		<div id="batch-field" style="display:none;">
			<div class="mobile-field">
				<label><?php echo _('Batch Number'); ?></label>
				<input type="text" class="mobile-scan-input" id="put_batch"
					placeholder="<?php echo _('Batch #'); ?>" autocomplete="off">
			</div>
		</div>

		<!-- Suggested bin from putaway engine -->
		<div class="mobile-card" id="suggested-bin-card" style="display:none;">
			<div class="mobile-card-title"><?php echo _('Suggested Bin'); ?></div>
			<div id="suggested-bin"></div>
		</div>

		<div class="mobile-scan-area">
			<label><?php echo _('Scan Destination Bin'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_bin"
				placeholder="<?php echo _('Bin code...'); ?>" autocomplete="off">
		</div>

		<div class="mobile-btn-row">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(1)"><?php echo _('Back'); ?></button>
			<button type="button" class="mobile-btn mobile-btn-success" onclick="confirmPutaway()">
				<?php echo _('Confirm Putaway'); ?>
			</button>
		</div>
		<div id="putaway-result"></div>
	</div>

	<!-- Step 3: Done -->
	<div id="step3" style="display:none;">
		<div id="final-result"></div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="resetForm()">
			<?php echo _('Putaway Another'); ?>
		</button>
	</div>

</div>

<script>
var mh = new MobileHelper({ ajaxUrl: 'mobile_ajax.php' });
var currentItem = null;

function goStep(n) {
	for (var i = 1; i <= 3; i++)
		document.getElementById('step' + i).style.display = (i === n) ? '' : 'none';
	mh.setStep(n, 3);
	if (n === 1) mh.focusScan('scan_item');
	if (n === 2) mh.focusScan('scan_bin');
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
			if (match.track_by) html += mh.cardRow('Tracking', match.track_by);
			document.getElementById('item-card').innerHTML = html;

			var tb = match.track_by || 'none';
			document.getElementById('serial-field').style.display =
				(tb === 'serial' || tb === 'both') ? '' : 'none';
			document.getElementById('batch-field').style.display =
				(tb === 'batch' || tb === 'both') ? '' : 'none';

			// Pre-fill serial/batch if scanned
			if (match.type === 'serial') {
				document.getElementById('put_serial').value = match.serial_no || '';
			} else if (match.type === 'batch') {
				document.getElementById('put_batch').value = match.batch_no || '';
			}

			mh.clearResult('item-result');
			goStep(2);
		} else {
			mh.showResult('item-result', resp.error || 'No match found', 'error');
		}
	});
}

function confirmPutaway() {
	var binCode = document.getElementById('scan_bin').value.trim();
	if (!binCode) { mh.toast('Scan destination bin first', 'error'); return; }

	// Resolve bin
	mh.showLoading('putaway-result');
	mh.post('scan_lookup', { scan: binCode }, function(resp) {
		if (resp.success && resp.matches && resp.matches.length > 0 && resp.matches[0].type === 'bin') {
			var bin = resp.matches[0];
			doPutaway(bin.loc_id);
		} else {
			mh.showResult('putaway-result', 'Bin not found: ' + binCode, 'error');
		}
	});
}

function doPutaway(binLocId) {
	mh.post('confirm_putaway', {
		stock_id: currentItem ? currentItem.stock_id : '',
		qty: document.getElementById('put_qty').value,
		bin_loc_id: binLocId,
		serial_no: document.getElementById('put_serial').value.trim(),
		batch_no: document.getElementById('put_batch').value.trim(),
		loc_code: document.getElementById('loc_code').value
	}, function(resp) {
		if (resp.success) {
			mh.showResult('final-result', resp.message, 'success');
			mh.toast(resp.message, 'success');
			goStep(3);
		} else {
			mh.showResult('putaway-result', resp.error, 'error');
			mh.toast(resp.error, 'error');
		}
	});
}

function resetForm() {
	currentItem = null;
	document.getElementById('scan_item').value = '';
	document.getElementById('put_qty').value = '1';
	document.getElementById('put_serial').value = '';
	document.getElementById('put_batch').value = '';
	document.getElementById('scan_bin').value = '';
	mh.clearResult('item-result');
	mh.clearResult('putaway-result');
	mh.clearResult('final-result');
	goStep(1);
}

mh.initScanner(function(code) {
	if (document.getElementById('step1').style.display !== 'none') {
		document.getElementById('scan_item').value = code;
		lookupItem();
	} else if (document.getElementById('step2').style.display !== 'none') {
		document.getElementById('scan_bin').value = code;
		confirmPutaway();
	}
});

document.getElementById('scan_item').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); lookupItem(); }
});
document.getElementById('scan_bin').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); confirmPutaway(); }
});
</script>

<?php end_page(); ?>
