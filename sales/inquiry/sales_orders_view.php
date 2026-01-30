<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
$path_to_root = '../..';

include_once($path_to_root.'/includes/db_pager.inc');
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/sales/includes/sales_ui.inc');
include_once($path_to_root.'/reporting/includes/reporting.inc');

$page_security = 'SA_SALESTRANSVIEW';

set_page_security( @$_POST['order_view_mode'],
	array(	'OutstandingOnly' => 'SA_SALESDELIVERY',
			'InvoiceTemplates' => 'SA_SALESINVOICE',
			'DeliveryTemplates' => 'SA_SALESDELIVERY',
			'PrepaidOrders' => 'SA_SALESINVOICE'),
	array(	'OutstandingOnly' => 'SA_SALESDELIVERY',
			'InvoiceTemplates' => 'SA_SALESINVOICE',
			'DeliveryTemplates' => 'SA_SALESDELIVERY',
			'PrepaidOrders' => 'SA_SALESINVOICE')
);

if (get_post('type'))
	$trans_type = $_POST['type'];
elseif (isset($_GET['type']) && $_GET['type'] == ST_SALESQUOTE)
	$trans_type = ST_SALESQUOTE;
else
	$trans_type = ST_SALESORDER;

if ($trans_type == ST_SALESORDER) {
	if (isset($_GET['OutstandingOnly']) && ($_GET['OutstandingOnly'] == true)) {
		$_POST['order_view_mode'] = 'OutstandingOnly';
		$_SESSION['page_title'] = _($help_context = 'Search Outstanding Sales Orders');
	}
	elseif (isset($_GET['InvoiceTemplates']) && ($_GET['InvoiceTemplates'] == true)) {
		$_POST['order_view_mode'] = 'InvoiceTemplates';
		$_SESSION['page_title'] = _($help_context = 'Search Template for Invoicing');
	}
	elseif (isset($_GET['DeliveryTemplates']) && ($_GET['DeliveryTemplates'] == true)) {
		$_POST['order_view_mode'] = 'DeliveryTemplates';
		$_SESSION['page_title'] = _($help_context = 'Select Template for Delivery');
	}
	elseif (isset($_GET['PrepaidOrders']) && ($_GET['PrepaidOrders'] == true)) {
		$_POST['order_view_mode'] = 'PrepaidOrders';
		$_SESSION['page_title'] = _($help_context = 'Invoicing Prepayment Orders');
	}
	elseif (!isset($_POST['order_view_mode'])) {
		$_POST['order_view_mode'] = false;
		$_SESSION['page_title'] = _($help_context = 'Search All Sales Orders');
	}
}
else {
	$_POST['order_view_mode'] = 'Quotations';
	$_SESSION['page_title'] = _($help_context = 'Search All Sales Quotations');
}

$js = '';

if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

//---------------------------------------------------------------------------------------------

function check_overdue($row) {
	global $trans_type;
	if ($trans_type == ST_SALESQUOTE)
		return (date1_greater_date2(Today(), sql2date($row['delivery_date'])));
	else
		return ($row['type'] == 0
			&& date1_greater_date2(Today(), sql2date($row['delivery_date']))
			&& ($row['TotDelivered'] < $row['TotQuantity']));
}

function view_link($dummy, $order_no) {
	global $trans_type;
	return  get_customer_trans_view_str($trans_type, $order_no);
}

function prt_link($row) {
	global $trans_type;
	return print_document_link($row['order_no'], _('Print'), $trans_type, ICON_PRINT);
}

function edit_link($row) {
	global $page_nested;

	if (is_prepaid_order_open($row['order_no']))
		return '';

	return $page_nested ? '' : trans_editor_link($row['trans_type'], $row['order_no']);
}

function dispatch_link($row) {
	global $trans_type, $page_nested;

	if ($row['ord_payments'] + $row['inv_payments'] < $row['prep_amount'])
		return '';

	if ($trans_type == ST_SALESORDER) {
		if ($row['TotDelivered'] < $row['TotQuantity'] && !$page_nested)
			return pager_link( _('Dispatch'), '/sales/customer_delivery.php?OrderNumber='.$row['order_no'], ICON_DOC);
		else
			return '';
	}		
	else
		return pager_link( _('Sales Order'), '/sales/sales_order_entry.php?OrderNumber='.$row['order_no'], ICON_DOC);
}

function invoice_link($row) {
	global $trans_type;
	if ($trans_type == ST_SALESORDER)
		return pager_link( _('Invoice'), '/sales/sales_order_entry.php?NewInvoice='.$row['order_no'], ICON_DOC);
	else
		return '';
}

function delivery_link($row) {
	return pager_link( _('Delivery'), '/sales/sales_order_entry.php?NewDelivery='.$row['order_no'], ICON_DOC);
}

function order_link($row) {
	return pager_link( _('Sales Order'), '/sales/sales_order_entry.php?NewQuoteToSalesOrder='.$row['order_no'], ICON_DOC);
}

function tmpl_checkbox($row) {
	global $trans_type, $page_nested;

	if ($trans_type == ST_SALESQUOTE || !check_sales_order_type($row['order_no']))
		return '';

	if ($page_nested)
		return '';
	$name = 'chgtpl' .$row['order_no'];
	$value = $row['type'] ? 1:0;

	// save also in hidden field for testing during 'Update'

	return checkbox(null, $name, $value, true, _('Set this order as a template for direct deliveries/invoices')).hidden('last['.$row['order_no'].']', $value, false);
}

function unallocated_prepayments($row) {

	if ($row['ord_payments'] > 0) {
		$pmts = get_payments_for($row['order_no'], $row['trans_type'], $row['debtor_no']);

		foreach($pmts as $pmt) {
			$list[] = get_trans_view_str($pmt['trans_type_from'], $pmt['trans_no_from'], get_reference($pmt['trans_type_from'], $pmt['trans_no_from']));
		}
		return implode(',', $list);
	}
	else
		return '';
}

function invoice_prep_link($row) {
	// invoicing should be available only for partially allocated orders
	return 
		$row['inv_payments'] < $row['total'] ?
		pager_link($row['ord_payments']  ? _('Prepayment Invoice') : _('Final Invoice'),
		'/sales/customer_invoice.php?InvoicePrepayments=' .$row['order_no'], ICON_DOC) : '';
}

$id = find_submit('_chgtpl');
if ($id != -1) {
	sales_order_set_template($id, check_value('chgtpl'.$id));
	$Ajax->activate('orders_tbl');
}

if (isset($_POST['Update']) && isset($_POST['last'])) {
	foreach($_POST['last'] as $id => $value)
		if ($value != check_value('chgtpl'.$id))
			sales_order_set_template($id, !check_value('chgtpl'.$id));
}

$show_dates = !in_array($_POST['order_view_mode'], array('OutstandingOnly', 'InvoiceTemplates', 'DeliveryTemplates'));

//---------------------------------------------------------------------------------------------
//	Order range form
//
if (get_post('_OrderNumber_changed') || get_post('_OrderReference_changed')) { // enable/disable selection controls
	$disable = get_post('OrderNumber') !== '' || get_post('OrderReference') !== '';

	if ($show_dates) {
		$Ajax->addDisable(true, 'OrdersAfterDate', $disable);
		$Ajax->addDisable(true, 'OrdersToDate', $disable);
	}

	$Ajax->activate('orders_tbl');
}

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
ref_cells(_('#:'), 'OrderNumber', '',null, '', true);
ref_cells(_('Ref'), 'OrderReference', '',null, '', true);

if ($show_dates)
    yesno_list_cells('', 'by_delivery', null, ($trans_type==ST_SALESORDER ? _('Delivery date') : _('Valid until')).':', ($trans_type==ST_SALESORDER ? _('Order date') : _('Quotation date')).':');

if ($show_dates) {
	date_cells(_('from:'), 'OrdersAfterDate', '', null, -user_transaction_days());
	date_cells(_('to:'), 'OrdersToDate', '', null, 1);
}
locations_list_cells(_('Location:'), 'StockLocation', null, true, true);

if($show_dates) {
	end_row();
	end_table();

	start_table(TABLESTYLE_NOBORDER);
	start_row();
}
stock_items_list_cells(_('Item:'), 'SelectStockFromList', null, true, true);

if (!$page_nested)
	customer_list_cells(_('Select a customer: '), 'customer_id', null, true, true);
if ($trans_type == ST_SALESQUOTE)
	check_cells(_('Show All:'), 'show_all');
if ($trans_type == ST_SALESORDER)
	check_cells(_('Zero values'), 'show_voided');
if ($show_dates && $trans_type == ST_SALESORDER)
	check_cells(_('No auto'), 'no_auto');

submit_cells('SearchOrders', _('Search'),'',_('Select documents'), 'default');
hidden('order_view_mode', $_POST['order_view_mode']);
hidden('type', $trans_type);

end_row();

end_table(1);

//---------------------------------------------------------------------------------------------
//	Orders inquiry table
//
$sql = get_sql_for_sales_orders_view($trans_type, get_post('OrderNumber'), get_post('order_view_mode'), get_post('SelectStockFromList'), get_post('OrdersAfterDate'), get_post('OrdersToDate'), get_post('OrderReference'), get_post('StockLocation'), get_post('customer_id'), check_value('show_voided'), get_post('by_delivery'), get_post('no_auto'));

if ($trans_type == ST_SALESORDER)
	$cols = array(
		_('Order #') => array('fun'=>'view_link', 'align'=>'right', 'ord' =>''),
		_('Ref') => array('type' => 'sorder.reference', 'ord' => '') ,
		_('Customer') => array('type' => 'debtor.name' , 'ord' => '') ,
		_('Branch'), 
		_('Cust Order Ref'),
		_('Order Date') => array('type' =>  'date', 'ord' => ''),
		_('Required By') =>array('type'=>'date', 'ord'=>''),
		_('Delivery To'), 
		_('Order Total') => array('type'=>'amount', 'ord'=>''),
		'Type' => 'skip',
		_('Currency') => array('align'=>'center')
	);
else
	$cols = array(
		_('Quote #') => array('fun'=>'view_link', 'align'=>'right', 'ord' => ''),
		_('Ref'),
		_('Customer'),
		_('Branch'), 
		_('Cust Order Ref'),
		_('Quote Date') => 'date',
		_('Valid until') =>array('type'=>'date', 'ord'=>''),
		_('Delivery To'), 
		_('Quote Total') => array('type'=>'amount', 'ord'=>''),
		'Type' => 'skip',
		_('Currency') => array('align'=>'center')
	);
if ($_POST['order_view_mode'] == 'OutstandingOnly') {
	array_append($cols, array(
		array('insert'=>true, 'fun'=>'edit_link'),
		array('insert'=>true, 'fun'=>'dispatch_link'),
		array('insert'=>true, 'fun'=>'prt_link')));

}
elseif ($_POST['order_view_mode'] == 'InvoiceTemplates') {
	array_substitute($cols, 4, 1, _('Description'));
	array_append($cols, array( array('insert'=>true, 'fun'=>'invoice_link')));

}
else if ($_POST['order_view_mode'] == 'DeliveryTemplates') {
	array_substitute($cols, 4, 1, _('Description'));
	array_append($cols, array(
			array('insert'=>true, 'fun'=>'delivery_link'))
	);
}
else if ($_POST['order_view_mode'] == 'PrepaidOrders') {
	array_append($cols, array(
		_('New Payments') => array('insert'=>true, 'fun'=>'unallocated_prepayments'),
		array('insert'=>true, 'fun'=>'invoice_prep_link')
	));

}
elseif ($trans_type == ST_SALESQUOTE) {
	 array_append($cols,array(
					array('insert'=>true, 'fun'=>'edit_link'),
					array('insert'=>true, 'fun'=>'order_link'),
					array('insert'=>true, 'fun'=>'prt_link')));
}
elseif ($trans_type == ST_SALESORDER) {
	 array_append($cols,array(
			_('Tmpl') => array('insert'=>true, 'fun'=>'tmpl_checkbox'),
					array('insert'=>true, 'fun'=>'edit_link'),
					array('insert'=>true, 'fun'=>'dispatch_link'),
					array('insert'=>true, 'fun'=>'prt_link')));
};


$table =& new_db_pager('orders_tbl', $sql, $cols);
$table->set_marker('check_overdue', _('Marked items are overdue.'));

$table->width = '80%';

display_db_pager($table);
submit_center('Update', _('Update'), true, '', null);

end_form();
end_page();
