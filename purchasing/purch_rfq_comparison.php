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

$page_security = 'SA_PURCHRFQ';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Purchase RFQ Comparison'));

$rfq_id = (int)get_post('rfq_id', isset($_GET['rfq_id']) ? (int)$_GET['rfq_id'] : 0);

/**
 * Redirect back to the RFQ comparison page with the current RFQ id.
 *
 * @param int   $rfq_id
 * @param array $extra_params
 * @return void
 */
function redirect_to_purch_rfq_comparison($rfq_id, $extra_params = array())
{
	$params = array('rfq_id=' . (int)$rfq_id);
	foreach ((array)$extra_params as $param_name => $param_value)
		$params[] = rawurlencode($param_name) . '=' . rawurlencode((string)$param_value);

	meta_forward($_SERVER['PHP_SELF'], implode('&', $params));
}

if ($rfq_id <= 0) {
	display_error(_('Select a valid RFQ first.'));
	end_page();
	return;
}

if (isset($_GET['score_updated']))
	display_notification(_('Vendor evaluation has been updated.'));

if (isset($_GET['awarded']))
	display_notification(_('The RFQ has been awarded to the selected vendor.'));

if (isset($_GET['partial_award']))
	display_notification(_('The RFQ has been partially awarded by item.'));

if (isset($_GET['po_created']) && (int)$_GET['po_created'] > 0) {
	$po_number = (int)$_GET['po_created'];
	display_notification(_('Purchase order has been created from the awarded RFQ vendor.'));
	display_note(get_trans_view_str(ST_PURCHORDER, $po_number, _('View Purchase Order') . ' #' . $po_number), 0, 1);
}

$update_score_vendor_id = find_submit('UpdateVendorScore');
if ($update_score_vendor_id > 0) {
	if (evaluate_vendor_response(
		$update_score_vendor_id,
		input_num('vendor_score_' . $update_score_vendor_id),
		trim(get_post('vendor_score_notes_' . $update_score_vendor_id))
	)) {
		redirect_to_purch_rfq_comparison($rfq_id, array('score_updated' => 1));
	} else {
		display_error(_('The vendor evaluation could not be updated.'));
	}
}

$award_vendor_id = find_submit('AwardVendor');
if ($award_vendor_id > 0) {
	if (award_rfq($rfq_id, $award_vendor_id))
		redirect_to_purch_rfq_comparison($rfq_id, array('awarded' => 1));
	else
		display_error(_('The RFQ could not be awarded to the selected vendor.'));
}

if (isset($_POST['AwardSelectedLines'])) {
	$comparison_for_award = compare_rfq_responses($rfq_id);
	$line_awards = array();
	foreach ($comparison_for_award['items'] as $rfq_item_id => $comparison_item) {
		$selected_vendor = (int)get_post('award_vendor_item_' . $rfq_item_id);
		if ($selected_vendor > 0)
			$line_awards[$rfq_item_id] = $selected_vendor;
	}

	if (award_rfq($rfq_id, 0, $line_awards))
		redirect_to_purch_rfq_comparison($rfq_id, array('partial_award' => 1));
	else
		display_error(_('The selected line awards could not be saved.'));
}

$create_po_vendor_id = find_submit('CreatePOFromVendor');
if ($create_po_vendor_id > 0) {
	$po_number = create_po_from_rfq($rfq_id, $create_po_vendor_id);
	if ($po_number) {
		redirect_to_purch_rfq_comparison($rfq_id, array('po_created' => $po_number));
	} else {
		display_error(_('The purchase order could not be created from this RFQ vendor.'));
	}
}

$comparison = compare_rfq_responses($rfq_id);
$rfq = $comparison['rfq'];

if (!$rfq) {
	display_error(_('The selected RFQ could not be found.'));
	end_page();
	return;
}

start_form();
hidden('rfq_id', $rfq_id);

echo '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;">';
echo '<div>';
echo '<h2 style="margin:0;">' . htmlspecialchars($rfq['reference']) . '</h2>';
echo '<div style="margin-top:6px;">' . purch_rfq_status_badge($rfq['status']) . '</div>';
echo '</div>';
echo '<div>';
hyperlink_params($path_to_root . '/purchasing/purch_rfq_entry.php', _('Back to RFQ'), 'rfq_id=' . (int)$rfq_id);
echo '&nbsp;';
hyperlink_no_params('purchasing/inquiry/purch_rfq_view.php', _('Back to RFQ Inquiry'));
echo '</div>';
echo '</div>';

br();

start_table(TABLESTYLE2, "width='100%'");
label_row(_('Description:'), $rfq['description'] ? $rfq['description'] : '-');
label_row(_('RFQ Date:'), sql2date($rfq['created_date']));
label_row(_('Deadline Date:'), $rfq['deadline_date'] ? sql2date($rfq['deadline_date']) : '-');
label_row(_('Required Delivery Date:'), $rfq['required_delivery_date'] ? sql2date($rfq['required_delivery_date']) : '-');
label_row(_('Target Total:'), price_format($rfq['target_total']));
end_table(1);

display_heading(_('Vendor Summary'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Vendor'), _('Status'), _('Quoted Total'), _('Lead Days'), _('Score'), _('Evaluator Notes'), ''));

$k = 0;
foreach ($comparison['vendors'] as $rfq_vendor_id => $vendor) {
	alt_table_row_color($k);
	label_cell($vendor['supp_name']);
	label_cell(purch_rfq_vendor_status_badge($vendor['status']));
	amount_cell($vendor['total_quoted']);
	label_cell((int)$vendor['delivery_lead_days'] > 0 ? (int)$vendor['delivery_lead_days'] : '-');
	echo '<td><input type="text" name="vendor_score_' . $rfq_vendor_id . '" value="' . number_format((float)$vendor['evaluator_score'], 2) . '" size="6" class="amount"></td>';
	echo '<td><input type="text" name="vendor_score_notes_' . $rfq_vendor_id . '" value="' . htmlspecialchars($vendor['evaluator_notes'], ENT_QUOTES, 'UTF-8') . '" size="30"></td>';
	echo '<td nowrap>';
	submit('UpdateVendorScore' . $rfq_vendor_id, _('Save Score'), true, _('Save evaluation score'));
	echo '&nbsp;';
	if ($rfq['status'] !== 'awarded')
		submit('AwardVendor' . $rfq_vendor_id, _('Award All Lines'), true, _('Award all quoted items to this vendor'));
	if ((int)$vendor['is_winner'] === 1) {
		echo '&nbsp;';
		submit('CreatePOFromVendor' . $rfq_vendor_id, _('Create PO'), true, _('Create purchase order from this awarded vendor'));
	}
	echo '</td>';
	end_row();
}

if ($k == 0)
	label_row('', _('No vendor responses are available yet.'), 'colspan=7 align=center');

end_table(1);

if (!empty($comparison['items']) && !empty($comparison['vendors'])) {
	display_heading(_('Quote Comparison Matrix'));
	start_table(TABLESTYLE, "width='100%'");

	$header = array(_('Item'), _('Requested Qty'), _('Target Price'));
	foreach ($comparison['vendors'] as $vendor)
		$header[] = $vendor['supp_name'];
	table_header($header);

	$k = 0;
	foreach ($comparison['items'] as $rfq_item_id => $comparison_item) {
		alt_table_row_color($k);
		label_cell($comparison_item['stock_id'] . ' - ' . $comparison_item['line_description']);
		qty_cell($comparison_item['quantity']);
		amount_cell($comparison_item['target_price']);

		foreach ($comparison['vendors'] as $rfq_vendor_id => $vendor) {
			if (!isset($comparison_item['quotes'][$rfq_vendor_id])) {
				label_cell('-');
				continue;
			}

			$quote = $comparison_item['quotes'][$rfq_vendor_id];
			$style = in_array($rfq_vendor_id, $comparison_item['best_vendor_ids'])
				? "style='background:#e8f7e8;border-left:3px solid #28a745;'"
				: '';
			echo '<td ' . $style . '>';
			echo '<div><strong>' . price_format($quote['quoted_price']) . '</strong></div>';
			echo '<div style="font-size:11px;color:#666;">' . _('Qty: ') . number_format2($quote['quoted_quantity'], get_qty_dec($comparison_item['stock_id'])) . '</div>';
			if ((int)$quote['delivery_lead_days'] > 0)
				echo '<div style="font-size:11px;color:#666;">' . _('Lead: ') . (int)$quote['delivery_lead_days'] . ' ' . _('days') . '</div>';
			echo '</td>';
		}

		end_row();
	}

	end_table(1);

	if ($rfq['status'] !== 'awarded') {
		display_heading(_('Partial Award by Item'));
		start_table(TABLESTYLE2, "width='80%'");
		foreach ($comparison['items'] as $rfq_item_id => $comparison_item) {
			$selected_vendor_id = isset($rfq['award_map'][$rfq_item_id]) ? (int)$rfq['award_map'][$rfq_item_id] : 0;
			if ($selected_vendor_id <= 0 && !empty($comparison_item['best_vendor_ids']))
				$selected_vendor_id = (int)$comparison_item['best_vendor_ids'][0];

			echo '<tr><td class="label">' . htmlspecialchars($comparison_item['stock_id'] . ' - ' . $comparison_item['line_description']) . '</td><td>';
			echo '<select name="award_vendor_item_' . $rfq_item_id . '">';
			echo '<option value="0">' . _('Select vendor') . '</option>';
			foreach ($comparison['vendors'] as $rfq_vendor_id => $vendor) {
				if (!isset($comparison_item['quotes'][$rfq_vendor_id]))
					continue;

				echo '<option value="' . (int)$rfq_vendor_id . '"' . ($selected_vendor_id == $rfq_vendor_id ? ' selected' : '') . '>' . htmlspecialchars($vendor['supp_name']) . '</option>';
			}
			echo '</select>';
			echo '</td></tr>';
		}
		end_table(1);
		submit_center('AwardSelectedLines', _('Award Selected Lines'), true, '', 'default');
	}
}

end_form();
end_page();