<?php

// Install-time foreign keys are managed in PHP after schema/data import.
// Matching comments are kept next to the affected CREATE TABLE blocks in
// en_US-new.sql and en_US-demo.sql so future changes update both places.

$install_constraint_queries = array(
	"ALTER TABLE `0_purch_requisition_lines` ADD CONSTRAINT `fk_purch_req_line_header` FOREIGN KEY (`requisition_id`) REFERENCES `0_purch_requisitions` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_purch_rfq_items` ADD CONSTRAINT `fk_purch_rfq_item_header` FOREIGN KEY (`rfq_id`) REFERENCES `0_purch_rfq` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_purch_rfq_vendors` ADD CONSTRAINT `fk_purch_rfq_vendor_header` FOREIGN KEY (`rfq_id`) REFERENCES `0_purch_rfq` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_purch_rfq_vendor_lines` ADD CONSTRAINT `fk_purch_rfq_vendor_line_vendor` FOREIGN KEY (`rfq_vendor_id`) REFERENCES `0_purch_rfq_vendors` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_purch_rfq_vendor_lines` ADD CONSTRAINT `fk_purch_rfq_vendor_line_item` FOREIGN KEY (`rfq_item_id`) REFERENCES `0_purch_rfq_items` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_purch_agreement_lines` ADD CONSTRAINT `fk_purch_agreement_lines_header` FOREIGN KEY (`agreement_id`) REFERENCES `0_purch_agreements` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_vendor_evaluation_scores` ADD CONSTRAINT `fk_vendor_eval_scores_eval` FOREIGN KEY (`evaluation_id`) REFERENCES `0_vendor_evaluations` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_vendor_evaluation_scores` ADD CONSTRAINT `fk_vendor_eval_scores_criteria` FOREIGN KEY (`criteria_id`) REFERENCES `0_vendor_evaluation_criteria` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_purch_order_template_lines` ADD CONSTRAINT `fk_purch_template_lines_header` FOREIGN KEY (`template_id`) REFERENCES `0_purch_order_templates` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_procurement_plan_lines` ADD CONSTRAINT `fk_procurement_plan_lines_header` FOREIGN KEY (`plan_id`) REFERENCES `0_procurement_plan` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_quality_inspection_readings` ADD CONSTRAINT `fk_qir_inspection` FOREIGN KEY (`inspection_id`) REFERENCES `0_quality_inspections` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_quality_inspection_readings` ADD CONSTRAINT `fk_qir_parameter` FOREIGN KEY (`parameter_id`) REFERENCES `0_quality_parameters` (`id`)",
	"ALTER TABLE `0_recall_items` ADD CONSTRAINT `fk_ri_recall` FOREIGN KEY (`recall_id`) REFERENCES `0_recall_campaigns` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_sales_agreement_lines` ADD CONSTRAINT `fk_sales_agreement_lines_header` FOREIGN KEY (`agreement_id`) REFERENCES `0_sales_agreements` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_warranty_claim_parts` ADD CONSTRAINT `fk_wcp_claim` FOREIGN KEY (`claim_id`) REFERENCES `0_warranty_claims` (`id`) ON DELETE CASCADE",
	"ALTER TABLE `0_wh_bin_stock` ADD CONSTRAINT `fk_bin_stock_item` FOREIGN KEY (`stock_id`) REFERENCES `0_stock_master` (`stock_id`)",
	"ALTER TABLE `0_wh_bin_stock` ADD CONSTRAINT `fk_bin_stock_loc` FOREIGN KEY (`wh_loc_id`) REFERENCES `0_wh_locations` (`loc_id`)",
	"ALTER TABLE `0_wh_cycle_count_lines` ADD CONSTRAINT `fk_cc_line_count` FOREIGN KEY (`count_id`) REFERENCES `0_wh_cycle_counts` (`count_id`) ON DELETE CASCADE",
	"ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_parent` FOREIGN KEY (`parent_loc_id`) REFERENCES `0_wh_locations` (`loc_id`)",
	"ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_storage_cat` FOREIGN KEY (`storage_category_id`) REFERENCES `0_wh_storage_categories` (`id`) ON DELETE SET NULL",
	"ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_type` FOREIGN KEY (`location_type_id`) REFERENCES `0_wh_location_types` (`id`)",
	"ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_warehouse` FOREIGN KEY (`warehouse_loc_code`) REFERENCES `0_locations` (`loc_code`)",
	"ALTER TABLE `0_wh_material_request_lines` ADD CONSTRAINT `fk_mr_line_req` FOREIGN KEY (`request_id`) REFERENCES `0_wh_material_requests` (`request_id`) ON DELETE CASCADE",
	"ALTER TABLE `0_wh_operation_lines` ADD CONSTRAINT `fk_op_line_op` FOREIGN KEY (`op_id`) REFERENCES `0_wh_operations` (`op_id`) ON DELETE CASCADE",
	"ALTER TABLE `0_wh_package_contents` ADD CONSTRAINT `fk_pkg_content_pkg` FOREIGN KEY (`package_id`) REFERENCES `0_wh_packages` (`package_id`) ON DELETE CASCADE",
	"ALTER TABLE `0_wh_return_order_lines` ADD CONSTRAINT `fk_ret_line_ret` FOREIGN KEY (`return_id`) REFERENCES `0_wh_return_orders` (`return_id`) ON DELETE CASCADE",
	"ALTER TABLE `0_wh_route_rules` ADD CONSTRAINT `fk_rule_route` FOREIGN KEY (`route_id`) REFERENCES `0_wh_routes` (`route_id`) ON DELETE CASCADE",
	"ALTER TABLE `0_wh_transfer_order_lines` ADD CONSTRAINT `fk_to_line_transfer` FOREIGN KEY (`transfer_id`) REFERENCES `0_wh_transfer_orders` (`transfer_id`) ON DELETE CASCADE"
);

return array(
	'en_US-new.sql' => $install_constraint_queries,
	'en_US-demo.sql' => $install_constraint_queries,
);