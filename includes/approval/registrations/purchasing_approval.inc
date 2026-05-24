<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Purchasing Module Registration & Executors

	Registers Purchase Order, Supplier Invoice, and GRN transaction
	types for the approval workflow. Provides executor callbacks that
	reconstruct purch_order objects and post approved drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');

// =====================================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_PURCHORDER, array(
	'label'          => _('Purchase Order'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_purchase_order',
	'amount_getter'  => 'get_purchase_draft_amount',
	'draft_renderer' => 'render_purchase_draft',
	'edit_url'       => 'purchasing/po_entry_items.php',
));

ApprovalWorkflow::registerTransactionType(ST_SUPPINVOICE, array(
	'label'          => _('Supplier Invoice'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_supplier_invoice',
	'amount_getter'  => 'get_purchase_draft_amount',
	'draft_renderer' => 'render_purchase_draft',
	'edit_url'       => 'purchasing/supplier_invoice.php',
));

ApprovalWorkflow::registerTransactionType(ST_PURCHREQUISITION, array(
	'label'          => _('Purchase Requisition'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_purchase_requisition',
	'rejector'       => 'reject_purchase_requisition_draft',
	'canceler'       => 'cancel_purchase_requisition_draft',
	'amount_getter'  => 'get_purchase_requisition_draft_amount',
	'draft_renderer' => 'render_purchase_requisition_draft',
	'edit_url'       => 'purchasing/purch_requisition_entry.php',
));

ApprovalWorkflow::registerTransactionType(ST_PURCHAGREEMENT, array(
	'label'          => _('Purchase Agreement'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_purchase_agreement',
	'rejector'       => 'reject_purchase_agreement_draft',
	'canceler'       => 'cancel_purchase_agreement_draft',
	'amount_getter'  => 'get_purchase_agreement_draft_amount',
	'draft_renderer' => 'render_purchase_agreement_draft',
	'edit_url'       => 'purchasing/purch_agreement_entry.php',
));

ApprovalWorkflow::registerTransactionType(ST_PROCUREMENTPLAN, array(
	'label'          => _('Procurement Plan'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_procurement_plan',
	'rejector'       => 'reject_procurement_plan_draft',
	'canceler'       => 'cancel_procurement_plan_draft',
	'amount_getter'  => 'get_procurement_plan_draft_amount',
	'draft_renderer' => 'render_procurement_plan_draft',
	'edit_url'       => 'purchasing/procurement_plan.php',
));

ApprovalWorkflow::registerTransactionType(ST_PURCHMATCHEXCEPTION, array(
	'label'          => _('Purchase Matching Exception'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_matching_exception',
	'rejector'       => 'reject_matching_exception_draft',
	'canceler'       => 'cancel_matching_exception_draft',
	'amount_getter'  => 'get_matching_exception_draft_amount',
	'draft_renderer' => 'render_matching_exception_draft',
	'edit_url'       => 'purchasing/inquiry/matching_exceptions.php',
));

ApprovalWorkflow::registerTransactionType(ST_VENDOR_EVALUATION, array(
	'label'          => _('Vendor Evaluation'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_vendor_evaluation',
	'rejector'       => 'reject_vendor_evaluation_draft',
	'canceler'       => 'cancel_vendor_evaluation_draft',
	'amount_getter'  => 'get_vendor_evaluation_draft_amount',
	'draft_renderer' => 'render_vendor_evaluation_draft',
	'edit_url'       => 'purchasing/manage/vendor_evaluation.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Restore a purch_order object from draft data array.
 *
 * Creates a new purch_order instance and populates it with saved
 * properties and line items from the JSON draft.
 *
 * @param array $data Draft data array (decoded JSON)
 * @return object Restored purch_order instance
 */
function restore_purchase_order_from_data($data)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/po_class.inc');

	$po = new purch_order();

	$po->trans_type       = (int)$data['trans_type'];
	$po->trans_no         = isset($data['trans_no']) ? $data['trans_no'] : 0;
	$po->supplier_id      = (int)$data['supplier_id'];
	$po->supplier_name    = isset($data['supplier_name']) ? $data['supplier_name'] : '';
	$po->curr_code        = isset($data['curr_code']) ? $data['curr_code'] : '';
	$po->supp_ref         = isset($data['supp_ref']) ? $data['supp_ref'] : '';
	$po->delivery_address = isset($data['delivery_address']) ? $data['delivery_address'] : '';
	$po->Comments         = isset($data['Comments']) ? $data['Comments'] : '';
	$po->Location         = isset($data['Location']) ? $data['Location'] : '';
	$po->agreement_id     = isset($data['agreement_id']) ? (int)$data['agreement_id'] : 0;
	$po->requisition_id   = isset($data['requisition_id']) ? (int)$data['requisition_id'] : 0;
	$po->rfq_id           = isset($data['rfq_id']) ? (int)$data['rfq_id'] : 0;
	$po->orig_order_date  = isset($data['orig_order_date']) ? $data['orig_order_date'] : '';
	$po->due_date         = isset($data['due_date']) ? $data['due_date'] : '';
	$po->order_no         = isset($data['order_no']) ? (int)$data['order_no'] : 0;
	$po->reference        = isset($data['reference']) ? $data['reference'] : '';
	$po->tax_group_id     = isset($data['tax_group_id']) ? (int)$data['tax_group_id'] : 0;
	$po->tax_included     = isset($data['tax_included']) ? (int)$data['tax_included'] : 0;
	$po->terms            = isset($data['terms']) ? $data['terms'] : array();
	$po->ex_rate          = isset($data['ex_rate']) ? $data['ex_rate'] : 1;
	$po->cash_account     = isset($data['cash_account']) ? $data['cash_account'] : '';
	$po->dimension        = isset($data['dimension']) ? (int)$data['dimension'] : 0;
	$po->dimension2       = isset($data['dimension2']) ? (int)$data['dimension2'] : 0;
	$po->tax_overrides    = isset($data['tax_overrides']) ? $data['tax_overrides'] : array();
	$po->prep_amount      = isset($data['prep_amount']) ? (float)$data['prep_amount'] : 0;
	$po->fixed_asset      = isset($data['fixed_asset']) ? $data['fixed_asset'] : false;

	// Restore line items
	if (!empty($data['line_items'])) {
		foreach ($data['line_items'] as $line_data) {
			$po->add_to_order(
				count($po->line_items),
				$line_data['stock_id'],
				$line_data['quantity'],
				isset($line_data['item_description']) ? $line_data['item_description'] : '',
				$line_data['price'],
				isset($line_data['units']) ? $line_data['units'] : '',
				isset($line_data['req_del_date']) ? $line_data['req_del_date'] : '',
				isset($line_data['qty_inv']) ? $line_data['qty_inv'] : 0,
				isset($line_data['qty_received']) ? $line_data['qty_received'] : 0
			);
		}
	}

	return $po;
}

/**
 * Restore a supp_trans object from draft data array.
 *
 * @param array $data Draft data array (decoded JSON)
 * @return object Restored supp_trans instance
 */
function restore_supplier_trans_from_data($data)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/supp_trans_class.inc');

	$supp_trans = new supp_trans(ST_SUPPINVOICE);
	$supp_trans->trans_type = isset($data['trans_type']) ? (int)$data['trans_type'] : ST_SUPPINVOICE;
	$supp_trans->trans_no = isset($data['trans_no']) ? (int)$data['trans_no'] : 0;
	$supp_trans->supplier_id = isset($data['supplier_id']) ? (int)$data['supplier_id'] : 0;
	$supp_trans->supplier_name = isset($data['supplier_name']) ? $data['supplier_name'] : '';
	$supp_trans->currency = isset($data['currency']) ? $data['currency'] : '';
	$supp_trans->tax_group_id = isset($data['tax_group_id']) ? (int)$data['tax_group_id'] : 0;
	$supp_trans->tax_included = isset($data['tax_included']) ? (int)$data['tax_included'] : 0;
	$supp_trans->tran_date = isset($data['tran_date']) ? $data['tran_date'] : '';
	$supp_trans->due_date = isset($data['due_date']) ? $data['due_date'] : '';
	$supp_trans->reference = isset($data['reference']) ? $data['reference'] : '';
	$supp_trans->supp_reference = isset($data['supp_reference']) ? $data['supp_reference'] : '';
	$supp_trans->Comments = isset($data['Comments']) ? $data['Comments'] : '';
	$supp_trans->ov_amount = isset($data['ov_amount']) ? (float)$data['ov_amount'] : 0;
	$supp_trans->ov_discount = isset($data['ov_discount']) ? (float)$data['ov_discount'] : 0;
	$supp_trans->ov_gst = isset($data['ov_gst']) ? (float)$data['ov_gst'] : 0;
	$supp_trans->dimension = isset($data['dimension']) ? (int)$data['dimension'] : 0;
	$supp_trans->dimension2 = isset($data['dimension2']) ? (int)$data['dimension2'] : 0;
	$supp_trans->ex_rate = isset($data['ex_rate']) ? (float)$data['ex_rate'] : 1;
	$supp_trans->tax_overrides = isset($data['tax_overrides']) && is_array($data['tax_overrides'])
		? $data['tax_overrides'] : array();

	$supp_trans->grn_items = array();
	if (!empty($data['grn_items']) && is_array($data['grn_items'])) {
		foreach ($data['grn_items'] as $row) {
			$supp_trans->add_grn_to_trans(
				isset($row['id']) ? $row['id'] : 0,
				isset($row['po_detail_item']) ? $row['po_detail_item'] : 0,
				isset($row['item_code']) ? $row['item_code'] : '',
				isset($row['item_description']) ? $row['item_description'] : '',
				isset($row['qty_recd']) ? $row['qty_recd'] : 0,
				isset($row['prev_quantity_inv']) ? $row['prev_quantity_inv'] : 0,
				isset($row['this_quantity_inv']) ? $row['this_quantity_inv'] : 0,
				isset($row['order_price']) ? $row['order_price'] : 0,
				isset($row['chg_price']) ? $row['chg_price'] : 0,
				isset($row['std_cost_unit']) ? $row['std_cost_unit'] : null,
				isset($row['gl_code']) ? $row['gl_code'] : ''
			);
		}
	}

	$supp_trans->gl_codes = array();
	if (!empty($data['gl_codes']) && is_array($data['gl_codes'])) {
		foreach ($data['gl_codes'] as $row) {
			$supp_trans->add_gl_codes_to_trans(
				isset($row['gl_code']) ? $row['gl_code'] : '',
				isset($row['gl_act_name']) ? $row['gl_act_name'] : '',
				isset($row['gl_dim']) ? $row['gl_dim'] : 0,
				isset($row['gl_dim2']) ? $row['gl_dim2'] : 0,
				isset($row['amount']) ? $row['amount'] : 0,
				isset($row['memo_']) ? $row['memo_'] : ''
			);
		}
	}

	return $supp_trans;
}

/**
 * Execute an approved Purchase Order draft.
 *
 * Reconstructs the purch_order from saved data and calls
 * add_direct_supp_trans() to post the order.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_purchase_order($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

	$po = restore_purchase_order_from_data($draft_data);
	$po->trans_type = ST_PURCHORDER;

	$order_no = add_direct_supp_trans($po);

	return $order_no ? $order_no : false;
}

/**
 * Execute an approved Supplier Invoice draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_supplier_invoice($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

	if (isset($draft_data['grn_items']) || isset($draft_data['gl_codes'])) {
		$supp_trans = restore_supplier_trans_from_data($draft_data);
		$invoice_no = add_supp_invoice($supp_trans, array('skip_matching' => true, 'from_approval' => true));
		return $invoice_no ? $invoice_no : false;
	}

	$po = restore_purchase_order_from_data($draft_data);
	$po->trans_type = ST_SUPPINVOICE;

	$order_no = add_direct_supp_trans($po);

	return $order_no ? $order_no : false;
}

/**
 * Execute an approved Purchase Requisition draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id Approval draft ID
 * @return int|false
 */
function execute_approved_purchase_requisition($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/purch_requisition_db.inc');

	$requisition_id = isset($draft_data['requisition_id']) ? (int)$draft_data['requisition_id'] : 0;
	if ($requisition_id <= 0)
		return false;

	return approve_requisition($requisition_id, 0) ? $requisition_id : false;
}

/**
 * Execute an approved Purchase Agreement draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id Approval draft ID
 * @return int|false
 */
function execute_approved_purchase_agreement($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/purch_agreement_db.inc');

	$agreement_id = isset($draft_data['agreement_id']) ? (int)$draft_data['agreement_id'] : 0;
	if ($agreement_id <= 0)
		return false;

	return confirm_agreement($agreement_id) ? $agreement_id : false;
}

/**
 * Execute an approved Procurement Plan draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id Approval draft ID
 * @return int|false
 */
function execute_approved_procurement_plan($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/procurement_plan_db.inc');

	$plan_id = isset($draft_data['plan_id']) ? (int)$draft_data['plan_id'] : 0;
	if ($plan_id <= 0)
		return false;

	return confirm_procurement_plan($plan_id) ? $plan_id : false;
}

/**
 * Execute an approved Purchase Matching Exception draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id Approval draft ID
 * @return int|false
 */
function execute_approved_matching_exception($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/purch_matching_db.inc');

	$exception_id = isset($draft_data['exception_id']) ? (int)$draft_data['exception_id'] : 0;
	$target_status = isset($draft_data['target_status']) ? $draft_data['target_status'] : 'approved';
	$requested_notes = isset($draft_data['requested_notes']) ? $draft_data['requested_notes'] : '';

	if ($exception_id <= 0)
		return false;

	return apply_matching_exception_decision($exception_id, $target_status, (int)$_SESSION['wa_current_user']->user, $requested_notes)
		? $exception_id
		: false;
}

/**
 * Execute an approved Vendor Evaluation draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id Approval draft ID
 * @return int|false
 */
function execute_approved_vendor_evaluation($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/vendor_evaluation_db.inc');

	$evaluation_id = isset($draft_data['evaluation_id']) ? (int)$draft_data['evaluation_id'] : 0;
	if ($evaluation_id <= 0)
		return false;

	$approver_user_id = isset($_SESSION['wa_current_user']) ? (int)$_SESSION['wa_current_user']->user : 0;

	return approve_evaluation($evaluation_id, $approver_user_id) ? $evaluation_id : false;
}

/**
 * Sync a rejected requisition approval draft back to the requisition record.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_purchase_requisition_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/purch_requisition_db.inc');

	$requisition_id = isset($draft_data['requisition_id']) ? (int)$draft_data['requisition_id'] : 0;
	if ($requisition_id <= 0)
		return false;

	return reject_requisition($requisition_id, 0, $comments);
}

/**
 * Sync a cancelled requisition approval draft back to draft status.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_purchase_requisition_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/purch_requisition_db.inc');

	$requisition_id = isset($draft_data['requisition_id']) ? (int)$draft_data['requisition_id'] : 0;
	if ($requisition_id <= 0)
		return false;

	return reopen_purch_requisition($requisition_id);
}

/**
 * Record agreement draft rejection metadata.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_purchase_agreement_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/purch_agreement_db.inc');

	$agreement_id = isset($draft_data['agreement_id']) ? (int)$draft_data['agreement_id'] : 0;
	if ($agreement_id <= 0)
		return false;

	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement)
		return false;

	$custom_data = decode_purch_agreement_custom_data($agreement['custom_data']);
	$custom_data['approval_rejection_reason'] = $comments;
	$custom_data['approval_rejected_draft_id'] = (int)$draft_id;

	$sql = "UPDATE " . TB_PREF . "purch_agreements SET
		custom_data = " . sql_purch_agreement_custom_data($custom_data) . "
		WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not record purchase agreement rejection metadata');

	return true;
}

/**
 * Record agreement draft cancellation metadata.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_purchase_agreement_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/purch_agreement_db.inc');

	$agreement_id = isset($draft_data['agreement_id']) ? (int)$draft_data['agreement_id'] : 0;
	if ($agreement_id <= 0)
		return false;

	$agreement = get_purch_agreement($agreement_id);
	if (!$agreement)
		return false;

	$custom_data = decode_purch_agreement_custom_data($agreement['custom_data']);
	$custom_data['approval_cancel_reason'] = $comments;
	$custom_data['approval_cancelled_draft_id'] = (int)$draft_id;

	$sql = "UPDATE " . TB_PREF . "purch_agreements SET
		custom_data = " . sql_purch_agreement_custom_data($custom_data) . "
		WHERE id = " . (int)$agreement_id;
	db_query($sql, 'could not record purchase agreement cancellation metadata');

	return true;
}

/**
 * Sync a rejected procurement plan draft back to draft status.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_procurement_plan_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/procurement_plan_db.inc');

	$plan_id = isset($draft_data['plan_id']) ? (int)$draft_data['plan_id'] : 0;
	if ($plan_id <= 0)
		return false;

	return reopen_procurement_plan($plan_id);
}

/**
 * Sync a cancelled procurement plan draft back to draft status.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_procurement_plan_draft($draft_data, $draft_id, $comments)
{
	return reject_procurement_plan_draft($draft_data, $draft_id, $comments);
}

/**
 * Handle matching exception draft rejection by keeping business record open.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_matching_exception_draft($draft_data, $draft_id, $comments)
{
	return true;
}

/**
 * Handle matching exception draft cancellation by keeping business record open.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_matching_exception_draft($draft_data, $draft_id, $comments)
{
	return true;
}

/**
 * Reopen vendor evaluation on approval rejection.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_vendor_evaluation_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/db/vendor_evaluation_db.inc');

	$evaluation_id = isset($draft_data['evaluation_id']) ? (int)$draft_data['evaluation_id'] : 0;
	if ($evaluation_id <= 0)
		return false;

	return reopen_vendor_evaluation($evaluation_id);
}

/**
 * Reopen vendor evaluation on approval cancellation.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_vendor_evaluation_draft($draft_data, $draft_id, $comments)
{
	return reject_vendor_evaluation_draft($draft_data, $draft_id, $comments);
}

// =====================================================
// AMOUNT GETTERS
// =====================================================

/**
 * Calculate the total amount of a purchase draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total amount (sum of qty * price)
 */
function get_purchase_draft_amount($draft_data)
{
	$total = 0;
	if (!empty($draft_data['line_items'])) {
		foreach ($draft_data['line_items'] as $line) {
			$price = isset($line['price']) ? (float)$line['price'] : 0;
			$qty   = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$total += round($qty * $price, 2);
		}
	}
	return $total;
}

/**
 * Calculate the total estimated amount of a purchase requisition draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float
 */
function get_purchase_requisition_draft_amount($draft_data)
{
	return isset($draft_data['total_estimated']) ? (float)$draft_data['total_estimated'] : 0;
}

/**
 * Calculate the committed total amount of a purchase agreement draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float
 */
function get_purchase_agreement_draft_amount($draft_data)
{
	if (isset($draft_data['total_committed']))
		return (float)$draft_data['total_committed'];

	$total = 0;
	if (!empty($draft_data['line_items'])) {
		foreach ($draft_data['line_items'] as $line) {
			$quantity = isset($line['committed_qty']) ? (float)$line['committed_qty'] : 0;
			$price = isset($line['unit_price']) ? (float)$line['unit_price'] : 0;
			$discount = isset($line['discount_percent']) ? (float)$line['discount_percent'] : 0;
			$total += $quantity * ($price * (1 - ($discount / 100)));
		}
	}

	return $total;
}

/**
 * Calculate the total estimated amount of a procurement plan draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float
 */
function get_procurement_plan_draft_amount($draft_data)
{
	return isset($draft_data['estimated_total']) ? (float)$draft_data['estimated_total'] : 0;
}

/**
 * Calculate approval amount for one matching exception draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float
 */
function get_matching_exception_draft_amount($draft_data)
{
	if (isset($draft_data['variance_amount']))
		return abs((float)$draft_data['variance_amount']);

	if (isset($draft_data['actual_value']) && isset($draft_data['expected_value']))
		return abs((float)$draft_data['actual_value'] - (float)$draft_data['expected_value']);

	return 0;
}

/**
 * Calculate amount for vendor evaluation draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float
 */
function get_vendor_evaluation_draft_amount($draft_data)
{
	if (isset($draft_data['overall_score']))
		return (float)$draft_data['overall_score'];

	if (isset($draft_data['amount']))
		return (float)$draft_data['amount'];

	return 0;
}

// =====================================================
// DRAFT RENDERERS
// =====================================================

/**
 * Render a purchase transaction draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_purchase_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Supplier') . '</th><td>' . htmlspecialchars(isset($draft_data['supplier_name']) ? $draft_data['supplier_name'] : $draft_data['supplier_id']) . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars(isset($draft_data['orig_order_date']) ? $draft_data['orig_order_date'] : '') . '</td>';
	$html .= '<th>' . _('Due Date') . '</th><td>' . htmlspecialchars(isset($draft_data['due_date']) ? $draft_data['due_date'] : '') . '</td></tr>';

	if (!empty($draft_data['supp_ref']))
		$html .= '<tr><th>' . _('Supplier Ref') . '</th><td colspan="3">' . htmlspecialchars($draft_data['supp_ref']) . '</td></tr>';

	if (!empty($draft_data['Comments']))
		$html .= '<tr><th>' . _('Comments') . '</th><td colspan="3">' . htmlspecialchars($draft_data['Comments']) . '</td></tr>';

	$html .= '</table>';

	// Line items
	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Price') . '</th><th>' . _('Total') . '</th></tr>';

		$grand_total = 0;
		foreach ($draft_data['line_items'] as $line) {
			$price     = isset($line['price']) ? (float)$line['price'] : 0;
			$qty       = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$line_total = round($qty * $price, 2);
			$grand_total += $line_total;

			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['item_description']) ? $line['item_description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($qty, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="4"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}

/**
 * Render a purchase requisition draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string
 */
function render_purchase_requisition_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td>';
	$html .= '<th>' . _('Requester') . '</th><td>' . htmlspecialchars(isset($draft_data['requester_name']) ? $draft_data['requester_name'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Department') . '</th><td>' . htmlspecialchars(isset($draft_data['department_name']) ? $draft_data['department_name'] : '') . '</td>';
	$html .= '<th>' . _('Location') . '</th><td>' . htmlspecialchars(isset($draft_data['location_name']) ? $draft_data['location_name'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Request Date') . '</th><td>' . htmlspecialchars(isset($draft_data['request_date']) ? $draft_data['request_date'] : '') . '</td>';
	$html .= '<th>' . _('Required Date') . '</th><td>' . htmlspecialchars(isset($draft_data['required_date']) ? $draft_data['required_date'] : '') . '</td></tr>';

	if (!empty($draft_data['purpose']))
		$html .= '<tr><th>' . _('Purpose') . '</th><td colspan="3">' . htmlspecialchars($draft_data['purpose']) . '</td></tr>';

	if (!empty($draft_data['notes']))
		$html .= '<tr><th>' . _('Notes') . '</th><td colspan="3">' . htmlspecialchars($draft_data['notes']) . '</td></tr>';

	$html .= '<tr><th>' . _('Estimated Total') . '</th><td colspan="3"><b>' . number_format(get_purchase_requisition_draft_amount($draft_data), 2) . '</b></td></tr>';
	$html .= '</table>';

	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th><th>' . _('Qty') . '</th>';
		$html .= '<th>' . _('Estimated Price') . '</th><th>' . _('Preferred Supplier') . '</th><th>' . _('Line Total') . '</th></tr>';

		foreach ($draft_data['line_items'] as $line) {
			$quantity = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$price = isset($line['estimated_unit_price']) ? (float)$line['estimated_unit_price'] : 0;
			$line_total = round($quantity * $price, 2);

			$html .= '<tr><td>' . htmlspecialchars(isset($line['stock_id']) ? $line['stock_id'] : '') . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['description']) ? $line['description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($quantity, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['preferred_supplier_name']) ? $line['preferred_supplier_name'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		$html .= '</table>';
	}

	return $html;
}

/**
 * Render a purchase agreement draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string
 */
function render_purchase_agreement_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td>';
	$html .= '<th>' . _('Supplier') . '</th><td>' . htmlspecialchars(isset($draft_data['supplier_name']) ? $draft_data['supplier_name'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Agreement Type') . '</th><td>' . htmlspecialchars(isset($draft_data['agreement_type']) ? $draft_data['agreement_type'] : '') . '</td>';
	$html .= '<th>' . _('Buyer') . '</th><td>' . htmlspecialchars(isset($draft_data['buyer_name']) ? $draft_data['buyer_name'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Start Date') . '</th><td>' . htmlspecialchars(isset($draft_data['date_start']) ? $draft_data['date_start'] : '') . '</td>';
	$html .= '<th>' . _('End Date') . '</th><td>' . htmlspecialchars(isset($draft_data['date_end']) ? $draft_data['date_end'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Delivery Location') . '</th><td>' . htmlspecialchars(isset($draft_data['delivery_location_name']) ? $draft_data['delivery_location_name'] : '') . '</td>';
	$html .= '<th>' . _('Committed Total') . '</th><td><b>' . number_format(get_purchase_agreement_draft_amount($draft_data), 2) . '</b></td></tr>';

	if (!empty($draft_data['notes']))
		$html .= '<tr><th>' . _('Notes') . '</th><td colspan="3">' . htmlspecialchars($draft_data['notes']) . '</td></tr>';

	$html .= '</table>';

	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th><th>' . _('Committed Qty') . '</th>';
		$html .= '<th>' . _('Unit Price') . '</th><th>' . _('Discount %') . '</th><th>' . _('Line Total') . '</th></tr>';

		foreach ($draft_data['line_items'] as $line) {
			$quantity = isset($line['committed_qty']) ? (float)$line['committed_qty'] : 0;
			$price = isset($line['unit_price']) ? (float)$line['unit_price'] : 0;
			$discount = isset($line['discount_percent']) ? (float)$line['discount_percent'] : 0;
			$line_total = round($quantity * ($price * (1 - ($discount / 100))), 2);

			$html .= '<tr><td>' . htmlspecialchars(isset($line['stock_id']) ? $line['stock_id'] : '') . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['description']) ? $line['description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($quantity, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td align="right">' . number_format($discount, 2) . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		$html .= '</table>';
	}

	return $html;
}

/**
 * Render a procurement plan draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string
 */
function render_procurement_plan_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td>';
	$html .= '<th>' . _('Plan Date') . '</th><td>' . htmlspecialchars(isset($draft_data['plan_date']) ? $draft_data['plan_date'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Plan Type') . '</th><td>' . htmlspecialchars(isset($draft_data['plan_type']) ? ucwords(str_replace('_', ' ', $draft_data['plan_type'])) : '') . '</td>';
	$html .= '<th>' . _('Estimated Total') . '</th><td><b>' . number_format(get_procurement_plan_draft_amount($draft_data), 2) . '</b></td></tr>';
	$html .= '<tr><th>' . _('Line Count') . '</th><td>' . (isset($draft_data['line_count']) ? (int)$draft_data['line_count'] : 0) . '</td>';
	$html .= '<th>' . _('Pending/Approved/Ordered/Skipped') . '</th><td>'
		. (isset($draft_data['pending_count']) ? (int)$draft_data['pending_count'] : 0) . ' / '
		. (isset($draft_data['approved_count']) ? (int)$draft_data['approved_count'] : 0) . ' / '
		. (isset($draft_data['ordered_count']) ? (int)$draft_data['ordered_count'] : 0) . ' / '
		. (isset($draft_data['skipped_count']) ? (int)$draft_data['skipped_count'] : 0)
		. '</td></tr>';
	$html .= '</table>';

	if (!empty($draft_data['lines'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item') . '</th><th>' . _('Location') . '</th><th>' . _('Qty') . '</th>';
		$html .= '<th>' . _('Supplier') . '</th><th>' . _('Estimated Price') . '</th><th>' . _('Status') . '</th></tr>';

		foreach ($draft_data['lines'] as $line) {
			$html .= '<tr><td>' . htmlspecialchars(isset($line['stock_id']) ? $line['stock_id'] : '') . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['location_name']) && $line['location_name'] !== '' ? $line['location_name'] : (isset($line['location']) ? $line['location'] : '')) . '</td>';
			$html .= '<td align="right">' . number_format(isset($line['suggested_order_qty']) ? (float)$line['suggested_order_qty'] : 0, 2) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['supplier_name']) ? $line['supplier_name'] : '') . '</td>';
			$html .= '<td align="right">' . number_format(isset($line['estimated_price']) ? (float)$line['estimated_price'] : 0, 2) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['status_label']) ? $line['status_label'] : '') . '</td></tr>';
		}

		$html .= '</table>';
	}

	return $html;
}

/**
 * Render a matching exception draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string
 */
function render_matching_exception_draft($draft_data)
{
	$target_status = isset($draft_data['target_status']) ? ucfirst($draft_data['target_status']) : _('Approve');
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Exception ID') . '</th><td>#' . (isset($draft_data['exception_id']) ? (int)$draft_data['exception_id'] : 0) . '</td>';
	$html .= '<th>' . _('Requested Action') . '</th><td><b>' . htmlspecialchars($target_status) . '</b></td></tr>';
	$html .= '<tr><th>' . _('Type') . '</th><td>' . htmlspecialchars(isset($draft_data['exception_type']) ? $draft_data['exception_type'] : '') . '</td>';
	$html .= '<th>' . _('Supplier') . '</th><td>' . htmlspecialchars(isset($draft_data['supplier_name']) ? $draft_data['supplier_name'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Document') . '</th><td>' . htmlspecialchars((isset($draft_data['trans_no']) ? (int)$draft_data['trans_no'] : 0) > 0 ? ('#' . (int)$draft_data['trans_no']) : '') . '</td>';
	$html .= '<th>' . _('Item') . '</th><td>' . htmlspecialchars(isset($draft_data['stock_id']) ? $draft_data['stock_id'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Expected') . '</th><td>' . number_format(isset($draft_data['expected_value']) ? (float)$draft_data['expected_value'] : 0, 2) . '</td>';
	$html .= '<th>' . _('Actual') . '</th><td>' . number_format(isset($draft_data['actual_value']) ? (float)$draft_data['actual_value'] : 0, 2) . '</td></tr>';
	$html .= '<tr><th>' . _('Variance') . '</th><td>' . number_format(get_matching_exception_draft_amount($draft_data), 2) . '</td>';
	$html .= '<th>' . _('Variance %') . '</th><td>' . number_format(isset($draft_data['variance_percent']) ? (float)$draft_data['variance_percent'] : 0, 2) . '%</td></tr>';

	if (!empty($draft_data['requested_notes']))
		$html .= '<tr><th>' . _('Notes') . '</th><td colspan="3">' . htmlspecialchars($draft_data['requested_notes']) . '</td></tr>';

	$html .= '</table>';
	return $html;
}

/**
 * Render vendor evaluation draft summary.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_vendor_evaluation_draft($draft_data)
{
	$html = '<table class="tablestyle2">';
	$html .= '<tr><td class="label">' . _('Evaluation ID') . ':</td><td>'
		. (int)(isset($draft_data['evaluation_id']) ? $draft_data['evaluation_id'] : 0)
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Supplier') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['supplier_name']) ? $draft_data['supplier_name'] : (isset($draft_data['supplier_id']) ? $draft_data['supplier_id'] : ''))
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Evaluator') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['evaluator_name']) ? $draft_data['evaluator_name'] : (isset($draft_data['evaluator_id']) ? $draft_data['evaluator_id'] : ''))
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Evaluation Date') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['evaluation_date']) ? $draft_data['evaluation_date'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Period') . ':</td><td>'
		. htmlspecialchars(isset($draft_data['period_from']) ? $draft_data['period_from'] : '')
		. ' - '
		. htmlspecialchars(isset($draft_data['period_to']) ? $draft_data['period_to'] : '')
		. '</td></tr>';
	$html .= '<tr><td class="label">' . _('Overall Score') . ':</td><td>'
		. number_format((float)(isset($draft_data['overall_score']) ? $draft_data['overall_score'] : 0), 2)
		. '</td></tr>';

	if (!empty($draft_data['recommendation'])) {
		$html .= '<tr><td class="label">' . _('Recommendation') . ':</td><td>'
			. htmlspecialchars($draft_data['recommendation'])
			. '</td></tr>';
	}

	$html .= '</table>';

	return $html;
}
