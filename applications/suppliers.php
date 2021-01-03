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
class suppliers_app extends application 
{
	function __construct() 
	{
		parent::__construct("AP", _($this->help_context = "&Purchases"));

		$this->add_module(_("Transactions"));
		$this->add_lapp_function(0, _("Purchase &Order Entry"),
			"purchasing/po_entry_items.php?NewOrder=Yes", 'SA_PURCHASEORDER', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("&Outstanding Purchase Orders Maintenance"),
			"purchasing/inquiry/po_search.php?", 'SA_GRN', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("Direct &GRN"),
			"purchasing/po_entry_items.php?NewGRN=Yes", 'SA_GRN', MENU_TRANSACTION);
		$this->add_lapp_function(0, _("Direct Supplier &Invoice"),
			"purchasing/po_entry_items.php?NewInvoice=Yes", 'SA_SUPPLIERINVOICE', MENU_TRANSACTION);

		$this->add_rapp_function(0, _("&Payments to Suppliers"),
			"purchasing/supplier_payment.php?", 'SA_SUPPLIERPAYMNT', MENU_TRANSACTION);
		$this->add_rapp_function(0, "","");
		$this->add_rapp_function(0, _("Supplier &Invoices"),
			"purchasing/supplier_invoice.php?New=1", 'SA_SUPPLIERINVOICE', MENU_TRANSACTION);
		$this->add_rapp_function(0, _("Supplier &Credit Notes"),
			"purchasing/supplier_credit.php?New=1", 'SA_SUPPLIERCREDIT', MENU_TRANSACTION);
		$this->add_rapp_function(0, _("&Allocate Supplier Payments or Credit Notes"),
			"purchasing/allocations/supplier_allocation_main.php?", 'SA_SUPPLIERALLOC', MENU_TRANSACTION);

		$this->add_module(_("Inquiries and Reports"));
		$this->add_lapp_function(1, _("Purchase Orders &Inquiry"),
			"purchasing/inquiry/po_search_completed.php?", 'SA_SUPPTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _("Supplier Transaction &Inquiry"),
			"purchasing/inquiry/supplier_inquiry.php?", 'SA_SUPPTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _("Supplier Allocation &Inquiry"),
			"purchasing/inquiry/supplier_allocation_inquiry.php?", 'SA_SUPPLIERALLOC', MENU_INQUIRY);

		$this->add_rapp_function(1, _("Supplier and Purchasing &Reports"),
			"reporting/reports_main.php?Class=1", 'SA_SUPPTRANSVIEW', MENU_REPORT);

		$this->add_module(_("Maintenance"));
		$this->add_lapp_function(2, _("&Suppliers"),
			"purchasing/manage/suppliers.php?", 'SA_SUPPLIER', MENU_ENTRY);

		$this->add_extensions();
	}
}


