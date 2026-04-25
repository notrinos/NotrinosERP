<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Manufacturing Module Registration & Executors

	Registers Work Order transaction type for the approval workflow.
	Provides executor callback that posts approved work order drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');

// =====================================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_WORKORDER, array(
	'label'          => _('Work Order'),
	'application'    => 'manuf',
	'executor'       => 'execute_approved_work_order',
	'amount_getter'  => 'get_work_order_draft_amount',
	'draft_renderer' => 'render_work_order_draft',
	'edit_url'       => 'manufacturing/work_order_entry.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Execute an approved Work Order draft.
 *
 * Calls add_work_order() with the saved form data to create the work order.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted work order ID or false on failure
 */
function execute_approved_work_order($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/manufacturing/includes/manufacturing_db.inc');

	$wo_ref      = $draft_data['wo_ref'];
	$loc_code    = $draft_data['location'];
	$units_reqd  = (float)$draft_data['quantity'];
	$stock_id    = $draft_data['stock_id'];
	$type        = (int)$draft_data['wo_type'];
	$date_       = $draft_data['date'];
	$required_by = isset($draft_data['required_by']) ? $draft_data['required_by'] : $draft_data['date'];
	$memo_       = isset($draft_data['memo_']) ? $draft_data['memo_'] : '';
	$costs       = isset($draft_data['costs']) ? (float)$draft_data['costs'] : 0;
	$cr_acc      = isset($draft_data['cr_acc']) ? $draft_data['cr_acc'] : '';
	$labour      = isset($draft_data['labour']) ? (float)$draft_data['labour'] : 0;
	$cr_lab_acc  = isset($draft_data['cr_lab_acc']) ? $draft_data['cr_lab_acc'] : '';

	$woid = add_work_order(
		$wo_ref, $loc_code, $units_reqd, $stock_id,
		$type, $date_, $required_by, $memo_,
		$costs, $cr_acc, $labour, $cr_lab_acc
	);

	return $woid;
}

// =====================================================
// AMOUNT GETTERS
// =====================================================

/**
 * Calculate the estimated cost of a work order draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Estimated total cost (quantity * standard_cost + overhead costs)
 */
function get_work_order_draft_amount($draft_data)
{
	$quantity = isset($draft_data['quantity']) ? (float)$draft_data['quantity'] : 0;
	$costs    = isset($draft_data['costs']) ? (float)$draft_data['costs'] : 0;
	$labour   = isset($draft_data['labour']) ? (float)$draft_data['labour'] : 0;

	return $costs + $labour;
}

// =====================================================
// DRAFT RENDERERS
// =====================================================

/**
 * Render a work order draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_work_order_draft($draft_data)
{
	$wo_types = array(0 => _('Assemble'), 1 => _('Unassemble'), 2 => _('Advanced'));
	$wo_type_label = isset($wo_types[(int)$draft_data['wo_type']]) ? $wo_types[(int)$draft_data['wo_type']] : _('Unknown');

	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Reference') . '</th><td>' . htmlspecialchars($draft_data['wo_ref']) . '</td>';
	$html .= '<th>' . _('Type') . '</th><td>' . $wo_type_label . '</td></tr>';
	$html .= '<tr><th>' . _('Item') . '</th><td>' . htmlspecialchars($draft_data['stock_id']) . '</td>';
	$html .= '<th>' . _('Quantity') . '</th><td>' . number_format((float)$draft_data['quantity'], 2) . '</td></tr>';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars($draft_data['date']) . '</td>';
	$html .= '<th>' . _('Required By') . '</th><td>' . htmlspecialchars(isset($draft_data['required_by']) ? $draft_data['required_by'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Location') . '</th><td>' . htmlspecialchars($draft_data['location']) . '</td>';
	$html .= '<td colspan="2"></td></tr>';

	if (!empty($draft_data['costs']) || !empty($draft_data['labour'])) {
		$html .= '<tr><th>' . _('Additional Costs') . '</th><td>' . number_format((float)$draft_data['costs'], 2) . '</td>';
		$html .= '<th>' . _('Labour Cost') . '</th><td>' . number_format((float)$draft_data['labour'], 2) . '</td></tr>';
	}

	if (!empty($draft_data['memo_']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo_']) . '</td></tr>';

	$html .= '</table>';

	return $html;
}
