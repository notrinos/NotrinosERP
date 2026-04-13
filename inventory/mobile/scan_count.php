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
 * Mobile Scan Cycle Count — Scan-based cycle counting workflow.
 *
 * Steps: 1. Select count session → 2. Scan Bin → 3. Scan Item → 4. Enter Qty → loop
 */

$page_security = 'SA_WAREHOUSE_CYCLE_COUNT';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Mobile Count');

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

<div class="mobile-page" id="mobile-count">

	<div class="mobile-header">
		<a href="<?php echo $path_to_root; ?>/inventory/mobile/" class="mobile-back">&#8592; Back</a>
		<h2><?php echo _('Cycle Count'); ?></h2>
	</div>

	<div class="mobile-steps">
		<div class="mobile-step active" id="step-ind-1">
			<span class="mobile-step-num">1</span> <?php echo _('Session'); ?>
		</div>
		<div class="mobile-step" id="step-ind-2">
			<span class="mobile-step-num">2</span> <?php echo _('Bin'); ?>
		</div>
		<div class="mobile-step" id="step-ind-3">
			<span class="mobile-step-num">3</span> <?php echo _('Count'); ?>
		</div>
	</div>

	<div class="mobile-field">
		<label><?php echo _('Warehouse'); ?></label>
		<select id="loc_code"><?php echo $loc_options; ?></select>
	</div>

	<!-- Step 1: Select Count Session -->
	<div id="step1">
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="loadSessions()">
			<?php echo _('Load Active Count Sessions'); ?>
		</button>
		<div id="sessions-list" class="mobile-loading" style="display:none;"></div>
		<div id="session-result"></div>
	</div>

	<!-- Step 2: Scan Bin -->
	<div id="step2" style="display:none;">
		<div class="mobile-card" id="session-card"></div>

		<div class="mobile-scan-area">
			<label><?php echo _('Scan Bin to Count'); ?></label>
			<input type="text" class="mobile-scan-input" id="scan_bin"
				placeholder="<?php echo _('Bin code...'); ?>" autocomplete="off">
		</div>

		<button type="button" class="mobile-btn mobile-btn-primary" onclick="lookupBin()">
			<?php echo _('Go to Bin'); ?>
		</button>

		<div id="bin-result"></div>

		<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(1)" style="margin-top:8px;">
			<?php echo _('Change Session'); ?>
		</button>
	</div>

	<!-- Step 3: Count Items in Bin -->
	<div id="step3" style="display:none;">
		<div class="mobile-card" id="bin-card"></div>

		<!-- Current bin contents from system -->
		<div class="mobile-card" id="bin-contents-card" style="display:none;">
			<div class="mobile-card-title"><?php echo _('System Stock in Bin'); ?></div>
			<div id="bin-contents"></div>
		</div>

		<div class="mobile-scan-area">
			<label><?php echo _('Scan / Enter Item'); ?></label>
			<input type="text" class="mobile-scan-input" id="count_item"
				placeholder="<?php echo _('Item or barcode...'); ?>" autocomplete="off">
		</div>

		<div id="tracking-fields" style="display:none;">
			<div class="mobile-field">
				<label><?php echo _('Serial #'); ?></label>
				<input type="text" id="count_serial" placeholder="<?php echo _('Optional'); ?>">
			</div>
			<div class="mobile-field">
				<label><?php echo _('Batch #'); ?></label>
				<input type="text" id="count_batch" placeholder="<?php echo _('Optional'); ?>">
			</div>
		</div>

		<div class="mobile-field">
			<label><?php echo _('Counted Quantity'); ?></label>
			<input type="number" id="count_qty" value="0" min="0" step="any"
				style="font-size:24px; text-align:center; font-weight:bold;">
		</div>

		<button type="button" class="mobile-btn mobile-btn-success" onclick="submitCount()">
			<?php echo _('Submit Count'); ?>
		</button>

		<div id="count-result"></div>

		<!-- Count history for current bin -->
		<div class="mobile-card" id="count-history-card" style="display:none; margin-top:12px;">
			<div class="mobile-card-title"><?php echo _('Counted Items'); ?></div>
			<div id="count-history"></div>
		</div>

		<div class="mobile-btn-row" style="margin-top:12px;">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(2)">
				<?php echo _('Next Bin'); ?>
			</button>
		</div>
	</div>

</div>

<script>
var mh = new MobileHelper({ ajaxUrl: 'mobile_ajax.php' });
var currentCountId = 0;
var currentBinId = 0;
var countedItems = [];

function goStep(n) {
	for (var i = 1; i <= 3; i++)
		document.getElementById('step' + i).style.display = (i === n) ? '' : 'none';
	mh.setStep(n, 3);

	if (n === 2) {
		mh.focusScan('scan_bin');
		countedItems = [];
	}
	if (n === 3) mh.focusScan('count_item');
}

function loadSessions() {
	var loc = document.getElementById('loc_code').value;
	var container = document.getElementById('sessions-list');
	container.style.display = '';
	container.innerHTML = '<div class="mobile-loading"><div class="mobile-spinner"></div></div>';

	mh.post('get_pending_counts', { loc_code: loc }, function(resp) {
		if (resp.success && resp.counts && resp.counts.length > 0) {
			var html = '<ul class="mobile-list">';
			for (var i = 0; i < resp.counts.length; i++) {
				var c = resp.counts[i];
				html += '<li class="mobile-list-item" onclick="selectSession(' + c.count_id + ', \'' +
					mh.escapeHtml(c.count_no || '#' + c.count_id) + '\')">';
				html += '<div class="mobile-list-item-main">';
				html += '<div class="mobile-list-item-title">' + mh.escapeHtml(c.count_no || '#' + c.count_id) + '</div>';
				html += '<div class="mobile-list-item-sub">' + mh.escapeHtml(c.count_date || '') + ' — ' + mh.escapeHtml(c.status) + '</div>';
				html += '</div>';
				html += '<div class="mobile-list-item-action">&#8250;</div>';
				html += '</li>';
			}
			html += '</ul>';
			container.innerHTML = html;
		} else {
			container.innerHTML = '<div class="mobile-empty">' +
				'<div class="mobile-empty-icon">&#128203;</div>' +
				'<div class="mobile-empty-text">No active count sessions</div></div>';
		}
	});
}

function selectSession(id, label) {
	currentCountId = id;
	document.getElementById('session-card').innerHTML =
		'<div class="mobile-card-title">Count Session</div>' +
		mh.cardRow('Session', label) +
		mh.cardRow('ID', '#' + id);
	goStep(2);
}

function lookupBin() {
	var code = document.getElementById('scan_bin').value.trim();
	if (!code) return;

	mh.showLoading('bin-result');
	mh.post('scan_lookup', { scan: code }, function(resp) {
		if (resp.success && resp.matches && resp.matches.length > 0 && resp.matches[0].type === 'bin') {
			var bin = resp.matches[0];
			currentBinId = bin.loc_id;
			document.getElementById('bin-card').innerHTML =
				'<div class="mobile-card-title">Counting Bin</div>' +
				mh.cardRow('Code', bin.loc_code) +
				mh.cardRow('Name', bin.loc_name);
			mh.clearResult('bin-result');

			// Load bin contents from system
			loadBinContents(bin.loc_id);

			goStep(3);
		} else {
			mh.showResult('bin-result', 'Bin not found: ' + code, 'error');
		}
	});
}

function loadBinContents(binId) {
	mh.post('get_bin_contents', { bin_loc_id: binId }, function(resp) {
		if (resp.success && resp.items && resp.items.length > 0) {
			var html = '';
			for (var i = 0; i < resp.items.length; i++) {
				var item = resp.items[i];
				html += '<div class="mobile-card-row">';
				html += '<span class="mobile-card-label">' + mh.escapeHtml(item.stock_id);
				if (item.batch_no) html += ' [' + mh.escapeHtml(item.batch_no) + ']';
				if (item.serial_no) html += ' SN:' + mh.escapeHtml(item.serial_no);
				html += '</span>';
				html += '<span class="mobile-card-value">' + item.qty_on_hand + '</span>';
				html += '</div>';
			}
			document.getElementById('bin-contents').innerHTML = html;
			document.getElementById('bin-contents-card').style.display = '';
		} else {
			document.getElementById('bin-contents-card').style.display = 'none';
		}
	});
}

function submitCount() {
	var stockId = document.getElementById('count_item').value.trim();
	var qty = document.getElementById('count_qty').value;
	if (!stockId) { mh.toast('Enter item ID first', 'error'); return; }

	mh.post('count_line', {
		count_id: currentCountId,
		stock_id: stockId,
		bin_loc_id: currentBinId,
		counted_qty: qty,
		serial_no: document.getElementById('count_serial').value.trim(),
		batch_no: document.getElementById('count_batch').value.trim()
	}, function(resp) {
		if (resp.success) {
			mh.showResult('count-result', resp.message, 'success');
			mh.toast(resp.message, 'success');

			// Add to history
			var varClass = resp.variance > 0 ? 'mobile-variance-pos' :
				(resp.variance < 0 ? 'mobile-variance-neg' : 'mobile-variance-zero');
			countedItems.push({
				stock_id: stockId,
				counted: qty,
				system: resp.system_qty,
				variance: resp.variance
			});
			var histHtml = '';
			for (var i = countedItems.length - 1; i >= 0; i--) {
				var ci = countedItems[i];
				var vc = ci.variance > 0 ? 'mobile-variance-pos' :
					(ci.variance < 0 ? 'mobile-variance-neg' : 'mobile-variance-zero');
				histHtml += '<div class="mobile-card-row">';
				histHtml += '<span class="mobile-card-label">' + mh.escapeHtml(ci.stock_id) + '</span>';
				histHtml += '<span class="mobile-card-value">' +
					ci.counted + ' <span class="' + vc + '">(var: ' + (ci.variance > 0 ? '+' : '') + ci.variance + ')</span></span>';
				histHtml += '</div>';
			}
			document.getElementById('count-history').innerHTML = histHtml;
			document.getElementById('count-history-card').style.display = '';

			// Reset for next item
			document.getElementById('count_item').value = '';
			document.getElementById('count_qty').value = '0';
			document.getElementById('count_serial').value = '';
			document.getElementById('count_batch').value = '';
			mh.focusScan('count_item');
		} else {
			mh.showResult('count-result', resp.error, 'error');
			mh.toast(resp.error, 'error');
		}
	});
}

mh.initScanner(function(code) {
	if (document.getElementById('step2').style.display !== 'none') {
		document.getElementById('scan_bin').value = code;
		lookupBin();
	} else if (document.getElementById('step3').style.display !== 'none') {
		document.getElementById('count_item').value = code;
		// Auto-lookup item to show tracking fields
		mh.post('scan_lookup', { scan: code }, function(resp) {
			if (resp.success && resp.matches && resp.matches.length > 0) {
				var tb = resp.matches[0].track_by || 'none';
				document.getElementById('tracking-fields').style.display =
					(tb !== 'none') ? '' : 'none';
			}
		});
	}
});

document.getElementById('scan_bin').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); lookupBin(); }
});
document.getElementById('count_item').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); document.getElementById('count_qty').focus(); }
});
document.getElementById('count_qty').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); submitCount(); }
});
</script>

<?php end_page(); ?>
