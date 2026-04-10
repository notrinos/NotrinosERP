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
 * Mobile Pick List — FEFO-guided picking workflow.
 *
 * Shows pending pick operations with guided bin-by-bin walking
 * path. Supports batch/serial selection with FEFO priority.
 *
 * Steps: 1. Browse picks → 2. Walk to bin → 3. Confirm pick → loop
 */

$page_security = 'SA_WAREHOUSE_PICKING';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Mobile Pick List');

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

<div class="mobile-page" id="mobile-pick">

	<div class="mobile-header">
		<a href="<?php echo $path_to_root; ?>/inventory/mobile/" class="mobile-back">&#8592; Back</a>
		<h2><?php echo _('Pick List'); ?></h2>
	</div>

	<div class="mobile-steps">
		<div class="mobile-step active" id="step-ind-1">
			<span class="mobile-step-num">1</span> <?php echo _('Select'); ?>
		</div>
		<div class="mobile-step" id="step-ind-2">
			<span class="mobile-step-num">2</span> <?php echo _('Pick'); ?>
		</div>
		<div class="mobile-step" id="step-ind-3">
			<span class="mobile-step-num">3</span> <?php echo _('Done'); ?>
		</div>
	</div>

	<div class="mobile-field">
		<label><?php echo _('Warehouse'); ?></label>
		<select id="loc_code"><?php echo $loc_options; ?></select>
	</div>

	<!-- Step 1: Browse Pending Picks -->
	<div id="step1">
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="loadPicks()">
			<?php echo _('Load Pending Picks'); ?>
		</button>
		<div id="picks-list"></div>
	</div>

	<!-- Step 2: Pick Items -->
	<div id="step2" style="display:none;">
		<div class="mobile-card" id="pick-header"></div>

		<!-- Pick lines -->
		<div id="pick-lines"></div>

		<!-- Current pick line -->
		<div class="mobile-card" id="current-pick" style="display:none;">
			<div class="mobile-card-title"><?php echo _('Current Pick'); ?></div>
			<div id="current-pick-info"></div>

			<div class="mobile-scan-area" style="margin-top:8px;">
				<label><?php echo _('Scan Bin to Confirm'); ?></label>
				<input type="text" class="mobile-scan-input" id="pick_bin_confirm"
					placeholder="<?php echo _('Scan bin barcode...'); ?>" autocomplete="off">
			</div>

			<div class="mobile-field">
				<label><?php echo _('Picked Qty'); ?></label>
				<input type="number" id="pick_qty" value="0" min="0" step="any"
					style="font-size:20px; text-align:center;">
			</div>

			<div id="pick-serial-field" style="display:none;">
				<div class="mobile-field">
					<label><?php echo _('Serial #'); ?></label>
					<input type="text" id="pick_serial" placeholder="<?php echo _('Scan serial'); ?>">
				</div>
			</div>

			<div id="pick-batch-field" style="display:none;">
				<div class="mobile-field">
					<label><?php echo _('Batch #'); ?></label>
					<input type="text" id="pick_batch" placeholder="<?php echo _('Scan batch'); ?>">
				</div>
			</div>

			<button type="button" class="mobile-btn mobile-btn-success" onclick="confirmPick()">
				<?php echo _('Confirm Pick'); ?>
			</button>
			<div id="pick-result"></div>
		</div>

		<div class="mobile-btn-row" style="margin-top:12px;">
			<button type="button" class="mobile-btn mobile-btn-secondary" onclick="goStep(1)">
				<?php echo _('Back to List'); ?>
			</button>
		</div>
	</div>

	<!-- Step 3: Completed -->
	<div id="step3" style="display:none;">
		<div class="mobile-result mobile-result-success">
			<?php echo _('All items picked successfully!'); ?>
		</div>
		<button type="button" class="mobile-btn mobile-btn-primary" onclick="goStep(1)">
			<?php echo _('Pick Next Order'); ?>
		</button>
	</div>

</div>

<script>
var mh = new MobileHelper({ ajaxUrl: 'mobile_ajax.php' });
var currentOp = null;
var currentLineIdx = 0;
var pickLines = [];

function goStep(n) {
	for (var i = 1; i <= 3; i++)
		document.getElementById('step' + i).style.display = (i === n) ? '' : 'none';
	mh.setStep(n, 3);
}

function loadPicks() {
	var loc = document.getElementById('loc_code').value;
	mh.showLoading('picks-list');

	mh.post('get_pending_picks', { loc_code: loc }, function(resp) {
		if (resp.success && resp.operations && resp.operations.length > 0) {
			var html = '<ul class="mobile-list">';
			for (var i = 0; i < resp.operations.length; i++) {
				var op = resp.operations[i];
				var lineCount = (op.lines ? op.lines.length : 0);
				html += '<li class="mobile-list-item" data-op-idx="' + i + '" onclick="selectPick(' + i + ')">';
				html += '<div class="mobile-list-item-main">';
				html += '<div class="mobile-list-item-title">Pick #' + op.op_id + '</div>';
				html += '<div class="mobile-list-item-sub">Order #' + (op.source_doc_no || '') + ' — ' + lineCount + ' line(s)</div>';
				html += '</div>';
				html += '<div class="mobile-list-item-action">&#8250;</div>';
				html += '</li>';
			}
			html += '</ul>';
			document.getElementById('picks-list').innerHTML = html;

			// Store ops for selection
			window._pickOps = resp.operations;
		} else {
			document.getElementById('picks-list').innerHTML =
				'<div class="mobile-empty"><div class="mobile-empty-icon">&#128230;</div>' +
				'<div class="mobile-empty-text">No pending picks</div></div>';
		}
	});
}

function selectPick(idx) {
	if (!window._pickOps || !window._pickOps[idx]) return;
	currentOp = window._pickOps[idx];
	pickLines = currentOp.lines || [];
	currentLineIdx = 0;

	document.getElementById('pick-header').innerHTML =
		'<div class="mobile-card-title">Pick #' + currentOp.op_id + '</div>' +
		mh.cardRow('Order', '#' + (currentOp.source_doc_no || '')) +
		mh.cardRow('Lines', pickLines.length);

	showPickLines();
	goStep(2);
}

function showPickLines() {
	var html = '';
	for (var i = 0; i < pickLines.length; i++) {
		var line = pickLines[i];
		var status = (i < currentLineIdx) ? 'done' :
			(i === currentLineIdx) ? 'active' : '';
		var badge = (i < currentLineIdx) ? mh.badge('Picked', 'green') :
			(i === currentLineIdx) ? mh.badge('Next', 'blue') : '';

		html += '<div class="mobile-list-item" style="cursor:default;">';
		html += '<div class="mobile-list-item-main">';
		html += '<div class="mobile-list-item-title">' + mh.escapeHtml(line.stock_id) + ' ' + badge + '</div>';
		html += '<div class="mobile-list-item-sub">Qty: ' + line.qty +
			(line.from_bin_code ? ' — Bin: ' + mh.escapeHtml(line.from_bin_code) : '') + '</div>';
		html += '</div></div>';
	}
	document.getElementById('pick-lines').innerHTML = html;

	if (currentLineIdx < pickLines.length) {
		var cl = pickLines[currentLineIdx];
		document.getElementById('current-pick').style.display = '';
		document.getElementById('current-pick-info').innerHTML =
			mh.cardRow('Item', cl.stock_id) +
			mh.cardRow('Qty', cl.qty) +
			mh.cardRow('From Bin', cl.from_bin_code || 'Any');
		document.getElementById('pick_qty').value = cl.qty;
		mh.focusScan('pick_bin_confirm');
	} else {
		document.getElementById('current-pick').style.display = 'none';
		goStep(3);
	}
}

function confirmPick() {
	if (currentLineIdx >= pickLines.length) return;
	var line = pickLines[currentLineIdx];

	var binConfirm = document.getElementById('pick_bin_confirm').value.trim();

	// Resolve bin code to ID
	mh.showLoading('pick-result');
	mh.post('scan_lookup', { scan: binConfirm || (line.from_bin_code || '') }, function(resp) {
		var binId = 0;
		if (resp.success && resp.matches && resp.matches.length > 0 && resp.matches[0].type === 'bin') {
			binId = resp.matches[0].loc_id;
		} else if (line.from_bin_id) {
			binId = line.from_bin_id;
		}

		if (binId <= 0) {
			mh.showResult('pick-result', 'Could not identify bin', 'error');
			return;
		}

		mh.post('confirm_pick', {
			stock_id: line.stock_id,
			qty: document.getElementById('pick_qty').value,
			bin_loc_id: binId,
			serial_no: document.getElementById('pick_serial').value.trim(),
			batch_no: document.getElementById('pick_batch').value.trim(),
			op_id: (currentLineIdx === pickLines.length - 1) ? currentOp.op_id : 0
		}, function(pickResp) {
			if (pickResp.success) {
				mh.toast(pickResp.message, 'success');
				currentLineIdx++;
				// Reset fields
				document.getElementById('pick_bin_confirm').value = '';
				document.getElementById('pick_qty').value = '';
				document.getElementById('pick_serial').value = '';
				document.getElementById('pick_batch').value = '';
				mh.clearResult('pick-result');
				showPickLines();
			} else {
				mh.showResult('pick-result', pickResp.error, 'error');
				mh.toast(pickResp.error, 'error');
			}
		});
	});
}

mh.initScanner(function(code) {
	if (document.getElementById('step2').style.display !== 'none') {
		document.getElementById('pick_bin_confirm').value = code;
		confirmPick();
	}
});

document.getElementById('pick_bin_confirm').addEventListener('keypress', function(e) {
	if (e.key === 'Enter') { e.preventDefault(); confirmPick(); }
});
</script>

<?php end_page(); ?>
