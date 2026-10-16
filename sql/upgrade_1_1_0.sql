-- NotrinosERP 1.0.0 -> 1.1.0 release upgrade.
-- Baseline: 3ce219018583085f401faa9d77bdaf78d9b6b075
-- Source: afb872fa9b0391d0374119c9d241020393d5de30
-- Additive schema only; existing business rows are preserved.
-- Run through Setup > Software Upgrade, not as a fresh-install database.

CREATE TABLE `0_domain_audit_chain_state` (
  `chain_scope` varchar(96) NOT NULL,
  `last_sequence` bigint unsigned NOT NULL DEFAULT '0',
  `last_event_hash` char(64) NOT NULL,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`chain_scope`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `0_domain_audit_events` (
  `event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `chain_scope` varchar(96) NOT NULL,
  `chain_sequence` bigint unsigned NOT NULL,
  `occurred_at` datetime NOT NULL,
  `actor_user_id` smallint unsigned NOT NULL DEFAULT '0',
  `security_context_hash` char(64) NOT NULL,
  `company_id` int unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL DEFAULT '0',
  `entity_type` varchar(64) NOT NULL,
  `entity_key_hash` char(64) NOT NULL,
  `action` varchar(64) NOT NULL,
  `reason_code` varchar(64) NOT NULL,
  `correlation_id` char(32) NOT NULL,
  `legacy_source` varchar(64) NOT NULL DEFAULT '',
  `legacy_id` bigint unsigned NOT NULL DEFAULT '0',
  `before_hash` char(64) DEFAULT NULL,
  `after_hash` char(64) DEFAULT NULL,
  `restricted_diff` text NOT NULL,
  `previous_event_hash` char(64) NOT NULL,
  `event_hash` char(64) NOT NULL,
  `key_id` char(16) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`event_id`),
  UNIQUE KEY `domain_audit_chain_sequence_uq` (`chain_scope`,`chain_sequence`),
  UNIQUE KEY `domain_audit_event_hash_uq` (`event_hash`),
  KEY `domain_audit_actor_time_idx` (`actor_user_id`,`occurred_at`),
  KEY `domain_audit_entity_time_idx` (`entity_type`,`entity_key_hash`,`occurred_at`),
  KEY `domain_audit_correlation_idx` (`correlation_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `0_domain_audit_outbox` (
  `outbox_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `event_id` bigint unsigned NOT NULL,
  `chain_scope` varchar(96) NOT NULL,
  `chain_sequence` bigint unsigned NOT NULL,
  `payload` longtext NOT NULL,
  `payload_hash` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`outbox_id`),
  UNIQUE KEY `domain_audit_outbox_event_uq` (`event_id`),
  UNIQUE KEY `domain_audit_outbox_chain_uq` (`chain_scope`,`chain_sequence`),
  KEY `domain_audit_outbox_created_idx` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `0_domain_audit_checkpoints` (
  `checkpoint_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `chain_scope` varchar(96) NOT NULL,
  `chain_sequence` bigint unsigned NOT NULL,
  `event_hash` char(64) NOT NULL,
  `event_count` bigint unsigned NOT NULL,
  `key_id` char(16) NOT NULL,
  `signature` char(64) NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`checkpoint_id`),
  UNIQUE KEY `domain_audit_checkpoint_chain_uq` (`chain_scope`,`chain_sequence`),
  KEY `domain_audit_checkpoint_created_idx` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `0_domain_audit_export_receipts` (
  `receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `company_id` int unsigned NOT NULL,
  `chain_scope` varchar(96) NOT NULL,
  `sequence_start` bigint unsigned NOT NULL,
  `sequence_end` bigint unsigned NOT NULL,
  `event_count` int unsigned NOT NULL,
  `package_hash` char(64) NOT NULL,
  `checkpoint_event_hash` char(64) NOT NULL,
  `previous_receipt_hash` char(64) NOT NULL,
  `custody_provider` varchar(64) NOT NULL,
  `custody_object_token` char(64) NOT NULL,
  `custody_key_id` char(64) NOT NULL,
  `received_at` datetime NOT NULL,
  `retained_until` date NOT NULL,
  `receipt_payload` text NOT NULL,
  `receipt_signature` varchar(1024) NOT NULL,
  `receipt_hash` char(64) NOT NULL,
  `recorded_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`receipt_id`),
  UNIQUE KEY `domain_audit_receipt_start_uq` (`chain_scope`,`sequence_start`),
  UNIQUE KEY `domain_audit_receipt_end_uq` (`chain_scope`,`sequence_end`),
  UNIQUE KEY `domain_audit_receipt_hash_uq` (`receipt_hash`),
  UNIQUE KEY `domain_audit_receipt_object_uq` (`custody_provider`,`custody_object_token`),
  KEY `domain_audit_receipt_recorded_idx` (`recorded_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE `0_hrm_persons` (
	`person_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_status` varchar(16) NOT NULL DEFAULT 'active',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`person_id`),
	KEY `hrm_person_status_idx` (`person_status`,`person_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_names` (
	`name_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`first_name` varchar(100) NOT NULL,
	`middle_name` varchar(100) NOT NULL DEFAULT '',
	`last_name` varchar(100) NOT NULL,
	`effective_from` datetime DEFAULT NULL,
	`effective_to` datetime DEFAULT NULL,
	`source` varchar(32) NOT NULL,
	`verification_status` varchar(16) NOT NULL DEFAULT 'unverified',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`name_id`),
	KEY `hrm_person_name_asof_idx` (`person_id`,`effective_from`,`effective_to`,`name_id`),
	CONSTRAINT `0_hrm_person_names_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_contacts` (
	`contact_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`contact_type` varchar(32) NOT NULL,
	`value_ciphertext` varbinary(384) NOT NULL,
	`masked_value` varchar(64) NOT NULL,
	`lookup_token` char(64) NOT NULL,
	`key_id` char(16) NOT NULL,
	`is_primary` tinyint(1) NOT NULL DEFAULT '0',
	`effective_from` datetime DEFAULT NULL,
	`effective_to` datetime DEFAULT NULL,
	`source` varchar(32) NOT NULL,
	`verification_status` varchar(16) NOT NULL DEFAULT 'unverified',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`contact_id`),
	KEY `hrm_person_contact_asof_idx` (`person_id`,`contact_type`,`effective_from`,`effective_to`,`contact_id`),
	KEY `hrm_person_contact_token_idx` (`contact_type`,`lookup_token`,`contact_id`),
	CONSTRAINT `0_hrm_person_contacts_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_identifiers` (
	`identifier_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`identifier_type` varchar(32) NOT NULL,
	`issuing_jurisdiction` varchar(16) NOT NULL DEFAULT '',
	`value_ciphertext` varbinary(384) NOT NULL,
	`masked_value` varchar(64) NOT NULL,
	`lookup_token` char(64) NOT NULL,
	`key_id` char(16) NOT NULL,
	`valid_to` date DEFAULT NULL,
	`effective_from` datetime DEFAULT NULL,
	`effective_to` datetime DEFAULT NULL,
	`source` varchar(32) NOT NULL,
	`verification_status` varchar(16) NOT NULL DEFAULT 'unverified',
	`verification_method` varchar(32) NOT NULL DEFAULT '',
	`approval_status` varchar(16) NOT NULL DEFAULT 'unapproved',
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`supersedes_identifier_id` bigint(20) unsigned DEFAULT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`identifier_id`),
	KEY `hrm_person_identifier_asof_idx` (`person_id`,`identifier_type`,`effective_from`,`effective_to`,`identifier_id`),
	KEY `hrm_person_identifier_token_idx` (`identifier_type`,`lookup_token`,`identifier_id`),
	UNIQUE KEY `hrm_person_identifier_supersedes_uq` (`supersedes_identifier_id`),
	CONSTRAINT `0_hrm_person_identifiers_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_identifiers_supersedes_fk` FOREIGN KEY (`supersedes_identifier_id`) REFERENCES `0_hrm_person_identifiers` (`identifier_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_identifier_verification_requests` (
	`verification_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`identifier_id` bigint(20) unsigned NOT NULL,
	`identifier_row_version` int(10) unsigned NOT NULL,
	`verification_method` varchar(32) NOT NULL,
	`request_status` varchar(16) NOT NULL DEFAULT 'pending',
	`request_row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`requested_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`requested_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`verification_request_id`),
	KEY `hrm_person_identifier_verification_identifier_idx` (`identifier_id`,`identifier_row_version`,`verification_request_id`),
	KEY `hrm_person_identifier_verification_status_idx` (`request_status`,`verification_request_id`),
	CONSTRAINT `0_hrm_person_identifier_verification_identifier_fk` FOREIGN KEY (`identifier_id`) REFERENCES `0_hrm_person_identifiers` (`identifier_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_identifier_verification_evidence` (
	`verification_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`verification_request_id` bigint(20) unsigned NOT NULL,
	`identifier_id` bigint(20) unsigned NOT NULL,
	`identifier_row_version` int(10) unsigned NOT NULL,
	`request_row_version` int(10) unsigned NOT NULL,
	`approval_draft_id` int(11) NOT NULL,
	`verification_method` varchar(32) NOT NULL,
	`evidence_source` varchar(32) NOT NULL,
	`evidence_reference_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`evidence_payload_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`checker_assurance_event_id` bigint(20) unsigned NOT NULL,
	`verified_at` datetime NOT NULL,
	`evidence_row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`recorded_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`recorded_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`verification_evidence_id`),
	UNIQUE KEY `hrm_person_identifier_verification_evidence_request_uq` (`verification_request_id`),
	KEY `hrm_person_identifier_verification_evidence_identifier_idx` (`identifier_id`,`identifier_row_version`,`verification_evidence_id`),
	KEY `hrm_person_identifier_verification_evidence_verified_idx` (`verified_at`,`verification_evidence_id`),
	KEY `hrm_person_identifier_verification_evidence_approval_idx` (`approval_draft_id`,`verification_evidence_id`),
	KEY `hrm_person_identifier_verification_evidence_assurance_idx` (`checker_assurance_event_id`,`verification_evidence_id`),
	CONSTRAINT `0_hrm_person_identifier_verification_evidence_request_fk` FOREIGN KEY (`verification_request_id`) REFERENCES `0_hrm_person_identifier_verification_requests` (`verification_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_bank_accounts` (
	`person_bank_account_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`details_ciphertext` varbinary(1280) NOT NULL,
	`masked_account` varchar(64) NOT NULL DEFAULT '',
	`masked_routing` varchar(64) NOT NULL DEFAULT '',
	`account_lookup_token` char(64) NOT NULL DEFAULT '',
	`key_id` char(16) NOT NULL,
	`payment_method` tinyint(3) unsigned NOT NULL DEFAULT '0',
	`verification_status` varchar(16) NOT NULL DEFAULT 'unverified',
	`verification_method` varchar(32) NOT NULL DEFAULT '',
	`approval_status` varchar(16) NOT NULL DEFAULT 'unapproved',
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`supersedes_person_bank_account_id` bigint(20) unsigned DEFAULT NULL,
	`effective_from` datetime DEFAULT NULL,
	`effective_to` datetime DEFAULT NULL,
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`person_bank_account_id`),
	KEY `hrm_person_bank_asof_idx` (`person_id`,`effective_from`,`effective_to`,`person_bank_account_id`),
	KEY `hrm_person_bank_token_idx` (`account_lookup_token`,`person_bank_account_id`),
	UNIQUE KEY `hrm_person_bank_supersedes_uq` (`supersedes_person_bank_account_id`),
	CONSTRAINT `0_hrm_person_bank_accounts_supersedes_fk` FOREIGN KEY (`supersedes_person_bank_account_id`) REFERENCES `0_hrm_person_bank_accounts` (`person_bank_account_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_bank_accounts_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_bank_account_verification_requests` (
	`verification_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_bank_account_id` bigint(20) unsigned NOT NULL,
	`bank_account_row_version` int(10) unsigned NOT NULL,
	`verification_method` varchar(32) NOT NULL,
	`request_status` varchar(16) NOT NULL DEFAULT 'pending',
	`request_row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`requested_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`requested_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`verification_request_id`),
	KEY `hrm_person_bank_verification_account_idx` (`person_bank_account_id`,`bank_account_row_version`,`verification_request_id`),
	KEY `hrm_person_bank_verification_status_idx` (`request_status`,`verification_request_id`),
	CONSTRAINT `0_hrm_person_bank_verification_account_fk` FOREIGN KEY (`person_bank_account_id`) REFERENCES `0_hrm_person_bank_accounts` (`person_bank_account_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_bank_account_verification_evidence` (
	`verification_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`verification_request_id` bigint(20) unsigned NOT NULL,
	`person_bank_account_id` bigint(20) unsigned NOT NULL,
	`bank_account_row_version` int(10) unsigned NOT NULL,
	`request_row_version` int(10) unsigned NOT NULL,
	`approval_draft_id` int(11) NOT NULL,
	`verification_method` varchar(32) NOT NULL,
	`evidence_source` varchar(32) NOT NULL,
	`evidence_reference_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`evidence_payload_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`checker_assurance_event_id` bigint(20) unsigned NOT NULL,
	`verified_at` datetime NOT NULL,
	`evidence_row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`recorded_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`recorded_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`verification_evidence_id`),
	UNIQUE KEY `hrm_person_bank_verification_evidence_request_uq` (`verification_request_id`),
	KEY `hrm_person_bank_verification_evidence_account_idx` (`person_bank_account_id`,`bank_account_row_version`,`verification_evidence_id`),
	KEY `hrm_person_bank_verification_evidence_verified_idx` (`verified_at`,`verification_evidence_id`),
	KEY `hrm_person_bank_verification_evidence_approval_idx` (`approval_draft_id`,`verification_evidence_id`),
	KEY `hrm_person_bank_verification_evidence_assurance_idx` (`checker_assurance_event_id`,`verification_evidence_id`),
	CONSTRAINT `0_hrm_person_bank_verification_evidence_request_fk` FOREIGN KEY (`verification_request_id`) REFERENCES `0_hrm_person_bank_account_verification_requests` (`verification_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_bank_payment_elections` (
	`payment_election_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`election_status` varchar(16) NOT NULL DEFAULT 'draft',
	`approval_status` varchar(16) NOT NULL DEFAULT 'unapproved',
	`approval_draft_id` int(11) DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`supersedes_payment_election_id` bigint(20) unsigned DEFAULT NULL,
	`effective_from` datetime DEFAULT NULL,
	`effective_to` datetime DEFAULT NULL,
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`payment_election_id`),
	KEY `hrm_bank_payment_election_asof_idx` (`person_id`,`currency_code`,`effective_from`,`effective_to`,`payment_election_id`),
	KEY `hrm_bank_payment_election_status_idx` (`election_status`,`approval_status`,`payment_election_id`),
	KEY `hrm_bank_payment_election_approval_idx` (`approval_draft_id`,`payment_election_id`),
	UNIQUE KEY `hrm_bank_payment_election_supersedes_uq` (`supersedes_payment_election_id`),
	CONSTRAINT `0_hrm_bank_payment_election_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_bank_payment_election_supersedes_fk` FOREIGN KEY (`supersedes_payment_election_id`) REFERENCES `0_hrm_person_bank_payment_elections` (`payment_election_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_bank_payment_allocations` (
	`payment_allocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`payment_election_id` bigint(20) unsigned NOT NULL,
	`person_bank_account_id` bigint(20) unsigned NOT NULL,
	`bank_account_row_version` int(10) unsigned NOT NULL,
	`allocation_type` varchar(16) NOT NULL DEFAULT 'percentage',
	`allocation_value` decimal(20,6) NOT NULL DEFAULT '0.000000',
	`priority` smallint(5) unsigned NOT NULL DEFAULT '1',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`payment_allocation_id`),
	UNIQUE KEY `hrm_bank_payment_allocation_priority_uq` (`payment_election_id`,`priority`),
	UNIQUE KEY `hrm_bank_payment_allocation_account_uq` (`payment_election_id`,`person_bank_account_id`,`bank_account_row_version`),
	KEY `hrm_bank_payment_allocation_bank_idx` (`person_bank_account_id`,`bank_account_row_version`,`payment_allocation_id`),
	CONSTRAINT `0_hrm_bank_payment_allocation_election_fk` FOREIGN KEY (`payment_election_id`) REFERENCES `0_hrm_person_bank_payment_elections` (`payment_election_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_bank_payment_allocation_bank_fk` FOREIGN KEY (`person_bank_account_id`) REFERENCES `0_hrm_person_bank_accounts` (`person_bank_account_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_addresses` (
	`address_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`address_type` varchar(24) NOT NULL DEFAULT 'residential',
	`address_line` varchar(255) NOT NULL DEFAULT '',
	`city` varchar(60) NOT NULL DEFAULT '',
	`region` varchar(60) NOT NULL DEFAULT '',
	`country` varchar(60) NOT NULL DEFAULT '',
	`postal_code` varchar(32) NOT NULL DEFAULT '',
	`is_residence` tinyint(1) NOT NULL DEFAULT '1',
	`is_tax_residence` tinyint(1) NOT NULL DEFAULT '0',
	`effective_from` datetime DEFAULT NULL,
	`effective_to` datetime DEFAULT NULL,
	`source` varchar(32) NOT NULL,
	`verification_status` varchar(16) NOT NULL DEFAULT 'unverified',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`address_id`),
	KEY `hrm_person_address_asof_idx` (`person_id`,`address_type`,`effective_from`,`effective_to`,`address_id`),
	KEY `hrm_person_address_jurisdiction_idx` (`country`,`region`,`person_id`,`address_id`),
	CONSTRAINT `0_hrm_person_addresses_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_emergency_contacts` (
	`emergency_contact_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`contact_name` varchar(100) NOT NULL DEFAULT '',
	`relationship` varchar(60) NOT NULL DEFAULT '',
	`phone` varchar(30) NOT NULL DEFAULT '',
	`is_primary` tinyint(1) NOT NULL DEFAULT '1',
	`consent_status` varchar(16) NOT NULL DEFAULT 'unknown',
	`effective_from` datetime DEFAULT NULL,
	`effective_to` datetime DEFAULT NULL,
	`source` varchar(32) NOT NULL,
	`verification_status` varchar(16) NOT NULL DEFAULT 'unverified',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`emergency_contact_id`),
	KEY `hrm_person_emergency_asof_idx` (`person_id`,`is_primary`,`effective_from`,`effective_to`,`emergency_contact_id`),
	CONSTRAINT `0_hrm_person_emergency_contacts_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_duplicate_reviews` (
	`review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id_low` bigint(20) unsigned NOT NULL,
	`person_id_high` bigint(20) unsigned NOT NULL,
	`candidate_fingerprint` char(64) NOT NULL,
	`disposition` varchar(32) NOT NULL,
	`reason_code` varchar(64) NOT NULL,
	`reviewed_at` datetime NOT NULL,
	`reviewed_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`predecessor_review_id` bigint(20) unsigned DEFAULT NULL,
	`decision_key` char(64) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`review_id`),
	KEY `hrm_person_dup_review_lookup_idx` (`person_id_low`,`person_id_high`,`candidate_fingerprint`,`review_id`),
	KEY `hrm_person_dup_review_disposition_idx` (`disposition`,`reviewed_at`,`review_id`),
	UNIQUE KEY `hrm_person_dup_review_decision_uq` (`decision_key`),
	KEY `hrm_person_dup_review_predecessor_idx` (`predecessor_review_id`),
	CONSTRAINT `0_hrm_person_dup_review_low_fk` FOREIGN KEY (`person_id_low`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_dup_review_high_fk` FOREIGN KEY (`person_id_high`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_dup_review_predecessor_fk` FOREIGN KEY (`predecessor_review_id`) REFERENCES `0_hrm_person_duplicate_reviews` (`review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_link_proposals` (
	`proposal_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`review_id` bigint(20) unsigned NOT NULL,
	`person_id_low` bigint(20) unsigned NOT NULL,
	`person_id_high` bigint(20) unsigned NOT NULL,
	`proposal_type` varchar(32) NOT NULL,
	`proposed_at` datetime NOT NULL,
	`proposed_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`proposal_key` char(64) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`proposal_id`),
	UNIQUE KEY `hrm_person_link_prop_review_uq` (`review_id`,`proposal_type`),
	KEY `hrm_person_link_prop_pair_idx` (`person_id_low`,`person_id_high`,`proposal_id`),
	KEY `hrm_person_link_prop_type_idx` (`proposal_type`,`proposed_at`,`proposal_id`),
	UNIQUE KEY `hrm_person_link_prop_key_uq` (`proposal_key`),
	CONSTRAINT `0_hrm_person_link_prop_review_fk` FOREIGN KEY (`review_id`) REFERENCES `0_hrm_person_duplicate_reviews` (`review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_link_prop_low_fk` FOREIGN KEY (`person_id_low`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_link_prop_high_fk` FOREIGN KEY (`person_id_high`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_workers` (
	`worker_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`person_id` bigint(20) unsigned NOT NULL,
	`worker_number` varchar(20) NOT NULL,
	`active_from` date DEFAULT NULL,
	`active_to` date DEFAULT NULL,
	`worker_status` varchar(16) NOT NULL DEFAULT 'active',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`worker_id`),
	UNIQUE KEY `hrm_worker_number_uq` (`worker_number`),
	KEY `hrm_worker_person_asof_idx` (`person_id`,`active_from`,`active_to`),
	CONSTRAINT `0_hrm_workers_person_fk` FOREIGN KEY (`person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_employee_worker_map` (
	`mapping_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`employee_number` int(11) NOT NULL,
	`employee_id` varchar(20) NOT NULL,
	`worker_id` bigint(20) unsigned NOT NULL,
	`migration_source` varchar(32) NOT NULL,
	`linked_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`retired_at` datetime DEFAULT NULL,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`mapping_id`),
	UNIQUE KEY `hrm_employee_worker_number_uq` (`employee_number`),
	UNIQUE KEY `hrm_employee_worker_code_uq` (`employee_id`),
	UNIQUE KEY `hrm_employee_worker_worker_uq` (`worker_id`),
	KEY `hrm_employee_worker_retired_idx` (`retired_at`,`mapping_id`),
	CONSTRAINT `0_hrm_employee_worker_map_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_legal_entities` (
	`legal_entity_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`entity_code` varchar(32) NOT NULL,
	`registered_name` varchar(100) NOT NULL,
	`registration_number` varchar(55) DEFAULT NULL,
	`tax_registration_number` varchar(55) DEFAULT NULL,
	`country_code` char(2) DEFAULT NULL,
	`base_currency` char(3) NOT NULL,
	`entity_status` varchar(16) NOT NULL DEFAULT 'active',
	`legacy_default` tinyint(1) DEFAULT '1',
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`legal_entity_id`),
	UNIQUE KEY `hrm_legal_entity_code_uq` (`entity_code`),
	UNIQUE KEY `hrm_legal_entity_legacy_default_uq` (`legacy_default`),
	KEY `hrm_legal_entity_status_idx` (`entity_status`,`legal_entity_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_grades` (
	`grade_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`legacy_grade_id` int(11) NOT NULL,
	`grade_code` varchar(32) NOT NULL,
	`legacy_grade_code` varchar(20) DEFAULT NULL,
	`grade_name` varchar(60) NOT NULL,
	`grade_level` int(11) NOT NULL DEFAULT '0',
	`grade_status` varchar(16) NOT NULL DEFAULT 'active',
	`reference_quality` varchar(32) NOT NULL DEFAULT 'legacy_pay_grade_only',
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`grade_id`),
	UNIQUE KEY `hrm_grade_legacy_uq` (`legacy_grade_id`),
	UNIQUE KEY `hrm_grade_entity_code_uq` (`legal_entity_id`,`grade_code`),
	KEY `hrm_grade_status_level_idx` (`legal_entity_id`,`grade_status`,`grade_level`,`grade_id`),
	CONSTRAINT `0_hrm_grades_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_jobs` (
	`job_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`job_code` varchar(32) NOT NULL,
	`job_name` varchar(120) NOT NULL,
	`job_status` varchar(16) NOT NULL DEFAULT 'active',
	`reference_quality` varchar(32) NOT NULL DEFAULT 'greenfield_normalized_only',
	`source` varchar(32) NOT NULL DEFAULT 'greenfield_normalized',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`job_id`),
	UNIQUE KEY `hrm_job_entity_code_uq` (`legal_entity_id`,`job_code`),
	KEY `hrm_job_status_idx` (`legal_entity_id`,`job_status`,`job_id`),
	CONSTRAINT `0_hrm_jobs_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_establishments` (
	`establishment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`establishment_code` varchar(32) NOT NULL,
	`establishment_name` varchar(120) NOT NULL,
	`establishment_status` varchar(16) NOT NULL DEFAULT 'active',
	`reference_quality` varchar(32) NOT NULL DEFAULT 'greenfield_normalized_only',
	`source` varchar(32) NOT NULL DEFAULT 'greenfield_normalized',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`establishment_id`),
	UNIQUE KEY `hrm_establishment_entity_code_uq` (`legal_entity_id`,`establishment_code`),
	KEY `hrm_establishment_status_idx` (`legal_entity_id`,`establishment_status`,`establishment_id`),
	CONSTRAINT `0_hrm_establishments_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_work_locations` (
	`work_location_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`establishment_id` bigint(20) unsigned DEFAULT NULL,
	`work_location_code` varchar(32) NOT NULL,
	`work_location_name` varchar(120) NOT NULL,
	`work_location_status` varchar(16) NOT NULL DEFAULT 'active',
	`reference_quality` varchar(32) NOT NULL DEFAULT 'greenfield_normalized_only',
	`source` varchar(32) NOT NULL DEFAULT 'greenfield_normalized',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`work_location_id`),
	UNIQUE KEY `hrm_work_location_entity_code_uq` (`legal_entity_id`,`work_location_code`),
	KEY `hrm_work_location_status_idx` (`legal_entity_id`,`work_location_status`,`work_location_id`),
	KEY `hrm_work_location_establishment_idx` (`establishment_id`,`work_location_status`,`work_location_id`),
	CONSTRAINT `0_hrm_work_locations_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_work_locations_establishment_fk` FOREIGN KEY (`establishment_id`) REFERENCES `0_hrm_establishments` (`establishment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_positions` (
	`position_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`job_id` bigint(20) unsigned DEFAULT NULL,
	`legacy_position_id` int(11) NOT NULL,
	`position_code` varchar(32) NOT NULL,
	`legacy_position_code` varchar(20) DEFAULT NULL,
	`position_name` text NOT NULL,
	`position_status` varchar(16) NOT NULL DEFAULT 'active',
	`reference_quality` varchar(32) NOT NULL DEFAULT 'legacy_position_identity_only',
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`position_id`),
	UNIQUE KEY `hrm_position_legacy_uq` (`legacy_position_id`),
	UNIQUE KEY `hrm_position_entity_code_uq` (`legal_entity_id`,`position_code`),
	KEY `hrm_position_status_idx` (`legal_entity_id`,`position_status`,`position_id`),
	KEY `hrm_position_job_idx` (`job_id`,`position_status`,`position_id`),
	CONSTRAINT `0_hrm_positions_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_positions_job_fk` FOREIGN KEY (`job_id`) REFERENCES `0_hrm_jobs` (`job_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_position_hierarchy` (
	`hierarchy_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`position_id` bigint(20) unsigned NOT NULL,
	`reports_to_position_id` bigint(20) unsigned NOT NULL,
	`effective_from` date NOT NULL,
	`effective_to` date DEFAULT NULL,
	`reference_quality` varchar(32) NOT NULL DEFAULT 'greenfield_explicit_only',
	`source` varchar(64) NOT NULL DEFAULT 'hrm_position_hierarchy_foundation',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`hierarchy_id`),
	UNIQUE KEY `hrm_position_hierarchy_start_uq` (`position_id`,`effective_from`),
	KEY `hrm_position_hierarchy_child_asof_idx` (`position_id`,`effective_from`,`effective_to`,`hierarchy_id`),
	KEY `hrm_position_hierarchy_parent_asof_idx` (`reports_to_position_id`,`effective_from`,`effective_to`,`hierarchy_id`),
	KEY `hrm_position_hierarchy_entity_idx` (`legal_entity_id`,`effective_from`,`effective_to`,`hierarchy_id`),
	CONSTRAINT `0_hrm_position_hierarchy_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_position_hierarchy_position_fk` FOREIGN KEY (`position_id`) REFERENCES `0_hrm_positions` (`position_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_position_hierarchy_reports_to_fk` FOREIGN KEY (`reports_to_position_id`) REFERENCES `0_hrm_positions` (`position_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_position_headcount_budgets` (
	`budget_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`position_id` bigint(20) unsigned NOT NULL,
	`budgeted_headcount` int(10) unsigned NOT NULL,
	`effective_from` date NOT NULL,
	`effective_to` date DEFAULT NULL,
	`reference_quality` varchar(32) NOT NULL DEFAULT 'greenfield_explicit_only',
	`source` varchar(64) NOT NULL DEFAULT 'hrm_position_headcount_foundation',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`budget_id`),
	UNIQUE KEY `hrm_position_headcount_start_uq` (`position_id`,`effective_from`),
	KEY `hrm_position_headcount_position_asof_idx` (`position_id`,`effective_from`,`effective_to`,`budget_id`),
	KEY `hrm_position_headcount_entity_asof_idx` (`legal_entity_id`,`effective_from`,`effective_to`,`budget_id`),
	CONSTRAINT `0_hrm_position_headcount_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_position_headcount_position_fk` FOREIGN KEY (`position_id`) REFERENCES `0_hrm_positions` (`position_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_organization_units` (
	`organization_unit_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`legacy_department_id` int(11) NOT NULL,
	`unit_code` varchar(32) NOT NULL,
	`legacy_department_code` varchar(20) DEFAULT NULL,
	`unit_name` text NOT NULL,
	`parent_organization_unit_id` bigint(20) unsigned DEFAULT NULL,
	`legacy_manager_employee_id` varchar(20) DEFAULT NULL,
	`legacy_cost_center_id` int(11) NOT NULL DEFAULT '0',
	`unit_status` varchar(16) NOT NULL DEFAULT 'active',
	`reference_quality` varchar(32) NOT NULL DEFAULT 'legacy_department_only',
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(5) unsigned DEFAULT NULL,
	PRIMARY KEY (`organization_unit_id`),
	UNIQUE KEY `hrm_org_unit_legacy_department_uq` (`legacy_department_id`),
	UNIQUE KEY `hrm_org_unit_entity_code_uq` (`legal_entity_id`,`unit_code`),
	KEY `hrm_org_unit_parent_idx` (`parent_organization_unit_id`,`organization_unit_id`),
	KEY `hrm_org_unit_status_idx` (`legal_entity_id`,`unit_status`,`organization_unit_id`),
	CONSTRAINT `0_hrm_org_units_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_org_units_parent_fk` FOREIGN KEY (`parent_organization_unit_id`) REFERENCES `0_hrm_organization_units` (`organization_unit_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_employments` (
	`employment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`worker_id` bigint(20) unsigned NOT NULL,
	`employment_sequence` int(10) unsigned NOT NULL,
	`legal_entity_id` bigint(20) unsigned NOT NULL,
	`legacy_employee_number` int(11) NOT NULL,
	`legacy_current` tinyint(1) DEFAULT '1',
	`start_date` date DEFAULT NULL,
	`end_date` date DEFAULT NULL,
	`employment_status` varchar(16) NOT NULL DEFAULT 'active',
	`termination_reason_code` varchar(32) DEFAULT NULL,
	`interval_quality` varchar(24) NOT NULL DEFAULT 'exact',
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(6) unsigned DEFAULT NULL,
	PRIMARY KEY (`employment_id`),
	UNIQUE KEY `hrm_employment_worker_sequence_uq` (`worker_id`,`employment_sequence`),
	UNIQUE KEY `hrm_employment_legacy_current_uq` (`legacy_employee_number`,`legacy_current`),
	KEY `hrm_employment_worker_asof_idx` (`worker_id`,`start_date`,`end_date`,`employment_id`),
	KEY `hrm_employment_status_idx` (`employment_status`,`end_date`,`employment_id`),
	KEY `hrm_employment_legal_entity_idx` (`legal_entity_id`,`employment_id`),
	CONSTRAINT `0_hrm_employments_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_employments_legal_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_contracts` (
	`contract_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`employment_id` bigint(20) unsigned NOT NULL,
	`contract_sequence` int(10) unsigned NOT NULL,
	`legacy_employee_number` int(11) NOT NULL,
	`legacy_current` tinyint(1) DEFAULT '1',
	`legacy_employment_type` smallint(6) NOT NULL,
	`contract_type_code` varchar(24) NOT NULL,
	`type_quality` varchar(24) NOT NULL DEFAULT 'exact',
	`effective_from` date DEFAULT NULL,
	`effective_to` date DEFAULT NULL,
	`interval_quality` varchar(24) NOT NULL DEFAULT 'exact',
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(6) unsigned DEFAULT NULL,
	PRIMARY KEY (`contract_id`),
	UNIQUE KEY `hrm_contract_employment_sequence_uq` (`employment_id`,`contract_sequence`),
	UNIQUE KEY `hrm_contract_legacy_current_uq` (`legacy_employee_number`,`legacy_current`),
	KEY `hrm_contract_employment_asof_idx` (`employment_id`,`effective_from`,`effective_to`,`contract_id`),
	KEY `hrm_contract_type_idx` (`contract_type_code`,`effective_to`,`contract_id`),
	CONSTRAINT `0_hrm_contracts_employment_fk` FOREIGN KEY (`employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_assignments` (
	`assignment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`employment_id` bigint(20) unsigned NOT NULL,
	`assignment_sequence` int(10) unsigned NOT NULL,
	`legacy_employee_number` int(11) NOT NULL,
	`legacy_current` tinyint(1) DEFAULT '1',
	`assignment_type_code` varchar(24) NOT NULL DEFAULT 'legacy_primary',
	`primary_flag` tinyint(1) NOT NULL DEFAULT '1',
	`legacy_department_id` int(11) NOT NULL DEFAULT '0',
	`organization_unit_id` bigint(20) unsigned DEFAULT NULL,
	`legacy_position_id` int(11) NOT NULL DEFAULT '0',
	`position_id` bigint(20) unsigned DEFAULT NULL,
	`job_id` bigint(20) unsigned DEFAULT NULL,
	`work_location_id` bigint(20) unsigned DEFAULT NULL,
	`manager_assignment_id` bigint(20) unsigned DEFAULT NULL,
	`legacy_grade_id` int(11) NOT NULL DEFAULT '0',
	`grade_id` bigint(20) unsigned DEFAULT NULL,
	`legacy_shift_id` int(11) DEFAULT NULL,
	`legacy_reporting_to` varchar(20) DEFAULT NULL,
	`legacy_cost_center_id` int(11) DEFAULT NULL,
	`legacy_dimension2_id` int(11) NOT NULL DEFAULT '0',
	`reference_quality` varchar(24) NOT NULL DEFAULT 'legacy_unverified',
	`effective_from` date DEFAULT NULL,
	`effective_to` date DEFAULT NULL,
	`interval_quality` varchar(24) NOT NULL DEFAULT 'exact',
	`source` varchar(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime DEFAULT NULL,
	`updated_by` smallint(6) unsigned DEFAULT NULL,
	PRIMARY KEY (`assignment_id`),
	UNIQUE KEY `hrm_assignment_employment_sequence_uq` (`employment_id`,`assignment_sequence`),
	UNIQUE KEY `hrm_assignment_legacy_current_uq` (`legacy_employee_number`,`legacy_current`),
	KEY `hrm_assignment_employment_asof_idx` (`employment_id`,`effective_from`,`effective_to`,`assignment_id`),
	KEY `hrm_assignment_legacy_work_idx` (`legacy_department_id`,`legacy_position_id`,`legacy_grade_id`,`assignment_id`),
	KEY `hrm_assignment_org_unit_asof_idx` (`organization_unit_id`,`effective_from`,`effective_to`,`assignment_id`),
	KEY `hrm_assignment_grade_asof_idx` (`grade_id`,`effective_from`,`effective_to`,`assignment_id`),
	KEY `hrm_assignment_position_asof_idx` (`position_id`,`effective_from`,`effective_to`,`assignment_id`),
	KEY `hrm_assignment_job_asof_idx` (`job_id`,`effective_from`,`effective_to`,`assignment_id`),
	KEY `hrm_assignment_work_location_asof_idx` (`work_location_id`,`effective_from`,`effective_to`,`assignment_id`),
	KEY `hrm_assignment_manager_asof_idx` (`manager_assignment_id`,`effective_from`,`effective_to`,`assignment_id`),
	CONSTRAINT `0_hrm_assignments_employment_fk` FOREIGN KEY (`employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_assignments_org_unit_fk` FOREIGN KEY (`organization_unit_id`) REFERENCES `0_hrm_organization_units` (`organization_unit_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_assignments_grade_fk` FOREIGN KEY (`grade_id`) REFERENCES `0_hrm_grades` (`grade_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_assignments_position_fk` FOREIGN KEY (`position_id`) REFERENCES `0_hrm_positions` (`position_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_assignments_job_fk` FOREIGN KEY (`job_id`) REFERENCES `0_hrm_jobs` (`job_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_assignments_work_location_fk` FOREIGN KEY (`work_location_id`) REFERENCES `0_hrm_work_locations` (`work_location_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_assignments_manager_assignment_fk` FOREIGN KEY (`manager_assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_lifecycle_commands` (
	`lifecycle_command_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`worker_id` bigint(20) unsigned NOT NULL,
	`employment_id` bigint(20) unsigned DEFAULT NULL,
	`assignment_id` bigint(20) unsigned DEFAULT NULL,
	`command_type` varchar(32) NOT NULL,
	`command_status` varchar(16) NOT NULL DEFAULT 'pending',
	`effective_at` datetime NOT NULL,
	`idempotency_key_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`request_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`approval_draft_id` int(11) DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`requested_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`requested_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`completed_at` datetime DEFAULT NULL,
	`completed_by` smallint(6) unsigned DEFAULT NULL,
	PRIMARY KEY (`lifecycle_command_id`),
	UNIQUE KEY `hrm_lifecycle_command_idempotency_uq` (`worker_id`,`command_type`,`idempotency_key_hash`),
	KEY `hrm_lifecycle_command_status_idx` (`command_status`,`effective_at`,`lifecycle_command_id`),
	KEY `hrm_lifecycle_command_employment_idx` (`employment_id`,`lifecycle_command_id`),
	KEY `hrm_lifecycle_command_assignment_idx` (`assignment_id`,`lifecycle_command_id`),
	KEY `hrm_lifecycle_command_approval_idx` (`approval_draft_id`,`lifecycle_command_id`),
	CONSTRAINT `0_hrm_lifecycle_command_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_lifecycle_command_employment_fk` FOREIGN KEY (`employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_lifecycle_command_assignment_fk` FOREIGN KEY (`assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_lifecycle_hire_commands` (
	`hire_command_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`command_status` varchar(16) NOT NULL DEFAULT 'pending',
	`effective_at` datetime NOT NULL,
	`idempotency_key_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`request_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`approval_draft_id` int(11) DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`requested_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`requested_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`completed_at` datetime DEFAULT NULL,
	`completed_by` smallint(6) unsigned DEFAULT NULL,
	PRIMARY KEY (`hire_command_id`),
	UNIQUE KEY `hrm_lifecycle_hire_command_idempotency_uq` (`idempotency_key_hash`),
	KEY `hrm_lifecycle_hire_command_status_idx` (`command_status`,`effective_at`,`hire_command_id`),
	KEY `hrm_lifecycle_hire_command_approval_idx` (`approval_draft_id`,`hire_command_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_lifecycle_hire_payloads` (
	`hire_command_id` bigint(20) unsigned NOT NULL,
	`payload_state` varchar(16) NOT NULL DEFAULT 'staged',
	`envelope_version` smallint(5) unsigned NOT NULL DEFAULT '1',
	`payload_ciphertext` mediumblob DEFAULT NULL,
	`ciphertext_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`request_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`key_id` char(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`key_generation` int(10) unsigned NOT NULL,
	`purge_after` datetime NOT NULL,
	`purged_at` datetime DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`hire_command_id`),
	KEY `hrm_lifecycle_hire_payload_purge_idx` (`payload_state`,`purge_after`,`hire_command_id`),
	KEY `hrm_lifecycle_hire_payload_key_idx` (`key_generation`,`key_id`,`hire_command_id`),
	CONSTRAINT `0_hrm_lifecycle_hire_payloads_command_fk` FOREIGN KEY (`hire_command_id`) REFERENCES `0_hrm_lifecycle_hire_commands` (`hire_command_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_lifecycle_tasks` (
	`lifecycle_task_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`lifecycle_command_id` bigint(20) unsigned NOT NULL,
	`task_code` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`task_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'pending',
	`confidentiality_class` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'restricted',
	`retention_locked` tinyint(1) NOT NULL DEFAULT '1',
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`created_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	`completed_at` datetime DEFAULT NULL,
	`completed_by` smallint(6) unsigned DEFAULT NULL,
	PRIMARY KEY (`lifecycle_task_id`),
	UNIQUE KEY `hrm_lifecycle_task_code_uq` (`lifecycle_command_id`,`task_code`),
	KEY `hrm_lifecycle_task_status_idx` (`task_status`,`lifecycle_command_id`,`lifecycle_task_id`),
	KEY `hrm_lifecycle_task_confidentiality_idx` (`confidentiality_class`,`task_status`,`lifecycle_task_id`),
	CONSTRAINT `0_hrm_lifecycle_task_command_fk` FOREIGN KEY (`lifecycle_command_id`) REFERENCES `0_hrm_lifecycle_commands` (`lifecycle_command_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_canonical_links` (
	`link_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`proposal_id` bigint(20) unsigned NOT NULL,
	`canonical_person_id` bigint(20) unsigned NOT NULL,
	`alias_person_id` bigint(20) unsigned NOT NULL,
	`worker_count` int(10) unsigned NOT NULL,
	`linked_at` datetime NOT NULL,
	`linked_by` smallint(6) unsigned NOT NULL,
	`link_key` char(64) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`link_id`),
	UNIQUE KEY `hrm_person_canonical_link_proposal_uq` (`proposal_id`),
	UNIQUE KEY `hrm_person_canonical_link_alias_uq` (`alias_person_id`),
	UNIQUE KEY `hrm_person_canonical_link_key_uq` (`link_key`),
	KEY `hrm_person_canonical_link_person_idx` (`canonical_person_id`,`link_id`),
	CONSTRAINT `0_hrm_person_canonical_link_proposal_fk` FOREIGN KEY (`proposal_id`) REFERENCES `0_hrm_person_link_proposals` (`proposal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_canonical_link_canonical_fk` FOREIGN KEY (`canonical_person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_canonical_link_alias_fk` FOREIGN KEY (`alias_person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_person_canonical_link_workers` (
	`link_worker_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`link_id` bigint(20) unsigned NOT NULL,
	`worker_id` bigint(20) unsigned NOT NULL,
	`mapping_id` bigint(20) unsigned NOT NULL,
	`previous_person_id` bigint(20) unsigned NOT NULL,
	`canonical_person_id` bigint(20) unsigned NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`link_worker_id`),
	UNIQUE KEY `hrm_person_canonical_link_worker_uq` (`link_id`,`worker_id`),
	KEY `hrm_person_canonical_link_worker_map_idx` (`mapping_id`,`worker_id`),
	KEY `hrm_person_canonical_link_worker_person_idx` (`canonical_person_id`,`worker_id`),
	CONSTRAINT `0_hrm_person_canonical_link_worker_link_fk` FOREIGN KEY (`link_id`) REFERENCES `0_hrm_person_canonical_links` (`link_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_canonical_link_worker_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_canonical_link_worker_map_fk` FOREIGN KEY (`mapping_id`) REFERENCES `0_hrm_employee_worker_map` (`mapping_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_canonical_link_worker_previous_fk` FOREIGN KEY (`previous_person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_person_canonical_link_worker_canonical_fk` FOREIGN KEY (`canonical_person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_employment_rehires` (
	`rehire_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`proposal_id` bigint(20) unsigned NOT NULL,
	`canonical_link_id` bigint(20) unsigned NOT NULL,
	`predecessor_employment_id` bigint(20) unsigned NOT NULL,
	`successor_employment_id` bigint(20) unsigned NOT NULL,
	`canonical_person_id` bigint(20) unsigned NOT NULL,
	`alias_person_id` bigint(20) unsigned NOT NULL,
	`predecessor_end_date` date NOT NULL,
	`successor_start_date` date NOT NULL,
	`executed_at` datetime NOT NULL,
	`executed_by` smallint(6) unsigned NOT NULL,
	`rehire_key` char(64) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`rehire_id`),
	UNIQUE KEY `hrm_employment_rehire_proposal_uq` (`proposal_id`),
	UNIQUE KEY `hrm_employment_rehire_link_uq` (`canonical_link_id`),
	UNIQUE KEY `hrm_employment_rehire_successor_uq` (`successor_employment_id`),
	UNIQUE KEY `hrm_employment_rehire_key_uq` (`rehire_key`),
	KEY `hrm_employment_rehire_predecessor_idx` (`predecessor_employment_id`,`rehire_id`),
	KEY `hrm_employment_rehire_person_idx` (`canonical_person_id`,`alias_person_id`,`rehire_id`),
	CONSTRAINT `0_hrm_employment_rehire_proposal_fk` FOREIGN KEY (`proposal_id`) REFERENCES `0_hrm_person_link_proposals` (`proposal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_employment_rehire_link_fk` FOREIGN KEY (`canonical_link_id`) REFERENCES `0_hrm_person_canonical_links` (`link_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_employment_rehire_predecessor_fk` FOREIGN KEY (`predecessor_employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_employment_rehire_successor_fk` FOREIGN KEY (`successor_employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_employment_rehire_canonical_fk` FOREIGN KEY (`canonical_person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_employment_rehire_alias_fk` FOREIGN KEY (`alias_person_id`) REFERENCES `0_hrm_persons` (`person_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_user_sessions` (
	`id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`session_key_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`user_id` smallint(6) NOT NULL,
	`role_id` int(11) NOT NULL,
	`auth_method` varchar(32) NOT NULL DEFAULT 'unknown',
	`assurance_level` tinyint(3) unsigned NOT NULL DEFAULT '1',
	`assurance_method` varchar(32) NOT NULL DEFAULT '',
	`assurance_issued_at` datetime DEFAULT NULL,
	`assurance_expires_at` datetime DEFAULT NULL,
	`created_at` datetime NOT NULL,
	`last_seen_at` datetime NOT NULL,
	`idle_expires_at` datetime NOT NULL,
	`absolute_expires_at` datetime NOT NULL,
	`revoked_at` datetime DEFAULT NULL,
	`revoked_by` smallint(6) DEFAULT NULL,
	`revocation_reason` varchar(40) DEFAULT NULL,
	`user_agent_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	PRIMARY KEY (`id`),
	UNIQUE KEY `session_key_hash` (`session_key_hash`),
	KEY `user_active` (`user_id`,`revoked_at`),
	KEY `session_expiry` (`idle_expires_at`,`absolute_expires_at`)
) ENGINE=InnoDB;

CREATE TABLE `0_user_session_assurance_events` (
	`assurance_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`session_id` bigint(20) unsigned NOT NULL,
	`assurance_level` tinyint(3) unsigned NOT NULL,
	`assurance_method` varchar(32) NOT NULL,
	`action_class` varchar(64) NOT NULL,
	`action_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`issued_at` datetime NOT NULL,
	`expires_at` datetime NOT NULL,
	`consumed_at` datetime DEFAULT NULL,
	PRIMARY KEY (`assurance_event_id`),
	UNIQUE KEY `assurance_nonce_hash` (`nonce_hash`),
	KEY `assurance_session_active` (`session_id`,`consumed_at`,`expires_at`),
	KEY `assurance_action_scope` (`action_class`,`action_hash`,`expires_at`),
	CONSTRAINT `0_user_session_assurance_event_session_fk` FOREIGN KEY (`session_id`) REFERENCES `0_user_sessions` (`id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_user_recovery_materials` (
	`recovery_material_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`user_id` smallint(6) NOT NULL,
	`material_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'recovery_code',
	`generation` int(10) unsigned NOT NULL DEFAULT '1',
	`material_slot` smallint(5) unsigned NOT NULL,
	`secret_hash` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`issued_at` datetime NOT NULL,
	`expires_at` datetime NOT NULL,
	`consumed_at` datetime DEFAULT NULL,
	`revoked_at` datetime DEFAULT NULL,
	`revoked_by` smallint(6) DEFAULT NULL,
	`revocation_reason` varchar(40) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	PRIMARY KEY (`recovery_material_id`),
	UNIQUE KEY `recovery_material_generation_slot` (`user_id`,`material_type`,`generation`,`material_slot`),
	KEY `recovery_material_user_active` (`user_id`,`material_type`,`consumed_at`,`revoked_at`,`expires_at`),
	KEY `recovery_material_expiry` (`expires_at`,`recovery_material_id`)
) ENGINE=InnoDB;

CREATE TABLE `0_user_recovery_redemption_requests` (
	`recovery_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`request_key_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`user_id` smallint(6) NOT NULL,
	`recovery_material_id` bigint(20) unsigned NOT NULL,
	`source_bucket_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`request_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'pending',
	`created_at` datetime NOT NULL,
	`expires_at` datetime NOT NULL,
	`terminal_at` datetime DEFAULT NULL,
	`terminal_reason` varchar(40) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	PRIMARY KEY (`recovery_request_id`),
	UNIQUE KEY `recovery_redemption_request_key` (`request_key_hash`),
	KEY `recovery_redemption_account_material` (`user_id`,`recovery_material_id`,`request_status`,`expires_at`,`recovery_request_id`),
	KEY `recovery_redemption_source` (`source_bucket_hash`,`request_status`,`expires_at`,`recovery_request_id`)
) ENGINE=InnoDB;

CREATE TABLE `0_user_recovery_redemption_controls` (
	`recovery_control_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`control_scope` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`control_key_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`failed_attempt_count` smallint(5) unsigned NOT NULL DEFAULT '0',
	`window_started_at` datetime DEFAULT NULL,
	`last_failed_at` datetime DEFAULT NULL,
	`locked_until` datetime DEFAULT NULL,
	`last_cleared_at` datetime DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	PRIMARY KEY (`recovery_control_id`),
	UNIQUE KEY `recovery_redemption_control_key` (`control_scope`,`control_key_hash`),
	KEY `recovery_redemption_control_lock` (`control_scope`,`locked_until`,`recovery_control_id`),
	KEY `recovery_redemption_control_window` (`control_scope`,`window_started_at`,`recovery_control_id`)
) ENGINE=InnoDB;

CREATE TABLE `0_user_break_glass_activation_leases` (
	`break_glass_activation_lease_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`break_glass_designation_id` bigint(20) unsigned NOT NULL,
	`user_id` smallint(6) NOT NULL,
	`scope_code` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'security_recovery',
	`required_auth_method` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'local_password',
	`reason_code` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`ticket_ref_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`activated_at` datetime NOT NULL,
	`expires_at` datetime NOT NULL,
	`session_id` bigint(20) unsigned DEFAULT NULL,
	`active_slot` tinyint(3) unsigned DEFAULT '1',
	`terminalized_at` datetime DEFAULT NULL,
	`terminalized_by` smallint(6) DEFAULT NULL,
	`terminal_reason` varchar(40) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	PRIMARY KEY (`break_glass_activation_lease_id`),
	UNIQUE KEY `break_glass_active_designation` (`break_glass_designation_id`,`active_slot`),
	KEY `break_glass_lease_user_active` (`user_id`,`scope_code`,`active_slot`,`expires_at`),
	KEY `break_glass_lease_expiry` (`expires_at`,`active_slot`,`break_glass_activation_lease_id`),
	KEY `break_glass_lease_session` (`session_id`,`terminalized_at`)
) ENGINE=InnoDB;

CREATE TABLE `0_user_break_glass_designations` (
	`break_glass_designation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`user_id` smallint(6) NOT NULL,
	`scope_code` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'security_recovery',
	`required_auth_method` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'local_password',
	`max_activation_seconds` smallint(5) unsigned NOT NULL DEFAULT '900',
	`designated_at` datetime NOT NULL,
	`designated_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`revoked_at` datetime DEFAULT NULL,
	`revoked_by` smallint(6) DEFAULT NULL,
	`revocation_reason` varchar(40) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	PRIMARY KEY (`break_glass_designation_id`),
	UNIQUE KEY `break_glass_user_scope` (`user_id`,`scope_code`),
	KEY `break_glass_active_scope` (`scope_code`,`revoked_at`,`user_id`)
) ENGINE=InnoDB;

CREATE TABLE `0_user_federation_providers` (
	`provider_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`provider_key` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`protocol` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`issuer_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`provider_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'disabled',
	`provider_row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`created_at` datetime NOT NULL,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	`updated_at` datetime NOT NULL,
	`updated_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`provider_id`),
	UNIQUE KEY `federation_provider_key` (`provider_key`),
	UNIQUE KEY `federation_provider_issuer` (`protocol`,`issuer_hash`),
	KEY `federation_provider_status` (`provider_status`,`provider_id`)
) ENGINE=InnoDB;

CREATE TABLE `0_user_federation_provider_verifier_configs` (
	`verifier_config_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`provider_id` bigint(20) unsigned NOT NULL,
	`config_version` int(10) unsigned NOT NULL,
	`verifier_profile` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`audience_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`trust_material_ref_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`max_clock_skew_seconds` smallint(5) unsigned NOT NULL DEFAULT '60',
	`assurance_level_cap` tinyint(3) unsigned NOT NULL DEFAULT '1',
	`config_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'disabled',
	`created_at` datetime NOT NULL,
	`created_by` smallint(5) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`verifier_config_id`),
	UNIQUE KEY `federation_verifier_provider_version` (`provider_id`,`config_version`),
	KEY `federation_verifier_status` (`config_status`,`provider_id`,`config_version`),
	CONSTRAINT `0_user_federation_provider_verifier_provider_fk` FOREIGN KEY (`provider_id`) REFERENCES `0_user_federation_providers` (`provider_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_user_federation_verifier_transactions` (
	`verifier_transaction_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`verifier_config_id` bigint(20) unsigned NOT NULL,
	`transaction_key_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`correlation_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	`proof_ref_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	`created_at` datetime NOT NULL,
	`expires_at` datetime NOT NULL,
	`consumed_at` datetime DEFAULT NULL,
	PRIMARY KEY (`verifier_transaction_id`),
	UNIQUE KEY `federation_verifier_transaction_key` (`transaction_key_hash`),
	UNIQUE KEY `federation_verifier_transaction_correlation` (`verifier_config_id`,`correlation_hash`),
	KEY `federation_verifier_transaction_active` (`verifier_config_id`,`consumed_at`,`expires_at`),
	CONSTRAINT `0_user_federation_verifier_transaction_config_fk` FOREIGN KEY (`verifier_config_id`) REFERENCES `0_user_federation_provider_verifier_configs` (`verifier_config_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_user_federation_oidc_callback_claims` (
	`callback_claim_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`verifier_transaction_id` bigint(20) unsigned NOT NULL,
	`claim_token_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`claim_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'claimed',
	`claimed_at` datetime NOT NULL,
	`claim_expires_at` datetime NOT NULL,
	`terminal_at` datetime DEFAULT NULL,
	`terminal_reason` varchar(40) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	PRIMARY KEY (`callback_claim_id`),
	UNIQUE KEY `federation_oidc_callback_claim_transaction` (`verifier_transaction_id`),
	UNIQUE KEY `federation_oidc_callback_claim_token` (`claim_token_hash`),
	KEY `federation_oidc_callback_claim_expiry` (`claim_status`,`claim_expires_at`,`callback_claim_id`),
	CONSTRAINT `0_user_federation_oidc_callback_claim_transaction_fk` FOREIGN KEY (`verifier_transaction_id`) REFERENCES `0_user_federation_verifier_transactions` (`verifier_transaction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_user_federation_verifier_replay_markers` (
	`replay_marker_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`verifier_config_id` bigint(20) unsigned NOT NULL,
	`replay_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`replay_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`observed_at` datetime NOT NULL,
	`expires_at` datetime NOT NULL,
	PRIMARY KEY (`replay_marker_id`),
	UNIQUE KEY `federation_verifier_replay_value` (`verifier_config_id`,`replay_kind`,`replay_hash`),
	KEY `federation_verifier_replay_expiry` (`expires_at`,`replay_marker_id`),
	CONSTRAINT `0_user_federation_verifier_replay_config_fk` FOREIGN KEY (`verifier_config_id`) REFERENCES `0_user_federation_provider_verifier_configs` (`verifier_config_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_hrm_ess_identity_link_requests` (
  `request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `user_id` smallint(6) NOT NULL,
  `worker_id` bigint(20) unsigned NOT NULL,
  `evidence_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `requested_at` datetime NOT NULL,
  `requested_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`request_id`),
  UNIQUE KEY `hrm_ess_identity_request_digest_uq` (`request_digest`),
  KEY `hrm_ess_identity_request_user_idx` (`user_id`,`request_id`),
  KEY `hrm_ess_identity_request_worker_idx` (`worker_id`,`request_id`),
  CONSTRAINT `0_hrm_ess_identity_request_user_fk` FOREIGN KEY (`user_id`) REFERENCES `0_users` (`id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_identity_request_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_ess_identity_links` (
  `link_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `request_id` bigint(20) unsigned NOT NULL,
  `user_id` smallint(6) NOT NULL,
  `worker_id` bigint(20) unsigned NOT NULL,
  `approval_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `link_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `linked_at` datetime NOT NULL,
  `linked_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`link_id`),
  UNIQUE KEY `hrm_ess_identity_link_request_uq` (`request_id`),
  UNIQUE KEY `hrm_ess_identity_link_user_uq` (`user_id`),
  UNIQUE KEY `hrm_ess_identity_link_worker_uq` (`worker_id`),
  UNIQUE KEY `hrm_ess_identity_link_digest_uq` (`link_digest`),
  CONSTRAINT `0_hrm_ess_identity_link_request_fk` FOREIGN KEY (`request_id`) REFERENCES `0_hrm_ess_identity_link_requests` (`request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_identity_link_user_fk` FOREIGN KEY (`user_id`) REFERENCES `0_users` (`id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_identity_link_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_ess_manager_delegations` (
  `delegation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `delegator_worker_id` bigint(20) unsigned NOT NULL,
  `delegate_worker_id` bigint(20) unsigned NOT NULL,
  `scope_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'direct_team',
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `evidence_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `approval_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `delegation_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `requested_by` smallint(6) unsigned NOT NULL,
  `approved_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`delegation_id`),
  UNIQUE KEY `hrm_ess_manager_delegation_digest_uq` (`delegation_digest`),
  KEY `hrm_ess_manager_delegator_asof_idx` (`delegator_worker_id`,`effective_from`,`effective_to`,`delegation_id`),
  KEY `hrm_ess_manager_delegate_asof_idx` (`delegate_worker_id`,`effective_from`,`effective_to`,`delegation_id`),
  CONSTRAINT `0_hrm_ess_manager_delegator_worker_fk` FOREIGN KEY (`delegator_worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_manager_delegate_worker_fk` FOREIGN KEY (`delegate_worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_ess_manager_delegation_requests` (
  `request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `delegator_worker_id` bigint(20) unsigned NOT NULL,
  `delegate_worker_id` bigint(20) unsigned NOT NULL,
  `scope_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'direct_team',
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `evidence_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `requested_by` smallint(6) unsigned NOT NULL,
  `approved_by` smallint(6) unsigned DEFAULT NULL,
  `approval_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
  `approved_at` datetime DEFAULT NULL,
  `expires_at` datetime NOT NULL,
  `final_delegation_id` bigint(20) unsigned DEFAULT NULL,
  `row_version` int(10) unsigned NOT NULL DEFAULT '1',
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`request_id`),
  UNIQUE KEY `hrm_ess_manager_delegation_request_digest_uq` (`request_digest`),
  UNIQUE KEY `hrm_ess_manager_delegation_request_nonce_uq` (`request_nonce_hash`),
  UNIQUE KEY `hrm_ess_manager_delegation_request_final_uq` (`final_delegation_id`),
  KEY `hrm_ess_manager_delegation_request_status_idx` (`request_status`,`expires_at`,`request_id`),
  KEY `hrm_ess_manager_delegation_request_delegator_idx` (`delegator_worker_id`,`effective_from`,`effective_to`,`request_id`),
  KEY `hrm_ess_manager_delegation_request_delegate_idx` (`delegate_worker_id`,`effective_from`,`effective_to`,`request_id`),
  CONSTRAINT `0_hrm_ess_manager_delegation_request_delegator_fk` FOREIGN KEY (`delegator_worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_manager_delegation_request_delegate_fk` FOREIGN KEY (`delegate_worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_manager_delegation_request_final_fk` FOREIGN KEY (`final_delegation_id`) REFERENCES `0_hrm_ess_manager_delegations` (`delegation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_ess_self_change_requests` (
  `request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `user_id` smallint(6) NOT NULL,
  `worker_id` bigint(20) unsigned NOT NULL,
  `field_set_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'contact_private_v1',
  `payload_ciphertext` varbinary(4096) NOT NULL,
  `payload_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payload_key_id` char(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `baseline_auth_tag` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `requested_by` smallint(6) unsigned NOT NULL,
  `approved_by` smallint(6) unsigned DEFAULT NULL,
  `approval_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
  `approved_at` datetime DEFAULT NULL,
  `expires_at` datetime NOT NULL,
  `row_version` int(10) unsigned NOT NULL DEFAULT '1',
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`request_id`),
  UNIQUE KEY `hrm_ess_self_change_request_digest_uq` (`request_digest`),
  UNIQUE KEY `hrm_ess_self_change_request_nonce_uq` (`request_nonce_hash`),
  KEY `hrm_ess_self_change_request_subject_idx` (`worker_id`,`request_status`,`expires_at`,`request_id`),
  KEY `hrm_ess_self_change_request_user_idx` (`user_id`,`request_status`,`request_id`),
  CONSTRAINT `0_hrm_ess_self_change_request_user_fk` FOREIGN KEY (`user_id`) REFERENCES `0_users` (`id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_self_change_request_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_ess_self_change_execution_receipts` (
  `receipt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `request_id` bigint(20) unsigned NOT NULL,
  `request_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `approval_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `execution_nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `execution_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `executor_id` smallint(6) NOT NULL,
  `field_names_json` varchar(512) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
  `executed_at` datetime NOT NULL,
  PRIMARY KEY (`receipt_id`),
  UNIQUE KEY `hrm_ess_self_change_execution_request_uq` (`request_id`),
  UNIQUE KEY `hrm_ess_self_change_execution_nonce_uq` (`execution_nonce_hash`),
  UNIQUE KEY `hrm_ess_self_change_execution_digest_uq` (`execution_digest`),
  CONSTRAINT `0_hrm_ess_self_change_execution_request_fk` FOREIGN KEY (`request_id`) REFERENCES `0_hrm_ess_self_change_requests` (`request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_ess_self_change_execution_executor_fk` FOREIGN KEY (`executor_id`) REFERENCES `0_users` (`id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_user_federation_external_subject_links` (
	`external_subject_link_id` bigint unsigned NOT NULL AUTO_INCREMENT,
	`provider_id` bigint unsigned NOT NULL,
	`user_id` smallint(6) NOT NULL,
	`external_subject_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`verifier_transaction_id` bigint unsigned NOT NULL,
	`link_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'active',
	`linked_at` datetime NOT NULL,
	`deprovisioned_at` datetime DEFAULT NULL,
	`deprovisioned_by` smallint(6) DEFAULT NULL,
	`deprovision_reason` varchar(40) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	PRIMARY KEY (`external_subject_link_id`),
	UNIQUE KEY `federation_subject_provider_subject` (`provider_id`,`external_subject_hash`),
	UNIQUE KEY `federation_subject_provider_user` (`provider_id`,`user_id`),
	UNIQUE KEY `federation_subject_transaction` (`verifier_transaction_id`),
	KEY `federation_subject_user_status` (`user_id`,`link_status`,`external_subject_link_id`),
	CONSTRAINT `0_user_federation_subject_link_provider_fk` FOREIGN KEY (`provider_id`) REFERENCES `0_user_federation_providers` (`provider_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_user_federation_subject_link_transaction_fk` FOREIGN KEY (`verifier_transaction_id`) REFERENCES `0_user_federation_verifier_transactions` (`verifier_transaction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_user_federation_subject_link_user_fk` FOREIGN KEY (`user_id`) REFERENCES `0_users` (`id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_user_federation_verifier_decisions` (
	`verifier_decision_id` bigint unsigned NOT NULL AUTO_INCREMENT,
	`verifier_transaction_id` bigint unsigned NOT NULL,
	`replay_marker_id` bigint unsigned NOT NULL,
	`external_subject_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`verified_assurance_level` tinyint unsigned NOT NULL DEFAULT '1',
	`assurance_method` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`verified_at` datetime NOT NULL,
	`valid_until` datetime NOT NULL,
	PRIMARY KEY (`verifier_decision_id`),
	UNIQUE KEY `federation_verifier_decision_transaction` (`verifier_transaction_id`),
	UNIQUE KEY `federation_verifier_decision_replay` (`replay_marker_id`),
	KEY `federation_verifier_decision_subject` (`external_subject_hash`,`valid_until`,`verifier_decision_id`),
	CONSTRAINT `0_user_federation_verifier_decision_transaction_fk` FOREIGN KEY (`verifier_transaction_id`) REFERENCES `0_user_federation_verifier_transactions` (`verifier_transaction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_user_federation_verifier_decision_replay_fk` FOREIGN KEY (`replay_marker_id`) REFERENCES `0_user_federation_verifier_replay_markers` (`replay_marker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_user_federation_session_bindings` (
	`federation_session_binding_id` bigint unsigned NOT NULL AUTO_INCREMENT,
	`session_id` bigint unsigned NOT NULL,
	`verifier_decision_id` bigint unsigned NOT NULL,
	`external_subject_link_id` bigint unsigned NOT NULL,
	`bound_at` datetime NOT NULL,
	PRIMARY KEY (`federation_session_binding_id`),
	UNIQUE KEY `federation_session_binding_session` (`session_id`),
	UNIQUE KEY `federation_session_binding_decision` (`verifier_decision_id`),
	KEY `federation_session_binding_link` (`external_subject_link_id`,`session_id`),
	CONSTRAINT `0_user_federation_session_binding_session_fk` FOREIGN KEY (`session_id`) REFERENCES `0_user_sessions` (`id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_user_federation_session_binding_decision_fk` FOREIGN KEY (`verifier_decision_id`) REFERENCES `0_user_federation_verifier_decisions` (`verifier_decision_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_user_federation_session_binding_link_fk` FOREIGN KEY (`external_subject_link_id`) REFERENCES `0_user_federation_external_subject_links` (`external_subject_link_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `0_hrm_lifecycle_task_document_versions` (
	`task_document_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`lifecycle_task_id` bigint(20) unsigned NOT NULL,
	`employee_document_id` int(11) NOT NULL,
	`employee_worker_mapping_id` bigint(20) unsigned NOT NULL,
	`document_version` int(10) unsigned NOT NULL DEFAULT '1',
	`relationship_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`confidentiality_class` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'restricted',
	`document_snapshot_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`retention_locked` tinyint(1) NOT NULL DEFAULT '1',
	`row_version` int(10) unsigned NOT NULL DEFAULT '1',
	`linked_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	`linked_by` smallint(6) unsigned NOT NULL DEFAULT '0',
	PRIMARY KEY (`task_document_version_id`),
	UNIQUE KEY `hrm_lifecycle_task_document_version_uq` (`lifecycle_task_id`,`employee_document_id`,`document_version`),
	KEY `hrm_lifecycle_task_document_mapping_idx` (`employee_worker_mapping_id`,`employee_document_id`,`task_document_version_id`),
	KEY `hrm_lifecycle_task_document_retention_idx` (`retention_locked`,`confidentiality_class`,`task_document_version_id`),
	CONSTRAINT `0_hrm_lifecycle_task_document_task_fk` FOREIGN KEY (`lifecycle_task_id`) REFERENCES `0_hrm_lifecycle_tasks` (`lifecycle_task_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_lifecycle_task_document_document_fk` FOREIGN KEY (`employee_document_id`) REFERENCES `0_employee_documents` (`doc_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT `0_hrm_lifecycle_task_document_mapping_fk` FOREIGN KEY (`employee_worker_mapping_id`) REFERENCES `0_hrm_employee_worker_map` (`mapping_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_employee_document_migration_journal` (
	`journal_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`doc_id` int(11) NOT NULL,
	`source_path_hash` char(64) NOT NULL,
	`source_size` int(11) unsigned DEFAULT NULL,
	`source_mtime` bigint(20) unsigned DEFAULT NULL,
	`source_sha256` char(64) DEFAULT NULL,
	`detected_mime` varchar(100) DEFAULT NULL,
	`target_attachment_id` int(11) unsigned DEFAULT NULL,
	`attempt_no` int(11) unsigned NOT NULL DEFAULT '1',
	`policy_version` varchar(40) NOT NULL,
	`scanner_status` varchar(32) NOT NULL,
	`status` varchar(24) NOT NULL,
	`error_code` varchar(64) NOT NULL DEFAULT '',
	`actor_id` smallint(6) unsigned NOT NULL DEFAULT '0',
	`batch_id` char(32) NOT NULL,
	`created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`journal_id`),
	KEY `employee_document_migration_source` (`doc_id`,`source_path_hash`,`journal_id`),
	KEY `employee_document_migration_target` (`target_attachment_id`),
	KEY `employee_document_migration_status` (`status`,`journal_id`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `0_payroll_reversal_evidence` (
	`evidence_id`         bigint(20) unsigned NOT NULL AUTO_INCREMENT,
	`payroll_period_id`   int(11) NOT NULL,
	`payslip_id`          int(11) NOT NULL,
	`original_trans_no`   int(11) NOT NULL,
	`evidence_type`       varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`target_id`           int(11) NOT NULL DEFAULT '0',
	`linked_trans_no`     int(11) DEFAULT NULL,
	`before_state`        longtext DEFAULT NULL,
	`after_state_hash`    char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
	`reason_code`         varchar(40) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
	`actor_id`            smallint(6) DEFAULT NULL,
	`effective_date`      date NOT NULL,
	`created_at`          timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
	PRIMARY KEY (`evidence_id`),
	UNIQUE KEY `payroll_reversal_evidence_identity` (`payroll_period_id`,`payslip_id`,`evidence_type`,`target_id`),
	UNIQUE KEY `payroll_reversal_linked_trans` (`linked_trans_no`),
	KEY `payroll_reversal_period_type` (`payroll_period_id`,`evidence_type`),
	KEY `payroll_reversal_original_trans` (`original_trans_no`)
) ENGINE=InnoDB;

CREATE TABLE `0_hrm_lifecycle_rehire_executions` (
  `lifecycle_command_id` bigint(20) unsigned NOT NULL,
  `predecessor_employment_id` bigint(20) unsigned NOT NULL,
  `predecessor_contract_id` bigint(20) unsigned NOT NULL,
  `predecessor_assignment_id` bigint(20) unsigned NOT NULL,
  `successor_employment_id` bigint(20) unsigned NOT NULL,
  `successor_contract_id` bigint(20) unsigned NOT NULL,
  `successor_assignment_id` bigint(20) unsigned NOT NULL,
  `history_id` int(11) NOT NULL,
  `first_hire_date` date NOT NULL,
  PRIMARY KEY (`lifecycle_command_id`),
  UNIQUE KEY `hrm_rehire_predecessor_uq` (`predecessor_employment_id`),
  UNIQUE KEY `hrm_rehire_successor_uq` (`successor_employment_id`),
  UNIQUE KEY `hrm_rehire_history_uq` (`history_id`),
  CONSTRAINT `0_hrm_rehire_command_fk` FOREIGN KEY (`lifecycle_command_id`) REFERENCES `0_hrm_lifecycle_commands` (`lifecycle_command_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_rehire_predecessor_fk` FOREIGN KEY (`predecessor_employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_rehire_old_contract_fk` FOREIGN KEY (`predecessor_contract_id`) REFERENCES `0_hrm_contracts` (`contract_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_rehire_old_assignment_fk` FOREIGN KEY (`predecessor_assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_rehire_successor_fk` FOREIGN KEY (`successor_employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_rehire_new_contract_fk` FOREIGN KEY (`successor_contract_id`) REFERENCES `0_hrm_contracts` (`contract_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_rehire_new_assignment_fk` FOREIGN KEY (`successor_assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_rehire_history_fk` FOREIGN KEY (`history_id`) REFERENCES `0_employee_history` (`history_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_worker_profile_versions` (
  `profile_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint(20) unsigned NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `effective_from` datetime NOT NULL,
  `key_id` char(16) NOT NULL,
  `payload_ciphertext` mediumblob NOT NULL,
  `source` varchar(16) NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`profile_version_id`),
  UNIQUE KEY `hrm_worker_profile_version_uq` (`worker_id`,`version_no`),
  UNIQUE KEY `hrm_worker_profile_predecessor_uq` (`predecessor_id`),
  CONSTRAINT `0_hrm_worker_profile_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON DELETE RESTRICT ON UPDATE RESTRICT,
  CONSTRAINT `0_hrm_worker_profile_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_worker_profile_versions` (`profile_version_id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_employee_projection_policy` (
  `policy_id` tinyint(3) unsigned NOT NULL,
  `company_id` int(10) unsigned NOT NULL,
  `mode` varchar(16) NOT NULL DEFAULT 'shadow',
  `revision` int(10) unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`policy_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_work_authorizations` (
  `authorization_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint(20) unsigned NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `authorization_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `issuing_jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `authorization_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'pending',
  `valid_from` date NOT NULL,
  `valid_to` date DEFAULT NULL,
  `masked_identifier` varchar(32) NOT NULL DEFAULT '',
  `key_id` char(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `details_ciphertext` mediumblob NOT NULL,
  `verification_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'unverified',
  `verified_at` datetime DEFAULT NULL,
  `verified_by` smallint(6) unsigned DEFAULT NULL,
  `employee_document_id` int(11) DEFAULT NULL,
  `row_version` int(10) unsigned NOT NULL DEFAULT '1',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`authorization_id`),
  UNIQUE KEY `hrm_work_authorization_version_uq` (`worker_id`,`version_no`),
  UNIQUE KEY `hrm_work_authorization_predecessor_uq` (`predecessor_id`),
  KEY `hrm_work_authorization_expiry_idx` (`authorization_status`,`valid_to`,`worker_id`),
  KEY `hrm_work_authorization_document_idx` (`employee_document_id`),
  CONSTRAINT `0_hrm_work_authorization_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_work_authorization_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_work_authorizations` (`authorization_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_work_authorization_document_fk` FOREIGN KEY (`employee_document_id`) REFERENCES `0_employee_documents` (`doc_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_work_authorization_policy_versions` (
  `policy_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `policy_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `issuing_jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `authorization_types_json` text NOT NULL,
  `expiry_warning_days` smallint(5) unsigned NOT NULL DEFAULT '30',
  `policy_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `qualified_review_ref` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `approved_at` datetime DEFAULT NULL,
  `approved_by` smallint(6) unsigned DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`policy_version_id`),
  UNIQUE KEY `hrm_work_authorization_policy_version_uq` (`policy_code`,`version_no`),
  UNIQUE KEY `hrm_work_authorization_policy_predecessor_uq` (`predecessor_id`),
  KEY `hrm_work_authorization_policy_effective_idx` (`issuing_jurisdiction`,`policy_status`,`effective_from`,`effective_to`),
  CONSTRAINT `0_hrm_work_authorization_policy_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_work_authorization_policy_versions` (`policy_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_work_authorization_retention_policy_versions` (
  `retention_policy_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `policy_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `issuing_jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `retention_days` smallint(5) unsigned NOT NULL,
  `anchor_event` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'authorization_valid_to',
  `policy_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `qualified_review_ref` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `approved_at` datetime DEFAULT NULL,
  `approved_by` smallint(6) unsigned DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`retention_policy_version_id`),
  UNIQUE KEY `hrm_work_authorization_retention_policy_version_uq` (`policy_code`,`version_no`),
  UNIQUE KEY `hrm_work_authorization_retention_policy_predecessor_uq` (`predecessor_id`),
  KEY `hrm_work_authorization_retention_policy_effective_idx` (`issuing_jurisdiction`,`policy_status`,`effective_from`,`effective_to`),
  CONSTRAINT `0_hrm_work_authorization_retention_policy_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_work_authorization_retention_policy_versions` (`retention_policy_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_work_authorization_delivery_policy_versions` (
  `delivery_policy_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `policy_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `issuing_jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `delivery_requirement` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'stateless_only',
  `recipient_rule_ref` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `channel_rule_ref` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `recovery_rule_ref` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `policy_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `qualified_review_ref` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `approved_at` datetime DEFAULT NULL,
  `approved_by` smallint(6) unsigned DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`delivery_policy_version_id`),
  UNIQUE KEY `hrm_work_authorization_delivery_policy_version_uq` (`policy_code`,`version_no`),
  UNIQUE KEY `hrm_work_authorization_delivery_policy_predecessor_uq` (`predecessor_id`),
  KEY `hrm_work_authorization_delivery_policy_effective_idx` (`issuing_jurisdiction`,`policy_status`,`effective_from`,`effective_to`),
  CONSTRAINT `0_hrm_work_authorization_delivery_policy_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_work_authorization_delivery_policy_versions` (`delivery_policy_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_assignment_work_authorization_jurisdictions` (
  `assignment_id` bigint(20) unsigned NOT NULL,
  `issuing_jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_source` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `row_version` int(10) unsigned NOT NULL DEFAULT '1',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`assignment_id`),
  KEY `hrm_assignment_wa_jurisdiction_idx` (`issuing_jurisdiction`,`assignment_id`),
  CONSTRAINT `0_hrm_assignment_wa_jurisdiction_assignment_fk` FOREIGN KEY (`assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_definitions` (
  `definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `attribute_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`definition_id`),
  UNIQUE KEY `hrm_attribute_definition_code_uq` (`attribute_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_definition_versions` (
  `definition_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `definition_id` bigint(20) unsigned NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `value_type` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `allowed_subject` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `sensitivity` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `owner_scope` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `validation_json` text NOT NULL,
  `definition_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`definition_version_id`),
  UNIQUE KEY `hrm_attribute_definition_version_uq` (`definition_id`,`version_no`),
  UNIQUE KEY `hrm_attribute_definition_predecessor_uq` (`predecessor_id`),
  KEY `hrm_attribute_definition_effective_idx` (`definition_id`,`effective_from`,`effective_to`),
  CONSTRAINT `0_hrm_attribute_definition_version_definition_fk` FOREIGN KEY (`definition_id`) REFERENCES `0_hrm_attribute_definitions` (`definition_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_definition_version_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_attribute_definition_versions` (`definition_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_values` (
  `attribute_value_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `definition_version_id` bigint(20) unsigned NOT NULL,
  `subject_type` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `subject_id` bigint(20) unsigned NOT NULL,
  `value_version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `effective_from` datetime NOT NULL,
  `effective_to` datetime DEFAULT NULL,
  `key_id` char(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `value_ciphertext` mediumblob NOT NULL,
  `ciphertext_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`attribute_value_id`),
  UNIQUE KEY `hrm_attribute_value_version_uq` (`definition_version_id`,`subject_type`,`subject_id`,`value_version_no`),
  UNIQUE KEY `hrm_attribute_value_predecessor_uq` (`predecessor_id`),
  KEY `hrm_attribute_value_subject_idx` (`subject_type`,`subject_id`,`definition_version_id`,`effective_from`,`effective_to`),
  CONSTRAINT `0_hrm_attribute_value_definition_version_fk` FOREIGN KEY (`definition_version_id`) REFERENCES `0_hrm_attribute_definition_versions` (`definition_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_value_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_attribute_values` (`attribute_value_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_legacy_mapping_versions` (
  `mapping_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `legacy_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `attribute_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `subject_type` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'worker',
  `mapping_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `qualified_review_ref` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `approved_at` datetime DEFAULT NULL,
  `approved_by` smallint(6) unsigned DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`mapping_version_id`),
  UNIQUE KEY `hrm_attribute_legacy_mapping_version_uq` (`legacy_key`,`version_no`),
  UNIQUE KEY `hrm_attribute_legacy_mapping_predecessor_uq` (`predecessor_id`),
  KEY `hrm_attribute_legacy_mapping_effective_idx` (`legacy_key`,`mapping_status`,`effective_from`,`effective_to`),
  KEY `hrm_attribute_legacy_mapping_attribute_idx` (`attribute_code`,`mapping_status`,`effective_from`,`effective_to`),
  CONSTRAINT `0_hrm_attribute_legacy_mapping_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_attribute_legacy_mapping_versions` (`mapping_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_mapping_attribute_fk` FOREIGN KEY (`attribute_code`) REFERENCES `0_hrm_attribute_definitions` (`attribute_code`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_legacy_conversion_receipts` (
  `conversion_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `mapping_version_id` bigint(20) unsigned NOT NULL,
  `worker_id` bigint(20) unsigned NOT NULL,
  `profile_version_id` bigint(20) unsigned NOT NULL,
  `definition_version_id` bigint(20) unsigned NOT NULL,
  `attribute_value_id` bigint(20) unsigned NOT NULL,
  `legacy_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `execution_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `converted_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `converted_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`conversion_id`),
  UNIQUE KEY `hrm_attribute_legacy_conversion_mapping_worker_uq` (`mapping_version_id`,`worker_id`),
  UNIQUE KEY `hrm_attribute_legacy_conversion_value_uq` (`attribute_value_id`),
  UNIQUE KEY `hrm_attribute_legacy_conversion_digest_uq` (`execution_digest`),
  KEY `hrm_attribute_legacy_conversion_worker_idx` (`worker_id`,`conversion_id`),
  KEY `hrm_attribute_legacy_conversion_profile_idx` (`profile_version_id`),
  KEY `hrm_attribute_legacy_conversion_definition_idx` (`definition_version_id`),
  CONSTRAINT `0_hrm_attribute_legacy_conversion_mapping_fk` FOREIGN KEY (`mapping_version_id`) REFERENCES `0_hrm_attribute_legacy_mapping_versions` (`mapping_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_conversion_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_conversion_profile_fk` FOREIGN KEY (`profile_version_id`) REFERENCES `0_hrm_worker_profile_versions` (`profile_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_conversion_definition_fk` FOREIGN KEY (`definition_version_id`) REFERENCES `0_hrm_attribute_definition_versions` (`definition_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_conversion_value_fk` FOREIGN KEY (`attribute_value_id`) REFERENCES `0_hrm_attribute_values` (`attribute_value_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_legacy_conversion_batches` (
  `batch_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `mapping_version_id` bigint(20) unsigned NOT NULL,
  `request_nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `candidate_set_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `candidate_as_of` date NOT NULL,
  `candidate_count` smallint(5) unsigned NOT NULL,
  `batch_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `maker_id` smallint(6) unsigned NOT NULL,
  `checker_id` smallint(6) unsigned DEFAULT NULL,
  `approved_at` datetime DEFAULT NULL,
  `expires_at` datetime NOT NULL,
  `row_version` int(10) unsigned NOT NULL DEFAULT '1',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`batch_id`),
  UNIQUE KEY `hrm_attribute_legacy_batch_nonce_uq` (`request_nonce_hash`),
  UNIQUE KEY `hrm_attribute_legacy_batch_candidate_set_uq` (`mapping_version_id`,`candidate_set_digest`),
  KEY `hrm_attribute_legacy_batch_status_idx` (`batch_status`,`expires_at`,`batch_id`),
  CONSTRAINT `0_hrm_attribute_legacy_batch_mapping_fk` FOREIGN KEY (`mapping_version_id`) REFERENCES `0_hrm_attribute_legacy_mapping_versions` (`mapping_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_legacy_conversion_batch_items` (
  `batch_item_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `batch_id` bigint(20) unsigned NOT NULL,
  `employee_number` int(11) NOT NULL,
  `worker_id` bigint(20) unsigned NOT NULL,
  `profile_version_id` bigint(20) unsigned NOT NULL,
  `definition_version_id` bigint(20) unsigned NOT NULL,
  `candidate_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `item_state` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'pending',
  `conversion_id` bigint(20) unsigned DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`batch_item_id`),
  UNIQUE KEY `hrm_attribute_legacy_batch_item_employee_uq` (`batch_id`,`employee_number`),
  UNIQUE KEY `hrm_attribute_legacy_batch_item_worker_uq` (`batch_id`,`worker_id`),
  UNIQUE KEY `hrm_attribute_legacy_batch_item_digest_uq` (`batch_id`,`candidate_digest`),
  KEY `hrm_attribute_legacy_batch_item_worker_idx` (`worker_id`,`batch_item_id`),
  KEY `hrm_attribute_legacy_batch_item_profile_idx` (`profile_version_id`),
  KEY `hrm_attribute_legacy_batch_item_definition_idx` (`definition_version_id`),
  KEY `hrm_attribute_legacy_batch_item_conversion_idx` (`conversion_id`),
  CONSTRAINT `0_hrm_attribute_legacy_batch_item_batch_fk` FOREIGN KEY (`batch_id`) REFERENCES `0_hrm_attribute_legacy_conversion_batches` (`batch_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_batch_item_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_batch_item_profile_fk` FOREIGN KEY (`profile_version_id`) REFERENCES `0_hrm_worker_profile_versions` (`profile_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_batch_item_definition_fk` FOREIGN KEY (`definition_version_id`) REFERENCES `0_hrm_attribute_definition_versions` (`definition_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_batch_item_conversion_fk` FOREIGN KEY (`conversion_id`) REFERENCES `0_hrm_attribute_legacy_conversion_receipts` (`conversion_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_legacy_cleanup_requests` (
  `cleanup_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `batch_id` bigint(20) unsigned NOT NULL,
  `request_nonce_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reconciliation_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `recovery_set_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `key_count` smallint(5) unsigned NOT NULL,
  `request_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'draft',
  `maker_id` smallint(6) unsigned NOT NULL,
  `checker_id` smallint(6) unsigned DEFAULT NULL,
  `approved_at` datetime DEFAULT NULL,
  `expires_at` datetime NOT NULL,
  `row_version` int(10) unsigned NOT NULL DEFAULT '1',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`cleanup_request_id`),
  UNIQUE KEY `hrm_attribute_legacy_cleanup_batch_uq` (`batch_id`),
  UNIQUE KEY `hrm_attribute_legacy_cleanup_nonce_uq` (`request_nonce_hash`),
  KEY `hrm_attribute_legacy_cleanup_status_idx` (`request_status`,`expires_at`,`cleanup_request_id`),
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_batch_fk` FOREIGN KEY (`batch_id`) REFERENCES `0_hrm_attribute_legacy_conversion_batches` (`batch_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_attribute_legacy_cleanup_receipts` (
  `cleanup_receipt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `cleanup_request_id` bigint(20) unsigned NOT NULL,
  `batch_item_id` bigint(20) unsigned NOT NULL,
  `employee_number` int(11) NOT NULL,
  `worker_id` bigint(20) unsigned NOT NULL,
  `mapping_version_id` bigint(20) unsigned NOT NULL,
  `conversion_id` bigint(20) unsigned NOT NULL,
  `pre_cleanup_profile_version_id` bigint(20) unsigned NOT NULL,
  `post_cleanup_profile_version_id` bigint(20) unsigned NOT NULL,
  `execution_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `executed_by` smallint(6) unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`cleanup_receipt_id`),
  UNIQUE KEY `hrm_attribute_legacy_cleanup_receipt_item_uq` (`cleanup_request_id`,`batch_item_id`),
  UNIQUE KEY `hrm_attribute_legacy_cleanup_receipt_batch_item_uq` (`batch_item_id`),
  UNIQUE KEY `hrm_attribute_legacy_cleanup_receipt_post_profile_uq` (`post_cleanup_profile_version_id`),
  UNIQUE KEY `hrm_attribute_legacy_cleanup_receipt_digest_uq` (`execution_digest`),
  KEY `hrm_attribute_legacy_cleanup_receipt_worker_idx` (`worker_id`),
  KEY `hrm_attribute_legacy_cleanup_receipt_mapping_idx` (`mapping_version_id`),
  KEY `hrm_attribute_legacy_cleanup_receipt_conversion_idx` (`conversion_id`),
  KEY `hrm_attribute_legacy_cleanup_receipt_pre_profile_idx` (`pre_cleanup_profile_version_id`),
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_receipt_request_fk` FOREIGN KEY (`cleanup_request_id`) REFERENCES `0_hrm_attribute_legacy_cleanup_requests` (`cleanup_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_receipt_item_fk` FOREIGN KEY (`batch_item_id`) REFERENCES `0_hrm_attribute_legacy_conversion_batch_items` (`batch_item_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_receipt_worker_fk` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_receipt_mapping_fk` FOREIGN KEY (`mapping_version_id`) REFERENCES `0_hrm_attribute_legacy_mapping_versions` (`mapping_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_receipt_conversion_fk` FOREIGN KEY (`conversion_id`) REFERENCES `0_hrm_attribute_legacy_conversion_receipts` (`conversion_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_receipt_pre_profile_fk` FOREIGN KEY (`pre_cleanup_profile_version_id`) REFERENCES `0_hrm_worker_profile_versions` (`profile_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_attribute_legacy_cleanup_receipt_post_profile_fk` FOREIGN KEY (`post_cleanup_profile_version_id`) REFERENCES `0_hrm_worker_profile_versions` (`profile_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_artifacts` (
  `artifact_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `artifact_key` varchar(160) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`artifact_id`),
  UNIQUE KEY `hrm_pay_rule_artifact_key_uq` (`artifact_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_artifact_versions` (
  `artifact_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `artifact_id` bigint(20) unsigned NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_id` bigint(20) unsigned DEFAULT NULL,
  `artifact_semver` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `custody_status` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_class` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_json` mediumtext NOT NULL,
  `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `plan_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `validator_contract` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `plan_contract` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_json` text NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`artifact_version_id`),
  UNIQUE KEY `hrm_pay_rule_artifact_version_uq` (`artifact_id`,`version_no`),
  UNIQUE KEY `hrm_pay_rule_artifact_semver_uq` (`artifact_id`,`artifact_semver`),
  UNIQUE KEY `hrm_pay_rule_artifact_predecessor_uq` (`predecessor_id`),
  KEY `hrm_pay_rule_artifact_status_idx` (`artifact_id`,`custody_status`,`version_no`),
  CONSTRAINT `0_hrm_pay_rule_artifact_version_artifact_fk` FOREIGN KEY (`artifact_id`) REFERENCES `0_hrm_pay_rule_artifacts` (`artifact_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_pay_rule_artifact_version_predecessor_fk` FOREIGN KEY (`predecessor_id`) REFERENCES `0_hrm_pay_rule_artifact_versions` (`artifact_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_import_candidates` (
  `candidate_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `source_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_row_id` bigint(20) unsigned NOT NULL,
  `source_snapshot_json` text NOT NULL,
  `source_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `diagnostics_json` text NOT NULL,
  `candidate_status` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`candidate_id`),
  UNIQUE KEY `hrm_pay_rule_import_source_snapshot_uq` (`source_kind`,`source_row_id`,`source_snapshot_sha256`),
  KEY `hrm_pay_rule_import_candidate_status_idx` (`candidate_status`,`source_kind`,`candidate_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_import_reviews` (
  `review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `candidate_manifest_json` text NOT NULL,
  `candidate_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `plan_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_class` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_json` mediumtext NOT NULL,
  `evidence_id` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `review_status` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `maker_id` smallint(6) unsigned NOT NULL,
  `reviewer_id` smallint(6) unsigned NOT NULL,
  `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reviewed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`review_id`),
  UNIQUE KEY `hrm_pay_rule_import_review_hash_uq` (`review_sha256`),
  KEY `hrm_pay_rule_import_review_status_idx` (`review_status`,`review_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_import_receipts` (
  `receipt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `review_id` bigint(20) unsigned NOT NULL,
  `artifact_version_id` bigint(20) unsigned NOT NULL,
  `candidate_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `plan_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `executor_id` smallint(6) unsigned NOT NULL,
  `receipt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reconciliation_status` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `executed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`receipt_id`),
  UNIQUE KEY `hrm_pay_rule_import_receipt_review_uq` (`review_id`),
  UNIQUE KEY `hrm_pay_rule_import_receipt_artifact_uq` (`artifact_version_id`),
  UNIQUE KEY `hrm_pay_rule_import_receipt_hash_uq` (`receipt_sha256`),
  CONSTRAINT `0_hrm_pay_rule_import_receipt_review_fk` FOREIGN KEY (`review_id`) REFERENCES `0_hrm_pay_rule_import_reviews` (`review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_pay_rule_import_receipt_artifact_fk` FOREIGN KEY (`artifact_version_id`) REFERENCES `0_hrm_pay_rule_artifact_versions` (`artifact_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_rules` (
  `rule_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_key` varchar(160) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `rule_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`rule_id`),
  UNIQUE KEY `hrm_pay_rule_rule_key_uq` (`rule_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_rule_versions` (
  `rule_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_id` bigint(20) unsigned NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_version_id` bigint(20) unsigned DEFAULT NULL,
  `version_label` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_version_id` bigint(20) unsigned NOT NULL,
  `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `plan_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `author_id` smallint(6) unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`rule_version_id`),
  UNIQUE KEY `hrm_pay_rule_rule_version_uq` (`rule_id`,`version_no`),
  UNIQUE KEY `hrm_pay_rule_rule_version_label_uq` (`rule_id`,`version_label`),
  UNIQUE KEY `hrm_pay_rule_rule_predecessor_uq` (`predecessor_version_id`),
  UNIQUE KEY `hrm_pay_rule_rule_artifact_version_uq` (`artifact_version_id`),
  CONSTRAINT `0_hrm_pay_rule_rule_version_rule_fk` FOREIGN KEY (`rule_id`) REFERENCES `0_hrm_pay_rule_rules` (`rule_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_pay_rule_rule_version_predecessor_fk` FOREIGN KEY (`predecessor_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_pay_rule_rule_version_artifact_fk` FOREIGN KEY (`artifact_version_id`) REFERENCES `0_hrm_pay_rule_artifact_versions` (`artifact_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_regulatory_evidence` (
  `regulatory_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_version_id` bigint(20) unsigned NOT NULL,
  `jurisdiction_token` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_reference` varchar(512) NOT NULL,
  `source_evidence_ref` varchar(256) NOT NULL,
  `source_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `captured_at` datetime NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  PRIMARY KEY (`regulatory_evidence_id`),
  UNIQUE KEY `hrm_pay_rule_regulatory_version_uq` (`rule_version_id`),
  UNIQUE KEY `hrm_pay_rule_regulatory_hash_uq` (`evidence_sha256`),
  KEY `hrm_pay_rule_regulatory_jurisdiction_idx` (`jurisdiction_token`,`effective_from`,`effective_to`),
  CONSTRAINT `0_hrm_pay_rule_regulatory_version_fk` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_lifecycle_reviews` (
  `lifecycle_review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_version_id` bigint(20) unsigned NOT NULL,
  `review_decision` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_token` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reviewer_id` smallint(6) unsigned NOT NULL,
  `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reviewed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`lifecycle_review_id`),
  UNIQUE KEY `hrm_pay_rule_lifecycle_review_hash_uq` (`review_sha256`),
  KEY `hrm_pay_rule_lifecycle_review_version_idx` (`rule_version_id`,`review_decision`,`lifecycle_review_id`),
  CONSTRAINT `0_hrm_pay_rule_lifecycle_review_version_fk` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_lifecycle_publications` (
  `publication_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_version_id` bigint(20) unsigned NOT NULL,
  `lifecycle_review_id` bigint(20) unsigned NOT NULL,
  `publisher_id` smallint(6) unsigned NOT NULL,
  `impact_scope_token` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `impact_preview_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `publication_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `published_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`publication_id`),
  UNIQUE KEY `hrm_pay_rule_lifecycle_publication_version_uq` (`rule_version_id`),
  UNIQUE KEY `hrm_pay_rule_lifecycle_publication_review_uq` (`lifecycle_review_id`),
  UNIQUE KEY `hrm_pay_rule_lifecycle_publication_hash_uq` (`publication_sha256`),
  CONSTRAINT `0_hrm_pay_rule_lifecycle_publication_version_fk` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_pay_rule_lifecycle_publication_review_fk` FOREIGN KEY (`lifecycle_review_id`) REFERENCES `0_hrm_pay_rule_lifecycle_reviews` (`lifecycle_review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_lifecycle_events` (
  `event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_version_id` bigint(20) unsigned NOT NULL,
  `publication_id` bigint(20) unsigned NOT NULL,
  `event_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `successor_rule_version_id` bigint(20) unsigned DEFAULT NULL,
  `actor_id` smallint(6) unsigned NOT NULL,
  `reason_token` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `event_at` datetime NOT NULL,
  `event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  PRIMARY KEY (`event_id`),
  UNIQUE KEY `hrm_pay_rule_lifecycle_event_publication_uq` (`publication_id`),
  UNIQUE KEY `hrm_pay_rule_lifecycle_event_hash_uq` (`event_sha256`),
  KEY `hrm_pay_rule_lifecycle_event_successor_idx` (`successor_rule_version_id`),
  CONSTRAINT `0_hrm_pay_rule_lifecycle_event_version_fk` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_pay_rule_lifecycle_event_publication_fk` FOREIGN KEY (`publication_id`) REFERENCES `0_hrm_pay_rule_lifecycle_publications` (`publication_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_hrm_pay_rule_lifecycle_event_successor_fk` FOREIGN KEY (`successor_rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_emergency_changes` (
  `emergency_change_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_version_id` bigint(20) unsigned NOT NULL,
  `requester_id` smallint(6) unsigned NOT NULL,
  `incident_reference` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_token` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `declared_at` datetime NOT NULL,
  `emergency_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  PRIMARY KEY (`emergency_change_id`),
  UNIQUE KEY `hrm_pay_rule_emergency_version_uq` (`rule_version_id`),
  UNIQUE KEY `hrm_pay_rule_emergency_hash_uq` (`emergency_sha256`),
  CONSTRAINT `0_hrm_pay_rule_emergency_version_fk` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_jurisdictions` (
  `jurisdiction_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `jurisdiction_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`jurisdiction_id`),
  UNIQUE KEY `hrm_pay_rule_jurisdiction_key_uq` (`jurisdiction_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_jurisdiction_versions` (
  `jurisdiction_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `jurisdiction_id` bigint(20) unsigned NOT NULL,
  `version_no` int(10) unsigned NOT NULL,
  `predecessor_version_id` bigint(20) unsigned DEFAULT NULL,
  `jurisdiction_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `parent_jurisdiction_id` bigint(20) unsigned DEFAULT NULL,
  `hierarchy_depth` smallint(5) unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `authority_source_ref` varchar(256) NOT NULL,
  `version_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`jurisdiction_version_id`),
  UNIQUE KEY `hrm_pay_rule_jurisdiction_version_no_uq` (`jurisdiction_id`,`version_no`),
  UNIQUE KEY `hrm_pay_rule_jurisdiction_version_start_uq` (`jurisdiction_id`,`effective_from`),
  UNIQUE KEY `hrm_pay_rule_jurisdiction_version_hash_uq` (`version_sha256`),
  UNIQUE KEY `hrm_pay_rule_jurisdiction_predecessor_uq` (`predecessor_version_id`),
  KEY `hrm_pay_rule_jurisdiction_asof_idx` (`jurisdiction_id`,`effective_from`,`effective_to`,`jurisdiction_version_id`),
  KEY `hrm_pay_rule_jurisdiction_parent_idx` (`parent_jurisdiction_id`,`effective_from`,`effective_to`,`jurisdiction_version_id`),
  CONSTRAINT `0_pr5_jv_jurisdiction_fk` FOREIGN KEY (`jurisdiction_id`) REFERENCES `0_hrm_pay_rule_jurisdictions` (`jurisdiction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_jv_predecessor_fk` FOREIGN KEY (`predecessor_version_id`) REFERENCES `0_hrm_pay_rule_jurisdiction_versions` (`jurisdiction_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_jv_parent_fk` FOREIGN KEY (`parent_jurisdiction_id`) REFERENCES `0_hrm_pay_rule_jurisdictions` (`jurisdiction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_subject_jurisdiction_bindings` (
  `binding_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `subject_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `subject_id` bigint(20) unsigned NOT NULL,
  `subject_legal_entity_id` bigint(20) unsigned NOT NULL,
  `fact_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `jurisdiction_id` bigint(20) unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `source_token` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_token` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`binding_id`),
  UNIQUE KEY `hrm_pay_rule_subject_binding_hash_uq` (`binding_sha256`),
  UNIQUE KEY `hrm_pay_rule_subject_binding_start_uq` (`subject_type`,`subject_id`,`fact_role`,`effective_from`),
  KEY `hrm_pay_rule_subject_binding_asof_idx` (`subject_type`,`subject_id`,`fact_role`,`effective_from`,`effective_to`,`binding_id`),
  KEY `hrm_pay_rule_subject_binding_jurisdiction_idx` (`jurisdiction_id`,`effective_from`,`effective_to`,`binding_id`),
  KEY `hrm_pay_rule_subject_binding_entity_idx` (`subject_legal_entity_id`,`effective_from`,`effective_to`,`binding_id`),
  CONSTRAINT `0_pr5_sjb_entity_fk` FOREIGN KEY (`subject_legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_sjb_jurisdiction_fk` FOREIGN KEY (`jurisdiction_id`) REFERENCES `0_hrm_pay_rule_jurisdictions` (`jurisdiction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_jurisdiction_fact_sets` (
  `fact_set_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `employment_id` bigint(20) unsigned NOT NULL,
  `assignment_id` bigint(20) unsigned NOT NULL,
  `legal_entity_id` bigint(20) unsigned NOT NULL,
  `work_location_id` bigint(20) unsigned DEFAULT NULL,
  `as_of_date` date NOT NULL,
  `source_state_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `fact_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `sealed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`fact_set_id`),
  UNIQUE KEY `hrm_pay_rule_fact_set_hash_uq` (`fact_set_sha256`),
  KEY `hrm_pay_rule_fact_set_scope_idx` (`employment_id`,`assignment_id`,`as_of_date`,`fact_set_id`),
  CONSTRAINT `0_pr5_fs_employment_fk` FOREIGN KEY (`employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_fs_assignment_fk` FOREIGN KEY (`assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_fs_entity_fk` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_fs_location_fk` FOREIGN KEY (`work_location_id`) REFERENCES `0_hrm_work_locations` (`work_location_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_jurisdiction_fact_items` (
  `fact_item_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `fact_set_id` bigint(20) unsigned NOT NULL,
  `binding_id` bigint(20) unsigned NOT NULL,
  `jurisdiction_version_id` bigint(20) unsigned NOT NULL,
  `fact_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `subject_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `subject_id` bigint(20) unsigned NOT NULL,
  `jurisdiction_id` bigint(20) unsigned NOT NULL,
  `item_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  PRIMARY KEY (`fact_item_id`),
  UNIQUE KEY `hrm_pay_rule_fact_item_binding_uq` (`fact_set_id`,`binding_id`),
  UNIQUE KEY `hrm_pay_rule_fact_item_hash_uq` (`item_sha256`),
  KEY `hrm_pay_rule_fact_item_role_idx` (`fact_set_id`,`fact_role`,`jurisdiction_id`,`fact_item_id`),
  CONSTRAINT `0_pr5_fi_set_fk` FOREIGN KEY (`fact_set_id`) REFERENCES `0_hrm_pay_rule_jurisdiction_fact_sets` (`fact_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_fi_binding_fk` FOREIGN KEY (`binding_id`) REFERENCES `0_hrm_pay_rule_subject_jurisdiction_bindings` (`binding_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_fi_version_fk` FOREIGN KEY (`jurisdiction_version_id`) REFERENCES `0_hrm_pay_rule_jurisdiction_versions` (`jurisdiction_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_fi_jurisdiction_fk` FOREIGN KEY (`jurisdiction_id`) REFERENCES `0_hrm_pay_rule_jurisdictions` (`jurisdiction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_version_applicability` (
  `applicability_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `rule_version_id` bigint(20) unsigned NOT NULL,
  `publication_id` bigint(20) unsigned NOT NULL,
  `jurisdiction_id` bigint(20) unsigned NOT NULL,
  `fact_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `resolution_group_key` varchar(160) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `applicability_mode` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `precedence_rank` smallint(5) unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `applicability_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `created_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`applicability_id`),
  UNIQUE KEY `hrm_pay_rule_applicability_hash_uq` (`applicability_sha256`),
  UNIQUE KEY `hrm_pay_rule_applicability_start_uq` (`rule_version_id`,`jurisdiction_id`,`fact_role`,`resolution_group_key`,`effective_from`),
  KEY `hrm_pay_rule_applicability_lookup_idx` (`resolution_group_key`,`fact_role`,`effective_from`,`effective_to`,`precedence_rank`,`applicability_id`),
  KEY `hrm_pay_rule_applicability_jurisdiction_idx` (`jurisdiction_id`,`effective_from`,`effective_to`,`applicability_id`),
  CONSTRAINT `0_pr5_ra_version_fk` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_ra_publication_fk` FOREIGN KEY (`publication_id`) REFERENCES `0_hrm_pay_rule_lifecycle_publications` (`publication_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_ra_jurisdiction_fk` FOREIGN KEY (`jurisdiction_id`) REFERENCES `0_hrm_pay_rule_jurisdictions` (`jurisdiction_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_resolution_requests` (
  `resolution_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `employment_id` bigint(20) unsigned NOT NULL,
  `assignment_id` bigint(20) unsigned NOT NULL,
  `fact_set_id` bigint(20) unsigned NOT NULL,
  `as_of_date` date NOT NULL,
  `idempotency_key_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `applicability_state_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `requested_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `requested_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`resolution_id`),
  UNIQUE KEY `hrm_pay_rule_resolution_idempotency_uq` (`employment_id`,`assignment_id`,`idempotency_key_hash`),
  UNIQUE KEY `hrm_pay_rule_resolution_request_hash_uq` (`request_sha256`),
  KEY `hrm_pay_rule_resolution_scope_idx` (`employment_id`,`assignment_id`,`as_of_date`,`resolution_id`),
  CONSTRAINT `0_pr5_rr_employment_fk` FOREIGN KEY (`employment_id`) REFERENCES `0_hrm_employments` (`employment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_rr_assignment_fk` FOREIGN KEY (`assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_rr_fact_set_fk` FOREIGN KEY (`fact_set_id`) REFERENCES `0_hrm_pay_rule_jurisdiction_fact_sets` (`fact_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_resolution_candidates` (
  `candidate_trace_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `resolution_id` bigint(20) unsigned NOT NULL,
  `applicability_id` bigint(20) unsigned NOT NULL,
  `rule_version_id` bigint(20) unsigned NOT NULL,
  `precedence_rank` smallint(5) unsigned NOT NULL,
  `specificity_depth` smallint(5) unsigned NOT NULL,
  `applicability_mode` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `candidate_state` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `candidate_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  PRIMARY KEY (`candidate_trace_id`),
  UNIQUE KEY `hrm_pay_rule_resolution_candidate_uq` (`resolution_id`,`applicability_id`),
  UNIQUE KEY `hrm_pay_rule_resolution_candidate_hash_uq` (`candidate_sha256`),
  CONSTRAINT `0_pr5_rc_request_fk` FOREIGN KEY (`resolution_id`) REFERENCES `0_hrm_pay_rule_resolution_requests` (`resolution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_rc_applicability_fk` FOREIGN KEY (`applicability_id`) REFERENCES `0_hrm_pay_rule_version_applicability` (`applicability_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_pr5_rc_version_fk` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_resolution_decisions` (
  `resolution_id` bigint(20) unsigned NOT NULL,
  `outcome_token` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `selected_summary_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `resolution_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `explanation_token` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decided_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `decided_by` smallint(6) unsigned NOT NULL,
  PRIMARY KEY (`resolution_id`),
  UNIQUE KEY `hrm_pay_rule_resolution_decision_hash_uq` (`resolution_sha256`),
  KEY `hrm_pay_rule_resolution_outcome_idx` (`outcome_token`,`resolution_id`),
  CONSTRAINT `0_pr5_rd_request_fk` FOREIGN KEY (`resolution_id`) REFERENCES `0_hrm_pay_rule_resolution_requests` (`resolution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_rule_migration_candidates` (
  `candidate_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `source_kind` varchar(64) NOT NULL,
  `source_identity` varchar(160) NOT NULL,
  `source_payload_sha256` char(64) NOT NULL,
  `classification_token` varchar(32) NOT NULL,
  `classification_reason` varchar(120) NOT NULL,
  `translated_sha256` char(64) DEFAULT NULL,
  `result_set_sha256` char(64) DEFAULT NULL,
  `row_count` int unsigned NOT NULL,
  `candidate_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`candidate_id`), UNIQUE KEY `uq_pr6_candidate_sha` (`candidate_sha256`),
  KEY `idx_pr6_source_payload` (`source_payload_sha256`), KEY `idx_pr6_class` (`classification_token`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_rule_migration_exceptions` (
  `exception_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `candidate_id` bigint unsigned NOT NULL,
  `disposition` varchar(40) NOT NULL,
  `reason_token` varchar(120) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `exception_sha256` char(64) NOT NULL,
  `reviewed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`exception_id`), UNIQUE KEY `uq_pr6_exception_candidate` (`candidate_id`), UNIQUE KEY `uq_pr6_exception_sha` (`exception_sha256`),
  CONSTRAINT `fk_pr6_exc_candidate` FOREIGN KEY (`candidate_id`) REFERENCES `0_hrm_pay_rule_migration_candidates` (`candidate_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_rule_translation_bindings` (
  `binding_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `candidate_id` bigint unsigned NOT NULL,
  `rule_version_id` bigint unsigned NOT NULL,
  `artifact_version_id` bigint unsigned NOT NULL,
  `target_sha256` char(64) NOT NULL,
  `publication_sha256` char(64) NOT NULL,
  `binding_sha256` char(64) NOT NULL,
  `bound_by` int unsigned NOT NULL,
  `bound_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`binding_id`), UNIQUE KEY `uq_pr6_binding_candidate` (`candidate_id`), UNIQUE KEY `uq_pr6_binding_sha` (`binding_sha256`),
  KEY `idx_pr6_binding_rule` (`rule_version_id`),
  CONSTRAINT `fk_pr6_bind_candidate` FOREIGN KEY (`candidate_id`) REFERENCES `0_hrm_pay_rule_migration_candidates` (`candidate_id`),
  CONSTRAINT `fk_pr6_bind_rule_version` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`),
  CONSTRAINT `fk_pr6_bind_artifact_version` FOREIGN KEY (`artifact_version_id`) REFERENCES `0_hrm_pay_rule_artifact_versions` (`artifact_version_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_rule_tolerance_policies` (
  `tolerance_policy_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `rule_version_id` bigint unsigned DEFAULT NULL,
  `result_type` varchar(32) NOT NULL,
  `absolute_tolerance` decimal(24,8) NOT NULL,
  `materiality_token` varchar(120) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `maker_id` int unsigned NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `policy_version` varchar(40) NOT NULL,
  `policy_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`tolerance_policy_id`), UNIQUE KEY `uq_pr6_tolerance_sha` (`policy_sha256`),
  KEY `idx_pr6_tolerance_rule` (`rule_version_id`),
  CONSTRAINT `fk_pr6_tol_rule_version` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_rule_shadow_runs` (
  `run_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `company_id` int unsigned NOT NULL,
  `payroll_period_id` int unsigned NOT NULL,
  `department_id` int unsigned NOT NULL,
  `source_state_sha256` char(64) NOT NULL,
  `jurisdiction_evidence_sha256` char(64) NOT NULL,
  `comparison_policy_version` varchar(40) NOT NULL,
  `idempotency_sha256` char(64) NOT NULL,
  `run_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL,
  `executed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`run_id`), UNIQUE KEY `uq_pr6_shadow_idem` (`idempotency_sha256`), UNIQUE KEY `uq_pr6_shadow_sha` (`run_sha256`),
  KEY `idx_pr6_shadow_scope` (`company_id`,`department_id`,`payroll_period_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_rule_shadow_comparisons` (
  `comparison_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `run_id` bigint unsigned NOT NULL,
  `employee_id` varchar(64) NOT NULL,
  `candidate_id` bigint unsigned NOT NULL,
  `rule_version_id` bigint unsigned NOT NULL,
  `legacy_result_sha256` char(64) NOT NULL,
  `governed_result_sha256` char(64) NOT NULL,
  `outcome_token` varchar(24) NOT NULL,
  `tolerance_policy_id` bigint unsigned DEFAULT NULL,
  `comparison_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`comparison_id`), UNIQUE KEY `uq_pr6_comparison_sha` (`comparison_sha256`),
  KEY `idx_pr6_compare_run` (`run_id`), KEY `idx_pr6_compare_outcome` (`outcome_token`),
  CONSTRAINT `fk_pr6_cmp_run` FOREIGN KEY (`run_id`) REFERENCES `0_hrm_pay_rule_shadow_runs` (`run_id`),
  CONSTRAINT `fk_pr6_cmp_candidate` FOREIGN KEY (`candidate_id`) REFERENCES `0_hrm_pay_rule_migration_candidates` (`candidate_id`),
  CONSTRAINT `fk_pr6_cmp_rule_version` FOREIGN KEY (`rule_version_id`) REFERENCES `0_hrm_pay_rule_rule_versions` (`rule_version_id`),
  CONSTRAINT `fk_pr6_cmp_tolerance` FOREIGN KEY (`tolerance_policy_id`) REFERENCES `0_hrm_pay_rule_tolerance_policies` (`tolerance_policy_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_rule_activation_eligibility` (
  `eligibility_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `company_id` int unsigned NOT NULL,
  `department_id` int unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `source_state_sha256` char(64) NOT NULL,
  `migration_set_sha256` char(64) NOT NULL,
  `shadow_evidence_sha256` char(64) NOT NULL,
  `jurisdiction_evidence_sha256` char(64) NOT NULL,
  `candidate_count` int unsigned NOT NULL,
  `published_binding_count` int unsigned NOT NULL,
  `compared_candidate_count` int unsigned NOT NULL,
  `comparison_count` int unsigned NOT NULL,
  `unresolved_count` int unsigned NOT NULL,
  `unsupported_count` int unsigned NOT NULL,
  `rollback_ready` tinyint(1) NOT NULL DEFAULT 0,
  `maker_id` int unsigned NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `approver_id` int unsigned NOT NULL,
  `executor_id` int unsigned NOT NULL,
  `eligibility_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`eligibility_id`), UNIQUE KEY `uq_pr6_eligibility_sha` (`eligibility_sha256`),
  KEY `idx_pr6_eligibility_scope` (`company_id`,`department_id`,`effective_from`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_rule_activation_commands` (
  `command_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `company_id` int unsigned NOT NULL,
  `department_id` int unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `sequence_no` int unsigned NOT NULL,
  `predecessor_command_id` bigint unsigned DEFAULT NULL,
  `command_type` varchar(16) NOT NULL,
  `eligibility_id` bigint unsigned NOT NULL,
  `source_state_sha256` char(64) NOT NULL,
  `executor_id` int unsigned NOT NULL,
  `reason_token` varchar(120) NOT NULL,
  `command_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`command_id`), UNIQUE KEY `uq_pr6_command_sha` (`command_sha256`), UNIQUE KEY `uq_pr6_scope_seq` (`company_id`,`department_id`,`sequence_no`),
  KEY `idx_pr6_command_pred` (`predecessor_command_id`), KEY `idx_pr6_command_eligibility` (`eligibility_id`),
  CONSTRAINT `fk_pr6_cmd_pred` FOREIGN KEY (`predecessor_command_id`) REFERENCES `0_hrm_pay_rule_activation_commands` (`command_id`),
  CONSTRAINT `fk_pr6_cmd_eligibility` FOREIGN KEY (`eligibility_id`) REFERENCES `0_hrm_pay_rule_activation_eligibility` (`eligibility_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_calendars` (
  `calendar_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `calendar_code` varchar(64) NOT NULL,
  `calendar_name` varchar(160) NOT NULL,
  `legal_entity_id` bigint unsigned DEFAULT NULL,
  `calendar_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`calendar_id`), UNIQUE KEY `uq_pc2_calendar_code` (`calendar_code`), UNIQUE KEY `uq_pc2_calendar_sha` (`calendar_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_calendar_versions` (
  `calendar_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `calendar_id` bigint unsigned NOT NULL,
  `version_no` int unsigned NOT NULL,
  `frequency_token` varchar(32) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `timezone_name` varchar(64) NOT NULL,
  `version_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`calendar_version_id`), UNIQUE KEY `uq_pc2_calendar_version` (`calendar_id`,`version_no`), UNIQUE KEY `uq_pc2_calver_sha` (`version_sha256`),
  CONSTRAINT `fk_pc2_calver_calendar` FOREIGN KEY (`calendar_id`) REFERENCES `0_hrm_payroll_calendars` (`calendar_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_calendar_periods` (
  `calendar_period_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `calendar_version_id` bigint unsigned NOT NULL,
  `period_key` varchar(80) NOT NULL,
  `period_start` date NOT NULL,
  `period_end` date NOT NULL,
  `cutoff_date` date NOT NULL,
  `pay_date` date NOT NULL,
  `post_date` date NOT NULL,
  `file_date` date NOT NULL,
  `run_type` varchar(24) NOT NULL,
  `period_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`calendar_period_id`), UNIQUE KEY `uq_pc2_period_key` (`calendar_version_id`,`period_key`), UNIQUE KEY `uq_pc2_period_sha` (`period_sha256`),
  CONSTRAINT `fk_pc2_period_calver` FOREIGN KEY (`calendar_version_id`) REFERENCES `0_hrm_payroll_calendar_versions` (`calendar_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_groups` (
  `payroll_group_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `group_code` varchar(64) NOT NULL,
  `group_name` varchar(160) NOT NULL,
  `legal_entity_id` bigint unsigned DEFAULT NULL,
  `group_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`payroll_group_id`), UNIQUE KEY `uq_pc2_group_code` (`group_code`), UNIQUE KEY `uq_pc2_group_sha` (`group_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_group_versions` (
  `group_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_group_id` bigint unsigned NOT NULL,
  `version_no` int unsigned NOT NULL,
  `calendar_version_id` bigint unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `version_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`group_version_id`), UNIQUE KEY `uq_pc2_group_version` (`payroll_group_id`,`version_no`), UNIQUE KEY `uq_pc2_grpver_sha` (`version_sha256`),
  CONSTRAINT `fk_pc2_grpver_group` FOREIGN KEY (`payroll_group_id`) REFERENCES `0_hrm_payroll_groups` (`payroll_group_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc2_grpver_calver` FOREIGN KEY (`calendar_version_id`) REFERENCES `0_hrm_payroll_calendar_versions` (`calendar_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_relationships` (
  `payroll_relationship_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint unsigned NOT NULL,
  `employment_id` bigint unsigned DEFAULT NULL,
  `legal_entity_id` bigint unsigned DEFAULT NULL,
  `relationship_key` varchar(96) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `relationship_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`payroll_relationship_id`), UNIQUE KEY `uq_pc2_relationship_key` (`relationship_key`), UNIQUE KEY `uq_pc2_relationship_sha` (`relationship_sha256`), KEY `idx_pc2_relationship_worker` (`worker_id`,`effective_from`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_relationship_members` (
  `relationship_member_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `member_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`relationship_member_id`), UNIQUE KEY `uq_pc2_relationship_member` (`payroll_relationship_id`,`assignment_id`,`effective_from`), UNIQUE KEY `uq_pc2_relmem_sha` (`member_sha256`),
  CONSTRAINT `fk_pc2_relmem_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_group_enrollments` (
  `enrollment_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `group_version_id` bigint unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `enrollment_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`enrollment_id`), UNIQUE KEY `uq_pc2_enrollment` (`payroll_relationship_id`,`group_version_id`,`effective_from`), UNIQUE KEY `uq_pc2_enrollment_sha` (`enrollment_sha256`),
  CONSTRAINT `fk_pc2_enr_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc2_enr_group_version` FOREIGN KEY (`group_version_id`) REFERENCES `0_hrm_payroll_group_versions` (`group_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_legacy_period_mapping_reviews` (
  `mapping_review_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `legacy_period_id` int NOT NULL,
  `source_period_sha256` char(64) NOT NULL,
  `review_status` varchar(24) NOT NULL,
  `ambiguity_codes` varchar(512) NOT NULL,
  `calendar_resolution` varchar(96) DEFAULT NULL,
  `group_resolution` varchar(96) DEFAULT NULL,
  `legal_entity_resolution` varchar(96) DEFAULT NULL,
  `currency_resolution` varchar(16) DEFAULT NULL,
  `reviewed_by` int unsigned DEFAULT NULL,
  `reviewed_at` datetime DEFAULT NULL,
  `review_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_review_id`),
  UNIQUE KEY `uq_pc2m_legacy_period_review_sha` (`review_sha256`),
  KEY `idx_pc2m_legacy_period` (`legacy_period_id`,`review_status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_seed_proposals` (
  `proposal_id` bigint unsigned NOT NULL AUTO_INCREMENT, `proposal_key` varchar(96) NOT NULL, `legal_entity_id` bigint unsigned NOT NULL, `currency_code` char(3) NOT NULL,
  `calendar_code` varchar(64) NOT NULL, `calendar_name` varchar(160) NOT NULL, `group_code` varchar(64) NOT NULL, `group_name` varchar(160) NOT NULL,
  `frequency_token` varchar(32) NOT NULL, `timezone_name` varchar(64) NOT NULL, `effective_from` date NOT NULL, `period_plan_json` text NOT NULL, `period_plan_sha256` char(64) NOT NULL, `proposal_sha256` char(64) NOT NULL,
  `maker_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`proposal_id`), UNIQUE KEY `uq_pc2_seed_prop_key` (`proposal_key`), UNIQUE KEY `uq_pc2_seed_prop_sha` (`proposal_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_seed_approvals` (
  `approval_id` bigint unsigned NOT NULL AUTO_INCREMENT, `proposal_id` bigint unsigned NOT NULL, `proposal_sha256` char(64) NOT NULL, `decision_token` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL, `approval_sha256` char(64) NOT NULL, `checker_id` int unsigned NOT NULL, `approved_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`approval_id`), UNIQUE KEY `uq_pc2_seed_approval_prop` (`proposal_id`), UNIQUE KEY `uq_pc2_seed_approval_sha` (`approval_sha256`),
  CONSTRAINT `fk_pc2_seed_approval_prop` FOREIGN KEY (`proposal_id`) REFERENCES `0_hrm_payroll_seed_proposals` (`proposal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_seed_executions` (
  `execution_id` bigint unsigned NOT NULL AUTO_INCREMENT, `proposal_id` bigint unsigned NOT NULL, `approval_id` bigint unsigned NOT NULL, `calendar_id` bigint unsigned NOT NULL,
  `calendar_version_id` bigint unsigned NOT NULL, `payroll_group_id` bigint unsigned NOT NULL, `group_version_id` bigint unsigned NOT NULL, `currency_code` char(3) NOT NULL, `period_count` int unsigned NOT NULL, `period_set_sha256` char(64) NOT NULL, `execution_sha256` char(64) NOT NULL,
  `executor_id` int unsigned NOT NULL, `executed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`execution_id`), UNIQUE KEY `uq_pc2_seed_execution_prop` (`proposal_id`), UNIQUE KEY `uq_pc2_seed_execution_sha` (`execution_sha256`),
  CONSTRAINT `fk_pc2_seed_exec_prop` FOREIGN KEY (`proposal_id`) REFERENCES `0_hrm_payroll_seed_proposals` (`proposal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc2_seed_exec_approval` FOREIGN KEY (`approval_id`) REFERENCES `0_hrm_payroll_seed_approvals` (`approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_legacy_period_mapping_plans` (
  `mapping_plan_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legacy_period_id` int NOT NULL, `source_period_sha256` char(64) NOT NULL, `calendar_version_id` bigint unsigned NOT NULL,
  `group_version_id` bigint unsigned NOT NULL, `legal_entity_id` bigint unsigned NOT NULL, `currency_code` char(3) NOT NULL, `period_key` varchar(80) NOT NULL,
  `period_start` date NOT NULL, `period_end` date NOT NULL, `cutoff_date` date NOT NULL, `pay_date` date NOT NULL, `post_date` date NOT NULL, `file_date` date NOT NULL,
  `mapping_plan_sha256` char(64) NOT NULL, `maker_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_plan_id`), UNIQUE KEY `uq_pc2_map_plan_legacy` (`legacy_period_id`), UNIQUE KEY `uq_pc2_map_plan_sha` (`mapping_plan_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_legacy_period_mapping_approvals` (
  `mapping_approval_id` bigint unsigned NOT NULL AUTO_INCREMENT, `mapping_plan_id` bigint unsigned NOT NULL, `mapping_plan_sha256` char(64) NOT NULL, `evidence_sha256` char(64) NOT NULL,
  `approval_sha256` char(64) NOT NULL, `checker_id` int unsigned NOT NULL, `approved_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_approval_id`), UNIQUE KEY `uq_pc2_map_approval_plan` (`mapping_plan_id`), UNIQUE KEY `uq_pc2_map_approval_sha` (`approval_sha256`),
  CONSTRAINT `fk_pc2_map_approval_plan` FOREIGN KEY (`mapping_plan_id`) REFERENCES `0_hrm_payroll_legacy_period_mapping_plans` (`mapping_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_legacy_period_conversion_receipts` (
  `conversion_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `mapping_plan_id` bigint unsigned NOT NULL, `mapping_approval_id` bigint unsigned NOT NULL,
  `legacy_period_id` int NOT NULL, `source_period_sha256` char(64) NOT NULL, `calendar_period_id` bigint unsigned NOT NULL, `conversion_sha256` char(64) NOT NULL,
  `executor_id` int unsigned NOT NULL, `executed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`conversion_receipt_id`), UNIQUE KEY `uq_pc2_conv_plan` (`mapping_plan_id`), UNIQUE KEY `uq_pc2_conv_legacy` (`legacy_period_id`), UNIQUE KEY `uq_pc2_conv_sha` (`conversion_sha256`),
  CONSTRAINT `fk_pc2_conv_plan` FOREIGN KEY (`mapping_plan_id`) REFERENCES `0_hrm_payroll_legacy_period_mapping_plans` (`mapping_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc2_conv_approval` FOREIGN KEY (`mapping_approval_id`) REFERENCES `0_hrm_payroll_legacy_period_mapping_approvals` (`mapping_approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_legacy_period_reconciliations` (
  `reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT, `conversion_receipt_id` bigint unsigned NOT NULL, `legacy_period_id` int NOT NULL, `source_period_sha256` char(64) NOT NULL,
  `target_period_sha256` char(64) NOT NULL, `status_token` varchar(24) NOT NULL, `evidence_sha256` char(64) NOT NULL, `reconciliation_sha256` char(64) NOT NULL,
  `reviewer_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`reconciliation_id`), UNIQUE KEY `uq_pc2_recon_receipt` (`conversion_receipt_id`), UNIQUE KEY `uq_pc2_recon_sha` (`reconciliation_sha256`),
  CONSTRAINT `fk_pc2_recon_receipt` FOREIGN KEY (`conversion_receipt_id`) REFERENCES `0_hrm_payroll_legacy_period_conversion_receipts` (`conversion_receipt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_relationship_migration_plans` (
  `relationship_plan_id` bigint unsigned NOT NULL AUTO_INCREMENT, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL, `group_version_id` bigint unsigned NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL,
  `source_identity_sha256` char(64) NOT NULL, `relationship_plan_sha256` char(64) NOT NULL, `maker_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`relationship_plan_id`), UNIQUE KEY `uq_pc2_rel_plan_sha` (`relationship_plan_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_relationship_migration_approvals` (
  `relationship_approval_id` bigint unsigned NOT NULL AUTO_INCREMENT, `relationship_plan_id` bigint unsigned NOT NULL, `relationship_plan_sha256` char(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL, `approval_sha256` char(64) NOT NULL, `checker_id` int unsigned NOT NULL, `approved_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`relationship_approval_id`), UNIQUE KEY `uq_pc2_rel_approval_plan` (`relationship_plan_id`), UNIQUE KEY `uq_pc2_rel_approval_sha` (`approval_sha256`),
  CONSTRAINT `fk_pc2_relmig_approval_plan` FOREIGN KEY (`relationship_plan_id`) REFERENCES `0_hrm_payroll_relationship_migration_plans` (`relationship_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_relationship_migration_receipts` (
  `relationship_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `relationship_plan_id` bigint unsigned NOT NULL, `relationship_approval_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL, `relationship_member_id` bigint unsigned NOT NULL, `enrollment_id` bigint unsigned NOT NULL, `receipt_sha256` char(64) NOT NULL,
  `executor_id` int unsigned NOT NULL, `executed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`relationship_receipt_id`), UNIQUE KEY `uq_pc2_rel_receipt_plan` (`relationship_plan_id`), UNIQUE KEY `uq_pc2_rel_receipt_sha` (`receipt_sha256`),
  CONSTRAINT `fk_pc2_relmig_receipt_plan` FOREIGN KEY (`relationship_plan_id`) REFERENCES `0_hrm_payroll_relationship_migration_plans` (`relationship_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc2_relmig_receipt_approval` FOREIGN KEY (`relationship_approval_id`) REFERENCES `0_hrm_payroll_relationship_migration_approvals` (`relationship_approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_elements` (
  `pay_element_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `element_code` varchar(64) NOT NULL,
  `identity_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`pay_element_id`),
  UNIQUE KEY `uq_pc3_element_code` (`element_code`),
  UNIQUE KEY `uq_pc3_element_sha` (`identity_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_element_versions` (
  `element_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `pay_element_id` bigint unsigned NOT NULL,
  `version_no` int unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `element_type` varchar(32) NOT NULL,
  `calculation_mode` varchar(32) NOT NULL,
  `rule_artifact_sha256` char(64) DEFAULT NULL,
  `version_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`element_version_id`),
  UNIQUE KEY `uq_pc3_element_version` (`pay_element_id`,`version_no`),
  UNIQUE KEY `uq_pc3_element_version_sha` (`version_sha256`),
  KEY `idx_pc3_element_effective` (`pay_element_id`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_pc3_elemv_element` FOREIGN KEY (`pay_element_id`) REFERENCES `0_hrm_pay_elements` (`pay_element_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_input_definitions` (
  `input_definition_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `element_version_id` bigint unsigned NOT NULL,
  `input_code` varchar(64) NOT NULL,
  `value_type` varchar(16) NOT NULL,
  `unit_token` varchar(32) NOT NULL,
  `required_flag` tinyint unsigned NOT NULL DEFAULT 0,
  `definition_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`input_definition_id`),
  UNIQUE KEY `uq_pc3_input_definition` (`element_version_id`,`input_code`),
  UNIQUE KEY `uq_pc3_input_definition_sha` (`definition_sha256`),
  UNIQUE KEY `uq_pc3_indef_binding` (`input_definition_id`,`element_version_id`,`value_type`),
  CONSTRAINT `fk_pc3_indef_element_version` FOREIGN KEY (`element_version_id`) REFERENCES `0_hrm_pay_element_versions` (`element_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_eligibility_versions` (
  `eligibility_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `element_version_id` bigint unsigned NOT NULL,
  `version_no` int unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `eligibility_kind` varchar(32) NOT NULL,
  `rule_artifact_sha256` char(64) DEFAULT NULL,
  `eligibility_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`eligibility_version_id`),
  UNIQUE KEY `uq_pc3_eligibility_version` (`element_version_id`,`version_no`),
  UNIQUE KEY `uq_pc3_eligibility_sha` (`eligibility_sha256`),
  KEY `idx_pc3_eligibility_effective` (`element_version_id`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_pc3_eligv_element_version` FOREIGN KEY (`element_version_id`) REFERENCES `0_hrm_pay_element_versions` (`element_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_payroll_approved_inputs` (
  `approved_input_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned DEFAULT NULL,
  `element_version_id` bigint unsigned NOT NULL,
  `input_definition_id` bigint unsigned NOT NULL,
  `effective_date` date NOT NULL,
  `value_type` varchar(16) NOT NULL,
  `value_decimal` decimal(24,8) DEFAULT NULL,
  `value_date` date DEFAULT NULL,
  `value_code` varchar(128) DEFAULT NULL,
  `value_boolean` tinyint unsigned DEFAULT NULL,
  `source_kind` varchar(32) NOT NULL,
  `source_event_id` varchar(128) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `approval_evidence_sha256` char(64) NOT NULL,
  `approved_by` int unsigned NOT NULL,
  `approved_at` datetime NOT NULL,
  `override_reason` varchar(255) DEFAULT NULL,
  `supersedes_input_id` bigint unsigned DEFAULT NULL,
  `input_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`approved_input_id`),
  UNIQUE KEY `uq_pc3_input_sha` (`input_sha256`),
  UNIQUE KEY `uq_pc3_source_event` (`input_definition_id`,`source_kind`,`source_event_id`),
  KEY `idx_pc3_input_relationship` (`payroll_relationship_id`,`effective_date`),
  KEY `idx_pc3_input_assignment` (`assignment_id`,`effective_date`),
  KEY `idx_pc3_input_element_version` (`element_version_id`,`effective_date`),
  CONSTRAINT `fk_pc3_input_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc3_input_definition_binding` FOREIGN KEY (`input_definition_id`,`element_version_id`,`value_type`) REFERENCES `0_hrm_pay_input_definitions` (`input_definition_id`,`element_version_id`,`value_type`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc3_input_supersedes` FOREIGN KEY (`supersedes_input_id`) REFERENCES `0_hrm_payroll_approved_inputs` (`approved_input_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_003_legacy_mapping_reviews` (
  `mapping_review_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `source_kind` varchar(32) NOT NULL,
  `source_row_id` bigint unsigned NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `review_status` varchar(24) NOT NULL,
  `ambiguity_codes` varchar(1024) NOT NULL,
  `element_code_resolution` varchar(64) DEFAULT NULL,
  `element_version_resolution` varchar(96) DEFAULT NULL,
  `eligibility_resolution` varchar(96) DEFAULT NULL,
  `input_resolution` varchar(96) DEFAULT NULL,
  `provenance_resolution` varchar(96) DEFAULT NULL,
  `reviewed_by` int unsigned DEFAULT NULL,
  `reviewed_at` datetime DEFAULT NULL,
  `review_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_review_id`),
  UNIQUE KEY `uq_pc3m_review_sha` (`review_sha256`),
  KEY `idx_pc3m_source` (`source_kind`,`source_row_id`,`review_status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_003_legacy_mapping_plans` (
  `mapping_plan_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `source_kind` varchar(32) NOT NULL,
  `source_row_id` bigint unsigned NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `target_kind` varchar(32) NOT NULL,
  `target_payload_json` longtext NOT NULL,
  `target_payload_sha256` char(64) NOT NULL,
  `supersedes_mapping_plan_id` bigint unsigned DEFAULT NULL,
  `mapping_plan_sha256` char(64) NOT NULL,
  `maker_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_plan_id`),
  UNIQUE KEY `uq_pc3_mapping_plan_sha` (`mapping_plan_sha256`),
  KEY `idx_pc3_mapping_source` (`source_kind`,`source_row_id`,`source_sha256`),
  UNIQUE KEY `uq_pc3_mapping_supersedes` (`supersedes_mapping_plan_id`),
  CONSTRAINT `fk_pc3_mapping_supersedes` FOREIGN KEY (`supersedes_mapping_plan_id`) REFERENCES `0_hrm_pay_core_003_legacy_mapping_plans` (`mapping_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_003_legacy_mapping_approvals` (
  `mapping_approval_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `mapping_plan_id` bigint unsigned NOT NULL,
  `mapping_plan_sha256` char(64) NOT NULL,
  `decision_token` varchar(16) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `approval_sha256` char(64) NOT NULL,
  `checker_id` int unsigned NOT NULL,
  `approved_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_approval_id`),
  UNIQUE KEY `uq_pc3_mapping_approval_plan` (`mapping_plan_id`),
  UNIQUE KEY `uq_pc3_mapping_approval_sha` (`approval_sha256`),
  CONSTRAINT `fk_pc3_mapping_approval_plan` FOREIGN KEY (`mapping_plan_id`) REFERENCES `0_hrm_pay_core_003_legacy_mapping_plans` (`mapping_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_003_legacy_conversion_receipts` (
  `conversion_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `mapping_plan_id` bigint unsigned NOT NULL,
  `mapping_approval_id` bigint unsigned NOT NULL,
  `source_kind` varchar(32) NOT NULL,
  `source_row_id` bigint unsigned NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `target_kind` varchar(32) NOT NULL,
  `target_set_json` longtext NOT NULL,
  `target_set_sha256` char(64) NOT NULL,
  `conversion_sha256` char(64) NOT NULL,
  `executor_id` int unsigned NOT NULL,
  `executed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`conversion_receipt_id`),
  UNIQUE KEY `uq_pc3_conversion_plan` (`mapping_plan_id`),
  UNIQUE KEY `uq_pc3_conversion_sha` (`conversion_sha256`),
  UNIQUE KEY `uq_pc3_conversion_source` (`source_kind`,`source_row_id`,`source_sha256`),
  CONSTRAINT `fk_pc3_conversion_plan` FOREIGN KEY (`mapping_plan_id`) REFERENCES `0_hrm_pay_core_003_legacy_mapping_plans` (`mapping_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc3_conversion_approval` FOREIGN KEY (`mapping_approval_id`) REFERENCES `0_hrm_pay_core_003_legacy_mapping_approvals` (`mapping_approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_003_legacy_conversion_reconciliations` (
  `reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `conversion_receipt_id` bigint unsigned NOT NULL,
  `source_kind` varchar(32) NOT NULL,
  `source_row_id` bigint unsigned NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `target_set_sha256` char(64) NOT NULL,
  `status_token` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `recovery_action` varchar(48) NOT NULL,
  `supersedes_reconciliation_id` bigint unsigned DEFAULT NULL,
  `reconciliation_sha256` char(64) NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`reconciliation_id`),
  UNIQUE KEY `uq_pc3_reconciliation_sha` (`reconciliation_sha256`),
  KEY `idx_pc3_reconciliation_receipt` (`conversion_receipt_id`,`created_at`),
  UNIQUE KEY `uq_pc3_reconciliation_supersedes` (`supersedes_reconciliation_id`),
  CONSTRAINT `fk_pc3_reconciliation_receipt` FOREIGN KEY (`conversion_receipt_id`) REFERENCES `0_hrm_pay_core_003_legacy_conversion_receipts` (`conversion_receipt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc3_reconciliation_supersedes` FOREIGN KEY (`supersedes_reconciliation_id`) REFERENCES `0_hrm_pay_core_003_legacy_conversion_reconciliations` (`reconciliation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_003_compatibility_parity_evidence` (
  `parity_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `conversion_receipt_id` bigint unsigned NOT NULL,
  `source_kind` varchar(32) NOT NULL,
  `source_row_id` bigint unsigned NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `target_set_sha256` char(64) NOT NULL,
  `projection_sha256` char(64) NOT NULL,
  `status_token` varchar(16) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `verified_by` int unsigned NOT NULL,
  `verified_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `parity_sha256` char(64) NOT NULL,
  PRIMARY KEY (`parity_evidence_id`),
  UNIQUE KEY `uq_pc3_parity_sha` (`parity_sha256`),
  KEY `idx_pc3_parity_receipt` (`conversion_receipt_id`,`status_token`),
  CONSTRAINT `fk_pc3_parity_receipt` FOREIGN KEY (`conversion_receipt_id`) REFERENCES `0_hrm_pay_core_003_legacy_conversion_receipts` (`conversion_receipt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_snapshots` (
  `snapshot_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned DEFAULT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `run_identity` varchar(128) NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `currency` char(3) NOT NULL,
  `approved_inputs_sha256` char(64) NOT NULL,
  `element_versions_sha256` char(64) NOT NULL,
  `eligibility_sha256` char(64) NOT NULL,
  `rule_artifacts_sha256` char(64) NOT NULL,
  `source_events_sha256` char(64) NOT NULL,
  `configuration_sha256` char(64) NOT NULL,
  `snapshot_payload_json` longtext NOT NULL,
  `snapshot_sha256` char(64) NOT NULL,
  `sealed_by` int unsigned NOT NULL,
  `sealed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`snapshot_id`),
  UNIQUE KEY `uq_pc4_snapshot_sha` (`snapshot_sha256`),
  UNIQUE KEY `uq_pc4_snapshot_run` (`payroll_relationship_id`,`calendar_period_id`,`run_identity`,`snapshot_sha256`),
  KEY `idx_pc4_snapshot_scope` (`payroll_relationship_id`,`calendar_period_id`),
  CONSTRAINT `fk_pc4_snapshot_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc4_snapshot_period` FOREIGN KEY (`calendar_period_id`) REFERENCES `0_hrm_payroll_calendar_periods` (`calendar_period_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_results` (
  `result_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `snapshot_id` bigint unsigned NOT NULL,
  `snapshot_sha256` char(64) NOT NULL,
  `currency` char(3) NOT NULL,
  `gross_amount` decimal(24,8) NOT NULL,
  `deduction_amount` decimal(24,8) NOT NULL,
  `net_amount` decimal(24,8) NOT NULL,
  `line_set_sha256` char(64) NOT NULL,
  `trace_set_sha256` char(64) NOT NULL,
  `result_sha256` char(64) NOT NULL,
  `executor_id` int unsigned NOT NULL,
  `executed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`result_id`),
  UNIQUE KEY `uq_pc4_result_sha` (`result_sha256`),
  KEY `idx_pc4_result_snapshot` (`snapshot_id`),
  CONSTRAINT `fk_pc4_result_snapshot` FOREIGN KEY (`snapshot_id`) REFERENCES `0_hrm_pay_core_004_snapshots` (`snapshot_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_result_lines` (
  `result_line_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `result_id` bigint unsigned NOT NULL,
  `line_no` int unsigned NOT NULL,
  `element_version_id` bigint unsigned NOT NULL,
  `line_kind` varchar(32) NOT NULL,
  `currency` char(3) NOT NULL,
  `amount` decimal(24,8) NOT NULL,
  `quantity` decimal(24,8) NOT NULL,
  `rate` decimal(24,8) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `line_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`result_line_id`),
  UNIQUE KEY `uq_pc4_result_line_no` (`result_id`,`line_no`),
  UNIQUE KEY `uq_pc4_result_line_sha` (`result_id`,`line_sha256`),
  KEY `idx_pc4_result_line_element` (`element_version_id`),
  CONSTRAINT `fk_pc4_line_result` FOREIGN KEY (`result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc4_line_element` FOREIGN KEY (`element_version_id`) REFERENCES `0_hrm_pay_element_versions` (`element_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_traces` (
  `trace_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `result_id` bigint unsigned NOT NULL,
  `trace_seq` int unsigned NOT NULL,
  `trace_kind` varchar(64) NOT NULL,
  `artifact_sha256` char(64) NOT NULL,
  `input_sha256` char(64) NOT NULL,
  `output_sha256` char(64) NOT NULL,
  `value_mode` varchar(16) NOT NULL,
  `value_digest` char(64) DEFAULT NULL,
  `trace_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`trace_id`),
  UNIQUE KEY `uq_pc4_trace_seq` (`result_id`,`trace_seq`),
  UNIQUE KEY `uq_pc4_trace_sha` (`result_id`,`trace_sha256`),
  CONSTRAINT `fk_pc4_trace_result` FOREIGN KEY (`result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_integrity_evidence` (
  `integrity_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `result_id` bigint unsigned NOT NULL,
  `snapshot_sha256` char(64) NOT NULL,
  `result_sha256` char(64) NOT NULL,
  `line_set_sha256` char(64) NOT NULL,
  `trace_set_sha256` char(64) NOT NULL,
  `verification_status` varchar(16) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `integrity_sha256` char(64) NOT NULL,
  `verified_by` int unsigned NOT NULL,
  `verified_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`integrity_evidence_id`),
  UNIQUE KEY `uq_pc4_integrity_sha` (`integrity_sha256`),
  KEY `idx_pc4_integrity_result` (`result_id`,`verification_status`),
  CONSTRAINT `fk_pc4_integrity_result` FOREIGN KEY (`result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_correction_lineage` (
  `lineage_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `predecessor_result_id` bigint unsigned NOT NULL,
  `successor_result_id` bigint unsigned NOT NULL,
  `lineage_kind` varchar(16) NOT NULL,
  `reason_code` varchar(64) NOT NULL,
  `approval_evidence_sha256` char(64) NOT NULL,
  `lineage_sha256` char(64) NOT NULL,
  `checker_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`lineage_id`),
  UNIQUE KEY `uq_pc4_lineage_successor` (`successor_result_id`),
  UNIQUE KEY `uq_pc4_lineage_sha` (`lineage_sha256`),
  KEY `idx_pc4_lineage_predecessor` (`predecessor_result_id`),
  CONSTRAINT `fk_pc4_lineage_predecessor` FOREIGN KEY (`predecessor_result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc4_lineage_successor` FOREIGN KEY (`successor_result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_legacy_provenance_gaps` (
  `legacy_provenance_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `legacy_source_kind` varchar(32) NOT NULL,
  `legacy_source_id` bigint unsigned NOT NULL,
  `legacy_source_sha256` char(64) NOT NULL,
  `provenance_state` varchar(16) NOT NULL,
  `gap_codes` varchar(1024) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `provenance_sha256` char(64) NOT NULL,
  `recorded_by` int unsigned NOT NULL,
  `recorded_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`legacy_provenance_id`),
  UNIQUE KEY `uq_pc4_legacy_provenance_sha` (`provenance_sha256`),
  UNIQUE KEY `uq_pc4_legacy_source_state` (`legacy_source_kind`,`legacy_source_id`,`legacy_source_sha256`,`provenance_state`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_recovery_evidence` (
  `recovery_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `result_id` bigint unsigned NOT NULL,
  `integrity_evidence_id` bigint unsigned NOT NULL,
  `recovery_state` varchar(24) NOT NULL,
  `action_token` varchar(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `recovery_sha256` char(64) NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`recovery_evidence_id`),
  UNIQUE KEY `uq_pc4_recovery_sha` (`recovery_sha256`),
  KEY `idx_pc4_recovery_result` (`result_id`,`created_at`),
  CONSTRAINT `fk_pc4_recovery_result` FOREIGN KEY (`result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc4_recovery_integrity` FOREIGN KEY (`integrity_evidence_id`) REFERENCES `0_hrm_pay_core_004_integrity_evidence` (`integrity_evidence_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_004_archive_evidence` (
  `archive_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `result_id` bigint unsigned NOT NULL,
  `integrity_evidence_id` bigint unsigned NOT NULL,
  `archive_state` varchar(24) NOT NULL,
  `archive_locator_sha256` char(64) NOT NULL,
  `archive_payload_sha256` char(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `archive_sha256` char(64) NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`archive_evidence_id`),
  UNIQUE KEY `uq_pc4_archive_sha` (`archive_sha256`),
  KEY `idx_pc4_archive_result` (`result_id`,`created_at`),
  CONSTRAINT `fk_pc4_archive_result` FOREIGN KEY (`result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc4_archive_integrity` FOREIGN KEY (`integrity_evidence_id`) REFERENCES `0_hrm_pay_core_004_integrity_evidence` (`integrity_evidence_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_balance_definitions` (
  `balance_definition_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `balance_code` varchar(64) NOT NULL,
  `version_no` int unsigned NOT NULL,
  `unit_code` varchar(16) NOT NULL,
  `currency_code` char(3) DEFAULT NULL,
  `dimension_mask` varchar(255) NOT NULL,
  `accumulator_mask` varchar(128) NOT NULL,
  `reset_policy` varchar(24) NOT NULL,
  `reset_month` tinyint unsigned DEFAULT NULL,
  `reset_day` tinyint unsigned DEFAULT NULL,
  `expiry_policy` varchar(24) NOT NULL,
  `expiry_date` date DEFAULT NULL,
  `expiry_days` int unsigned DEFAULT NULL,
  `carry_forward_allowed` tinyint(1) NOT NULL DEFAULT 0,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `definition_sha256` char(64) NOT NULL,
  `status` varchar(16) NOT NULL DEFAULT 'active',
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`balance_definition_id`),
  UNIQUE KEY `uq_pc6_balance_version` (`balance_code`,`version_no`),
  UNIQUE KEY `uq_pc6_balance_def_sha` (`definition_sha256`),
  KEY `idx_pc6_balance_effective` (`balance_code`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_balance_movements` (
  `movement_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `balance_definition_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `person_id` bigint unsigned DEFAULT NULL,
  `worker_id` bigint unsigned DEFAULT NULL,
  `assignment_id` bigint unsigned DEFAULT NULL,
  `calendar_period_id` bigint unsigned DEFAULT NULL,
  `jurisdiction_code` varchar(64) DEFAULT NULL,
  `leave_type_id` bigint unsigned DEFAULT NULL,
  `loan_id` bigint unsigned DEFAULT NULL,
  `source_system` varchar(32) NOT NULL,
  `source_type` varchar(32) NOT NULL,
  `source_event_key` varchar(128) NOT NULL,
  `source_event_sha256` char(64) NOT NULL,
  `movement_kind` varchar(24) NOT NULL,
  `effective_date` date NOT NULL,
  `recognition_date` date NOT NULL,
  `currency_code` char(3) DEFAULT NULL,
  `unit_code` varchar(16) NOT NULL,
  `amount` decimal(30,8) NOT NULL,
  `definition_sha256` char(64) NOT NULL,
  `predecessor_movement_id` bigint unsigned DEFAULT NULL,
  `lineage_sha256` char(64) DEFAULT NULL,
  `movement_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`movement_id`),
  UNIQUE KEY `uq_pc6_movement_sha` (`movement_sha256`),
  UNIQUE KEY `uq_pc6_source_event` (`balance_definition_id`,`payroll_relationship_id`,`source_system`,`source_event_key`),
  KEY `idx_pc6_movement_scope_date` (`balance_definition_id`,`payroll_relationship_id`,`effective_date`,`recognition_date`),
  KEY `idx_pc6_movement_predecessor` (`predecessor_movement_id`),
  CONSTRAINT `fk_pc6_movement_definition` FOREIGN KEY (`balance_definition_id`) REFERENCES `0_hrm_pay_balance_definitions` (`balance_definition_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc6_movement_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc6_movement_period` FOREIGN KEY (`calendar_period_id`) REFERENCES `0_hrm_payroll_calendar_periods` (`calendar_period_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc6_movement_predecessor` FOREIGN KEY (`predecessor_movement_id`) REFERENCES `0_hrm_pay_balance_movements` (`movement_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_balance_opening_inventory` (
  `opening_inventory_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `balance_definition_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `person_id` bigint unsigned DEFAULT NULL,
  `worker_id` bigint unsigned DEFAULT NULL,
  `assignment_id` bigint unsigned DEFAULT NULL,
  `calendar_period_id` bigint unsigned DEFAULT NULL,
  `jurisdiction_code` varchar(64) DEFAULT NULL,
  `leave_type_id` bigint unsigned DEFAULT NULL,
  `loan_id` bigint unsigned DEFAULT NULL,
  `legacy_source_type` varchar(32) NOT NULL,
  `legacy_source_id` varchar(128) NOT NULL,
  `legacy_source_sha256` char(64) NOT NULL,
  `as_of_date` date NOT NULL,
  `currency_code` char(3) DEFAULT NULL,
  `unit_code` varchar(16) NOT NULL,
  `amount` decimal(30,8) NOT NULL,
  `provenance_state` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `inventory_sha256` char(64) NOT NULL,
  `inventoried_by` int unsigned NOT NULL,
  `reviewed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`opening_inventory_id`),
  UNIQUE KEY `uq_pc6_opening_sha` (`inventory_sha256`),
  UNIQUE KEY `uq_pc6_opening_source` (`balance_definition_id`,`payroll_relationship_id`,`legacy_source_type`,`legacy_source_id`,`legacy_source_sha256`),
  CONSTRAINT `fk_pc6_opening_definition` FOREIGN KEY (`balance_definition_id`) REFERENCES `0_hrm_pay_balance_definitions` (`balance_definition_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc6_opening_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_balance_snapshots` (
  `balance_snapshot_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `balance_definition_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `person_id` bigint unsigned DEFAULT NULL,
  `worker_id` bigint unsigned DEFAULT NULL,
  `assignment_id` bigint unsigned DEFAULT NULL,
  `calendar_period_id` bigint unsigned DEFAULT NULL,
  `jurisdiction_code` varchar(64) DEFAULT NULL,
  `leave_type_id` bigint unsigned DEFAULT NULL,
  `loan_id` bigint unsigned DEFAULT NULL,
  `as_of_date` date NOT NULL,
  `period_start` date NOT NULL,
  `period_end` date NOT NULL,
  `ptd_amount` decimal(30,8) NOT NULL,
  `mtd_amount` decimal(30,8) NOT NULL,
  `qtd_amount` decimal(30,8) NOT NULL,
  `ytd_amount` decimal(30,8) NOT NULL,
  `lifetime_amount` decimal(30,8) NOT NULL,
  `movement_set_sha256` char(64) NOT NULL,
  `scope_sha256` char(64) NOT NULL,
  `definition_sha256` char(64) NOT NULL,
  `snapshot_sha256` char(64) NOT NULL,
  `built_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`balance_snapshot_id`),
  UNIQUE KEY `uq_pc6_snapshot_sha` (`snapshot_sha256`),
  UNIQUE KEY `uq_pc6_snapshot_scope` (`balance_definition_id`,`scope_sha256`,`as_of_date`,`movement_set_sha256`),
  CONSTRAINT `fk_pc6_snapshot_definition` FOREIGN KEY (`balance_definition_id`) REFERENCES `0_hrm_pay_balance_definitions` (`balance_definition_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc6_snapshot_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_balance_reconciliations` (
  `reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `balance_snapshot_id` bigint unsigned NOT NULL,
  `accumulator_kind` varchar(16) NOT NULL,
  `source_type` varchar(32) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `expected_amount` decimal(30,8) NOT NULL,
  `actual_amount` decimal(30,8) NOT NULL,
  `difference_amount` decimal(30,8) NOT NULL,
  `reconciliation_state` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `reconciliation_sha256` char(64) NOT NULL,
  `reviewed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`reconciliation_id`),
  UNIQUE KEY `uq_pc6_reconciliation_sha` (`reconciliation_sha256`),
  CONSTRAINT `fk_pc6_recon_snapshot` FOREIGN KEY (`balance_snapshot_id`) REFERENCES `0_hrm_pay_balance_snapshots` (`balance_snapshot_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_balance_recovery_evidence` (
  `recovery_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `reconciliation_id` bigint unsigned NOT NULL,
  `action_token` varchar(64) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `recovery_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`recovery_evidence_id`),
  UNIQUE KEY `uq_pc6_recovery_sha` (`recovery_sha256`),
  CONSTRAINT `fk_pc6_recovery_recon` FOREIGN KEY (`reconciliation_id`) REFERENCES `0_hrm_pay_balance_reconciliations` (`reconciliation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_events` (
  `retro_event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `legal_entity_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `worker_id` bigint unsigned DEFAULT NULL,
  `person_id` bigint unsigned DEFAULT NULL,
  `assignment_id` bigint unsigned DEFAULT NULL,
  `source_family` varchar(64) NOT NULL,
  `source_system` varchar(64) NOT NULL,
  `source_type` varchar(64) NOT NULL,
  `source_key` varchar(160) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `event_sha256` char(64) NOT NULL,
  `recorded_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`retro_event_id`),
  UNIQUE KEY `uq_pc7_event_sha` (`event_sha256`),
  UNIQUE KEY `uq_pc7_event_source` (`payroll_relationship_id`,`source_family`,`source_system`,`source_key`),
  KEY `idx_pc7_event_effective` (`payroll_relationship_id`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_pc7_event_relationship` FOREIGN KEY (`payroll_relationship_id`) REFERENCES `0_hrm_payroll_relationships` (`payroll_relationship_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_affected_results` (
  `affected_result_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `retro_event_id` bigint unsigned NOT NULL,
  `original_result_id` bigint unsigned NOT NULL,
  `original_result_sha256` char(64) NOT NULL,
  `original_snapshot_sha256` char(64) NOT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `period_start` date NOT NULL,
  `period_end` date NOT NULL,
  `discovery_sha256` char(64) NOT NULL,
  `discovered_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`affected_result_id`),
  UNIQUE KEY `uq_pc7_affected_event_result` (`retro_event_id`,`original_result_id`),
  UNIQUE KEY `uq_pc7_discovery_sha` (`discovery_sha256`),
  CONSTRAINT `fk_pc7_affected_event` FOREIGN KEY (`retro_event_id`) REFERENCES `0_hrm_pay_retro_events` (`retro_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_affected_result` FOREIGN KEY (`original_result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_contexts` (
  `context_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `affected_result_id` bigint unsigned NOT NULL,
  `original_result_id` bigint unsigned NOT NULL,
  `original_result_sha256` char(64) NOT NULL,
  `original_snapshot_sha256` char(64) NOT NULL,
  `approved_inputs_sha256` char(64) NOT NULL,
  `element_versions_sha256` char(64) NOT NULL,
  `eligibility_sha256` char(64) NOT NULL,
  `rule_artifacts_sha256` char(64) NOT NULL,
  `jurisdiction_resolution_sha256` char(64) NOT NULL,
  `source_events_sha256` char(64) NOT NULL,
  `configuration_sha256` char(64) NOT NULL,
  `recompute_snapshot_sha256` char(64) DEFAULT NULL,
  `context_payload_json` longtext NOT NULL,
  `provenance_state` varchar(16) NOT NULL,
  `gap_codes_json` text NOT NULL,
  `context_sha256` char(64) NOT NULL,
  `recorded_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`context_id`),
  UNIQUE KEY `uq_pc7_context_sha` (`context_sha256`),
  UNIQUE KEY `uq_pc7_context_affected` (`affected_result_id`,`context_sha256`),
  CONSTRAINT `fk_pc7_context_affected` FOREIGN KEY (`affected_result_id`) REFERENCES `0_hrm_pay_retro_affected_results` (`affected_result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_context_result` FOREIGN KEY (`original_result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_deltas` (
  `delta_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `affected_result_id` bigint unsigned NOT NULL,
  `context_id` bigint unsigned NOT NULL,
  `delta_key` varchar(128) NOT NULL,
  `element_version_id` bigint unsigned NOT NULL,
  `line_kind` varchar(32) NOT NULL,
  `currency` char(3) NOT NULL,
  `original_amount` decimal(30,8) NOT NULL,
  `recomputed_amount` decimal(30,8) NOT NULL,
  `delta_amount` decimal(30,8) NOT NULL,
  `delta_sha256` char(64) NOT NULL,
  `computed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`delta_id`),
  UNIQUE KEY `uq_pc7_delta_sha` (`delta_sha256`),
  UNIQUE KEY `uq_pc7_delta_context_key` (`context_id`,`delta_key`),
  CONSTRAINT `fk_pc7_delta_affected` FOREIGN KEY (`affected_result_id`) REFERENCES `0_hrm_pay_retro_affected_results` (`affected_result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_delta_context` FOREIGN KEY (`context_id`) REFERENCES `0_hrm_pay_retro_contexts` (`context_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_arrears` (
  `arrears_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `affected_result_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `currency` char(3) NOT NULL,
  `direction` varchar(32) NOT NULL,
  `originating_delta_set_sha256` char(64) NOT NULL,
  `policy_evidence_sha256` char(64) NOT NULL,
  `initial_amount` decimal(30,8) NOT NULL,
  `arrears_sha256` char(64) NOT NULL,
  `opened_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`arrears_id`),
  UNIQUE KEY `uq_pc7_arrears_sha` (`arrears_sha256`),
  KEY `idx_pc7_arrears_scope` (`payroll_relationship_id`,`currency`),
  CONSTRAINT `fk_pc7_arrears_affected` FOREIGN KEY (`affected_result_id`) REFERENCES `0_hrm_pay_retro_affected_results` (`affected_result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_arrears_movements` (
  `arrears_movement_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `arrears_id` bigint unsigned NOT NULL,
  `movement_kind` varchar(32) NOT NULL,
  `amount` decimal(30,8) NOT NULL,
  `remaining_amount` decimal(30,8) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `policy_evidence_sha256` char(64) NOT NULL,
  `movement_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`arrears_movement_id`),
  UNIQUE KEY `uq_pc7_arrears_movement_sha` (`movement_sha256`),
  CONSTRAINT `fk_pc7_arrears_movement` FOREIGN KEY (`arrears_id`) REFERENCES `0_hrm_pay_retro_arrears` (`arrears_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_plans` (
  `retro_plan_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `affected_result_id` bigint unsigned NOT NULL,
  `context_id` bigint unsigned NOT NULL,
  `plan_type` varchar(16) NOT NULL,
  `idempotency_key` varchar(128) NOT NULL,
  `reason_code` varchar(64) NOT NULL,
  `reason_evidence_sha256` char(64) NOT NULL,
  `historical_plan_sha256` char(64) NOT NULL,
  `delta_set_sha256` char(64) NOT NULL,
  `bring_forward_date` date DEFAULT NULL,
  `target_calendar_period_id` bigint unsigned DEFAULT NULL,
  `predecessor_result_sha256` char(64) DEFAULT NULL,
  `plan_sha256` char(64) NOT NULL,
  `maker_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`retro_plan_id`),
  UNIQUE KEY `uq_pc7_plan_sha` (`plan_sha256`),
  UNIQUE KEY `uq_pc7_plan_idem` (`affected_result_id`,`idempotency_key`),
  CONSTRAINT `fk_pc7_plan_affected` FOREIGN KEY (`affected_result_id`) REFERENCES `0_hrm_pay_retro_affected_results` (`affected_result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_plan_context` FOREIGN KEY (`context_id`) REFERENCES `0_hrm_pay_retro_contexts` (`context_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_approvals` (
  `approval_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `retro_plan_id` bigint unsigned NOT NULL,
  `plan_sha256` char(64) NOT NULL,
  `approval_evidence_sha256` char(64) NOT NULL,
  `approval_sha256` char(64) NOT NULL,
  `checker_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`approval_id`),
  UNIQUE KEY `uq_pc7_approval_plan` (`retro_plan_id`),
  UNIQUE KEY `uq_pc7_approval_sha` (`approval_sha256`),
  CONSTRAINT `fk_pc7_approval_plan` FOREIGN KEY (`retro_plan_id`) REFERENCES `0_hrm_pay_retro_plans` (`retro_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_executions` (
  `execution_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `retro_plan_id` bigint unsigned NOT NULL,
  `approval_id` bigint unsigned NOT NULL,
  `original_result_id` bigint unsigned NOT NULL,
  `successor_snapshot_id` bigint unsigned NOT NULL,
  `successor_result_id` bigint unsigned NOT NULL,
  `execution_kind` varchar(16) NOT NULL,
  `successor_result_sha256` char(64) NOT NULL,
  `delta_set_sha256` char(64) NOT NULL,
  `balance_movement_set_sha256` char(64) DEFAULT NULL,
  `core004_lineage_id` bigint unsigned NOT NULL,
  `execution_sha256` char(64) NOT NULL,
  `executor_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`execution_id`),
  UNIQUE KEY `uq_pc7_execution_plan` (`retro_plan_id`),
  UNIQUE KEY `uq_pc7_execution_successor` (`successor_result_id`),
  UNIQUE KEY `uq_pc7_execution_sha` (`execution_sha256`),
  UNIQUE KEY `uq_pc7_execution_original_kind` (`original_result_id`,`execution_kind`),
  CONSTRAINT `fk_pc7_execution_plan` FOREIGN KEY (`retro_plan_id`) REFERENCES `0_hrm_pay_retro_plans` (`retro_plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_execution_approval` FOREIGN KEY (`approval_id`) REFERENCES `0_hrm_pay_retro_approvals` (`approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_execution_original` FOREIGN KEY (`original_result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_execution_successor` FOREIGN KEY (`successor_result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_execution_snapshot` FOREIGN KEY (`successor_snapshot_id`) REFERENCES `0_hrm_pay_core_004_snapshots` (`snapshot_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_execution_lineage` FOREIGN KEY (`core004_lineage_id`) REFERENCES `0_hrm_pay_core_004_correction_lineage` (`lineage_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_downstream_obligations` (
  `obligation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `execution_id` bigint unsigned NOT NULL,
  `domain` varchar(16) NOT NULL,
  `obligation_kind` varchar(48) NOT NULL,
  `original_evidence_sha256` char(64) NOT NULL,
  `predecessor_obligation_id` bigint unsigned DEFAULT NULL,
  `obligation_state` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `obligation_sha256` char(64) NOT NULL,
  `recorded_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`obligation_id`),
  UNIQUE KEY `uq_pc7_obligation_sha` (`obligation_sha256`),
  UNIQUE KEY `uq_pc7_obligation_exec_domain` (`execution_id`,`domain`,`obligation_kind`,`original_evidence_sha256`),
  CONSTRAINT `fk_pc7_obligation_execution` FOREIGN KEY (`execution_id`) REFERENCES `0_hrm_pay_retro_executions` (`execution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pc7_obligation_predecessor` FOREIGN KEY (`predecessor_obligation_id`) REFERENCES `0_hrm_pay_retro_downstream_obligations` (`obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_reconciliations` (
  `reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `execution_id` bigint unsigned NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `expected_delta_amount` decimal(30,8) NOT NULL,
  `actual_delta_amount` decimal(30,8) NOT NULL,
  `difference_amount` decimal(30,8) NOT NULL,
  `reconciliation_state` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `reconciliation_sha256` char(64) NOT NULL,
  `reviewed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`reconciliation_id`),
  UNIQUE KEY `uq_pc7_reconciliation_sha` (`reconciliation_sha256`),
  CONSTRAINT `fk_pc7_recon_execution` FOREIGN KEY (`execution_id`) REFERENCES `0_hrm_pay_retro_executions` (`execution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_retro_recovery_evidence` (
  `recovery_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `reconciliation_id` bigint unsigned NOT NULL,
  `action_token` varchar(64) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `recovery_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`recovery_evidence_id`),
  UNIQUE KEY `uq_pc7_recovery_sha` (`recovery_sha256`),
  CONSTRAINT `fk_pc7_recovery_recon` FOREIGN KEY (`reconciliation_id`) REFERENCES `0_hrm_pay_retro_reconciliations` (`reconciliation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_validator_defs` (
  `validator_definition_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `validator_key` varchar(96) NOT NULL, `validator_version` varchar(64) NOT NULL, `domain` varchar(64) NOT NULL, `severity` varchar(16) NOT NULL, `origin` varchar(16) NOT NULL, `capability` varchar(64) NOT NULL, `purpose` varchar(255) NOT NULL,
  `input_contract_sha256` char(64) NOT NULL, `output_contract_sha256` char(64) NOT NULL, `threshold_policy_sha256` char(64) DEFAULT NULL, `applicability_json` longtext NOT NULL, `definition_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`validator_definition_id`), UNIQUE KEY `uq_pc8_validator_sha` (`definition_sha256`), UNIQUE KEY `uq_pc8_validator_version` (`validator_key`,`validator_version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_threshold_policies` (
 `threshold_policy_id` bigint unsigned NOT NULL AUTO_INCREMENT, `policy_key` varchar(96) NOT NULL, `version_no` int unsigned NOT NULL, `measure` varchar(64) NOT NULL, `unit_code` varchar(32) NOT NULL, `currency_code` char(3) DEFAULT NULL, `mode` varchar(16) NOT NULL, `tolerance` decimal(30,8) NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `policy_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`threshold_policy_id`), UNIQUE KEY `uq_pc8_threshold_sha` (`policy_sha256`), UNIQUE KEY `uq_pc8_threshold_version` (`policy_key`,`version_no`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_pack_bindings` (
 `pack_binding_id` bigint unsigned NOT NULL AUTO_INCREMENT, `pack_key` varchar(96) NOT NULL, `pack_version` varchar(64) NOT NULL, `validator_definition_id` bigint unsigned NOT NULL, `capability` varchar(64) NOT NULL, `artifact_sha256` char(64) NOT NULL, `binding_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`pack_binding_id`), UNIQUE KEY `uq_pc8_pack_sha` (`binding_sha256`), UNIQUE KEY `uq_pc8_pack_version` (`pack_key`,`pack_version`,`validator_definition_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_runs` (
 `validation_run_id` bigint unsigned NOT NULL AUTO_INCREMENT, `validator_definition_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `worker_id` bigint unsigned DEFAULT NULL, `person_id` bigint unsigned DEFAULT NULL, `assignment_id` bigint unsigned DEFAULT NULL, `calendar_period_id` bigint unsigned DEFAULT NULL, `result_id` bigint unsigned NOT NULL, `result_sha256` char(64) NOT NULL, `source_sha256` char(64) NOT NULL, `evidence_set_sha256` char(64) NOT NULL, `idempotency_key` varchar(128) NOT NULL, `outcome` varchar(16) NOT NULL, `finding_set_sha256` char(64) NOT NULL, `execution_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`validation_run_id`), UNIQUE KEY `uq_pc8_run_command` (`validator_definition_id`,`payroll_relationship_id`,`idempotency_key`), UNIQUE KEY `uq_pc8_run_sha` (`execution_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_findings` (
 `finding_id` bigint unsigned NOT NULL AUTO_INCREMENT, `validation_run_id` bigint unsigned NOT NULL, `finding_key` varchar(96) NOT NULL, `code` varchar(96) NOT NULL, `severity` varchar(16) NOT NULL, `evidence_sha256` char(64) NOT NULL, `finding_sha256` char(64) NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`finding_id`), UNIQUE KEY `uq_pc8_finding_run_key` (`validation_run_id`,`finding_key`), UNIQUE KEY `uq_pc8_finding_sha` (`finding_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_resolutions` (
 `resolution_id` bigint unsigned NOT NULL AUTO_INCREMENT, `finding_id` bigint unsigned NOT NULL, `reason_code` varchar(96) NOT NULL, `support_sha256` char(64) NOT NULL, `resulting_state` varchar(24) NOT NULL, `command_key` varchar(128) NOT NULL, `resolution_sha256` char(64) NOT NULL, `resolver_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`resolution_id`), UNIQUE KEY `uq_pc8_resolution_command` (`finding_id`,`command_key`), UNIQUE KEY `uq_pc8_resolution_sha` (`resolution_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_signoffs` (
 `signoff_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `calendar_period_id` bigint unsigned DEFAULT NULL, `result_set_sha256` char(64) NOT NULL, `validation_set_sha256` char(64) NOT NULL, `resolution_set_sha256` char(64) NOT NULL, `signoff_state` varchar(16) NOT NULL, `command_key` varchar(128) NOT NULL, `signoff_sha256` char(64) NOT NULL, `signer_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`signoff_id`), UNIQUE KEY `uq_pc8_signoff_command` (`payroll_relationship_id`,`command_key`), UNIQUE KEY `uq_pc8_signoff_sha` (`signoff_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_variances` (
 `variance_id` bigint unsigned NOT NULL AUTO_INCREMENT, `variance_kind` varchar(24) NOT NULL, `legal_entity_id` int unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `current_result_id` bigint unsigned NOT NULL, `comparison_result_id` bigint unsigned NOT NULL, `current_result_sha256` char(64) NOT NULL, `comparison_result_sha256` char(64) NOT NULL, `currency` char(3) NOT NULL, `measure_set_json` longtext NOT NULL, `threshold_policy_sha256` char(64) DEFAULT NULL, `variance_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`variance_id`), UNIQUE KEY `uq_pc8_variance_sha` (`variance_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_reconciliations` (
 `reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT, `reconciliation_kind` varchar(24) NOT NULL, `legal_entity_id` int unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `calendar_period_id` bigint unsigned DEFAULT NULL, `result_set_sha256` char(64) NOT NULL, `source_evidence_sha256` char(64) NOT NULL, `expected_value` decimal(30,8) NOT NULL, `actual_value` decimal(30,8) NOT NULL, `difference_value` decimal(30,8) NOT NULL, `unit_code` varchar(32) NOT NULL, `currency_code` char(3) DEFAULT NULL, `threshold_policy_sha256` char(64) DEFAULT NULL, `reconciliation_state` varchar(24) NOT NULL, `evidence_sha256` char(64) NOT NULL, `reconciliation_sha256` char(64) NOT NULL, `reviewed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`reconciliation_id`), UNIQUE KEY `uq_pc8_reconciliation_sha` (`reconciliation_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_validation_recovery_evidence` (
 `recovery_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT, `subject_type` varchar(64) NOT NULL, `subject_sha256` char(64) NOT NULL, `action_token` varchar(64) NOT NULL, `source_sha256` char(64) NOT NULL, `evidence_sha256` char(64) NOT NULL, `recovery_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`recovery_evidence_id`), UNIQUE KEY `uq_pc8_recovery_sha` (`recovery_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_runs` (
  `run_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `run_key` varchar(96) NOT NULL,
  `run_type` varchar(32) NOT NULL,
  `payroll_period_id` int unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `calendar_period_id` bigint unsigned DEFAULT NULL,
  `period_name` varchar(80) NOT NULL,
  `from_date` date NOT NULL,
  `to_date` date NOT NULL,
  `chunk_size` int unsigned NOT NULL,
  `request_sha256` char(64) NOT NULL,
  `state` varchar(24) NOT NULL,
  `state_version` bigint unsigned NOT NULL,
  `state_sha256` char(64) NOT NULL,
  `checkpoint_no` int unsigned NOT NULL DEFAULT 0,
  `lease_worker_sha256` char(64) DEFAULT NULL,
  `lease_token_sha256` char(64) DEFAULT NULL,
  `lease_expires_at` datetime DEFAULT NULL,
  `heartbeat_at` datetime DEFAULT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`run_id`),
  UNIQUE KEY `uq_pc9_run_key` (`run_key`),
  UNIQUE KEY `uq_pc9_request` (`request_sha256`),
  UNIQUE KEY `uq_pc9_payroll_period` (`payroll_period_id`),
  KEY `ix_pc9_run_state_lease` (`state`,`lease_expires_at`),
  KEY `ix_pc9_run_scope` (`legal_entity_id`,`calendar_period_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_items` (
  `run_item_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `run_id` bigint unsigned NOT NULL,
  `ordinal_no` int unsigned NOT NULL,
  `employee_id` varchar(64) NOT NULL,
  `employee_id_sha256` char(64) NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `worker_id` bigint unsigned NOT NULL,
  `person_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `calendar_period_id` bigint unsigned DEFAULT NULL,
  `object_scope_sha256` char(64) NOT NULL,
  `input_sha256` char(64) NOT NULL,
  `item_state` varchar(24) NOT NULL,
  `state_version` bigint unsigned NOT NULL,
  `item_state_sha256` char(64) NOT NULL,
  `attempt_no` int unsigned NOT NULL DEFAULT 0,
  `result_sha256` char(64) DEFAULT NULL,
  `result_payload_json` longtext DEFAULT NULL,
  `lease_token_sha256` char(64) DEFAULT NULL,
  `last_error_code` varchar(96) DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`run_item_id`),
  UNIQUE KEY `uq_pc9_item_ordinal` (`run_id`,`ordinal_no`),
  UNIQUE KEY `uq_pc9_item_employee` (`run_id`,`employee_id_sha256`),
  KEY `ix_pc9_item_state` (`run_id`,`item_state`,`ordinal_no`),
  KEY `ix_pc9_item_scope` (`legal_entity_id`,`payroll_relationship_id`,`assignment_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_lease_events` (
  `lease_event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `run_id` bigint unsigned NOT NULL, `event_kind` varchar(24) NOT NULL,
  `worker_sha256` char(64) NOT NULL, `lease_token_sha256` char(64) NOT NULL,
  `acquired_at` datetime NOT NULL, `expires_at` datetime NOT NULL,
  `lease_event_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`lease_event_id`), UNIQUE KEY `uq_pc9_lease_sha` (`lease_event_sha256`), KEY `ix_pc9_lease_run` (`run_id`,`lease_event_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_attempts` (
  `attempt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `run_id` bigint unsigned NOT NULL,
  `run_item_id` bigint unsigned NOT NULL, `attempt_no` int unsigned NOT NULL,
  `command_key` varchar(128) NOT NULL, `worker_sha256` char(64) NOT NULL,
  `predecessor_state_sha256` char(64) NOT NULL, `outcome` varchar(24) NOT NULL,
  `result_sha256` char(64) DEFAULT NULL, `error_code` varchar(96) DEFAULT NULL,
  `evidence_sha256` char(64) NOT NULL, `attempt_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`attempt_id`), UNIQUE KEY `uq_pc9_attempt_sha` (`attempt_sha256`), UNIQUE KEY `uq_pc9_attempt_no` (`run_item_id`,`attempt_no`), KEY `ix_pc9_attempt_run` (`run_id`,`attempt_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_checkpoints` (
  `checkpoint_id` bigint unsigned NOT NULL AUTO_INCREMENT, `run_id` bigint unsigned NOT NULL,
  `checkpoint_no` int unsigned NOT NULL, `completed_items` int unsigned NOT NULL,
  `failed_items` int unsigned NOT NULL, `quarantined_items` int unsigned NOT NULL,
  `item_set_sha256` char(64) NOT NULL, `state_sha256` char(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL, `checkpoint_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`checkpoint_id`), UNIQUE KEY `uq_pc9_checkpoint_sha` (`checkpoint_sha256`), UNIQUE KEY `uq_pc9_checkpoint_no` (`run_id`,`checkpoint_no`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_transitions` (
  `transition_id` bigint unsigned NOT NULL AUTO_INCREMENT, `run_id` bigint unsigned NOT NULL,
  `from_state` varchar(24) NOT NULL, `to_state` varchar(24) NOT NULL,
  `expected_version` bigint unsigned NOT NULL, `resulting_version` bigint unsigned NOT NULL,
  `command_key` varchar(128) NOT NULL, `predecessor_state_sha256` char(64) NOT NULL,
  `resulting_state_sha256` char(64) NOT NULL, `transition_sha256` char(64) NOT NULL,
  `actor_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`transition_id`), UNIQUE KEY `uq_pc9_transition_sha` (`transition_sha256`), UNIQUE KEY `uq_pc9_transition_version` (`run_id`,`resulting_version`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_quarantine` (
  `quarantine_id` bigint unsigned NOT NULL AUTO_INCREMENT, `run_id` bigint unsigned NOT NULL,
  `run_item_id` bigint unsigned NOT NULL, `attempt_sha256` char(64) NOT NULL,
  `reason_code` varchar(96) NOT NULL, `evidence_sha256` char(64) NOT NULL,
  `quarantine_sha256` char(64) NOT NULL, `quarantined_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`quarantine_id`), UNIQUE KEY `uq_pc9_quarantine_sha` (`quarantine_sha256`), UNIQUE KEY `uq_pc9_quarantine_attempt` (`attempt_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_009_recovery_evidence` (
  `recovery_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT, `run_id` bigint unsigned NOT NULL,
  `action_token` varchar(64) NOT NULL, `predecessor_state_sha256` char(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL, `recovery_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`recovery_evidence_id`), UNIQUE KEY `uq_pc9_recovery_sha` (`recovery_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_010_approval_intents` (
  `approval_intent_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_period_id` int unsigned NOT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `payroll_group_id` bigint unsigned NOT NULL,
  `group_version_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `result_set_sha256` char(64) NOT NULL,
  `result_count` int unsigned NOT NULL,
  `gross_amount` decimal(20,6) NOT NULL,
  `deduction_amount` decimal(20,6) NOT NULL,
  `net_amount` decimal(20,6) NOT NULL,
  `projection_contract_version` varchar(48) NOT NULL,
  `projection_set_sha256` char(64) NOT NULL,
  `cutover_transition_sha256` char(64) NOT NULL,
  `command_key` varchar(128) NOT NULL,
  `command_sha256` char(64) NOT NULL,
  `maker_id` int unsigned NOT NULL,
  `intent_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`approval_intent_id`),
  UNIQUE KEY `uq_pc10_intent_sha` (`intent_sha256`),
  UNIQUE KEY `uq_pc10_intent_cmd` (`payroll_group_id`,`command_key`),
  KEY `ix_pc10_intent_period` (`payroll_period_id`,`result_set_sha256`),
  KEY `ix_pc10_intent_scope` (`legal_entity_id`,`payroll_group_id`,`calendar_period_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_010_approvals` (
  `approval_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `approval_intent_id` bigint unsigned NOT NULL,
  `payroll_period_id` int unsigned NOT NULL,
  `result_set_sha256` char(64) NOT NULL,
  `projection_set_sha256` char(64) NOT NULL,
  `intent_sha256` char(64) NOT NULL,
  `approval_draft_id` int unsigned NOT NULL,
  `decision_token` varchar(24) NOT NULL,
  `approver_id` int unsigned NOT NULL,
  `approval_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`approval_id`),
  UNIQUE KEY `uq_pc10_approval_sha` (`approval_sha256`),
  UNIQUE KEY `uq_pc10_approval_draft` (`approval_draft_id`),
  KEY `ix_pc10_approval_period` (`payroll_period_id`,`result_set_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_010_projections` (
  `projection_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_period_id` int unsigned NOT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `payroll_group_id` bigint unsigned NOT NULL,
  `group_version_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `employee_id_sha256` char(64) NOT NULL,
  `result_id` bigint unsigned NOT NULL,
  `result_sha256` char(64) NOT NULL,
  `projection_contract_version` varchar(48) NOT NULL,
  `legacy_payslip_id` int unsigned DEFAULT NULL,
  `header_sha256` char(64) NOT NULL,
  `detail_set_sha256` char(64) NOT NULL,
  `command_key` varchar(128) NOT NULL,
  `command_sha256` char(64) NOT NULL,
  `projection_sha256` char(64) NOT NULL,
  `projected_by` int unsigned NOT NULL,
  `projected_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`projection_id`),
  UNIQUE KEY `uq_pc10_projection_sha` (`projection_sha256`),
  UNIQUE KEY `uq_pc10_projection_cmd` (`payroll_period_id`,`command_key`,`result_id`),
  UNIQUE KEY `uq_pc10_projection_result` (`result_id`,`projection_contract_version`),
  KEY `ix_pc10_projection_period` (`payroll_period_id`,`result_id`),
  KEY `ix_pc10_projection_scope` (`legal_entity_id`,`payroll_group_id`,`payroll_relationship_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_010_parity_evidence` (
  `parity_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_period_id` int unsigned NOT NULL,
  `payroll_group_id` bigint unsigned NOT NULL,
  `group_version_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `result_set_sha256` char(64) NOT NULL,
  `candidate_projection_set_sha256` char(64) NOT NULL,
  `legacy_projection_set_sha256` char(64) NOT NULL,
  `parity_status` varchar(16) NOT NULL,
  `mismatch_count` int unsigned NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `parity_sha256` char(64) NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`parity_id`),
  UNIQUE KEY `uq_pc10_parity_sha` (`parity_sha256`),
  KEY `ix_pc10_parity_scope` (`payroll_group_id`,`payroll_period_id`,`parity_status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_010_cutover_transitions` (
  `transition_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_group_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `sequence_no` bigint unsigned NOT NULL,
  `from_state` varchar(24) NOT NULL,
  `to_state` varchar(24) NOT NULL,
  `expected_sequence_no` bigint unsigned NOT NULL,
  `result_contract_version` varchar(32) NOT NULL,
  `parity_id` bigint unsigned DEFAULT NULL,
  `command_key` varchar(128) NOT NULL,
  `command_sha256` char(64) NOT NULL,
  `predecessor_transition_sha256` char(64) DEFAULT NULL,
  `transition_sha256` char(64) NOT NULL,
  `actor_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`transition_id`),
  UNIQUE KEY `uq_pc10_transition_sha` (`transition_sha256`),
  UNIQUE KEY `uq_pc10_transition_seq` (`payroll_group_id`,`sequence_no`),
  UNIQUE KEY `uq_pc10_transition_cmd` (`payroll_group_id`,`command_key`),
  KEY `ix_pc10_transition_scope` (`legal_entity_id`,`payroll_group_id`,`transition_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_010_recovery_evidence` (
  `recovery_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_group_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `payroll_period_id` int unsigned DEFAULT NULL,
  `transition_id` bigint unsigned DEFAULT NULL,
  `recovery_action` varchar(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `recovery_sha256` char(64) NOT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`recovery_id`),
  UNIQUE KEY `uq_pc10_recovery_sha` (`recovery_sha256`),
  KEY `ix_pc10_recovery_scope` (`payroll_group_id`,`payroll_period_id`,`recovery_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_pay_core_010_usage_events` (
  `usage_event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `payroll_group_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `payroll_period_id` int unsigned DEFAULT NULL,
  `consumer_token` varchar(64) NOT NULL,
  `authority_mode` varchar(24) NOT NULL,
  `source_token` varchar(48) NOT NULL,
  `event_sha256` char(64) NOT NULL,
  `recorded_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`usage_event_id`),
  UNIQUE KEY `uq_pc10_usage_sha` (`event_sha256`),
  KEY `ix_pc10_usage_scope` (`payroll_group_id`,`payroll_period_id`,`consumer_token`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_work_patterns` (
  `work_pattern_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint unsigned NOT NULL,
  `employment_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `timezone_name` varchar(64) NOT NULL,
  `cycle_days` smallint unsigned NOT NULL,
  `pattern_code` varchar(64) NOT NULL,
  `source_kind` varchar(32) NOT NULL,
  `predecessor_pattern_id` bigint unsigned DEFAULT NULL,
  `pattern_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`work_pattern_id`),
  UNIQUE KEY `uq_tim1_pattern_sha` (`pattern_sha256`),
  UNIQUE KEY `uq_tim1_pattern_from` (`assignment_id`,`effective_from`),
  KEY `ix_tim1_pattern_scope` (`legal_entity_id`,`worker_id`,`assignment_id`,`effective_from`),
  UNIQUE KEY `uq_tim1_pattern_pred` (`predecessor_pattern_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_work_pattern_segments` (
  `segment_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `work_pattern_id` bigint unsigned NOT NULL,
  `cycle_day` smallint unsigned NOT NULL,
  `segment_sequence` smallint unsigned NOT NULL,
  `local_start_time` time NOT NULL,
  `local_end_time` time NOT NULL,
  `crosses_midnight` tinyint unsigned NOT NULL,
  `paid_minutes` int unsigned NOT NULL,
  `break_minutes` int unsigned NOT NULL,
  `rest_minutes` int unsigned NOT NULL,
  `segment_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`segment_id`),
  UNIQUE KEY `uq_tim1_segment_sha` (`segment_sha256`),
  UNIQUE KEY `uq_tim1_segment_seq` (`work_pattern_id`,`cycle_day`,`segment_sequence`),
  KEY `ix_tim1_segment_pattern` (`work_pattern_id`,`cycle_day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_raw_events` (
  `raw_event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint unsigned NOT NULL,
  `employment_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `source_channel` varchar(32) NOT NULL,
  `source_event_key` varchar(128) NOT NULL,
  `recorded_at_utc` datetime NOT NULL,
  `effective_local_at` datetime NOT NULL,
  `timezone_name` varchar(64) NOT NULL,
  `effective_utc_at` datetime NOT NULL,
  `utc_offset_minutes` smallint NOT NULL,
  `event_type` varchar(32) NOT NULL,
  `source_payload_sha256` char(64) NOT NULL,
  `predecessor_event_id` bigint unsigned DEFAULT NULL,
  `ingest_command_key` varchar(128) NOT NULL,
  `event_sha256` char(64) NOT NULL,
  `recorded_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`raw_event_id`),
  UNIQUE KEY `uq_tim1_raw_source` (`source_channel`,`source_event_key`),
  UNIQUE KEY `uq_tim1_raw_sha` (`event_sha256`),
  UNIQUE KEY `uq_tim1_raw_cmd` (`ingest_command_key`),
  KEY `ix_tim1_raw_scope` (`legal_entity_id`,`worker_id`,`assignment_id`,`effective_utc_at`),
  UNIQUE KEY `uq_tim1_raw_pred` (`predecessor_event_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_raw_event_conflicts` (
  `conflict_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `source_channel` varchar(32) NOT NULL,
  `source_event_key` varchar(128) NOT NULL,
  `existing_raw_event_id` bigint unsigned NOT NULL,
  `existing_event_sha256` char(64) NOT NULL,
  `attempted_event_sha256` char(64) NOT NULL,
  `attempted_payload_sha256` char(64) NOT NULL,
  `ingest_command_key` varchar(128) NOT NULL,
  `conflict_sha256` char(64) NOT NULL,
  `recorded_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`conflict_id`),
  UNIQUE KEY `uq_tim1_conflict_sha` (`conflict_sha256`),
  KEY `ix_tim1_conflict_source` (`source_channel`,`source_event_key`,`conflict_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_reviewed_entries` (
  `reviewed_entry_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint unsigned NOT NULL,
  `employment_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `work_date` date NOT NULL,
  `period_start_utc` datetime NOT NULL,
  `period_end_utc` datetime NOT NULL,
  `duration_minutes` int unsigned NOT NULL,
  `source_event_set_sha256` char(64) NOT NULL,
  `source_event_count` int unsigned NOT NULL,
  `work_pattern_sha256` char(64) NOT NULL,
  `review_command_key` varchar(128) NOT NULL,
  `predecessor_entry_id` bigint unsigned DEFAULT NULL,
  `reviewer_id` int unsigned NOT NULL,
  `review_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`reviewed_entry_id`),
  UNIQUE KEY `uq_tim1_review_sha` (`review_sha256`),
  UNIQUE KEY `uq_tim1_review_cmd` (`review_command_key`),
  KEY `ix_tim1_review_scope` (`legal_entity_id`,`worker_id`,`assignment_id`,`work_date`),
  UNIQUE KEY `uq_tim1_review_pred` (`predecessor_entry_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_reviewed_event_links` (
  `reviewed_event_link_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `reviewed_entry_id` bigint unsigned NOT NULL,
  `raw_event_id` bigint unsigned NOT NULL,
  `event_sha256` char(64) NOT NULL,
  `link_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`reviewed_event_link_id`),
  UNIQUE KEY `uq_tim1_rel_sha` (`link_sha256`),
  UNIQUE KEY `uq_tim1_rel_pair` (`reviewed_entry_id`,`raw_event_id`),
  KEY `ix_tim1_rel_event` (`raw_event_id`,`reviewed_entry_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_reviewed_allocations` (
  `allocation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `reviewed_entry_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `allocation_category` varchar(32) NOT NULL,
  `quantity_minutes` int unsigned NOT NULL,
  `allocation_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`allocation_id`),
  UNIQUE KEY `uq_tim1_alloc_sha` (`allocation_sha256`),
  KEY `ix_tim1_alloc_review` (`reviewed_entry_id`,`allocation_id`),
  KEY `ix_tim1_alloc_rel` (`payroll_relationship_id`,`assignment_id`,`reviewed_entry_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_approvals` (
  `approval_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `reviewed_entry_id` bigint unsigned NOT NULL,
  `review_sha256` char(64) NOT NULL,
  `allocation_set_sha256` char(64) NOT NULL,
  `approval_evidence_sha256` char(64) NOT NULL,
  `approval_command_key` varchar(128) NOT NULL,
  `approver_id` int unsigned NOT NULL,
  `approval_sha256` char(64) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`approval_id`),
  UNIQUE KEY `uq_tim1_approval_sha` (`approval_sha256`),
  UNIQUE KEY `uq_tim1_approval_cmd` (`approval_command_key`),
  UNIQUE KEY `uq_tim1_approval_review` (`reviewed_entry_id`,`review_sha256`),
  KEY `ix_tim1_approval_entry` (`reviewed_entry_id`,`approval_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_payroll_input_mappings` (
  `mapping_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `legal_entity_id` int unsigned NOT NULL,
  `allocation_category` varchar(32) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date DEFAULT NULL,
  `element_version_id` bigint unsigned NOT NULL,
  `input_definition_id` bigint unsigned NOT NULL,
  `unit_token` varchar(24) NOT NULL,
  `predecessor_mapping_id` bigint unsigned DEFAULT NULL,
  `mapping_sha256` char(64) NOT NULL,
  `created_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_id`),
  UNIQUE KEY `uq_tim1_mapping_sha` (`mapping_sha256`),
  UNIQUE KEY `uq_tim1_mapping_from` (`legal_entity_id`,`allocation_category`,`effective_from`),
  KEY `ix_tim1_mapping_scope` (`legal_entity_id`,`allocation_category`,`effective_from`,`effective_to`),
  UNIQUE KEY `uq_tim1_mapping_pred` (`predecessor_mapping_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_payroll_input_receipts` (
  `receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `approval_id` bigint unsigned NOT NULL,
  `allocation_id` bigint unsigned NOT NULL,
  `mapping_id` bigint unsigned NOT NULL,
  `payroll_period_id` int unsigned NOT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `approved_input_id` bigint unsigned NOT NULL,
  `approved_input_sha256` char(64) NOT NULL,
  `adapter_version` varchar(48) NOT NULL,
  `command_key` varchar(128) NOT NULL,
  `receipt_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`receipt_id`),
  UNIQUE KEY `uq_tim1_receipt_sha` (`receipt_sha256`),
  UNIQUE KEY `uq_tim1_receipt_cmd` (`command_key`),
  UNIQUE KEY `uq_tim1_receipt_alloc` (`approval_id`,`allocation_id`),
  KEY `ix_tim1_receipt_period` (`payroll_period_id`,`calendar_period_id`,`payroll_relationship_id`,`assignment_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_legacy_attendance_receipts` (
  `legacy_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `attendance_id` bigint unsigned NOT NULL,
  `attendance_sha256` char(64) NOT NULL,
  `migration_status` varchar(24) NOT NULL,
  `regular_minutes` int unsigned DEFAULT NULL,
  `overtime_minutes` int unsigned DEFAULT NULL,
  `legacy_approved` tinyint unsigned NOT NULL,
  `raw_event_id` bigint unsigned DEFAULT NULL,
  `reviewed_entry_id` bigint unsigned DEFAULT NULL,
  `approval_id` bigint unsigned DEFAULT NULL,
  `reason_token` varchar(64) DEFAULT NULL,
  `receipt_sha256` char(64) NOT NULL,
  `executed_by` int unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`legacy_receipt_id`),
  UNIQUE KEY `uq_tim1_legacy_attendance` (`attendance_id`),
  UNIQUE KEY `uq_tim1_legacy_sha` (`receipt_sha256`),
  KEY `ix_tim1_legacy_status` (`migration_status`,`legacy_receipt_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_overtime_policies` (
 `policy_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `policy_code` varchar(64) NOT NULL, `policy_version` varchar(64) NOT NULL, `source_kind` varchar(24) NOT NULL, `source_ref` varchar(160) NOT NULL, `source_evidence_sha256` char(64) NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `priority_rank` int unsigned NOT NULL, `week_start_day` tinyint unsigned DEFAULT NULL, `weekly_scope` varchar(24) DEFAULT NULL, `weekend_days` varchar(24) DEFAULT NULL, `predecessor_policy_id` bigint unsigned DEFAULT NULL, `policy_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`policy_id`), UNIQUE KEY `uq_tim2_policy_sha` (`policy_sha256`), UNIQUE KEY `uq_tim2_policy_ver` (`legal_entity_id`,`policy_code`,`policy_version`), UNIQUE KEY `uq_tim2_policy_pred` (`predecessor_policy_id`), KEY `ix_tim2_policy_scope` (`legal_entity_id`,`effective_from`,`effective_to`,`priority_rank`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_overtime_policy_rules` (
 `rule_id` bigint unsigned NOT NULL AUTO_INCREMENT, `policy_id` bigint unsigned NOT NULL, `rule_sequence` smallint unsigned NOT NULL, `daily_threshold_minutes` int unsigned DEFAULT NULL, `weekly_threshold_minutes` int unsigned DEFAULT NULL, `weekend_required` tinyint unsigned DEFAULT NULL, `holiday_required` tinyint unsigned DEFAULT NULL, `rest_day_required` tinyint unsigned DEFAULT NULL, `approved_request_required` tinyint unsigned NOT NULL DEFAULT 0, `schedule_deviation_min_minutes` int unsigned DEFAULT NULL, `qualification_mode` varchar(32) NOT NULL, `qualification_category` varchar(64) NOT NULL, `rule_sha256` char(64) NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`rule_id`), UNIQUE KEY `uq_tim2_rule_sha` (`rule_sha256`), UNIQUE KEY `uq_tim2_rule_seq` (`policy_id`,`rule_sequence`), KEY `ix_tim2_rule_policy` (`policy_id`,`rule_sequence`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_overtime_request_receipts` (
 `request_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `request_id` int unsigned NOT NULL, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `overtime_type_id` int unsigned NOT NULL, `work_date` date NOT NULL, `requested_minutes` int unsigned NOT NULL, `request_snapshot_sha256` char(64) NOT NULL, `approval_draft_id` int unsigned DEFAULT NULL, `approval_command_key` varchar(128) NOT NULL, `approved_by_actor` varchar(64) NOT NULL, `approval_receipt_sha256` char(64) NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`request_receipt_id`), UNIQUE KEY `uq_tim2_req_sha` (`approval_receipt_sha256`), UNIQUE KEY `uq_tim2_req_id` (`request_id`), UNIQUE KEY `uq_tim2_req_cmd` (`approval_command_key`), KEY `ix_tim2_req_scope` (`legal_entity_id`,`assignment_id`,`work_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_import_connectors` (
 `connector_id` bigint unsigned NOT NULL AUTO_INCREMENT, `connector_key` varchar(64) NOT NULL, `connector_version` varchar(64) NOT NULL, `legal_entity_id` int unsigned NOT NULL, `source_system` varchar(64) NOT NULL, `channel_kind` varchar(24) NOT NULL, `auth_mode` varchar(32) NOT NULL, `max_bytes` int unsigned NOT NULL, `max_items` int unsigned NOT NULL, `allowed_mime` varchar(96) NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `predecessor_connector_id` bigint unsigned DEFAULT NULL, `connector_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`connector_id`), UNIQUE KEY `uq_tim2_con_sha` (`connector_sha256`), UNIQUE KEY `uq_tim2_con_ver` (`legal_entity_id`,`connector_key`,`connector_version`), UNIQUE KEY `uq_tim2_con_pred` (`predecessor_connector_id`), KEY `ix_tim2_con_scope` (`legal_entity_id`,`connector_key`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_import_batches` (
 `import_batch_id` bigint unsigned NOT NULL AUTO_INCREMENT, `connector_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `source_batch_key` varchar(128) NOT NULL, `content_sha256` char(64) NOT NULL, `file_name_sha256` char(64) NOT NULL, `file_size_bytes` int unsigned NOT NULL, `normalization_version` varchar(48) NOT NULL, `received_at_utc` datetime NOT NULL, `command_key` varchar(128) NOT NULL, `retry_of_batch_id` bigint unsigned DEFAULT NULL, `status_token` varchar(24) NOT NULL, `batch_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`import_batch_id`), UNIQUE KEY `uq_tim2_batch_sha` (`batch_sha256`), UNIQUE KEY `uq_tim2_batch_cmd` (`command_key`), KEY `ix_tim2_batch_source` (`connector_id`,`source_batch_key`,`import_batch_id`), KEY `ix_tim2_batch_status` (`status_token`,`import_batch_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_import_items` (
 `import_item_id` bigint unsigned NOT NULL AUTO_INCREMENT, `import_batch_id` bigint unsigned NOT NULL, `source_item_key` varchar(128) NOT NULL, `item_index` int unsigned NOT NULL, `payload_sha256` char(64) NOT NULL, `event_kind` varchar(24) NOT NULL, `effective_at_utc` datetime DEFAULT NULL, `worker_id` bigint unsigned DEFAULT NULL, `employment_id` bigint unsigned DEFAULT NULL, `assignment_id` bigint unsigned DEFAULT NULL, `legal_entity_id` int unsigned DEFAULT NULL, `raw_event_id` bigint unsigned DEFAULT NULL, `raw_event_sha256` char(64) DEFAULT NULL, `predecessor_item_id` bigint unsigned DEFAULT NULL, `status_token` varchar(24) NOT NULL, `item_sha256` char(64) NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`import_item_id`), UNIQUE KEY `uq_tim2_item_sha` (`item_sha256`), UNIQUE KEY `uq_tim2_item_batchkey` (`import_batch_id`,`source_item_key`), UNIQUE KEY `uq_tim2_item_pred` (`predecessor_item_id`), KEY `ix_tim2_item_source` (`source_item_key`,`import_item_id`), KEY `ix_tim2_item_status` (`status_token`,`import_item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_import_errors` (
 `import_error_id` bigint unsigned NOT NULL AUTO_INCREMENT, `import_batch_id` bigint unsigned NOT NULL, `import_item_id` bigint unsigned DEFAULT NULL, `source_item_key` varchar(128) DEFAULT NULL, `error_code` varchar(64) NOT NULL, `error_context_sha256` char(64) NOT NULL, `error_sha256` char(64) NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`import_error_id`), UNIQUE KEY `uq_tim2_err_sha` (`error_sha256`), KEY `ix_tim2_err_batch` (`import_batch_id`,`import_error_id`), KEY `ix_tim2_err_code` (`error_code`,`import_error_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_overtime_qualifications` (
 `qualification_id` bigint unsigned NOT NULL AUTO_INCREMENT, `policy_id` bigint unsigned NOT NULL, `policy_sha256` char(64) NOT NULL, `approval_id` bigint unsigned NOT NULL, `reviewed_entry_id` bigint unsigned NOT NULL, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `request_receipt_id` bigint unsigned DEFAULT NULL, `work_date` date NOT NULL, `source_minutes` int unsigned NOT NULL, `daily_approved_minutes` int unsigned NOT NULL, `weekly_approved_minutes` int unsigned NOT NULL, `scheduled_minutes` int unsigned NOT NULL, `is_weekend` tinyint unsigned NOT NULL, `is_holiday` tinyint unsigned NOT NULL, `is_rest_day` tinyint unsigned NOT NULL, `day_context_sha256` char(64) NOT NULL, `qualified_minutes` int unsigned NOT NULL, `qualification_status` varchar(24) NOT NULL, `qualification_category` varchar(64) DEFAULT NULL, `reason_token` varchar(64) NOT NULL, `input_sha256` char(64) NOT NULL, `predecessor_qualification_id` bigint unsigned DEFAULT NULL, `qualification_sha256` char(64) NOT NULL, `qualified_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`qualification_id`), UNIQUE KEY `uq_tim2_qual_sha` (`qualification_sha256`), UNIQUE KEY `uq_tim2_qual_pred` (`predecessor_qualification_id`), KEY `ix_tim2_qual_approval` (`approval_id`,`qualification_id`), KEY `ix_tim2_qual_scope` (`legal_entity_id`,`assignment_id`,`work_date`), KEY `ix_tim2_qual_status` (`qualification_status`,`qualification_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_overtime_payroll_mappings` (
 `mapping_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `qualification_category` varchar(64) NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `element_version_id` bigint unsigned NOT NULL, `input_definition_id` bigint unsigned NOT NULL, `unit_token` varchar(24) NOT NULL, `predecessor_mapping_id` bigint unsigned DEFAULT NULL, `mapping_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`mapping_id`), UNIQUE KEY `uq_tim2_map_sha` (`mapping_sha256`), UNIQUE KEY `uq_tim2_map_from` (`legal_entity_id`,`qualification_category`,`effective_from`), UNIQUE KEY `uq_tim2_map_pred` (`predecessor_mapping_id`), KEY `ix_tim2_map_scope` (`legal_entity_id`,`qualification_category`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_time_overtime_payroll_receipts` (
 `receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `qualification_id` bigint unsigned NOT NULL, `mapping_id` bigint unsigned NOT NULL, `payroll_period_id` int unsigned NOT NULL, `calendar_period_id` bigint unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `approved_input_id` bigint unsigned NOT NULL, `approved_input_sha256` char(64) NOT NULL, `adapter_version` varchar(48) NOT NULL, `command_key` varchar(128) NOT NULL, `receipt_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`receipt_id`), UNIQUE KEY `uq_tim2_pay_sha` (`receipt_sha256`), UNIQUE KEY `uq_tim2_pay_cmd` (`command_key`), UNIQUE KEY `uq_tim2_pay_qual` (`qualification_id`), KEY `ix_tim2_pay_period` (`payroll_period_id`,`calendar_period_id`,`payroll_relationship_id`,`assignment_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_policy_versions` (
 `policy_version_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `policy_code` varchar(64) NOT NULL, `version_token` varchar(64) NOT NULL, `leave_id` int unsigned NOT NULL, `source_kind` varchar(32) NOT NULL, `source_ref` varchar(160) NOT NULL, `source_evidence_sha256` char(64) NOT NULL, `employment_type` int DEFAULT NULL, `grade_id` int unsigned DEFAULT NULL, `assignment_id` bigint unsigned DEFAULT NULL, `work_location_id` bigint unsigned DEFAULT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `priority_rank` int NOT NULL, `unit_code` varchar(16) NOT NULL, `cycle_basis` varchar(24) NOT NULL, `entitlement_mode` varchar(24) NOT NULL, `entitlement_units` decimal(20,6) DEFAULT NULL, `accrual_period` varchar(24) DEFAULT NULL, `waiting_days` int unsigned NOT NULL, `proration_mode` varchar(32) NOT NULL, `reference_cycle_minutes` int unsigned DEFAULT NULL, `carryover_enabled` tinyint(1) NOT NULL, `carryover_cap_units` decimal(20,6) DEFAULT NULL, `expiry_days` int unsigned DEFAULT NULL, `carryover_consumption_order` varchar(24) DEFAULT NULL, `negative_balance_limit_units` decimal(20,6) NOT NULL, `donation_allowed` tinyint(1) NOT NULL, `cashout_allowed` tinyint(1) NOT NULL, `min_request_units` decimal(20,6) DEFAULT NULL, `rounding_scale` tinyint unsigned NOT NULL, `predecessor_policy_version_id` bigint unsigned DEFAULT NULL, `policy_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`policy_version_id`), UNIQUE KEY `uq_lve1_pol_sha` (`policy_sha256`), UNIQUE KEY `uq_lve1_pol_ver` (`legal_entity_id`,`policy_code`,`version_token`), UNIQUE KEY `uq_lve1_pol_pred` (`predecessor_policy_version_id`), KEY `ix_lve1_pol_match` (`legal_entity_id`,`leave_id`,`effective_from`,`effective_to`,`priority_rank`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_entitlements` (
 `entitlement_id` bigint unsigned NOT NULL AUTO_INCREMENT, `policy_version_id` bigint unsigned NOT NULL, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `leave_id` int unsigned NOT NULL, `cycle_start` date NOT NULL, `cycle_end` date NOT NULL, `unit_code` varchar(16) NOT NULL, `entitlement_units` decimal(20,6) NOT NULL, `policy_sha256` char(64) NOT NULL, `schedule_sha256` char(64) NOT NULL, `command_key` varchar(128) NOT NULL, `entitlement_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`entitlement_id`), UNIQUE KEY `uq_lve1_ent_cmd` (`command_key`), UNIQUE KEY `uq_lve1_ent_sha` (`entitlement_sha256`), UNIQUE KEY `uq_lve1_ent_cycle` (`policy_version_id`,`worker_id`,`assignment_id`,`cycle_start`), KEY `ix_lve1_ent_scope` (`worker_id`,`employment_id`,`assignment_id`,`legal_entity_id`,`leave_id`,`cycle_start`,`cycle_end`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_movements` (
 `movement_id` bigint unsigned NOT NULL AUTO_INCREMENT, `entitlement_id` bigint unsigned NOT NULL, `policy_version_id` bigint unsigned NOT NULL, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `leave_id` int unsigned NOT NULL, `effective_date` date NOT NULL, `unit_code` varchar(16) NOT NULL, `movement_kind` varchar(24) NOT NULL, `units` decimal(20,6) NOT NULL, `source_type` varchar(48) NOT NULL, `source_key` varchar(128) NOT NULL, `source_sha256` char(64) NOT NULL, `request_id` bigint unsigned DEFAULT NULL, `predecessor_movement_id` bigint unsigned DEFAULT NULL, `command_key` varchar(160) NOT NULL, `lineage_sha256` char(64) NOT NULL, `movement_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`movement_id`), UNIQUE KEY `uq_lve1_mov_cmd` (`command_key`), UNIQUE KEY `uq_lve1_mov_sha` (`movement_sha256`), UNIQUE KEY `uq_lve1_mov_pred` (`predecessor_movement_id`), KEY `ix_lve1_mov_ent` (`entitlement_id`,`effective_date`,`movement_id`), KEY `ix_lve1_mov_req` (`request_id`,`movement_kind`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_request_receipts` (
 `request_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `request_id` bigint unsigned NOT NULL, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `leave_id` int unsigned NOT NULL, `requested_from` date NOT NULL, `requested_to` date NOT NULL, `requested_units` decimal(20,6) NOT NULL, `unit_code` varchar(16) NOT NULL, `policy_set_sha256` char(64) NOT NULL, `schedule_sha256` char(64) NOT NULL, `request_snapshot_sha256` char(64) NOT NULL, `reservation_command_key` varchar(160) NOT NULL, `receipt_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`request_receipt_id`), UNIQUE KEY `uq_lve1_req_id` (`request_id`), UNIQUE KEY `uq_lve1_req_cmd` (`reservation_command_key`), UNIQUE KEY `uq_lve1_req_sha` (`receipt_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_opening_inventory` (
 `opening_inventory_id` bigint unsigned NOT NULL AUTO_INCREMENT, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `leave_id` int unsigned NOT NULL, `fiscal_year` int NOT NULL, `legacy_balance_id` bigint unsigned NOT NULL, `legacy_balance_sha256` char(64) NOT NULL, `as_of_date` date NOT NULL, `unit_code` varchar(16) NOT NULL, `entitled_units` decimal(20,6) NOT NULL, `carried_units` decimal(20,6) NOT NULL, `taken_units` decimal(20,6) NOT NULL, `pending_units` decimal(20,6) NOT NULL, `adjusted_units` decimal(20,6) NOT NULL, `provenance_state` varchar(32) NOT NULL, `evidence_sha256` char(64) NOT NULL, `inventory_sha256` char(64) NOT NULL, `inventoried_by` int unsigned NOT NULL, `reviewed_by` int unsigned DEFAULT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`opening_inventory_id`), UNIQUE KEY `uq_lve1_opn_legacy` (`legacy_balance_id`), UNIQUE KEY `uq_lve1_opn_sha` (`inventory_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_projection_snapshots` (
 `projection_id` bigint unsigned NOT NULL AUTO_INCREMENT, `entitlement_id` bigint unsigned NOT NULL, `as_of_date` date NOT NULL, `available_units` decimal(20,6) NOT NULL, `reserved_units` decimal(20,6) NOT NULL, `consumed_units` decimal(20,6) NOT NULL, `accrued_units` decimal(20,6) NOT NULL, `carried_units` decimal(20,6) NOT NULL, `expired_units` decimal(20,6) NOT NULL, `donated_units` decimal(20,6) NOT NULL, `cashed_out_units` decimal(20,6) NOT NULL, `movement_set_sha256` char(64) NOT NULL, `projection_sha256` char(64) NOT NULL, `built_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`projection_id`), UNIQUE KEY `uq_lve1_prj_sha` (`projection_sha256`), KEY `ix_lve1_prj_ent` (`entitlement_id`,`as_of_date`,`projection_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_reconciliations` (
 `reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT, `projection_id` bigint unsigned NOT NULL, `legacy_balance_id` bigint unsigned DEFAULT NULL, `legacy_balance_sha256` char(64) DEFAULT NULL, `difference_units` decimal(20,6) NOT NULL, `reconciliation_state` varchar(32) NOT NULL, `evidence_sha256` char(64) NOT NULL, `reconciliation_sha256` char(64) NOT NULL, `reviewed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`reconciliation_id`), UNIQUE KEY `uq_lve1_rec_sha` (`reconciliation_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_recovery_evidence` (
 `recovery_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT, `reconciliation_id` bigint unsigned NOT NULL, `action_token` varchar(64) NOT NULL, `source_sha256` char(64) NOT NULL, `evidence_sha256` char(64) NOT NULL, `recovery_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`recovery_evidence_id`), UNIQUE KEY `uq_lve1_rcv_sha` (`recovery_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_payroll_mappings` (
 `mapping_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `leave_id` int unsigned NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `element_version_id` bigint unsigned NOT NULL, `input_definition_id` bigint unsigned NOT NULL, `unit_code` varchar(16) NOT NULL, `predecessor_mapping_id` bigint unsigned DEFAULT NULL, `mapping_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`mapping_id`), UNIQUE KEY `uq_lve1_map_sha` (`mapping_sha256`), UNIQUE KEY `uq_lve1_map_from` (`legal_entity_id`,`leave_id`,`effective_from`), UNIQUE KEY `uq_lve1_map_pred` (`predecessor_mapping_id`), KEY `ix_lve1_map_scope` (`legal_entity_id`,`leave_id`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_leave_payroll_handoffs` (
 `handoff_id` bigint unsigned NOT NULL AUTO_INCREMENT, `movement_id` bigint unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `payroll_period_id` bigint unsigned NOT NULL, `calendar_period_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `unit_code` varchar(16) NOT NULL, `approved_input_id` bigint unsigned NOT NULL, `approved_input_sha256` char(64) NOT NULL, `adapter_version` varchar(48) NOT NULL, `command_key` varchar(160) NOT NULL, `handoff_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`handoff_id`), UNIQUE KEY `uq_lve1_pay_mov` (`movement_id`), UNIQUE KEY `uq_lve1_pay_cmd` (`command_key`), UNIQUE KEY `uq_lve1_pay_sha` (`handoff_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_salary_ranges` (
 `range_version_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `grade_id` int unsigned NOT NULL, `range_code` varchar(64) NOT NULL, `version_token` varchar(64) NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `currency_code` char(3) NOT NULL, `pay_basis` varchar(24) NOT NULL, `minimum_amount` decimal(24,6) NOT NULL, `midpoint_amount` decimal(24,6) NOT NULL, `maximum_amount` decimal(24,6) NOT NULL, `source_kind` varchar(32) NOT NULL, `source_ref` varchar(160) NOT NULL, `source_evidence_sha256` char(64) NOT NULL, `predecessor_range_version_id` bigint unsigned DEFAULT NULL, `range_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`range_version_id`), UNIQUE KEY `uq_cmp1_rng_sha` (`range_sha256`), UNIQUE KEY `uq_cmp1_rng_ver` (`legal_entity_id`,`range_code`,`version_token`), UNIQUE KEY `uq_cmp1_rng_pred` (`predecessor_range_version_id`), KEY `ix_cmp1_rng_match` (`legal_entity_id`,`grade_id`,`currency_code`,`pay_basis`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_review_cycles` (
 `review_cycle_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `cycle_code` varchar(64) NOT NULL, `version_token` varchar(64) NOT NULL, `open_date` date NOT NULL, `close_date` date NOT NULL, `award_effective_from` date NOT NULL, `award_effective_to` date NOT NULL, `status_token` varchar(16) NOT NULL, `purpose_code` varchar(64) NOT NULL, `evidence_sha256` char(64) NOT NULL, `predecessor_review_cycle_id` bigint unsigned DEFAULT NULL, `cycle_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`review_cycle_id`), UNIQUE KEY `uq_cmp1_cyc_sha` (`cycle_sha256`), UNIQUE KEY `uq_cmp1_cyc_ver` (`legal_entity_id`,`cycle_code`,`version_token`), UNIQUE KEY `uq_cmp1_cyc_pred` (`predecessor_review_cycle_id`), KEY `ix_cmp1_cyc_scope` (`legal_entity_id`,`status_token`,`open_date`,`close_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_budgets` (
 `budget_version_id` bigint unsigned NOT NULL AUTO_INCREMENT, `review_cycle_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `currency_code` char(3) NOT NULL, `budget_amount` decimal(24,6) NOT NULL, `evidence_sha256` char(64) NOT NULL, `predecessor_budget_version_id` bigint unsigned DEFAULT NULL, `budget_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`budget_version_id`), UNIQUE KEY `uq_cmp1_bud_sha` (`budget_sha256`), UNIQUE KEY `uq_cmp1_bud_pred` (`predecessor_budget_version_id`), KEY `ix_cmp1_bud_scope` (`review_cycle_id`,`legal_entity_id`,`currency_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_opening_inventory` (
 `opening_inventory_id` bigint unsigned NOT NULL AUTO_INCREMENT, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `employee_id` varchar(20) NOT NULL, `as_of_date` date NOT NULL, `legacy_source_kind` varchar(32) NOT NULL, `legacy_source_row_id` bigint unsigned NOT NULL, `legacy_source_sha256` char(64) NOT NULL, `base_amount` decimal(24,6) NOT NULL, `currency_code` char(3) NOT NULL, `pay_basis` varchar(24) NOT NULL, `basic_element_id` int unsigned NOT NULL, `evidence_sha256` char(64) NOT NULL, `inventory_sha256` char(64) NOT NULL, `inventoried_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`opening_inventory_id`), UNIQUE KEY `uq_cmp1_opn_sha` (`inventory_sha256`), UNIQUE KEY `uq_cmp1_opn_scope` (`assignment_id`,`as_of_date`), KEY `ix_cmp1_opn_employee` (`employee_id`,`as_of_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_opening_reviews` (
 `opening_review_id` bigint unsigned NOT NULL AUTO_INCREMENT, `opening_inventory_id` bigint unsigned NOT NULL, `inventory_sha256` char(64) NOT NULL, `decision_token` varchar(16) NOT NULL, `evidence_sha256` char(64) NOT NULL, `review_sha256` char(64) NOT NULL, `reviewed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`opening_review_id`), UNIQUE KEY `uq_cmp1_opr_sha` (`review_sha256`), UNIQUE KEY `uq_cmp1_opr_inv` (`opening_inventory_id`), KEY `ix_cmp1_opr_dec` (`decision_token`,`opening_inventory_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_cutover_plans` (
 `cutover_plan_id` bigint unsigned NOT NULL AUTO_INCREMENT, `opening_review_id` bigint unsigned NOT NULL, `opening_inventory_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `employee_id` varchar(20) NOT NULL, `effective_date` date NOT NULL, `request_key` varchar(128) NOT NULL, `opening_review_sha256` char(64) NOT NULL, `plan_sha256` char(64) NOT NULL, `maker_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`cutover_plan_id`), UNIQUE KEY `uq_cmp1_cut_plan_sha` (`plan_sha256`), UNIQUE KEY `uq_cmp1_cut_key` (`request_key`), UNIQUE KEY `uq_cmp1_cut_assignment` (`assignment_id`), KEY `ix_cmp1_cut_employee` (`employee_id`,`effective_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_cutover_approvals` (
 `cutover_approval_id` bigint unsigned NOT NULL AUTO_INCREMENT, `cutover_plan_id` bigint unsigned NOT NULL, `plan_sha256` char(64) NOT NULL, `decision_token` varchar(16) NOT NULL, `evidence_sha256` char(64) NOT NULL, `approval_sha256` char(64) NOT NULL, `checker_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`cutover_approval_id`), UNIQUE KEY `uq_cmp1_cut_app_sha` (`approval_sha256`), UNIQUE KEY `uq_cmp1_cut_app_plan` (`cutover_plan_id`), KEY `ix_cmp1_cut_app_dec` (`decision_token`,`cutover_plan_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_cutover_receipts` (
 `cutover_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `cutover_plan_id` bigint unsigned NOT NULL, `cutover_approval_id` bigint unsigned NOT NULL, `opening_inventory_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `employee_id` varchar(20) NOT NULL, `effective_date` date NOT NULL, `projection_salary_id` bigint unsigned DEFAULT NULL, `cutover_sha256` char(64) NOT NULL, `executor_id` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`cutover_receipt_id`), UNIQUE KEY `uq_cmp1_cut_rcp_sha` (`cutover_sha256`), UNIQUE KEY `uq_cmp1_cut_rcp_plan` (`cutover_plan_id`), UNIQUE KEY `uq_cmp1_cut_rcp_assignment` (`assignment_id`), KEY `ix_cmp1_cut_rcp_employee` (`employee_id`,`effective_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_award_commands` (
 `award_command_id` bigint unsigned NOT NULL AUTO_INCREMENT, `worker_id` bigint unsigned NOT NULL, `employment_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `legal_entity_id` int unsigned NOT NULL, `employee_id` varchar(20) NOT NULL, `review_cycle_id` bigint unsigned NOT NULL, `range_version_id` bigint unsigned NOT NULL, `budget_version_id` bigint unsigned NOT NULL, `award_kind` varchar(24) NOT NULL, `effective_date` date NOT NULL, `old_amount` decimal(24,6) NOT NULL, `new_amount` decimal(24,6) NOT NULL, `currency_code` char(3) NOT NULL, `pay_basis` varchar(24) NOT NULL, `budget_impact` decimal(24,6) NOT NULL, `purpose_code` varchar(64) NOT NULL, `evidence_sha256` char(64) NOT NULL, `idempotency_hash` char(64) NOT NULL, `request_sha256` char(64) NOT NULL, `command_status` varchar(16) NOT NULL, `row_version` int unsigned NOT NULL, `approval_draft_id` bigint unsigned DEFAULT NULL, `requested_by` int unsigned NOT NULL, `approved_by` int unsigned DEFAULT NULL, `approved_at` datetime DEFAULT NULL, `executed_by` int unsigned DEFAULT NULL, `completed_at` datetime DEFAULT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`award_command_id`), UNIQUE KEY `uq_cmp1_cmd_idem` (`idempotency_hash`), UNIQUE KEY `uq_cmp1_cmd_req` (`request_sha256`), KEY `ix_cmp1_cmd_scope` (`assignment_id`,`effective_date`,`command_status`), KEY `ix_cmp1_cmd_draft` (`approval_draft_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_award_receipts` (
 `award_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `award_command_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `review_cycle_id` bigint unsigned NOT NULL, `range_version_id` bigint unsigned NOT NULL, `budget_version_id` bigint unsigned NOT NULL, `award_kind` varchar(24) NOT NULL, `effective_date` date NOT NULL, `old_amount` decimal(24,6) NOT NULL, `new_amount` decimal(24,6) NOT NULL, `currency_code` char(3) NOT NULL, `pay_basis` varchar(24) NOT NULL, `budget_impact` decimal(24,6) NOT NULL, `purpose_code` varchar(64) NOT NULL, `request_sha256` char(64) NOT NULL, `predecessor_award_receipt_id` bigint unsigned DEFAULT NULL, `execution_key_hash` char(64) NOT NULL, `receipt_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`award_receipt_id`), UNIQUE KEY `uq_cmp1_awd_sha` (`receipt_sha256`), UNIQUE KEY `uq_cmp1_awd_cmd` (`award_command_id`), UNIQUE KEY `uq_cmp1_awd_pred` (`predecessor_award_receipt_id`), KEY `ix_cmp1_awd_scope` (`assignment_id`,`effective_date`,`award_receipt_id`), KEY `ix_cmp1_awd_budget` (`budget_version_id`,`currency_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_legacy_projection_receipts` (
 `projection_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT, `award_receipt_id` bigint unsigned NOT NULL, `employee_id` varchar(20) NOT NULL, `basic_element_id` int unsigned NOT NULL, `employee_salary_id` bigint unsigned NOT NULL, `effective_date` date NOT NULL, `amount` decimal(24,6) NOT NULL, `source_sha256` char(64) NOT NULL, `projection_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`projection_receipt_id`), UNIQUE KEY `uq_cmp1_prj_sha` (`projection_sha256`), UNIQUE KEY `uq_cmp1_prj_award` (`award_receipt_id`), UNIQUE KEY `uq_cmp1_prj_salary` (`employee_salary_id`), KEY `ix_cmp1_prj_employee` (`employee_id`,`effective_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_payroll_mappings` (
 `mapping_id` bigint unsigned NOT NULL AUTO_INCREMENT, `legal_entity_id` int unsigned NOT NULL, `currency_code` char(3) NOT NULL, `pay_basis` varchar(24) NOT NULL, `effective_from` date NOT NULL, `effective_to` date DEFAULT NULL, `element_version_id` bigint unsigned NOT NULL, `input_definition_id` bigint unsigned NOT NULL, `predecessor_mapping_id` bigint unsigned DEFAULT NULL, `mapping_sha256` char(64) NOT NULL, `created_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`mapping_id`), UNIQUE KEY `uq_cmp1_map_sha` (`mapping_sha256`), UNIQUE KEY `uq_cmp1_map_pred` (`predecessor_mapping_id`), KEY `ix_cmp1_map_scope` (`legal_entity_id`,`currency_code`,`pay_basis`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_payroll_handoffs` (
 `handoff_id` bigint unsigned NOT NULL AUTO_INCREMENT, `award_receipt_id` bigint unsigned NOT NULL, `payroll_relationship_id` bigint unsigned NOT NULL, `payroll_period_id` int unsigned NOT NULL, `calendar_period_id` bigint unsigned NOT NULL, `assignment_id` bigint unsigned NOT NULL, `approved_input_id` bigint unsigned NOT NULL, `approved_input_sha256` char(64) NOT NULL, `adapter_version` varchar(64) NOT NULL, `command_key` varchar(128) NOT NULL, `handoff_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`handoff_id`), UNIQUE KEY `uq_cmp1_pay_sha` (`handoff_sha256`), UNIQUE KEY `uq_cmp1_pay_cmd` (`command_key`), UNIQUE KEY `uq_cmp1_pay_award` (`award_receipt_id`), KEY `ix_cmp1_pay_rel` (`payroll_relationship_id`,`payroll_period_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_reconciliations` (
 `reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT, `assignment_id` bigint unsigned NOT NULL, `as_of_date` date NOT NULL, `governed_amount` decimal(24,6) NOT NULL, `legacy_amount` decimal(24,6) NOT NULL, `difference_amount` decimal(24,6) NOT NULL, `status_token` varchar(24) NOT NULL, `evidence_sha256` char(64) NOT NULL, `predecessor_reconciliation_id` bigint unsigned DEFAULT NULL, `reconciliation_sha256` char(64) NOT NULL, `reviewed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`reconciliation_id`), UNIQUE KEY `uq_cmp1_rec_sha` (`reconciliation_sha256`), UNIQUE KEY `uq_cmp1_rec_pred` (`predecessor_reconciliation_id`), KEY `ix_cmp1_rec_scope` (`assignment_id`,`as_of_date`,`status_token`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_comp_recovery_evidence` (
 `recovery_evidence_id` bigint unsigned NOT NULL AUTO_INCREMENT, `reconciliation_id` bigint unsigned NOT NULL, `action_token` varchar(48) NOT NULL, `source_sha256` char(64) NOT NULL, `evidence_sha256` char(64) NOT NULL, `recovery_sha256` char(64) NOT NULL, `executed_by` int unsigned NOT NULL, `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (`recovery_evidence_id`), UNIQUE KEY `uq_cmp1_rcv_sha` (`recovery_sha256`), KEY `ix_cmp1_rcv_rec` (`reconciliation_id`,`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_pack_adapter_versions` (
  `pack_adapter_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `adapter_code` varchar(64) NOT NULL,
  `version_token` varchar(64) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `capability_json` longtext NOT NULL,
  `data_contract_sha256` char(64) NOT NULL,
  `mapping_sha256` char(64) NOT NULL,
  `predecessor_adapter_version_id` bigint unsigned NULL,
  `adapter_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`pack_adapter_version_id`),
  UNIQUE KEY `uq_ben1_pad_sha` (`adapter_sha256`),
  UNIQUE KEY `uq_ben1_pad_ver` (`adapter_code`,`version_token`),
  UNIQUE KEY `uq_ben1_pad_pred` (`predecessor_adapter_version_id`),
  KEY `ix_ben1_pad_eff` (`adapter_code`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_provider_adapter_versions` (
  `provider_adapter_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `adapter_code` varchar(64) NOT NULL,
  `version_token` varchar(64) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `capability_json` longtext NOT NULL,
  `data_contract_sha256` char(64) NOT NULL,
  `mapping_sha256` char(64) NOT NULL,
  `minimization_sha256` char(64) NOT NULL,
  `predecessor_adapter_version_id` bigint unsigned NULL,
  `adapter_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`provider_adapter_version_id`),
  UNIQUE KEY `uq_ben1_prv_sha` (`adapter_sha256`),
  UNIQUE KEY `uq_ben1_prv_ver` (`adapter_code`,`version_token`),
  UNIQUE KEY `uq_ben1_prv_pred` (`predecessor_adapter_version_id`),
  KEY `ix_ben1_prv_eff` (`adapter_code`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_plans` (
  `plan_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `legal_entity_id` bigint unsigned NOT NULL,
  `plan_code` varchar(64) NOT NULL,
  `identity_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`plan_id`),
  UNIQUE KEY `uq_ben1_plan_code` (`legal_entity_id`,`plan_code`),
  UNIQUE KEY `uq_ben1_plan_sha` (`identity_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_plan_versions` (
  `plan_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `plan_id` bigint unsigned NOT NULL,
  `version_token` varchar(64) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `status_token` varchar(24) NOT NULL,
  `coverage_basis_code` varchar(64) NOT NULL,
  `contribution_basis_code` varchar(64) NOT NULL,
  `pack_adapter_version_id` bigint unsigned NULL,
  `provider_adapter_version_id` bigint unsigned NULL,
  `source_evidence_sha256` char(64) NOT NULL,
  `predecessor_plan_version_id` bigint unsigned NULL,
  `version_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`plan_version_id`),
  UNIQUE KEY `uq_ben1_pv_sha` (`version_sha256`),
  UNIQUE KEY `uq_ben1_pv_ver` (`plan_id`,`version_token`),
  UNIQUE KEY `uq_ben1_pv_pred` (`predecessor_plan_version_id`),
  KEY `ix_ben1_pv_eff` (`plan_id`,`effective_from`,`effective_to`,`status_token`),
  CONSTRAINT `fk_ben1_pvr_1` FOREIGN KEY (`plan_id`) REFERENCES `0_hrm_ben_plans` (`plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_pvr_2` FOREIGN KEY (`pack_adapter_version_id`) REFERENCES `0_hrm_ben_pack_adapter_versions` (`pack_adapter_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_pvr_3` FOREIGN KEY (`provider_adapter_version_id`) REFERENCES `0_hrm_ben_provider_adapter_versions` (`provider_adapter_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_pvr_4` FOREIGN KEY (`predecessor_plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_eligibility_versions` (
  `eligibility_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `plan_version_id` bigint unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `resolver_code` varchar(32) NOT NULL,
  `rule_payload_json` longtext NOT NULL,
  `rule_artifact_sha256` char(64) NOT NULL,
  `predecessor_eligibility_version_id` bigint unsigned NULL,
  `eligibility_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`eligibility_version_id`),
  UNIQUE KEY `uq_ben1_elig_sha` (`eligibility_sha256`),
  UNIQUE KEY `uq_ben1_elig_pred` (`predecessor_eligibility_version_id`),
  KEY `ix_ben1_elig_eff` (`plan_version_id`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ben1_eli_1` FOREIGN KEY (`plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_eli_2` FOREIGN KEY (`predecessor_eligibility_version_id`) REFERENCES `0_hrm_ben_eligibility_versions` (`eligibility_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_enrollment_windows` (
  `window_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `plan_version_id` bigint unsigned NOT NULL,
  `window_code` varchar(64) NOT NULL,
  `window_start` date NOT NULL,
  `window_end` date NOT NULL,
  `qualifying_basis_code` varchar(64) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `predecessor_window_id` bigint unsigned NULL,
  `window_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`window_id`),
  UNIQUE KEY `uq_ben1_win_sha` (`window_sha256`),
  UNIQUE KEY `uq_ben1_win_code` (`plan_version_id`,`window_code`),
  UNIQUE KEY `uq_ben1_win_pred` (`predecessor_window_id`),
  KEY `ix_ben1_win_date` (`plan_version_id`,`window_start`,`window_end`),
  CONSTRAINT `fk_ben1_win_1` FOREIGN KEY (`plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_win_2` FOREIGN KEY (`predecessor_window_id`) REFERENCES `0_hrm_ben_enrollment_windows` (`window_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_consents` (
  `consent_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint unsigned NOT NULL,
  `dependent_id` bigint unsigned NULL,
  `purpose_code` varchar(64) NOT NULL,
  `decision_token` varchar(24) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `evidence_sha256` char(64) NOT NULL,
  `predecessor_consent_id` bigint unsigned NULL,
  `consent_sha256` char(64) NOT NULL,
  `recorded_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`consent_id`),
  UNIQUE KEY `uq_ben1_con_sha` (`consent_sha256`),
  UNIQUE KEY `uq_ben1_con_pred` (`predecessor_consent_id`),
  KEY `ix_ben1_con_scope` (`worker_id`,`dependent_id`,`purpose_code`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_enrollments` (
  `enrollment_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint unsigned NOT NULL,
  `employment_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `plan_id` bigint unsigned NOT NULL,
  `plan_version_id` bigint unsigned NOT NULL,
  `eligibility_version_id` bigint unsigned NOT NULL,
  `window_id` bigint unsigned NOT NULL,
  `enrollment_state` varchar(24) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `qualifying_basis_code` varchar(64) NOT NULL,
  `election_evidence_sha256` char(64) NOT NULL,
  `predecessor_enrollment_id` bigint unsigned NULL,
  `idempotency_hash` char(64) NOT NULL,
  `enrollment_sha256` char(64) NOT NULL,
  `recorded_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`enrollment_id`),
  UNIQUE KEY `uq_ben1_enr_sha` (`enrollment_sha256`),
  UNIQUE KEY `uq_ben1_enr_idem` (`idempotency_hash`),
  UNIQUE KEY `uq_ben1_enr_pred` (`predecessor_enrollment_id`),
  KEY `ix_ben1_enr_scope` (`worker_id`,`plan_id`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ben1_enr_1` FOREIGN KEY (`plan_id`) REFERENCES `0_hrm_ben_plans` (`plan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_enr_2` FOREIGN KEY (`plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_enr_3` FOREIGN KEY (`eligibility_version_id`) REFERENCES `0_hrm_ben_eligibility_versions` (`eligibility_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_enr_4` FOREIGN KEY (`window_id`) REFERENCES `0_hrm_ben_enrollment_windows` (`window_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_enr_5` FOREIGN KEY (`predecessor_enrollment_id`) REFERENCES `0_hrm_ben_enrollments` (`enrollment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_enrollment_dependants` (
  `enrollment_dependant_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `enrollment_id` bigint unsigned NOT NULL,
  `dependent_id` bigint unsigned NULL,
  `consent_id` bigint unsigned NOT NULL,
  `coverage_relation_code` varchar(64) NOT NULL,
  `privacy_classification` varchar(64) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `relationship_evidence_sha256` char(64) NOT NULL,
  `binding_sha256` char(64) NOT NULL,
  `recorded_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`enrollment_dependant_id`),
  UNIQUE KEY `uq_ben1_dep_sha` (`binding_sha256`),
  UNIQUE KEY `uq_ben1_dep_scope` (`enrollment_id`,`dependent_id`,`effective_from`),
  KEY `ix_ben1_dep_priv` (`dependent_id`,`privacy_classification`),
  CONSTRAINT `fk_ben1_dep_1` FOREIGN KEY (`enrollment_id`) REFERENCES `0_hrm_ben_enrollments` (`enrollment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_dep_2` FOREIGN KEY (`consent_id`) REFERENCES `0_hrm_ben_consents` (`consent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_life_event_type_versions` (
  `life_event_type_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `plan_version_id` bigint unsigned NOT NULL,
  `event_type_code` varchar(64) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `approval_required` tinyint(1) NOT NULL,
  `configuration_sha256` char(64) NOT NULL,
  `predecessor_event_type_version_id` bigint unsigned NOT NULL,
  `event_type_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`life_event_type_version_id`),
  UNIQUE KEY `uq_ben1_let_sha` (`event_type_sha256`),
  UNIQUE KEY `uq_ben1_let_pred` (`predecessor_event_type_version_id`),
  KEY `ix_ben1_let_eff` (`plan_version_id`,`event_type_code`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ben1_let_1` FOREIGN KEY (`plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_let_2` FOREIGN KEY (`predecessor_event_type_version_id`) REFERENCES `0_hrm_ben_life_event_type_versions` (`life_event_type_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_life_event_requests` (
  `life_event_request_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `worker_id` bigint unsigned NOT NULL,
  `employment_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `life_event_type_version_id` bigint unsigned NOT NULL,
  `enrollment_before_id` bigint unsigned NULL,
  `enrollment_after_id` bigint unsigned NULL,
  `occurrence_date` date NOT NULL,
  `effective_date` date NOT NULL,
  `event_evidence_sha256` char(64) NOT NULL,
  `idempotency_hash` char(64) NOT NULL,
  `request_sha256` char(64) NOT NULL,
  `requested_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`life_event_request_id`),
  UNIQUE KEY `uq_ben1_ler_sha` (`request_sha256`),
  UNIQUE KEY `uq_ben1_ler_idem` (`idempotency_hash`),
  KEY `ix_ben1_ler_scope` (`worker_id`,`effective_date`,`life_event_type_version_id`),
  CONSTRAINT `fk_ben1_ler_1` FOREIGN KEY (`life_event_type_version_id`) REFERENCES `0_hrm_ben_life_event_type_versions` (`life_event_type_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_ler_2` FOREIGN KEY (`enrollment_before_id`) REFERENCES `0_hrm_ben_enrollments` (`enrollment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_ler_3` FOREIGN KEY (`enrollment_after_id`) REFERENCES `0_hrm_ben_enrollments` (`enrollment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_life_event_reviews` (
  `life_event_review_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `life_event_request_id` bigint unsigned NOT NULL,
  `request_sha256` char(64) NOT NULL,
  `decision_token` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `review_sha256` char(64) NULL,
  `reviewed_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`life_event_review_id`),
  UNIQUE KEY `uq_ben1_lrv_sha` (`review_sha256`),
  UNIQUE KEY `uq_ben1_lrv_req` (`life_event_request_id`),
  CONSTRAINT `fk_ben1_lrv_1` FOREIGN KEY (`life_event_request_id`) REFERENCES `0_hrm_ben_life_event_requests` (`life_event_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_life_events` (
  `life_event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `life_event_request_id` bigint unsigned NOT NULL,
  `life_event_review_id` bigint unsigned NULL,
  `worker_id` bigint unsigned NOT NULL,
  `employment_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `legal_entity_id` bigint unsigned NOT NULL,
  `life_event_type_version_id` bigint unsigned NOT NULL,
  `enrollment_before_id` bigint unsigned NULL,
  `enrollment_after_id` bigint unsigned NULL,
  `occurrence_date` date NOT NULL,
  `effective_date` date NOT NULL,
  `event_evidence_sha256` char(64) NOT NULL,
  `request_sha256` char(64) NOT NULL,
  `review_sha256` char(64) NULL,
  `life_event_sha256` char(64) NOT NULL,
  `executed_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`life_event_id`),
  UNIQUE KEY `uq_ben1_evt_sha` (`life_event_sha256`),
  UNIQUE KEY `uq_ben1_evt_req` (`life_event_request_id`),
  KEY `ix_ben1_evt_scope` (`worker_id`,`effective_date`,`life_event_type_version_id`),
  CONSTRAINT `fk_ben1_evt_1` FOREIGN KEY (`life_event_request_id`) REFERENCES `0_hrm_ben_life_event_requests` (`life_event_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_evt_2` FOREIGN KEY (`life_event_review_id`) REFERENCES `0_hrm_ben_life_event_reviews` (`life_event_review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_contribution_versions` (
  `contribution_version_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `plan_version_id` bigint unsigned NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `currency_code` char(3) NOT NULL,
  `unit_code` varchar(32) NOT NULL,
  `calculation_kind` varchar(32) NOT NULL,
  `employee_fixed_amount` decimal(20,6) NOT NULL,
  `employer_fixed_amount` decimal(20,6) NOT NULL,
  `costing_adapter_version_id` bigint unsigned NULL,
  `rounding_contract_sha256` char(64) NOT NULL,
  `source_evidence_sha256` char(64) NOT NULL,
  `predecessor_contribution_version_id` bigint unsigned NULL,
  `contribution_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`contribution_version_id`),
  UNIQUE KEY `uq_ben1_cver_sha` (`contribution_sha256`),
  UNIQUE KEY `uq_ben1_cver_pred` (`predecessor_contribution_version_id`),
  KEY `ix_ben1_cver_eff` (`plan_version_id`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ben1_cvr_1` FOREIGN KEY (`plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_cvr_2` FOREIGN KEY (`predecessor_contribution_version_id`) REFERENCES `0_hrm_ben_contribution_versions` (`contribution_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_contribution_receipts` (
  `contribution_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `enrollment_id` bigint unsigned NOT NULL,
  `contribution_version_id` bigint unsigned NOT NULL,
  `coverage_period_start` date NOT NULL,
  `coverage_period_end` date NOT NULL,
  `effective_date` date NOT NULL,
  `employee_amount` decimal(20,6) NOT NULL,
  `employer_amount` decimal(20,6) NOT NULL,
  `currency_code` char(3) NOT NULL,
  `unit_code` varchar(32) NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `idempotency_hash` char(64) NOT NULL,
  `receipt_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`contribution_receipt_id`),
  UNIQUE KEY `uq_ben1_crc_sha` (`receipt_sha256`),
  UNIQUE KEY `uq_ben1_crc_idem` (`idempotency_hash`),
  KEY `ix_ben1_crc_scope` (`enrollment_id`,`coverage_period_start`,`coverage_period_end`),
  CONSTRAINT `fk_ben1_crc_1` FOREIGN KEY (`enrollment_id`) REFERENCES `0_hrm_ben_enrollments` (`enrollment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_crc_2` FOREIGN KEY (`contribution_version_id`) REFERENCES `0_hrm_ben_contribution_versions` (`contribution_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_arrears` (
  `arrears_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `contribution_receipt_id` bigint unsigned NOT NULL,
  `portion_code` varchar(16) NOT NULL,
  `period_start` varchar(96) NOT NULL,
  `period_end` varchar(96) NOT NULL,
  `amount` decimal(20,6) NOT NULL,
  `currency_code` char(3) NOT NULL,
  `unit_code` varchar(32) NOT NULL,
  `reason_code` varchar(64) NOT NULL,
  `source_obligation_sha256` char(64) NOT NULL,
  `obligation_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`arrears_id`),
  UNIQUE KEY `uq_ben1_arr_sha` (`obligation_sha256`),
  KEY `ix_ben1_arr_receipt` (`contribution_receipt_id`,`portion_code`),
  CONSTRAINT `fk_ben1_arr_1` FOREIGN KEY (`contribution_receipt_id`) REFERENCES `0_hrm_ben_contribution_receipts` (`contribution_receipt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_arrears_movements` (
  `arrears_movement_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `arrears_id` bigint unsigned NOT NULL,
  `movement_kind` varchar(32) NOT NULL,
  `amount` decimal(20,6) NOT NULL,
  `effective_date` date NOT NULL,
  `source_sha256` char(64) NOT NULL,
  `payroll_handoff_id` bigint unsigned NULL,
  `predecessor_movement_id` bigint unsigned NULL,
  `movement_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`arrears_movement_id`),
  UNIQUE KEY `uq_ben1_amv_sha` (`movement_sha256`),
  UNIQUE KEY `uq_ben1_amv_pred` (`predecessor_movement_id`),
  KEY `ix_ben1_amv_scope` (`arrears_id`,`effective_date`),
  CONSTRAINT `fk_ben1_amv_1` FOREIGN KEY (`arrears_id`) REFERENCES `0_hrm_ben_arrears` (`arrears_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_amv_2` FOREIGN KEY (`predecessor_movement_id`) REFERENCES `0_hrm_ben_arrears_movements` (`arrears_movement_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_continuations` (
  `continuation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `enrollment_id` bigint unsigned NOT NULL,
  `pack_adapter_version_id` bigint unsigned NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `status_change_evidence_sha256` char(64) NOT NULL,
  `entitlement_evidence_sha256` char(64) NOT NULL,
  `predecessor_continuation_id` bigint unsigned NULL,
  `continuation_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`continuation_id`),
  UNIQUE KEY `uq_ben1_cnt_sha` (`continuation_sha256`),
  UNIQUE KEY `uq_ben1_cnt_pred` (`predecessor_continuation_id`),
  KEY `ix_ben1_cnt_scope` (`enrollment_id`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ben1_cnt_1` FOREIGN KEY (`enrollment_id`) REFERENCES `0_hrm_ben_enrollments` (`enrollment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_cnt_2` FOREIGN KEY (`pack_adapter_version_id`) REFERENCES `0_hrm_ben_pack_adapter_versions` (`pack_adapter_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_cnt_3` FOREIGN KEY (`predecessor_continuation_id`) REFERENCES `0_hrm_ben_continuations` (`continuation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_payroll_mappings` (
  `mapping_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `legal_entity_id` bigint unsigned NOT NULL,
  `plan_version_id` bigint unsigned NOT NULL,
  `portion_code` varchar(16) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `element_version_id` bigint unsigned NOT NULL,
  `input_definition_id` bigint unsigned NOT NULL,
  `unit_code` varchar(32) NOT NULL,
  `predecessor_mapping_id` bigint unsigned NULL,
  `mapping_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mapping_id`),
  UNIQUE KEY `uq_ben1_pmap_sha` (`mapping_sha256`),
  UNIQUE KEY `uq_ben1_pmap_pred` (`predecessor_mapping_id`),
  KEY `ix_ben1_pmap_eff` (`legal_entity_id`,`plan_version_id`,`portion_code`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ben1_pmp_1` FOREIGN KEY (`plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_pmp_2` FOREIGN KEY (`predecessor_mapping_id`) REFERENCES `0_hrm_ben_payroll_mappings` (`mapping_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_payroll_handoffs` (
  `handoff_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `contribution_receipt_id` bigint unsigned NOT NULL,
  `mapping_id` bigint unsigned NOT NULL,
  `portion_code` varchar(16) NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `payroll_period_id` bigint unsigned NOT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NOT NULL,
  `amount` decimal(20,6) NOT NULL,
  `approved_input_id` bigint unsigned NOT NULL,
  `approved_input_sha256` char(64) NOT NULL,
  `adapter_version` varchar(64) NOT NULL,
  `command_key` varchar(160) NOT NULL,
  `handoff_sha256` char(64) NOT NULL,
  `executed_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`handoff_id`),
  UNIQUE KEY `uq_ben1_pho_sha` (`handoff_sha256`),
  UNIQUE KEY `uq_ben1_pho_cmd` (`command_key`),
  UNIQUE KEY `uq_ben1_pho_receipt_portion` (`contribution_receipt_id`,`portion_code`),
  CONSTRAINT `fk_ben1_pho_1` FOREIGN KEY (`contribution_receipt_id`) REFERENCES `0_hrm_ben_contribution_receipts` (`contribution_receipt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_pho_2` FOREIGN KEY (`mapping_id`) REFERENCES `0_hrm_ben_payroll_mappings` (`mapping_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_balance_mappings` (
  `balance_mapping_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `legal_entity_id` bigint unsigned NOT NULL,
  `plan_version_id` bigint unsigned NOT NULL,
  `portion_code` varchar(16) NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `balance_definition_id` bigint unsigned NOT NULL,
  `predecessor_balance_mapping_id` bigint unsigned NULL,
  `mapping_sha256` char(64) NOT NULL,
  `created_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`balance_mapping_id`),
  UNIQUE KEY `uq_ben1_bmap_sha` (`mapping_sha256`),
  UNIQUE KEY `uq_ben1_bmap_pred` (`predecessor_balance_mapping_id`),
  KEY `ix_ben1_bmap_eff` (`legal_entity_id`,`plan_version_id`,`portion_code`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ben1_bmp_1` FOREIGN KEY (`plan_version_id`) REFERENCES `0_hrm_ben_plan_versions` (`plan_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_bmp_2` FOREIGN KEY (`predecessor_balance_mapping_id`) REFERENCES `0_hrm_ben_balance_mappings` (`balance_mapping_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_balance_handoffs` (
  `balance_handoff_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `arrears_id` bigint unsigned NOT NULL,
  `balance_mapping_id` bigint unsigned NOT NULL,
  `pay_core_movement_id` bigint unsigned NOT NULL,
  `pay_core_movement_sha256` char(64) NOT NULL,
  `boundary_version` varchar(64) NOT NULL,
  `command_key` varchar(160) NOT NULL,
  `handoff_sha256` char(64) NOT NULL,
  `recorded_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`balance_handoff_id`),
  UNIQUE KEY `uq_ben1_bho_sha` (`handoff_sha256`),
  UNIQUE KEY `uq_ben1_bho_cmd` (`command_key`),
  UNIQUE KEY `uq_ben1_bho_arrears` (`arrears_id`),
  CONSTRAINT `fk_ben1_bho_1` FOREIGN KEY (`arrears_id`) REFERENCES `0_hrm_ben_arrears` (`arrears_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_bho_2` FOREIGN KEY (`balance_mapping_id`) REFERENCES `0_hrm_ben_balance_mappings` (`balance_mapping_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_provider_operations` (
  `provider_operation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `enrollment_id` bigint unsigned NOT NULL,
  `provider_adapter_version_id` bigint unsigned NULL,
  `operation_code` varchar(64) NOT NULL,
  `participant_reference_sha256` char(64) NOT NULL,
  `dependant_scope_sha256` char(64) NOT NULL,
  `request_payload_json` longtext NOT NULL,
  `request_sha256` char(64) NOT NULL,
  `idempotency_hash` char(64) NOT NULL,
  `attempt_no` int unsigned NOT NULL,
  `response_status` varchar(24) NOT NULL,
  `response_sha256` char(64) NULL,
  `acknowledgement_ref_sha256` char(64) NULL,
  `reconciliation_state` varchar(24) NOT NULL,
  `retry_state` varchar(24) NOT NULL,
  `recovery_state` varchar(24) NOT NULL,
  `operation_sha256` char(64) NOT NULL,
  `recorded_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`provider_operation_id`),
  UNIQUE KEY `uq_ben1_pop_sha` (`operation_sha256`),
  UNIQUE KEY `uq_ben1_pop_attempt` (`idempotency_hash`,`attempt_no`),
  KEY `ix_ben1_pop_state` (`provider_adapter_version_id`,`reconciliation_state`,`retry_state`,`recovery_state`),
  CONSTRAINT `fk_ben1_pop_1` FOREIGN KEY (`enrollment_id`) REFERENCES `0_hrm_ben_enrollments` (`enrollment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_pop_2` FOREIGN KEY (`provider_adapter_version_id`) REFERENCES `0_hrm_ben_provider_adapter_versions` (`provider_adapter_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ben_provider_reconciliations` (
  `provider_reconciliation_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `provider_operation_id` bigint unsigned NOT NULL,
  `expected_sha256` char(64) NOT NULL,
  `observed_sha256` char(64) NULL,
  `status_token` varchar(24) NOT NULL,
  `evidence_sha256` char(64) NOT NULL,
  `predecessor_reconciliation_id` bigint unsigned NULL,
  `reconciliation_sha256` char(64) NOT NULL,
  `reviewed_by` bigint unsigned NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`provider_reconciliation_id`),
  UNIQUE KEY `uq_ben1_prec_sha` (`reconciliation_sha256`),
  UNIQUE KEY `uq_ben1_prec_pred` (`predecessor_reconciliation_id`),
  KEY `ix_ben1_prec_op` (`provider_operation_id`,`status_token`),
  CONSTRAINT `fk_ben1_prc_1` FOREIGN KEY (`provider_operation_id`) REFERENCES `0_hrm_ben_provider_operations` (`provider_operation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ben1_prc_2` FOREIGN KEY (`predecessor_reconciliation_id`) REFERENCES `0_hrm_ben_provider_reconciliations` (`provider_reconciliation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE `0_hrm_ess_002_delegation_capabilities` (
  `capability_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `delegation_id` bigint(20) unsigned NOT NULL,
  `legal_entity_id` bigint(20) unsigned NOT NULL,
  `capability_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NOT NULL,
  `grant_reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `capability_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `granted_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`capability_id`),
  UNIQUE KEY `uq_ess2_cap_sha` (`capability_sha256`),
  UNIQUE KEY `uq_ess2_cap_scope` (`delegation_id`,`legal_entity_id`,`capability_code`,`effective_from`),
  KEY `ix_ess2_cap_asof` (`delegation_id`,`capability_code`,`effective_from`,`effective_to`),
  CONSTRAINT `fk_ess2_cap_delegation` FOREIGN KEY (`delegation_id`) REFERENCES `0_hrm_ess_manager_delegations` (`delegation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_ess_002_delegation_revocations` (
  `revocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `capability_id` bigint(20) unsigned NOT NULL,
  `revoked_at` datetime NOT NULL,
  `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revoked_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`revocation_id`),
  UNIQUE KEY `uq_ess2_rev_sha` (`revocation_sha256`),
  UNIQUE KEY `uq_ess2_rev_cap` (`capability_id`),
  KEY `ix_ess2_rev_time` (`capability_id`,`revoked_at`),
  CONSTRAINT `fk_ess2_rev_capability` FOREIGN KEY (`capability_id`) REFERENCES `0_hrm_ess_002_delegation_capabilities` (`capability_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_ess_002_command_receipts` (
  `receipt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `actor_user_id` smallint(6) unsigned NOT NULL,
  `worker_id` bigint(20) unsigned NOT NULL,
  `assignment_id` bigint(20) unsigned NOT NULL,
  `legal_entity_id` bigint(20) unsigned NOT NULL,
  `command_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `idempotency_hash` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result_json` text NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`receipt_id`),
  UNIQUE KEY `uq_ess2_cmd_idem` (`idempotency_hash`),
  KEY `ix_ess2_cmd_worker` (`worker_id`,`command_type`,`created_at`),
  KEY `ix_ess2_cmd_assignment` (`assignment_id`),
  KEY `ix_ess2_cmd_legal_entity` (`legal_entity_id`),
  CONSTRAINT `fk_ess2_cmd_worker` FOREIGN KEY (`worker_id`) REFERENCES `0_hrm_workers` (`worker_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ess2_cmd_assignment` FOREIGN KEY (`assignment_id`) REFERENCES `0_hrm_assignments` (`assignment_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_ess2_cmd_legal_entity` FOREIGN KEY (`legal_entity_id`) REFERENCES `0_hrm_legal_entities` (`legal_entity_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_raw_blobs` (
  `raw_blob_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `raw_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `byte_length` bigint(20) unsigned NOT NULL,
  `media_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `content` longblob NOT NULL,
  `received_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`raw_blob_id`),
  UNIQUE KEY `uq_pctry_rb_sha` (`raw_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_packs` (
  `pack_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `pack_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`pack_id`),
  UNIQUE KEY `uq_pctry_pack_code` (`pack_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_pack_versions` (
  `pack_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `pack_id` bigint(20) unsigned NOT NULL,
  `version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `manifest_schema_version` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `raw_blob_id` bigint(20) unsigned NOT NULL,
  `manifest_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `publisher_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signer_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `key_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signed_at` datetime NOT NULL,
  `capability_registry_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `capabilities_json` text NOT NULL,
  `host_min_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_max_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `registered_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`pack_version_id`),
  UNIQUE KEY `uq_pctry_pv_identity` (`pack_id`,`version`),
  UNIQUE KEY `uq_pctry_pv_semantic` (`pack_id`,`version`,`semantic_root`),
  KEY `ix_pctry_pv_raw` (`raw_blob_id`),
  CONSTRAINT `fk_pctry_pv_pack` FOREIGN KEY (`pack_id`) REFERENCES `0_pay_ctry_packs` (`pack_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pctry_pv_raw` FOREIGN KEY (`raw_blob_id`) REFERENCES `0_pay_ctry_raw_blobs` (`raw_blob_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_artifacts` (
  `artifact_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `ordinal_no` smallint(5) unsigned NOT NULL,
  `normalized_path` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_type` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `media_type` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `byte_length` bigint(20) unsigned NOT NULL,
  `sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `is_evidence` tinyint(1) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`artifact_id`),
  UNIQUE KEY `uq_pctry_art_path` (`pack_version_id`,`normalized_path`),
  UNIQUE KEY `uq_pctry_art_ord` (`pack_version_id`,`ordinal_no`),
  KEY `ix_pctry_art_sha` (`sha256`),
  CONSTRAINT `fk_pctry_art_pv` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_publishers` (
  `publisher_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `publisher_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `display_name` varchar(160) NOT NULL,
  `valid_from` datetime NOT NULL,
  `valid_to` datetime NULL,
  `identity_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`publisher_id`),
  UNIQUE KEY `uq_pctry_pub_code` (`publisher_code`),
  UNIQUE KEY `uq_pctry_pub_sha` (`identity_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_publisher_revocations` (
  `publisher_revocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `publisher_id` bigint(20) unsigned NOT NULL,
  `effective_at` datetime NOT NULL,
  `reason_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revoked_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`publisher_revocation_id`),
  UNIQUE KEY `uq_pctry_pub_rev_pub` (`publisher_id`),
  UNIQUE KEY `uq_pctry_pub_rev_sha` (`revocation_sha256`),
  CONSTRAINT `fk_pctry_pub_rev_pub` FOREIGN KEY (`publisher_id`) REFERENCES `0_pay_ctry_publishers` (`publisher_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_signers` (
  `signer_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `publisher_id` bigint(20) unsigned NOT NULL,
  `signer_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `display_name` varchar(160) NOT NULL,
  `identity_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`signer_id`),
  UNIQUE KEY `uq_pctry_signer_code` (`signer_code`),
  UNIQUE KEY `uq_pctry_signer_sha` (`identity_sha256`),
  KEY `ix_pctry_signer_pub` (`publisher_id`),
  CONSTRAINT `fk_pctry_signer_pub` FOREIGN KEY (`publisher_id`) REFERENCES `0_pay_ctry_publishers` (`publisher_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_trust_keys` (
  `trust_key_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `signer_id` bigint(20) unsigned NOT NULL,
  `key_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `algorithm` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `public_key_pem` text NOT NULL,
  `public_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `valid_from` datetime NOT NULL,
  `valid_to` datetime NULL,
  `trusted_at` datetime NOT NULL,
  `key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `imported_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`trust_key_id`),
  UNIQUE KEY `uq_pctry_key_id` (`key_id`),
  UNIQUE KEY `uq_pctry_key_material` (`public_key_sha256`),
  UNIQUE KEY `uq_pctry_key_sha` (`key_sha256`),
  KEY `ix_pctry_key_signer` (`signer_id`),
  CONSTRAINT `fk_pctry_key_signer` FOREIGN KEY (`signer_id`) REFERENCES `0_pay_ctry_signers` (`signer_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_trust_key_revocations` (
  `key_revocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `trust_key_id` bigint(20) unsigned NOT NULL,
  `effective_at` datetime NOT NULL,
  `reason_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `compromised` tinyint(1) unsigned NOT NULL,
  `revocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revoked_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`key_revocation_id`),
  UNIQUE KEY `uq_pctry_key_rev_key` (`trust_key_id`),
  UNIQUE KEY `uq_pctry_key_rev_sha` (`revocation_sha256`),
  CONSTRAINT `fk_pctry_key_rev_key` FOREIGN KEY (`trust_key_id`) REFERENCES `0_pay_ctry_trust_keys` (`trust_key_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_trust_key_supersessions` (
  `key_supersession_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `predecessor_key_id` bigint(20) unsigned NOT NULL,
  `successor_key_id` bigint(20) unsigned NOT NULL,
  `effective_at` datetime NOT NULL,
  `supersession_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`key_supersession_id`),
  UNIQUE KEY `uq_pctry_key_sup_pred` (`predecessor_key_id`),
  UNIQUE KEY `uq_pctry_key_sup_sha` (`supersession_sha256`),
  KEY `ix_pctry_key_sup_succ` (`successor_key_id`),
  CONSTRAINT `fk_pctry_key_sup_pred` FOREIGN KEY (`predecessor_key_id`) REFERENCES `0_pay_ctry_trust_keys` (`trust_key_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pctry_key_sup_succ` FOREIGN KEY (`successor_key_id`) REFERENCES `0_pay_ctry_trust_keys` (`trust_key_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_verification_reports` (
  `verification_report_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `raw_blob_id` bigint(20) unsigned NOT NULL,
  `pack_version_id` bigint(20) unsigned NULL,
  `candidate_pack_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `candidate_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `manifest_schema_version` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `manifest_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `publisher_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `signer_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `key_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `trust_decision` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `trusted_time_decision` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revocation_decision` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `forbidden_content_result` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `capability_result` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `dependency_result` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `final_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `failure_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `verifier_version` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `details_json` text NOT NULL,
  `report_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `verified_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`verification_report_id`),
  UNIQUE KEY `uq_pctry_vr_sha` (`report_sha256`),
  KEY `ix_pctry_vr_raw` (`raw_blob_id`),
  KEY `ix_pctry_vr_pack` (`pack_version_id`),
  KEY `ix_pctry_vr_status` (`final_status`,`created_at`),
  CONSTRAINT `fk_pctry_vr_raw` FOREIGN KEY (`raw_blob_id`) REFERENCES `0_pay_ctry_raw_blobs` (`raw_blob_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pctry_vr_pv` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_capability_registry_versions` (
  `registry_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `registry_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `registry_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`registry_version_id`),
  UNIQUE KEY `uq_pctry_cap_ver` (`registry_version`),
  UNIQUE KEY `uq_pctry_cap_ver_sha` (`registry_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_capability_definitions` (
  `capability_definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `registry_version_id` bigint(20) unsigned NOT NULL,
  `capability_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `required_constraints_json` text NOT NULL,
  `allowed_constraints_json` text NOT NULL,
  `exclusive_group` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `allowed_artifact_types_json` text NOT NULL,
  `definition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`capability_definition_id`),
  UNIQUE KEY `uq_pctry_cap_def_code` (`registry_version_id`,`capability_code`),
  UNIQUE KEY `uq_pctry_cap_def_sha` (`definition_sha256`),
  CONSTRAINT `fk_pctry_cap_def_ver` FOREIGN KEY (`registry_version_id`) REFERENCES `0_pay_ctry_capability_registry_versions` (`registry_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_dependency_locks` (
  `dependency_lock_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `lock_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`dependency_lock_id`),
  UNIQUE KEY `uq_pctry_dep_pack` (`pack_version_id`),
  UNIQUE KEY `uq_pctry_dep_sha` (`lock_sha256`),
  CONSTRAINT `fk_pctry_dep_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_dependency_lock_members` (
  `dependency_lock_member_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `dependency_lock_id` bigint(20) unsigned NOT NULL,
  `ordinal_no` smallint(5) unsigned NOT NULL,
  `dependency_pack_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `required_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `resolved_pack_version_id` bigint(20) unsigned NOT NULL,
  `member_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`dependency_lock_member_id`),
  UNIQUE KEY `uq_pctry_dep_member_ord` (`dependency_lock_id`,`ordinal_no`),
  UNIQUE KEY `uq_pctry_dep_member_pack` (`dependency_lock_id`,`dependency_pack_code`),
  UNIQUE KEY `uq_pctry_dep_member_sha` (`member_sha256`),
  KEY `ix_pctry_dep_resolved` (`resolved_pack_version_id`),
  CONSTRAINT `fk_pctry_dep_member_lock` FOREIGN KEY (`dependency_lock_id`) REFERENCES `0_pay_ctry_dependency_locks` (`dependency_lock_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pctry_dep_member_resolved` FOREIGN KEY (`resolved_pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_install_receipts` (
  `install_receipt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `idempotency_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `raw_blob_id` bigint(20) unsigned NOT NULL,
  `verification_report_id` bigint(20) unsigned NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `receipt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `installed_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`install_receipt_id`),
  UNIQUE KEY `uq_pctry_ir_idem` (`idempotency_sha256`),
  UNIQUE KEY `uq_pctry_ir_sha` (`receipt_sha256`),
  KEY `ix_pctry_ir_raw` (`raw_blob_id`),
  KEY `ix_pctry_ir_report` (`verification_report_id`),
  KEY `ix_pctry_ir_pack` (`pack_version_id`),
  CONSTRAINT `fk_pctry_ir_raw` FOREIGN KEY (`raw_blob_id`) REFERENCES `0_pay_ctry_raw_blobs` (`raw_blob_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pctry_ir_report` FOREIGN KEY (`verification_report_id`) REFERENCES `0_pay_ctry_verification_reports` (`verification_report_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pctry_ir_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_site_operation_journal` (
  `operation_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `operation_key` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `event_no` smallint(5) unsigned NOT NULL,
  `operation_type` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `event_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `predecessor_pack_version_id` bigint(20) unsigned NULL,
  `verification_report_id` bigint(20) unsigned NOT NULL,
  `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `actor_id` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`operation_event_id`),
  UNIQUE KEY `uq_pct2_op_event` (`operation_key`,`event_no`),
  UNIQUE KEY `uq_pct2_op_evidence` (`evidence_sha256`),
  KEY `ix_pct2_op_pack` (`pack_version_id`,`event_type`,`created_at`),
  KEY `ix_pct2_op_pred` (`predecessor_pack_version_id`),
  KEY `ix_pct2_op_report` (`verification_report_id`),
  CONSTRAINT `fk_pct2_op_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_op_pred` FOREIGN KEY (`predecessor_pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_op_report` FOREIGN KEY (`verification_report_id`) REFERENCES `0_pay_ctry_verification_reports` (`verification_report_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_site_installations` (
  `site_installation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `predecessor_site_installation_id` bigint(20) unsigned NULL,
  `operation_event_id` bigint(20) unsigned NOT NULL,
  `installation_kind` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `receipt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `installed_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`site_installation_id`),
  UNIQUE KEY `uq_pct2_si_event` (`operation_event_id`),
  UNIQUE KEY `uq_pct2_si_receipt` (`receipt_sha256`),
  KEY `ix_pct2_si_pack` (`pack_version_id`,`created_at`),
  KEY `ix_pct2_si_pred` (`predecessor_site_installation_id`),
  CONSTRAINT `fk_pct2_si_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_si_pred` FOREIGN KEY (`predecessor_site_installation_id`) REFERENCES `0_pay_ctry_site_installations` (`site_installation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_si_event` FOREIGN KEY (`operation_event_id`) REFERENCES `0_pay_ctry_site_operation_journal` (`operation_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_impact_simulations` (
  `impact_simulation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `impact_key` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `site_installation_id` bigint(20) unsigned NOT NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `action_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `dependency_lock_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `open_run_count` int(10) unsigned NOT NULL,
  `open_approval_count` int(10) unsigned NOT NULL,
  `downstream_intent_count` int(10) unsigned NOT NULL,
  `blocking_finding_count` int(10) unsigned NOT NULL,
  `impact_payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `impact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `simulated_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`impact_simulation_id`),
  UNIQUE KEY `uq_pct2_im_key` (`impact_key`),
  UNIQUE KEY `uq_pct2_im_hash` (`impact_sha256`),
  KEY `ix_pct2_im_install` (`site_installation_id`,`created_at`),
  KEY `ix_pct2_im_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_from`),
  CONSTRAINT `fk_pct2_im_install` FOREIGN KEY (`site_installation_id`) REFERENCES `0_pay_ctry_site_installations` (`site_installation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_proposals` (
  `activation_proposal_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `proposal_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `site_installation_id` bigint(20) unsigned NOT NULL,
  `impact_simulation_id` bigint(20) unsigned NOT NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `action_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `dependency_lock_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `impact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `proposal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `maker_id` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`activation_proposal_id`),
  UNIQUE KEY `uq_pct2_ap_key` (`proposal_key`),
  UNIQUE KEY `uq_pct2_ap_hash` (`proposal_sha256`),
  KEY `ix_pct2_ap_install` (`site_installation_id`,`created_at`),
  KEY `ix_pct2_ap_impact` (`impact_simulation_id`),
  KEY `ix_pct2_ap_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_from`),
  CONSTRAINT `fk_pct2_ap_install` FOREIGN KEY (`site_installation_id`) REFERENCES `0_pay_ctry_site_installations` (`site_installation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_ap_impact` FOREIGN KEY (`impact_simulation_id`) REFERENCES `0_pay_ctry_activation_impact_simulations` (`impact_simulation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_approvals` (
  `activation_approval_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `activation_proposal_id` bigint(20) unsigned NOT NULL,
  `proposal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decision_token` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `checker_id` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`activation_approval_id`),
  UNIQUE KEY `uq_pct2_aa_proposal` (`activation_proposal_id`),
  UNIQUE KEY `uq_pct2_aa_hash` (`approval_sha256`),
  KEY `ix_pct2_aa_checker` (`checker_id`,`created_at`),
  CONSTRAINT `fk_pct2_aa_proposal` FOREIGN KEY (`activation_proposal_id`) REFERENCES `0_pay_ctry_activation_proposals` (`activation_proposal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activations` (
  `activation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `activation_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `activation_proposal_id` bigint(20) unsigned NOT NULL,
  `activation_approval_id` bigint(20) unsigned NOT NULL,
  `site_installation_id` bigint(20) unsigned NOT NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `action_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `dependency_lock_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `proposal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `assurance_event_id` bigint(20) unsigned NOT NULL,
  `execution_action_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `activation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `executor_id` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`activation_id`),
  UNIQUE KEY `uq_pct2_ac_key` (`activation_key`),
  UNIQUE KEY `uq_pct2_ac_proposal` (`activation_proposal_id`),
  UNIQUE KEY `uq_pct2_ac_approval` (`activation_approval_id`),
  UNIQUE KEY `uq_pct2_ac_hash` (`activation_sha256`),
  KEY `ix_pct2_ac_install` (`site_installation_id`,`created_at`),
  KEY `ix_pct2_ac_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_from`),
  CONSTRAINT `fk_pct2_ac_proposal` FOREIGN KEY (`activation_proposal_id`) REFERENCES `0_pay_ctry_activation_proposals` (`activation_proposal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_ac_approval` FOREIGN KEY (`activation_approval_id`) REFERENCES `0_pay_ctry_activation_approvals` (`activation_approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_ac_install` FOREIGN KEY (`site_installation_id`) REFERENCES `0_pay_ctry_site_installations` (`site_installation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_bindings` (
  `binding_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `activation_id` bigint(20) unsigned NOT NULL,
  `predecessor_binding_id` bigint(20) unsigned NULL,
  `site_installation_id` bigint(20) unsigned NOT NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `action_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `activation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `bound_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`binding_id`),
  UNIQUE KEY `uq_pct2_bd_activation` (`activation_id`),
  UNIQUE KEY `uq_pct2_bd_hash` (`binding_sha256`),
  KEY `ix_pct2_bd_pred` (`predecessor_binding_id`),
  KEY `ix_pct2_bd_install` (`site_installation_id`,`created_at`),
  KEY `ix_pct2_bd_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_from`),
  CONSTRAINT `fk_pct2_bd_activation` FOREIGN KEY (`activation_id`) REFERENCES `0_pay_ctry_activations` (`activation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_bd_pred` FOREIGN KEY (`predecessor_binding_id`) REFERENCES `0_pay_ctry_activation_bindings` (`binding_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_bd_install` FOREIGN KEY (`site_installation_id`) REFERENCES `0_pay_ctry_site_installations` (`site_installation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_suspensions` (
  `suspension_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `suspension_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `activation_id` bigint(20) unsigned NOT NULL,
  `binding_id` bigint(20) unsigned NOT NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `action_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_at` datetime NOT NULL,
  `reason_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `assurance_event_id` bigint(20) unsigned NOT NULL,
  `action_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `suspension_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `acted_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`suspension_id`),
  UNIQUE KEY `uq_pct2_ss_key` (`suspension_key`),
  UNIQUE KEY `uq_pct2_ss_hash` (`suspension_sha256`),
  KEY `ix_pct2_ss_activation` (`activation_id`,`effective_at`),
  KEY `ix_pct2_ss_binding` (`binding_id`,`effective_at`),
  KEY `ix_pct2_ss_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_at`),
  CONSTRAINT `fk_pct2_ss_activation` FOREIGN KEY (`activation_id`) REFERENCES `0_pay_ctry_activations` (`activation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_ss_binding` FOREIGN KEY (`binding_id`) REFERENCES `0_pay_ctry_activation_bindings` (`binding_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_outbox` (
  `outbox_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `outbox_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `event_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_id` bigint(20) unsigned NOT NULL,
  `activation_id` bigint(20) unsigned NULL,
  `suspension_id` bigint(20) unsigned NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `effective_at` datetime NOT NULL,
  `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `outbox_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`outbox_id`),
  UNIQUE KEY `uq_pct2_ob_key` (`outbox_key`),
  UNIQUE KEY `uq_pct2_ob_hash` (`outbox_sha256`),
  KEY `ix_pct2_ob_source` (`source_type`,`source_id`,`created_at`),
  KEY `ix_pct2_ob_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_at`),
  CONSTRAINT `fk_pct2_ob_activation` FOREIGN KEY (`activation_id`) REFERENCES `0_pay_ctry_activations` (`activation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_ob_suspension` FOREIGN KEY (`suspension_id`) REFERENCES `0_pay_ctry_activation_suspensions` (`suspension_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_delivery_attempts` (
  `delivery_attempt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `outbox_id` bigint(20) unsigned NOT NULL,
  `attempt_no` int(10) unsigned NOT NULL,
  `target_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `delivery_status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `error_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `attempted_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`delivery_attempt_id`),
  UNIQUE KEY `uq_pct2_da_attempt` (`outbox_id`,`attempt_no`),
  UNIQUE KEY `uq_pct2_da_evidence` (`evidence_sha256`),
  KEY `ix_pct2_da_status` (`outbox_id`,`delivery_status`,`created_at`),
  CONSTRAINT `fk_pct2_da_outbox` FOREIGN KEY (`outbox_id`) REFERENCES `0_pay_ctry_activation_outbox` (`outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_run_bindings` (
  `run_binding_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `run_id` bigint(20) unsigned NOT NULL,
  `binding_id` bigint(20) unsigned NULL,
  `activation_id` bigint(20) unsigned NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `from_date` date NOT NULL,
  `to_date` date NOT NULL,
  `resolution_state` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `captured_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`run_binding_id`),
  UNIQUE KEY `uq_pct2_rg_run` (`run_id`),
  UNIQUE KEY `uq_pct2_rg_hash` (`snapshot_sha256`),
  KEY `ix_pct2_rg_binding` (`binding_id`,`created_at`),
  KEY `ix_pct2_rg_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`from_date`),
  CONSTRAINT `fk_pct2_rg_run` FOREIGN KEY (`run_id`) REFERENCES `0_hrm_pay_core_009_runs` (`run_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_rg_binding` FOREIGN KEY (`binding_id`) REFERENCES `0_pay_ctry_activation_bindings` (`binding_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_rg_activation` FOREIGN KEY (`activation_id`) REFERENCES `0_pay_ctry_activations` (`activation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_activation_revocations` (
  `revocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `revocation_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `activation_id` bigint(20) unsigned NOT NULL,
  `binding_id` bigint(20) unsigned NOT NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `action_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_at` datetime NOT NULL,
  `reason_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `assurance_event_id` bigint(20) unsigned NOT NULL,
  `revocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revoked_by` smallint(6) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`revocation_id`),
  UNIQUE KEY `uq_pct2_rv_key` (`revocation_key`),
  UNIQUE KEY `uq_pct2_rv_hash` (`revocation_sha256`),
  KEY `ix_pct2_rv_activation` (`activation_id`,`effective_at`),
  KEY `ix_pct2_rv_binding` (`binding_id`,`effective_at`),
  KEY `ix_pct2_rv_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_at`),
  CONSTRAINT `fk_pct2_rv_activation` FOREIGN KEY (`activation_id`) REFERENCES `0_pay_ctry_activations` (`activation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct2_rv_binding` FOREIGN KEY (`binding_id`) REFERENCES `0_pay_ctry_activation_bindings` (`binding_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_capability_contract_versions` (
  `registry_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `registry_version` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `registry_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signature_scheme` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signed_by` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signature_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_min_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_max_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`registry_version_id`),
  UNIQUE KEY `uq_pct3_cv_version` (`registry_version`),
  UNIQUE KEY `uq_pct3_cv_sha` (`registry_sha256`),
  UNIQUE KEY `uq_pct3_cv_signature` (`signature_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_capability_contract_definitions` (
  `capability_definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `registry_version_id` bigint(20) unsigned NOT NULL,
  `capability_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `namespace` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `cardinality` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `exclusive_group` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `artifact_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_adapter` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `definition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`capability_definition_id`),
  UNIQUE KEY `uq_pct3_cd_code` (`registry_version_id`,`capability_code`),
  UNIQUE KEY `uq_pct3_cd_sha` (`definition_sha256`),
  KEY `ix_pct3_cd_adapter` (`host_adapter`,`host_version`),
  CONSTRAINT `fk_pct3_cd_registry` FOREIGN KEY (`registry_version_id`) REFERENCES `0_pay_ctry_capability_contract_versions` (`registry_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_host_adapters` (
  `host_adapter_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `adapter_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `adapter_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `media_types_json` text NOT NULL,
  `adapter_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`host_adapter_id`),
  UNIQUE KEY `uq_pct3_ha_code` (`adapter_code`,`adapter_version`),
  UNIQUE KEY `uq_pct3_ha_sha` (`adapter_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_pack_artifact_contracts` (
  `artifact_contract_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `artifact_id` bigint(20) unsigned NOT NULL,
  `registry_version_id` bigint(20) unsigned NOT NULL,
  `capability_definition_id` bigint(20) unsigned NOT NULL,
  `ordinal_no` int(10) unsigned NOT NULL,
  `artifact_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `media_type` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_adapter` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `contract_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `contract_status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`artifact_contract_id`),
  UNIQUE KEY `uq_pct3_ac_identity` (`pack_version_id`,`artifact_id`,`capability_definition_id`),
  UNIQUE KEY `uq_pct3_ac_sha` (`contract_sha256`),
  KEY `ix_pct3_ac_registry` (`registry_version_id`,`contract_status`),
  KEY `ix_pct3_ac_host` (`host_adapter`,`host_version`),
  KEY `fk_pct3_ac_artifact` (`artifact_id`),
  KEY `fk_pct3_ac_definition` (`capability_definition_id`),
  CONSTRAINT `fk_pct3_ac_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct3_ac_artifact` FOREIGN KEY (`artifact_id`) REFERENCES `0_pay_ctry_artifacts` (`artifact_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct3_ac_registry` FOREIGN KEY (`registry_version_id`) REFERENCES `0_pay_ctry_capability_contract_versions` (`registry_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct3_ac_definition` FOREIGN KEY (`capability_definition_id`) REFERENCES `0_pay_ctry_capability_contract_definitions` (`capability_definition_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_contract_validation_reports` (
  `validation_report_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `artifact_contract_id` bigint(20) unsigned NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `registry_version_id` bigint(20) unsigned NOT NULL,
  `capability_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `validation_status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `failure_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `details_json` text NOT NULL,
  `report_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `verified_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`validation_report_id`),
  UNIQUE KEY `uq_pct3_vr_sha` (`report_sha256`),
  KEY `ix_pct3_vr_contract` (`artifact_contract_id`,`validation_status`),
  KEY `ix_pct3_vr_pack` (`pack_version_id`,`capability_code`),
  KEY `fk_pct3_vr_registry` (`registry_version_id`),
  CONSTRAINT `fk_pct3_vr_contract` FOREIGN KEY (`artifact_contract_id`) REFERENCES `0_pay_ctry_pack_artifact_contracts` (`artifact_contract_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct3_vr_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct3_vr_registry` FOREIGN KEY (`registry_version_id`) REFERENCES `0_pay_ctry_capability_contract_versions` (`registry_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_contract_migration_edges` (
  `migration_edge_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `artifact_contract_id` bigint(20) unsigned NOT NULL,
  `predecessor_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `successor_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `migration_strategy` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reversible` tinyint(1) unsigned NOT NULL,
  `edge_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`migration_edge_id`),
  UNIQUE KEY `uq_pct3_me_sha` (`edge_sha256`),
  UNIQUE KEY `uq_pct3_me_successor` (`artifact_contract_id`,`successor_version`),
  KEY `ix_pct3_me_predecessor` (`predecessor_version`),
  CONSTRAINT `fk_pct3_me_contract` FOREIGN KEY (`artifact_contract_id`) REFERENCES `0_pay_ctry_pack_artifact_contracts` (`artifact_contract_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_evidence_sources` (
  `evidence_source_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `source_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_version` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `issuer` varchar(191) NOT NULL,
  `jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` datetime NOT NULL,
  `effective_to` datetime NULL,
  `acquired_at` datetime NOT NULL,
  `recorded_at` datetime NOT NULL,
  `source_locator` varchar(512) NULL,
  `evidence_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_identity_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `trust_status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`evidence_source_id`),
  UNIQUE KEY `uq_pct4_es_identity` (`source_identity_sha256`),
  UNIQUE KEY `uq_pct4_es_code_version` (`source_code`,`source_version`,`evidence_digest`),
  KEY `ix_pct4_es_scope` (`jurisdiction`,`effective_from`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_evidence_items` (
  `evidence_item_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `evidence_source_id` bigint(20) unsigned NOT NULL,
  `evidence_type` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `media_type` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `content_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_identity_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_locator` varchar(512) NULL,
  `recorded_by` bigint(20) unsigned NOT NULL,
  `recorded_at` datetime NOT NULL,
  PRIMARY KEY (`evidence_item_id`),
  UNIQUE KEY `uq_pct4_ei_identity` (`evidence_identity_sha256`),
  UNIQUE KEY `uq_pct4_ei_source_content` (`evidence_source_id`,`content_sha256`),
  CONSTRAINT `fk_pct4_ei_source` FOREIGN KEY (`evidence_source_id`) REFERENCES `0_pay_ctry_evidence_sources` (`evidence_source_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_evidence_inventories` (
  `evidence_inventory_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `inventory_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `algorithm_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `canonical_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `item_count` int(10) unsigned NOT NULL,
  `created_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`evidence_inventory_id`),
  UNIQUE KEY `uq_pct4_ev_key` (`inventory_key`),
  UNIQUE KEY `uq_pct4_ev_root` (`pack_version_id`,`semantic_root`,`evidence_root`),
  KEY `ix_pct4_ev_pack` (`pack_version_id`,`created_at`),
  CONSTRAINT `fk_pct4_ev_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_evidence_inventory_members` (
  `inventory_member_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `evidence_inventory_id` bigint(20) unsigned NOT NULL,
  `evidence_item_id` bigint(20) unsigned NOT NULL,
  `canonical_ordinal` int(10) unsigned NOT NULL,
  `member_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`inventory_member_id`),
  UNIQUE KEY `uq_pct4_em_ordinal` (`evidence_inventory_id`,`canonical_ordinal`),
  UNIQUE KEY `uq_pct4_em_item` (`evidence_inventory_id`,`evidence_item_id`),
  UNIQUE KEY `uq_pct4_em_sha` (`member_sha256`),
  KEY `fk_pct4_em_item` (`evidence_item_id`),
  CONSTRAINT `fk_pct4_em_inventory` FOREIGN KEY (`evidence_inventory_id`) REFERENCES `0_pay_ctry_evidence_inventories` (`evidence_inventory_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_em_item` FOREIGN KEY (`evidence_item_id`) REFERENCES `0_pay_ctry_evidence_items` (`evidence_item_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_review_decisions` (
  `review_decision_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `evidence_inventory_id` bigint(20) unsigned NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `review_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reviewer_id` bigint(20) unsigned NOT NULL,
  `reviewer_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `independence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decision` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_item_id` bigint(20) unsigned NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decision_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decided_at` datetime NOT NULL,
  PRIMARY KEY (`review_decision_id`),
  UNIQUE KEY `uq_pct4_rd_sha` (`decision_sha256`),
  UNIQUE KEY `uq_pct4_rd_once` (`pack_version_id`,`review_type`,`decision_sha256`),
  KEY `ix_pct4_rd_inventory` (`evidence_inventory_id`,`review_type`),
  KEY `fk_pct4_rd_evidence` (`evidence_item_id`),
  CONSTRAINT `fk_pct4_rd_inventory` FOREIGN KEY (`evidence_inventory_id`) REFERENCES `0_pay_ctry_evidence_inventories` (`evidence_inventory_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_rd_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_rd_evidence` FOREIGN KEY (`evidence_item_id`) REFERENCES `0_pay_ctry_evidence_items` (`evidence_item_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_parallel_run_evidence` (
  `parallel_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `evidence_inventory_id` bigint(20) unsigned NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `runner_identity` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `test_identity` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `test_version` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `fixture_identity` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `executed_at` datetime NOT NULL,
  `result_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `expected_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `observed_digest` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `acceptance_criteria_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `accepted` tinyint(1) unsigned NOT NULL,
  `synthetic` tinyint(1) unsigned NOT NULL,
  `engineering_only` tinyint(1) unsigned NOT NULL,
  `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `recorded_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`parallel_evidence_id`),
  UNIQUE KEY `uq_pct4_pr_sha` (`evidence_sha256`),
  KEY `ix_pct4_pr_pack` (`pack_version_id`,`executed_at`),
  KEY `ix_pct4_pr_inventory` (`evidence_inventory_id`,`executed_at`),
  CONSTRAINT `fk_pct4_pr_inventory` FOREIGN KEY (`evidence_inventory_id`) REFERENCES `0_pay_ctry_evidence_inventories` (`evidence_inventory_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_pr_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_certification_signing_keys` (
  `certification_signing_key_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `key_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `actor_id` bigint(20) unsigned NOT NULL,
  `actor_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `algorithm` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `public_key_pem` text NOT NULL,
  `public_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `valid_from` datetime NOT NULL,
  `valid_to` datetime NULL,
  `key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `imported_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`certification_signing_key_id`),
  UNIQUE KEY `uq_pct4_sk_key_id` (`key_id`),
  UNIQUE KEY `uq_pct4_sk_public` (`public_key_sha256`),
  UNIQUE KEY `uq_pct4_sk_sha` (`key_sha256`),
  KEY `ix_pct4_sk_actor` (`actor_id`,`valid_from`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_certification_key_revocations` (
  `key_revocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `certification_signing_key_id` bigint(20) unsigned NOT NULL,
  `effective_at` datetime NOT NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revoked_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`key_revocation_id`),
  UNIQUE KEY `uq_pct4_kr_key` (`certification_signing_key_id`),
  UNIQUE KEY `uq_pct4_kr_sha` (`revocation_sha256`),
  CONSTRAINT `fk_pct4_kr_key` FOREIGN KEY (`certification_signing_key_id`) REFERENCES `0_pay_ctry_certification_signing_keys` (`certification_signing_key_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_certification_events` (
  `certification_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `certification_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `event_no` int(10) unsigned NOT NULL,
  `predecessor_event_id` bigint(20) unsigned NULL,
  `predecessor_decision_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `evidence_inventory_id` bigint(20) unsigned NOT NULL,
  `registry_version_id` bigint(20) unsigned NOT NULL,
  `parallel_evidence_id` bigint(20) unsigned NULL,
  `from_status` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `to_status` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `review_chain_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `claim_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `valid_from` datetime NULL,
  `expires_at` datetime NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decision_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `actor_id` bigint(20) unsigned NOT NULL,
  `actor_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `assurance_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `certification_signing_key_id` bigint(20) unsigned NULL,
  `signature_algorithm` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `signed_at` datetime NULL,
  `signature_base64` text NULL,
  `signature_payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`certification_event_id`),
  UNIQUE KEY `uq_pct4_ce_event` (`certification_key`,`event_no`),
  UNIQUE KEY `uq_pct4_ce_decision` (`decision_sha256`),
  UNIQUE KEY `uq_pct4_ce_predecessor` (`predecessor_event_id`),
  KEY `ix_pct4_ce_pack` (`pack_version_id`,`to_status`),
  KEY `fk_pct4_ce_inventory` (`evidence_inventory_id`),
  KEY `fk_pct4_ce_registry` (`registry_version_id`),
  KEY `fk_pct4_ce_parallel` (`parallel_evidence_id`),
  KEY `fk_pct4_ce_signing` (`certification_signing_key_id`),
  CONSTRAINT `fk_pct4_ce_predecessor` FOREIGN KEY (`predecessor_event_id`) REFERENCES `0_pay_ctry_certification_events` (`certification_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_ce_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_ce_inventory` FOREIGN KEY (`evidence_inventory_id`) REFERENCES `0_pay_ctry_evidence_inventories` (`evidence_inventory_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_ce_registry` FOREIGN KEY (`registry_version_id`) REFERENCES `0_pay_ctry_capability_contract_versions` (`registry_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_ce_parallel` FOREIGN KEY (`parallel_evidence_id`) REFERENCES `0_pay_ctry_parallel_run_evidence` (`parallel_evidence_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_ce_signing` FOREIGN KEY (`certification_signing_key_id`) REFERENCES `0_pay_ctry_certification_signing_keys` (`certification_signing_key_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_certification_claims` (
  `certification_claim_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `certification_event_id` bigint(20) unsigned NOT NULL,
  `jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `company_id` bigint(20) unsigned NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `effective_from` datetime NOT NULL,
  `effective_to` datetime NOT NULL,
  `payroll_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `employee_category` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `filing_capability` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payment_capability` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `host_version` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `capability_registry_version` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `limitations` text NOT NULL,
  `claim_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`certification_claim_id`),
  UNIQUE KEY `uq_pct4_cc_sha` (`claim_scope_sha256`),
  KEY `ix_pct4_cc_event` (`certification_event_id`,`effective_to`),
  KEY `ix_pct4_cc_scope` (`jurisdiction`,`scope_type`,`company_id`,`legal_entity_id`),
  CONSTRAINT `fk_pct4_cc_event` FOREIGN KEY (`certification_event_id`) REFERENCES `0_pay_ctry_certification_events` (`certification_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_publication_records` (
  `publication_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `certification_event_id` bigint(20) unsigned NOT NULL,
  `certification_claim_id` bigint(20) unsigned NOT NULL,
  `publication_event` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `predecessor_publication_id` bigint(20) unsigned NULL,
  `published_at` datetime NOT NULL,
  `support_until` datetime NULL,
  `publication_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `actor_id` bigint(20) unsigned NOT NULL,
  `actor_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `assurance_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`publication_id`),
  UNIQUE KEY `uq_pct4_pb_sha` (`publication_sha256`),
  KEY `ix_pct4_pb_event` (`certification_event_id`,`publication_event`),
  KEY `fk_pct4_pb_claim` (`certification_claim_id`),
  KEY `fk_pct4_pb_pred` (`predecessor_publication_id`),
  CONSTRAINT `fk_pct4_pb_event` FOREIGN KEY (`certification_event_id`) REFERENCES `0_pay_ctry_certification_events` (`certification_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_pb_claim` FOREIGN KEY (`certification_claim_id`) REFERENCES `0_pay_ctry_certification_claims` (`certification_claim_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_pb_pred` FOREIGN KEY (`predecessor_publication_id`) REFERENCES `0_pay_ctry_publication_records` (`publication_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_certification_emergency_actions` (
  `emergency_action_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `certification_event_id` bigint(20) unsigned NOT NULL,
  `certification_claim_id` bigint(20) unsigned NULL,
  `action_type` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_at` datetime NOT NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `affected_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `follow_up_required` tinyint(1) unsigned NOT NULL,
  `assurance_event_id` bigint(20) unsigned NULL,
  `action_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `acted_by` bigint(20) unsigned NOT NULL,
  `actor_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`emergency_action_id`),
  UNIQUE KEY `uq_pct4_ea_sha` (`action_sha256`),
  KEY `ix_pct4_ea_event` (`certification_event_id`,`effective_at`),
  KEY `fk_pct4_ea_claim` (`certification_claim_id`),
  CONSTRAINT `fk_pct4_ea_event` FOREIGN KEY (`certification_event_id`) REFERENCES `0_pay_ctry_certification_events` (`certification_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_ea_claim` FOREIGN KEY (`certification_claim_id`) REFERENCES `0_pay_ctry_certification_claims` (`certification_claim_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_certification_raci_assignments` (
  `raci_assignment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `governance_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `principal_id` bigint(20) unsigned NOT NULL,
  `responsibility` char(1) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `jurisdiction` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `valid_from` datetime NOT NULL,
  `valid_to` datetime NULL,
  `assignment_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `assigned_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`raci_assignment_id`),
  UNIQUE KEY `uq_pct4_ra_sha` (`assignment_sha256`),
  KEY `ix_pct4_ra_role` (`governance_role`,`principal_id`),
  KEY `ix_pct4_ra_scope` (`jurisdiction`,`valid_from`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_certification_recovery_records` (
  `recovery_record_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `certification_event_id` bigint(20) unsigned NOT NULL,
  `predecessor_recovery_id` bigint(20) unsigned NULL,
  `recovery_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `observed_state_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `expected_state_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reconciliation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decision` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `recovered_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`recovery_record_id`),
  UNIQUE KEY `uq_pct4_rr_sha` (`reconciliation_sha256`),
  KEY `ix_pct4_rr_event` (`certification_event_id`,`created_at`),
  KEY `fk_pct4_rr_pred` (`predecessor_recovery_id`),
  CONSTRAINT `fk_pct4_rr_event` FOREIGN KEY (`certification_event_id`) REFERENCES `0_pay_ctry_certification_events` (`certification_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct4_rr_pred` FOREIGN KEY (`predecessor_recovery_id`) REFERENCES `0_pay_ctry_certification_recovery_records` (`recovery_record_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_catalog_releases` (
  `catalog_release_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `release_no` int(10) unsigned NOT NULL,
  `predecessor_catalog_release_id` bigint(20) unsigned NULL,
  `predecessor_release_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `catalog_root_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `released_at` datetime NOT NULL,
  `publisher_id` bigint(20) unsigned NOT NULL,
  `signer_id` bigint(20) unsigned NOT NULL,
  `trust_key_id` bigint(20) unsigned NOT NULL,
  `signature_algorithm` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signature_payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signature_base64` text NOT NULL,
  `release_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `received_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`catalog_release_id`),
  UNIQUE KEY `uq_pct7_cr_release` (`catalog_code`,`release_no`),
  UNIQUE KEY `uq_pct7_cr_sha` (`release_sha256`),
  UNIQUE KEY `uq_pct7_cr_pred` (`predecessor_catalog_release_id`),
  KEY `ix_pct7_cr_root` (`catalog_root_sha256`,`released_at`),
  KEY `fk_pct7_cr_publisher` (`publisher_id`),
  KEY `fk_pct7_cr_signer` (`signer_id`),
  KEY `fk_pct7_cr_key` (`trust_key_id`),
  CONSTRAINT `fk_pct7_cr_pred` FOREIGN KEY (`predecessor_catalog_release_id`) REFERENCES `0_pay_ctry_catalog_releases` (`catalog_release_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_cr_publisher` FOREIGN KEY (`publisher_id`) REFERENCES `0_pay_ctry_publishers` (`publisher_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_cr_signer` FOREIGN KEY (`signer_id`) REFERENCES `0_pay_ctry_signers` (`signer_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_cr_key` FOREIGN KEY (`trust_key_id`) REFERENCES `0_pay_ctry_trust_keys` (`trust_key_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_catalog_release_admissions` (
  `catalog_admission_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_release_id` bigint(20) unsigned NOT NULL,
  `decision` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signature_verified` tinyint(1) unsigned NOT NULL,
  `trust_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `maker_id` bigint(20) unsigned NOT NULL,
  `checker_id` bigint(20) unsigned NOT NULL,
  `decided_at` datetime NOT NULL,
  `admission_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`catalog_admission_id`),
  UNIQUE KEY `uq_pct7_ca_release` (`catalog_release_id`),
  UNIQUE KEY `uq_pct7_ca_sha` (`admission_sha256`),
  KEY `ix_pct7_ca_checker` (`checker_id`,`decided_at`),
  CONSTRAINT `fk_pct7_ca_release` FOREIGN KEY (`catalog_release_id`) REFERENCES `0_pay_ctry_catalog_releases` (`catalog_release_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notices` (
  `update_notice_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `notice_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `catalog_release_id` bigint(20) unsigned NOT NULL,
  `pack_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `current_pack_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `current_semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `current_evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `successor_pack_version` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `successor_semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `successor_evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `severity` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `deadline_at` datetime NOT NULL,
  `required_action` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `applicability_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `impact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `dedupe_key` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `notice_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `issued_at` datetime NOT NULL,
  `created_by` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`update_notice_id`),
  UNIQUE KEY `uq_pct7_un_key` (`notice_key`),
  UNIQUE KEY `uq_pct7_un_dedupe` (`dedupe_key`),
  UNIQUE KEY `uq_pct7_un_sha` (`notice_sha256`),
  KEY `ix_pct7_un_pack` (`pack_code`,`current_pack_version`,`successor_pack_version`),
  KEY `ix_pct7_un_deadline` (`severity`,`deadline_at`),
  KEY `fk_pct7_un_release` (`catalog_release_id`),
  CONSTRAINT `fk_pct7_un_release` FOREIGN KEY (`catalog_release_id`) REFERENCES `0_pay_ctry_catalog_releases` (`catalog_release_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notice_applicability` (
  `notice_applicability_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `update_notice_id` bigint(20) unsigned NOT NULL,
  `company_id` bigint(20) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `current_binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `run_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `affected_open_run_count` int(10) unsigned NOT NULL,
  `affected_future_run_count` int(10) unsigned NOT NULL,
  `impact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `applicability_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`notice_applicability_id`),
  UNIQUE KEY `uq_pct7_na_sha` (`applicability_sha256`),
  KEY `ix_pct7_na_notice` (`update_notice_id`,`company_id`),
  KEY `ix_pct7_na_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`),
  CONSTRAINT `fk_pct7_na_notice` FOREIGN KEY (`update_notice_id`) REFERENCES `0_pay_ctry_update_notices` (`update_notice_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notice_audiences` (
  `notice_audience_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `update_notice_id` bigint(20) unsigned NOT NULL,
  `notice_applicability_id` bigint(20) unsigned NOT NULL,
  `audience_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `audience_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `audience_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `recipient_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `audience_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `selected_by` bigint(20) unsigned NOT NULL,
  `selected_at` datetime NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`notice_audience_id`),
  UNIQUE KEY `uq_pct7_au_key` (`audience_key`),
  UNIQUE KEY `uq_pct7_au_sha` (`audience_sha256`),
  KEY `ix_pct7_au_notice` (`update_notice_id`,`notice_applicability_id`),
  KEY `ix_pct7_au_type` (`audience_type`,`selected_at`),
  CONSTRAINT `fk_pct7_au_notice` FOREIGN KEY (`update_notice_id`) REFERENCES `0_pay_ctry_update_notices` (`update_notice_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  KEY `fk_pct7_au_app` (`notice_applicability_id`),
  CONSTRAINT `fk_pct7_au_app` FOREIGN KEY (`notice_applicability_id`) REFERENCES `0_pay_ctry_update_notice_applicability` (`notice_applicability_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notice_recipient_events` (
  `recipient_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `notice_audience_id` bigint(20) unsigned NOT NULL,
  `update_notice_id` bigint(20) unsigned NOT NULL,
  `notice_applicability_id` bigint(20) unsigned NOT NULL,
  `recipient_user_id` bigint(20) unsigned NOT NULL,
  `recipient_event_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `event_no` int(10) unsigned NOT NULL,
  `predecessor_recipient_event_id` bigint(20) unsigned NULL,
  `predecessor_recipient_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `event_type` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `recipient_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `acted_by` bigint(20) unsigned NOT NULL,
  `occurred_at` datetime NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`recipient_event_id`),
  UNIQUE KEY `uq_pct7_re_key` (`recipient_event_key`),
  UNIQUE KEY `uq_pct7_re_stream` (`notice_audience_id`,`recipient_user_id`,`event_no`),
  UNIQUE KEY `uq_pct7_re_pred` (`predecessor_recipient_event_id`),
  UNIQUE KEY `uq_pct7_re_sha` (`recipient_event_sha256`),
  KEY `ix_pct7_re_user` (`recipient_user_id`,`occurred_at`),
  KEY `ix_pct7_re_notice` (`update_notice_id`,`notice_applicability_id`),
  CONSTRAINT `fk_pct7_re_audience` FOREIGN KEY (`notice_audience_id`) REFERENCES `0_pay_ctry_update_notice_audiences` (`notice_audience_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_re_notice` FOREIGN KEY (`update_notice_id`) REFERENCES `0_pay_ctry_update_notices` (`update_notice_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  KEY `fk_pct7_re_app` (`notice_applicability_id`),
  CONSTRAINT `fk_pct7_re_app` FOREIGN KEY (`notice_applicability_id`) REFERENCES `0_pay_ctry_update_notice_applicability` (`notice_applicability_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_re_pred` FOREIGN KEY (`predecessor_recipient_event_id`) REFERENCES `0_pay_ctry_update_notice_recipient_events` (`recipient_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notice_acknowledgements` (
  `notice_acknowledgement_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `recipient_event_id` bigint(20) unsigned NOT NULL,
  `update_notice_id` bigint(20) unsigned NOT NULL,
  `notice_applicability_id` bigint(20) unsigned NOT NULL,
  `recipient_user_id` bigint(20) unsigned NOT NULL,
  `acknowledgement_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `acknowledged_at` datetime NOT NULL,
  `acknowledgement_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `actor_id` bigint(20) unsigned NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`notice_acknowledgement_id`),
  UNIQUE KEY `uq_pct7_ack_recipient` (`recipient_event_id`),
  UNIQUE KEY `uq_pct7_ack_sha` (`acknowledgement_sha256`),
  KEY `ix_pct7_ack_user` (`recipient_user_id`,`acknowledged_at`),
  KEY `ix_pct7_ack_notice` (`update_notice_id`,`notice_applicability_id`),
  CONSTRAINT `fk_pct7_ack_recipient` FOREIGN KEY (`recipient_event_id`) REFERENCES `0_pay_ctry_update_notice_recipient_events` (`recipient_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_ack_notice` FOREIGN KEY (`update_notice_id`) REFERENCES `0_pay_ctry_update_notices` (`update_notice_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  KEY `fk_pct7_ack_app` (`notice_applicability_id`),
  CONSTRAINT `fk_pct7_ack_app` FOREIGN KEY (`notice_applicability_id`) REFERENCES `0_pay_ctry_update_notice_applicability` (`notice_applicability_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notice_lifecycle_events` (
  `notice_lifecycle_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `update_notice_id` bigint(20) unsigned NOT NULL,
  `notice_applicability_id` bigint(20) unsigned NOT NULL,
  `recipient_event_id` bigint(20) unsigned NULL,
  `lifecycle_event_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `event_type` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `due_at` datetime NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `lifecycle_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `actor_id` bigint(20) unsigned NOT NULL,
  `occurred_at` datetime NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`notice_lifecycle_event_id`),
  UNIQUE KEY `uq_pct7_le_key` (`lifecycle_event_key`),
  UNIQUE KEY `uq_pct7_le_sha` (`lifecycle_sha256`),
  KEY `ix_pct7_le_notice` (`update_notice_id`,`notice_applicability_id`,`occurred_at`),
  KEY `ix_pct7_le_recipient` (`recipient_event_id`,`event_type`,`occurred_at`),
  CONSTRAINT `fk_pct7_le_notice` FOREIGN KEY (`update_notice_id`) REFERENCES `0_pay_ctry_update_notices` (`update_notice_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  KEY `fk_pct7_le_app` (`notice_applicability_id`),
  CONSTRAINT `fk_pct7_le_app` FOREIGN KEY (`notice_applicability_id`) REFERENCES `0_pay_ctry_update_notice_applicability` (`notice_applicability_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_le_recipient` FOREIGN KEY (`recipient_event_id`) REFERENCES `0_pay_ctry_update_notice_recipient_events` (`recipient_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notice_delivery_outbox` (
  `delivery_outbox_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `recipient_event_id` bigint(20) unsigned NOT NULL,
  `update_notice_id` bigint(20) unsigned NOT NULL,
  `notice_applicability_id` bigint(20) unsigned NOT NULL,
  `recipient_user_id` bigint(20) unsigned NOT NULL,
  `outbox_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `channel` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `outbox_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `queued_by` bigint(20) unsigned NOT NULL,
  `queued_at` datetime NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`delivery_outbox_id`),
  UNIQUE KEY `uq_pct7_ob_key` (`outbox_key`),
  UNIQUE KEY `uq_pct7_ob_recipient_channel` (`recipient_event_id`,`channel`),
  UNIQUE KEY `uq_pct7_ob_sha` (`outbox_sha256`),
  KEY `ix_pct7_ob_user` (`recipient_user_id`,`channel`,`queued_at`),
  KEY `ix_pct7_ob_notice` (`update_notice_id`,`notice_applicability_id`),
  CONSTRAINT `fk_pct7_ob_recipient` FOREIGN KEY (`recipient_event_id`) REFERENCES `0_pay_ctry_update_notice_recipient_events` (`recipient_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_pct7_ob_notice` FOREIGN KEY (`update_notice_id`) REFERENCES `0_pay_ctry_update_notices` (`update_notice_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  KEY `fk_pct7_ob_app` (`notice_applicability_id`),
  CONSTRAINT `fk_pct7_ob_app` FOREIGN KEY (`notice_applicability_id`) REFERENCES `0_pay_ctry_update_notice_applicability` (`notice_applicability_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_ctry_update_notice_delivery_attempts` (
  `delivery_attempt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `delivery_outbox_id` bigint(20) unsigned NOT NULL,
  `attempt_no` int(10) unsigned NOT NULL,
  `provider_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `delivery_status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `error_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `delivery_attempt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `attempted_by` bigint(20) unsigned NOT NULL,
  `attempted_at` datetime NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`delivery_attempt_id`),
  UNIQUE KEY `uq_pct7_da_attempt` (`delivery_outbox_id`,`attempt_no`),
  UNIQUE KEY `uq_pct7_da_sha` (`delivery_attempt_sha256`),
  KEY `ix_pct7_da_status` (`delivery_outbox_id`,`delivery_status`,`attempted_at`),
  CONSTRAINT `fk_pct7_da_outbox` FOREIGN KEY (`delivery_outbox_id`) REFERENCES `0_pay_ctry_update_notice_delivery_outbox` (`delivery_outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_journal_intents` (
 `journal_intent_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `payroll_group_id` bigint(20) unsigned NOT NULL, `payroll_period_id` bigint(20) unsigned NOT NULL,
 `result_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `journal_date` date NOT NULL, `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `mapping_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `line_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `allocation_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `debit_minor` bigint(20) unsigned NOT NULL, `credit_minor` bigint(20) unsigned NOT NULL,
 `journal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `prepared_by` bigint(20) unsigned NOT NULL, `prepared_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`journal_intent_id`), UNIQUE KEY `uq_pgl1_ji_sha` (`journal_sha256`),
 UNIQUE KEY `uq_pgl1_ji_result` (`result_set_sha256`,`approval_sha256`),
 KEY `ix_pgl1_ji_scope` (`legal_entity_id`,`payroll_group_id`,`payroll_period_id`), KEY `ix_pgl1_ji_date` (`journal_date`,`currency_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_journal_lines` (
 `journal_line_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `journal_intent_id` bigint(20) unsigned NOT NULL,
 `source_result_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_line_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `line_no` int(10) unsigned NOT NULL, `account_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `entry_side` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `amount_minor` bigint(20) unsigned NOT NULL,
 `memo_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `exception_reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
 `exception_approved_by` bigint(20) unsigned NULL,
 `mapping_rule_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `canonical_line_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`journal_line_id`), UNIQUE KEY `uq_pgl1_jl_no` (`journal_intent_id`,`line_no`), UNIQUE KEY `uq_pgl1_jl_sha` (`canonical_line_sha256`),
 KEY `ix_pgl1_jl_account` (`account_code`,`entry_side`), KEY `ix_pgl1_jl_source` (`source_result_sha256`,`source_line_key`),
 KEY `ix_pgl1_jl_mapping` (`mapping_rule_sha256`), KEY `fk_pgl1_jl_intent` (`journal_intent_id`),
 CONSTRAINT `fk_pgl1_jl_intent` FOREIGN KEY (`journal_intent_id`) REFERENCES `0_hrm_pay_gl_001_journal_intents` (`journal_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_allocations` (
 `allocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `journal_line_id` bigint(20) unsigned NOT NULL,
 `allocation_no` int(10) unsigned NOT NULL, `allocated_amount_minor` bigint(20) unsigned NOT NULL,
 `allocation_weight_ppm` int(10) unsigned NOT NULL, `allocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`allocation_id`), UNIQUE KEY `uq_pgl1_al_no` (`journal_line_id`,`allocation_no`), UNIQUE KEY `uq_pgl1_al_sha` (`allocation_sha256`), KEY `fk_pgl1_al_line` (`journal_line_id`),
 CONSTRAINT `fk_pgl1_al_line` FOREIGN KEY (`journal_line_id`) REFERENCES `0_hrm_pay_gl_001_journal_lines` (`journal_line_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_allocation_axes` (
 `allocation_axis_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `allocation_id` bigint(20) unsigned NOT NULL,
 `axis_no` int(10) unsigned NOT NULL, `axis_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `axis_value_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `axis_value_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `core_slot_hint` tinyint(3) unsigned NULL,
 `axis_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`allocation_axis_id`), UNIQUE KEY `uq_pgl1_ax_no` (`allocation_id`,`axis_no`), UNIQUE KEY `uq_pgl1_ax_code` (`allocation_id`,`axis_code`), UNIQUE KEY `uq_pgl1_ax_sha` (`axis_sha256`), KEY `fk_pgl1_ax_alloc` (`allocation_id`),
 CONSTRAINT `fk_pgl1_ax_alloc` FOREIGN KEY (`allocation_id`) REFERENCES `0_hrm_pay_gl_001_allocations` (`allocation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_mapping_versions` (
 `mapping_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `mapping_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `effective_from` date NOT NULL, `effective_to` date NULL,
 `maker_id` bigint(20) unsigned NOT NULL, `checker_id` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL, `mapping_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`mapping_version_id`), UNIQUE KEY `uq_pgl1_mv_scope` (`company_id`,`legal_entity_id`,`mapping_key`,`effective_from`),
 UNIQUE KEY `uq_pgl1_mv_sha` (`mapping_snapshot_sha256`), KEY `ix_pgl1_mv_effective` (`company_id`,`legal_entity_id`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_mapping_rules` (
 `mapping_rule_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `mapping_version_id` bigint(20) unsigned NOT NULL,
 `rule_no` int(10) unsigned NOT NULL, `source_kind` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `entry_side` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `account_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `dimension1_axis_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
 `dimension2_axis_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
 `suspense_allowed` tinyint(1) unsigned NOT NULL DEFAULT 0, `rule_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`mapping_rule_id`), UNIQUE KEY `uq_pgl1_mr_no` (`mapping_version_id`,`rule_no`),
 UNIQUE KEY `uq_pgl1_mr_sha` (`rule_sha256`), KEY `ix_pgl1_mr_source` (`source_kind`,`source_key`,`entry_side`),
 KEY `fk_pgl1_mr_version` (`mapping_version_id`),
 CONSTRAINT `fk_pgl1_mr_version` FOREIGN KEY (`mapping_version_id`) REFERENCES `0_hrm_pay_gl_001_mapping_versions` (`mapping_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_reconciliations` (
 `reconciliation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `journal_intent_id` bigint(20) unsigned NOT NULL,
 `journal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `line_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `allocation_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `native_projection_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `debit_minor` bigint(20) unsigned NOT NULL, `credit_minor` bigint(20) unsigned NOT NULL,
 `status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reconciliation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_by` bigint(20) unsigned NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`reconciliation_id`), UNIQUE KEY `uq_pgl1_rc_intent` (`journal_intent_id`),
 UNIQUE KEY `uq_pgl1_rc_sha` (`reconciliation_sha256`), KEY `fk_pgl1_rc_intent` (`journal_intent_id`),
 CONSTRAINT `fk_pgl1_rc_intent` FOREIGN KEY (`journal_intent_id`) REFERENCES `0_hrm_pay_gl_001_journal_intents` (`journal_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_reviews` (
 `review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `reconciliation_id` bigint(20) unsigned NOT NULL,
 `decision` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `reviewer_id` bigint(20) unsigned NOT NULL,
 `reviewed_at` datetime NOT NULL, `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`review_id`), UNIQUE KEY `uq_pgl1_rv_recon` (`reconciliation_id`),
 UNIQUE KEY `uq_pgl1_rv_sha` (`review_sha256`), KEY `fk_pgl1_rv_recon` (`reconciliation_id`),
 CONSTRAINT `fk_pgl1_rv_recon` FOREIGN KEY (`reconciliation_id`) REFERENCES `0_hrm_pay_gl_001_reconciliations` (`reconciliation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_posting_authorities` (
 `authority_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `journal_intent_id` bigint(20) unsigned NOT NULL,
 `reconciliation_id` bigint(20) unsigned NOT NULL, `review_id` bigint(20) unsigned NOT NULL,
 `command_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `executor_id` bigint(20) unsigned NOT NULL,
 `authorized_at` datetime NOT NULL, `authority_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`authority_id`), UNIQUE KEY `uq_pgl1_pa_cmd` (`command_key_sha256`), UNIQUE KEY `uq_pgl1_pa_sha` (`authority_sha256`),
 UNIQUE KEY `uq_pgl1_pa_intent` (`journal_intent_id`), KEY `fk_pgl1_pa_recon` (`reconciliation_id`), KEY `fk_pgl1_pa_review` (`review_id`),
 CONSTRAINT `fk_pgl1_pa_intent` FOREIGN KEY (`journal_intent_id`) REFERENCES `0_hrm_pay_gl_001_journal_intents` (`journal_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_pa_recon` FOREIGN KEY (`reconciliation_id`) REFERENCES `0_hrm_pay_gl_001_reconciliations` (`reconciliation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_pa_review` FOREIGN KEY (`review_id`) REFERENCES `0_hrm_pay_gl_001_reviews` (`review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_gl_postings` (
 `posting_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `journal_intent_id` bigint(20) unsigned NOT NULL,
 `authority_id` bigint(20) unsigned NOT NULL, `posting_kind` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `predecessor_posting_id` bigint(20) unsigned NULL, `effect_key` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `gl_type` int(11) NOT NULL, `gl_trans_no` bigint(20) unsigned NOT NULL, `journal_date` date NOT NULL,
 `posting_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `posted_by` bigint(20) unsigned NOT NULL,
 `posted_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`posting_id`), UNIQUE KEY `uq_pgl1_gp_effect` (`effect_key`), UNIQUE KEY `uq_pgl1_gp_sha` (`posting_sha256`),
 UNIQUE KEY `uq_pgl1_gp_gl` (`gl_type`,`gl_trans_no`), KEY `ix_pgl1_gp_intent` (`journal_intent_id`,`posting_kind`),
 KEY `fk_pgl1_gp_auth` (`authority_id`), KEY `fk_pgl1_gp_pred` (`predecessor_posting_id`),
 CONSTRAINT `fk_pgl1_gp_intent` FOREIGN KEY (`journal_intent_id`) REFERENCES `0_hrm_pay_gl_001_journal_intents` (`journal_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_gp_auth` FOREIGN KEY (`authority_id`) REFERENCES `0_hrm_pay_gl_001_posting_authorities` (`authority_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_gp_pred` FOREIGN KEY (`predecessor_posting_id`) REFERENCES `0_hrm_pay_gl_001_gl_postings` (`posting_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_gl_posting_lines` (
 `posting_line_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `posting_id` bigint(20) unsigned NOT NULL,
 `journal_line_id` bigint(20) unsigned NOT NULL, `allocation_id` bigint(20) unsigned NOT NULL,
 `core_dimension_id` bigint(20) unsigned NOT NULL DEFAULT 0, `core_dimension2_id` bigint(20) unsigned NOT NULL DEFAULT 0,
 `gl_counter` bigint(20) unsigned NOT NULL, `source_amount_minor` bigint(20) unsigned NOT NULL,
 `home_amount_minor` bigint(20) NOT NULL, `projection_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`posting_line_id`), UNIQUE KEY `uq_pgl1_gpl_alloc` (`posting_id`,`allocation_id`),
 UNIQUE KEY `uq_pgl1_gpl_counter` (`gl_counter`), UNIQUE KEY `uq_pgl1_gpl_sha` (`projection_sha256`),
 KEY `fk_pgl1_gpl_post` (`posting_id`), KEY `fk_pgl1_gpl_line` (`journal_line_id`), KEY `fk_pgl1_gpl_alloc` (`allocation_id`),
 CONSTRAINT `fk_pgl1_gpl_post` FOREIGN KEY (`posting_id`) REFERENCES `0_hrm_pay_gl_001_gl_postings` (`posting_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_gpl_line` FOREIGN KEY (`journal_line_id`) REFERENCES `0_hrm_pay_gl_001_journal_lines` (`journal_line_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_gpl_alloc` FOREIGN KEY (`allocation_id`) REFERENCES `0_hrm_pay_gl_001_allocations` (`allocation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_recovery_commands` (
 `recovery_command_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `company_id` bigint(20) unsigned NOT NULL,
 `journal_intent_id` bigint(20) unsigned NOT NULL, `target_posting_id` bigint(20) unsigned NOT NULL,
 `command_kind` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recovery_date` date NOT NULL, `command_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `maker_id` bigint(20) unsigned NOT NULL, `reviewer_id` bigint(20) unsigned NOT NULL, `approved_at` datetime NOT NULL,
 `command_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`recovery_command_id`), UNIQUE KEY `uq_pgl1_gc_cmd` (`command_key_sha256`), UNIQUE KEY `uq_pgl1_gc_sha` (`command_sha256`),
 KEY `ix_pgl1_gc_target` (`target_posting_id`,`command_kind`), KEY `fk_pgl1_gc_intent` (`journal_intent_id`),
 CONSTRAINT `fk_pgl1_gc_intent` FOREIGN KEY (`journal_intent_id`) REFERENCES `0_hrm_pay_gl_001_journal_intents` (`journal_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_gc_target` FOREIGN KEY (`target_posting_id`) REFERENCES `0_hrm_pay_gl_001_gl_postings` (`posting_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_reversal_repost_links` (
 `recovery_link_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `recovery_command_id` bigint(20) unsigned NOT NULL,
 `predecessor_posting_id` bigint(20) unsigned NOT NULL, `successor_posting_id` bigint(20) unsigned NOT NULL,
 `link_kind` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `effect_key` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`recovery_link_id`), UNIQUE KEY `uq_pgl1_gr_cmd` (`recovery_command_id`),
 UNIQUE KEY `uq_pgl1_gr_effect` (`effect_key`), UNIQUE KEY `uq_pgl1_gr_succ` (`successor_posting_id`),
 KEY `fk_pgl1_gr_pred` (`predecessor_posting_id`), KEY `fk_pgl1_gr_succ` (`successor_posting_id`),
 CONSTRAINT `fk_pgl1_gr_cmd` FOREIGN KEY (`recovery_command_id`) REFERENCES `0_hrm_pay_gl_001_recovery_commands` (`recovery_command_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_gr_pred` FOREIGN KEY (`predecessor_posting_id`) REFERENCES `0_hrm_pay_gl_001_gl_postings` (`posting_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pgl1_gr_succ` FOREIGN KEY (`successor_posting_id`) REFERENCES `0_hrm_pay_gl_001_gl_postings` (`posting_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_gl_001_cutovers` (
 `cutover_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `payroll_group_id` bigint(20) unsigned NOT NULL,
 `effective_from` date NOT NULL,
 `mapping_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL, `reviewer_id` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL, `cutover_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`cutover_id`),
 UNIQUE KEY `uq_pgl1_co_scope` (`company_id`,`legal_entity_id`,`payroll_group_id`,`effective_from`),
 UNIQUE KEY `uq_pgl1_co_sha` (`cutover_sha256`),
 KEY `ix_pgl1_co_effective` (`company_id`,`legal_entity_id`,`payroll_group_id`,`effective_from`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_payment_intents` (
 `payment_intent_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `payroll_group_id` bigint(20) unsigned NOT NULL, `payroll_period_id` bigint(20) unsigned NOT NULL,
 `result_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `pay_gl_journal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `total_minor` bigint(20) unsigned NOT NULL,
 `item_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `allocation_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `intent_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `prepared_by` bigint(20) unsigned NOT NULL, `prepared_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_intent_id`),
 UNIQUE KEY `uq_ppm1_pi_sha` (`intent_sha256`),
 UNIQUE KEY `uq_ppm1_pi_result` (`result_set_sha256`,`approval_sha256`,`currency_code`),
 KEY `ix_ppm1_pi_scope` (`company_id`,`legal_entity_id`,`payroll_group_id`,`payroll_period_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_payment_items` (
 `payment_item_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `payment_intent_id` bigint(20) unsigned NOT NULL, `item_no` int(10) unsigned NOT NULL,
 `beneficiary_subject_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_result_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_line_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `amount_minor` bigint(20) unsigned NOT NULL,
 `payment_election_id` bigint(20) unsigned NOT NULL,
 `payment_election_row_version` bigint(20) unsigned NOT NULL,
 `bank_allocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `item_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_item_id`), UNIQUE KEY `uq_ppm1_it_no` (`payment_intent_id`,`item_no`),
 UNIQUE KEY `uq_ppm1_it_sha` (`item_sha256`), KEY `ix_ppm1_it_benef` (`beneficiary_subject_sha256`,`currency_code`),
 KEY `fk_ppm1_it_intent` (`payment_intent_id`),
 CONSTRAINT `fk_ppm1_it_intent` FOREIGN KEY (`payment_intent_id`) REFERENCES `0_hrm_pay_pmt_001_payment_intents` (`payment_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_item_allocations` (
 `payment_allocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `payment_item_id` bigint(20) unsigned NOT NULL, `allocation_no` int(10) unsigned NOT NULL,
 `person_bank_account_id` bigint(20) unsigned NOT NULL, `bank_account_row_version` bigint(20) unsigned NOT NULL,
 `allocation_weight_ppm` int(10) unsigned NOT NULL, `allocated_amount_minor` bigint(20) unsigned NOT NULL,
 `beneficiary_token_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `allocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_allocation_id`), UNIQUE KEY `uq_ppm1_al_no` (`payment_item_id`,`allocation_no`),
 UNIQUE KEY `uq_ppm1_al_sha` (`allocation_sha256`), KEY `ix_ppm1_al_bank` (`person_bank_account_id`,`bank_account_row_version`),
 KEY `fk_ppm1_al_item` (`payment_item_id`),
 CONSTRAINT `fk_ppm1_al_item` FOREIGN KEY (`payment_item_id`) REFERENCES `0_hrm_pay_pmt_001_payment_items` (`payment_item_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_batches` (
 `payment_batch_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `payroll_group_id` bigint(20) unsigned NOT NULL, `payroll_period_id` bigint(20) unsigned NOT NULL,
 `payment_date` date NOT NULL, `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `intent_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `item_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `total_minor` bigint(20) unsigned NOT NULL,
 `batch_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `prepared_by` bigint(20) unsigned NOT NULL, `prepared_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_batch_id`), UNIQUE KEY `uq_ppm1_ba_sha` (`batch_sha256`),
 KEY `ix_ppm1_ba_scope` (`company_id`,`legal_entity_id`,`payroll_group_id`,`payroll_period_id`),
 KEY `ix_ppm1_ba_date` (`payment_date`,`currency_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_batch_items` (
 `batch_item_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `payment_batch_id` bigint(20) unsigned NOT NULL, `payment_item_id` bigint(20) unsigned NOT NULL,
 `batch_item_no` int(10) unsigned NOT NULL,
 `payment_item_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `batch_item_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`batch_item_id`), UNIQUE KEY `uq_ppm1_bi_no` (`payment_batch_id`,`batch_item_no`),
 UNIQUE KEY `uq_ppm1_bi_item` (`payment_batch_id`,`payment_item_id`), UNIQUE KEY `uq_ppm1_bi_sha` (`batch_item_sha256`),
 KEY `fk_ppm1_bi_batch` (`payment_batch_id`), KEY `fk_ppm1_bi_item` (`payment_item_id`),
 CONSTRAINT `fk_ppm1_bi_batch` FOREIGN KEY (`payment_batch_id`) REFERENCES `0_hrm_pay_pmt_001_batches` (`payment_batch_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_ppm1_bi_item` FOREIGN KEY (`payment_item_id`) REFERENCES `0_hrm_pay_pmt_001_payment_items` (`payment_item_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_batch_releases` (
 `batch_release_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `payment_batch_id` bigint(20) unsigned NOT NULL,
 `batch_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `maker_id` bigint(20) unsigned NOT NULL, `checker_id` bigint(20) unsigned NOT NULL,
 `assurance_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `decision` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `release_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `released_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`batch_release_id`), UNIQUE KEY `uq_ppm1_br_sha` (`release_sha256`),
 UNIQUE KEY `uq_ppm1_br_batch` (`payment_batch_id`),
 CONSTRAINT `fk_ppm1_br_batch` FOREIGN KEY (`payment_batch_id`) REFERENCES `0_hrm_pay_pmt_001_batches` (`payment_batch_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_artifacts` (
 `payment_artifact_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `batch_release_id` bigint(20) unsigned NOT NULL,
 `release_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_batch_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `country_pack_version_id` bigint(20) unsigned NOT NULL,
 `renderer_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `renderer_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_config_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `media_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `external_object_ref_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `total_minor` bigint(20) unsigned NOT NULL, `item_count` int(10) unsigned NOT NULL,
 `rendered_by` bigint(20) unsigned NOT NULL, `rendered_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_artifact_id`), UNIQUE KEY `uq_ppm1_ar_sha` (`artifact_sha256`),
 UNIQUE KEY `uq_ppm1_ar_binding` (`batch_release_id`,`provider_id`,`provider_config_sha256`),
 KEY `ix_ppm1_ar_batch` (`source_batch_sha256`),
 CONSTRAINT `fk_ppm1_ar_release` FOREIGN KEY (`batch_release_id`) REFERENCES `0_hrm_pay_pmt_001_batch_releases` (`batch_release_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_outbox` (
 `outbox_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `payment_artifact_id` bigint(20) unsigned NOT NULL,
 `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_config_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `transmission_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `idempotency_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `authorized_by` bigint(20) unsigned NOT NULL,
 `assurance_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `authorized_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`outbox_id`), UNIQUE KEY `uq_ppm1_ob_idem` (`idempotency_key_sha256`),
 KEY `ix_ppm1_ob_artifact` (`payment_artifact_id`),
 CONSTRAINT `fk_ppm1_ob_artifact` FOREIGN KEY (`payment_artifact_id`) REFERENCES `0_hrm_pay_pmt_001_artifacts` (`payment_artifact_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_transmission_attempts` (
 `transmission_attempt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `outbox_id` bigint(20) unsigned NOT NULL, `attempt_no` int(10) unsigned NOT NULL,
 `attempt_status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `result_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `uncertain_result` tinyint(1) unsigned NOT NULL DEFAULT 0,
 `attempt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `attempted_by` bigint(20) unsigned NOT NULL, `attempted_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`transmission_attempt_id`), UNIQUE KEY `uq_ppm1_ta_no` (`outbox_id`,`attempt_no`),
 UNIQUE KEY `uq_ppm1_ta_sha` (`attempt_sha256`), KEY `ix_ppm1_ta_status` (`outbox_id`,`attempt_status`),
 CONSTRAINT `fk_ppm1_ta_outbox` FOREIGN KEY (`outbox_id`) REFERENCES `0_hrm_pay_pmt_001_outbox` (`outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_failures` (
 `payment_failure_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `outbox_id` bigint(20) unsigned NOT NULL, `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `failure_kind` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `affected_amount_minor` bigint(20) unsigned NOT NULL,
 `affected_item_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `failure_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by` bigint(20) unsigned NOT NULL, `recorded_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_failure_id`), UNIQUE KEY `uq_ppm1_fl_sha` (`failure_sha256`),
 KEY `ix_ppm1_fl_outbox` (`outbox_id`,`failure_kind`),
 CONSTRAINT `fk_ppm1_fl_outbox` FOREIGN KEY (`outbox_id`) REFERENCES `0_hrm_pay_pmt_001_outbox` (`outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_reissues` (
 `payment_reissue_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `payment_failure_id` bigint(20) unsigned NOT NULL,
 `failure_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `predecessor_payment_item_id` bigint(20) unsigned NOT NULL,
 `predecessor_payment_item_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `successor_payment_intent_id` bigint(20) unsigned NOT NULL,
 `successor_payment_intent_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `maker_id` bigint(20) unsigned NOT NULL, `checker_id` bigint(20) unsigned NOT NULL,
 `assurance_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reissue_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_reissue_id`), UNIQUE KEY `uq_ppm1_ri_sha` (`reissue_sha256`),
 UNIQUE KEY `uq_ppm1_ri_pred` (`payment_failure_id`,`predecessor_payment_item_id`),
 KEY `ix_ppm1_ri_succ` (`successor_payment_intent_id`),
 CONSTRAINT `fk_ppm1_ri_failure` FOREIGN KEY (`payment_failure_id`) REFERENCES `0_hrm_pay_pmt_001_failures` (`payment_failure_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_ppm1_ri_pred` FOREIGN KEY (`predecessor_payment_item_id`) REFERENCES `0_hrm_pay_pmt_001_payment_items` (`payment_item_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_ppm1_ri_succ` FOREIGN KEY (`successor_payment_intent_id`) REFERENCES `0_hrm_pay_pmt_001_payment_intents` (`payment_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_settlements` (
 `payment_settlement_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `outbox_id` bigint(20) unsigned NOT NULL,
 `provider_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `settled_minor` bigint(20) unsigned NOT NULL,
 `settlement_status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `settlement_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by` bigint(20) unsigned NOT NULL, `settled_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_settlement_id`), UNIQUE KEY `uq_ppm1_st_sha` (`settlement_sha256`),
 KEY `ix_ppm1_st_outbox` (`outbox_id`,`settlement_status`),
 CONSTRAINT `fk_ppm1_st_outbox` FOREIGN KEY (`outbox_id`) REFERENCES `0_hrm_pay_pmt_001_outbox` (`outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_reconciliations` (
 `payment_reconciliation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `payment_intent_id` bigint(20) unsigned NOT NULL, `payment_batch_id` bigint(20) unsigned NOT NULL,
 `payment_artifact_id` bigint(20) unsigned NOT NULL, `outbox_id` bigint(20) unsigned NOT NULL,
 `payment_settlement_id` bigint(20) unsigned DEFAULT NULL,
 `payment_intent_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `batch_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `transmission_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `settlement_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `pay_gl_journal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approved_minor` bigint(20) unsigned NOT NULL, `intent_minor` bigint(20) unsigned NOT NULL,
 `batch_minor` bigint(20) unsigned NOT NULL, `artifact_minor` bigint(20) unsigned NOT NULL,
 `settled_minor` bigint(20) unsigned NOT NULL,
 `status` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `discrepancy_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `reconciliation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewer_id` bigint(20) unsigned NOT NULL, `reviewed_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_reconciliation_id`), UNIQUE KEY `uq_ppm1_rc_sha` (`reconciliation_sha256`),
 KEY `ix_ppm1_rc_batch` (`payment_batch_id`,`status`), KEY `ix_ppm1_rc_outbox` (`outbox_id`),
 CONSTRAINT `fk_ppm1_rc_intent` FOREIGN KEY (`payment_intent_id`) REFERENCES `0_hrm_pay_pmt_001_payment_intents` (`payment_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_ppm1_rc_batch` FOREIGN KEY (`payment_batch_id`) REFERENCES `0_hrm_pay_pmt_001_batches` (`payment_batch_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_ppm1_rc_artifact` FOREIGN KEY (`payment_artifact_id`) REFERENCES `0_hrm_pay_pmt_001_artifacts` (`payment_artifact_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_ppm1_rc_outbox` FOREIGN KEY (`outbox_id`) REFERENCES `0_hrm_pay_pmt_001_outbox` (`outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_ppm1_rc_settlement` FOREIGN KEY (`payment_settlement_id`) REFERENCES `0_hrm_pay_pmt_001_settlements` (`payment_settlement_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_pmt_001_cutovers` (
 `payment_cutover_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `payroll_group_id` bigint(20) unsigned NOT NULL, `effective_from` date NOT NULL,
 `proposal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `maker_id` bigint(20) unsigned NOT NULL, `checker_id` bigint(20) unsigned NOT NULL,
 `assurance_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `cutover_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`payment_cutover_id`), UNIQUE KEY `uq_ppm1_co_sha` (`cutover_sha256`),
 UNIQUE KEY `uq_ppm1_co_scope` (`company_id`,`legal_entity_id`,`payroll_group_id`,`effective_from`),
 KEY `ix_ppm1_co_current` (`company_id`,`legal_entity_id`,`payroll_group_id`,`effective_from`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_obligations` (
 `filing_obligation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `jurisdiction_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `obligation_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `period_start` date NOT NULL, `period_end` date NOT NULL, `due_date` date NOT NULL,
 `filing_frequency` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `country_pack_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `country_pack_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `schema_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `schema_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `connector_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `connector_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `obligation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `prepared_by` bigint(20) unsigned NOT NULL, `prepared_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`filing_obligation_id`), UNIQUE KEY `uq_pfl1_ob_sha` (`obligation_sha256`),
 UNIQUE KEY `uq_pfl1_ob_sem` (`company_id`,`legal_entity_id`,`jurisdiction_code`,`obligation_type`,`period_end`,`country_pack_version`,`schema_version`),
 KEY `ix_pfl1_ob_due` (`company_id`,`legal_entity_id`,`due_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_source_sets` (
 `filing_source_set_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `filing_obligation_id` bigint(20) unsigned NOT NULL,
 `source_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `run_count` int(10) unsigned NOT NULL, `result_count` int(10) unsigned NOT NULL,
 `line_count` int(10) unsigned NOT NULL, `balance_count` int(10) unsigned NOT NULL, `liability_count` int(10) unsigned NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `liability_total_minor` bigint(20) NOT NULL,
 `bound_by` bigint(20) unsigned NOT NULL, `bound_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`filing_source_set_id`), UNIQUE KEY `uq_pfl1_ss_sha` (`source_set_sha256`),
 UNIQUE KEY `uq_pfl1_ss_ob` (`filing_obligation_id`,`source_set_sha256`),
 KEY `fk_pfl1_ss_ob` (`filing_obligation_id`),
 CONSTRAINT `fk_pfl1_ss_ob` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_source_links` (
 `filing_source_link_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `filing_source_set_id` bigint(20) unsigned NOT NULL, `link_no` int(10) unsigned NOT NULL,
 `source_kind` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `predecessor_source_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `run_kind` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `amount_minor` bigint(20) NOT NULL,
 `source_link_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`filing_source_link_id`), UNIQUE KEY `uq_pfl1_sl_no` (`filing_source_set_id`,`link_no`),
 UNIQUE KEY `uq_pfl1_sl_sha` (`source_link_sha256`), KEY `ix_pfl1_sl_src` (`source_kind`,`source_sha256`),
 KEY `fk_pfl1_sl_set` (`filing_source_set_id`),
 CONSTRAINT `fk_pfl1_sl_set` FOREIGN KEY (`filing_source_set_id`) REFERENCES `0_hrm_pay_fil_001_source_sets` (`filing_source_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_artifacts` (
 `filing_artifact_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `filing_obligation_id` bigint(20) unsigned NOT NULL, `filing_source_set_id` bigint(20) unsigned NOT NULL,
 `obligation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `source_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `renderer_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `renderer_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `schema_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `schema_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `media_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `byte_length` bigint(20) unsigned NOT NULL, `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `rendered_by` bigint(20) unsigned NOT NULL, `rendered_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`filing_artifact_id`), UNIQUE KEY `uq_pfl1_ar_sha` (`artifact_sha256`), UNIQUE KEY `uq_pfl1_ar_set` (`filing_source_set_id`,`artifact_sha256`),
 KEY `fk_pfl1_ar_ob` (`filing_obligation_id`), KEY `fk_pfl1_ar_set` (`filing_source_set_id`),
 CONSTRAINT `fk_pfl1_ar_ob` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_ar_set` FOREIGN KEY (`filing_source_set_id`) REFERENCES `0_hrm_pay_fil_001_source_sets` (`filing_source_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_validations` (
 `filing_validation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `filing_artifact_id` bigint(20) unsigned NOT NULL,
 `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `validator_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `validator_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `schema_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `schema_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `validation_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `findings_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `validation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `validated_by` bigint(20) unsigned NOT NULL, `validated_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`filing_validation_id`), UNIQUE KEY `uq_pfl1_va_sha` (`validation_sha256`), KEY `ix_pfl1_va_art` (`artifact_sha256`), KEY `fk_pfl1_va_art` (`filing_artifact_id`),
 CONSTRAINT `fk_pfl1_va_art` FOREIGN KEY (`filing_artifact_id`) REFERENCES `0_hrm_pay_fil_001_artifacts` (`filing_artifact_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_schema_authorities` (
 `filing_schema_authority_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL, `legal_entity_id` bigint(20) unsigned NOT NULL,
 `jurisdiction_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `obligation_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `country_pack_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `country_pack_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `schema_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `schema_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `schema_artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `legal_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `technical_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `certification_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `generation_allowed` tinyint(1) unsigned NOT NULL, `transmission_allowed` tinyint(1) unsigned NOT NULL,
 `connector_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `connector_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `effective_from` date NOT NULL, `effective_to` date NOT NULL, `authority_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL, `approved_by` bigint(20) unsigned NOT NULL, `approved_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`filing_schema_authority_id`), UNIQUE KEY `uq_pfl1_sa_sha` (`authority_sha256`),
 UNIQUE KEY `uq_pfl1_sa_scope` (`company_id`,`legal_entity_id`,`jurisdiction_code`,`obligation_type`,`country_pack_id`,`country_pack_version`,`schema_id`,`schema_version`,`effective_from`),
 KEY `ix_pfl1_sa_effective` (`company_id`,`legal_entity_id`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_authorization_intents` (
 `filing_authorization_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `filing_obligation_id` bigint(20) unsigned NOT NULL, `filing_artifact_id` bigint(20) unsigned NOT NULL,
 `filing_validation_id` bigint(20) unsigned NOT NULL, `filing_schema_authority_id` bigint(20) unsigned NOT NULL,
 `obligation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `artifact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `validation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `authority_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `authorization_kind` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `predecessor_authorization_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `authorization_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `prepared_by` bigint(20) unsigned NOT NULL, `reviewed_by` bigint(20) unsigned NOT NULL, `authorized_by` bigint(20) unsigned NOT NULL,
 `authorized_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`filing_authorization_id`), UNIQUE KEY `uq_pfl1_au_sha` (`authorization_sha256`),
 UNIQUE KEY `uq_pfl1_au_art` (`filing_artifact_id`,`authorization_sha256`),
 KEY `fk_pfl1_au_ob` (`filing_obligation_id`), KEY `fk_pfl1_au_art` (`filing_artifact_id`), KEY `fk_pfl1_au_val` (`filing_validation_id`), KEY `fk_pfl1_au_sa` (`filing_schema_authority_id`),
 CONSTRAINT `fk_pfl1_au_ob` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_au_art` FOREIGN KEY (`filing_artifact_id`) REFERENCES `0_hrm_pay_fil_001_artifacts` (`filing_artifact_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_au_val` FOREIGN KEY (`filing_validation_id`) REFERENCES `0_hrm_pay_fil_001_validations` (`filing_validation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_au_sa` FOREIGN KEY (`filing_schema_authority_id`) REFERENCES `0_hrm_pay_fil_001_schema_authorities` (`filing_schema_authority_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_tt80_taxpayer_profiles` (
 `tt80_taxpayer_profile_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `filing_obligation_id` bigint(20) unsigned NOT NULL,
 `taxpayer_tax_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `taxpayer_name` varchar(255) NOT NULL,
 `taxpayer_address` varchar(512) NOT NULL,
 `taxpayer_ward_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `taxpayer_ward_name` varchar(255) NOT NULL,
 `taxpayer_province_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `taxpayer_province_name` varchar(255) NOT NULL,
 `tax_office_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `tax_office_name` varchar(255) NOT NULL,
 `source_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `captured_by` bigint(20) unsigned NOT NULL,
 `captured_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`tt80_taxpayer_profile_id`),
 UNIQUE KEY `uq_pfl1_tp_sha` (`profile_sha256`),
 UNIQUE KEY `uq_pfl1_tp_ob_sha` (`filing_obligation_id`,`profile_sha256`),
 KEY `fk_pfl1_tp_ob` (`filing_obligation_id`),
 CONSTRAINT `fk_pfl1_tp_ob` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_tt80_person_facts` (
 `tt80_person_fact_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `filing_source_set_id` bigint(20) unsigned NOT NULL,
 `subject_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `paid_worker` tinyint(1) unsigned NOT NULL,
 `withheld_person` tinyint(1) unsigned NOT NULL,
 `tax_residency` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `employment_contract_term_class` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `filing_taxable_income_minor` bigint(20) NOT NULL,
 `withholding_subject_taxable_income_minor` bigint(20) NOT NULL,
 `personal_income_tax_withheld_minor` bigint(20) NOT NULL,
 `foreign_insurer_nonmandatory_insurance_taxable_minor` bigint(20) NOT NULL,
 `foreign_insurer_nonmandatory_insurance_withheld_minor` bigint(20) NOT NULL,
 `petroleum_contract_exempt_taxable_minor` bigint(20) NOT NULL,
 `resolution_exempt_income_minor` bigint(20) NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `fact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `captured_by` bigint(20) unsigned NOT NULL,
 `captured_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`tt80_person_fact_id`),
 UNIQUE KEY `uq_pfl1_pf_sha` (`fact_sha256`),
 UNIQUE KEY `uq_pfl1_pf_set_sub_sha` (`filing_source_set_id`,`subject_key`,`fact_sha256`),
 KEY `ix_pfl1_pf_set_sub` (`filing_source_set_id`,`subject_key`),
 KEY `fk_pfl1_pf_set` (`filing_source_set_id`),
 CONSTRAINT `fk_pfl1_pf_set` FOREIGN KEY (`filing_source_set_id`) REFERENCES `0_hrm_pay_fil_001_source_sets` (`filing_source_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_tt80_person_fact_links` (
 `tt80_person_fact_link_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `tt80_person_fact_id` bigint(20) unsigned NOT NULL,
 `filing_source_link_id` bigint(20) unsigned NOT NULL,
 `source_link_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `binding_role` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`tt80_person_fact_link_id`),
 UNIQUE KEY `uq_pfl1_pl_sha` (`binding_sha256`),
 UNIQUE KEY `uq_pfl1_pl_fact_link_role` (`tt80_person_fact_id`,`filing_source_link_id`,`binding_role`),
 KEY `fk_pfl1_pl_fact` (`tt80_person_fact_id`),
 KEY `fk_pfl1_pl_link` (`filing_source_link_id`),
 CONSTRAINT `fk_pfl1_pl_fact` FOREIGN KEY (`tt80_person_fact_id`) REFERENCES `0_hrm_pay_fil_001_tt80_person_facts` (`tt80_person_fact_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_pl_link` FOREIGN KEY (`filing_source_link_id`) REFERENCES `0_hrm_pay_fil_001_source_links` (`filing_source_link_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_tt80_semantic_reviews` (
 `tt80_semantic_review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `filing_obligation_id` bigint(20) unsigned NOT NULL,
 `filing_source_set_id` bigint(20) unsigned NOT NULL,
 `tt80_taxpayer_profile_id` bigint(20) unsigned NOT NULL,
 `taxpayer_profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `expected_subject_count` int(10) unsigned NOT NULL,
 `expected_subject_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `subject_roster_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `person_fact_count` int(10) unsigned NOT NULL,
 `person_fact_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `person_fact_link_count` int(10) unsigned NOT NULL,
 `person_fact_link_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_evidence_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `maker_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `completeness_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `findings_json` varchar(1024) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `findings_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `reviewed_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`tt80_semantic_review_id`),
 UNIQUE KEY `uq_pfl1_sr_sha` (`review_sha256`),
 KEY `ix_pfl1_sr_set_status` (`filing_source_set_id`,`completeness_status`),
 KEY `fk_pfl1_sr_ob` (`filing_obligation_id`),
 KEY `fk_pfl1_sr_set` (`filing_source_set_id`),
 KEY `fk_pfl1_sr_tp` (`tt80_taxpayer_profile_id`),
 CONSTRAINT `fk_pfl1_sr_ob` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_sr_set` FOREIGN KEY (`filing_source_set_id`) REFERENCES `0_hrm_pay_fil_001_source_sets` (`filing_source_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_sr_tp` FOREIGN KEY (`tt80_taxpayer_profile_id`) REFERENCES `0_hrm_pay_fil_001_tt80_taxpayer_profiles` (`tt80_taxpayer_profile_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_fil_001_tt80_semantic_mappings` (
 `tt80_semantic_mapping_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `tt80_semantic_review_id` bigint(20) unsigned NOT NULL,
 `filing_obligation_id` bigint(20) unsigned NOT NULL,
 `filing_source_set_id` bigint(20) unsigned NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `mapping_contract_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `taxpayer_profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `person_fact_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `indicator_values_json` varchar(2048) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `indicator_values_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `mapping_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `published_by` bigint(20) unsigned NOT NULL,
 `published_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`tt80_semantic_mapping_id`),
 UNIQUE KEY `uq_pfl1_sm_sha` (`mapping_sha256`),
 UNIQUE KEY `uq_pfl1_sm_review` (`tt80_semantic_review_id`),
 KEY `ix_pfl1_sm_set` (`filing_source_set_id`,`tt80_semantic_mapping_id`),
 KEY `fk_pfl1_sm_ob` (`filing_obligation_id`),
 CONSTRAINT `fk_pfl1_sm_review` FOREIGN KEY (`tt80_semantic_review_id`) REFERENCES `0_hrm_pay_fil_001_tt80_semantic_reviews` (`tt80_semantic_review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_sm_ob` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pfl1_sm_set` FOREIGN KEY (`filing_source_set_id`) REFERENCES `0_hrm_pay_fil_001_source_sets` (`filing_source_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_resources` (
 `api_resource_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `resource_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `contract_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `resource_kind` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `company_scope_required` tinyint(1) unsigned NOT NULL, `object_scope_required` tinyint(1) unsigned NOT NULL, `read_capable` tinyint(1) unsigned NOT NULL, `command_capable` tinyint(1) unsigned NOT NULL, `resource_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `reviewed_by` bigint(20) unsigned NOT NULL, `approved_by` bigint(20) unsigned NOT NULL, `approved_at` datetime NOT NULL, `created_at` datetime NOT NULL, PRIMARY KEY (`api_resource_id`), UNIQUE KEY `uq_papi1_rs_key_ver` (`resource_key`,`contract_version`), UNIQUE KEY `uq_papi1_rs_sha` (`resource_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_machine_principals` (
 `api_principal_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `company_id` int(11) unsigned NOT NULL, `principal_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `display_name` varchar(160) NOT NULL, `purpose_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `principal_status` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `principal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `reviewed_by` bigint(20) unsigned NOT NULL, `approved_by` bigint(20) unsigned NOT NULL, `approved_at` datetime NOT NULL, `created_at` datetime NOT NULL, PRIMARY KEY (`api_principal_id`), UNIQUE KEY `uq_papi1_mp_key` (`company_id`,`principal_key`), UNIQUE KEY `uq_papi1_mp_sha` (`principal_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_principal_scopes` (
 `api_principal_scope_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT, `api_principal_id` bigint(20) unsigned NOT NULL, `api_resource_id` bigint(20) unsigned NOT NULL, `action_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `object_scope_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL, `object_scope_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL, `scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `reviewed_by` bigint(20) unsigned NOT NULL, `approved_by` bigint(20) unsigned NOT NULL, `approved_at` datetime NOT NULL, `created_at` datetime NOT NULL, PRIMARY KEY (`api_principal_scope_id`), UNIQUE KEY `uq_papi1_ps_sha` (`scope_sha256`), UNIQUE KEY `uq_papi1_ps_scope` (`api_principal_id`,`api_resource_id`,`action_code`,`object_scope_type`,`object_scope_key`), KEY `fk_papi1_ps_principal` (`api_principal_id`), KEY `fk_papi1_ps_resource` (`api_resource_id`), CONSTRAINT `fk_papi1_ps_principal` FOREIGN KEY (`api_principal_id`) REFERENCES `0_hrm_pay_api_001_machine_principals` (`api_principal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT, CONSTRAINT `fk_papi1_ps_resource` FOREIGN KEY (`api_resource_id`) REFERENCES `0_hrm_pay_api_001_resources` (`api_resource_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_credentials` (
 `api_credential_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `api_principal_id` bigint(20) unsigned NOT NULL,
 `credential_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `verifier_scheme` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `secret_verifier_hash` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `valid_from` datetime NOT NULL, `expires_at` datetime NOT NULL,
 `credential_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL, `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`api_credential_id`), UNIQUE KEY `uq_papi1_cr_key` (`credential_key`), UNIQUE KEY `uq_papi1_cr_sha` (`credential_sha256`),
 KEY `fk_papi1_cr_principal` (`api_principal_id`),
 CONSTRAINT `fk_papi1_cr_principal` FOREIGN KEY (`api_principal_id`) REFERENCES `0_hrm_pay_api_001_machine_principals` (`api_principal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_credential_revocations` (
 `credential_revocation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `api_credential_id` bigint(20) unsigned NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `revocation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `revoked_by` bigint(20) unsigned NOT NULL, `revoked_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`credential_revocation_id`), UNIQUE KEY `uq_papi1_cv_credential` (`api_credential_id`), UNIQUE KEY `uq_papi1_cv_sha` (`revocation_sha256`),
 KEY `fk_papi1_cv_credential` (`api_credential_id`),
 CONSTRAINT `fk_papi1_cv_credential` FOREIGN KEY (`api_credential_id`) REFERENCES `0_hrm_pay_api_001_credentials` (`api_credential_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_authn_decisions` (
 `api_authn_decision_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `api_principal_id` bigint(20) unsigned DEFAULT NULL,
 `api_credential_id` bigint(20) unsigned DEFAULT NULL,
 `credential_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `outcome_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `correlation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `decision_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `occurred_at` datetime NOT NULL, `created_at` datetime NOT NULL,
 PRIMARY KEY (`api_authn_decision_id`), UNIQUE KEY `uq_papi1_ad_sha` (`decision_sha256`),
 UNIQUE KEY `uq_papi1_ad_corr` (`company_id`,`correlation_sha256`),
 KEY `ix_papi1_ad_company_time` (`company_id`,`occurred_at`),
 KEY `fk_papi1_ad_principal` (`api_principal_id`), KEY `fk_papi1_ad_credential` (`api_credential_id`),
 CONSTRAINT `fk_papi1_ad_principal` FOREIGN KEY (`api_principal_id`) REFERENCES `0_hrm_pay_api_001_machine_principals` (`api_principal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_ad_credential` FOREIGN KEY (`api_credential_id`) REFERENCES `0_hrm_pay_api_001_credentials` (`api_credential_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_provision_requests` (
 `provision_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `target_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `target_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `target_payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_by` bigint(20) unsigned NOT NULL,
 `requested_at` datetime NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`provision_request_id`),
 UNIQUE KEY `uq_papi1_pr_sha` (`request_sha256`),
 KEY `ix_papi1_pr_company_kind` (`company_id`,`target_kind`,`requested_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_provision_reviews` (
 `provision_review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `provision_request_id` bigint(20) unsigned NOT NULL,
 `decision_code` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `reviewed_at` datetime NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`provision_review_id`),
 UNIQUE KEY `uq_papi1_pv_request` (`provision_request_id`),
 UNIQUE KEY `uq_papi1_pv_sha` (`review_sha256`),
 CONSTRAINT `fk_papi1_pv_request` FOREIGN KEY (`provision_request_id`) REFERENCES `0_hrm_pay_api_001_provision_requests` (`provision_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_provision_approvals` (
 `provision_approval_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `provision_request_id` bigint(20) unsigned NOT NULL,
 `provision_review_id` bigint(20) unsigned NOT NULL,
 `decision_code` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`provision_approval_id`),
 UNIQUE KEY `uq_papi1_pa_request` (`provision_request_id`),
 UNIQUE KEY `uq_papi1_pa_review` (`provision_review_id`),
 UNIQUE KEY `uq_papi1_pa_sha` (`approval_sha256`),
 CONSTRAINT `fk_papi1_pa_request` FOREIGN KEY (`provision_request_id`) REFERENCES `0_hrm_pay_api_001_provision_requests` (`provision_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_pa_review` FOREIGN KEY (`provision_review_id`) REFERENCES `0_hrm_pay_api_001_provision_reviews` (`provision_review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_rate_limit_policies` (
 `api_rate_limit_policy_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `api_resource_id` bigint(20) unsigned NOT NULL,
 `action_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `window_seconds` int(10) unsigned NOT NULL,
 `max_requests` int(10) unsigned NOT NULL,
 `policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`api_rate_limit_policy_id`),
 UNIQUE KEY `uq_papi1_rlp_resource_action` (`api_resource_id`,`action_code`),
 UNIQUE KEY `uq_papi1_rlp_sha` (`policy_sha256`),
 CONSTRAINT `fk_papi1_rlp_resource` FOREIGN KEY (`api_resource_id`) REFERENCES `0_hrm_pay_api_001_resources` (`api_resource_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_rate_limit_decisions` (
 `api_rate_limit_decision_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `api_principal_id` bigint(20) unsigned NOT NULL,
 `api_resource_id` bigint(20) unsigned NOT NULL,
 `api_rate_limit_policy_id` bigint(20) unsigned NOT NULL,
 `window_start` datetime NOT NULL,
 `request_ordinal` int(10) unsigned NOT NULL,
 `outcome_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `correlation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `decision_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `occurred_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`api_rate_limit_decision_id`),
 UNIQUE KEY `uq_papi1_rld_sha` (`decision_sha256`),
 UNIQUE KEY `uq_papi1_rld_ordinal` (`api_rate_limit_policy_id`,`api_principal_id`,`window_start`,`request_ordinal`),
 KEY `ix_papi1_rld_company_time` (`company_id`,`occurred_at`),
 CONSTRAINT `fk_papi1_rld_principal` FOREIGN KEY (`api_principal_id`) REFERENCES `0_hrm_pay_api_001_machine_principals` (`api_principal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_rld_resource` FOREIGN KEY (`api_resource_id`) REFERENCES `0_hrm_pay_api_001_resources` (`api_resource_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_rld_policy` FOREIGN KEY (`api_rate_limit_policy_id`) REFERENCES `0_hrm_pay_api_001_rate_limit_policies` (`api_rate_limit_policy_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_provision_executions` (
 `provision_execution_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `provision_request_id` bigint(20) unsigned NOT NULL,
 `provision_approval_id` bigint(20) unsigned NOT NULL,
 `target_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `target_record_id` bigint(20) unsigned NOT NULL,
 `target_payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `executor_id` bigint(20) unsigned NOT NULL,
 `execution_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `executed_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`provision_execution_id`),
 UNIQUE KEY `uq_papi1_pe_request` (`provision_request_id`),
 UNIQUE KEY `uq_papi1_pe_approval` (`provision_approval_id`),
 UNIQUE KEY `uq_papi1_pe_sha` (`execution_sha256`),
 CONSTRAINT `fk_papi1_pe_request` FOREIGN KEY (`provision_request_id`) REFERENCES `0_hrm_pay_api_001_provision_requests` (`provision_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_pe_approval` FOREIGN KEY (`provision_approval_id`) REFERENCES `0_hrm_pay_api_001_provision_approvals` (`provision_approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_read_requests` (
 `api_read_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `api_authn_decision_id` bigint(20) unsigned DEFAULT NULL,
 `api_principal_id` bigint(20) unsigned DEFAULT NULL,
 `api_resource_id` bigint(20) unsigned DEFAULT NULL,
 `api_rate_limit_decision_id` bigint(20) unsigned DEFAULT NULL,
 `method` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `resource_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_target_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `outcome_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `correlation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `occurred_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`api_read_request_id`),
 UNIQUE KEY `uq_papi1_rr_corr` (`company_id`,`correlation_sha256`),
 UNIQUE KEY `uq_papi1_rr_sha` (`request_sha256`),
 KEY `fk_papi1_rr_authn` (`api_authn_decision_id`),
 KEY `fk_papi1_rr_principal` (`api_principal_id`),
 KEY `fk_papi1_rr_resource` (`api_resource_id`),
 KEY `fk_papi1_rr_rate` (`api_rate_limit_decision_id`),
 CONSTRAINT `fk_papi1_rr_authn` FOREIGN KEY (`api_authn_decision_id`) REFERENCES `0_hrm_pay_api_001_authn_decisions` (`api_authn_decision_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_rr_principal` FOREIGN KEY (`api_principal_id`) REFERENCES `0_hrm_pay_api_001_machine_principals` (`api_principal_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_rr_resource` FOREIGN KEY (`api_resource_id`) REFERENCES `0_hrm_pay_api_001_resources` (`api_resource_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_rr_rate` FOREIGN KEY (`api_rate_limit_decision_id`) REFERENCES `0_hrm_pay_api_001_rate_limit_decisions` (`api_rate_limit_decision_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_read_responses` (
 `api_read_response_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `api_read_request_id` bigint(20) unsigned NOT NULL,
 `company_id` int(11) unsigned NOT NULL,
 `resource_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `outcome_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `row_count` int(11) unsigned NOT NULL,
 `response_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `occurred_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`api_read_response_id`),
 UNIQUE KEY `uq_papi1_rres_request` (`api_read_request_id`),
 UNIQUE KEY `uq_papi1_rres_sha` (`response_sha256`),
 CONSTRAINT `fk_papi1_rres_request` FOREIGN KEY (`api_read_request_id`) REFERENCES `0_hrm_pay_api_001_read_requests` (`api_read_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_targets` (
 `webhook_target_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `target_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `event_type` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `contract_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `resource_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `endpoint_config_ref_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `signing_key_ref_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `target_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_target_id`),
 UNIQUE KEY `uq_papi1_wt_key` (`company_id`,`target_key`),
 UNIQUE KEY `uq_papi1_wt_sha` (`target_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_events` (
 `webhook_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `event_type` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `contract_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `resource_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_type` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_json` mediumtext NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `dedupe_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `occurred_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_event_id`),
 UNIQUE KEY `uq_papi1_we_dedupe` (`company_id`,`dedupe_key_sha256`),
 UNIQUE KEY `uq_papi1_we_sha` (`event_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_outbox` (
 `webhook_outbox_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_event_id` bigint(20) unsigned NOT NULL,
 `webhook_target_id` bigint(20) unsigned NOT NULL,
 `queue_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `outbox_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `queued_by` bigint(20) unsigned NOT NULL,
 `queued_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_outbox_id`),
 UNIQUE KEY `uq_papi1_wo_pair` (`webhook_event_id`,`webhook_target_id`),
 UNIQUE KEY `uq_papi1_wo_queue` (`queue_key_sha256`),
 UNIQUE KEY `uq_papi1_wo_sha` (`outbox_sha256`),
 CONSTRAINT `fk_papi1_wo_event` FOREIGN KEY (`webhook_event_id`) REFERENCES `0_hrm_pay_api_001_webhook_events` (`webhook_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wo_target` FOREIGN KEY (`webhook_target_id`) REFERENCES `0_hrm_pay_api_001_webhook_targets` (`webhook_target_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_delivery_attempts` (
 `webhook_delivery_attempt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_outbox_id` bigint(20) unsigned NOT NULL,
 `attempt_no` int(10) unsigned NOT NULL,
 `outcome_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `http_status_code` int(10) unsigned DEFAULT NULL,
 `provider_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `response_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `error_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `attempt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `attempted_by` bigint(20) unsigned NOT NULL,
 `attempted_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_delivery_attempt_id`),
 UNIQUE KEY `uq_papi1_wa_attempt` (`webhook_outbox_id`,`attempt_no`),
 UNIQUE KEY `uq_papi1_wa_lineage` (`webhook_outbox_id`,`webhook_delivery_attempt_id`),
 UNIQUE KEY `uq_papi1_wa_sha` (`attempt_sha256`),
 CONSTRAINT `fk_papi1_wa_outbox` FOREIGN KEY (`webhook_outbox_id`) REFERENCES `0_hrm_pay_api_001_webhook_outbox` (`webhook_outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_dead_letters` (
 `webhook_dead_letter_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_outbox_id` bigint(20) unsigned NOT NULL,
 `last_attempt_id` bigint(20) unsigned NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `dead_letter_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by` bigint(20) unsigned NOT NULL,
 `recorded_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_dead_letter_id`),
 UNIQUE KEY `uq_papi1_wd_outbox` (`webhook_outbox_id`),
 UNIQUE KEY `uq_papi1_wd_lineage` (`webhook_dead_letter_id`,`webhook_outbox_id`,`last_attempt_id`),
 UNIQUE KEY `uq_papi1_wd_sha` (`dead_letter_sha256`),
 CONSTRAINT `fk_papi1_wd_outbox` FOREIGN KEY (`webhook_outbox_id`) REFERENCES `0_hrm_pay_api_001_webhook_outbox` (`webhook_outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wd_attempt` FOREIGN KEY (`webhook_outbox_id`,`last_attempt_id`) REFERENCES `0_hrm_pay_api_001_webhook_delivery_attempts` (`webhook_outbox_id`,`webhook_delivery_attempt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_target_lifecycle` (
 `webhook_target_lifecycle_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `webhook_target_id` bigint(20) unsigned NOT NULL,
 `action_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `successor_target_id` bigint(20) unsigned DEFAULT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `lifecycle_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `recorded_by` bigint(20) unsigned NOT NULL,
 `effective_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_target_lifecycle_id`),
 UNIQUE KEY `uq_papi1_wtl_event` (`webhook_target_id`,`effective_at`,`action_code`),
 UNIQUE KEY `uq_papi1_wtl_sha` (`lifecycle_sha256`),
 CONSTRAINT `fk_papi1_wtl_target` FOREIGN KEY (`webhook_target_id`) REFERENCES `0_hrm_pay_api_001_webhook_targets` (`webhook_target_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wtl_successor` FOREIGN KEY (`successor_target_id`) REFERENCES `0_hrm_pay_api_001_webhook_targets` (`webhook_target_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_secret_refs` (
 `webhook_secret_ref_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `ref_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_key` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `secret_ref_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `valid_from` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_secret_ref_id`),
 UNIQUE KEY `uq_papi1_wsr_ref` (`company_id`,`ref_kind`,`secret_ref_sha256`),
 UNIQUE KEY `uq_papi1_wsr_sha` (`secret_ref_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_secret_rotations` (
 `webhook_secret_rotation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `ref_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `previous_secret_ref_id` bigint(20) unsigned NOT NULL,
 `successor_secret_ref_id` bigint(20) unsigned NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `rotation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `effective_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_secret_rotation_id`),
 UNIQUE KEY `uq_papi1_wsrn_successor` (`successor_secret_ref_id`),
 UNIQUE KEY `uq_papi1_wsrn_sha` (`rotation_sha256`),
 CONSTRAINT `fk_papi1_wsrn_previous` FOREIGN KEY (`previous_secret_ref_id`) REFERENCES `0_hrm_pay_api_001_webhook_secret_refs` (`webhook_secret_ref_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wsrn_successor` FOREIGN KEY (`successor_secret_ref_id`) REFERENCES `0_hrm_pay_api_001_webhook_secret_refs` (`webhook_secret_ref_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_delivery_policies` (
 `webhook_delivery_policy_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `webhook_target_id` bigint(20) unsigned NOT NULL,
 `policy_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `max_attempts` smallint(5) unsigned NOT NULL,
 `initial_backoff_seconds` int(10) unsigned NOT NULL,
 `max_backoff_seconds` int(10) unsigned NOT NULL,
 `lease_seconds` int(10) unsigned NOT NULL,
 `policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_delivery_policy_id`),
 UNIQUE KEY `uq_papi1_wdp_version` (`webhook_target_id`,`policy_version`),
 UNIQUE KEY `uq_papi1_wdp_sha` (`policy_sha256`),
 CONSTRAINT `fk_papi1_wdp_target` FOREIGN KEY (`webhook_target_id`) REFERENCES `0_hrm_pay_api_001_webhook_targets` (`webhook_target_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_claim_leases` (
 `webhook_claim_lease_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_outbox_id` bigint(20) unsigned NOT NULL,
 `webhook_delivery_policy_id` bigint(20) unsigned NOT NULL,
 `lease_ordinal` int(10) unsigned NOT NULL,
 `claim_token_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `claimed_by` bigint(20) unsigned NOT NULL,
 `claimed_at` datetime NOT NULL,
 `expires_at` datetime NOT NULL,
 `claim_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_claim_lease_id`),
 UNIQUE KEY `uq_papi1_wcl_ordinal` (`webhook_outbox_id`,`lease_ordinal`),
 UNIQUE KEY `uq_papi1_wcl_token` (`claim_token_sha256`),
 UNIQUE KEY `uq_papi1_wcl_sha` (`claim_sha256`),
 CONSTRAINT `fk_papi1_wcl_outbox` FOREIGN KEY (`webhook_outbox_id`) REFERENCES `0_hrm_pay_api_001_webhook_outbox` (`webhook_outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wcl_policy` FOREIGN KEY (`webhook_delivery_policy_id`) REFERENCES `0_hrm_pay_api_001_webhook_delivery_policies` (`webhook_delivery_policy_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_egress_tls_policies` (
 `webhook_egress_tls_policy_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `webhook_target_id` bigint(20) unsigned NOT NULL,
 `policy_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `destination_host` varchar(253) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `destination_port` smallint(5) unsigned NOT NULL,
 `scheme_code` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `minimum_tls_version` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `verify_peer_required` tinyint(1) unsigned NOT NULL,
 `verify_hostname_required` tinyint(1) unsigned NOT NULL,
 `policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_egress_tls_policy_id`),
 UNIQUE KEY `uq_papi1_wetp_version` (`webhook_target_id`,`policy_version`),
 UNIQUE KEY `uq_papi1_wetp_sha` (`policy_sha256`),
 CONSTRAINT `fk_papi1_wetp_target` FOREIGN KEY (`webhook_target_id`) REFERENCES `0_hrm_pay_api_001_webhook_targets` (`webhook_target_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_signing_profiles` (
 `webhook_signing_profile_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `webhook_target_id` bigint(20) unsigned NOT NULL,
 `webhook_secret_ref_id` bigint(20) unsigned NOT NULL,
 `profile_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `canonicalization_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `signature_algorithm` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `signature_header_name` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `timestamp_header_name` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_signing_profile_id`),
 UNIQUE KEY `uq_papi1_wsp_version` (`webhook_target_id`,`profile_version`),
 UNIQUE KEY `uq_papi1_wsp_sha` (`profile_sha256`),
 CONSTRAINT `fk_papi1_wsp_target` FOREIGN KEY (`webhook_target_id`) REFERENCES `0_hrm_pay_api_001_webhook_targets` (`webhook_target_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wsp_secret` FOREIGN KEY (`webhook_secret_ref_id`) REFERENCES `0_hrm_pay_api_001_webhook_secret_refs` (`webhook_secret_ref_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_provider_responses` (
 `webhook_provider_response_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_delivery_attempt_id` bigint(20) unsigned NOT NULL,
 `http_status_code` smallint(5) unsigned NOT NULL,
 `provider_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `response_headers_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `response_body_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `response_body_bytes` int(10) unsigned NOT NULL,
 `response_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_provider_response_id`),
 UNIQUE KEY `uq_papi1_wpr_attempt` (`webhook_delivery_attempt_id`),
 UNIQUE KEY `uq_papi1_wpr_sha` (`response_sha256`),
 CONSTRAINT `fk_papi1_wpr_attempt` FOREIGN KEY (`webhook_delivery_attempt_id`) REFERENCES `0_hrm_pay_api_001_webhook_delivery_attempts` (`webhook_delivery_attempt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_replay_defenses` (
 `webhook_replay_defense_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_outbox_id` bigint(20) unsigned NOT NULL,
 `replay_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `valid_until` datetime NOT NULL,
 `defense_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by` bigint(20) unsigned NOT NULL,
 `recorded_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_replay_defense_id`),
 UNIQUE KEY `uq_papi1_wrd_outbox` (`webhook_outbox_id`),
 UNIQUE KEY `uq_papi1_wrd_key` (`replay_key_sha256`),
 UNIQUE KEY `uq_papi1_wrd_sha` (`defense_sha256`),
 CONSTRAINT `fk_papi1_wrd_outbox` FOREIGN KEY (`webhook_outbox_id`) REFERENCES `0_hrm_pay_api_001_webhook_outbox` (`webhook_outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_reprocess_authorizations` (
 `webhook_reprocess_authorization_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_dead_letter_id` bigint(20) unsigned NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_by` bigint(20) unsigned NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `authorization_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_reprocess_authorization_id`),
 UNIQUE KEY `uq_papi1_wra_deadletter` (`webhook_dead_letter_id`),
 UNIQUE KEY `uq_papi1_wra_lineage` (`webhook_reprocess_authorization_id`,`webhook_dead_letter_id`),
 UNIQUE KEY `uq_papi1_wra_request` (`request_sha256`),
 UNIQUE KEY `uq_papi1_wra_sha` (`authorization_sha256`),
 CONSTRAINT `fk_papi1_wra_deadletter` FOREIGN KEY (`webhook_dead_letter_id`) REFERENCES `0_hrm_pay_api_001_webhook_dead_letters` (`webhook_dead_letter_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_retry_dispatch_reservations` (
 `webhook_retry_dispatch_reservation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_outbox_id` bigint(20) unsigned NOT NULL,
 `previous_attempt_id` bigint(20) unsigned NOT NULL,
 `webhook_claim_lease_id` bigint(20) unsigned NOT NULL,
 `attempt_no` int(10) unsigned NOT NULL,
 `eligible_at` datetime NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `replay_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reservation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reserved_by` bigint(20) unsigned NOT NULL,
 `reserved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_retry_dispatch_reservation_id`),
 UNIQUE KEY `uq_papi1_wrr_attempt` (`webhook_outbox_id`,`attempt_no`),
 UNIQUE KEY `uq_papi1_wrr_claim` (`webhook_claim_lease_id`),
 UNIQUE KEY `uq_papi1_wrr_replay` (`replay_key_sha256`),
 UNIQUE KEY `uq_papi1_wrr_sha` (`reservation_sha256`),
 CONSTRAINT `fk_papi1_wrr_outbox` FOREIGN KEY (`webhook_outbox_id`) REFERENCES `0_hrm_pay_api_001_webhook_outbox` (`webhook_outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wrr_attempt` FOREIGN KEY (`webhook_outbox_id`,`previous_attempt_id`) REFERENCES `0_hrm_pay_api_001_webhook_delivery_attempts` (`webhook_outbox_id`,`webhook_delivery_attempt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wrr_claim` FOREIGN KEY (`webhook_claim_lease_id`) REFERENCES `0_hrm_pay_api_001_webhook_claim_leases` (`webhook_claim_lease_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_reprocess_requests` (
 `webhook_reprocess_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_dead_letter_id` bigint(20) unsigned NOT NULL,
 `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_by` bigint(20) unsigned NOT NULL,
 `requested_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_reprocess_request_id`),
 UNIQUE KEY `uq_papi1_wrq_sha` (`request_sha256`),
 KEY `ix_papi1_wrq_deadletter` (`webhook_dead_letter_id`),
 CONSTRAINT `fk_papi1_wrq_deadletter` FOREIGN KEY (`webhook_dead_letter_id`) REFERENCES `0_hrm_pay_api_001_webhook_dead_letters` (`webhook_dead_letter_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_reprocess_reviews` (
 `webhook_reprocess_review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_reprocess_request_id` bigint(20) unsigned NOT NULL,
 `decision_code` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `reviewed_at` datetime NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_reprocess_review_id`),
 UNIQUE KEY `uq_papi1_wrv_request` (`webhook_reprocess_request_id`),
 UNIQUE KEY `uq_papi1_wrv_pair` (`webhook_reprocess_request_id`,`webhook_reprocess_review_id`),
 UNIQUE KEY `uq_papi1_wrv_sha` (`review_sha256`),
 CONSTRAINT `fk_papi1_wrv_request` FOREIGN KEY (`webhook_reprocess_request_id`) REFERENCES `0_hrm_pay_api_001_webhook_reprocess_requests` (`webhook_reprocess_request_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_reprocess_approvals` (
 `webhook_reprocess_approval_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_reprocess_request_id` bigint(20) unsigned NOT NULL,
 `webhook_reprocess_review_id` bigint(20) unsigned NOT NULL,
 `decision_code` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_reprocess_approval_id`),
 UNIQUE KEY `uq_papi1_wrp_request` (`webhook_reprocess_request_id`),
 UNIQUE KEY `uq_papi1_wrp_review` (`webhook_reprocess_review_id`),
 UNIQUE KEY `uq_papi1_wrp_sha` (`approval_sha256`),
 CONSTRAINT `fk_papi1_wrp_review` FOREIGN KEY (`webhook_reprocess_request_id`,`webhook_reprocess_review_id`) REFERENCES `0_hrm_pay_api_001_webhook_reprocess_reviews` (`webhook_reprocess_request_id`,`webhook_reprocess_review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_reprocess_dispatch_reservations` (
 `webhook_reprocess_dispatch_reservation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_reprocess_authorization_id` bigint(20) unsigned NOT NULL,
 `webhook_dead_letter_id` bigint(20) unsigned NOT NULL,
 `webhook_outbox_id` bigint(20) unsigned NOT NULL,
 `previous_attempt_id` bigint(20) unsigned NOT NULL,
 `dispatch_request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `replay_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reservation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reserved_by` bigint(20) unsigned NOT NULL,
 `reserved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_reprocess_dispatch_reservation_id`),
 UNIQUE KEY `uq_papi1_wdr_auth` (`webhook_reprocess_authorization_id`),
 UNIQUE KEY `uq_papi1_wdr_deadletter` (`webhook_dead_letter_id`),
 UNIQUE KEY `uq_papi1_wdr_replay` (`replay_key_sha256`),
 UNIQUE KEY `uq_papi1_wdr_sha` (`reservation_sha256`),
 CONSTRAINT `fk_papi1_wdr_auth` FOREIGN KEY (`webhook_reprocess_authorization_id`,`webhook_dead_letter_id`) REFERENCES `0_hrm_pay_api_001_webhook_reprocess_authorizations` (`webhook_reprocess_authorization_id`,`webhook_dead_letter_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wdr_deadletter` FOREIGN KEY (`webhook_dead_letter_id`,`webhook_outbox_id`,`previous_attempt_id`) REFERENCES `0_hrm_pay_api_001_webhook_dead_letters` (`webhook_dead_letter_id`,`webhook_outbox_id`,`last_attempt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wdr_outbox` FOREIGN KEY (`webhook_outbox_id`) REFERENCES `0_hrm_pay_api_001_webhook_outbox` (`webhook_outbox_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_papi1_wdr_attempt` FOREIGN KEY (`webhook_outbox_id`,`previous_attempt_id`) REFERENCES `0_hrm_pay_api_001_webhook_delivery_attempts` (`webhook_outbox_id`,`webhook_delivery_attempt_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_inbound_sources` (
 `webhook_inbound_source_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `source_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `profile_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `event_type` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `webhook_secret_ref_id` bigint(20) unsigned NOT NULL,
 `canonicalization_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `signature_algorithm` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `signature_header_name` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `timestamp_header_name` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `max_clock_skew_seconds` int(10) unsigned NOT NULL,
 `replay_ttl_seconds` int(10) unsigned NOT NULL,
 `valid_from` datetime NOT NULL,
 `expires_at` datetime NOT NULL,
 `source_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_inbound_source_id`),
 UNIQUE KEY `uq_papi1_wis_keyver` (`company_id`,`source_key`,`profile_version`),
 UNIQUE KEY `uq_papi1_wis_sha` (`source_sha256`),
 CONSTRAINT `fk_papi1_wis_secret` FOREIGN KEY (`webhook_secret_ref_id`) REFERENCES `0_hrm_pay_api_001_webhook_secret_refs` (`webhook_secret_ref_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_inbound_receipts` (
 `webhook_inbound_receipt_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_inbound_source_id` bigint(20) unsigned NOT NULL,
 `company_id` int(11) NOT NULL,
 `delivery_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_target_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `event_type_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `signature_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `timestamp_value` varchar(20) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `verification_outcome_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `replay_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `correlation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `receipt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `received_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_inbound_receipt_id`),
 UNIQUE KEY `uq_papi1_wir_lineage` (`webhook_inbound_receipt_id`,`webhook_inbound_source_id`),
 UNIQUE KEY `uq_papi1_wir_corr` (`company_id`,`correlation_sha256`),
 UNIQUE KEY `uq_papi1_wir_sha` (`receipt_sha256`),
 CONSTRAINT `fk_papi1_wir_source` FOREIGN KEY (`webhook_inbound_source_id`) REFERENCES `0_hrm_pay_api_001_webhook_inbound_sources` (`webhook_inbound_source_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_api_001_webhook_inbound_replay_defenses` (
 `webhook_inbound_replay_defense_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `webhook_inbound_source_id` bigint(20) unsigned NOT NULL,
 `webhook_inbound_receipt_id` bigint(20) unsigned NOT NULL,
 `replay_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `valid_until` datetime NOT NULL,
 `defense_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`webhook_inbound_replay_defense_id`),
 UNIQUE KEY `uq_papi1_wid_receipt` (`webhook_inbound_receipt_id`),
 UNIQUE KEY `uq_papi1_wid_replay` (`webhook_inbound_source_id`,`replay_key_sha256`),
 UNIQUE KEY `uq_papi1_wid_sha` (`defense_sha256`),
 CONSTRAINT `fk_papi1_wid_receipt` FOREIGN KEY (`webhook_inbound_receipt_id`,`webhook_inbound_source_id`) REFERENCES `0_hrm_pay_api_001_webhook_inbound_receipts` (`webhook_inbound_receipt_id`,`webhook_inbound_source_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_anl_001_metric_definitions` (
 `metric_definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `metric_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `definition_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_model_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `value_type_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `unit_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `currency_semantics_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `as_of_semantics_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `aggregation_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_contract_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `field_policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `definition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `valid_from` datetime NOT NULL,
 `expires_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`metric_definition_id`),
 UNIQUE KEY `uq_hanl1_md_lineage` (`metric_definition_id`,`company_id`),
 UNIQUE KEY `uq_hanl1_md_keyver` (`company_id`,`metric_key`,`definition_version`),
 UNIQUE KEY `uq_hanl1_md_sha` (`definition_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_anl_001_suppression_policies` (
 `suppression_policy_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `policy_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `policy_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `minimum_cohort_size` int(10) unsigned NOT NULL,
 `suppress_exact_counts` tinyint(1) NOT NULL,
 `export_allowed` tinyint(1) NOT NULL,
 `policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `valid_from` datetime NOT NULL,
 `expires_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`suppression_policy_id`),
 UNIQUE KEY `uq_hanl1_sp_lineage` (`suppression_policy_id`,`company_id`),
 UNIQUE KEY `uq_hanl1_sp_keyver` (`company_id`,`policy_key`,`policy_version`),
 UNIQUE KEY `uq_hanl1_sp_sha` (`policy_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_anl_001_snapshot_requests` (
 `snapshot_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `metric_definition_id` bigint(20) unsigned NOT NULL,
 `suppression_policy_id` bigint(20) unsigned NOT NULL,
 `as_of_utc` datetime NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `filter_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `field_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_watermark_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_by` bigint(20) unsigned NOT NULL,
 `purpose_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`snapshot_request_id`),
 UNIQUE KEY `uq_hanl1_sr_lineage` (`snapshot_request_id`,`company_id`),
 UNIQUE KEY `uq_hanl1_sr_sha` (`request_sha256`),
 KEY `ix_hanl1_sr_metric_asof` (`company_id`,`metric_definition_id`,`as_of_utc`),
 CONSTRAINT `fk_hanl1_sr_metric` FOREIGN KEY (`metric_definition_id`,`company_id`) REFERENCES `0_hrm_anl_001_metric_definitions` (`metric_definition_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_hanl1_sr_policy` FOREIGN KEY (`suppression_policy_id`,`company_id`) REFERENCES `0_hrm_anl_001_suppression_policies` (`suppression_policy_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_anl_001_snapshots` (
 `snapshot_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `snapshot_request_id` bigint(20) unsigned NOT NULL,
 `row_count` bigint(20) unsigned NOT NULL,
 `suppressed_row_count` bigint(20) unsigned NOT NULL,
 `source_total_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `result_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `status_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `captured_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`snapshot_id`),
 UNIQUE KEY `uq_hanl1_ss_lineage` (`snapshot_id`,`company_id`),
 UNIQUE KEY `uq_hanl1_ss_request` (`snapshot_request_id`),
 UNIQUE KEY `uq_hanl1_ss_sha` (`snapshot_sha256`),
 CONSTRAINT `fk_hanl1_ss_request` FOREIGN KEY (`snapshot_request_id`,`company_id`) REFERENCES `0_hrm_anl_001_snapshot_requests` (`snapshot_request_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_anl_001_query_audits` (
 `query_audit_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `metric_definition_id` bigint(20) unsigned NOT NULL,
 `snapshot_id` bigint(20) unsigned NOT NULL,
 `actor_user_id` bigint(20) unsigned NOT NULL,
 `query_purpose_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `filter_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `field_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `as_of_utc` datetime NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `decision_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `suppression_applied` tinyint(1) NOT NULL,
 `cohort_size_band_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `export_requested` tinyint(1) NOT NULL,
 `export_allowed` tinyint(1) NOT NULL,
 `drilldown_requested` tinyint(1) NOT NULL,
 `query_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `occurred_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`query_audit_id`),
 UNIQUE KEY `uq_hanl1_qa_lineage` (`query_audit_id`,`company_id`),
 UNIQUE KEY `uq_hanl1_qa_sha` (`query_sha256`),
 KEY `ix_hanl1_qa_actor_time` (`company_id`,`actor_user_id`,`occurred_at`),
 CONSTRAINT `fk_hanl1_qa_metric` FOREIGN KEY (`metric_definition_id`,`company_id`) REFERENCES `0_hrm_anl_001_metric_definitions` (`metric_definition_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_hanl1_qa_snapshot` FOREIGN KEY (`snapshot_id`,`company_id`) REFERENCES `0_hrm_anl_001_snapshots` (`snapshot_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_anl_001_drilldown_authorizations` (
 `drilldown_authorization_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `query_audit_id` bigint(20) unsigned NOT NULL,
 `metric_definition_id` bigint(20) unsigned NOT NULL,
 `purpose_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `field_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewer_user_id` bigint(20) unsigned NOT NULL,
 `approver_user_id` bigint(20) unsigned NOT NULL,
 `decision_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `expires_at` datetime NOT NULL,
 `authorization_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `authorized_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`drilldown_authorization_id`),
 UNIQUE KEY `uq_hanl1_da_lineage` (`drilldown_authorization_id`,`company_id`),
 UNIQUE KEY `uq_hanl1_da_sha` (`authorization_sha256`),
 KEY `ix_hanl1_da_query` (`company_id`,`query_audit_id`,`expires_at`),
 CONSTRAINT `fk_hanl1_da_query` FOREIGN KEY (`query_audit_id`,`company_id`) REFERENCES `0_hrm_anl_001_query_audits` (`query_audit_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_hanl1_da_metric` FOREIGN KEY (`metric_definition_id`,`company_id`) REFERENCES `0_hrm_anl_001_metric_definitions` (`metric_definition_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_ai_001_requests` (
 `ai_request_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `capability_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `purpose_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `actor_user_id` bigint(20) unsigned NOT NULL,
 `authorized_input_refs_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `object_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `field_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `data_minimization_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `opt_out_checked` tinyint(1) NOT NULL,
 `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`ai_request_id`),
 UNIQUE KEY `uq_pai1_rq_lineage` (`ai_request_id`,`company_id`),
 UNIQUE KEY `uq_pai1_rq_sha` (`request_sha256`),
 KEY `ix_pai1_rq_actor_time` (`company_id`,`actor_user_id`,`requested_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_ai_001_outputs` (
 `ai_output_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `ai_request_id` bigint(20) unsigned NOT NULL,
 `provider_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `model_version` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `generated_content_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `grounding_refs_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `confidence_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `abstained` tinyint(1) NOT NULL,
 `provider_failure` tinyint(1) NOT NULL,
 `output_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `generated_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`ai_output_id`),
 UNIQUE KEY `uq_pai1_ou_lineage` (`ai_output_id`,`company_id`),
 UNIQUE KEY `uq_pai1_ou_sha` (`output_sha256`),
 KEY `ix_pai1_ou_request` (`company_id`,`ai_request_id`,`generated_at`),
 CONSTRAINT `fk_pai1_ou_request` FOREIGN KEY (`ai_request_id`,`company_id`) REFERENCES `0_hrm_pay_ai_001_requests` (`ai_request_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_ai_001_reviews` (
 `ai_review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `ai_output_id` bigint(20) unsigned NOT NULL,
 `reviewer_user_id` bigint(20) unsigned NOT NULL,
 `disposition_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `false_positive` tinyint(1) NOT NULL,
 `appeal_requested` tinyint(1) NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`ai_review_id`),
 UNIQUE KEY `uq_pai1_rv_lineage` (`ai_review_id`,`company_id`),
 UNIQUE KEY `uq_pai1_rv_sha` (`review_sha256`),
 KEY `ix_pai1_rv_output` (`company_id`,`ai_output_id`,`reviewed_at`),
 CONSTRAINT `fk_pai1_rv_output` FOREIGN KEY (`ai_output_id`,`company_id`) REFERENCES `0_hrm_pay_ai_001_outputs` (`ai_output_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_skill_definitions` (
 `skill_definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `skill_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `skill_name` varchar(140) NOT NULL,
 `category_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `description` text,
 `proficiency_scale_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `effective_from` date NOT NULL,
 `effective_to` date DEFAULT NULL,
 `definition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`skill_definition_id`),
 UNIQUE KEY `uq_htal1_sd_lineage` (`skill_definition_id`,`company_id`),
 UNIQUE KEY `uq_htal1_sd_version` (`company_id`,`skill_code`,`effective_from`),
 UNIQUE KEY `uq_htal1_sd_sha` (`definition_sha256`),
 KEY `ix_htal1_sd_effective` (`company_id`,`skill_code`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_employee_skill_evidence` (
 `employee_skill_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `skill_definition_id` bigint(20) unsigned NOT NULL,
 `employee_id` varchar(20) NOT NULL,
 `evidence_type_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `proficiency_value` decimal(9,4) DEFAULT NULL,
 `evidence_date` date NOT NULL,
 `valid_from` date NOT NULL,
 `valid_to` date DEFAULT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`employee_skill_evidence_id`),
 UNIQUE KEY `uq_htal1_se_lineage` (`employee_skill_evidence_id`,`company_id`),
 UNIQUE KEY `uq_htal1_se_sha` (`evidence_sha256`),
 KEY `ix_htal1_se_employee` (`company_id`,`employee_id`,`valid_from`,`valid_to`),
 KEY `ix_htal1_se_skill` (`company_id`,`skill_definition_id`,`evidence_date`),
 CONSTRAINT `fk_htal1_se_skill` FOREIGN KEY (`skill_definition_id`,`company_id`) REFERENCES `0_hrm_tal_001_skill_definitions` (`skill_definition_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_certification_definitions` (
 `certification_definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `certification_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `certification_name` varchar(180) NOT NULL,
 `issuer_name` varchar(180) NOT NULL,
 `category_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `expiry_policy_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `default_validity_days` int(10) unsigned DEFAULT NULL,
 `effective_from` date NOT NULL,
 `effective_to` date DEFAULT NULL,
 `definition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`certification_definition_id`),
 UNIQUE KEY `uq_htal1_cd_lineage` (`certification_definition_id`,`company_id`),
 UNIQUE KEY `uq_htal1_cd_version` (`company_id`,`certification_code`,`effective_from`),
 UNIQUE KEY `uq_htal1_cd_sha` (`definition_sha256`),
 KEY `ix_htal1_cd_effective` (`company_id`,`certification_code`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_employee_certification_evidence` (
 `employee_certification_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `certification_definition_id` bigint(20) unsigned NOT NULL,
 `employee_id` varchar(20) NOT NULL,
 `credential_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `issued_on` date NOT NULL,
 `expires_on` date DEFAULT NULL,
 `verification_status_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `verification_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`employee_certification_evidence_id`),
 UNIQUE KEY `uq_htal1_ce_lineage` (`employee_certification_evidence_id`,`company_id`),
 UNIQUE KEY `uq_htal1_ce_sha` (`evidence_sha256`),
 KEY `ix_htal1_ce_employee` (`company_id`,`employee_id`,`issued_on`,`expires_on`),
 KEY `ix_htal1_ce_cert` (`company_id`,`certification_definition_id`,`issued_on`),
 CONSTRAINT `fk_htal1_ce_cert` FOREIGN KEY (`certification_definition_id`,`company_id`) REFERENCES `0_hrm_tal_001_certification_definitions` (`certification_definition_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_export_audit` (
 `export_audit_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `actor_user_id` bigint(20) unsigned NOT NULL,
 `scope_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `subject_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `format_code` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `row_count` int(11) unsigned NOT NULL,
 `export_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`export_audit_id`),
 KEY `ix_htal1_xa_company_actor_time` (`company_id`,`actor_user_id`,`created_at`),
 KEY `ix_htal1_xa_subject` (`company_id`,`subject_reference_sha256`,`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_performance_cycle_definitions` (
 `performance_cycle_definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `cycle_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `cycle_name` varchar(140) NOT NULL,
 `purpose_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `period_start` date NOT NULL,
 `period_end` date NOT NULL,
 `rating_scale_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `effective_from` date NOT NULL,
 `effective_to` date DEFAULT NULL,
 `definition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`performance_cycle_definition_id`),
 UNIQUE KEY `uq_htal1_pc_lineage` (`performance_cycle_definition_id`,`company_id`),
 UNIQUE KEY `uq_htal1_pc_version` (`company_id`,`cycle_code`,`effective_from`),
 UNIQUE KEY `uq_htal1_pc_sha` (`definition_sha256`),
 KEY `ix_htal1_pc_effective` (`company_id`,`cycle_code`,`effective_from`,`effective_to`),
 KEY `ix_htal1_pc_period` (`company_id`,`period_start`,`period_end`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_performance_review_subjects` (
 `performance_review_subject_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `performance_cycle_definition_id` bigint(20) unsigned NOT NULL,
 `employee_id` varchar(20) NOT NULL,
 `review_type_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `review_window_start` date NOT NULL,
 `review_window_end` date NOT NULL,
 `subject_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `subject_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`performance_review_subject_id`),
 UNIQUE KEY `uq_htal1_pr_lineage` (`performance_review_subject_id`,`company_id`),
 UNIQUE KEY `uq_htal1_pr_subject` (`company_id`,`performance_cycle_definition_id`,`employee_id`,`review_type_code`),
 UNIQUE KEY `uq_htal1_pr_sha` (`subject_sha256`),
 KEY `ix_htal1_pr_employee` (`company_id`,`employee_id`,`review_window_start`,`review_window_end`),
 CONSTRAINT `fk_htal1_pr_cycle` FOREIGN KEY (`performance_cycle_definition_id`,`company_id`) REFERENCES `0_hrm_tal_001_performance_cycle_definitions` (`performance_cycle_definition_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_performance_assessment_evidence` (
 `performance_assessment_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `performance_review_subject_id` bigint(20) unsigned NOT NULL,
 `assessment_role_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `dimension_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `rating_value` decimal(9,4) DEFAULT NULL,
 `comment_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `evidence_date` date NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`performance_assessment_evidence_id`),
 UNIQUE KEY `uq_htal1_pa_lineage` (`performance_assessment_evidence_id`,`company_id`),
 UNIQUE KEY `uq_htal1_pa_sha` (`evidence_sha256`),
 KEY `ix_htal1_pa_subject` (`company_id`,`performance_review_subject_id`,`evidence_date`),
 KEY `ix_htal1_pa_dimension` (`company_id`,`dimension_code`,`evidence_date`),
 CONSTRAINT `fk_htal1_pa_subject` FOREIGN KEY (`performance_review_subject_id`,`company_id`) REFERENCES `0_hrm_tal_001_performance_review_subjects` (`performance_review_subject_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_performance_workflow_events` (
 `performance_workflow_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `performance_review_subject_id` bigint(20) unsigned NOT NULL,
 `from_state_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `to_state_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `aggregate_rating_value` decimal(9,4) DEFAULT NULL,
 `rating_basis_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 `reason_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `transition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `actor_user_id` bigint(20) unsigned NOT NULL,
 `occurred_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`performance_workflow_event_id`),
 UNIQUE KEY `uq_htal1_pw_lineage` (`performance_workflow_event_id`,`company_id`),
 UNIQUE KEY `uq_htal1_pw_sha` (`transition_sha256`),
 KEY `ix_htal1_pw_subject` (`company_id`,`performance_review_subject_id`,`performance_workflow_event_id`),
 CONSTRAINT `fk_htal1_pw_subject` FOREIGN KEY (`performance_review_subject_id`,`company_id`) REFERENCES `0_hrm_tal_001_performance_review_subjects` (`performance_review_subject_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_learning_definitions` (
 `learning_definition_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `learning_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `learning_name` varchar(140) NOT NULL,
 `delivery_mode_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_name` varchar(140) NOT NULL DEFAULT '',
 `duration_minutes` int(11) unsigned NOT NULL DEFAULT 0,
 `effective_from` date NOT NULL,
 `effective_to` date DEFAULT NULL,
 `definition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`learning_definition_id`),
 UNIQUE KEY `uq_htal1_ld_lineage` (`learning_definition_id`,`company_id`),
 UNIQUE KEY `uq_htal1_ld_version` (`company_id`,`learning_code`,`effective_from`),
 UNIQUE KEY `uq_htal1_ld_sha` (`definition_sha256`),
 KEY `ix_htal1_ld_effective` (`company_id`,`learning_code`,`effective_from`,`effective_to`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_learning_assignments` (
 `learning_assignment_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `learning_definition_id` bigint(20) unsigned NOT NULL,
 `employee_id` varchar(20) NOT NULL,
 `assigned_on` date NOT NULL,
 `due_on` date DEFAULT NULL,
 `assignment_status_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `assignment_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `assigned_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`learning_assignment_id`),
 UNIQUE KEY `uq_htal1_la_lineage` (`learning_assignment_id`,`company_id`),
 UNIQUE KEY `uq_htal1_la_sha` (`assignment_sha256`),
 KEY `ix_htal1_la_employee` (`company_id`,`employee_id`,`assigned_on`,`due_on`),
 CONSTRAINT `fk_htal1_la_def` FOREIGN KEY (`learning_definition_id`,`company_id`) REFERENCES `0_hrm_tal_001_learning_definitions` (`learning_definition_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_learning_completion_evidence` (
 `learning_completion_evidence_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `learning_assignment_id` bigint(20) unsigned NOT NULL,
 `completion_status_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `completed_on` date NOT NULL,
 `score_value` decimal(9,4) DEFAULT NULL,
 `evidence_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recorded_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`learning_completion_evidence_id`),
 UNIQUE KEY `uq_htal1_lc_lineage` (`learning_completion_evidence_id`,`company_id`),
 UNIQUE KEY `uq_htal1_lc_sha` (`evidence_sha256`),
 KEY `ix_htal1_lc_assignment` (`company_id`,`learning_assignment_id`,`completed_on`),
 CONSTRAINT `fk_htal1_lc_assignment` FOREIGN KEY (`learning_assignment_id`,`company_id`) REFERENCES `0_hrm_tal_001_learning_assignments` (`learning_assignment_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_confidential_cases` (
 `confidential_case_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `subject_employee_id` varchar(20) NOT NULL,
 `case_type_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `purpose_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `opened_on` date NOT NULL,
 `retention_until` date NOT NULL,
 `sensitivity_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `title_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `case_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`confidential_case_id`),
 UNIQUE KEY `uq_htal1_cc_lineage` (`confidential_case_id`,`company_id`),
 UNIQUE KEY `uq_htal1_cc_ref` (`case_reference_sha256`),
 KEY `ix_htal1_cc_subject` (`company_id`,`subject_employee_id`,`opened_on`),
 KEY `ix_htal1_cc_retention` (`company_id`,`retention_until`,`confidential_case_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_tal_001_confidential_case_access_grants` (
 `confidential_case_access_grant_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) NOT NULL,
 `confidential_case_id` bigint(20) unsigned NOT NULL,
 `grantee_user_id` bigint(20) unsigned NOT NULL,
 `access_role_code` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `purpose_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `valid_from` date NOT NULL,
 `valid_to` date DEFAULT NULL,
 `grant_reference_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `granted_by_user_id` bigint(20) unsigned NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`confidential_case_access_grant_id`),
 UNIQUE KEY `uq_htal1_cg_lineage` (`confidential_case_access_grant_id`,`company_id`),
 UNIQUE KEY `uq_htal1_cg_ref` (`grant_reference_sha256`),
 KEY `ix_htal1_cg_case_user` (`company_id`,`confidential_case_id`,`grantee_user_id`,`valid_from`,`valid_to`),
 CONSTRAINT `fk_htal1_cg_case` FOREIGN KEY (`confidential_case_id`,`company_id`) REFERENCES `0_hrm_tal_001_confidential_cases` (`confidential_case_id`,`company_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_publisher_profiles` (
  `publisher_profile_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `publisher_id` bigint(20) unsigned NOT NULL,
  `profile_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `identity_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `support_policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `security_contact_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `terms_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `requested_by` bigint(20) unsigned NOT NULL,
  `requested_at` datetime NOT NULL,
  `profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`publisher_profile_id`),
  UNIQUE KEY `uq_peco1_pp_code` (`profile_code`),
  UNIQUE KEY `uq_peco1_pp_sha` (`profile_sha256`),
  KEY `ix_peco1_pp_pub` (`publisher_id`,`requested_at`),
  CONSTRAINT `fk_peco1_pp_pub` FOREIGN KEY (`publisher_id`) REFERENCES `0_pay_ctry_publishers` (`publisher_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_pack_candidates` (
  `catalog_candidate_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `publisher_profile_id` bigint(20) unsigned NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `candidate_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `compatibility_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `certification_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `support_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `vulnerability_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revocation_state_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `sandbox_profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `submitted_by` bigint(20) unsigned NOT NULL,
  `submitted_at` datetime NOT NULL,
  `candidate_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`catalog_candidate_id`),
  UNIQUE KEY `uq_peco1_pc_code` (`candidate_code`),
  UNIQUE KEY `uq_peco1_pc_sha` (`candidate_sha256`),
  UNIQUE KEY `uq_peco1_pc_profile_pack` (`publisher_profile_id`,`pack_version_id`),
  KEY `ix_peco1_pc_pack` (`pack_version_id`,`submitted_at`),
  CONSTRAINT `fk_peco1_pc_profile` FOREIGN KEY (`publisher_profile_id`) REFERENCES `0_pay_eco_001_publisher_profiles` (`publisher_profile_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_pc_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_catalog_decisions` (
  `catalog_decision_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_candidate_id` bigint(20) unsigned NOT NULL,
  `decision` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `trust_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `certification_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `compatibility_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `vulnerability_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `decision_reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `maker_id` bigint(20) unsigned NOT NULL,
  `checker_id` bigint(20) unsigned NOT NULL,
  `decided_at` datetime NOT NULL,
  `decision_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`catalog_decision_id`),
  UNIQUE KEY `uq_peco1_cd_candidate` (`catalog_candidate_id`),
  UNIQUE KEY `uq_peco1_cd_sha` (`decision_sha256`),
  KEY `ix_peco1_cd_checker` (`checker_id`,`decided_at`),
  CONSTRAINT `fk_peco1_cd_candidate` FOREIGN KEY (`catalog_candidate_id`) REFERENCES `0_pay_eco_001_pack_candidates` (`catalog_candidate_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_sandbox_evaluations` (
  `sandbox_evaluation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_candidate_id` bigint(20) unsigned NOT NULL,
  `sandbox_profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `conformance_suite_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `manifest_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_inventory_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `findings_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evaluated_by` bigint(20) unsigned NOT NULL,
  `evaluated_at` datetime NOT NULL,
  `evaluation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`sandbox_evaluation_id`),
  UNIQUE KEY `uq_peco1_se_candidate_profile` (`catalog_candidate_id`,`sandbox_profile_sha256`),
  UNIQUE KEY `uq_peco1_se_sha` (`evaluation_sha256`),
  KEY `ix_peco1_se_result` (`result`,`evaluated_at`),
  CONSTRAINT `fk_peco1_se_candidate` FOREIGN KEY (`catalog_candidate_id`) REFERENCES `0_pay_eco_001_pack_candidates` (`catalog_candidate_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_dependency_scans` (
  `dependency_scan_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_candidate_id` bigint(20) unsigned NOT NULL,
  `sandbox_evaluation_id` bigint(20) unsigned NOT NULL,
  `dependency_lock_id` bigint(20) unsigned NOT NULL,
  `scan_profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `scan_suite_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `dependency_lock_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `dependency_inventory_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `dependency_trust_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `publisher_vulnerability_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `dependency_result` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `vulnerability_result` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `findings_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `scanned_by` smallint(6) unsigned NOT NULL,
  `scanned_at` datetime NOT NULL,
  `scan_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`dependency_scan_id`),
  UNIQUE KEY `uq_peco1_ds_candidate_snapshot` (`catalog_candidate_id`,`scan_profile_sha256`,`dependency_trust_snapshot_sha256`),
  UNIQUE KEY `uq_peco1_ds_sha` (`scan_sha256`),
  KEY `ix_peco1_ds_result` (`result`,`scanned_at`),
  KEY `ix_peco1_ds_sandbox` (`sandbox_evaluation_id`),
  KEY `ix_peco1_ds_lock` (`dependency_lock_id`),
  CONSTRAINT `fk_peco1_ds_candidate` FOREIGN KEY (`catalog_candidate_id`) REFERENCES `0_pay_eco_001_pack_candidates` (`catalog_candidate_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_ds_sandbox` FOREIGN KEY (`sandbox_evaluation_id`) REFERENCES `0_pay_eco_001_sandbox_evaluations` (`sandbox_evaluation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_ds_lock` FOREIGN KEY (`dependency_lock_id`) REFERENCES `0_pay_ctry_dependency_locks` (`dependency_lock_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_vulnerability_snapshots` (
  `vulnerability_snapshot_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `source_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `snapshot_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_authority_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `issued_at` datetime NOT NULL,
  `expires_at` datetime NOT NULL,
  `advisory_count` smallint(5) unsigned NOT NULL,
  `advisories_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `ingested_by` smallint(6) unsigned NOT NULL,
  `ingested_at` datetime NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`vulnerability_snapshot_id`),
  UNIQUE KEY `uq_peco1_vs_code` (`source_code`,`snapshot_code`),
  UNIQUE KEY `uq_peco1_vs_sha` (`snapshot_sha256`),
  KEY `ix_peco1_vs_effective` (`issued_at`,`expires_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_vulnerability_advisories` (
  `vulnerability_advisory_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `vulnerability_snapshot_id` bigint(20) unsigned NOT NULL,
  `ordinal_no` smallint(5) unsigned NOT NULL,
  `advisory_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `package_code` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `affected_version` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `severity` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `advisory_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `advisory_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`vulnerability_advisory_id`),
  UNIQUE KEY `uq_peco1_va_snapshot_ordinal` (`vulnerability_snapshot_id`,`ordinal_no`),
  UNIQUE KEY `uq_peco1_va_snapshot_advisory` (`vulnerability_snapshot_id`,`advisory_code`,`package_code`,`affected_version`),
  UNIQUE KEY `uq_peco1_va_snapshot_sha` (`vulnerability_snapshot_id`,`advisory_sha256`),
  KEY `ix_peco1_va_subject` (`package_code`,`affected_version`),
  CONSTRAINT `fk_peco1_va_snapshot` FOREIGN KEY (`vulnerability_snapshot_id`) REFERENCES `0_pay_eco_001_vulnerability_snapshots` (`vulnerability_snapshot_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_vulnerability_correlations` (
  `vulnerability_correlation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `dependency_scan_id` bigint(20) unsigned NOT NULL,
  `vulnerability_snapshot_id` bigint(20) unsigned NOT NULL,
  `subject_inventory_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `matched_advisories_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `max_severity` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `findings_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `correlated_by` smallint(6) unsigned NOT NULL,
  `correlated_at` datetime NOT NULL,
  `correlation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`vulnerability_correlation_id`),
  UNIQUE KEY `uq_peco1_vc_scan_snapshot` (`dependency_scan_id`,`vulnerability_snapshot_id`),
  UNIQUE KEY `uq_peco1_vc_sha` (`correlation_sha256`),
  KEY `ix_peco1_vc_result` (`result`,`correlated_at`),
  KEY `fk_peco1_vc_snapshot` (`vulnerability_snapshot_id`),
  CONSTRAINT `fk_peco1_vc_scan` FOREIGN KEY (`dependency_scan_id`) REFERENCES `0_pay_eco_001_dependency_scans` (`dependency_scan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_vc_snapshot` FOREIGN KEY (`vulnerability_snapshot_id`) REFERENCES `0_pay_eco_001_vulnerability_snapshots` (`vulnerability_snapshot_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_artifact_admissions` (
  `artifact_admission_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_candidate_id` bigint(20) unsigned NOT NULL,
  `verification_report_id` bigint(20) unsigned NOT NULL,
  `sandbox_evaluation_id` bigint(20) unsigned NOT NULL,
  `dependency_scan_id` bigint(20) unsigned NOT NULL,
  `vulnerability_correlation_id` bigint(20) unsigned NOT NULL,
  `admission_profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `manifest_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `artifact_inventory_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reproducibility_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signature_attestation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `trust_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `admission_findings_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `admitted_by` smallint(6) unsigned NOT NULL,
  `admitted_at` datetime NOT NULL,
  `admission_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`artifact_admission_id`),
  UNIQUE KEY `uq_peco1_aa_candidate_evidence` (`catalog_candidate_id`,`verification_report_id`,`vulnerability_correlation_id`),
  UNIQUE KEY `uq_peco1_aa_sha` (`admission_sha256`),
  KEY `ix_peco1_aa_result` (`result`,`admitted_at`),
  KEY `ix_peco1_aa_report` (`verification_report_id`),
  KEY `ix_peco1_aa_sandbox` (`sandbox_evaluation_id`),
  KEY `ix_peco1_aa_scan` (`dependency_scan_id`),
  KEY `ix_peco1_aa_correlation` (`vulnerability_correlation_id`),
  CONSTRAINT `fk_peco1_aa_candidate` FOREIGN KEY (`catalog_candidate_id`) REFERENCES `0_pay_eco_001_pack_candidates` (`catalog_candidate_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_aa_report` FOREIGN KEY (`verification_report_id`) REFERENCES `0_pay_ctry_verification_reports` (`verification_report_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_aa_sandbox` FOREIGN KEY (`sandbox_evaluation_id`) REFERENCES `0_pay_eco_001_sandbox_evaluations` (`sandbox_evaluation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_aa_scan` FOREIGN KEY (`dependency_scan_id`) REFERENCES `0_pay_eco_001_dependency_scans` (`dependency_scan_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_aa_correlation` FOREIGN KEY (`vulnerability_correlation_id`) REFERENCES `0_pay_eco_001_vulnerability_correlations` (`vulnerability_correlation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_catalog_publications` (
  `catalog_publication_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_candidate_id` bigint(20) unsigned NOT NULL,
  `catalog_decision_id` bigint(20) unsigned NOT NULL,
  `artifact_admission_id` bigint(20) unsigned NOT NULL,
  `certification_publication_id` bigint(20) unsigned NOT NULL,
  `catalog_release_id` bigint(20) unsigned NOT NULL,
  `catalog_admission_id` bigint(20) unsigned NOT NULL,
  `publication_profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `catalog_root_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `manifest_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `semantic_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `evidence_root` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `support_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `vulnerability_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `trust_snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `published_by` bigint(20) unsigned NOT NULL,
  `published_at` datetime NOT NULL,
  `publication_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`catalog_publication_id`),
  UNIQUE KEY `uq_peco1_cp_candidate` (`catalog_candidate_id`),
  UNIQUE KEY `uq_peco1_cp_sha` (`publication_sha256`),
  KEY `ix_peco1_cp_release` (`catalog_release_id`,`published_at`),
  KEY `ix_peco1_cp_admission` (`catalog_admission_id`),
  KEY `ix_peco1_cp_certpub` (`certification_publication_id`),
  CONSTRAINT `fk_peco1_cp_candidate` FOREIGN KEY (`catalog_candidate_id`) REFERENCES `0_pay_eco_001_pack_candidates` (`catalog_candidate_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cp_decision` FOREIGN KEY (`catalog_decision_id`) REFERENCES `0_pay_eco_001_catalog_decisions` (`catalog_decision_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cp_artifact` FOREIGN KEY (`artifact_admission_id`) REFERENCES `0_pay_eco_001_artifact_admissions` (`artifact_admission_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cp_certpub` FOREIGN KEY (`certification_publication_id`) REFERENCES `0_pay_ctry_publication_records` (`publication_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cp_release` FOREIGN KEY (`catalog_release_id`) REFERENCES `0_pay_ctry_catalog_releases` (`catalog_release_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cp_catalog_admission` FOREIGN KEY (`catalog_admission_id`) REFERENCES `0_pay_ctry_catalog_release_admissions` (`catalog_admission_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_catalog_lifecycle_events` (
  `catalog_lifecycle_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_publication_id` bigint(20) unsigned NOT NULL,
  `source_type` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `certification_emergency_action_id` bigint(20) unsigned NULL,
  `publisher_revocation_id` bigint(20) unsigned NULL,
  `trust_key_revocation_id` bigint(20) unsigned NULL,
  `action_type` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_at` datetime NOT NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `source_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `propagated_by` smallint(6) unsigned NOT NULL,
  `propagated_at` datetime NOT NULL,
  `lifecycle_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`catalog_lifecycle_event_id`),
  UNIQUE KEY `uq_peco1_cl_sha` (`lifecycle_sha256`),
  UNIQUE KEY `uq_peco1_cl_cert_action` (`catalog_publication_id`,`certification_emergency_action_id`),
  UNIQUE KEY `uq_peco1_cl_pub_rev` (`catalog_publication_id`,`publisher_revocation_id`),
  UNIQUE KEY `uq_peco1_cl_key_rev` (`catalog_publication_id`,`trust_key_revocation_id`),
  KEY `ix_peco1_cl_publication` (`catalog_publication_id`,`effective_at`),
  KEY `ix_peco1_cl_cert_action` (`certification_emergency_action_id`),
  KEY `ix_peco1_cl_pub_rev` (`publisher_revocation_id`),
  KEY `ix_peco1_cl_key_rev` (`trust_key_revocation_id`),
  CONSTRAINT `fk_peco1_cl_publication` FOREIGN KEY (`catalog_publication_id`) REFERENCES `0_pay_eco_001_catalog_publications` (`catalog_publication_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cl_cert_action` FOREIGN KEY (`certification_emergency_action_id`) REFERENCES `0_pay_ctry_certification_emergency_actions` (`emergency_action_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cl_pub_rev` FOREIGN KEY (`publisher_revocation_id`) REFERENCES `0_pay_ctry_publisher_revocations` (`publisher_revocation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_cl_key_rev` FOREIGN KEY (`trust_key_revocation_id`) REFERENCES `0_pay_ctry_trust_key_revocations` (`key_revocation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_marketplace_installations` (
  `marketplace_installation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `catalog_publication_id` bigint(20) unsigned NOT NULL,
  `site_installation_id` bigint(20) unsigned NOT NULL,
  `pack_version_id` bigint(20) unsigned NOT NULL,
  `publication_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `receipt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `recorded_by` smallint(6) unsigned NOT NULL,
  `recorded_at` datetime NOT NULL,
  `marketplace_installation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`marketplace_installation_id`),
  UNIQUE KEY `uq_peco1_mi_pub_install` (`catalog_publication_id`,`site_installation_id`),
  UNIQUE KEY `uq_peco1_mi_sha` (`marketplace_installation_sha256`),
  KEY `ix_peco1_mi_install` (`site_installation_id`,`recorded_at`),
  KEY `ix_peco1_mi_pack` (`pack_version_id`),
  CONSTRAINT `fk_peco1_mi_publication` FOREIGN KEY (`catalog_publication_id`) REFERENCES `0_pay_eco_001_catalog_publications` (`catalog_publication_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_mi_install` FOREIGN KEY (`site_installation_id`) REFERENCES `0_pay_ctry_site_installations` (`site_installation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_mi_pack` FOREIGN KEY (`pack_version_id`) REFERENCES `0_pay_ctry_pack_versions` (`pack_version_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_pay_eco_001_marketplace_activations` (
  `marketplace_activation_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `marketplace_installation_id` bigint(20) unsigned NOT NULL,
  `activation_id` bigint(20) unsigned NOT NULL,
  `binding_id` bigint(20) unsigned NOT NULL,
  `company_id` smallint(5) unsigned NOT NULL,
  `scope_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `legal_entity_id` bigint(20) unsigned NULL,
  `payroll_scope_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `action_type` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effective_from` date NOT NULL,
  `effective_to` date NULL,
  `activation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `recorded_by` smallint(6) unsigned NOT NULL,
  `recorded_at` datetime NOT NULL,
  `marketplace_activation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `created_at` datetime NOT NULL,
  PRIMARY KEY (`marketplace_activation_id`),
  UNIQUE KEY `uq_peco1_ma_activation` (`activation_id`),
  UNIQUE KEY `uq_peco1_ma_binding` (`binding_id`),
  UNIQUE KEY `uq_peco1_ma_sha` (`marketplace_activation_sha256`),
  KEY `ix_peco1_ma_install` (`marketplace_installation_id`,`recorded_at`),
  KEY `ix_peco1_ma_scope` (`company_id`,`scope_type`,`legal_entity_id`,`payroll_scope_key`,`effective_from`),
  CONSTRAINT `fk_peco1_ma_market_install` FOREIGN KEY (`marketplace_installation_id`) REFERENCES `0_pay_eco_001_marketplace_installations` (`marketplace_installation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_ma_activation` FOREIGN KEY (`activation_id`) REFERENCES `0_pay_ctry_activations` (`activation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `fk_peco1_ma_binding` FOREIGN KEY (`binding_id`) REFERENCES `0_pay_ctry_activation_bindings` (`binding_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_rec_001_profiles` (
 `reconciliation_profile_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` bigint(20) unsigned NOT NULL,
 `legal_entity_id` bigint(20) unsigned NOT NULL,
 `payroll_period_id` int(10) unsigned NOT NULL,
 `pay_core_run_id` bigint(20) unsigned NOT NULL,
 `filing_obligation_id` bigint(20) unsigned NOT NULL,
 `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `policy_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `tolerance_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `rounding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `filing_completion_policy_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payroll_scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `prepared_by` bigint(20) unsigned NOT NULL,
 `prepared_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`reconciliation_profile_id`),
 UNIQUE KEY `uq_pr1_pf_sha` (`profile_sha256`),
 UNIQUE KEY `uq_pr1_pf_scope` (`company_id`,`legal_entity_id`,`payroll_period_id`,`pay_core_run_id`,`filing_obligation_id`,`policy_version`,`tolerance_sha256`,`rounding_sha256`),
 KEY `ix_pr1_pf_scope` (`company_id`,`legal_entity_id`,`payroll_period_id`),
 KEY `fk_pr1_pf_run` (`pay_core_run_id`),
 KEY `fk_pr1_pf_ob` (`filing_obligation_id`),
 CONSTRAINT `fk_pr1_pf_run` FOREIGN KEY (`pay_core_run_id`) REFERENCES `0_hrm_pay_core_009_runs` (`run_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_pf_ob` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_rec_001_evidence_sets` (
 `reconciliation_evidence_set_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `reconciliation_profile_id` bigint(20) unsigned NOT NULL,
 `evidence_kind` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `membership_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `item_count` int(10) unsigned NOT NULL,
 `amount_minor` bigint(20) NOT NULL,
 `status_token` varchar(24) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `current_evidence` tinyint(1) unsigned NOT NULL,
 `source_contract_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `recomputed_by` bigint(20) unsigned NOT NULL,
 `recomputed_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`reconciliation_evidence_set_id`),
 UNIQUE KEY `uq_pr1_es_sha` (`evidence_sha256`),
 UNIQUE KEY `uq_pr1_es_scope` (`reconciliation_profile_id`,`evidence_kind`,`membership_sha256`),
 KEY `ix_pr1_es_current` (`reconciliation_profile_id`,`evidence_kind`,`current_evidence`),
 KEY `fk_pr1_es_pf` (`reconciliation_profile_id`),
 CONSTRAINT `fk_pr1_es_pf` FOREIGN KEY (`reconciliation_profile_id`) REFERENCES `0_hrm_pay_rec_001_profiles` (`reconciliation_profile_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_rec_001_checklists` (
 `close_checklist_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `reconciliation_profile_id` bigint(20) unsigned NOT NULL,
 `payroll_evidence_set_id` bigint(20) unsigned NOT NULL,
 `gl_evidence_set_id` bigint(20) unsigned NOT NULL,
 `payment_evidence_set_id` bigint(20) unsigned NOT NULL,
 `filing_evidence_set_id` bigint(20) unsigned NOT NULL,
 `filing_completion_receipt_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `readiness_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `checklist_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `prepared_by` bigint(20) unsigned NOT NULL,
 `prepared_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`close_checklist_id`),
 UNIQUE KEY `uq_pr1_cl_sha` (`checklist_sha256`),
 UNIQUE KEY `uq_pr1_cl_ready` (`reconciliation_profile_id`,`readiness_sha256`),
 KEY `fk_pr1_cl_pf` (`reconciliation_profile_id`),
 KEY `fk_pr1_cl_pay` (`payroll_evidence_set_id`),
 KEY `fk_pr1_cl_gl` (`gl_evidence_set_id`),
 KEY `fk_pr1_cl_pmt` (`payment_evidence_set_id`),
 KEY `fk_pr1_cl_fil` (`filing_evidence_set_id`),
 CONSTRAINT `fk_pr1_cl_pf` FOREIGN KEY (`reconciliation_profile_id`) REFERENCES `0_hrm_pay_rec_001_profiles` (`reconciliation_profile_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_cl_pay` FOREIGN KEY (`payroll_evidence_set_id`) REFERENCES `0_hrm_pay_rec_001_evidence_sets` (`reconciliation_evidence_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_cl_gl` FOREIGN KEY (`gl_evidence_set_id`) REFERENCES `0_hrm_pay_rec_001_evidence_sets` (`reconciliation_evidence_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_cl_pmt` FOREIGN KEY (`payment_evidence_set_id`) REFERENCES `0_hrm_pay_rec_001_evidence_sets` (`reconciliation_evidence_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_cl_fil` FOREIGN KEY (`filing_evidence_set_id`) REFERENCES `0_hrm_pay_rec_001_evidence_sets` (`reconciliation_evidence_set_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_rec_001_reviews` (
 `reconciliation_review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `close_checklist_id` bigint(20) unsigned NOT NULL,
 `checklist_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `readiness_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `decision` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `review_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `reviewed_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`reconciliation_review_id`),
 UNIQUE KEY `uq_pr1_rv_checklist` (`close_checklist_id`),
 UNIQUE KEY `uq_pr1_rv_sha` (`review_sha256`),
 KEY `fk_pr1_rv_checklist` (`close_checklist_id`),
 CONSTRAINT `fk_pr1_rv_checklist` FOREIGN KEY (`close_checklist_id`) REFERENCES `0_hrm_pay_rec_001_checklists` (`close_checklist_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_rec_001_approvals` (
 `reconciliation_approval_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `reconciliation_review_id` bigint(20) unsigned NOT NULL,
 `close_checklist_id` bigint(20) unsigned NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `decision` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `assurance_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`reconciliation_approval_id`),
 UNIQUE KEY `uq_pr1_ap_review` (`reconciliation_review_id`),
 UNIQUE KEY `uq_pr1_ap_checklist` (`close_checklist_id`),
 UNIQUE KEY `uq_pr1_ap_sha` (`approval_sha256`),
 KEY `fk_pr1_ap_review` (`reconciliation_review_id`),
 KEY `fk_pr1_ap_checklist` (`close_checklist_id`),
 CONSTRAINT `fk_pr1_ap_review` FOREIGN KEY (`reconciliation_review_id`) REFERENCES `0_hrm_pay_rec_001_reviews` (`reconciliation_review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_ap_checklist` FOREIGN KEY (`close_checklist_id`) REFERENCES `0_hrm_pay_rec_001_checklists` (`close_checklist_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_rec_001_state_events` (
 `reconciliation_state_event_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `reconciliation_profile_id` bigint(20) unsigned NOT NULL,
 `close_checklist_id` bigint(20) unsigned NOT NULL,
 `reconciliation_review_id` bigint(20) unsigned NOT NULL,
 `reconciliation_approval_id` bigint(20) unsigned NOT NULL,
 `company_id` int(11) unsigned NOT NULL,
 `legal_entity_id` bigint(20) unsigned NOT NULL,
 `payroll_period_id` bigint(20) unsigned NOT NULL,
 `pay_core_run_id` bigint(20) unsigned NOT NULL,
 `filing_obligation_id` bigint(20) unsigned NOT NULL,
 `event_sequence` int(11) unsigned NOT NULL,
 `prior_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `event_type` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `resulting_state` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reason_code` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `checklist_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `transition_evidence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `acted_by` bigint(20) unsigned NOT NULL,
 `acted_at` datetime NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`reconciliation_state_event_id`),
 UNIQUE KEY `uq_pr1_se_scope_seq` (`company_id`,`pay_core_run_id`,`filing_obligation_id`,`event_sequence`),
 UNIQUE KEY `uq_pr1_se_sha` (`event_sha256`),
 KEY `ix_pr1_se_scope_state` (`company_id`,`pay_core_run_id`,`filing_obligation_id`,`resulting_state`),
 KEY `fk_pr1_se_profile` (`reconciliation_profile_id`),
 KEY `fk_pr1_se_checklist` (`close_checklist_id`),
 KEY `fk_pr1_se_review` (`reconciliation_review_id`),
 KEY `fk_pr1_se_approval` (`reconciliation_approval_id`),
 KEY `fk_pr1_se_run` (`pay_core_run_id`),
 KEY `fk_pr1_se_obligation` (`filing_obligation_id`),
 CONSTRAINT `fk_pr1_se_profile` FOREIGN KEY (`reconciliation_profile_id`) REFERENCES `0_hrm_pay_rec_001_profiles` (`reconciliation_profile_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_se_checklist` FOREIGN KEY (`close_checklist_id`) REFERENCES `0_hrm_pay_rec_001_checklists` (`close_checklist_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_se_review` FOREIGN KEY (`reconciliation_review_id`) REFERENCES `0_hrm_pay_rec_001_reviews` (`reconciliation_review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_se_approval` FOREIGN KEY (`reconciliation_approval_id`) REFERENCES `0_hrm_pay_rec_001_approvals` (`reconciliation_approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_se_run` FOREIGN KEY (`pay_core_run_id`) REFERENCES `0_hrm_pay_core_009_runs` (`run_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
 CONSTRAINT `fk_pr1_se_obligation` FOREIGN KEY (`filing_obligation_id`) REFERENCES `0_hrm_pay_fil_001_obligations` (`filing_obligation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_con_001_provider_profiles` (
 `provider_profile_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `provider_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `delivery_model_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `jurisdiction_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `residency_region_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `adapter_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `due_diligence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `data_residency_review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `security_review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_contract_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_by` bigint(20) unsigned NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `profile_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`provider_profile_id`),
 UNIQUE KEY `uq_pcon1_pp_code` (`company_id`,`provider_code`),
 UNIQUE KEY `uq_pcon1_pp_sha` (`profile_sha256`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_con_001_mapping_versions` (
 `mapping_version_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `provider_profile_id` bigint(20) unsigned NOT NULL,
 `mapping_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `exchange_direction` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `payload_class` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `mapping_version` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_schema_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `target_schema_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `mapping_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `effective_from` date NOT NULL,
 `effective_to` date DEFAULT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `approved_at` datetime NOT NULL,
 `mapping_version_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`mapping_version_id`),
 UNIQUE KEY `uq_pcon1_mv_keyver` (`provider_profile_id`,`mapping_key`,`mapping_version`),
 UNIQUE KEY `uq_pcon1_mv_sha` (`mapping_version_sha256`),
 KEY `fk_pcon1_mv_profile` (`provider_profile_id`),
 CONSTRAINT `fk_pcon1_mv_profile` FOREIGN KEY (`provider_profile_id`) REFERENCES `0_hrm_pay_con_001_provider_profiles` (`provider_profile_id`) ON UPDATE RESTRICT ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `0_hrm_pay_con_001_provider_admission_reviews` (
 `provider_admission_review_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
 `company_id` int(11) unsigned NOT NULL,
 `provider_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `jurisdiction_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `delivery_model_code` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `residency_region_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `vendor_due_diligence_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `legal_review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `security_review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `data_residency_review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `delivery_model_disclosure_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `provider_contract_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `source_provenance_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `requested_by` bigint(20) unsigned NOT NULL,
 `reviewed_by` bigint(20) unsigned NOT NULL,
 `approved_by` bigint(20) unsigned NOT NULL,
 `decision_code` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `reviewed_at` datetime NOT NULL,
 `approved_at` datetime NOT NULL,
 `admission_review_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
 `created_at` datetime NOT NULL,
 PRIMARY KEY (`provider_admission_review_id`),
 UNIQUE KEY `uq_pcon1_par_hash` (`admission_review_sha256`),
 KEY `ix_pcon1_par_provider` (`company_id`,`provider_code`,`approved_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE `0_hrm_pay_glb_001_native_executions` (
  `native_execution_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `company_id` int unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `payroll_group_id` bigint unsigned NOT NULL,
  `group_version_id` bigint unsigned NOT NULL,
  `payroll_period_id` int unsigned NOT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `source_kind` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `producer_contract_version` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `producer_event_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `native_execution_key` varchar(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `revision_no` bigint unsigned NOT NULL,
  `predecessor_execution_id` bigint unsigned NULL,
  `joint_core009_run_id` bigint unsigned NULL,
  `joint_core009_state_version` bigint unsigned NULL,
  `joint_core009_state_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `joint_producer_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `member_count` int unsigned NOT NULL,
  `member_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `core010_result_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `gross_8` decimal(38,8) NOT NULL,
  `deduction_8` decimal(38,8) NOT NULL,
  `net_8` decimal(38,8) NOT NULL,
  `currency_code` char(3) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `maker_id` int unsigned NOT NULL,
  `produced_at_utc` datetime NOT NULL,
  `execution_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `receipt_json` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`native_execution_id`),
  UNIQUE KEY `uq_pgn_e_key` (`native_execution_key`),
  UNIQUE KEY `uq_pgn_e_sha` (`execution_sha256`),
  UNIQUE KEY `uq_pgn_e_revision` (`company_id`,`legal_entity_id`,`payroll_group_id`,`payroll_period_id`,`revision_no`),
  UNIQUE KEY `uq_pgn_e_predecessor` (`predecessor_execution_id`),
  UNIQUE KEY `uq_pgn_e_joint` (`joint_core009_run_id`),
  CONSTRAINT `0_fk_pgn_e_predecessor` FOREIGN KEY (`predecessor_execution_id`) REFERENCES `0_hrm_pay_glb_001_native_executions` (`native_execution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_e_joint` FOREIGN KEY (`joint_core009_run_id`) REFERENCES `0_hrm_pay_core_009_runs` (`run_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_ck_pgn_e_0` CHECK (member_count > 0 AND revision_no > 0 AND maker_id > 0),
  CONSTRAINT `0_ck_pgn_e_1` CHECK (gross_8 - deduction_8 = net_8),
  CONSTRAINT `0_ck_pgn_e_2` CHECK (source_kind = 'GOVERNED_NATIVE_V1' AND joint_core009_run_id IS NULL AND joint_core009_state_version IS NULL AND joint_core009_state_sha256 IS NULL AND joint_producer_sha256 IS NULL OR source_kind = 'JOINT_CORE009_V1' AND joint_core009_run_id IS NOT NULL AND joint_core009_state_version IS NOT NULL AND joint_core009_state_sha256 IS NOT NULL AND joint_producer_sha256 IS NOT NULL),
  CONSTRAINT `0_ck_pgn_e_3` CHECK (JSON_VALID(receipt_json))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE `0_hrm_pay_glb_001_native_members` (
  `native_member_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `native_execution_id` bigint unsigned NOT NULL,
  `ordinal_no` int unsigned NOT NULL,
  `payroll_relationship_id` bigint unsigned NOT NULL,
  `assignment_id` bigint unsigned NULL,
  `core004_snapshot_id` bigint unsigned NOT NULL,
  `core004_result_id` bigint unsigned NOT NULL,
  `snapshot_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `line_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `trace_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `joint_core009_run_item_id` bigint unsigned NULL,
  `joint_core009_document_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `cross_domain_parity_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `member_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `receipt_json` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`native_member_id`),
  UNIQUE KEY `uq_pgn_m_0` (`native_execution_id`,`ordinal_no`),
  UNIQUE KEY `uq_pgn_m_1` (`native_execution_id`,`payroll_relationship_id`),
  UNIQUE KEY `uq_pgn_m_2` (`native_execution_id`,`core004_snapshot_id`),
  UNIQUE KEY `uq_pgn_m_3` (`native_execution_id`,`core004_result_id`),
  UNIQUE KEY `uq_pgn_m_4` (`native_execution_id`,`joint_core009_run_item_id`),
  UNIQUE KEY `uq_pgn_m_sha` (`member_sha256`),
  KEY `ix_pgn_m_snapshot` (`core004_snapshot_id`),
  KEY `ix_pgn_m_result` (`core004_result_id`),
  KEY `ix_pgn_m_item` (`joint_core009_run_item_id`),
  CONSTRAINT `0_fk_pgn_m_execution` FOREIGN KEY (`native_execution_id`) REFERENCES `0_hrm_pay_glb_001_native_executions` (`native_execution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_m_snapshot` FOREIGN KEY (`core004_snapshot_id`) REFERENCES `0_hrm_pay_core_004_snapshots` (`snapshot_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_m_result` FOREIGN KEY (`core004_result_id`) REFERENCES `0_hrm_pay_core_004_results` (`result_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_m_item` FOREIGN KEY (`joint_core009_run_item_id`) REFERENCES `0_hrm_pay_core_009_items` (`run_item_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_ck_pgn_m_0` CHECK (ordinal_no > 0),
  CONSTRAINT `0_ck_pgn_m_1` CHECK (joint_core009_run_item_id IS NULL AND joint_core009_document_sha256 IS NULL AND cross_domain_parity_sha256 IS NULL OR joint_core009_run_item_id IS NOT NULL AND joint_core009_document_sha256 IS NOT NULL AND cross_domain_parity_sha256 IS NOT NULL),
  CONSTRAINT `0_ck_pgn_m_2` CHECK (JSON_VALID(receipt_json))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE `0_hrm_pay_glb_001_native_selection_events` (
  `selection_event_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `company_id` int unsigned NOT NULL,
  `legal_entity_id` int unsigned NOT NULL,
  `payroll_group_id` bigint unsigned NOT NULL,
  `group_version_id` bigint unsigned NOT NULL,
  `payroll_period_id` int unsigned NOT NULL,
  `calendar_period_id` bigint unsigned NOT NULL,
  `sequence_no` bigint unsigned NOT NULL,
  `predecessor_selection_event_id` bigint unsigned NULL,
  `predecessor_selection_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `event_kind` varchar(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `native_execution_id` bigint unsigned NULL,
  `reason_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `maker_id` int unsigned NOT NULL,
  `checker_id` int unsigned NOT NULL,
  `command_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `selection_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `occurred_at_utc` datetime NOT NULL,
  `receipt_json` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`selection_event_id`),
  UNIQUE KEY `uq_pgn_s_seq` (`company_id`,`legal_entity_id`,`payroll_group_id`,`payroll_period_id`,`sequence_no`),
  UNIQUE KEY `uq_pgn_s_sha` (`selection_sha256`),
  UNIQUE KEY `uq_pgn_s_cmd` (`company_id`,`legal_entity_id`,`payroll_group_id`,`payroll_period_id`,`command_key_sha256`),
  UNIQUE KEY `uq_pgn_s_pred` (`predecessor_selection_event_id`),
  KEY `ix_pgn_s_execution` (`native_execution_id`),
  CONSTRAINT `0_fk_pgn_s_pred` FOREIGN KEY (`predecessor_selection_event_id`) REFERENCES `0_hrm_pay_glb_001_native_selection_events` (`selection_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_s_execution` FOREIGN KEY (`native_execution_id`) REFERENCES `0_hrm_pay_glb_001_native_executions` (`native_execution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_ck_pgn_s_0` CHECK (sequence_no > 0 AND maker_id > 0 AND checker_id > 0 AND maker_id <> checker_id),
  CONSTRAINT `0_ck_pgn_s_1` CHECK (event_kind = 'select' AND native_execution_id IS NOT NULL OR event_kind IN ('suspend','revoke') AND native_execution_id IS NULL),
  CONSTRAINT `0_ck_pgn_s_2` CHECK (sequence_no = 1 AND predecessor_selection_event_id IS NULL AND predecessor_selection_sha256 IS NULL OR sequence_no > 1 AND predecessor_selection_event_id IS NOT NULL AND predecessor_selection_sha256 IS NOT NULL),
  CONSTRAINT `0_ck_pgn_s_3` CHECK (JSON_VALID(receipt_json))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE `0_hrm_pay_glb_001_native_approval_bindings` (
  `approval_binding_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `selection_event_id` bigint unsigned NOT NULL,
  `native_execution_id` bigint unsigned NOT NULL,
  `approval_intent_id` bigint unsigned NOT NULL,
  `approval_id` bigint unsigned NOT NULL,
  `intent_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `result_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `projection_set_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `cutover_transition_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `maker_id` int unsigned NOT NULL,
  `approver_id` int unsigned NOT NULL,
  `binder_id` int unsigned NOT NULL,
  `command_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `receipt_json` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`approval_binding_id`),
  UNIQUE KEY `uq_pgn_a_sel` (`selection_event_id`),
  UNIQUE KEY `uq_pgn_a_approval` (`approval_id`),
  UNIQUE KEY `uq_pgn_a_sha` (`binding_sha256`),
  KEY `ix_pgn_a_execution` (`native_execution_id`),
  KEY `ix_pgn_a_intent` (`approval_intent_id`),
  KEY `ix_pgn_a_cmd` (`command_key_sha256`),
  CONSTRAINT `0_fk_pgn_a_sel` FOREIGN KEY (`selection_event_id`) REFERENCES `0_hrm_pay_glb_001_native_selection_events` (`selection_event_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_a_execution` FOREIGN KEY (`native_execution_id`) REFERENCES `0_hrm_pay_glb_001_native_executions` (`native_execution_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_a_intent` FOREIGN KEY (`approval_intent_id`) REFERENCES `0_hrm_pay_core_010_approval_intents` (`approval_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_a_approval` FOREIGN KEY (`approval_id`) REFERENCES `0_hrm_pay_core_010_approvals` (`approval_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_ck_pgn_a_0` CHECK (maker_id > 0 AND approver_id > 0 AND binder_id > 0 AND maker_id <> approver_id),
  CONSTRAINT `0_ck_pgn_a_1` CHECK (JSON_VALID(receipt_json))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE `0_hrm_pay_glb_001_native_gl_bindings` (
  `gl_binding_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `approval_binding_id` bigint unsigned NOT NULL,
  `journal_intent_id` bigint unsigned NOT NULL,
  `reconciliation_id` bigint unsigned NOT NULL,
  `review_id` bigint unsigned NOT NULL,
  `posting_authority_id` bigint unsigned NOT NULL,
  `posting_id` bigint unsigned NOT NULL,
  `journal_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `approval_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `posting_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reconciliation_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `posting_kind` varchar(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `predecessor_posting_id` bigint unsigned NULL,
  `native_gl_effect_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `command_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `gl_binding_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `bound_by` int unsigned NOT NULL,
  `receipt_json` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`gl_binding_id`),
  UNIQUE KEY `uq_pgn_g_posting` (`posting_id`),
  UNIQUE KEY `uq_pgn_g_sha` (`gl_binding_sha256`),
  KEY `ix_pgn_g_approval` (`approval_binding_id`,`posting_id`),
  KEY `ix_pgn_g_0` (`journal_intent_id`),
  KEY `ix_pgn_g_1` (`reconciliation_id`),
  KEY `ix_pgn_g_2` (`review_id`),
  KEY `ix_pgn_g_3` (`posting_authority_id`),
  KEY `ix_pgn_g_4` (`predecessor_posting_id`),
  KEY `ix_pgn_g_5` (`command_key_sha256`),
  CONSTRAINT `0_fk_pgn_g_approval` FOREIGN KEY (`approval_binding_id`) REFERENCES `0_hrm_pay_glb_001_native_approval_bindings` (`approval_binding_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_g_intent` FOREIGN KEY (`journal_intent_id`) REFERENCES `0_hrm_pay_gl_001_journal_intents` (`journal_intent_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_g_recon` FOREIGN KEY (`reconciliation_id`) REFERENCES `0_hrm_pay_gl_001_reconciliations` (`reconciliation_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_g_review` FOREIGN KEY (`review_id`) REFERENCES `0_hrm_pay_gl_001_reviews` (`review_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_g_authority` FOREIGN KEY (`posting_authority_id`) REFERENCES `0_hrm_pay_gl_001_posting_authorities` (`authority_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_g_posting` FOREIGN KEY (`posting_id`) REFERENCES `0_hrm_pay_gl_001_gl_postings` (`posting_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_fk_pgn_g_pred` FOREIGN KEY (`predecessor_posting_id`) REFERENCES `0_hrm_pay_gl_001_gl_postings` (`posting_id`) ON UPDATE RESTRICT ON DELETE RESTRICT,
  CONSTRAINT `0_ck_pgn_g_0` CHECK (bound_by > 0),
  CONSTRAINT `0_ck_pgn_g_1` CHECK (posting_kind = 'post' AND predecessor_posting_id IS NULL OR posting_kind IN ('reverse','repost') AND predecessor_posting_id IS NOT NULL),
  CONSTRAINT `0_ck_pgn_g_2` CHECK (JSON_VALID(receipt_json))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE `0_hrm_pay_glb_001_native_command_receipts` (
  `command_receipt_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `company_id` int unsigned NOT NULL,
  `command_domain` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `scope_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `command_key_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `expected_predecessor_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
  `request_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effect_kind` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `effect_id` bigint unsigned NOT NULL,
  `effect_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `actor_id` int unsigned NOT NULL,
  `purpose_code` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `command_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `request_context_json` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `receipt_json` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`command_receipt_id`),
  UNIQUE KEY `uq_pgn_c_replay` (`company_id`,`command_domain`,`scope_sha256`,`command_key_sha256`),
  UNIQUE KEY `uq_pgn_c_sha` (`command_sha256`),
  KEY `ix_pgn_c_effect` (`effect_kind`,`effect_id`),
  CONSTRAINT `0_ck_pgn_c_0` CHECK (actor_id > 0 AND effect_id > 0),
  CONSTRAINT `0_ck_pgn_c_1` CHECK (JSON_VALID(request_context_json)),
  CONSTRAINT `0_ck_pgn_c_2` CHECK (JSON_VALID(receipt_json))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

ALTER TABLE `0_leave_details` ADD KEY `leave_employee_history_idx` (`employee_id`,`id`,`from_date`,`to_date`);

ALTER TABLE `0_payslips` MODIFY COLUMN `status`                  tinyint(1)   NOT NULL DEFAULT '0' COMMENT '0=draft, 1=confirmed, 2=posted, 3=paid, 4-5=legacy financial, 6=voided/reversed';

ALTER TABLE `0_document_types` ADD COLUMN `security_class` varchar(32) NOT NULL DEFAULT 'restricted';

ALTER TABLE `0_employee_documents` ADD COLUMN `access_key` char(64) DEFAULT NULL;

ALTER TABLE `0_employee_documents` ADD COLUMN `content_state` varchar(24) NOT NULL DEFAULT 'metadata_only';

ALTER TABLE `0_employee_documents` ADD COLUMN `content_sha256` char(64) DEFAULT NULL;

ALTER TABLE `0_employee_documents` ADD COLUMN `content_size`  int(11) unsigned DEFAULT NULL;

ALTER TABLE `0_employee_documents` ADD COLUMN `content_mime`  varchar(100) DEFAULT NULL;

ALTER TABLE `0_employee_documents` ADD COLUMN `retention_locked` tinyint(1) NOT NULL DEFAULT '0';

ALTER TABLE `0_employee_documents` ADD KEY `content_state` (`content_state`);

ALTER TABLE `0_overtime_requests` MODIFY COLUMN `status`        tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=pending, 1=approved, 2=rejected, 3=cancelled';

ALTER TABLE `0_payroll_periods` MODIFY COLUMN `status`              tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=draft, 1=calculated, 2=approved, 3=posted, 4=paid, 5=closed, 6=voided/reversed';

ALTER TABLE `0_employee_appraisals` MODIFY COLUMN `status`           tinyint(1) NOT NULL DEFAULT '0' COMMENT '0=draft,1=submitted,2=approved,3=rejected/cancelled';

ALTER TABLE `0_approval_notifications` ADD COLUMN `delivery_attempts` INT(10) UNSIGNED NOT NULL DEFAULT 0;

ALTER TABLE `0_approval_notifications` ADD COLUMN `attempt_limit`   SMALLINT(5) UNSIGNED NOT NULL DEFAULT 5;

ALTER TABLE `0_approval_notifications` ADD COLUMN `repair_count`    INT(10) UNSIGNED NOT NULL DEFAULT 0;

ALTER TABLE `0_approval_notifications` ADD COLUMN `last_repaired_by` INT(11) DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD COLUMN `last_repaired_at` DATETIME DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD COLUMN `next_attempt_at` DATETIME     DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD COLUMN `lease_token`     CHAR(64)     DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD COLUMN `lease_expires_at` DATETIME    DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD COLUMN `last_attempt_at` DATETIME     DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD COLUMN `last_error_code` VARCHAR(32)  NOT NULL DEFAULT '';

ALTER TABLE `0_approval_notifications` ADD COLUMN `dead_lettered_at` DATETIME    DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD COLUMN `sent_date`       DATETIME     DEFAULT NULL;

ALTER TABLE `0_approval_notifications` ADD UNIQUE KEY `approval_notification_lease_uq` (`lease_token`);

ALTER TABLE `0_approval_notifications` ADD KEY `approval_notification_delivery_ready` (`is_sent`, `dead_lettered_at`, `next_attempt_at`, `lease_expires_at`, `created_date`, `id`);

ALTER TABLE `0_approval_notifications` ADD KEY `approval_notification_dead_letter` (`dead_lettered_at`, `id`);

INSERT INTO `0_pay_ctry_capability_registry_versions` (`registry_version`,`registry_sha256`,`created_by`,`created_at`) SELECT 'pay-ctry-capabilities-v1','95d41321a98412df4df289b0ac4df9dcabed417d618bbdb77086ea0fba7237bc',0,'2026-09-23 00:00:00' WHERE NOT EXISTS (SELECT 1 FROM `0_pay_ctry_capability_registry_versions` WHERE `registry_version`='pay-ctry-capabilities-v1');

INSERT INTO `0_pay_ctry_capability_definitions` (`registry_version_id`,`capability_code`,`required_constraints_json`,`allowed_constraints_json`,`exclusive_group`,`allowed_artifact_types_json`,`definition_sha256`,`created_at`) SELECT v.registry_version_id,'core.country_pack.metadata','[]','["schema"]',NULL,'["declarative-json"]','0155864a5ee53ee39a2b33c6242c208ef66857e5dd224d314469d41563fb0305','2026-09-23 00:00:00' FROM `0_pay_ctry_capability_registry_versions` v WHERE v.registry_version='pay-ctry-capabilities-v1' AND NOT EXISTS (SELECT 1 FROM `0_pay_ctry_capability_definitions` d WHERE d.registry_version_id=v.registry_version_id AND d.capability_code='core.country_pack.metadata');

INSERT INTO `0_pay_ctry_capability_definitions` (`registry_version_id`,`capability_code`,`required_constraints_json`,`allowed_constraints_json`,`exclusive_group`,`allowed_artifact_types_json`,`definition_sha256`,`created_at`) SELECT v.registry_version_id,'core.country_pack.evidence','[]','["kind"]',NULL,'["evidence-json","test-fixture-json"]','78965aaf820f59e47d2103be76206968d64efa27f0f4eefd640b1ab08ec6b150','2026-09-23 00:00:00' FROM `0_pay_ctry_capability_registry_versions` v WHERE v.registry_version='pay-ctry-capabilities-v1' AND NOT EXISTS (SELECT 1 FROM `0_pay_ctry_capability_definitions` d WHERE d.registry_version_id=v.registry_version_id AND d.capability_code='core.country_pack.evidence');

INSERT INTO `0_pay_ctry_capability_contract_versions` (`registry_version`,`registry_sha256`,`signature_scheme`,`signed_by`,`signature_sha256`,`host_min_version`,`host_max_version`,`status`,`created_by`,`created_at`) VALUES ('pay-ctry-capabilities-v2','3a1e4f1d0aa1f24d4b0d501f3fb0d3d0e6a86f76f3b2a8b3c4d5e6f708192a3b','site-trust-ed25519-v1','pay-ctry-003-governed-registry','153179ed5ac0ac0eb6364239e5560316bbd8d1ad38794f6e0db9531f7955d2f2','1.0','1.0','active',0,NOW());

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.metadata','jurisdiction','metadata','one','jurisdiction.metadata','declarative-json','core.metadata.v1','1.0','11fdba31ede27e41a6382dd16fd844e11f78d419cb4f3835ae43ff5b8c400d26',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.variables','jurisdiction','variables','one','jurisdiction.variables','declarative-json','formula.variables.v1','1.0','6cb55c9bd0025d946437552b8e4ac3dce644a01224c9658a2ee1a177bd186226',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.rules','jurisdiction','rules','many',NULL,'declarative-json','rules.declarative.v1','1.0','dfb01d1e0987eb5281e1ee3ff30bdfe0fd04cb9d1b94494edef28998de9991eb',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.decisions_rates','jurisdiction','decisions_rates','many',NULL,'declarative-json','decisions.rates.v1','1.0','9004cbbaa4ce9368f6cb548e640e41a4d249e54cd5d9f5e0c71b7c5f02e80db3',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.balances','jurisdiction','balances','many',NULL,'declarative-json','balances.ledger.v1','1.0','13dc4a6265ca4bca31bab7c144ffd0993165e212d08385d41cd79c357b5002c4',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.validators','jurisdiction','validators','many',NULL,'declarative-json','validators.declarative.v1','1.0','afee5e3e6d1e19d8634013ebc1ad353c3f7769e6d37b8ff45dba2f24608d36bc',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.payslips','jurisdiction','payslips','one','jurisdiction.payslips','declarative-json','payslips.projection.v1','1.0','1db62fd255986a34ab00975e2a784f33b59dba6e1c2b25ced020a2ea667c01e9',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.filings','jurisdiction','filings','many',NULL,'declarative-json','filings.declarative.v1','1.0','8ff9803d9b34a673d5046ce2b8cf11232b4bd189f70b48972a4e1032f4532332',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.bank_files','jurisdiction','bank_files','many',NULL,'declarative-json','bank-files.renderer.v1','1.0','073bce7305b06d309519aa7d66386319e1f19db3b66c1670f4764bc1a83b4317',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.translations','jurisdiction','translations','many',NULL,'translation-xliff','translations.xliff.v1','1.0','638a517e230c41ebcf504e72775159b5f073d6ecf2509bd3d43b7546b7c461a6',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.migrations','jurisdiction','migrations','many',NULL,'declarative-json','migrations.review.v1','1.0','3304726fdb5e22e329d3a0217c132b6caca56ace8fc013197bc1ed4e95a3fc8f',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_capability_contract_definitions` (`registry_version_id`,`capability_code`,`namespace`,`semantic`,`cardinality`,`exclusive_group`,`artifact_type`,`host_adapter`,`host_version`,`definition_sha256`,`created_at`) SELECT `registry_version_id`,'jurisdiction.tests','jurisdiction','tests','many',NULL,'test-fixture-json','tests.conformance.v1','1.0','74aaf06c3c2458c9beddd6db01f7c03846e54b322ba3810516d8dec64efaaa58',NOW() FROM `0_pay_ctry_capability_contract_versions` WHERE `registry_version`='pay-ctry-capabilities-v2';

INSERT INTO `0_pay_ctry_host_adapters` (`adapter_code`,`adapter_version`,`media_types_json`,`adapter_sha256`,`status`,`created_at`) VALUES ('core.metadata.v1','1.0','["declarative-json"]','9b83f2f9e4d93b4788f11f1330bb15570fcdd70d71799eee2b0082ff5e8aec9b','active',NOW()),('formula.variables.v1','1.0','["declarative-json"]','83c4c6dc5392a3cec3d91b1e0105c8a7b0526c721a336a0eaaa2cfc8c1721025','active',NOW()),('rules.declarative.v1','1.0','["declarative-json"]','93e3ef03c1563a811581cae5d0a3e1254b4611c7b513cdad1b22f9a230d7cbc6','active',NOW()),('decisions.rates.v1','1.0','["declarative-json"]','219db4616ec0e955c5fc2ae0e227c903c4aa8a47432c7596fd09c05db3d25eee','active',NOW()),('balances.ledger.v1','1.0','["declarative-json"]','1fbe7da8a06802d7bd5bceaf12640549d910ec78d3f2d9d106eb9d4f462c0eef','active',NOW()),('validators.declarative.v1','1.0','["declarative-json"]','7aeeebc292abc5f3caa83d8d49929e04f193608b36b63d0e5034f1f05a69a888','active',NOW()),('payslips.projection.v1','1.0','["declarative-json"]','0416d7b37251d43dfcc2c3c3f412f577821597465c2d2eb0926833110daf47f8','active',NOW()),('filings.declarative.v1','1.0','["declarative-json"]','861adebd945d285ce09ba6f036125007300fce85e9fed171779b62d0e868bc38','active',NOW()),('bank-files.renderer.v1','1.0','["declarative-json","declarative-csv"]','57afcdde5b667bd35e1a7d6c9ba8a90608912ff8d3e2b03a0b538040c09df6e4','active',NOW()),('translations.xliff.v1','1.0','["translation-xliff"]','9c4df2c1b35f36ea730c62a4fa0e723d0ae56e9726b914efca3c76206f0233ce','active',NOW()),('migrations.review.v1','1.0','["declarative-json"]','f487248eec08bf014988cf34e4f3bf454c5a0d0cfb6475d1c57db14ba2cf8c13','active',NOW()),('tests.conformance.v1','1.0','["test-fixture-json"]','601788eeceaba22ea02f8da559f92db486995f9b4892cff7a22aa0d241b98836','active',NOW());

ALTER TABLE `0_hrm_pay_pmt_001_payment_items`
 ADD UNIQUE KEY `uq_ppm1_it_result` (`source_result_sha256`);

CREATE TRIGGER `0_domain_audit_events_no_update` BEFORE UPDATE ON `0_domain_audit_events`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 append-only evidence cannot be updated';

CREATE TRIGGER `0_domain_audit_events_no_delete` BEFORE DELETE ON `0_domain_audit_events`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 append-only evidence cannot be deleted';

CREATE TRIGGER `0_domain_audit_outbox_no_update` BEFORE UPDATE ON `0_domain_audit_outbox`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 append-only evidence cannot be updated';

CREATE TRIGGER `0_domain_audit_outbox_no_delete` BEFORE DELETE ON `0_domain_audit_outbox`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 append-only evidence cannot be deleted';

CREATE TRIGGER `0_domain_audit_checkpoints_no_update` BEFORE UPDATE ON `0_domain_audit_checkpoints`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 append-only evidence cannot be updated';

CREATE TRIGGER `0_domain_audit_checkpoints_no_delete` BEFORE DELETE ON `0_domain_audit_checkpoints`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 append-only evidence cannot be deleted';

CREATE TRIGGER `0_domain_audit_export_receipts_no_update` BEFORE UPDATE ON `0_domain_audit_export_receipts`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 custody receipts are append-only';

CREATE TRIGGER `0_domain_audit_export_receipts_no_delete` BEFORE DELETE ON `0_domain_audit_export_receipts`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AUD-001 custody receipts are append-only';

CREATE SQL SECURITY DEFINER VIEW `0_domain_audit_chain_state_read_v1` AS
SELECT `chain_scope`, `last_sequence`, `last_event_hash`
FROM `0_domain_audit_chain_state`;

CREATE SQL SECURITY DEFINER VIEW `0_domain_audit_genesis_checkpoint_read_v1` AS
SELECT `chain_scope`, `chain_sequence`, `event_hash`, `event_count`, `key_id`, `signature`, `created_at`
FROM `0_domain_audit_checkpoints`
WHERE `chain_sequence` = 0;

CREATE SQL SECURITY DEFINER VIEW `0_domain_audit_review_v1` AS
SELECT `event_id`, `chain_scope`, `chain_sequence`, `occurred_at`,
       `actor_user_id`, `company_id`, `legal_entity_id`, `entity_type`,
       `action`, `reason_code`, `correlation_id`, `previous_event_hash`,
       `event_hash`, `key_id`
FROM `0_domain_audit_events`;

DELIMITER $$
CREATE PROCEDURE `0_domain_audit_chain_initialize_v1`(
  IN p_chain_scope varchar(96),
  IN p_zero_hash char(64)
)
MODIFIES SQL DATA
SQL SECURITY DEFINER
INSERT IGNORE INTO `0_domain_audit_chain_state`
  (`chain_scope`, `last_sequence`, `last_event_hash`, `updated_at`)
SELECT p_chain_scope, 0, p_zero_hash, CURRENT_TIMESTAMP
WHERE p_chain_scope REGEXP '^company:[[:digit:]]+:legal_entity:[[:digit:]]+$'
  AND p_zero_hash REGEXP '^[a-f0-9]{64}$'$$
DELIMITER ;

DELIMITER $$
CREATE PROCEDURE `0_domain_audit_event_insert_v1`(
  IN p_chain_scope varchar(96),
  IN p_chain_sequence bigint unsigned,
  IN p_occurred_at datetime,
  IN p_actor_user_id int unsigned,
  IN p_security_context_hash char(64),
  IN p_company_id int unsigned,
  IN p_legal_entity_id bigint unsigned,
  IN p_entity_type varchar(64),
  IN p_entity_key_hash char(64),
  IN p_action varchar(64),
  IN p_reason_code varchar(64),
  IN p_correlation_id char(32),
  IN p_legacy_source varchar(64),
  IN p_legacy_id bigint unsigned,
  IN p_before_hash char(64),
  IN p_after_hash char(64),
  IN p_restricted_diff text,
  IN p_previous_event_hash char(64),
  IN p_event_hash char(64),
  IN p_key_id char(16)
)
MODIFIES SQL DATA
SQL SECURITY DEFINER
INSERT INTO `0_domain_audit_events`
  (`chain_scope`, `chain_sequence`, `occurred_at`, `actor_user_id`,
   `security_context_hash`, `company_id`, `legal_entity_id`, `entity_type`,
   `entity_key_hash`, `action`, `reason_code`, `correlation_id`,
   `legacy_source`, `legacy_id`, `before_hash`, `after_hash`,
   `restricted_diff`, `previous_event_hash`, `event_hash`, `key_id`)
SELECT
  p_chain_scope, p_chain_sequence, p_occurred_at, p_actor_user_id,
  p_security_context_hash, p_company_id, p_legal_entity_id, p_entity_type,
  p_entity_key_hash, p_action, p_reason_code, p_correlation_id,
  p_legacy_source, p_legacy_id, p_before_hash, p_after_hash,
  p_restricted_diff, p_previous_event_hash, p_event_hash, p_key_id
WHERE p_chain_scope = CONCAT('company:', p_company_id, ':legal_entity:', p_legal_entity_id)
  AND p_chain_sequence > 0
  AND p_actor_user_id <= 65535
  AND p_entity_type REGEXP '^[A-Za-z0-9_.:-]{1,64}$'
  AND p_action REGEXP '^[A-Za-z0-9_.:-]{1,64}$'
  AND p_reason_code REGEXP '^[A-Za-z0-9_.:-]{1,64}$'
  AND (p_legacy_source = '' OR p_legacy_source REGEXP '^[A-Za-z0-9_.:-]{1,64}$')
  AND p_security_context_hash REGEXP '^[a-f0-9]{64}$'
  AND p_entity_key_hash REGEXP '^[a-f0-9]{64}$'
  AND (p_before_hash IS NULL OR p_before_hash REGEXP '^[a-f0-9]{64}$')
  AND (p_after_hash IS NULL OR p_after_hash REGEXP '^[a-f0-9]{64}$')
  AND p_previous_event_hash REGEXP '^[a-f0-9]{64}$'
  AND p_event_hash REGEXP '^[a-f0-9]{64}$'
  AND p_correlation_id REGEXP '^[a-f0-9]{32}$'
  AND p_key_id REGEXP '^[a-f0-9]{16}$'
  AND OCTET_LENGTH(p_restricted_diff) <= 8192$$
DELIMITER ;

DELIMITER $$
CREATE PROCEDURE `0_domain_audit_outbox_insert_v1`(
  IN p_event_id bigint unsigned,
  IN p_chain_scope varchar(96),
  IN p_chain_sequence bigint unsigned,
  IN p_payload longtext,
  IN p_payload_hash char(64)
)
MODIFIES SQL DATA
SQL SECURITY DEFINER
INSERT INTO `0_domain_audit_outbox`
  (`event_id`, `chain_scope`, `chain_sequence`, `payload`, `payload_hash`, `created_at`)
SELECT p_event_id, p_chain_scope, p_chain_sequence, p_payload, p_payload_hash, CURRENT_TIMESTAMP
WHERE p_event_id > 0
  AND p_chain_scope REGEXP '^company:[[:digit:]]+:legal_entity:[[:digit:]]+$'
  AND p_chain_sequence > 0
  AND OCTET_LENGTH(p_payload) <= 32768
  AND p_payload_hash = SHA2(p_payload, 256)$$
DELIMITER ;

DELIMITER $$
CREATE PROCEDURE `0_domain_audit_checkpoint_insert_v1`(
  IN p_chain_scope varchar(96),
  IN p_chain_sequence bigint unsigned,
  IN p_event_hash char(64),
  IN p_event_count bigint unsigned,
  IN p_key_id char(16),
  IN p_signature char(64),
  IN p_created_at datetime
)
MODIFIES SQL DATA
SQL SECURITY DEFINER
INSERT INTO `0_domain_audit_checkpoints`
  (`chain_scope`, `chain_sequence`, `event_hash`, `event_count`, `key_id`, `signature`, `created_at`)
SELECT p_chain_scope, p_chain_sequence, p_event_hash, p_event_count, p_key_id, p_signature, p_created_at
WHERE p_chain_scope REGEXP '^company:[[:digit:]]+:legal_entity:[[:digit:]]+$'
  AND p_chain_sequence = p_event_count
  AND p_event_hash REGEXP '^[a-f0-9]{64}$'
  AND p_key_id REGEXP '^[a-f0-9]{16}$'
  AND p_signature REGEXP '^[a-f0-9]{64}$'$$
DELIMITER ;

DELIMITER $$
CREATE PROCEDURE `0_domain_audit_chain_advance_v1`(
  IN p_chain_scope varchar(96),
  IN p_previous_sequence bigint unsigned,
  IN p_previous_hash char(64),
  IN p_chain_sequence bigint unsigned,
  IN p_event_hash char(64)
)
MODIFIES SQL DATA
SQL SECURITY DEFINER
UPDATE `0_domain_audit_chain_state`
SET `last_sequence` = p_chain_sequence,
    `last_event_hash` = p_event_hash,
    `updated_at` = CURRENT_TIMESTAMP
WHERE BINARY `chain_scope` = BINARY p_chain_scope
  AND `last_sequence` = p_previous_sequence
  AND BINARY `last_event_hash` = BINARY p_previous_hash
  AND p_chain_sequence = p_previous_sequence + 1
  AND p_previous_hash REGEXP '^[a-f0-9]{64}$'
  AND p_event_hash REGEXP '^[a-f0-9]{64}$'$$
DELIMITER ;

CREATE TRIGGER `0_payroll_reversal_evidence_no_update`
BEFORE UPDATE ON `0_payroll_reversal_evidence`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-011 payroll reversal evidence is append-only';

CREATE TRIGGER `0_payroll_reversal_evidence_no_delete`
BEFORE DELETE ON `0_payroll_reversal_evidence`
FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-011 payroll reversal evidence is append-only';

CREATE TRIGGER `0_hrm_pay_rule_artifacts_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies update on hrm_pay_rule_artifacts';

CREATE TRIGGER `0_hrm_pay_rule_artifacts_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies delete on hrm_pay_rule_artifacts';

CREATE TRIGGER `0_hrm_pay_rule_artifact_versions_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_artifact_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies update on hrm_pay_rule_artifact_versions';

CREATE TRIGGER `0_hrm_pay_rule_artifact_versions_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_artifact_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies delete on hrm_pay_rule_artifact_versions';

CREATE TRIGGER `0_hrm_pay_rule_import_candidates_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_import_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies update on hrm_pay_rule_import_candidates';

CREATE TRIGGER `0_hrm_pay_rule_import_candidates_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_import_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies delete on hrm_pay_rule_import_candidates';

CREATE TRIGGER `0_hrm_pay_rule_import_reviews_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_import_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies update on hrm_pay_rule_import_reviews';

CREATE TRIGGER `0_hrm_pay_rule_import_reviews_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_import_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies delete on hrm_pay_rule_import_reviews';

CREATE TRIGGER `0_hrm_pay_rule_import_receipts_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_import_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies update on hrm_pay_rule_import_receipts';

CREATE TRIGGER `0_hrm_pay_rule_import_receipts_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_import_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-003 immutable custody denies delete on hrm_pay_rule_import_receipts';

CREATE TRIGGER `0_hrm_pay_rule_rules_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_rules` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies update on hrm_pay_rule_rules';

CREATE TRIGGER `0_hrm_pay_rule_rules_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_rules` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies delete on hrm_pay_rule_rules';

CREATE TRIGGER `0_hrm_pay_rule_rule_versions_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_rule_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies update on hrm_pay_rule_rule_versions';

CREATE TRIGGER `0_hrm_pay_rule_rule_versions_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_rule_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies delete on hrm_pay_rule_rule_versions';

CREATE TRIGGER `0_hrm_pay_rule_regulatory_evidence_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_regulatory_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies update on hrm_pay_rule_regulatory_evidence';

CREATE TRIGGER `0_hrm_pay_rule_regulatory_evidence_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_regulatory_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies delete on hrm_pay_rule_regulatory_evidence';

CREATE TRIGGER `0_hrm_pay_rule_lifecycle_reviews_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_lifecycle_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies update on hrm_pay_rule_lifecycle_reviews';

CREATE TRIGGER `0_hrm_pay_rule_lifecycle_reviews_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_lifecycle_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies delete on hrm_pay_rule_lifecycle_reviews';

CREATE TRIGGER `0_hrm_pay_rule_lifecycle_publications_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_lifecycle_publications` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies update on hrm_pay_rule_lifecycle_publications';

CREATE TRIGGER `0_hrm_pay_rule_lifecycle_publications_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_lifecycle_publications` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies delete on hrm_pay_rule_lifecycle_publications';

CREATE TRIGGER `0_hrm_pay_rule_lifecycle_events_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_lifecycle_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies update on hrm_pay_rule_lifecycle_events';

CREATE TRIGGER `0_hrm_pay_rule_lifecycle_events_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_lifecycle_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies delete on hrm_pay_rule_lifecycle_events';

CREATE TRIGGER `0_hrm_pay_rule_emergency_changes_immutable_update` BEFORE UPDATE ON `0_hrm_pay_rule_emergency_changes` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies update on hrm_pay_rule_emergency_changes';

CREATE TRIGGER `0_hrm_pay_rule_emergency_changes_immutable_delete` BEFORE DELETE ON `0_hrm_pay_rule_emergency_changes` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-004 immutable custody denies delete on hrm_pay_rule_emergency_changes';

CREATE TRIGGER `0_pr5_j_u` BEFORE UPDATE ON `0_hrm_pay_rule_jurisdictions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_jurisdictions';

CREATE TRIGGER `0_pr5_j_d` BEFORE DELETE ON `0_hrm_pay_rule_jurisdictions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_jurisdictions';

CREATE TRIGGER `0_pr5_jv_u` BEFORE UPDATE ON `0_hrm_pay_rule_jurisdiction_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_jurisdiction_versions';

CREATE TRIGGER `0_pr5_jv_d` BEFORE DELETE ON `0_hrm_pay_rule_jurisdiction_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_jurisdiction_versions';

CREATE TRIGGER `0_pr5_b_u` BEFORE UPDATE ON `0_hrm_pay_rule_subject_jurisdiction_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_subject_jurisdiction_bindings';

CREATE TRIGGER `0_pr5_b_d` BEFORE DELETE ON `0_hrm_pay_rule_subject_jurisdiction_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_subject_jurisdiction_bindings';

CREATE TRIGGER `0_pr5_fs_u` BEFORE UPDATE ON `0_hrm_pay_rule_jurisdiction_fact_sets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_jurisdiction_fact_sets';

CREATE TRIGGER `0_pr5_fs_d` BEFORE DELETE ON `0_hrm_pay_rule_jurisdiction_fact_sets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_jurisdiction_fact_sets';

CREATE TRIGGER `0_pr5_fi_u` BEFORE UPDATE ON `0_hrm_pay_rule_jurisdiction_fact_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_jurisdiction_fact_items';

CREATE TRIGGER `0_pr5_fi_d` BEFORE DELETE ON `0_hrm_pay_rule_jurisdiction_fact_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_jurisdiction_fact_items';

CREATE TRIGGER `0_pr5_a_u` BEFORE UPDATE ON `0_hrm_pay_rule_version_applicability` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_version_applicability';

CREATE TRIGGER `0_pr5_a_d` BEFORE DELETE ON `0_hrm_pay_rule_version_applicability` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_version_applicability';

CREATE TRIGGER `0_pr5_rr_u` BEFORE UPDATE ON `0_hrm_pay_rule_resolution_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_resolution_requests';

CREATE TRIGGER `0_pr5_rr_d` BEFORE DELETE ON `0_hrm_pay_rule_resolution_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_resolution_requests';

CREATE TRIGGER `0_pr5_rc_u` BEFORE UPDATE ON `0_hrm_pay_rule_resolution_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_resolution_candidates';

CREATE TRIGGER `0_pr5_rc_d` BEFORE DELETE ON `0_hrm_pay_rule_resolution_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_resolution_candidates';

CREATE TRIGGER `0_pr5_rd_u` BEFORE UPDATE ON `0_hrm_pay_rule_resolution_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies update on hrm_pay_rule_resolution_decisions';

CREATE TRIGGER `0_pr5_rd_d` BEFORE DELETE ON `0_hrm_pay_rule_resolution_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-005 immutable custody denies delete on hrm_pay_rule_resolution_decisions';

CREATE TRIGGER `0_pr6_cand_u` BEFORE UPDATE ON `0_hrm_pay_rule_migration_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_migration_candidates';

CREATE TRIGGER `0_pr6_cand_d` BEFORE DELETE ON `0_hrm_pay_rule_migration_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_migration_candidates';

CREATE TRIGGER `0_pr6_except_u` BEFORE UPDATE ON `0_hrm_pay_rule_migration_exceptions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_migration_exceptions';

CREATE TRIGGER `0_pr6_except_d` BEFORE DELETE ON `0_hrm_pay_rule_migration_exceptions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_migration_exceptions';

CREATE TRIGGER `0_pr6_bind_u` BEFORE UPDATE ON `0_hrm_pay_rule_translation_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_translation_bindings';

CREATE TRIGGER `0_pr6_bind_d` BEFORE DELETE ON `0_hrm_pay_rule_translation_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_translation_bindings';

CREATE TRIGGER `0_pr6_tol_u` BEFORE UPDATE ON `0_hrm_pay_rule_tolerance_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_tolerance_policies';

CREATE TRIGGER `0_pr6_tol_d` BEFORE DELETE ON `0_hrm_pay_rule_tolerance_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_tolerance_policies';

CREATE TRIGGER `0_pr6_shrun_u` BEFORE UPDATE ON `0_hrm_pay_rule_shadow_runs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_shadow_runs';

CREATE TRIGGER `0_pr6_shrun_d` BEFORE DELETE ON `0_hrm_pay_rule_shadow_runs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_shadow_runs';

CREATE TRIGGER `0_pr6_shcmp_u` BEFORE UPDATE ON `0_hrm_pay_rule_shadow_comparisons` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_shadow_comparisons';

CREATE TRIGGER `0_pr6_shcmp_d` BEFORE DELETE ON `0_hrm_pay_rule_shadow_comparisons` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_shadow_comparisons';

CREATE TRIGGER `0_pr6_elig_u` BEFORE UPDATE ON `0_hrm_pay_rule_activation_eligibility` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_activation_eligibility';

CREATE TRIGGER `0_pr6_elig_d` BEFORE DELETE ON `0_hrm_pay_rule_activation_eligibility` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_activation_eligibility';

CREATE TRIGGER `0_pr6_cmd_u` BEFORE UPDATE ON `0_hrm_pay_rule_activation_commands` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies update on hrm_pay_rule_activation_commands';

CREATE TRIGGER `0_pr6_cmd_d` BEFORE DELETE ON `0_hrm_pay_rule_activation_commands` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-RULE-006 immutable custody denies delete on hrm_pay_rule_activation_commands';

CREATE TRIGGER `0_pc2_cal_u` BEFORE UPDATE ON `0_hrm_payroll_calendars` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_calendars';

CREATE TRIGGER `0_pc2_cal_d` BEFORE DELETE ON `0_hrm_payroll_calendars` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_calendars';

CREATE TRIGGER `0_pc2_calv_u` BEFORE UPDATE ON `0_hrm_payroll_calendar_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_calendar_versions';

CREATE TRIGGER `0_pc2_calv_d` BEFORE DELETE ON `0_hrm_payroll_calendar_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_calendar_versions';

CREATE TRIGGER `0_pc2_per_u` BEFORE UPDATE ON `0_hrm_payroll_calendar_periods` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_calendar_periods';

CREATE TRIGGER `0_pc2_per_d` BEFORE DELETE ON `0_hrm_payroll_calendar_periods` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_calendar_periods';

CREATE TRIGGER `0_pc2_grp_u` BEFORE UPDATE ON `0_hrm_payroll_groups` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_groups';

CREATE TRIGGER `0_pc2_grp_d` BEFORE DELETE ON `0_hrm_payroll_groups` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_groups';

CREATE TRIGGER `0_pc2_grpv_u` BEFORE UPDATE ON `0_hrm_payroll_group_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_group_versions';

CREATE TRIGGER `0_pc2_grpv_d` BEFORE DELETE ON `0_hrm_payroll_group_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_group_versions';

CREATE TRIGGER `0_pc2_rel_u` BEFORE UPDATE ON `0_hrm_payroll_relationships` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_relationships';

CREATE TRIGGER `0_pc2_rel_d` BEFORE DELETE ON `0_hrm_payroll_relationships` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_relationships';

CREATE TRIGGER `0_pc2_relm_u` BEFORE UPDATE ON `0_hrm_payroll_relationship_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_relationship_members';

CREATE TRIGGER `0_pc2_relm_d` BEFORE DELETE ON `0_hrm_payroll_relationship_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_relationship_members';

CREATE TRIGGER `0_pc2_enr_u` BEFORE UPDATE ON `0_hrm_payroll_group_enrollments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_group_enrollments';

CREATE TRIGGER `0_pc2_enr_d` BEFORE DELETE ON `0_hrm_payroll_group_enrollments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_group_enrollments';

CREATE TRIGGER `0_pc2m_lpmr_u` BEFORE UPDATE ON `0_hrm_payroll_legacy_period_mapping_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 legacy mapping review custody denies update';

CREATE TRIGGER `0_pc2m_lpmr_d` BEFORE DELETE ON `0_hrm_payroll_legacy_period_mapping_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 legacy mapping review custody denies delete';

CREATE TRIGGER `0_pc2_sdp_u` BEFORE UPDATE ON `0_hrm_payroll_seed_proposals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_seed_proposals';

CREATE TRIGGER `0_pc2_sdp_d` BEFORE DELETE ON `0_hrm_payroll_seed_proposals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_seed_proposals';

CREATE TRIGGER `0_pc2_sda_u` BEFORE UPDATE ON `0_hrm_payroll_seed_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_seed_approvals';

CREATE TRIGGER `0_pc2_sda_d` BEFORE DELETE ON `0_hrm_payroll_seed_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_seed_approvals';

CREATE TRIGGER `0_pc2_sdx_u` BEFORE UPDATE ON `0_hrm_payroll_seed_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_seed_executions';

CREATE TRIGGER `0_pc2_sdx_d` BEFORE DELETE ON `0_hrm_payroll_seed_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_seed_executions';

CREATE TRIGGER `0_pc2_mpl_u` BEFORE UPDATE ON `0_hrm_payroll_legacy_period_mapping_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_legacy_period_mapping_plans';

CREATE TRIGGER `0_pc2_mpl_d` BEFORE DELETE ON `0_hrm_payroll_legacy_period_mapping_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_legacy_period_mapping_plans';

CREATE TRIGGER `0_pc2_map_u` BEFORE UPDATE ON `0_hrm_payroll_legacy_period_mapping_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_legacy_period_mapping_approvals';

CREATE TRIGGER `0_pc2_map_d` BEFORE DELETE ON `0_hrm_payroll_legacy_period_mapping_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_legacy_period_mapping_approvals';

CREATE TRIGGER `0_pc2_cvr_u` BEFORE UPDATE ON `0_hrm_payroll_legacy_period_conversion_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_legacy_period_conversion_receipts';

CREATE TRIGGER `0_pc2_cvr_d` BEFORE DELETE ON `0_hrm_payroll_legacy_period_conversion_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_legacy_period_conversion_receipts';

CREATE TRIGGER `0_pc2_rcn_u` BEFORE UPDATE ON `0_hrm_payroll_legacy_period_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_legacy_period_reconciliations';

CREATE TRIGGER `0_pc2_rcn_d` BEFORE DELETE ON `0_hrm_payroll_legacy_period_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_legacy_period_reconciliations';

CREATE TRIGGER `0_pc2_rmp_u` BEFORE UPDATE ON `0_hrm_payroll_relationship_migration_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_relationship_migration_plans';

CREATE TRIGGER `0_pc2_rmp_d` BEFORE DELETE ON `0_hrm_payroll_relationship_migration_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_relationship_migration_plans';

CREATE TRIGGER `0_pc2_rma_u` BEFORE UPDATE ON `0_hrm_payroll_relationship_migration_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_relationship_migration_approvals';

CREATE TRIGGER `0_pc2_rma_d` BEFORE DELETE ON `0_hrm_payroll_relationship_migration_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_relationship_migration_approvals';

CREATE TRIGGER `0_pc2_rmx_u` BEFORE UPDATE ON `0_hrm_payroll_relationship_migration_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies update on hrm_payroll_relationship_migration_receipts';

CREATE TRIGGER `0_pc2_rmx_d` BEFORE DELETE ON `0_hrm_payroll_relationship_migration_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-002 immutable custody denies delete on hrm_payroll_relationship_migration_receipts';

CREATE TRIGGER `0_pc3_elem_u` BEFORE UPDATE ON `0_hrm_pay_elements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_elements';

CREATE TRIGGER `0_pc3_elem_d` BEFORE DELETE ON `0_hrm_pay_elements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_elements';

CREATE TRIGGER `0_pc3_elemv_u` BEFORE UPDATE ON `0_hrm_pay_element_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_element_versions';

CREATE TRIGGER `0_pc3_elemv_d` BEFORE DELETE ON `0_hrm_pay_element_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_element_versions';

CREATE TRIGGER `0_pc3_indef_u` BEFORE UPDATE ON `0_hrm_pay_input_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_input_definitions';

CREATE TRIGGER `0_pc3_indef_d` BEFORE DELETE ON `0_hrm_pay_input_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_input_definitions';

CREATE TRIGGER `0_pc3_eligv_u` BEFORE UPDATE ON `0_hrm_pay_eligibility_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_eligibility_versions';

CREATE TRIGGER `0_pc3_eligv_d` BEFORE DELETE ON `0_hrm_pay_eligibility_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_eligibility_versions';

CREATE TRIGGER `0_pc3_apin_u` BEFORE UPDATE ON `0_hrm_payroll_approved_inputs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_payroll_approved_inputs';

CREATE TRIGGER `0_pc3_apin_d` BEFORE DELETE ON `0_hrm_payroll_approved_inputs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_payroll_approved_inputs';

CREATE TRIGGER `0_pc3m_lsmr_u` BEFORE UPDATE ON `0_hrm_pay_core_003_legacy_mapping_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 legacy mapping review custody denies update';

CREATE TRIGGER `0_pc3m_lsmr_d` BEFORE DELETE ON `0_hrm_pay_core_003_legacy_mapping_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 legacy mapping review custody denies delete';

CREATE TRIGGER `0_pc3_mpl_u` BEFORE UPDATE ON `0_hrm_pay_core_003_legacy_mapping_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_core_003_legacy_mapping_plans';

CREATE TRIGGER `0_pc3_mpl_d` BEFORE DELETE ON `0_hrm_pay_core_003_legacy_mapping_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_core_003_legacy_mapping_plans';

CREATE TRIGGER `0_pc3_map_u` BEFORE UPDATE ON `0_hrm_pay_core_003_legacy_mapping_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_core_003_legacy_mapping_approvals';

CREATE TRIGGER `0_pc3_map_d` BEFORE DELETE ON `0_hrm_pay_core_003_legacy_mapping_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_core_003_legacy_mapping_approvals';

CREATE TRIGGER `0_pc3_cvr_u` BEFORE UPDATE ON `0_hrm_pay_core_003_legacy_conversion_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_core_003_legacy_conversion_receipts';

CREATE TRIGGER `0_pc3_cvr_d` BEFORE DELETE ON `0_hrm_pay_core_003_legacy_conversion_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_core_003_legacy_conversion_receipts';

CREATE TRIGGER `0_pc3_rcn_u` BEFORE UPDATE ON `0_hrm_pay_core_003_legacy_conversion_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_core_003_legacy_conversion_reconciliations';

CREATE TRIGGER `0_pc3_rcn_d` BEFORE DELETE ON `0_hrm_pay_core_003_legacy_conversion_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_core_003_legacy_conversion_reconciliations';

CREATE TRIGGER `0_pc3_pry_u` BEFORE UPDATE ON `0_hrm_pay_core_003_compatibility_parity_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies update on hrm_pay_core_003_compatibility_parity_evidence';

CREATE TRIGGER `0_pc3_pry_d` BEFORE DELETE ON `0_hrm_pay_core_003_compatibility_parity_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-003 immutable custody denies delete on hrm_pay_core_003_compatibility_parity_evidence';

CREATE TRIGGER `0_pc4_snap_u` BEFORE UPDATE ON `0_hrm_pay_core_004_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_snapshots';

CREATE TRIGGER `0_pc4_snap_d` BEFORE DELETE ON `0_hrm_pay_core_004_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_snapshots';

CREATE TRIGGER `0_pc4_res_u` BEFORE UPDATE ON `0_hrm_pay_core_004_results` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_results';

CREATE TRIGGER `0_pc4_res_d` BEFORE DELETE ON `0_hrm_pay_core_004_results` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_results';

CREATE TRIGGER `0_pc4_line_u` BEFORE UPDATE ON `0_hrm_pay_core_004_result_lines` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_result_lines';

CREATE TRIGGER `0_pc4_line_d` BEFORE DELETE ON `0_hrm_pay_core_004_result_lines` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_result_lines';

CREATE TRIGGER `0_pc4_trace_u` BEFORE UPDATE ON `0_hrm_pay_core_004_traces` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_traces';

CREATE TRIGGER `0_pc4_trace_d` BEFORE DELETE ON `0_hrm_pay_core_004_traces` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_traces';

CREATE TRIGGER `0_pc4_integ_u` BEFORE UPDATE ON `0_hrm_pay_core_004_integrity_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_integrity_evidence';

CREATE TRIGGER `0_pc4_integ_d` BEFORE DELETE ON `0_hrm_pay_core_004_integrity_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_integrity_evidence';

CREATE TRIGGER `0_pc4_lin_u` BEFORE UPDATE ON `0_hrm_pay_core_004_correction_lineage` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_correction_lineage';

CREATE TRIGGER `0_pc4_lin_d` BEFORE DELETE ON `0_hrm_pay_core_004_correction_lineage` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_correction_lineage';

CREATE TRIGGER `0_pc4_lprov_u` BEFORE UPDATE ON `0_hrm_pay_core_004_legacy_provenance_gaps` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_legacy_provenance_gaps';

CREATE TRIGGER `0_pc4_lprov_d` BEFORE DELETE ON `0_hrm_pay_core_004_legacy_provenance_gaps` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_legacy_provenance_gaps';

CREATE TRIGGER `0_pc4_recv_u` BEFORE UPDATE ON `0_hrm_pay_core_004_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_recovery_evidence';

CREATE TRIGGER `0_pc4_recv_d` BEFORE DELETE ON `0_hrm_pay_core_004_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_recovery_evidence';

CREATE TRIGGER `0_pc4_arch_u` BEFORE UPDATE ON `0_hrm_pay_core_004_archive_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies update on hrm_pay_core_004_archive_evidence';

CREATE TRIGGER `0_pc4_arch_d` BEFORE DELETE ON `0_hrm_pay_core_004_archive_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-004 immutable custody denies delete on hrm_pay_core_004_archive_evidence';

CREATE TRIGGER `0_pc6_def_u` BEFORE UPDATE ON `0_hrm_pay_balance_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies update on hrm_pay_balance_definitions';

CREATE TRIGGER `0_pc6_def_d` BEFORE DELETE ON `0_hrm_pay_balance_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies delete on hrm_pay_balance_definitions';

CREATE TRIGGER `0_pc6_mov_u` BEFORE UPDATE ON `0_hrm_pay_balance_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies update on hrm_pay_balance_movements';

CREATE TRIGGER `0_pc6_mov_d` BEFORE DELETE ON `0_hrm_pay_balance_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies delete on hrm_pay_balance_movements';

CREATE TRIGGER `0_pc6_open_u` BEFORE UPDATE ON `0_hrm_pay_balance_opening_inventory` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies update on hrm_pay_balance_opening_inventory';

CREATE TRIGGER `0_pc6_open_d` BEFORE DELETE ON `0_hrm_pay_balance_opening_inventory` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies delete on hrm_pay_balance_opening_inventory';

CREATE TRIGGER `0_pc6_snap_u` BEFORE UPDATE ON `0_hrm_pay_balance_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies update on hrm_pay_balance_snapshots';

CREATE TRIGGER `0_pc6_snap_d` BEFORE DELETE ON `0_hrm_pay_balance_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies delete on hrm_pay_balance_snapshots';

CREATE TRIGGER `0_pc6_recon_u` BEFORE UPDATE ON `0_hrm_pay_balance_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies update on hrm_pay_balance_reconciliations';

CREATE TRIGGER `0_pc6_recon_d` BEFORE DELETE ON `0_hrm_pay_balance_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies delete on hrm_pay_balance_reconciliations';

CREATE TRIGGER `0_pc6_recv_u` BEFORE UPDATE ON `0_hrm_pay_balance_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies update on hrm_pay_balance_recovery_evidence';

CREATE TRIGGER `0_pc6_recv_d` BEFORE DELETE ON `0_hrm_pay_balance_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-006 immutable custody denies delete on hrm_pay_balance_recovery_evidence';

CREATE TRIGGER `0_pc7_evt_u` BEFORE UPDATE ON `0_hrm_pay_retro_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_events';

CREATE TRIGGER `0_pc7_evt_d` BEFORE DELETE ON `0_hrm_pay_retro_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_events';

CREATE TRIGGER `0_pc7_aff_u` BEFORE UPDATE ON `0_hrm_pay_retro_affected_results` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_affected_results';

CREATE TRIGGER `0_pc7_aff_d` BEFORE DELETE ON `0_hrm_pay_retro_affected_results` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_affected_results';

CREATE TRIGGER `0_pc7_ctx_u` BEFORE UPDATE ON `0_hrm_pay_retro_contexts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_contexts';

CREATE TRIGGER `0_pc7_ctx_d` BEFORE DELETE ON `0_hrm_pay_retro_contexts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_contexts';

CREATE TRIGGER `0_pc7_del_u` BEFORE UPDATE ON `0_hrm_pay_retro_deltas` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_deltas';

CREATE TRIGGER `0_pc7_del_d` BEFORE DELETE ON `0_hrm_pay_retro_deltas` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_deltas';

CREATE TRIGGER `0_pc7_arr_u` BEFORE UPDATE ON `0_hrm_pay_retro_arrears` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_arrears';

CREATE TRIGGER `0_pc7_arr_d` BEFORE DELETE ON `0_hrm_pay_retro_arrears` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_arrears';

CREATE TRIGGER `0_pc7_arm_u` BEFORE UPDATE ON `0_hrm_pay_retro_arrears_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_arrears_movements';

CREATE TRIGGER `0_pc7_arm_d` BEFORE DELETE ON `0_hrm_pay_retro_arrears_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_arrears_movements';

CREATE TRIGGER `0_pc7_pln_u` BEFORE UPDATE ON `0_hrm_pay_retro_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_plans';

CREATE TRIGGER `0_pc7_pln_d` BEFORE DELETE ON `0_hrm_pay_retro_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_plans';

CREATE TRIGGER `0_pc7_app_u` BEFORE UPDATE ON `0_hrm_pay_retro_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_approvals';

CREATE TRIGGER `0_pc7_app_d` BEFORE DELETE ON `0_hrm_pay_retro_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_approvals';

CREATE TRIGGER `0_pc7_exe_u` BEFORE UPDATE ON `0_hrm_pay_retro_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_executions';

CREATE TRIGGER `0_pc7_exe_d` BEFORE DELETE ON `0_hrm_pay_retro_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_executions';

CREATE TRIGGER `0_pc7_obl_u` BEFORE UPDATE ON `0_hrm_pay_retro_downstream_obligations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_downstream_obligations';

CREATE TRIGGER `0_pc7_obl_d` BEFORE DELETE ON `0_hrm_pay_retro_downstream_obligations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_downstream_obligations';

CREATE TRIGGER `0_pc7_rec_u` BEFORE UPDATE ON `0_hrm_pay_retro_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_reconciliations';

CREATE TRIGGER `0_pc7_rec_d` BEFORE DELETE ON `0_hrm_pay_retro_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_reconciliations';

CREATE TRIGGER `0_pc7_rcv_u` BEFORE UPDATE ON `0_hrm_pay_retro_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies update on hrm_pay_retro_recovery_evidence';

CREATE TRIGGER `0_pc7_rcv_d` BEFORE DELETE ON `0_hrm_pay_retro_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-007 immutable custody denies delete on hrm_pay_retro_recovery_evidence';

CREATE TRIGGER `0_pc8_vdf_u` BEFORE UPDATE ON `0_hrm_pay_validation_validator_defs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_validator_defs';

CREATE TRIGGER `0_pc8_vdf_d` BEFORE DELETE ON `0_hrm_pay_validation_validator_defs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_validator_defs';

CREATE TRIGGER `0_pc8_thr_u` BEFORE UPDATE ON `0_hrm_pay_validation_threshold_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_threshold_policies';

CREATE TRIGGER `0_pc8_thr_d` BEFORE DELETE ON `0_hrm_pay_validation_threshold_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_threshold_policies';

CREATE TRIGGER `0_pc8_pak_u` BEFORE UPDATE ON `0_hrm_pay_validation_pack_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_pack_bindings';

CREATE TRIGGER `0_pc8_pak_d` BEFORE DELETE ON `0_hrm_pay_validation_pack_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_pack_bindings';

CREATE TRIGGER `0_pc8_run_u` BEFORE UPDATE ON `0_hrm_pay_validation_runs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_runs';

CREATE TRIGGER `0_pc8_run_d` BEFORE DELETE ON `0_hrm_pay_validation_runs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_runs';

CREATE TRIGGER `0_pc8_fnd_u` BEFORE UPDATE ON `0_hrm_pay_validation_findings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_findings';

CREATE TRIGGER `0_pc8_fnd_d` BEFORE DELETE ON `0_hrm_pay_validation_findings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_findings';

CREATE TRIGGER `0_pc8_res_u` BEFORE UPDATE ON `0_hrm_pay_validation_resolutions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_resolutions';

CREATE TRIGGER `0_pc8_res_d` BEFORE DELETE ON `0_hrm_pay_validation_resolutions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_resolutions';

CREATE TRIGGER `0_pc8_sig_u` BEFORE UPDATE ON `0_hrm_pay_validation_signoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_signoffs';

CREATE TRIGGER `0_pc8_sig_d` BEFORE DELETE ON `0_hrm_pay_validation_signoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_signoffs';

CREATE TRIGGER `0_pc8_var_u` BEFORE UPDATE ON `0_hrm_pay_validation_variances` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_variances';

CREATE TRIGGER `0_pc8_var_d` BEFORE DELETE ON `0_hrm_pay_validation_variances` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_variances';

CREATE TRIGGER `0_pc8_rec_u` BEFORE UPDATE ON `0_hrm_pay_validation_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_reconciliations';

CREATE TRIGGER `0_pc8_rec_d` BEFORE DELETE ON `0_hrm_pay_validation_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_reconciliations';

CREATE TRIGGER `0_pc8_rcv_u` BEFORE UPDATE ON `0_hrm_pay_validation_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies update on hrm_pay_validation_recovery_evidence';

CREATE TRIGGER `0_pc8_rcv_d` BEFORE DELETE ON `0_hrm_pay_validation_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-008 immutable custody denies delete on hrm_pay_validation_recovery_evidence';

CREATE TRIGGER `0_pc9_lea_u` BEFORE UPDATE ON `0_hrm_pay_core_009_lease_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies update on hrm_pay_core_009_lease_events';

CREATE TRIGGER `0_pc9_lea_d` BEFORE DELETE ON `0_hrm_pay_core_009_lease_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies delete on hrm_pay_core_009_lease_events';

CREATE TRIGGER `0_pc9_att_u` BEFORE UPDATE ON `0_hrm_pay_core_009_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies update on hrm_pay_core_009_attempts';

CREATE TRIGGER `0_pc9_att_d` BEFORE DELETE ON `0_hrm_pay_core_009_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies delete on hrm_pay_core_009_attempts';

CREATE TRIGGER `0_pc9_chk_u` BEFORE UPDATE ON `0_hrm_pay_core_009_checkpoints` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies update on hrm_pay_core_009_checkpoints';

CREATE TRIGGER `0_pc9_chk_d` BEFORE DELETE ON `0_hrm_pay_core_009_checkpoints` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies delete on hrm_pay_core_009_checkpoints';

CREATE TRIGGER `0_pc9_trs_u` BEFORE UPDATE ON `0_hrm_pay_core_009_transitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies update on hrm_pay_core_009_transitions';

CREATE TRIGGER `0_pc9_trs_d` BEFORE DELETE ON `0_hrm_pay_core_009_transitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies delete on hrm_pay_core_009_transitions';

CREATE TRIGGER `0_pc9_qua_u` BEFORE UPDATE ON `0_hrm_pay_core_009_quarantine` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies update on hrm_pay_core_009_quarantine';

CREATE TRIGGER `0_pc9_qua_d` BEFORE DELETE ON `0_hrm_pay_core_009_quarantine` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies delete on hrm_pay_core_009_quarantine';

CREATE TRIGGER `0_pc9_rcv_u` BEFORE UPDATE ON `0_hrm_pay_core_009_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies update on hrm_pay_core_009_recovery_evidence';

CREATE TRIGGER `0_pc9_rcv_d` BEFORE DELETE ON `0_hrm_pay_core_009_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-009 immutable custody denies delete on hrm_pay_core_009_recovery_evidence';

CREATE TRIGGER `0_pc10_int_u` BEFORE UPDATE ON `0_hrm_pay_core_010_approval_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies update on hrm_pay_core_010_approval_intents';

CREATE TRIGGER `0_pc10_int_d` BEFORE DELETE ON `0_hrm_pay_core_010_approval_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies delete on hrm_pay_core_010_approval_intents';

CREATE TRIGGER `0_pc10_app_u` BEFORE UPDATE ON `0_hrm_pay_core_010_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies update on hrm_pay_core_010_approvals';

CREATE TRIGGER `0_pc10_app_d` BEFORE DELETE ON `0_hrm_pay_core_010_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies delete on hrm_pay_core_010_approvals';

CREATE TRIGGER `0_pc10_prj_u` BEFORE UPDATE ON `0_hrm_pay_core_010_projections` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies update on hrm_pay_core_010_projections';

CREATE TRIGGER `0_pc10_prj_d` BEFORE DELETE ON `0_hrm_pay_core_010_projections` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies delete on hrm_pay_core_010_projections';

CREATE TRIGGER `0_pc10_par_u` BEFORE UPDATE ON `0_hrm_pay_core_010_parity_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies update on hrm_pay_core_010_parity_evidence';

CREATE TRIGGER `0_pc10_par_d` BEFORE DELETE ON `0_hrm_pay_core_010_parity_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies delete on hrm_pay_core_010_parity_evidence';

CREATE TRIGGER `0_pc10_trs_u` BEFORE UPDATE ON `0_hrm_pay_core_010_cutover_transitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies update on hrm_pay_core_010_cutover_transitions';

CREATE TRIGGER `0_pc10_trs_d` BEFORE DELETE ON `0_hrm_pay_core_010_cutover_transitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies delete on hrm_pay_core_010_cutover_transitions';

CREATE TRIGGER `0_pc10_rcv_u` BEFORE UPDATE ON `0_hrm_pay_core_010_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies update on hrm_pay_core_010_recovery_evidence';

CREATE TRIGGER `0_pc10_rcv_d` BEFORE DELETE ON `0_hrm_pay_core_010_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies delete on hrm_pay_core_010_recovery_evidence';

CREATE TRIGGER `0_pc10_use_u` BEFORE UPDATE ON `0_hrm_pay_core_010_usage_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies update on hrm_pay_core_010_usage_events';

CREATE TRIGGER `0_pc10_use_d` BEFORE DELETE ON `0_hrm_pay_core_010_usage_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CORE-010 immutable custody denies delete on hrm_pay_core_010_usage_events';

CREATE TRIGGER `0_tim1_pat_u` BEFORE UPDATE ON `0_hrm_time_work_patterns` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_work_patterns';

CREATE TRIGGER `0_tim1_pat_d` BEFORE DELETE ON `0_hrm_time_work_patterns` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_work_patterns';

CREATE TRIGGER `0_tim1_seg_u` BEFORE UPDATE ON `0_hrm_time_work_pattern_segments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_work_pattern_segments';

CREATE TRIGGER `0_tim1_seg_d` BEFORE DELETE ON `0_hrm_time_work_pattern_segments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_work_pattern_segments';

CREATE TRIGGER `0_tim1_raw_u` BEFORE UPDATE ON `0_hrm_time_raw_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_raw_events';

CREATE TRIGGER `0_tim1_raw_d` BEFORE DELETE ON `0_hrm_time_raw_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_raw_events';

CREATE TRIGGER `0_tim1_cnf_u` BEFORE UPDATE ON `0_hrm_time_raw_event_conflicts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_raw_event_conflicts';

CREATE TRIGGER `0_tim1_cnf_d` BEFORE DELETE ON `0_hrm_time_raw_event_conflicts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_raw_event_conflicts';

CREATE TRIGGER `0_tim1_rev_u` BEFORE UPDATE ON `0_hrm_time_reviewed_entries` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_reviewed_entries';

CREATE TRIGGER `0_tim1_rev_d` BEFORE DELETE ON `0_hrm_time_reviewed_entries` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_reviewed_entries';

CREATE TRIGGER `0_tim1_lnk_u` BEFORE UPDATE ON `0_hrm_time_reviewed_event_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_reviewed_event_links';

CREATE TRIGGER `0_tim1_lnk_d` BEFORE DELETE ON `0_hrm_time_reviewed_event_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_reviewed_event_links';

CREATE TRIGGER `0_tim1_alc_u` BEFORE UPDATE ON `0_hrm_time_reviewed_allocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_reviewed_allocations';

CREATE TRIGGER `0_tim1_alc_d` BEFORE DELETE ON `0_hrm_time_reviewed_allocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_reviewed_allocations';

CREATE TRIGGER `0_tim1_app_u` BEFORE UPDATE ON `0_hrm_time_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_approvals';

CREATE TRIGGER `0_tim1_app_d` BEFORE DELETE ON `0_hrm_time_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_approvals';

CREATE TRIGGER `0_tim1_map_u` BEFORE UPDATE ON `0_hrm_time_payroll_input_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_payroll_input_mappings';

CREATE TRIGGER `0_tim1_map_d` BEFORE DELETE ON `0_hrm_time_payroll_input_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_payroll_input_mappings';

CREATE TRIGGER `0_tim1_rcp_u` BEFORE UPDATE ON `0_hrm_time_payroll_input_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_payroll_input_receipts';

CREATE TRIGGER `0_tim1_rcp_d` BEFORE DELETE ON `0_hrm_time_payroll_input_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_payroll_input_receipts';

CREATE TRIGGER `0_tim1_leg_u` BEFORE UPDATE ON `0_hrm_time_legacy_attendance_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies update on hrm_time_legacy_attendance_receipts';

CREATE TRIGGER `0_tim1_leg_d` BEFORE DELETE ON `0_hrm_time_legacy_attendance_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-001 immutable custody denies delete on hrm_time_legacy_attendance_receipts';

CREATE TRIGGER `0_tim2_pol_u` BEFORE UPDATE ON `0_hrm_time_overtime_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_overtime_policies';

CREATE TRIGGER `0_tim2_pol_d` BEFORE DELETE ON `0_hrm_time_overtime_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_overtime_policies';

CREATE TRIGGER `0_tim2_rul_u` BEFORE UPDATE ON `0_hrm_time_overtime_policy_rules` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_overtime_policy_rules';

CREATE TRIGGER `0_tim2_rul_d` BEFORE DELETE ON `0_hrm_time_overtime_policy_rules` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_overtime_policy_rules';

CREATE TRIGGER `0_tim2_req_u` BEFORE UPDATE ON `0_hrm_time_overtime_request_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_overtime_request_receipts';

CREATE TRIGGER `0_tim2_req_d` BEFORE DELETE ON `0_hrm_time_overtime_request_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_overtime_request_receipts';

CREATE TRIGGER `0_tim2_con_u` BEFORE UPDATE ON `0_hrm_time_import_connectors` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_import_connectors';

CREATE TRIGGER `0_tim2_con_d` BEFORE DELETE ON `0_hrm_time_import_connectors` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_import_connectors';

CREATE TRIGGER `0_tim2_bat_u` BEFORE UPDATE ON `0_hrm_time_import_batches` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_import_batches';

CREATE TRIGGER `0_tim2_bat_d` BEFORE DELETE ON `0_hrm_time_import_batches` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_import_batches';

CREATE TRIGGER `0_tim2_itm_u` BEFORE UPDATE ON `0_hrm_time_import_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_import_items';

CREATE TRIGGER `0_tim2_itm_d` BEFORE DELETE ON `0_hrm_time_import_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_import_items';

CREATE TRIGGER `0_tim2_err_u` BEFORE UPDATE ON `0_hrm_time_import_errors` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_import_errors';

CREATE TRIGGER `0_tim2_err_d` BEFORE DELETE ON `0_hrm_time_import_errors` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_import_errors';

CREATE TRIGGER `0_tim2_qua_u` BEFORE UPDATE ON `0_hrm_time_overtime_qualifications` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_overtime_qualifications';

CREATE TRIGGER `0_tim2_qua_d` BEFORE DELETE ON `0_hrm_time_overtime_qualifications` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_overtime_qualifications';

CREATE TRIGGER `0_tim2_map_u` BEFORE UPDATE ON `0_hrm_time_overtime_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_overtime_payroll_mappings';

CREATE TRIGGER `0_tim2_map_d` BEFORE DELETE ON `0_hrm_time_overtime_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_overtime_payroll_mappings';

CREATE TRIGGER `0_tim2_pay_u` BEFORE UPDATE ON `0_hrm_time_overtime_payroll_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies update on hrm_time_overtime_payroll_receipts';

CREATE TRIGGER `0_tim2_pay_d` BEFORE DELETE ON `0_hrm_time_overtime_payroll_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TIM-002 immutable custody denies delete on hrm_time_overtime_payroll_receipts';

CREATE TRIGGER `0_lve1_pol_u` BEFORE UPDATE ON `0_hrm_leave_policy_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_pol_d` BEFORE DELETE ON `0_hrm_leave_policy_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_ent_u` BEFORE UPDATE ON `0_hrm_leave_entitlements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_ent_d` BEFORE DELETE ON `0_hrm_leave_entitlements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_mov_u` BEFORE UPDATE ON `0_hrm_leave_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_mov_d` BEFORE DELETE ON `0_hrm_leave_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_req_u` BEFORE UPDATE ON `0_hrm_leave_request_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_req_d` BEFORE DELETE ON `0_hrm_leave_request_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_opn_u` BEFORE UPDATE ON `0_hrm_leave_opening_inventory` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_opn_d` BEFORE DELETE ON `0_hrm_leave_opening_inventory` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_prj_u` BEFORE UPDATE ON `0_hrm_leave_projection_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_prj_d` BEFORE DELETE ON `0_hrm_leave_projection_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_rec_u` BEFORE UPDATE ON `0_hrm_leave_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_rec_d` BEFORE DELETE ON `0_hrm_leave_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_rcv_u` BEFORE UPDATE ON `0_hrm_leave_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_rcv_d` BEFORE DELETE ON `0_hrm_leave_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_map_u` BEFORE UPDATE ON `0_hrm_leave_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_map_d` BEFORE DELETE ON `0_hrm_leave_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_lve1_pay_u` BEFORE UPDATE ON `0_hrm_leave_payroll_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies update';

CREATE TRIGGER `0_lve1_pay_d` BEFORE DELETE ON `0_hrm_leave_payroll_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-LVE-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_rng_u` BEFORE UPDATE ON `0_hrm_comp_salary_ranges` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_rng_d` BEFORE DELETE ON `0_hrm_comp_salary_ranges` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_cyc_u` BEFORE UPDATE ON `0_hrm_comp_review_cycles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_cyc_d` BEFORE DELETE ON `0_hrm_comp_review_cycles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_bud_u` BEFORE UPDATE ON `0_hrm_comp_budgets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_bud_d` BEFORE DELETE ON `0_hrm_comp_budgets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_opn_u` BEFORE UPDATE ON `0_hrm_comp_opening_inventory` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_opn_d` BEFORE DELETE ON `0_hrm_comp_opening_inventory` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_opr_u` BEFORE UPDATE ON `0_hrm_comp_opening_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_opr_d` BEFORE DELETE ON `0_hrm_comp_opening_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_cpl_u` BEFORE UPDATE ON `0_hrm_comp_cutover_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_cpl_d` BEFORE DELETE ON `0_hrm_comp_cutover_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_cap_u` BEFORE UPDATE ON `0_hrm_comp_cutover_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_cap_d` BEFORE DELETE ON `0_hrm_comp_cutover_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_crc_u` BEFORE UPDATE ON `0_hrm_comp_cutover_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_crc_d` BEFORE DELETE ON `0_hrm_comp_cutover_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_cmd_d` BEFORE DELETE ON `0_hrm_comp_award_commands` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 command custody denies delete';

CREATE TRIGGER `0_cmp1_awd_u` BEFORE UPDATE ON `0_hrm_comp_award_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_awd_d` BEFORE DELETE ON `0_hrm_comp_award_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_prj_u` BEFORE UPDATE ON `0_hrm_comp_legacy_projection_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_prj_d` BEFORE DELETE ON `0_hrm_comp_legacy_projection_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_map_u` BEFORE UPDATE ON `0_hrm_comp_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_map_d` BEFORE DELETE ON `0_hrm_comp_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_pay_u` BEFORE UPDATE ON `0_hrm_comp_payroll_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_pay_d` BEFORE DELETE ON `0_hrm_comp_payroll_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_rec_u` BEFORE UPDATE ON `0_hrm_comp_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_rec_d` BEFORE DELETE ON `0_hrm_comp_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_cmp1_rcv_u` BEFORE UPDATE ON `0_hrm_comp_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies update';

CREATE TRIGGER `0_cmp1_rcv_d` BEFORE DELETE ON `0_hrm_comp_recovery_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-CMP-001 immutable custody denies delete';

CREATE TRIGGER `0_ben1_pad_u` BEFORE UPDATE ON `0_hrm_ben_pack_adapter_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_pad_d` BEFORE DELETE ON `0_hrm_ben_pack_adapter_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_prv_u` BEFORE UPDATE ON `0_hrm_ben_provider_adapter_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_prv_d` BEFORE DELETE ON `0_hrm_ben_provider_adapter_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_pln_u` BEFORE UPDATE ON `0_hrm_ben_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_pln_d` BEFORE DELETE ON `0_hrm_ben_plans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_pvr_u` BEFORE UPDATE ON `0_hrm_ben_plan_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_pvr_d` BEFORE DELETE ON `0_hrm_ben_plan_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_eli_u` BEFORE UPDATE ON `0_hrm_ben_eligibility_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_eli_d` BEFORE DELETE ON `0_hrm_ben_eligibility_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_win_u` BEFORE UPDATE ON `0_hrm_ben_enrollment_windows` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_win_d` BEFORE DELETE ON `0_hrm_ben_enrollment_windows` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_con_u` BEFORE UPDATE ON `0_hrm_ben_consents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_con_d` BEFORE DELETE ON `0_hrm_ben_consents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_enr_u` BEFORE UPDATE ON `0_hrm_ben_enrollments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_enr_d` BEFORE DELETE ON `0_hrm_ben_enrollments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_dep_u` BEFORE UPDATE ON `0_hrm_ben_enrollment_dependants` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_dep_d` BEFORE DELETE ON `0_hrm_ben_enrollment_dependants` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_let_u` BEFORE UPDATE ON `0_hrm_ben_life_event_type_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_let_d` BEFORE DELETE ON `0_hrm_ben_life_event_type_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_ler_u` BEFORE UPDATE ON `0_hrm_ben_life_event_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_ler_d` BEFORE DELETE ON `0_hrm_ben_life_event_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_lrv_u` BEFORE UPDATE ON `0_hrm_ben_life_event_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_lrv_d` BEFORE DELETE ON `0_hrm_ben_life_event_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_evt_u` BEFORE UPDATE ON `0_hrm_ben_life_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_evt_d` BEFORE DELETE ON `0_hrm_ben_life_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_cvr_u` BEFORE UPDATE ON `0_hrm_ben_contribution_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_cvr_d` BEFORE DELETE ON `0_hrm_ben_contribution_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_crc_u` BEFORE UPDATE ON `0_hrm_ben_contribution_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_crc_d` BEFORE DELETE ON `0_hrm_ben_contribution_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_arr_u` BEFORE UPDATE ON `0_hrm_ben_arrears` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_arr_d` BEFORE DELETE ON `0_hrm_ben_arrears` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_amv_u` BEFORE UPDATE ON `0_hrm_ben_arrears_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_amv_d` BEFORE DELETE ON `0_hrm_ben_arrears_movements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_cnt_u` BEFORE UPDATE ON `0_hrm_ben_continuations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_cnt_d` BEFORE DELETE ON `0_hrm_ben_continuations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_pmp_u` BEFORE UPDATE ON `0_hrm_ben_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_pmp_d` BEFORE DELETE ON `0_hrm_ben_payroll_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_pho_u` BEFORE UPDATE ON `0_hrm_ben_payroll_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_pho_d` BEFORE DELETE ON `0_hrm_ben_payroll_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_bmp_u` BEFORE UPDATE ON `0_hrm_ben_balance_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_bmp_d` BEFORE DELETE ON `0_hrm_ben_balance_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_bho_u` BEFORE UPDATE ON `0_hrm_ben_balance_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_bho_d` BEFORE DELETE ON `0_hrm_ben_balance_handoffs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_pop_u` BEFORE UPDATE ON `0_hrm_ben_provider_operations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_pop_d` BEFORE DELETE ON `0_hrm_ben_provider_operations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ben1_prc_u` BEFORE UPDATE ON `0_hrm_ben_provider_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be updated';

CREATE TRIGGER `0_ben1_prc_d` BEFORE DELETE ON `0_hrm_ben_provider_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-BEN-001 immutable evidence cannot be deleted';

CREATE TRIGGER `0_ess2_cap_u` BEFORE UPDATE ON `0_hrm_ess_002_delegation_capabilities` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ESS-002 immutable delegation capability cannot be updated';

CREATE TRIGGER `0_ess2_cap_d` BEFORE DELETE ON `0_hrm_ess_002_delegation_capabilities` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ESS-002 immutable delegation capability cannot be deleted';

CREATE TRIGGER `0_ess2_rev_u` BEFORE UPDATE ON `0_hrm_ess_002_delegation_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ESS-002 immutable delegation revocation cannot be updated';

CREATE TRIGGER `0_ess2_rev_d` BEFORE DELETE ON `0_hrm_ess_002_delegation_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ESS-002 immutable delegation revocation cannot be deleted';

CREATE TRIGGER `0_ess2_cmd_u` BEFORE UPDATE ON `0_hrm_ess_002_command_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ESS-002 immutable command receipt cannot be updated';

CREATE TRIGGER `0_ess2_cmd_d` BEFORE DELETE ON `0_hrm_ess_002_command_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ESS-002 immutable command receipt cannot be deleted';

CREATE TRIGGER `0_ct1_rb_u` BEFORE UPDATE ON `0_pay_ctry_raw_blobs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_raw_blobs row cannot be updated';

CREATE TRIGGER `0_ct1_rb_d` BEFORE DELETE ON `0_pay_ctry_raw_blobs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_raw_blobs row cannot be deleted';

CREATE TRIGGER `0_ct1_pk_u` BEFORE UPDATE ON `0_pay_ctry_packs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_packs row cannot be updated';

CREATE TRIGGER `0_ct1_pk_d` BEFORE DELETE ON `0_pay_ctry_packs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_packs row cannot be deleted';

CREATE TRIGGER `0_ct1_pv_u` BEFORE UPDATE ON `0_pay_ctry_pack_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_pack_versions row cannot be updated';

CREATE TRIGGER `0_ct1_pv_d` BEFORE DELETE ON `0_pay_ctry_pack_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_pack_versions row cannot be deleted';

CREATE TRIGGER `0_ct1_ar_u` BEFORE UPDATE ON `0_pay_ctry_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_artifacts row cannot be updated';

CREATE TRIGGER `0_ct1_ar_d` BEFORE DELETE ON `0_pay_ctry_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_artifacts row cannot be deleted';

CREATE TRIGGER `0_ct1_pu_u` BEFORE UPDATE ON `0_pay_ctry_publishers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_publishers row cannot be updated';

CREATE TRIGGER `0_ct1_pu_d` BEFORE DELETE ON `0_pay_ctry_publishers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_publishers row cannot be deleted';

CREATE TRIGGER `0_ct1_pr_u` BEFORE UPDATE ON `0_pay_ctry_publisher_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_publisher_revocations row cannot be updated';

CREATE TRIGGER `0_ct1_pr_d` BEFORE DELETE ON `0_pay_ctry_publisher_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_publisher_revocations row cannot be deleted';

CREATE TRIGGER `0_ct1_si_u` BEFORE UPDATE ON `0_pay_ctry_signers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_signers row cannot be updated';

CREATE TRIGGER `0_ct1_si_d` BEFORE DELETE ON `0_pay_ctry_signers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_signers row cannot be deleted';

CREATE TRIGGER `0_ct1_tk_u` BEFORE UPDATE ON `0_pay_ctry_trust_keys` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_trust_keys row cannot be updated';

CREATE TRIGGER `0_ct1_tk_d` BEFORE DELETE ON `0_pay_ctry_trust_keys` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_trust_keys row cannot be deleted';

CREATE TRIGGER `0_ct1_kr_u` BEFORE UPDATE ON `0_pay_ctry_trust_key_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_trust_key_revocations row cannot be updated';

CREATE TRIGGER `0_ct1_kr_d` BEFORE DELETE ON `0_pay_ctry_trust_key_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_trust_key_revocations row cannot be deleted';

CREATE TRIGGER `0_ct1_ks_u` BEFORE UPDATE ON `0_pay_ctry_trust_key_supersessions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_trust_key_supersessions row cannot be updated';

CREATE TRIGGER `0_ct1_ks_d` BEFORE DELETE ON `0_pay_ctry_trust_key_supersessions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_trust_key_supersessions row cannot be deleted';

CREATE TRIGGER `0_ct1_vr_u` BEFORE UPDATE ON `0_pay_ctry_verification_reports` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_verification_reports row cannot be updated';

CREATE TRIGGER `0_ct1_vr_d` BEFORE DELETE ON `0_pay_ctry_verification_reports` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_verification_reports row cannot be deleted';

CREATE TRIGGER `0_ct1_cv_u` BEFORE UPDATE ON `0_pay_ctry_capability_registry_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_capability_registry_versions row cannot be updated';

CREATE TRIGGER `0_ct1_cv_d` BEFORE DELETE ON `0_pay_ctry_capability_registry_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_capability_registry_versions row cannot be deleted';

CREATE TRIGGER `0_ct1_cd_u` BEFORE UPDATE ON `0_pay_ctry_capability_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_capability_definitions row cannot be updated';

CREATE TRIGGER `0_ct1_cd_d` BEFORE DELETE ON `0_pay_ctry_capability_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_capability_definitions row cannot be deleted';

CREATE TRIGGER `0_ct1_dl_u` BEFORE UPDATE ON `0_pay_ctry_dependency_locks` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_dependency_locks row cannot be updated';

CREATE TRIGGER `0_ct1_dl_d` BEFORE DELETE ON `0_pay_ctry_dependency_locks` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_dependency_locks row cannot be deleted';

CREATE TRIGGER `0_ct1_dm_u` BEFORE UPDATE ON `0_pay_ctry_dependency_lock_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_dependency_lock_members row cannot be updated';

CREATE TRIGGER `0_ct1_dm_d` BEFORE DELETE ON `0_pay_ctry_dependency_lock_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_dependency_lock_members row cannot be deleted';

CREATE TRIGGER `0_ct1_ir_u` BEFORE UPDATE ON `0_pay_ctry_install_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_install_receipts row cannot be updated';

CREATE TRIGGER `0_ct1_ir_d` BEFORE DELETE ON `0_pay_ctry_install_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-001 immutable pay_ctry_install_receipts row cannot be deleted';

CREATE TRIGGER `0_ct2_oj_u` BEFORE UPDATE ON `0_pay_ctry_site_operation_journal` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_site_operation_journal row cannot be updated';

CREATE TRIGGER `0_ct2_oj_d` BEFORE DELETE ON `0_pay_ctry_site_operation_journal` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_site_operation_journal row cannot be deleted';

CREATE TRIGGER `0_ct2_si_u` BEFORE UPDATE ON `0_pay_ctry_site_installations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_site_installations row cannot be updated';

CREATE TRIGGER `0_ct2_si_d` BEFORE DELETE ON `0_pay_ctry_site_installations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_site_installations row cannot be deleted';

CREATE TRIGGER `0_ct2_im_u` BEFORE UPDATE ON `0_pay_ctry_activation_impact_simulations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_impact_simulations row cannot be updated';

CREATE TRIGGER `0_ct2_im_d` BEFORE DELETE ON `0_pay_ctry_activation_impact_simulations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_impact_simulations row cannot be deleted';

CREATE TRIGGER `0_ct2_ap_u` BEFORE UPDATE ON `0_pay_ctry_activation_proposals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_proposals row cannot be updated';

CREATE TRIGGER `0_ct2_ap_d` BEFORE DELETE ON `0_pay_ctry_activation_proposals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_proposals row cannot be deleted';

CREATE TRIGGER `0_ct2_aa_u` BEFORE UPDATE ON `0_pay_ctry_activation_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_approvals row cannot be updated';

CREATE TRIGGER `0_ct2_aa_d` BEFORE DELETE ON `0_pay_ctry_activation_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_approvals row cannot be deleted';

CREATE TRIGGER `0_ct2_ac_u` BEFORE UPDATE ON `0_pay_ctry_activations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activations row cannot be updated';

CREATE TRIGGER `0_ct2_ac_d` BEFORE DELETE ON `0_pay_ctry_activations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activations row cannot be deleted';

CREATE TRIGGER `0_ct2_bd_u` BEFORE UPDATE ON `0_pay_ctry_activation_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_bindings row cannot be updated';

CREATE TRIGGER `0_ct2_bd_d` BEFORE DELETE ON `0_pay_ctry_activation_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_bindings row cannot be deleted';

CREATE TRIGGER `0_ct2_ss_u` BEFORE UPDATE ON `0_pay_ctry_activation_suspensions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_suspensions row cannot be updated';

CREATE TRIGGER `0_ct2_ss_d` BEFORE DELETE ON `0_pay_ctry_activation_suspensions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_suspensions row cannot be deleted';

CREATE TRIGGER `0_ct2_ob_u` BEFORE UPDATE ON `0_pay_ctry_activation_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_outbox row cannot be updated';

CREATE TRIGGER `0_ct2_ob_d` BEFORE DELETE ON `0_pay_ctry_activation_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_outbox row cannot be deleted';

CREATE TRIGGER `0_ct2_da_u` BEFORE UPDATE ON `0_pay_ctry_activation_delivery_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_delivery_attempts row cannot be updated';

CREATE TRIGGER `0_ct2_da_d` BEFORE DELETE ON `0_pay_ctry_activation_delivery_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_delivery_attempts row cannot be deleted';

CREATE TRIGGER `0_ct2_rg_u` BEFORE UPDATE ON `0_pay_ctry_activation_run_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_run_bindings row cannot be updated';

CREATE TRIGGER `0_ct2_rg_d` BEFORE DELETE ON `0_pay_ctry_activation_run_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_run_bindings row cannot be deleted';

CREATE TRIGGER `0_ct2_rv_u` BEFORE UPDATE ON `0_pay_ctry_activation_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_revocations row cannot be updated';

CREATE TRIGGER `0_ct2_rv_d` BEFORE DELETE ON `0_pay_ctry_activation_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-002 immutable pay_ctry_activation_revocations row cannot be deleted';

CREATE TRIGGER `0_ct3_cv_u` BEFORE UPDATE ON `0_pay_ctry_capability_contract_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_capability_contract_versions row cannot be updated';

CREATE TRIGGER `0_ct3_cv_d` BEFORE DELETE ON `0_pay_ctry_capability_contract_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_capability_contract_versions row cannot be deleted';

CREATE TRIGGER `0_ct3_cd_u` BEFORE UPDATE ON `0_pay_ctry_capability_contract_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_capability_contract_definitions row cannot be updated';

CREATE TRIGGER `0_ct3_cd_d` BEFORE DELETE ON `0_pay_ctry_capability_contract_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_capability_contract_definitions row cannot be deleted';

CREATE TRIGGER `0_ct3_ha_u` BEFORE UPDATE ON `0_pay_ctry_host_adapters` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_host_adapters row cannot be updated';

CREATE TRIGGER `0_ct3_ha_d` BEFORE DELETE ON `0_pay_ctry_host_adapters` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_host_adapters row cannot be deleted';

CREATE TRIGGER `0_ct3_ac_u` BEFORE UPDATE ON `0_pay_ctry_pack_artifact_contracts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_pack_artifact_contracts row cannot be updated';

CREATE TRIGGER `0_ct3_ac_d` BEFORE DELETE ON `0_pay_ctry_pack_artifact_contracts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_pack_artifact_contracts row cannot be deleted';

CREATE TRIGGER `0_ct3_vr_u` BEFORE UPDATE ON `0_pay_ctry_contract_validation_reports` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_contract_validation_reports row cannot be updated';

CREATE TRIGGER `0_ct3_vr_d` BEFORE DELETE ON `0_pay_ctry_contract_validation_reports` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_contract_validation_reports row cannot be deleted';

CREATE TRIGGER `0_ct3_me_u` BEFORE UPDATE ON `0_pay_ctry_contract_migration_edges` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_contract_migration_edges row cannot be updated';

CREATE TRIGGER `0_ct3_me_d` BEFORE DELETE ON `0_pay_ctry_contract_migration_edges` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-003 immutable pay_ctry_contract_migration_edges row cannot be deleted';

CREATE TRIGGER `0_ct4_es_u` BEFORE UPDATE ON `0_pay_ctry_evidence_sources` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_sources row cannot be updated';

CREATE TRIGGER `0_ct4_es_d` BEFORE DELETE ON `0_pay_ctry_evidence_sources` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_sources row cannot be deleted';

CREATE TRIGGER `0_ct4_ei_u` BEFORE UPDATE ON `0_pay_ctry_evidence_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_items row cannot be updated';

CREATE TRIGGER `0_ct4_ei_d` BEFORE DELETE ON `0_pay_ctry_evidence_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_items row cannot be deleted';

CREATE TRIGGER `0_ct4_ev_u` BEFORE UPDATE ON `0_pay_ctry_evidence_inventories` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_inventories row cannot be updated';

CREATE TRIGGER `0_ct4_ev_d` BEFORE DELETE ON `0_pay_ctry_evidence_inventories` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_inventories row cannot be deleted';

CREATE TRIGGER `0_ct4_em_u` BEFORE UPDATE ON `0_pay_ctry_evidence_inventory_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_inventory_members row cannot be updated';

CREATE TRIGGER `0_ct4_em_d` BEFORE DELETE ON `0_pay_ctry_evidence_inventory_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_evidence_inventory_members row cannot be deleted';

CREATE TRIGGER `0_ct4_rd_u` BEFORE UPDATE ON `0_pay_ctry_review_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_review_decisions row cannot be updated';

CREATE TRIGGER `0_ct4_rd_d` BEFORE DELETE ON `0_pay_ctry_review_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_review_decisions row cannot be deleted';

CREATE TRIGGER `0_ct4_pr_u` BEFORE UPDATE ON `0_pay_ctry_parallel_run_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_parallel_run_evidence row cannot be updated';

CREATE TRIGGER `0_ct4_pr_d` BEFORE DELETE ON `0_pay_ctry_parallel_run_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_parallel_run_evidence row cannot be deleted';

CREATE TRIGGER `0_ct4_sk_u` BEFORE UPDATE ON `0_pay_ctry_certification_signing_keys` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_signing_keys row cannot be updated';

CREATE TRIGGER `0_ct4_sk_d` BEFORE DELETE ON `0_pay_ctry_certification_signing_keys` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_signing_keys row cannot be deleted';

CREATE TRIGGER `0_ct4_kr_u` BEFORE UPDATE ON `0_pay_ctry_certification_key_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_key_revocations row cannot be updated';

CREATE TRIGGER `0_ct4_kr_d` BEFORE DELETE ON `0_pay_ctry_certification_key_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_key_revocations row cannot be deleted';

CREATE TRIGGER `0_ct4_ce_u` BEFORE UPDATE ON `0_pay_ctry_certification_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_events row cannot be updated';

CREATE TRIGGER `0_ct4_ce_d` BEFORE DELETE ON `0_pay_ctry_certification_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_events row cannot be deleted';

CREATE TRIGGER `0_ct4_cc_u` BEFORE UPDATE ON `0_pay_ctry_certification_claims` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_claims row cannot be updated';

CREATE TRIGGER `0_ct4_cc_d` BEFORE DELETE ON `0_pay_ctry_certification_claims` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_claims row cannot be deleted';

CREATE TRIGGER `0_ct4_pb_u` BEFORE UPDATE ON `0_pay_ctry_publication_records` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_publication_records row cannot be updated';

CREATE TRIGGER `0_ct4_pb_d` BEFORE DELETE ON `0_pay_ctry_publication_records` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_publication_records row cannot be deleted';

CREATE TRIGGER `0_ct4_ea_u` BEFORE UPDATE ON `0_pay_ctry_certification_emergency_actions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_emergency_actions row cannot be updated';

CREATE TRIGGER `0_ct4_ea_d` BEFORE DELETE ON `0_pay_ctry_certification_emergency_actions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_emergency_actions row cannot be deleted';

CREATE TRIGGER `0_ct4_ra_u` BEFORE UPDATE ON `0_pay_ctry_certification_raci_assignments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_raci_assignments row cannot be updated';

CREATE TRIGGER `0_ct4_ra_d` BEFORE DELETE ON `0_pay_ctry_certification_raci_assignments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_raci_assignments row cannot be deleted';

CREATE TRIGGER `0_ct4_rr_u` BEFORE UPDATE ON `0_pay_ctry_certification_recovery_records` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_recovery_records row cannot be updated';

CREATE TRIGGER `0_ct4_rr_d` BEFORE DELETE ON `0_pay_ctry_certification_recovery_records` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-004 immutable pay_ctry_certification_recovery_records row cannot be deleted';

CREATE TRIGGER `0_ct7_cr_u` BEFORE UPDATE ON `0_pay_ctry_catalog_releases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_catalog_releases row cannot be updated';

CREATE TRIGGER `0_ct7_cr_d` BEFORE DELETE ON `0_pay_ctry_catalog_releases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_catalog_releases row cannot be deleted';

CREATE TRIGGER `0_ct7_ca_u` BEFORE UPDATE ON `0_pay_ctry_catalog_release_admissions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_catalog_release_admissions row cannot be updated';

CREATE TRIGGER `0_ct7_ca_d` BEFORE DELETE ON `0_pay_ctry_catalog_release_admissions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_catalog_release_admissions row cannot be deleted';

CREATE TRIGGER `0_ct7_un_u` BEFORE UPDATE ON `0_pay_ctry_update_notices` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notices row cannot be updated';

CREATE TRIGGER `0_ct7_un_d` BEFORE DELETE ON `0_pay_ctry_update_notices` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notices row cannot be deleted';

CREATE TRIGGER `0_ct7_na_u` BEFORE UPDATE ON `0_pay_ctry_update_notice_applicability` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_applicability row cannot be updated';

CREATE TRIGGER `0_ct7_na_d` BEFORE DELETE ON `0_pay_ctry_update_notice_applicability` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_applicability row cannot be deleted';

CREATE TRIGGER `0_ct7_au_u` BEFORE UPDATE ON `0_pay_ctry_update_notice_audiences` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_audiences row cannot be updated';

CREATE TRIGGER `0_ct7_au_d` BEFORE DELETE ON `0_pay_ctry_update_notice_audiences` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_audiences row cannot be deleted';

CREATE TRIGGER `0_ct7_re_u` BEFORE UPDATE ON `0_pay_ctry_update_notice_recipient_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_recipient_events row cannot be updated';

CREATE TRIGGER `0_ct7_re_d` BEFORE DELETE ON `0_pay_ctry_update_notice_recipient_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_recipient_events row cannot be deleted';

CREATE TRIGGER `0_ct7_ak_u` BEFORE UPDATE ON `0_pay_ctry_update_notice_acknowledgements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_acknowledgements row cannot be updated';

CREATE TRIGGER `0_ct7_ak_d` BEFORE DELETE ON `0_pay_ctry_update_notice_acknowledgements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_acknowledgements row cannot be deleted';

CREATE TRIGGER `0_ct7_le_u` BEFORE UPDATE ON `0_pay_ctry_update_notice_lifecycle_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_lifecycle_events row cannot be updated';

CREATE TRIGGER `0_ct7_le_d` BEFORE DELETE ON `0_pay_ctry_update_notice_lifecycle_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_lifecycle_events row cannot be deleted';

CREATE TRIGGER `0_ct7_ob_u` BEFORE UPDATE ON `0_pay_ctry_update_notice_delivery_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_delivery_outbox row cannot be updated';

CREATE TRIGGER `0_ct7_ob_d` BEFORE DELETE ON `0_pay_ctry_update_notice_delivery_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_delivery_outbox row cannot be deleted';

CREATE TRIGGER `0_ct7_da_u` BEFORE UPDATE ON `0_pay_ctry_update_notice_delivery_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_delivery_attempts row cannot be updated';

CREATE TRIGGER `0_ct7_da_d` BEFORE DELETE ON `0_pay_ctry_update_notice_delivery_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CTRY-007 immutable pay_ctry_update_notice_delivery_attempts row cannot be deleted';

CREATE TRIGGER `0_pgl1_ji_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_journal_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_journal_intents row cannot be updated';

CREATE TRIGGER `0_pgl1_ji_d` BEFORE DELETE ON `0_hrm_pay_gl_001_journal_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_journal_intents row cannot be deleted';

CREATE TRIGGER `0_pgl1_jl_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_journal_lines` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_journal_lines row cannot be updated';

CREATE TRIGGER `0_pgl1_jl_d` BEFORE DELETE ON `0_hrm_pay_gl_001_journal_lines` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_journal_lines row cannot be deleted';

CREATE TRIGGER `0_pgl1_al_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_allocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_allocations row cannot be updated';

CREATE TRIGGER `0_pgl1_al_d` BEFORE DELETE ON `0_hrm_pay_gl_001_allocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_allocations row cannot be deleted';

CREATE TRIGGER `0_pgl1_ax_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_allocation_axes` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_allocation_axes row cannot be updated';

CREATE TRIGGER `0_pgl1_ax_d` BEFORE DELETE ON `0_hrm_pay_gl_001_allocation_axes` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_allocation_axes row cannot be deleted';

CREATE TRIGGER `0_pgl1_mv_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_mapping_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_mapping_versions row cannot be updated';

CREATE TRIGGER `0_pgl1_mv_d` BEFORE DELETE ON `0_hrm_pay_gl_001_mapping_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_mapping_versions row cannot be deleted';

CREATE TRIGGER `0_pgl1_mr_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_mapping_rules` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_mapping_rules row cannot be updated';

CREATE TRIGGER `0_pgl1_mr_d` BEFORE DELETE ON `0_hrm_pay_gl_001_mapping_rules` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_mapping_rules row cannot be deleted';

CREATE TRIGGER `0_pgl1_rc_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_reconciliations row cannot be updated';

CREATE TRIGGER `0_pgl1_rc_d` BEFORE DELETE ON `0_hrm_pay_gl_001_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_reconciliations row cannot be deleted';

CREATE TRIGGER `0_pgl1_rv_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_reviews row cannot be updated';

CREATE TRIGGER `0_pgl1_rv_d` BEFORE DELETE ON `0_hrm_pay_gl_001_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_reviews row cannot be deleted';

CREATE TRIGGER `0_pgl1_pa_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_posting_authorities` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_posting_authorities row cannot be updated';

CREATE TRIGGER `0_pgl1_pa_d` BEFORE DELETE ON `0_hrm_pay_gl_001_posting_authorities` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_posting_authorities row cannot be deleted';

CREATE TRIGGER `0_pgl1_gp_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_gl_postings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_gl_postings row cannot be updated';

CREATE TRIGGER `0_pgl1_gp_d` BEFORE DELETE ON `0_hrm_pay_gl_001_gl_postings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_gl_postings row cannot be deleted';

CREATE TRIGGER `0_pgl1_gpl_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_gl_posting_lines` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_gl_posting_lines row cannot be updated';

CREATE TRIGGER `0_pgl1_gpl_d` BEFORE DELETE ON `0_hrm_pay_gl_001_gl_posting_lines` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_gl_posting_lines row cannot be deleted';

CREATE TRIGGER `0_pgl1_gc_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_recovery_commands` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_recovery_commands row cannot be updated';

CREATE TRIGGER `0_pgl1_gc_d` BEFORE DELETE ON `0_hrm_pay_gl_001_recovery_commands` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_recovery_commands row cannot be deleted';

CREATE TRIGGER `0_pgl1_gr_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_reversal_repost_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_reversal_repost_links row cannot be updated';

CREATE TRIGGER `0_pgl1_gr_d` BEFORE DELETE ON `0_hrm_pay_gl_001_reversal_repost_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_reversal_repost_links row cannot be deleted';

CREATE TRIGGER `0_pgl1_co_u` BEFORE UPDATE ON `0_hrm_pay_gl_001_cutovers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_cutovers row cannot be updated';

CREATE TRIGGER `0_pgl1_co_d` BEFORE DELETE ON `0_hrm_pay_gl_001_cutovers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-GL-001 immutable hrm_pay_gl_001_cutovers row cannot be deleted';

CREATE TRIGGER `0_ppm1_pi_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_payment_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_payment_intents row cannot be updated';

CREATE TRIGGER `0_ppm1_pi_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_payment_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_payment_intents row cannot be deleted';

CREATE TRIGGER `0_ppm1_it_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_payment_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_payment_items row cannot be updated';

CREATE TRIGGER `0_ppm1_it_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_payment_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_payment_items row cannot be deleted';

CREATE TRIGGER `0_ppm1_al_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_item_allocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_item_allocations row cannot be updated';

CREATE TRIGGER `0_ppm1_al_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_item_allocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_item_allocations row cannot be deleted';

CREATE TRIGGER `0_ppm1_ba_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_batches` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_batches row cannot be updated';

CREATE TRIGGER `0_ppm1_ba_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_batches` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_batches row cannot be deleted';

CREATE TRIGGER `0_ppm1_bi_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_batch_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_batch_items row cannot be updated';

CREATE TRIGGER `0_ppm1_bi_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_batch_items` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_batch_items row cannot be deleted';

CREATE TRIGGER `0_ppm1_br_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_batch_releases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_batch_releases row cannot be updated';

CREATE TRIGGER `0_ppm1_br_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_batch_releases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_batch_releases row cannot be deleted';

CREATE TRIGGER `0_ppm1_ar_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_artifacts row cannot be updated';

CREATE TRIGGER `0_ppm1_ar_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_artifacts row cannot be deleted';

CREATE TRIGGER `0_ppm1_ob_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_outbox row cannot be updated';

CREATE TRIGGER `0_ppm1_ob_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_outbox row cannot be deleted';

CREATE TRIGGER `0_ppm1_ta_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_transmission_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_transmission_attempts row cannot be updated';

CREATE TRIGGER `0_ppm1_ta_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_transmission_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_transmission_attempts row cannot be deleted';

CREATE TRIGGER `0_ppm1_fl_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_failures` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_failures row cannot be updated';

CREATE TRIGGER `0_ppm1_fl_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_failures` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_failures row cannot be deleted';

CREATE TRIGGER `0_ppm1_ri_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_reissues` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_reissues row cannot be updated';

CREATE TRIGGER `0_ppm1_ri_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_reissues` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_reissues row cannot be deleted';

CREATE TRIGGER `0_ppm1_st_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_settlements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_settlements row cannot be updated';

CREATE TRIGGER `0_ppm1_st_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_settlements` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_settlements row cannot be deleted';

CREATE TRIGGER `0_ppm1_rc_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_reconciliations row cannot be updated';

CREATE TRIGGER `0_ppm1_rc_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_reconciliations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_reconciliations row cannot be deleted';

CREATE TRIGGER `0_ppm1_co_u` BEFORE UPDATE ON `0_hrm_pay_pmt_001_cutovers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_cutovers row cannot be updated';

CREATE TRIGGER `0_ppm1_co_d` BEFORE DELETE ON `0_hrm_pay_pmt_001_cutovers` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-PMT-001 immutable hrm_pay_pmt_001_cutovers row cannot be deleted';

CREATE TRIGGER `0_pfl1_ob_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_obligations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_obligations row cannot be updated';

CREATE TRIGGER `0_pfl1_ob_d` BEFORE DELETE ON `0_hrm_pay_fil_001_obligations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_obligations row cannot be deleted';

CREATE TRIGGER `0_pfl1_ss_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_source_sets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_source_sets row cannot be updated';

CREATE TRIGGER `0_pfl1_ss_d` BEFORE DELETE ON `0_hrm_pay_fil_001_source_sets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_source_sets row cannot be deleted';

CREATE TRIGGER `0_pfl1_sl_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_source_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_source_links row cannot be updated';

CREATE TRIGGER `0_pfl1_sl_d` BEFORE DELETE ON `0_hrm_pay_fil_001_source_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_source_links row cannot be deleted';

CREATE TRIGGER `0_pfl1_ar_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_artifacts row cannot be updated';

CREATE TRIGGER `0_pfl1_ar_d` BEFORE DELETE ON `0_hrm_pay_fil_001_artifacts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_artifacts row cannot be deleted';

CREATE TRIGGER `0_pfl1_va_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_validations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_validations row cannot be updated';

CREATE TRIGGER `0_pfl1_va_d` BEFORE DELETE ON `0_hrm_pay_fil_001_validations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_validations row cannot be deleted';

CREATE TRIGGER `0_pfl1_sa_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_schema_authorities` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_schema_authorities row cannot be updated';

CREATE TRIGGER `0_pfl1_sa_d` BEFORE DELETE ON `0_hrm_pay_fil_001_schema_authorities` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_schema_authorities row cannot be deleted';

CREATE TRIGGER `0_pfl1_au_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_authorization_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_authorization_intents row cannot be updated';

CREATE TRIGGER `0_pfl1_au_d` BEFORE DELETE ON `0_hrm_pay_fil_001_authorization_intents` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_authorization_intents row cannot be deleted';

CREATE TRIGGER `0_pfl1_tp_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_tt80_taxpayer_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_taxpayer_profiles row cannot be updated';

CREATE TRIGGER `0_pfl1_tp_d` BEFORE DELETE ON `0_hrm_pay_fil_001_tt80_taxpayer_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_taxpayer_profiles row cannot be deleted';

CREATE TRIGGER `0_pfl1_pf_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_tt80_person_facts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_person_facts row cannot be updated';

CREATE TRIGGER `0_pfl1_pf_d` BEFORE DELETE ON `0_hrm_pay_fil_001_tt80_person_facts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_person_facts row cannot be deleted';

CREATE TRIGGER `0_pfl1_pl_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_tt80_person_fact_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_person_fact_links row cannot be updated';

CREATE TRIGGER `0_pfl1_pl_d` BEFORE DELETE ON `0_hrm_pay_fil_001_tt80_person_fact_links` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_person_fact_links row cannot be deleted';

CREATE TRIGGER `0_pfl1_sr_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_tt80_semantic_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_semantic_reviews row cannot be updated';

CREATE TRIGGER `0_pfl1_sr_d` BEFORE DELETE ON `0_hrm_pay_fil_001_tt80_semantic_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_semantic_reviews row cannot be deleted';

CREATE TRIGGER `0_pfl1_sm_u` BEFORE UPDATE ON `0_hrm_pay_fil_001_tt80_semantic_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_semantic_mappings row cannot be updated';

CREATE TRIGGER `0_pfl1_sm_d` BEFORE DELETE ON `0_hrm_pay_fil_001_tt80_semantic_mappings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-FIL-001 immutable hrm_pay_fil_001_tt80_semantic_mappings row cannot be deleted';

CREATE TRIGGER `0_papi1_rs_u` BEFORE UPDATE ON `0_hrm_pay_api_001_resources` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_resources row cannot be updated';

CREATE TRIGGER `0_papi1_rs_d` BEFORE DELETE ON `0_hrm_pay_api_001_resources` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_resources row cannot be deleted';

CREATE TRIGGER `0_papi1_mp_u` BEFORE UPDATE ON `0_hrm_pay_api_001_machine_principals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_machine_principals row cannot be updated';

CREATE TRIGGER `0_papi1_mp_d` BEFORE DELETE ON `0_hrm_pay_api_001_machine_principals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_machine_principals row cannot be deleted';

CREATE TRIGGER `0_papi1_ps_u` BEFORE UPDATE ON `0_hrm_pay_api_001_principal_scopes` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_principal_scopes row cannot be updated';

CREATE TRIGGER `0_papi1_ps_d` BEFORE DELETE ON `0_hrm_pay_api_001_principal_scopes` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_principal_scopes row cannot be deleted';

CREATE TRIGGER `0_papi1_cr_u` BEFORE UPDATE ON `0_hrm_pay_api_001_credentials` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_credentials row cannot be updated';

CREATE TRIGGER `0_papi1_cr_d` BEFORE DELETE ON `0_hrm_pay_api_001_credentials` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_credentials row cannot be deleted';

CREATE TRIGGER `0_papi1_cv_u` BEFORE UPDATE ON `0_hrm_pay_api_001_credential_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_credential_revocations row cannot be updated';

CREATE TRIGGER `0_papi1_cv_d` BEFORE DELETE ON `0_hrm_pay_api_001_credential_revocations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_credential_revocations row cannot be deleted';

CREATE TRIGGER `0_papi1_ad_u` BEFORE UPDATE ON `0_hrm_pay_api_001_authn_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_authn_decisions row cannot be updated';

CREATE TRIGGER `0_papi1_ad_d` BEFORE DELETE ON `0_hrm_pay_api_001_authn_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_authn_decisions row cannot be deleted';

CREATE TRIGGER `0_papi1_pr_u` BEFORE UPDATE ON `0_hrm_pay_api_001_provision_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_requests row cannot be updated';

CREATE TRIGGER `0_papi1_pr_d` BEFORE DELETE ON `0_hrm_pay_api_001_provision_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_requests row cannot be deleted';

CREATE TRIGGER `0_papi1_pv_u` BEFORE UPDATE ON `0_hrm_pay_api_001_provision_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_reviews row cannot be updated';

CREATE TRIGGER `0_papi1_pv_d` BEFORE DELETE ON `0_hrm_pay_api_001_provision_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_reviews row cannot be deleted';

CREATE TRIGGER `0_papi1_pa_u` BEFORE UPDATE ON `0_hrm_pay_api_001_provision_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_approvals row cannot be updated';

CREATE TRIGGER `0_papi1_pa_d` BEFORE DELETE ON `0_hrm_pay_api_001_provision_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_approvals row cannot be deleted';

CREATE TRIGGER `0_papi1_rlp_u` BEFORE UPDATE ON `0_hrm_pay_api_001_rate_limit_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_rate_limit_policies row cannot be updated';

CREATE TRIGGER `0_papi1_rlp_d` BEFORE DELETE ON `0_hrm_pay_api_001_rate_limit_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_rate_limit_policies row cannot be deleted';

CREATE TRIGGER `0_papi1_rld_u` BEFORE UPDATE ON `0_hrm_pay_api_001_rate_limit_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_rate_limit_decisions row cannot be updated';

CREATE TRIGGER `0_papi1_rld_d` BEFORE DELETE ON `0_hrm_pay_api_001_rate_limit_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_rate_limit_decisions row cannot be deleted';

CREATE TRIGGER `0_papi1_pe_u` BEFORE UPDATE ON `0_hrm_pay_api_001_provision_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_executions row cannot be updated';

CREATE TRIGGER `0_papi1_pe_d` BEFORE DELETE ON `0_hrm_pay_api_001_provision_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_provision_executions row cannot be deleted';

CREATE TRIGGER `0_papi1_rr_u` BEFORE UPDATE ON `0_hrm_pay_api_001_read_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_read_requests row cannot be updated';

CREATE TRIGGER `0_papi1_rr_d` BEFORE DELETE ON `0_hrm_pay_api_001_read_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_read_requests row cannot be deleted';

CREATE TRIGGER `0_papi1_rres_u` BEFORE UPDATE ON `0_hrm_pay_api_001_read_responses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_read_responses row cannot be updated';

CREATE TRIGGER `0_papi1_rres_d` BEFORE DELETE ON `0_hrm_pay_api_001_read_responses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_read_responses row cannot be deleted';

CREATE TRIGGER `0_papi1_wt_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_targets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_targets row cannot be updated';

CREATE TRIGGER `0_papi1_wt_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_targets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_targets row cannot be deleted';

CREATE TRIGGER `0_papi1_we_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_events row cannot be updated';

CREATE TRIGGER `0_papi1_we_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_events row cannot be deleted';

CREATE TRIGGER `0_papi1_wo_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_outbox row cannot be updated';

CREATE TRIGGER `0_papi1_wo_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_outbox` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_outbox row cannot be deleted';

CREATE TRIGGER `0_papi1_wa_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_delivery_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_delivery_attempts row cannot be updated';

CREATE TRIGGER `0_papi1_wa_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_delivery_attempts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_delivery_attempts row cannot be deleted';

CREATE TRIGGER `0_papi1_wd_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_dead_letters` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_dead_letters row cannot be updated';

CREATE TRIGGER `0_papi1_wd_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_dead_letters` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_dead_letters row cannot be deleted';

CREATE TRIGGER `0_papi1_wtl_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_target_lifecycle` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_target_lifecycle row cannot be updated';

CREATE TRIGGER `0_papi1_wtl_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_target_lifecycle` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_target_lifecycle row cannot be deleted';

CREATE TRIGGER `0_papi1_wsr_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_secret_refs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_secret_refs row cannot be updated';

CREATE TRIGGER `0_papi1_wsr_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_secret_refs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_secret_refs row cannot be deleted';

CREATE TRIGGER `0_papi1_wsrn_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_secret_rotations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_secret_rotations row cannot be updated';

CREATE TRIGGER `0_papi1_wsrn_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_secret_rotations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_secret_rotations row cannot be deleted';

CREATE TRIGGER `0_papi1_wdp_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_delivery_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_delivery_policies row cannot be updated';

CREATE TRIGGER `0_papi1_wdp_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_delivery_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_delivery_policies row cannot be deleted';

CREATE TRIGGER `0_papi1_wcl_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_claim_leases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_claim_leases row cannot be updated';

CREATE TRIGGER `0_papi1_wcl_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_claim_leases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_claim_leases row cannot be deleted';

CREATE TRIGGER `0_papi1_wetp_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_egress_tls_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_egress_tls_policies row cannot be updated';

CREATE TRIGGER `0_papi1_wetp_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_egress_tls_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_egress_tls_policies row cannot be deleted';

CREATE TRIGGER `0_papi1_wsp_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_signing_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_signing_profiles row cannot be updated';

CREATE TRIGGER `0_papi1_wsp_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_signing_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_signing_profiles row cannot be deleted';

CREATE TRIGGER `0_papi1_wpr_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_provider_responses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_provider_responses row cannot be updated';

CREATE TRIGGER `0_papi1_wpr_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_provider_responses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_provider_responses row cannot be deleted';

CREATE TRIGGER `0_papi1_wrd_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_replay_defenses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_replay_defenses row cannot be updated';

CREATE TRIGGER `0_papi1_wrd_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_replay_defenses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_replay_defenses row cannot be deleted';

CREATE TRIGGER `0_papi1_wra_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_reprocess_authorizations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_authorizations row cannot be updated';

CREATE TRIGGER `0_papi1_wra_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_reprocess_authorizations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_authorizations row cannot be deleted';

CREATE TRIGGER `0_papi1_wrr_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_retry_dispatch_reservations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_retry_dispatch_reservations row cannot be updated';

CREATE TRIGGER `0_papi1_wrr_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_retry_dispatch_reservations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_retry_dispatch_reservations row cannot be deleted';

CREATE TRIGGER `0_papi1_wrq_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_reprocess_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_requests row cannot be updated';

CREATE TRIGGER `0_papi1_wrq_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_reprocess_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_requests row cannot be deleted';

CREATE TRIGGER `0_papi1_wrv_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_reprocess_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_reviews row cannot be updated';

CREATE TRIGGER `0_papi1_wrv_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_reprocess_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_reviews row cannot be deleted';

CREATE TRIGGER `0_papi1_wrp_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_reprocess_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_approvals row cannot be updated';

CREATE TRIGGER `0_papi1_wrp_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_reprocess_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_approvals row cannot be deleted';

CREATE TRIGGER `0_papi1_wdr_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_reprocess_dispatch_reservations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_dispatch_reservations row cannot be updated';

CREATE TRIGGER `0_papi1_wdr_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_reprocess_dispatch_reservations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_reprocess_dispatch_reservations row cannot be deleted';

CREATE TRIGGER `0_papi1_wis_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_inbound_sources` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_inbound_sources row cannot be updated';

CREATE TRIGGER `0_papi1_wis_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_inbound_sources` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_inbound_sources row cannot be deleted';

CREATE TRIGGER `0_papi1_wir_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_inbound_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_inbound_receipts row cannot be updated';

CREATE TRIGGER `0_papi1_wir_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_inbound_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_inbound_receipts row cannot be deleted';

CREATE TRIGGER `0_papi1_wid_u` BEFORE UPDATE ON `0_hrm_pay_api_001_webhook_inbound_replay_defenses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_inbound_replay_defenses row cannot be updated';

CREATE TRIGGER `0_papi1_wid_d` BEFORE DELETE ON `0_hrm_pay_api_001_webhook_inbound_replay_defenses` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-API-001 immutable hrm_pay_api_001_webhook_inbound_replay_defenses row cannot be deleted';

CREATE TRIGGER `0_hanl1_md_u` BEFORE UPDATE ON `0_hrm_anl_001_metric_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_metric_definitions row cannot be updated';

CREATE TRIGGER `0_hanl1_md_d` BEFORE DELETE ON `0_hrm_anl_001_metric_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_metric_definitions row cannot be deleted';

CREATE TRIGGER `0_hanl1_sp_u` BEFORE UPDATE ON `0_hrm_anl_001_suppression_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_suppression_policies row cannot be updated';

CREATE TRIGGER `0_hanl1_sp_d` BEFORE DELETE ON `0_hrm_anl_001_suppression_policies` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_suppression_policies row cannot be deleted';

CREATE TRIGGER `0_hanl1_sr_u` BEFORE UPDATE ON `0_hrm_anl_001_snapshot_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_snapshot_requests row cannot be updated';

CREATE TRIGGER `0_hanl1_sr_d` BEFORE DELETE ON `0_hrm_anl_001_snapshot_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_snapshot_requests row cannot be deleted';

CREATE TRIGGER `0_hanl1_ss_u` BEFORE UPDATE ON `0_hrm_anl_001_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_snapshots row cannot be updated';

CREATE TRIGGER `0_hanl1_ss_d` BEFORE DELETE ON `0_hrm_anl_001_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_snapshots row cannot be deleted';

CREATE TRIGGER `0_hanl1_qa_u` BEFORE UPDATE ON `0_hrm_anl_001_query_audits` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_query_audits row cannot be updated';

CREATE TRIGGER `0_hanl1_qa_d` BEFORE DELETE ON `0_hrm_anl_001_query_audits` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_query_audits row cannot be deleted';

CREATE TRIGGER `0_hanl1_da_u` BEFORE UPDATE ON `0_hrm_anl_001_drilldown_authorizations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_drilldown_authorizations row cannot be updated';

CREATE TRIGGER `0_hanl1_da_d` BEFORE DELETE ON `0_hrm_anl_001_drilldown_authorizations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-ANL-001 immutable hrm_anl_001_drilldown_authorizations row cannot be deleted';

CREATE TRIGGER `0_pai1_rq_u` BEFORE UPDATE ON `0_hrm_pay_ai_001_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AI-001 immutable hrm_pay_ai_001_requests row cannot be updated';

CREATE TRIGGER `0_pai1_rq_d` BEFORE DELETE ON `0_hrm_pay_ai_001_requests` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AI-001 immutable hrm_pay_ai_001_requests row cannot be deleted';

CREATE TRIGGER `0_pai1_ou_u` BEFORE UPDATE ON `0_hrm_pay_ai_001_outputs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AI-001 immutable hrm_pay_ai_001_outputs row cannot be updated';

CREATE TRIGGER `0_pai1_ou_d` BEFORE DELETE ON `0_hrm_pay_ai_001_outputs` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AI-001 immutable hrm_pay_ai_001_outputs row cannot be deleted';

CREATE TRIGGER `0_pai1_rv_u` BEFORE UPDATE ON `0_hrm_pay_ai_001_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AI-001 immutable hrm_pay_ai_001_reviews row cannot be updated';

CREATE TRIGGER `0_pai1_rv_d` BEFORE DELETE ON `0_hrm_pay_ai_001_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-AI-001 immutable hrm_pay_ai_001_reviews row cannot be deleted';

CREATE TRIGGER `0_htal1_sd_u` BEFORE UPDATE ON `0_hrm_tal_001_skill_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_skill_definitions row cannot be updated';

CREATE TRIGGER `0_htal1_sd_d` BEFORE DELETE ON `0_hrm_tal_001_skill_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_skill_definitions row cannot be deleted';

CREATE TRIGGER `0_htal1_se_u` BEFORE UPDATE ON `0_hrm_tal_001_employee_skill_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_employee_skill_evidence row cannot be updated';

CREATE TRIGGER `0_htal1_se_d` BEFORE DELETE ON `0_hrm_tal_001_employee_skill_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_employee_skill_evidence row cannot be deleted';

CREATE TRIGGER `0_htal1_cd_u` BEFORE UPDATE ON `0_hrm_tal_001_certification_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_certification_definitions row cannot be updated';

CREATE TRIGGER `0_htal1_cd_d` BEFORE DELETE ON `0_hrm_tal_001_certification_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_certification_definitions row cannot be deleted';

CREATE TRIGGER `0_htal1_ce_u` BEFORE UPDATE ON `0_hrm_tal_001_employee_certification_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_employee_certification_evidence row cannot be updated';

CREATE TRIGGER `0_htal1_ce_d` BEFORE DELETE ON `0_hrm_tal_001_employee_certification_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_employee_certification_evidence row cannot be deleted';

CREATE TRIGGER `0_htal1_xa_u` BEFORE UPDATE ON `0_hrm_tal_001_export_audit` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_export_audit row cannot be updated';

CREATE TRIGGER `0_htal1_xa_d` BEFORE DELETE ON `0_hrm_tal_001_export_audit` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_export_audit row cannot be deleted';

CREATE TRIGGER `0_htal1_pc_u` BEFORE UPDATE ON `0_hrm_tal_001_performance_cycle_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_cycle_definitions row cannot be updated';

CREATE TRIGGER `0_htal1_pc_d` BEFORE DELETE ON `0_hrm_tal_001_performance_cycle_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_cycle_definitions row cannot be deleted';

CREATE TRIGGER `0_htal1_pr_u` BEFORE UPDATE ON `0_hrm_tal_001_performance_review_subjects` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_review_subjects row cannot be updated';

CREATE TRIGGER `0_htal1_pr_d` BEFORE DELETE ON `0_hrm_tal_001_performance_review_subjects` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_review_subjects row cannot be deleted';

CREATE TRIGGER `0_htal1_pa_u` BEFORE UPDATE ON `0_hrm_tal_001_performance_assessment_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_assessment_evidence row cannot be updated';

CREATE TRIGGER `0_htal1_pa_d` BEFORE DELETE ON `0_hrm_tal_001_performance_assessment_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_assessment_evidence row cannot be deleted';

CREATE TRIGGER `0_htal1_pw_u` BEFORE UPDATE ON `0_hrm_tal_001_performance_workflow_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_workflow_events row cannot be updated';

CREATE TRIGGER `0_htal1_pw_d` BEFORE DELETE ON `0_hrm_tal_001_performance_workflow_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_performance_workflow_events row cannot be deleted';

CREATE TRIGGER `0_htal1_ld_u` BEFORE UPDATE ON `0_hrm_tal_001_learning_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_learning_definitions row cannot be updated';

CREATE TRIGGER `0_htal1_ld_d` BEFORE DELETE ON `0_hrm_tal_001_learning_definitions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_learning_definitions row cannot be deleted';

CREATE TRIGGER `0_htal1_la_u` BEFORE UPDATE ON `0_hrm_tal_001_learning_assignments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_learning_assignments row cannot be updated';

CREATE TRIGGER `0_htal1_la_d` BEFORE DELETE ON `0_hrm_tal_001_learning_assignments` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_learning_assignments row cannot be deleted';

CREATE TRIGGER `0_htal1_lc_u` BEFORE UPDATE ON `0_hrm_tal_001_learning_completion_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_learning_completion_evidence row cannot be updated';

CREATE TRIGGER `0_htal1_lc_d` BEFORE DELETE ON `0_hrm_tal_001_learning_completion_evidence` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_learning_completion_evidence row cannot be deleted';

CREATE TRIGGER `0_htal1_cc_u` BEFORE UPDATE ON `0_hrm_tal_001_confidential_cases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_confidential_cases row cannot be updated';

CREATE TRIGGER `0_htal1_cc_d` BEFORE DELETE ON `0_hrm_tal_001_confidential_cases` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_confidential_cases row cannot be deleted';

CREATE TRIGGER `0_htal1_cg_u` BEFORE UPDATE ON `0_hrm_tal_001_confidential_case_access_grants` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_confidential_case_access_grants row cannot be updated';

CREATE TRIGGER `0_htal1_cg_d` BEFORE DELETE ON `0_hrm_tal_001_confidential_case_access_grants` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='HRM-TAL-001 immutable hrm_tal_001_confidential_case_access_grants row cannot be deleted';

CREATE TRIGGER `0_peco1_pp_u` BEFORE UPDATE ON `0_pay_eco_001_publisher_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_publisher_profiles row cannot be updated';

CREATE TRIGGER `0_peco1_pp_d` BEFORE DELETE ON `0_pay_eco_001_publisher_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_publisher_profiles row cannot be deleted';

CREATE TRIGGER `0_peco1_pc_u` BEFORE UPDATE ON `0_pay_eco_001_pack_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_pack_candidates row cannot be updated';

CREATE TRIGGER `0_peco1_pc_d` BEFORE DELETE ON `0_pay_eco_001_pack_candidates` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_pack_candidates row cannot be deleted';

CREATE TRIGGER `0_peco1_cd_u` BEFORE UPDATE ON `0_pay_eco_001_catalog_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_catalog_decisions row cannot be updated';

CREATE TRIGGER `0_peco1_cd_d` BEFORE DELETE ON `0_pay_eco_001_catalog_decisions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_catalog_decisions row cannot be deleted';

CREATE TRIGGER `0_peco1_se_u` BEFORE UPDATE ON `0_pay_eco_001_sandbox_evaluations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_sandbox_evaluations row cannot be updated';

CREATE TRIGGER `0_peco1_se_d` BEFORE DELETE ON `0_pay_eco_001_sandbox_evaluations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_sandbox_evaluations row cannot be deleted';

CREATE TRIGGER `0_peco1_ds_u` BEFORE UPDATE ON `0_pay_eco_001_dependency_scans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_dependency_scans row cannot be updated';

CREATE TRIGGER `0_peco1_ds_d` BEFORE DELETE ON `0_pay_eco_001_dependency_scans` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_dependency_scans row cannot be deleted';

CREATE TRIGGER `0_peco1_vs_u` BEFORE UPDATE ON `0_pay_eco_001_vulnerability_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_vulnerability_snapshots row cannot be updated';

CREATE TRIGGER `0_peco1_vs_d` BEFORE DELETE ON `0_pay_eco_001_vulnerability_snapshots` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_vulnerability_snapshots row cannot be deleted';

CREATE TRIGGER `0_peco1_va_u` BEFORE UPDATE ON `0_pay_eco_001_vulnerability_advisories` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_vulnerability_advisories row cannot be updated';

CREATE TRIGGER `0_peco1_va_d` BEFORE DELETE ON `0_pay_eco_001_vulnerability_advisories` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_vulnerability_advisories row cannot be deleted';

CREATE TRIGGER `0_peco1_vc_u` BEFORE UPDATE ON `0_pay_eco_001_vulnerability_correlations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_vulnerability_correlations row cannot be updated';

CREATE TRIGGER `0_peco1_vc_d` BEFORE DELETE ON `0_pay_eco_001_vulnerability_correlations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_vulnerability_correlations row cannot be deleted';

CREATE TRIGGER `0_peco1_aa_u` BEFORE UPDATE ON `0_pay_eco_001_artifact_admissions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_artifact_admissions row cannot be updated';

CREATE TRIGGER `0_peco1_aa_d` BEFORE DELETE ON `0_pay_eco_001_artifact_admissions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_artifact_admissions row cannot be deleted';

CREATE TRIGGER `0_peco1_cp_u` BEFORE UPDATE ON `0_pay_eco_001_catalog_publications` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_catalog_publications row cannot be updated';

CREATE TRIGGER `0_peco1_cp_d` BEFORE DELETE ON `0_pay_eco_001_catalog_publications` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_catalog_publications row cannot be deleted';

CREATE TRIGGER `0_peco1_cl_u` BEFORE UPDATE ON `0_pay_eco_001_catalog_lifecycle_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_catalog_lifecycle_events row cannot be updated';

CREATE TRIGGER `0_peco1_cl_d` BEFORE DELETE ON `0_pay_eco_001_catalog_lifecycle_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_catalog_lifecycle_events row cannot be deleted';

CREATE TRIGGER `0_peco1_mi_u` BEFORE UPDATE ON `0_pay_eco_001_marketplace_installations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_marketplace_installations row cannot be updated';

CREATE TRIGGER `0_peco1_mi_d` BEFORE DELETE ON `0_pay_eco_001_marketplace_installations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_marketplace_installations row cannot be deleted';

CREATE TRIGGER `0_peco1_ma_u` BEFORE UPDATE ON `0_pay_eco_001_marketplace_activations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_marketplace_activations row cannot be updated';

CREATE TRIGGER `0_peco1_ma_d` BEFORE DELETE ON `0_pay_eco_001_marketplace_activations` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-ECO-001 immutable pay_eco_001_marketplace_activations row cannot be deleted';

CREATE TRIGGER `0_pr1_pf_u` BEFORE UPDATE ON `0_hrm_pay_rec_001_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_profiles row cannot be updated';

CREATE TRIGGER `0_pr1_pf_d` BEFORE DELETE ON `0_hrm_pay_rec_001_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_profiles row cannot be deleted';

CREATE TRIGGER `0_pr1_es_u` BEFORE UPDATE ON `0_hrm_pay_rec_001_evidence_sets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_evidence_sets row cannot be updated';

CREATE TRIGGER `0_pr1_es_d` BEFORE DELETE ON `0_hrm_pay_rec_001_evidence_sets` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_evidence_sets row cannot be deleted';

CREATE TRIGGER `0_pr1_cl_u` BEFORE UPDATE ON `0_hrm_pay_rec_001_checklists` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_checklists row cannot be updated';

CREATE TRIGGER `0_pr1_cl_d` BEFORE DELETE ON `0_hrm_pay_rec_001_checklists` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_checklists row cannot be deleted';

CREATE TRIGGER `0_pr1_rv_u` BEFORE UPDATE ON `0_hrm_pay_rec_001_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_reviews row cannot be updated';

CREATE TRIGGER `0_pr1_rv_d` BEFORE DELETE ON `0_hrm_pay_rec_001_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_reviews row cannot be deleted';

CREATE TRIGGER `0_pr1_ap_u` BEFORE UPDATE ON `0_hrm_pay_rec_001_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_approvals row cannot be updated';

CREATE TRIGGER `0_pr1_ap_d` BEFORE DELETE ON `0_hrm_pay_rec_001_approvals` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_approvals row cannot be deleted';

CREATE TRIGGER `0_pr1_se_u` BEFORE UPDATE ON `0_hrm_pay_rec_001_state_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_state_events row cannot be updated';

CREATE TRIGGER `0_pr1_se_d` BEFORE DELETE ON `0_hrm_pay_rec_001_state_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-REC-001 immutable hrm_pay_rec_001_state_events row cannot be deleted';

CREATE TRIGGER `0_pcon1_pp_u` BEFORE UPDATE ON `0_hrm_pay_con_001_provider_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CON-001 immutable hrm_pay_con_001_provider_profiles row cannot be updated';

CREATE TRIGGER `0_pcon1_pp_d` BEFORE DELETE ON `0_hrm_pay_con_001_provider_profiles` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CON-001 immutable hrm_pay_con_001_provider_profiles row cannot be deleted';

CREATE TRIGGER `0_pcon1_mv_u` BEFORE UPDATE ON `0_hrm_pay_con_001_mapping_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CON-001 immutable hrm_pay_con_001_mapping_versions row cannot be updated';

CREATE TRIGGER `0_pcon1_mv_d` BEFORE DELETE ON `0_hrm_pay_con_001_mapping_versions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CON-001 immutable hrm_pay_con_001_mapping_versions row cannot be deleted';

CREATE TRIGGER `0_pcon1_par_u` BEFORE UPDATE ON `0_hrm_pay_con_001_provider_admission_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CON-001 immutable hrm_pay_con_001_provider_admission_reviews row cannot be updated';

CREATE TRIGGER `0_pcon1_par_d` BEFORE DELETE ON `0_hrm_pay_con_001_provider_admission_reviews` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='PAY-CON-001 immutable hrm_pay_con_001_provider_admission_reviews row cannot be deleted';

CREATE TRIGGER `0_pgn_e_u` BEFORE UPDATE ON `0_hrm_pay_glb_001_native_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_executions UPDATE denied';

CREATE TRIGGER `0_pgn_e_d` BEFORE DELETE ON `0_hrm_pay_glb_001_native_executions` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_executions DELETE denied';

CREATE TRIGGER `0_pgn_m_u` BEFORE UPDATE ON `0_hrm_pay_glb_001_native_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_members UPDATE denied';

CREATE TRIGGER `0_pgn_m_d` BEFORE DELETE ON `0_hrm_pay_glb_001_native_members` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_members DELETE denied';

CREATE TRIGGER `0_pgn_s_u` BEFORE UPDATE ON `0_hrm_pay_glb_001_native_selection_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_selection_events UPDATE denied';

CREATE TRIGGER `0_pgn_s_d` BEFORE DELETE ON `0_hrm_pay_glb_001_native_selection_events` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_selection_events DELETE denied';

CREATE TRIGGER `0_pgn_a_u` BEFORE UPDATE ON `0_hrm_pay_glb_001_native_approval_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_approval_bindings UPDATE denied';

CREATE TRIGGER `0_pgn_a_d` BEFORE DELETE ON `0_hrm_pay_glb_001_native_approval_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_approval_bindings DELETE denied';

CREATE TRIGGER `0_pgn_g_u` BEFORE UPDATE ON `0_hrm_pay_glb_001_native_gl_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_gl_bindings UPDATE denied';

CREATE TRIGGER `0_pgn_g_d` BEFORE DELETE ON `0_hrm_pay_glb_001_native_gl_bindings` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_gl_bindings DELETE denied';

CREATE TRIGGER `0_pgn_c_u` BEFORE UPDATE ON `0_hrm_pay_glb_001_native_command_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_command_receipts UPDATE denied';

CREATE TRIGGER `0_pgn_c_d` BEFORE DELETE ON `0_hrm_pay_glb_001_native_command_receipts` FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='S39 immutable hrm_pay_glb_001_native_command_receipts DELETE denied';


-- Reviewed dump-gap reconciliation, 2026-10-16.
-- Retain actual legacy data and grants; do not add the 17 unused development columns.
ALTER TABLE `0_crm_activities` ADD COLUMN `priority` tinyint(1) NOT NULL DEFAULT 0;

ALTER TABLE `0_debtors_master` MODIFY COLUMN `company_size` enum('small','medium','large','enterprise') COLLATE utf8_unicode_ci DEFAULT 'medium', MODIFY COLUMN `credit_insurance_limit` double DEFAULT 0, MODIFY COLUMN `credit_risk_score` enum('low','medium','high','critical') COLLATE utf8_unicode_ci DEFAULT 'medium', MODIFY COLUMN `customer_tier` enum('standard','silver','gold','platinum','vip') COLLATE utf8_unicode_ci DEFAULT 'standard', MODIFY COLUMN `industry` varchar(50) COLLATE utf8_unicode_ci DEFAULT '', MODIFY COLUMN `payment_behavior_score` double DEFAULT 0, MODIFY COLUMN `preferred_communication` enum('email','phone','whatsapp','portal') COLLATE utf8_unicode_ci DEFAULT 'email';

ALTER TABLE `0_sales_agreement_lines` ADD COLUMN `min_qty_per_order` double NOT NULL DEFAULT 0, MODIFY COLUMN `custom_data` longtext COLLATE utf8_unicode_ci DEFAULT NULL;

ALTER TABLE `0_sales_agreements` MODIFY COLUMN `created_date` datetime NULL DEFAULT CURRENT_TIMESTAMP, MODIFY COLUMN `custom_data` longtext COLLATE utf8_unicode_ci DEFAULT NULL, MODIFY COLUMN `date_end` date DEFAULT NULL, MODIFY COLUMN `reference` varchar(60) COLLATE utf8_unicode_ci NOT NULL DEFAULT '';

ALTER TABLE `0_sales_orders` MODIFY COLUMN `cost_total` double DEFAULT 0, MODIFY COLUMN `margin_total` double DEFAULT 0, MODIFY COLUMN `opportunity_id` int(11) DEFAULT 0, MODIFY COLUMN `template_id` int(11) DEFAULT 0;

ALTER TABLE `0_sales_quotation_template_lines` MODIFY COLUMN `discount_percent` double DEFAULT 0, MODIFY COLUMN `is_optional` tinyint(1) DEFAULT 0, MODIFY COLUMN `line_type` enum('product','section','note','optional') DEFAULT 'product', MODIFY COLUMN `quantity` double DEFAULT 1, MODIFY COLUMN `sort_order` int(11) DEFAULT 0, MODIFY COLUMN `stock_id` varchar(20) DEFAULT '';

ALTER TABLE `0_sales_quotation_templates` MODIFY COLUMN `default_payment_terms` int(11) DEFAULT 0, MODIFY COLUMN `default_ship_via` int(11) DEFAULT 0, MODIFY COLUMN `inactive` tinyint(1) DEFAULT 0, MODIFY COLUMN `sales_type` int(11) DEFAULT 0, MODIFY COLUMN `validity_days` int(11) DEFAULT 30;

ALTER TABLE `0_wh_scrap` MODIFY COLUMN `from_loc_id` int(11) DEFAULT NULL;

ALTER TABLE `0_quality_inspection_readings` ADD CONSTRAINT `fk_qir_inspection` FOREIGN KEY (`inspection_id`) REFERENCES `0_quality_inspections` (`id`) ON DELETE CASCADE;

ALTER TABLE `0_quality_inspection_readings` ADD CONSTRAINT `fk_qir_parameter` FOREIGN KEY (`parameter_id`) REFERENCES `0_quality_parameters` (`id`);

ALTER TABLE `0_recall_items` ADD CONSTRAINT `fk_ri_recall` FOREIGN KEY (`recall_id`) REFERENCES `0_recall_campaigns` (`id`) ON DELETE CASCADE;

ALTER TABLE `0_sales_agreement_lines` ADD CONSTRAINT `fk_sales_agreement_lines_header` FOREIGN KEY (`agreement_id`) REFERENCES `0_sales_agreements` (`id`) ON DELETE CASCADE;

ALTER TABLE `0_warranty_claim_parts` ADD CONSTRAINT `fk_wcp_claim` FOREIGN KEY (`claim_id`) REFERENCES `0_warranty_claims` (`id`) ON DELETE CASCADE;

ALTER TABLE `0_wh_bin_stock` ADD CONSTRAINT `fk_bin_stock_item` FOREIGN KEY (`stock_id`) REFERENCES `0_stock_master` (`stock_id`);

ALTER TABLE `0_wh_bin_stock` ADD CONSTRAINT `fk_bin_stock_loc` FOREIGN KEY (`wh_loc_id`) REFERENCES `0_wh_locations` (`loc_id`);

ALTER TABLE `0_wh_cycle_count_lines` ADD CONSTRAINT `fk_cc_line_count` FOREIGN KEY (`count_id`) REFERENCES `0_wh_cycle_counts` (`count_id`) ON DELETE CASCADE;

ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_parent` FOREIGN KEY (`parent_loc_id`) REFERENCES `0_wh_locations` (`loc_id`);

ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_storage_cat` FOREIGN KEY (`storage_category_id`) REFERENCES `0_wh_storage_categories` (`id`) ON DELETE SET NULL;

ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_type` FOREIGN KEY (`location_type_id`) REFERENCES `0_wh_location_types` (`id`);

ALTER TABLE `0_wh_locations` ADD CONSTRAINT `fk_wh_loc_warehouse` FOREIGN KEY (`warehouse_loc_code`) REFERENCES `0_locations` (`loc_code`);

ALTER TABLE `0_wh_material_request_lines` ADD CONSTRAINT `fk_mr_line_req` FOREIGN KEY (`request_id`) REFERENCES `0_wh_material_requests` (`request_id`) ON DELETE CASCADE;

ALTER TABLE `0_wh_operation_lines` ADD CONSTRAINT `fk_op_line_op` FOREIGN KEY (`op_id`) REFERENCES `0_wh_operations` (`op_id`) ON DELETE CASCADE;

ALTER TABLE `0_wh_package_contents` ADD CONSTRAINT `fk_pkg_content_pkg` FOREIGN KEY (`package_id`) REFERENCES `0_wh_packages` (`package_id`) ON DELETE CASCADE;

ALTER TABLE `0_wh_return_order_lines` ADD CONSTRAINT `fk_ret_line_ret` FOREIGN KEY (`return_id`) REFERENCES `0_wh_return_orders` (`return_id`) ON DELETE CASCADE;

ALTER TABLE `0_wh_route_rules` ADD CONSTRAINT `fk_rule_route` FOREIGN KEY (`route_id`) REFERENCES `0_wh_routes` (`route_id`) ON DELETE CASCADE;

ALTER TABLE `0_wh_transfer_order_lines` ADD CONSTRAINT `fk_to_line_transfer` FOREIGN KEY (`transfer_id`) REFERENCES `0_wh_transfer_orders` (`transfer_id`) ON DELETE CASCADE;

ALTER TABLE `0_sales_agreement_lines` ADD KEY `idx_stock` (`stock_id`);

UPDATE `0_document_types` SET security_class='identity' WHERE type_name='Passport' AND security_class='restricted';

UPDATE `0_document_types` SET security_class='identity' WHERE type_name='National ID' AND security_class='restricted';

UPDATE `0_document_types` SET security_class='identity' WHERE type_name='Driving License' AND security_class='restricted';

UPDATE `0_document_types` SET security_class='work_authorization' WHERE type_name='Residence Permit' AND security_class='restricted';

UPDATE `0_document_types` SET security_class='work_authorization' WHERE type_name='Work Permit' AND security_class='restricted';

UPDATE `0_document_types` SET security_class='medical' WHERE type_name='Medical Certificate' AND security_class='restricted';

-- Unused singular alias; the maintained preference is the plural key.
DELETE FROM `0_sys_prefs` WHERE name='purchase_agreement_expiry_alert_day';
