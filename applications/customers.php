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
class customers_app extends application 
{
	function __construct() 
	{
		parent::__construct("orders", _($this->help_context = "&Sales"));
	
		$this->add_module(_("Transactions"));
		$this->add_lapp_function(0, _("Sales &Quotation Entry"),
			"sales/sales_order_entry.php?NewQuotation=Yes", 'SA_SALESQUOTE', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("Sales &Order Entry"),
			"sales/sales_order_entry.php?NewOrder=Yes", 'SA_SALESORDER', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("Direct &Delivery"),
			"sales/sales_order_entry.php?NewDelivery=0", 'SA_SALESDELIVERY', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("Direct &Invoice"),
			"sales/sales_order_entry.php?NewInvoice=0", 'SA_SALESINVOICE', MENU_TRANSACTION);
		$this->add_lapp_function(0, "","");
		$this->add_lapp_function(0, _("&Delivery Against Sales Orders"),
			"sales/inquiry/sales_orders_view.php?OutstandingOnly=1", 'SA_SALESDELIVERY', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("&Invoice Against Sales Delivery"),
			"sales/inquiry/sales_deliveries_view.php?OutstandingOnly=1", 'SA_SALESINVOICE', MENU_TRANSACTION);

		$this->add_rapp_function(0, _("&Template Delivery"),
			"sales/inquiry/sales_orders_view.php?DeliveryTemplates=Yes", 'SA_SALESDELIVERY', MENU_TRANSACTION);
		$this->add_rapp_function(0, _("&Template Invoice"),
			"sales/inquiry/sales_orders_view.php?InvoiceTemplates=Yes", 'SA_SALESINVOICE', MENU_TRANSACTION);
		$this->add_rapp_function(0, _("&Create and Print Recurrent Invoices"),
			"sales/create_recurrent_invoices.php?", 'SA_SALESINVOICE', MENU_TRANSACTION);
		$this->add_rapp_function(0, "","");
		$this->add_rapp_function(0, _("Customer &Payments"),
			"sales/customer_payments.php?", 'SA_SALESPAYMNT', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("Invoice &Prepaid Orders"),
			"sales/inquiry/sales_orders_view.php?PrepaidOrders=Yes", 'SA_SALESINVOICE', MENU_TRANSACTION);
		$this->add_rapp_function(0, _("Customer &Credit Notes"),
			"sales/credit_note_entry.php?NewCredit=Yes", 'SA_SALESCREDIT', MENU_TRANSACTION);
		$this->add_rapp_function(0, _("&Allocate Customer Payments or Credit Notes"),
			"sales/allocations/customer_allocation_main.php?", 'SA_SALESALLOC', MENU_TRANSACTION);

		$this->add_module(_("Inquiries and Reports"));
		$this->add_lapp_function(1, _("Sales Quotation I&nquiry"),
			"sales/inquiry/sales_orders_view.php?type=32", 'SA_SALESTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _("Sales Order &Inquiry"),
			"sales/inquiry/sales_orders_view.php?type=30", 'SA_SALESTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _("Customer Transaction &Inquiry"),
			"sales/inquiry/customer_inquiry.php?", 'SA_SALESTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _("Customer Allocation &Inquiry"),
			"sales/inquiry/customer_allocation_inquiry.php?", 'SA_SALESALLOC', MENU_INQUIRY);

		$this->add_rapp_function(1, _("Customer and Sales &Reports"),
			"reporting/reports_main.php?Class=0", 'SA_SALESTRANSVIEW', MENU_REPORT);

		$this->add_module(_("Maintenance"));
		$this->add_lapp_function(2, _("Add and Manage &Customers"),
			"sales/manage/customers.php?", 'SA_CUSTOMER', MENU_ENTRY);
		$this->add_lapp_function(2, _("Customer &Branches"),
			"sales/manage/customer_branches.php?", 'SA_CUSTOMER', MENU_ENTRY);
		$this->add_lapp_function(2, _("Sales &Groups"),
			"sales/manage/sales_groups.php?", 'SA_SALESGROUP', MENU_MAINTENANCE);
		$this->add_lapp_function(2, _("Recurrent &Invoices"),
			"sales/manage/recurrent_invoices.php?", 'SA_SRECURRENT', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _("Sales T&ypes"),
			"sales/manage/sales_types.php?", 'SA_SALESTYPES', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _("Sales &Persons"),
			"sales/manage/sales_people.php?", 'SA_SALESMAN', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _("Sales &Areas"),
			"sales/manage/sales_areas.php?", 'SA_SALESAREA', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _("Credit &Status Setup"),
			"sales/manage/credit_status.php?", 'SA_CRSTATUS', MENU_MAINTENANCE);

		$this->add_extensions();
	}
}


