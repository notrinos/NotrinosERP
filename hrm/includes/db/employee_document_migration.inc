<?php
/**
 * PAY-SEC-002 employee-document content migration helpers.
 *
 * These procedural helpers extend the existing HRM repository and attachment
 * service. They retain source bytes, write only hashed source references to the
 * append-only journal, and never make quarantined or partial content available.
 */

include_once($path_to_root.'/hrm/includes/db/employee_document_db.inc');

/**
 * Return the numeric actor for migration evidence.
 *
 * @return int
 */
function employee_document_migration_actor_id() {
	return isset($_SESSION['wa_current_user']) && isset($_SESSION['wa_current_user']->user)
		? (int)$_SESSION['wa_current_user']->user : 0;
}

/**
 * Load the latest append-only journal event for a document.
 *
 * @param int $doc_id
 * @return array|false
 */
function get_latest_employee_document_migration_event($doc_id) {
	$sql = "SELECT * FROM ".TB_PREF."employee_document_migration_journal
		WHERE doc_id = ".db_escape((int)$doc_id)." ORDER BY journal_id DESC LIMIT 1";
	$result = db_query($sql, 'could not read employee document migration journal');
	return db_fetch_assoc($result);
}

/**
 * Append one normalized migration journal event.
 *
 * @param array $event
 * @return int|false
 */
function append_employee_document_migration_event($event) {
	$defaults = array(
		'doc_id' => 0, 'source_path_hash' => hash('sha256', 'metadata-only'),
		'source_size' => null, 'source_mtime' => null, 'source_sha256' => null,
		'detected_mime' => null, 'target_attachment_id' => null,
		'policy_version' => 'hrm-document-v1', 'scanner_status' => 'not_scanned',
		'status' => 'migration_failed', 'error_code' => 'unknown',
		'actor_id' => employee_document_migration_actor_id(), 'batch_id' => '',
	);
	$event = array_merge($defaults, $event);
	$attempt_result = db_query("SELECT COALESCE(MAX(attempt_no), 0) attempt_no FROM "
		.TB_PREF."employee_document_migration_journal WHERE doc_id = ".db_escape((int)$event['doc_id']));
	$attempt_row = db_fetch_assoc($attempt_result);
	$attempt = $attempt_row ? ((int)$attempt_row['attempt_no'] + 1) : 1;
	$sql = "INSERT INTO ".TB_PREF."employee_document_migration_journal
		(doc_id, source_path_hash, source_size, source_mtime, source_sha256,
		 detected_mime, target_attachment_id, attempt_no, policy_version,
		 scanner_status, status, error_code, actor_id, batch_id)
		VALUES (".db_escape((int)$event['doc_id']).", ".db_escape($event['source_path_hash']).", "
		.($event['source_size'] === null ? 'NULL' : db_escape((int)$event['source_size'])).", "
		.($event['source_mtime'] === null ? 'NULL' : db_escape((int)$event['source_mtime'])).", "
		.($event['source_sha256'] === null ? 'NULL' : db_escape($event['source_sha256'])).", "
		.($event['detected_mime'] === null ? 'NULL' : db_escape($event['detected_mime'])).", "
		.($event['target_attachment_id'] === null ? 'NULL' : db_escape((int)$event['target_attachment_id'])).", "
		.db_escape($attempt).", ".db_escape($event['policy_version']).", "
		.db_escape($event['scanner_status']).", ".db_escape($event['status']).", "
		.db_escape($event['error_code']).", ".db_escape((int)$event['actor_id']).", "
		.db_escape($event['batch_id']).")";
	if (!db_query($sql)) {
		return false;
	}
	return (int)db_insert_id();
}

/**
 * Update a document to an inaccessible terminal migration state and journal it.
 *
 * @param array $document
 * @param string $source_path_hash
 * @param array $validation
 * @param string $status
 * @param string $batch_id
 * @return bool
 */
function quarantine_employee_document_migration($document, $source_path_hash, $validation, $status, $batch_id) {
	begin_transaction();
	$sql = "UPDATE ".TB_PREF."employee_documents SET content_state = ".db_escape($status).",
		content_sha256 = NULL, content_size = NULL, content_mime = NULL
		WHERE doc_id = ".db_escape((int)$document['doc_id']);
	if (!db_query($sql)) {
		cancel_transaction();
		return false;
	}
	$journal_id = append_employee_document_migration_event(array(
		'doc_id' => (int)$document['doc_id'],
		'source_path_hash' => $source_path_hash,
		'source_size' => isset($validation['size']) ? (int)$validation['size'] : null,
		'source_mtime' => isset($validation['mtime']) ? (int)$validation['mtime'] : null,
		'source_sha256' => !empty($validation['sha256']) ? $validation['sha256'] : null,
		'detected_mime' => !empty($validation['mime']) ? $validation['mime'] : null,
		'scanner_status' => !empty($validation['scanner_status']) ? $validation['scanner_status'] : 'not_scanned',
		'status' => $status,
		'error_code' => !empty($validation['error_code']) ? $validation['error_code'] : 'migration_validation_failed',
		'batch_id' => $batch_id,
	));
	if ($journal_id === false) {
		cancel_transaction();
		return false;
	}
	commit_transaction();
	return true;
}

/**
 * Build immutable private-storage metadata for a migrated attachment.
 *
 * @param array $existing_custom
 * @param array $document
 * @param array $validation
 * @param string $source_kind
 * @return array
 */
function employee_document_migrated_attachment_metadata($existing_custom, $document, $validation, $source_kind) {
	return array_merge($existing_custom, array(
		'employee_document_id' => (int)$document['doc_id'],
		'migration_source' => $source_kind,
		'storage_backend' => 'hrm_private_v1',
		'encryption_format' => 'aes-256-cbc-hmac-sha256-v1',
		'content_state' => 'available',
		'plaintext_sha256' => $validation['sha256'],
		'plaintext_size' => (int)$validation['size'],
		'validated_mime' => $validation['mime'],
		'scanner_status' => 'clean',
		'content_policy_version' => 'hrm-document-v1',
	));
}

/**
 * Migrate one validated legacy-only document into a new core attachment link.
 *
 * @param array $document
 * @param array $validation
 * @param string $source_path_hash
 * @param string $batch_id
 * @return bool
 */
function migrate_legacy_employee_document_content($document, $validation, $source_path_hash, $batch_id) {
	begin_transaction();
	$stored = create_private_hrm_attachment(
		(int)$document['employee_number'],
		$document['doc_name'],
		$validation,
		employee_document_migrated_attachment_metadata(array(), $document, $validation, 'legacy_file_path')
	);
	if ($stored === false) {
		cancel_transaction();
		return false;
	}
	$sql = "UPDATE ".TB_PREF."employee_documents SET
		attachment_id = ".db_escape((int)$stored['attachment_id']).",
		content_state = 'available', content_sha256 = ".db_escape($validation['sha256']).",
		content_size = ".db_escape((int)$validation['size']).",
		content_mime = ".db_escape($validation['mime'])."
		WHERE doc_id = ".db_escape((int)$document['doc_id'])." AND attachment_id IS NULL";
	if (!db_query($sql)) {
		cancel_transaction();
		@unlink($stored['path']);
		return false;
	}
	$journal_id = append_employee_document_migration_event(array(
		'doc_id' => (int)$document['doc_id'], 'source_path_hash' => $source_path_hash,
		'source_size' => (int)$validation['size'], 'source_mtime' => (int)$validation['mtime'],
		'source_sha256' => $validation['sha256'], 'detected_mime' => $validation['mime'],
		'target_attachment_id' => (int)$stored['attachment_id'], 'scanner_status' => 'clean',
		'status' => 'migrated', 'error_code' => '', 'batch_id' => $batch_id,
	));
	if ($journal_id === false) {
		cancel_transaction();
		@unlink($stored['path']);
		return false;
	}
	commit_transaction();
	return true;
}

/**
 * Move one existing core-linked employee attachment into private encryption.
 *
 * @param array $document
 * @param array $validation
 * @param string $source_path_hash
 * @param string $batch_id
 * @return bool
 */
function migrate_core_employee_document_content($document, $validation, $source_path_hash, $batch_id) {
	$stored = store_private_hrm_document($validation['tmp_name']);
	if ($stored === false || !attachment_hash_equals($stored['plaintext_sha256'], $validation['sha256'])) {
		if ($stored !== false) @unlink($stored['path']);
		return false;
	}
	$existing_custom = attachment_custom_data_array($document['attachment_custom_data']);
	$custom_json = json_encode(
		employee_document_migrated_attachment_metadata($existing_custom, $document, $validation, 'core_attachment'),
		JSON_UNESCAPED_UNICODE
	);
	if ($custom_json === false) {
		@unlink($stored['path']);
		return false;
	}
	begin_transaction();
	$sql = "UPDATE ".TB_PREF."attachments SET unique_name = ".db_escape($stored['object_key']).",
		filesize = ".db_escape((int)$validation['size']).", filetype = ".db_escape($validation['mime']).",
		custom_data = ".attachment_db_json($custom_json)."
		WHERE id = ".db_escape((int)$document['attachment_id'])."
		AND type_no = ".db_escape(ST_EMPLOYEE)." AND trans_no = ".db_escape((int)$document['employee_number']);
	if (!db_query($sql)) {
		cancel_transaction();
		@unlink($stored['path']);
		return false;
	}
	$sql = "UPDATE ".TB_PREF."employee_documents SET content_state = 'available',
		content_sha256 = ".db_escape($validation['sha256']).",
		content_size = ".db_escape((int)$validation['size']).",
		content_mime = ".db_escape($validation['mime'])."
		WHERE doc_id = ".db_escape((int)$document['doc_id'])."
		AND attachment_id = ".db_escape((int)$document['attachment_id']);
	if (!db_query($sql)) {
		cancel_transaction();
		@unlink($stored['path']);
		return false;
	}
	$journal_id = append_employee_document_migration_event(array(
		'doc_id' => (int)$document['doc_id'], 'source_path_hash' => $source_path_hash,
		'source_size' => (int)$validation['size'], 'source_mtime' => (int)$validation['mtime'],
		'source_sha256' => $validation['sha256'], 'detected_mime' => $validation['mime'],
		'target_attachment_id' => (int)$document['attachment_id'], 'scanner_status' => 'clean',
		'status' => 'migrated', 'error_code' => '', 'batch_id' => $batch_id,
	));
	if ($journal_id === false) {
		cancel_transaction();
		@unlink($stored['path']);
		return false;
	}
	commit_transaction();
	return true;
}

/**
 * Reconcile an already-private document and detect changed legacy source bytes.
 *
 * @param array $document
 * @param string $batch_id
 * @return bool
 */
function reconcile_private_employee_document($document, $batch_id) {
	$decoded_custom = attachment_custom_data_array($document['attachment_custom_data']);
	$attachment = array(
		'unique_name' => $document['attachment_unique_name'],
		'custom_data' => $decoded_custom,
	);
	$plaintext = read_private_hrm_attachment($attachment);
	if ($plaintext === false
		|| !attachment_hash_equals(hash('sha256', $plaintext), $document['content_sha256'])
		|| strlen($plaintext) !== (int)$document['content_size']
	) {
		return false;
	}
	if (is_string($document['attachment_custom_data'])
		&& strpos($document['attachment_custom_data'], '&quot;') !== false
	) {
		$normalized_json = json_encode($decoded_custom, JSON_UNESCAPED_UNICODE);
		if ($normalized_json === false || !db_query("UPDATE ".TB_PREF."attachments SET custom_data="
			.attachment_db_json($normalized_json)." WHERE id=".db_escape((int)$document['attachment_id']))) {
			return false;
		}
	}
	if (!empty($document['file_path'])) {
		$source = canonical_historical_employee_document_path($document['file_path']);
		$latest = get_latest_employee_document_migration_event((int)$document['doc_id']);
		if ($source === false || !$latest || empty($latest['source_sha256'])
			|| !attachment_hash_equals(hash_file('sha256', $source), $latest['source_sha256'])
		) {
			$validation = array(
				'error_code' => 'source_changed_after_migration', 'scanner_status' => 'not_scanned',
				'size' => $source === false ? null : filesize($source),
				'mtime' => $source === false ? null : filemtime($source),
				'sha256' => $source === false ? null : hash_file('sha256', $source),
			);
			return quarantine_employee_document_migration(
				$document, hash('sha256', 'legacy:'.$document['file_path']),
				$validation, 'migration_conflict', $batch_id
			);
		}
	}
	return true;
}

/**
 * Backfill opaque access keys without exposing document identifiers.
 *
 * @return bool
 */
function backfill_employee_document_access_keys() {
	$result = db_query("SELECT doc_id FROM ".TB_PREF."employee_documents
		WHERE access_key IS NULL OR access_key = '' ORDER BY doc_id");
	while ($row = db_fetch_assoc($result)) {
		$updated = false;
		for ($attempt = 0; $attempt < 5 && !$updated; $attempt++) {
			$key = generate_employee_document_access_key();
			if ($key === false) return false;
			$updated = db_query("UPDATE ".TB_PREF."employee_documents SET access_key = ".db_escape($key)
				." WHERE doc_id = ".db_escape((int)$row['doc_id'])." AND (access_key IS NULL OR access_key = '')");
		}
		if (!$updated) return false;
	}
	return true;
}

/**
 * Migrate and reconcile every employee document in the current company.
 *
 * @param int $company
 * @param string $batch_id
 * @param array $evidence Aggregate PII-free evidence output.
 * @return bool
 */
function migrate_all_employee_document_content($company, $batch_id, &$evidence) {
	$evidence = array('core_migrated' => 0, 'legacy_migrated' => 0, 'already_private' => 0,
		'metadata_only' => 0, 'quarantined' => 0, 'failed' => 0);
	if (!harden_hrm_document_public_directories($company)
		|| ensure_hrm_document_private_storage($company) === false
		|| hrm_document_encryption_key($company) === false
		|| !backfill_employee_document_access_keys()
	) {
		return false;
	}
	$sql = "SELECT d.*, e.employee_number,
		a.unique_name attachment_unique_name, a.filename attachment_filename,
		a.filetype attachment_filetype, a.filesize attachment_filesize,
		a.custom_data attachment_custom_data, a.type_no attachment_type_no,
		a.trans_no attachment_trans_no
		FROM ".TB_PREF."employee_documents d
		LEFT JOIN ".TB_PREF."employees e ON e.employee_id = d.employee_id
		LEFT JOIN ".TB_PREF."attachments a ON a.id = d.attachment_id
		ORDER BY d.doc_id";
	$result = db_query($sql, 'could not inventory employee documents for migration');
	while ($document = db_fetch_assoc($result)) {
		if (empty($document['employee_number'])) {
			$evidence['failed']++;
			return false;
		}
		$custom = attachment_custom_data_array($document['attachment_custom_data']);
		if (!empty($document['attachment_id'])
			&& isset($custom['storage_backend']) && $custom['storage_backend'] === 'hrm_private_v1'
			&& isset($document['content_state']) && $document['content_state'] === 'available'
		) {
			if (!reconcile_private_employee_document($document, $batch_id)) return false;
			$evidence['already_private']++;
			continue;
		}
		if (empty($document['attachment_id']) && empty($document['file_path'])) {
			db_query("UPDATE ".TB_PREF."employee_documents SET content_state = 'metadata_only',
				content_sha256 = NULL, content_size = NULL, content_mime = NULL
				WHERE doc_id = ".db_escape((int)$document['doc_id']));
			if (!get_latest_employee_document_migration_event((int)$document['doc_id'])) {
				append_employee_document_migration_event(array(
					'doc_id' => (int)$document['doc_id'], 'status' => 'metadata_only',
					'error_code' => '', 'scanner_status' => 'not_required', 'batch_id' => $batch_id,
				));
			}
			$evidence['metadata_only']++;
			continue;
		}

		$source_kind = empty($document['attachment_id']) ? 'legacy' : 'core';
		if ($source_kind === 'legacy') {
			$source_path = canonical_historical_employee_document_path($document['file_path'], $company);
			$filename = $source_path === false ? '' : basename($source_path);
			$source_path_hash = hash('sha256', 'legacy:'.$document['file_path']);
		} else {
			if ((int)$document['attachment_type_no'] !== ST_EMPLOYEE
				|| (int)$document['attachment_trans_no'] !== (int)$document['employee_number']) return false;
			$source_path = get_safe_attachment_path($document['attachment_unique_name']);
			$filename = $document['attachment_filename'];
			$source_path_hash = hash('sha256', 'core:'.$document['attachment_unique_name']);
		}
		if ($source_path === false) {
			$validation = array('error_code' => 'unsafe_or_missing_source', 'scanner_status' => 'not_scanned');
			if (!quarantine_employee_document_migration($document, $source_path_hash, $validation, 'quarantined', $batch_id)) return false;
			$evidence['quarantined']++;
			continue;
		}
		$latest = get_latest_employee_document_migration_event((int)$document['doc_id']);
		if (($document['content_state'] === 'quarantined' || $document['content_state'] === 'migration_conflict')
			&& $latest && $latest['source_path_hash'] === $source_path_hash
			&& (int)$latest['source_size'] === (int)filesize($source_path)
			&& (int)$latest['source_mtime'] === (int)filemtime($source_path)
		) {
			$evidence['quarantined']++;
			continue;
		}
		$validation = validate_hrm_employee_document_file($source_path, $filename, 10 * 1024 * 1024, true);
		$validation['mtime'] = filemtime($source_path);
		if (!$validation['ok']) {
			if (!quarantine_employee_document_migration($document, $source_path_hash, $validation, 'quarantined', $batch_id)) return false;
			$evidence['quarantined']++;
			continue;
		}
		$migrated = $source_kind === 'legacy'
			? migrate_legacy_employee_document_content($document, $validation, $source_path_hash, $batch_id)
			: migrate_core_employee_document_content($document, $validation, $source_path_hash, $batch_id);
		if (!$migrated) {
			$evidence['failed']++;
			return false;
		}
		$evidence[$source_kind.'_migrated']++;
	}
	return true;
}

/**
 * Move unreferenced opaque private objects into the private quarantine area.
 * No byte is deleted; only current-company object keys are considered.
 *
 * @param int|null $company
 * @return int|false Number moved, or false on failure.
 */
function quarantine_unreferenced_hrm_private_objects($company = null) {
	$paths = ensure_hrm_document_private_storage($company);
	if ($paths === false) return false;
	$referenced = array();
	$result = db_query("SELECT unique_name,custom_data FROM ".TB_PREF."attachments WHERE type_no=".db_escape(ST_EMPLOYEE));
	while ($row = db_fetch_assoc($result)) {
		$custom = attachment_custom_data_array($row['custom_data']);
		if (isset($custom['storage_backend']) && $custom['storage_backend'] === 'hrm_private_v1') {
			$referenced[$row['unique_name']] = true;
		}
	}
	$moved = 0;
	$entries = scandir($paths['objects']);
	if ($entries === false) return false;
	foreach ($entries as $entry) {
		if ($entry === '.' || $entry === '..' || $entry === '.htaccess' || $entry === 'web.config') continue;
		if (!preg_match('/^[A-Za-z0-9_-]{16,128}$/D', $entry)) return false;
		if (isset($referenced[$entry])) continue;
		$source = $paths['objects'].DIRECTORY_SEPARATOR.$entry;
		$target = $paths['quarantine'].DIRECTORY_SEPARATOR.'orphan-'.$entry;
		if (!is_file($source) || file_exists($target) || !@rename($source, $target)) return false;
		$moved++;
	}
	return $moved;
}

/**
 * Verify all current employee-document states, links, hashes and journals.
 *
 * @param array $evidence
 * @return bool
 */
function reconcile_all_employee_document_content(&$evidence) {
	$orphan_objects = quarantine_unreferenced_hrm_private_objects();
	if ($orphan_objects === false) return false;
	$evidence = array('available' => 0, 'metadata_only' => 0, 'quarantined' => 0,
		'orphan_objects_quarantined' => (int)$orphan_objects);
	$sql = "SELECT d.*, e.employee_number, a.type_no, a.trans_no, a.unique_name,
		a.custom_data attachment_custom_data FROM ".TB_PREF."employee_documents d
		LEFT JOIN ".TB_PREF."employees e ON e.employee_id=d.employee_id
		LEFT JOIN ".TB_PREF."attachments a ON a.id=d.attachment_id ORDER BY d.doc_id";
	$result = db_query($sql, 'could not reconcile employee document migration');
	while ($document = db_fetch_assoc($result)) {
		if (empty($document['access_key']) || !preg_match('/^[a-f0-9]{64}$/D', $document['access_key'])) return false;
		$state = $document['content_state'];
		if ($state === 'available') {
			if (empty($document['attachment_id']) || (int)$document['type_no'] !== ST_EMPLOYEE
				|| (int)$document['trans_no'] !== (int)$document['employee_number']) return false;
			$plaintext = read_private_hrm_attachment(array(
				'unique_name' => $document['unique_name'], 'custom_data' => $document['attachment_custom_data']));
			if ($plaintext === false || strlen($plaintext) !== (int)$document['content_size']
				|| !attachment_hash_equals(hash('sha256', $plaintext), $document['content_sha256'])) return false;
			$custom = attachment_custom_data_array($document['attachment_custom_data']);
			$migration_source = isset($custom['migration_source']) ? $custom['migration_source'] : '';
			if ($migration_source === 'legacy_file_path' || $migration_source === 'core_attachment') {
				$event = get_latest_employee_document_migration_event((int)$document['doc_id']);
				if (!$event || $event['status'] !== 'migrated'
					|| (int)$event['target_attachment_id'] !== (int)$document['attachment_id']) return false;
			}
			$evidence['available']++;
		} elseif ($state === 'metadata_only') {
			if (!empty($document['attachment_id']) || !empty($document['file_path'])) return false;
			$evidence['metadata_only']++;
		} elseif ($state === 'quarantined' || $state === 'migration_conflict') {
			$evidence['quarantined']++;
		} else {
			return false;
		}
	}
	$orphan_result = db_query("SELECT COUNT(*) orphan_count FROM ".TB_PREF."attachments a
		LEFT JOIN ".TB_PREF."employee_documents d ON d.attachment_id=a.id
		WHERE a.type_no=".db_escape(ST_EMPLOYEE)." AND d.doc_id IS NULL");
	$orphan = db_fetch_assoc($orphan_result);
	return $orphan && (int)$orphan['orphan_count'] === 0;
}

/**
 * Copy the key and current encrypted objects into a temporary recovery set,
 * decrypt every copy, verify hashes, then remove only that exact temporary set.
 *
 * @param int $company
 * @param string $batch_id
 * @return bool
 */
function rehearse_employee_document_private_restore($company, $batch_id) {
	$paths = ensure_hrm_document_private_storage($company);
	$key = hrm_document_encryption_key($company);
	if ($paths === false || $key === false) return false;
	$rehearsal = $paths['root'].DIRECTORY_SEPARATOR.'recovery-rehearsal-'.$batch_id;
	if (file_exists($rehearsal) || !mkdir($rehearsal, 0700)) return false;
	$key_copy = $rehearsal.DIRECTORY_SEPARATOR.'content-v1.key.pem';
	if (file_put_contents($key_copy, base64_encode($key)."\n", LOCK_EX) === false) {
		@rmdir($rehearsal);
		return false;
	}
	$result = db_query("SELECT d.content_sha256, d.content_size, a.unique_name
		FROM ".TB_PREF."employee_documents d JOIN ".TB_PREF."attachments a ON a.id=d.attachment_id
		WHERE d.content_state='available' ORDER BY d.doc_id");
	$copies = array($key_copy);
	$ok = true;
	while ($row = db_fetch_assoc($result)) {
		$source = get_private_hrm_document_path($row['unique_name'], $company);
		$copy = $rehearsal.DIRECTORY_SEPARATOR.$row['unique_name'];
		if ($source === false || !copy($source, $copy)) { $ok = false; break; }
		$copies[] = $copy;
		$payload = file_get_contents($copy);
		$plaintext = $payload === false ? false : decrypt_hrm_document_bytes($payload, $key);
		if ($plaintext === false || strlen($plaintext) !== (int)$row['content_size']
			|| !attachment_hash_equals(hash('sha256', $plaintext), $row['content_sha256'])) {
			$ok = false; break;
		}
	}
	foreach ($copies as $copy) {
		if (is_file($copy)) @unlink($copy);
	}
	@rmdir($rehearsal);
	return $ok && !file_exists($rehearsal);
}
