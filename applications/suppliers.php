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
class SuppliersApp extends application {
	function __construct() {
		parent::__construct('AP', _($this->help_context = '&Purchases'));

		$this->add_module(_('Transactions'));
		// === Purchasing Sourcing Workflow ===
		$this->add_lapp_function(0, _('Purchase Re&quisition Entry'), 'purchasing/purch_requisition_entry.php?New=1', 'SA_PURCHREQUISITION', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Purchase &RFQ Entry'), 'purchasing/purch_rfq_entry.php?New=1', 'SA_PURCHRFQ', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Purchase &Agreement Entry'), 'purchasing/purch_agreement_entry.php?New=1', 'SA_PURCHAGREEMENT', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Purchase &Order Entry'), 'purchasing/po_entry_items.php?NewOrder=Yes', 'SA_PURCHASEORDER', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('&Outstanding Purchase Orders Maintenance'), 'purchasing/inquiry/po_search.php?', 'SA_GRN', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Direct &GRN'), 'purchasing/po_entry_items.php?NewGRN=Yes', 'SA_GRN', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Direct Supplier &Invoice'), 'purchasing/po_entry_items.php?NewInvoice=Yes', 'SA_SUPPLIERINVOICE', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('P&rocurement Plan'), 'purchasing/procurement_plan.php?', 'SA_PROCUREMENTPLAN', MENU_TRANSACTION);

		$this->add_rapp_function(0, _('&Payments to Suppliers'), 'purchasing/supplier_payment.php?', 'SA_SUPPLIERPAYMNT', MENU_TRANSACTION);
		$this->add_rapp_function(0, '','');
		$this->add_rapp_function(0, _('Supplier &Invoices'), 'purchasing/supplier_invoice.php?New=1', 'SA_SUPPLIERINVOICE', MENU_TRANSACTION);
		$this->add_rapp_function(0, _('Supplier &Credit Notes'), 'purchasing/supplier_credit.php?New=1', 'SA_SUPPLIERCREDIT', MENU_TRANSACTION);
		$this->add_rapp_function(0, _('&Allocate Supplier Payments or Credit Notes'), 'purchasing/allocations/supplier_allocation_main.php?', 'SA_SUPPLIERALLOC', MENU_TRANSACTION);

		$this->add_module(_('Inquiries and Reports'));
		// === Purchasing Sourcing Inquiries ===
		$this->add_lapp_function(1, _('Purchase Requisition &Inquiry'), 'purchasing/inquiry/purch_requisitions_view.php?', 'SA_PURCHREQUISITION', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Purchase RFQ In&quiry'), 'purchasing/inquiry/purch_rfq_view.php?', 'SA_PURCHRFQ', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Purchase Agreement In&quiry'), 'purchasing/inquiry/purch_agreements_view.php?', 'SA_PURCHAGREEMENT', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Vendor &Scorecard'), 'purchasing/inquiry/vendor_scorecard.php?', 'SA_VENDOREVALUATION', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Matching E&xceptions'), 'purchasing/inquiry/matching_exceptions.php?', 'SA_PURCHMATCHEXCEPTIONS', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Reorder &Status'), 'purchasing/inquiry/reorder_status.php?', 'SA_REORDERRULES', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Purchase &Dashboard'), 'purchasing/dashboard/purchase_dashboard.php?', 'SA_PURCHDASHBOARD', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Purchase Spend &Analysis'), 'reporting/rep_purchase_spend.php?', 'SA_PURCHREPORT', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Vendor Per&formance'), 'reporting/rep_vendor_performance.php?', 'SA_PURCHREPORT', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Purchase Price &Variance'), 'reporting/rep_purchase_variance.php?', 'SA_PURCHREPORT', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Purchase Orders &Inquiry'), 'purchasing/inquiry/po_search_completed.php?', 'SA_SUPPTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Supplier Transaction &Inquiry'), 'purchasing/inquiry/supplier_inquiry.php?', 'SA_SUPPTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Supplier Allocation &Inquiry'), 'purchasing/inquiry/supplier_allocation_inquiry.php?', 'SA_SUPPLIERALLOC', MENU_INQUIRY);

		$this->add_rapp_function(1, _('Supplier and Purchasing &Reports'), 'reporting/reports_main.php?Class=1', 'SA_SUPPTRANSVIEW', MENU_REPORT);

		$this->add_module(_('Maintenance'));
		$this->add_lapp_function(2, _('&Suppliers'), 'purchasing/manage/suppliers.php?', 'SA_SUPPLIER', MENU_ENTRY);
		$this->add_lapp_function(2, _('Vendor &Evaluations'), 'purchasing/manage/vendor_evaluation.php?', 'SA_VENDOREVALUATION', MENU_ENTRY);
		$this->add_lapp_function(2, _('Vendor Price&lists'), 'purchasing/manage/vendor_pricelists.php?', 'SA_VENDORPRICELIST', MENU_ENTRY);
		$this->add_lapp_function(2, _('Purchase &Matching Config'), 'purchasing/manage/matching_config.php?', 'SA_PURCHMATCHCONFIG', MENU_ENTRY);
		$this->add_lapp_function(2, _('Purchasing Reorder R&ules'), 'purchasing/manage/reorder_rules.php?', 'SA_REORDERRULES', MENU_ENTRY);
		$this->add_rapp_function(2, _('Vendor Evaluation &Criteria'), 'purchasing/manage/vendor_evaluation_criteria.php?', 'SA_VENDOREVALUATION', MENU_ENTRY);
		$this->add_rapp_function(2, _('Purchase Order T&emplates'), 'purchasing/manage/purch_templates.php?', 'SA_PURCHTEMPLATE', MENU_ENTRY);

		$this->add_extensions();
	}
}
