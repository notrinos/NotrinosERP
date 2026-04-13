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
 * Mobile Serial Lookup — Quick serial number information page.
 *
 * Scan a serial barcode or enter serial number to view full details,
 * status, location, warranty info, and recent movement history.
 */

$page_security = 'SA_SERIALINQUIRY';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Serial Lookup');

$js = '';
add_js_file('barcode_scanner.js');
add_js_ufile($path_to_root . '/inventory/mobile/mobile_helper.js?v=' . filemtime(dirname(__FILE__) . '/mobile_helper.js'));

page($_SESSION['page_title'], false, false, '', $js, false,
	$path_to_root . '/inventory/mobile/mobile.css');

include_once($path_to_root . '/includes/ui.inc');
?>

<div class="mobile-page" id="mobile-serial-lookup">

	<div class="mobile-header">
		<a href="<?php echo $path_to_root; ?>/inventory/mobile/" class="mobile-back">&#8592; Back</a>
		<h2><?php echo _('Serial Lookup'); ?></h2>
	</div>

	<!-- Scan Area -->
	<div class="mobile-scan-area">
		<label><?php echo _('Scan or Enter Serial Number'); ?></label>
		<input type="text" class="mobile-scan-input" id="scan_serial"
			placeholder="<?php echo _('Serial # or barcode...'); ?>"
			autocomplete="off" autofocus>
		<div class="mobile-scan-hint"><?php echo _('Scan serial barcode or type serial number'); ?></div>
	</div>

	<button type="button" class="mobile-btn mobile-btn-primary" onclick="lookupSerial()">
		<?php echo _('Look Up'); ?>
	</button>

	<div id="lookup-result"></div>

	<!-- Serial Detail (hidden until lookup) -->
	<div id="serial-detail" style="display:none;">

		<div class="mobile-card" id="serial-info-card">
			<div class="mobile-card-title" id="serial-title"></div>
			<div id="serial-info"></div>
		</div>

		<div class="mobile-card" id="serial-warranty-card" style="display:none;">
			<div class="mobile-card-title"><?php echo _('Warranty'); ?></div>
			<div id="serial-warranty"></div>
		</div>

		<div class="mobile-card" id="serial-movements-card">
			<div class="mobile-card-title"><?php echo _('Recent Movements'); ?></div>
			<div id="serial-movements"></div>
		</div>

	</div>

</div>

<script>
var mh = new MobileHelper({ ajaxUrl: 'mobile_ajax.php' });

function lookupSerial() {
	var code = document.getElementById('scan_serial').value.trim();
	if (!code) return;

	mh.showLoading('lookup-result');
	document.getElementById('serial-detail').style.display = 'none';

	mh.post('serial_lookup', { scan: code }, function(resp) {
		mh.clearResult('lookup-result');

		if (resp.success && resp.serial) {
			var s = resp.serial;
			document.getElementById('serial-detail').style.display = '';

			// Title with status badge
			document.getElementById('serial-title').innerHTML =
				mh.escapeHtml(s.serial_no) + ' ' + mh.badge(s.status, mh.statusColor(s.status));

			// Info card
			var html = '';
			html += mh.cardRow('Item', s.stock_id);
			if (s.item_description) html += mh.cardRow('Description', s.item_description);
			if (s.loc_code) html += mh.cardRow('Location', s.loc_code);
			if (s.location) html += mh.cardRow('Bin', s.location);
			if (s.supplier_id) html += mh.cardRow('Supplier', s.supplier_id);
			if (s.customer_id) html += mh.cardRow('Customer', s.customer_id);
			if (s.purchase_date) html += mh.cardRow('Purchase Date', s.purchase_date);
			if (s.manufacturing_date) html += mh.cardRow('Mfg Date', s.manufacturing_date);
			if (s.expiry_date) html += mh.cardRow('Expiry', s.expiry_date);
			if (s.purchase_cost && parseFloat(s.purchase_cost) > 0) html += mh.cardRow('Cost', s.purchase_cost);
			if (s.notes) html += mh.cardRow('Notes', s.notes);
			document.getElementById('serial-info').innerHTML = html;

			// Warranty
			if (s.warranty_start || s.warranty_end) {
				var wHtml = '';
				if (s.warranty_start) wHtml += mh.cardRow('Start', s.warranty_start);
				if (s.warranty_end) wHtml += mh.cardRow('End', s.warranty_end);
				document.getElementById('serial-warranty').innerHTML = wHtml;
				document.getElementById('serial-warranty-card').style.display = '';
			} else {
				document.getElementById('serial-warranty-card').style.display = 'none';
			}

			// Movements
			if (resp.movements && resp.movements.length > 0) {
				var mHtml = '';
				for (var i = 0; i < resp.movements.length; i++) {
					var m = resp.movements[i];
					mHtml += '<div class="mobile-card-row">';
					mHtml += '<span class="mobile-card-label">' +
						mh.escapeHtml(m.date || '') + ' — ' + mh.escapeHtml(m.type || '') + '</span>';
					mHtml += '<span class="mobile-card-value">';
					if (m.from_status && m.to_status && m.from_status !== m.to_status) {
						mHtml += mh.badge(m.from_status, mh.statusColor(m.from_status)) +
							' → ' + mh.badge(m.to_status, mh.statusColor(m.to_status));
					} else if (m.to_status) {
						mHtml += mh.badge(m.to_status, mh.statusColor(m.to_status));
					}
					mHtml += '</span>';
					mHtml += '</div>';
				}
				document.getElementById('serial-movements').innerHTML = mHtml;
			} else {
				document.getElementById('serial-movements').innerHTML =
					'<div class="mobile-empty"><div class="mobile-empty-text">No movements</div></div>';
			}
		} else {
			mh.showResult('lookup-result', resp.error || 'Serial not found', 'error');
		}
	});
}

mh.initScanner(function(code) {
	document.getElementById('scan_serial').value = code;
	lookupSerial();
});

document.getElementById('scan_serial').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); lookupSerial(); }
});
</script>

<?php end_page(); ?>
