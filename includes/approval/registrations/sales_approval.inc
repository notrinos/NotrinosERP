<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Sales Module Registration & Executors

	Registers Sales Invoice, Sales Order, and Customer Delivery transaction
	types for the approval workflow. Provides executor callbacks that
	reconstruct Cart objects and post approved drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');
include_once($path_to_root . '/sales/includes/db/sales_commission_db.inc');

/**
 * Ensure Sales Commission transaction type has numbering metadata.
 *
 * @return void
 */
function ensure_sales_commission_transaction_registration()
{
	static $is_registered = false;

	if ($is_registered || !function_exists('register_systype_db_info'))
		return;

	register_systype_db_info(ST_SALES_COMMISSION, array(
		TB_PREF . 'sales_commission_entries',
		null,
		'id',
		null,
		'trans_date'
	));

	$is_registered = true;
}

ensure_sales_commission_transaction_registration();

// =====================================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

require_once __DIR__.'/../../../sales/includes/db/sales_agreement_lifecycle_db.inc';
require_once __DIR__.'/../../../sales/includes/db/sales_agreement_order_db.inc';
require_once __DIR__.'/../../../sales/includes/db/sales_order_source_db.inc';

ApprovalWorkflow::registerTransactionType(ST_SALESAGREEMENT, array(
	'label'          => _('Sales Agreement'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_agreement',
	'rejector'       => 'reject_sales_agreement_draft',
	'canceler'       => 'cancel_sales_agreement_draft',
	'amount_getter'  => 'get_sales_agreement_draft_amount',
	'draft_renderer' => 'render_sales_agreement_draft',
	'edit_url'       => 'sales/sales_agreement_entry.php',
	'action_locker' => 'lock_sales_agreement_core_action',
	'action_validator' => 'validate_sales_agreement_core_action',
	'action_completion_validator' => 'verify_sales_agreement_core_completion',
));

ApprovalWorkflow::registerTransactionType(ST_SALESINVOICE, array(
	'label'          => _('Sales Invoice'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_invoice',
	'amount_getter'  => 'get_sales_draft_amount',
	'draft_renderer' => 'render_sales_draft',
	'edit_url'       => 'sales/customer_invoice.php',
));

ApprovalWorkflow::registerTransactionType(ST_SALESORDER, array(
	'label'          => _('Sales Order'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_order',
	'amount_getter'  => 'get_sales_draft_amount',
	'draft_renderer' => 'render_sales_draft',
	'edit_url'       => 'sales/sales_order_entry.php',
	'action_validator' => 'validate_agreement_sales_order_action',
	'action_completion_validator' => 'verify_agreement_sales_order_completion',
));

ApprovalWorkflow::registerTransactionType(ST_CUSTDELIVERY, array(
	'label'          => _('Customer Delivery'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_customer_delivery',
	'amount_getter'  => 'get_sales_draft_amount',
	'draft_renderer' => 'render_sales_draft',
	'edit_url'       => 'sales/customer_delivery.php',
));

ApprovalWorkflow::registerTransactionType(ST_SALES_COMMISSION, array(
	'label'          => _('Sales Commission'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_commission',
	'rejector'       => 'reject_sales_commission_draft',
	'canceler'       => 'cancel_sales_commission_draft',
	'amount_getter'  => 'get_sales_commission_draft_amount',
	'draft_renderer' => 'render_sales_commission_draft',
	'edit_url'       => 'sales/inquiry/commission_inquiry.php',
));

// Phase 5: Sales RMA
ApprovalWorkflow::registerTransactionType(ST_SALESRETURN, array(
	'label'          => _('Sales RMA'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_rma',
	'rejector'       => 'reject_sales_rma_draft',
	'canceler'       => 'cancel_sales_rma_draft',
	'amount_getter'  => 'get_rma_draft_amount',
	'draft_renderer' => 'render_rma_draft',
	'edit_url'       => 'sales/sales_rma_entry.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Restore a Sales Cart object from draft data array.
 *
 * Creates a new Cart instance and populates it with saved properties
 * and line items from the JSON draft.
 *
 * @param array $data Draft data array (decoded JSON)
 * @return object Restored Cart instance
 */
function restore_sales_cart_from_data($data)
{
	global $path_to_root;
	if(!class_exists('Cart'))include_once($path_to_root . '/sales/includes/cart_class.inc');
	if(!function_exists('restore_cart_discount_state'))include_once($path_to_root . '/sales/includes/db/sales_discount_db.inc');

    if(isset($data['company'],$data['company_prefix'],$data['company_sha256'])) {
        global $db_connections;
        $user=$_SESSION['wa_current_user'];
        if((string)$data['company']!==(string)$user->cur_con || $data['company_prefix']!==$db_connections[$user->cur_con]['tbpref']
            || $data['company_sha256']!==hash('sha256',serialize(array($user->cur_con,$db_connections[$user->cur_con]))))throw new RuntimeException('Sales cart belongs to a different company.');
    }
	$cart = new Cart($data['trans_type']);

	$customer_id = isset($data['customer_id']) ? $data['customer_id'] : '';
	$customer_name = isset($data['customer_name']) ? $data['customer_name'] : '';
	$customer_currency = isset($data['customer_currency']) ? $data['customer_currency'] : '';
	$default_discount = isset($data['default_discount']) ? $data['default_discount'] : 0;
	$payment = isset($data['payment']) ? $data['payment'] : 0;
	$branch_id = isset($data['Branch']) ? $data['Branch'] : 0;
	$tax_group_id = isset($data['tax_group_id']) ? $data['tax_group_id'] : 0;
	$sales_type = isset($data['sales_type']) ? $data['sales_type'] : 0;
	$tax_included = isset($data['tax_included']) ? $data['tax_included'] : 0;
	$price_factor = isset($data['price_factor']) ? $data['price_factor'] : 1;

	$cart->trans_no = isset($data['trans_no']) ? $data['trans_no'] : array(0 => 0);
	$cart->set_customer($customer_id, $customer_name, $customer_currency, $default_discount, $payment);
	$cart->set_branch($branch_id, $tax_group_id, '', isset($data['phone']) ? $data['phone'] : '', isset($data['email']) ? $data['email'] : '');
	$cart->set_sales_type($sales_type, '', $tax_included, $price_factor);
	$cart->set_location(isset($data['Location']) ? $data['Location'] : '', isset($data['location_name']) ? $data['location_name'] : '');
	$cart->set_delivery(
		isset($data['ship_via']) ? $data['ship_via'] : 0,
		isset($data['deliver_to']) ? $data['deliver_to'] : '',
		isset($data['delivery_address']) ? $data['delivery_address'] : '',
		isset($data['freight_cost']) ? $data['freight_cost'] : 0
	);

	// Restore core properties
	$cart->document_date     = $data['document_date'];
	$cart->due_date          = isset($data['due_date']) ? $data['due_date'] : '';
	$cart->reference         = $data['reference'];
	$cart->Comments          = isset($data['Comments']) ? $data['Comments'] : '';
	$cart->cust_ref          = isset($data['cust_ref']) ? $data['cust_ref'] : '';
	$cart->src_docs          = isset($data['src_docs']) ? $data['src_docs'] : array();
	$cart->salesman          = isset($data['salesman']) ? $data['salesman'] : 0;
	$cart->phone             = isset($data['phone']) ? $data['phone'] : '';
	$cart->email             = isset($data['email']) ? $data['email'] : '';
	$cart->dimension_id      = isset($data['dimension_id']) ? $data['dimension_id']
		: (isset($data['dimension']) ? $data['dimension'] : 0);
	$cart->dimension2_id     = isset($data['dimension2_id']) ? $data['dimension2_id']
		: (isset($data['dimension2']) ? $data['dimension2'] : 0);
	$cart->classification_intents = isset($data['classification_intents']) ? $data['classification_intents'] : null;
	$cart->classification_draft_token = isset($data['classification_draft_token']) ? $data['classification_draft_token'] : null;
	$cart->classification_draft_revisions = isset($data['classification_draft_revisions']) && is_array($data['classification_draft_revisions'])
		? $data['classification_draft_revisions'] : array();
	$cart->classification_revision_id = isset($data['classification_revision_id']) ? (int)$data['classification_revision_id'] : 0;
	$cart->classification_lineage_state = isset($data['classification_lineage_state']) ? $data['classification_lineage_state'] : 'legacy_unspecified';
	$cart->classification_source = isset($data['classification_source']) ? $data['classification_source'] : null;
	$cart->classification_fulfillment = isset($data['classification_fulfillment']) && is_array($data['classification_fulfillment'])
		? $data['classification_fulfillment'] : null;
	$cart->pos               = isset($data['pos']) ? $data['pos'] : null;
	$cart->cash_account      = isset($data['cash_account']) ? $data['cash_account'] : '';
	$cart->account_name      = isset($data['account_name']) ? $data['account_name'] : '';
	$cart->payment_terms     = isset($data['payment_terms']) ? $data['payment_terms'] : array();
	$cart->credit            = isset($data['credit']) ? $data['credit'] : 0;
	$cart->prepaid           = isset($data['prepaid']) ? $data['prepaid'] : false;
	$cart->prep_amount       = isset($data['prep_amount']) ? $data['prep_amount'] : 0;
	$cart->ex_rate           = isset($data['ex_rate']) ? $data['ex_rate'] : 1;
	$cart->fixed_asset       = isset($data['fixed_asset']) ? $data['fixed_asset'] : false;
	$cart->bo_policy         = isset($data['bo_policy']) ? $data['bo_policy'] : null;

	// Phase 2: Quotation Templates & CRM Pipeline Integration
	$cart->template_id          = isset($data['template_id']) ? (int)$data['template_id'] : 0;
	$cart->validity_date        = isset($data['validity_date']) ? $data['validity_date'] : null;
	$cart->opportunity_id       = isset($data['opportunity_id']) ? (int)$data['opportunity_id'] : 0;
	$cart->terms_and_conditions = isset($data['terms_and_conditions']) ? $data['terms_and_conditions'] : '';
	$cart->margin_total         = isset($data['margin_total']) ? (float)$data['margin_total'] : 0;
	$cart->cost_total           = isset($data['cost_total']) ? (float)$data['cost_total'] : 0;
	$cart->optional_items       = isset($data['optional_items']) ? $data['optional_items'] : array();

	// Phase 3: Sales Agreements & Contracts
	$cart->agreement_id         = isset($data['agreement_id']) ? (int)$data['agreement_id'] : 0;

	if (isset($data['sales_discount_state']))
		restore_cart_discount_state($cart, $data['sales_discount_state']);

	// Restore line items
	if (!empty($data['line_items'])) {
		foreach ($data['line_items'] as $line_data) {
			$cart->add_to_cart(
				count($cart->line_items),
				$line_data['stock_id'],
				$line_data['quantity'],
				$line_data['price'],
				isset($line_data['discount_percent']) ? $line_data['discount_percent'] : 0,
				isset($line_data['qty_done']) ? $line_data['qty_done'] : 0,
				isset($line_data['standard_cost']) ? $line_data['standard_cost'] : 0,
				isset($line_data['item_description']) ? $line_data['item_description'] : '',
				isset($line_data['id']) ? $line_data['id'] : 0,
				isset($line_data['src_no']) ? $line_data['src_no'] : 0,
                isset($line_data['src_id']) ? $line_data['src_id'] : 0
			);
            $restored=$cart->line_items[count($cart->line_items)-1];
            foreach(array('qty_dispatched','qty_old') as $field)if(isset($line_data[$field]))$restored->$field=$line_data[$field];
			if (isset($line_data['item_description']))
				$restored->item_description = $line_data['item_description'];
			foreach(array('dimension_id','dimension2_id') as $field)
				if (array_key_exists($field, $line_data)) $restored->$field = (int)$line_data[$field];
			foreach(array('classification_intents','classification_token','draft_line_key',
				'classification_draft_revision_id','classification_revision_id',
				'classification_lineage_state','classification_source') as $field)
				if (array_key_exists($field, $line_data)) $restored->$field = $line_data[$field];
		}
	}

	return $cart;
}

/**
 * Execute an approved Sales Invoice draft.
 *
 * Reconstructs the Cart from saved data and calls Cart::write()
 * to post the invoice.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_sales_invoice($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/sales_db.inc');

	$cart = restore_sales_cart_from_data($draft_data);
	$cart->trans_type = ST_SALESINVOICE;

	$invoice_no = $cart->write();

	if ($invoice_no === false || $invoice_no <= 0)
		return false;

	return $invoice_no;
}

/**
 * Execute an approved Sales Order draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_sales_order($draft_data, $reserved_trans_no, $draft_id)
{
	if (is_array($draft_data) && array_key_exists('agreement_conversion',$draft_data))
		return execute_agreement_sales_order($draft_data,$reserved_trans_no,$draft_id);
    if(is_array($draft_data) && isset($draft_data['native_sales_order']))return execute_native_sales_order($draft_data,$reserved_trans_no,$draft_id);
    return false; // Unknown-company legacy drafts must withdraw and resubmit.
}

/**
 * Execute an approved Customer Delivery draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_customer_delivery($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/sales_db.inc');

	$cart = restore_sales_cart_from_data($draft_data);
	$cart->trans_type = ST_CUSTDELIVERY;

	$policy = isset($draft_data['bo_policy']) ? (int)$draft_data['bo_policy'] : 0;
	$ret = $cart->write($policy);

	if ($ret === false || $ret <= 0)
		return false;

	return key($cart->trans_no);
}

/**
 * Execute an approved Sales Commission draft.
 *
 * @param array $draft_data
 * @param int   $reserved_trans_no
 * @param int   $draft_id
 * @return int|false
 */
function execute_approved_sales_commission($draft_data, $reserved_trans_no, $draft_id)
{
	$entry_id = isset($draft_data['entry_id']) ? (int)$draft_data['entry_id'] : 0;
	if ($entry_id <= 0)
		return false;

	if (!approve_sales_commission_entry_from_core($entry_id, 'approval_system'))
		return false;

	return $entry_id;
}

/**
 * Keep commission entry calculated when approval draft is rejected.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function reject_sales_commission_draft($draft_data, $draft_id, $comments)
{
	return true;
}

/**
 * Keep commission entry calculated when approval draft is cancelled.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function cancel_sales_commission_draft($draft_data, $draft_id, $comments)
{
	return true;
}

// =====================================================
// AMOUNT GETTERS
// =====================================================

/**
 * Calculate the total amount of a sales draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total amount (sum of qty * price * (1 - discount))
 */
function get_sales_draft_amount($draft_data)
{
	$total = 0;
	if (!empty($draft_data['line_items'])) {
		foreach ($draft_data['line_items'] as $line) {
			$price    = isset($line['price']) ? (float)$line['price'] : 0;
			$qty      = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$discount = isset($line['discount_percent']) ? (float)$line['discount_percent'] : 0;
			$total += round($qty * $price * (1 - $discount), 2);
		}
	}
	$total += isset($draft_data['freight_cost']) ? (float)$draft_data['freight_cost'] : 0;

	return $total;
}

/**
 * Get amount for Sales Commission draft.
 *
 * @param array $draft_data
 * @return float
 */
function get_sales_commission_draft_amount($draft_data)
{
	return isset($draft_data['commission_amount']) ? (float)$draft_data['commission_amount'] : 0;
}

// =====================================================
// DRAFT RENDERERS
// =====================================================

/**
 * Render a sales transaction draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_sales_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Customer') . '</th><td>' . htmlspecialchars(isset($draft_data['customer_name']) ? $draft_data['customer_name'] : $draft_data['customer_id']) . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars($draft_data['reference']) . '</td></tr>';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars($draft_data['document_date']) . '</td>';
	$html .= '<th>' . _('Due Date') . '</th><td>' . htmlspecialchars(isset($draft_data['due_date']) ? $draft_data['due_date'] : '') . '</td></tr>';

	if (!empty($draft_data['deliver_to']))
		$html .= '<tr><th>' . _('Deliver To') . '</th><td colspan="3">' . htmlspecialchars($draft_data['deliver_to']) . '</td></tr>';

	if (!empty($draft_data['Comments']))
		$html .= '<tr><th>' . _('Comments') . '</th><td colspan="3">' . htmlspecialchars($draft_data['Comments']) . '</td></tr>';

	$html .= '</table>';

	// Line items
	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Price') . '</th><th>' . _('Disc %') . '</th><th>' . _('Total') . '</th></tr>';

		$grand_total = 0;
		foreach ($draft_data['line_items'] as $line) {
			$price    = isset($line['price']) ? (float)$line['price'] : 0;
			$qty      = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$discount = isset($line['discount_percent']) ? (float)$line['discount_percent'] : 0;
			$line_total = round($qty * $price * (1 - $discount), 2);
			$grand_total += $line_total;

			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['item_description']) ? $line['item_description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($qty, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td align="right">' . number_format($discount * 100, 1) . '%</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		if (!empty($draft_data['freight_cost'])) {
			$grand_total += (float)$draft_data['freight_cost'];
			$html .= '<tr><td colspan="5"><i>' . _('Freight') . '</i></td>';
			$html .= '<td align="right">' . number_format((float)$draft_data['freight_cost'], 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="5"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}

/**
 * Render Sales Commission draft details for approval viewer.
 *
 * @param array $draft_data
 * @return string
 */
function render_sales_commission_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Entry ID') . '</th><td>' . (int)(isset($draft_data['entry_id']) ? $draft_data['entry_id'] : 0) . '</td>';
	$html .= '<th>' . _('Date') . '</th><td>' . htmlspecialchars(isset($draft_data['trans_date']) ? $draft_data['trans_date'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Sales Person') . '</th><td>' . htmlspecialchars(isset($draft_data['salesman_name']) ? $draft_data['salesman_name'] : '') . '</td>';
	$html .= '<th>' . _('Plan') . '</th><td>' . htmlspecialchars(isset($draft_data['plan_name']) ? $draft_data['plan_name'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Customer') . '</th><td>' . htmlspecialchars(isset($draft_data['customer_name']) ? $draft_data['customer_name'] : '') . '</td>';
	$html .= '<th>' . _('Transaction') . '</th><td>#' . (int)(isset($draft_data['trans_no']) ? $draft_data['trans_no'] : 0) . '</td></tr>';
	$html .= '<tr><th>' . _('Base Amount') . '</th><td>' . number_format((float)(isset($draft_data['base_amount']) ? $draft_data['base_amount'] : 0), 2) . '</td>';
	$html .= '<th>' . _('Rate %') . '</th><td>' . number_format((float)(isset($draft_data['commission_rate']) ? $draft_data['commission_rate'] : 0), 2) . '</td></tr>';
	$html .= '<tr><th>' . _('Commission') . '</th><td colspan="3"><b>' . number_format((float)(isset($draft_data['commission_amount']) ? $draft_data['commission_amount'] : 0), 2) . '</b></td></tr>';
	$html .= '</table>';

	return $html;
}

// =====================================================
// PHASE 5: RMA APPROVAL HELPERS
// =====================================================

/**
 * Execute an approved Sales RMA draft.
 *
 * Authorizes the RMA (changes status from pending → authorized).
 *
 * @param array $draft_data      Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id        Approval draft ID
 * @return int|false  RMA ID or false on failure
 */
function execute_approved_sales_rma($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/db/sales_rma_db.inc');

	$rma_id = isset($draft_data['rma_id']) ? (int)$draft_data['rma_id'] : 0;
	if (!$rma_id)
		return false;

	if (authorize_rma($rma_id, _('Approved via workflow')))
		return $rma_id;

	return false;
}

/**
 * Get the refund amount of a Sales RMA draft.
 *
 * @param array $draft_data
 * @return float
 */
function get_rma_draft_amount($draft_data)
{
	return isset($draft_data['refund_amount']) ? (float)$draft_data['refund_amount'] : 0;
}

/**
 * Render a Sales RMA draft for the approval review UI.
 *
 * @param array $draft_data
 * @return string HTML
 */
function render_rma_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Customer') . '</th><td>' . htmlspecialchars(isset($draft_data['customer_name']) ? $draft_data['customer_name'] : '') . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Request Date') . '</th><td>' . htmlspecialchars(isset($draft_data['request_date']) ? $draft_data['request_date'] : '') . '</td>';
	$html .= '<th>' . _('Return Method') . '</th><td>' . htmlspecialchars(isset($draft_data['return_method']) ? $draft_data['return_method'] : '') . '</td></tr>';
	if (!empty($draft_data['customer_notes']))
		$html .= '<tr><th>' . _('Customer Notes') . '</th><td colspan="3">' . htmlspecialchars($draft_data['customer_notes']) . '</td></tr>';
	$html .= '</table>';

	if (!empty($draft_data['lines'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Unit Price') . '</th><th>' . _('Condition') . '</th></tr>';
		$grand_total = 0;
		foreach ($draft_data['lines'] as $line) {
			$qty   = isset($line['quantity_authorized']) ? (float)$line['quantity_authorized'] : 0;
			$price = isset($line['unit_price']) ? (float)$line['unit_price'] : 0;
			$grand_total += $qty * $price;
			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['description']) ? $line['description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($qty, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['return_condition']) ? $line['return_condition'] : '') . '</td></tr>';
		}
		$html .= '<tr><td colspan="3"><b>' . _('Refund Amount') . '</b></td>';
		$html .= '<td align="right" colspan="2"><b>' . number_format(isset($draft_data['refund_amount']) ? (float)$draft_data['refund_amount'] : $grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}


/** Sales agreement shared-core callback, preserving the native source family. */
function execute_approved_sales_agreement($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	require_once $path_to_root.'/sales/includes/db/sales_agreement_db.inc';
	return execute_sales_agreement_core_action('approve',$draft_data,$draft_id);

}


/** Sales agreement shared-core callback, preserving the native source family. */
function reject_sales_agreement_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	require_once $path_to_root.'/sales/includes/db/sales_agreement_db.inc';
	return execute_sales_agreement_core_action('reject',$draft_data,$draft_id,$comments);

}


/** Sales agreement shared-core callback, preserving the native source family. */
function cancel_sales_agreement_draft($draft_data, $draft_id, $comments)
{
	global $path_to_root;
	require_once $path_to_root.'/sales/includes/db/sales_agreement_db.inc';
	return execute_sales_agreement_core_action('cancel',$draft_data,$draft_id,$comments);

}


/** Sales agreement shared-core callback, preserving the native source family. */
function get_sales_agreement_draft_amount($draft_data)
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


/** Sales agreement shared-core callback, preserving the native source family. */
function render_sales_agreement_draft($draft_data)
{
	$classification = array(); $opaque = false;
	foreach (array('dimension_id'=>1,'dimension2_id'=>2) as $field=>$axis)
		if (!empty($draft_data[$field])) $classification[$axis.':'.(int)$draft_data[$field]] = sprintf(_('Dimension %d: #%d'),$axis,(int)$draft_data[$field]);
	$payloads = array(isset($draft_data['custom_data']) ? $draft_data['custom_data'] : null);
	foreach (isset($draft_data['line_items']) ? $draft_data['line_items'] : array() as $line)
		$payloads[] = isset($line['custom_data']) ? $line['custom_data'] : null;
	foreach ($payloads as $payload) {
		$references = dimension_master_payload_references(is_array($payload) ? json_encode($payload) : $payload);
		if ($references === false) { $opaque = true; continue; }
		foreach ($references as $reference) $classification[$reference[1].':'.$reference[0]] = sprintf(_('Dimension %d: #%d'),$reference[1],$reference[0]);
	}
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td>';
	$html .= '<th>' . _('Customer') . '</th><td>' . htmlspecialchars(isset($draft_data['customer_name']) ? $draft_data['customer_name'] : (isset($draft_data['debtor_no']) ? '#'.$draft_data['debtor_no'] : '')) . '</td></tr>';
	$html .= '<tr><th>' . _('Agreement Type') . '</th><td>' . htmlspecialchars(isset($draft_data['agreement_type']) ? $draft_data['agreement_type'] : '') . '</td>';
	$html .= '<th>' . _('Sales Person') . '</th><td>' . htmlspecialchars(isset($draft_data['salesman_name']) ? $draft_data['salesman_name'] : (isset($draft_data['salesman_id']) ? '#'.$draft_data['salesman_id'] : '')) . '</td></tr>';
	$html .= '<tr><th>' . _('Start Date') . '</th><td>' . htmlspecialchars(isset($draft_data['date_start']) ? $draft_data['date_start'] : '') . '</td>';
	$html .= '<th>' . _('End Date') . '</th><td>' . htmlspecialchars(isset($draft_data['date_end']) ? $draft_data['date_end'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Branch') . '</th><td>' . htmlspecialchars(isset($draft_data['branch_name']) ? $draft_data['branch_name'] : (isset($draft_data['branch_code']) ? $draft_data['branch_code'] : '')) . '</td>';
	$html .= '<th>' . _('Committed Total') . '</th><td><b>' . number_format(get_sales_agreement_draft_amount($draft_data), 2) . '</b></td></tr>';
	$html .= '<tr><th>'._('Classifications').'</th><td colspan="3">'.htmlspecialchars(implode(', ',$classification))
		.($opaque ? ' '.htmlspecialchars(_('Additional retained classification requires owner review.')) : '').'</td></tr>';
	if (!empty($draft_data['terms_and_conditions']))
		$html .= '<tr><th>'._('Terms and Conditions').'</th><td colspan="3">'.htmlspecialchars($draft_data['terms_and_conditions']).'</td></tr>';

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
