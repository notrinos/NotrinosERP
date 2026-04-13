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
 * Mobile Scan Ship — Guided outbound delivery workflow.
 *
 * Steps: 1. Select delivery/order → 2. Scan Serial → 3. Confirm
 */

$page_security = 'SA_DISPATCH_OPERATIONS';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Mobile Ship');

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

<div class="mobile-page" id="mobile-ship">

	<div class="mobile-header">
		<a href="<?php echo $path_to_root; ?>/inventory/mobile/" class="mobile-back">&#8592; Back</a>
		<h2><?php echo _('Scan Ship'); ?></h2>
	</div>

	<div class="mobile-steps">
		<div class="mobile-step active" id="step-ind-1">
			<span class="mobile-step-num">1</span> <?php echo _('Item'); ?>
		</div>
		<div class="mobile-step" id="step-ind-2">
			<span class="mobile-step-num">2</span> <?php echo _('Serial'); ?>
		</div>
		<div class="mobile-step" id="step-ind-3">
			<span class="mobile-step-num">3</span> <?php echo _('Done'); ?>
		</div>
	</div>

	<div class="mobile-field">
		<label><?php echo _('Warehouse'); ?></label>
		<select id="loc_code"><?php echo $loc_options; ?></select>
	</div>

	<!-- Step 1: Select Item -->
	<div id="step1">
		<div class="mobile-scan-area">
			<label><?php echo _('Scan Item to Ship'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_item"
				placeholder="<?php echo _('Scan barcode or item ID...'); ?>"
				autocomplete="off" autofocus>
		</div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="lookupItem()">
			<?php echo _('Look Up Item'); ?>
		</button>
		<div id="item-result"></div>
	</div>

	<!-- Step 2: Scan Serial -->
	<div id="step2" style="display:none;">
		<div class="mobile-card" id="item-card"></div>

		<div class="mobile-card" id="available-serials-card" style="display:none;">
			<div class="mobile-card-title"><?php echo _('Available Serials'); ?></div>
			<ul class="mobile-list" id="serial-list"></ul>
		</div>

		<div class="mobile-scan-area">
			<label><?php echo _('Scan Serial Number to Ship'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_serial"
				placeholder="<?php echo _('Serial #'); ?>" autocomplete="off">
		</div>

		<div class="mobile-btn-row">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(1)"><?php echo _('Back'); ?></button>
			<button type="button" class="mobile-btn mobile-btn-success" onclick="confirmShip()">
				<?php echo _('Confirm Ship'); ?>
			</button>
		</div>
		<div id="ship-result"></div>
	</div>

	<!-- Step 3: Done -->
	<div id="step3" style="display:none;">
		<div id="final-result"></div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="resetForm()">
			<?php echo _('Ship Next Item'); ?>
		</button>
	</div>

</div>

<script>
var mh = new MobileHelper({ ajaxUrl: 'mobile_ajax.php' });
var currentItem = null;
var scannedSerials = [];

function goStep(n) {
	for (var i = 1; i <= 3; i++)
		document.getElementById('step' + i).style.display = (i === n) ? '' : 'none';
	mh.setStep(n, 3);
	if (n === 1) mh.focusScan('scan_item');
	if (n === 2) mh.focusScan('scan_serial');
}

function lookupItem() {
	var code = document.getElementById('scan_item').value.trim();
	if (!code) return;

	mh.showLoading('item-result');
	mh.post('scan_lookup', { scan: code }, function(resp) {
		if (resp.success && resp.matches && resp.matches.length > 0) {
			var match = resp.matches[0];
			currentItem = match;

			var html = '<div class="mobile-card-title">' + mh.escapeHtml(match.stock_id || match.serial_no || '') + '</div>';
			if (match.description) html += mh.cardRow('Description', match.description);
			if (match.status) html += mh.cardRow('Status', match.status);
			document.getElementById('item-card').innerHTML = html;

			mh.clearResult('item-result');

			// If scanned a serial directly, auto-populate
			if (match.type === 'serial') {
				document.getElementById('scan_serial').value = match.serial_no;
			}

			goStep(2);
		} else {
			mh.showResult('item-result', resp.error || 'No match found', 'error');
		}
	});
}

function confirmShip() {
	var serialNo = document.getElementById('scan_serial').value.trim();
	if (!serialNo) { mh.toast('Scan a serial number first', 'error'); return; }

	var stockId = currentItem ? (currentItem.stock_id || '') : '';
	mh.post('confirm_ship', {
		serial_no: serialNo,
		stock_id: stockId
	}, function(resp) {
		if (resp.success) {
			mh.showResult('final-result', resp.message, 'success');
			mh.toast(resp.message, 'success');
			goStep(3);
		} else {
			mh.showResult('ship-result', resp.error, 'error');
			mh.toast(resp.error, 'error');
		}
	});
}

function resetForm() {
	currentItem = null;
	document.getElementById('scan_item').value = '';
	document.getElementById('scan_serial').value = '';
	mh.clearResult('item-result');
	mh.clearResult('ship-result');
	mh.clearResult('final-result');
	goStep(1);
}

mh.initScanner(function(code) {
	if (document.getElementById('step1').style.display !== 'none') {
		document.getElementById('scan_item').value = code;
		lookupItem();
	} else if (document.getElementById('step2').style.display !== 'none') {
		document.getElementById('scan_serial').value = code;
		confirmShip();
	}
});

document.getElementById('scan_item').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); lookupItem(); }
});
document.getElementById('scan_serial').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); confirmShip(); }
});
</script>

<?php end_page(); ?>
