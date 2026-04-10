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
class InventoryApp extends application {
	function __construct() {
		parent::__construct('stock', _($this->help_context = '&Inventory'));

		$this->add_module(_('Transactions'));
		$this->add_lapp_function(0, _('Inventory Location &Transfers'), 'inventory/transfers.php?NewTransfer=1', 'SA_LOCATIONTRANSFER', MENU_TRANSACTION);
		$this->add_lapp_function(0, _('Inventory &Adjustments'), 'inventory/adjustments.php?NewAdjustment=1', 'SA_INVENTORYADJUSTMENT', MENU_TRANSACTION);

		$this->add_module(_('Inquiries and Reports'));
		$this->add_lapp_function(1, _('Inventory Item &Movements'), 'inventory/inquiry/stock_movements.php?', 'SA_ITEMSTRANSVIEW', MENU_INQUIRY);
		$this->add_lapp_function(1, _('Inventory Item &Status'), 'inventory/inquiry/stock_status.php?', 'SA_ITEMSSTATVIEW', MENU_INQUIRY);
		$this->add_rapp_function(1, _('Inventory &Reports'), 'reporting/reports_main.php?Class=2', 'SA_ITEMSTRANSVIEW', MENU_REPORT);

		$this->add_module(_('Maintenance'));
		$this->add_lapp_function(2, _('&Items'), 'inventory/manage/items.php?', 'SA_ITEM', MENU_ENTRY);
		$this->add_lapp_function(2, _('&Foreign Item Codes'), 'inventory/manage/item_codes.php?', 'SA_FORITEMCODE', MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Sales &Kits'), 'inventory/manage/sales_kits.php?', 'SA_SALESKIT', MENU_MAINTENANCE);
		$this->add_lapp_function(2, _('Item &Categories'), 'inventory/manage/item_categories.php?', 'SA_ITEMCATEGORY', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Inventory &Locations'), 'inventory/manage/locations.php?', 'SA_INVENTORYLOCATION', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('&Units of Measure'), 'inventory/manage/item_units.php?', 'SA_UOM', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('&Reorder Levels'), 'inventory/reorder_level.php?', 'SA_REORDER', MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('&Tracking Settings'), 'inventory/manage/tracking_settings.php?', 'SA_TRACKINGSETTINGS', MENU_MAINTENANCE);

		$this->add_module(_('Item Tracking'));
		$this->add_lapp_function(3, _('&Serial Numbers'), 'inventory/manage/serial_numbers.php?', 'SA_SERIALNUMBER', MENU_MAINTENANCE);
		$this->add_lapp_function(3, _('&Batch / Lot Numbers'), 'inventory/manage/stock_batches.php?', 'SA_BATCHNUMBER', MENU_MAINTENANCE);
		$this->add_lapp_function(3, _('Quality &Parameters'), 'inventory/manage/quality_parameters.php?', 'SA_QC_PARAMETERS', MENU_MAINTENANCE);
		$this->add_rapp_function(3, _('Serial &Inquiry'), 'inventory/inquiry/serial_inquiry.php?', 'SA_SERIALINQUIRY', MENU_INQUIRY);
		$this->add_rapp_function(3, _('Batch &Inquiry'), 'inventory/inquiry/batch_inquiry.php?', 'SA_BATCHINQUIRY', MENU_INQUIRY);
		$this->add_rapp_function(3, _('&Expiry Dashboard'), 'inventory/inquiry/expiry_dashboard.php?', 'SA_BATCHINQUIRY', MENU_INQUIRY);
		$this->add_rapp_function(3, _('&Quality Inspections'), 'inventory/quality_inspection.php?', 'SA_QC_INSPECTIONS', MENU_TRANSACTION);
		$this->add_lapp_function(3, _('Barcode &Labels'), 'inventory/manage/barcode_labels.php?', 'SA_BARCODELABELS', MENU_MAINTENANCE);
		$this->add_lapp_function(3, _('&Warranty Claims'), 'inventory/manage/warranty_claims.php?', 'SA_WARRANTY', MENU_TRANSACTION);
		$this->add_rapp_function(3, _('Recall &Campaigns'), 'inventory/manage/recall_campaigns.php?', 'SA_RECALL', MENU_TRANSACTION);
		$this->add_lapp_function(3, _('Serial &Lifecycle'), 'inventory/inquiry/serial_lifecycle.php?', 'SA_TRACEABILITY', MENU_INQUIRY);
		$this->add_rapp_function(3, _('Batch Li&fecycle'), 'inventory/inquiry/batch_lifecycle.php?', 'SA_TRACEABILITY', MENU_INQUIRY);
		$this->add_lapp_function(3, _('Customer &Equipment'), 'inventory/inquiry/customer_equipment.php?', 'SA_CUSTOMER_EQUIPMENT', MENU_INQUIRY);
		$this->add_rapp_function(3, _('Warranty &Provision'), 'inventory/inquiry/warranty_provision_inquiry.php?', 'SA_WARRANTY_PROVISION', MENU_INQUIRY);
		$this->add_rapp_function(3, _('Provision Se&ttings'), 'inventory/manage/warranty_provision_settings.php?', 'SA_TRACKINGSETTINGS', MENU_MAINTENANCE);
		$this->add_rapp_function(3, _('Re&gulatory Compliance'), 'inventory/manage/regulatory_compliance.php?', 'SA_REGULATORY', MENU_MAINTENANCE);

		$this->add_module(_('Warehouse Management'));
		$this->add_lapp_function(4, _('Warehouse &Dashboard'), 'inventory/warehouse/dashboard.php?', 'SA_WAREHOUSE_DASHBOARD', MENU_INQUIRY);
		$this->add_lapp_function(4, _('Warehouse &Locations'), 'inventory/warehouse/locations.php?', 'SA_WAREHOUSE_SETUP', MENU_MAINTENANCE);
		$this->add_lapp_function(4, _('&Storage Categories'), 'inventory/warehouse/storage_categories.php?', 'SA_WAREHOUSE_STORAGE', MENU_MAINTENANCE);
		$this->add_lapp_function(4, _('&Putaway Rules'), 'inventory/warehouse/putaway_rules.php?', 'SA_WAREHOUSE_PUTAWAY', MENU_MAINTENANCE);
		$this->add_lapp_function(4, _('&Removal Strategies'), 'inventory/warehouse/removal_strategies.php?', 'SA_WAREHOUSE_REMOVAL', MENU_MAINTENANCE);
		$this->add_rapp_function(4, _('ABC &Classification'), 'inventory/warehouse/abc_analysis.php?', 'SA_ABC_ANALYSIS', MENU_INQUIRY);
		$this->add_rapp_function(4, _('Receipt &Operations'), 'inventory/warehouse/receipt_operations.php?', 'SA_WAREHOUSE_OPERATIONS', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('Dispatch &Operations'), 'inventory/warehouse/dispatch_operations.php?', 'SA_DISPATCH_OPERATIONS', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('Transfer &Orders'), 'inventory/warehouse/transfer_orders.php?', 'SA_TRANSFERORDERS', MENU_TRANSACTION);
		$this->add_lapp_function(4, _('&Routes && Rules'), 'inventory/warehouse/routes.php?', 'SA_WAREHOUSE_ROUTES', MENU_MAINTENANCE);
		$this->add_rapp_function(4, _('&Picking Waves'), 'inventory/warehouse/picking.php?', 'SA_WAREHOUSE_PICKING', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('Pac&king Station'), 'inventory/warehouse/packing.php?', 'SA_WAREHOUSE_PACKING', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('&Shipping'), 'inventory/warehouse/shipping.php?', 'SA_WAREHOUSE_SHIPPING', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('C&ycle Counts'), 'inventory/warehouse/cycle_counts.php?', 'SA_WAREHOUSE_CYCLE_COUNT', MENU_TRANSACTION);
		$this->add_lapp_function(4, _('&Replenishment Engine'), 'inventory/warehouse/replenishment.php?', 'SA_WAREHOUSE_REPLENISHMENT', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('&Material Requests'), 'inventory/warehouse/material_requests.php?', 'SA_MATERIALREQUEST', MENU_TRANSACTION);
		$this->add_lapp_function(4, _('&Scrap Entry'), 'inventory/warehouse/scrap.php?', 'SA_WAREHOUSE_SCRAP', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('&Return Orders'), 'inventory/warehouse/returns.php?', 'SA_WAREHOUSE_RETURNS', MENU_TRANSACTION);
		$this->add_lapp_function(4, _('Cross-&Dock / Drop-Ship'), 'inventory/warehouse/crossdock_dropship.php?', 'SA_CROSSDOCK_DROPSHIP', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('Consi&gnment && VMI'), 'inventory/warehouse/consignment.php?', 'SA_CONSIGNMENT', MENU_TRANSACTION);
		$this->add_rapp_function(4, _('&Mobile Scanner'), 'inventory/mobile/', 'SA_OPEN', MENU_TRANSACTION);

		$this->add_module(_('Pricing and Costs'));
		$this->add_lapp_function(5, _('Sales &Pricing'), 'inventory/prices.php?', 'SA_SALESPRICE', MENU_MAINTENANCE);
		$this->add_lapp_function(5, _('Purchasing &Pricing'), 'inventory/purchasing_data.php?', 'SA_PURCHASEPRICING', MENU_MAINTENANCE);
		$this->add_rapp_function(5, _('Standard &Costs'), 'inventory/cost_update.php?', 'SA_STANDARDCOST', MENU_MAINTENANCE);

		$this->add_extensions();
	}
}
