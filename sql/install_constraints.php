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
	"ALTER TABLE `0_procurement_plan_lines` ADD CONSTRAINT `fk_procurement_plan_lines_header` FOREIGN KEY (`plan_id`) REFERENCES `0_procurement_plan` (`id`) ON DELETE CASCADE"
);

return array(
	'en_US-new.sql' => $install_constraint_queries,
	'en_US-demo.sql' => $install_constraint_queries,
);