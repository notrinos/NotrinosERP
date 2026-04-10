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
 * Mobile Scanner Hub — landing page for all mobile warehouse operations.
 *
 * Displays large touch-friendly tiles linking to each mobile workflow.
 * Designed for handheld scanner devices and tablets.
 */

$page_security = 'SA_OPEN';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Mobile Scanner');

page($_SESSION['page_title'], false, true, '', '',  false,
	$path_to_root . '/inventory/mobile/mobile.css');
?>

<div class="mobile-page" id="mobile-hub">

	<div class="mobile-header" style="text-align:center; margin-bottom:16px;">
		<h2 style="margin:0;"><?php echo _('Warehouse Scanner'); ?></h2>
		<p style="color:#666; margin:4px 0 0;"><?php echo _('Select an operation'); ?></p>
	</div>

	<div class="mobile-hub-grid">

		<?php if (user_check_access('SA_WAREHOUSE_OPERATIONS')) { ?>
		<a href="scan_receive.php" class="mobile-hub-tile">
			<span class="mobile-hub-icon">&#128230;</span>
			<span class="mobile-hub-label"><?php echo _('Receive'); ?></span>
			<span class="mobile-hub-desc"><?php echo _('Scan inbound items into bins'); ?></span>
		</a>
		<?php } ?>

		<?php if (user_check_access('SA_DISPATCH_OPERATIONS')) { ?>
		<a href="scan_ship.php" class="mobile-hub-tile">
			<span class="mobile-hub-icon">&#128666;</span>
			<span class="mobile-hub-label"><?php echo _('Ship'); ?></span>
			<span class="mobile-hub-desc"><?php echo _('Scan outbound shipments'); ?></span>
		</a>
		<?php } ?>

		<?php if (user_check_access('SA_LOCATIONTRANSFER')) { ?>
		<a href="scan_transfer.php" class="mobile-hub-tile">
			<span class="mobile-hub-icon">&#8646;</span>
			<span class="mobile-hub-label"><?php echo _('Transfer'); ?></span>
			<span class="mobile-hub-desc"><?php echo _('Bin-to-bin transfers'); ?></span>
		</a>
		<?php } ?>

		<?php if (user_check_access('SA_WAREHOUSE_CYCLE_COUNT')) { ?>
		<a href="scan_count.php" class="mobile-hub-tile">
			<span class="mobile-hub-icon">&#128203;</span>
			<span class="mobile-hub-label"><?php echo _('Count'); ?></span>
			<span class="mobile-hub-desc"><?php echo _('Cycle count verification'); ?></span>
		</a>
		<?php } ?>

		<?php if (user_check_access('SA_SERIALINQUIRY')) { ?>
		<a href="serial_lookup.php" class="mobile-hub-tile">
			<span class="mobile-hub-icon">&#128269;</span>
			<span class="mobile-hub-label"><?php echo _('Serial Lookup'); ?></span>
			<span class="mobile-hub-desc"><?php echo _('Trace serial number history'); ?></span>
		</a>
		<?php } ?>

		<?php if (user_check_access('SA_WAREHOUSE_OPERATIONS')) { ?>
		<a href="bin_putaway.php" class="mobile-hub-tile">
			<span class="mobile-hub-icon">&#128451;</span>
			<span class="mobile-hub-label"><?php echo _('Putaway'); ?></span>
			<span class="mobile-hub-desc"><?php echo _('Put items into bins'); ?></span>
		</a>
		<?php } ?>

		<?php if (user_check_access('SA_WAREHOUSE_PICKING')) { ?>
		<a href="pick_list.php" class="mobile-hub-tile">
			<span class="mobile-hub-icon">&#127919;</span>
			<span class="mobile-hub-label"><?php echo _('Pick List'); ?></span>
			<span class="mobile-hub-desc"><?php echo _('FEFO-guided order picking'); ?></span>
		</a>
		<?php } ?>

	</div>

</div>

<style>
.mobile-hub-grid {
	display: grid;
	grid-template-columns: 1fr 1fr;
	gap: 12px;
	max-width: 600px;
	margin: 0 auto;
	padding: 0 8px;
}
.mobile-hub-tile {
	display: flex;
	flex-direction: column;
	align-items: center;
	justify-content: center;
	padding: 20px 12px;
	background: #fff;
	border: 1px solid #dee2e6;
	border-radius: 10px;
	text-decoration: none;
	color: #333;
	min-height: 130px;
	transition: box-shadow 0.15s, border-color 0.15s;
}
.mobile-hub-tile:active,
.mobile-hub-tile:hover {
	border-color: #0d6efd;
	box-shadow: 0 2px 10px rgba(13,110,253,0.15);
}
.mobile-hub-icon {
	font-size: 40px;
	margin-bottom: 8px;
	line-height: 1;
}
.mobile-hub-label {
	font-size: 16px;
	font-weight: 600;
	margin-bottom: 4px;
}
.mobile-hub-desc {
	font-size: 12px;
	color: #666;
	text-align: center;
}
@media (max-width:400px) {
	.mobile-hub-grid {
		grid-template-columns: 1fr;
	}
	.mobile-hub-tile {
		min-height: 90px;
		flex-direction: row;
		gap: 12px;
		justify-content: flex-start;
		padding: 16px;
	}
	.mobile-hub-icon {
		font-size: 32px;
		margin-bottom: 0;
	}
	.mobile-hub-label {
		text-align: left;
	}
}
</style>

<?php end_page(); ?>
