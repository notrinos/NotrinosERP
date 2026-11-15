<?php
/**
 * DIM-REL-001: release rehearsal contract for the Dimension programme.
 *
 * Pure, read-only decision support (PHP 5.6 syntax, no database, no session, no write, no activation).
 * It states which release gates exist, what each needs as evidence, which Dimension families are
 * supported or explicitly unsupported, what a restricted pilot may touch, how a frozen release manifest
 * is verified, and that production activation can never be granted by this code. A development PASS is
 * not deployment acceptance: the gate report is truthful about every blocking item.
 */

/** Status vocabulary shared by checks, gates and the release decision. */
function dimension_release_status_names()
{
	return array('PASS', 'FAIL', 'BLOCKED', 'NOT_RUN', 'PARTIAL', 'PENDING');
}

/**
 * Release gates G0-G6 and the named evidence checks each one requires.
 *
 * G6 (production activation) has no check: it is a separate human authorization that no development
 * evidence satisfies.
 *
 * @return array gate id => array('title'=>string, 'checks'=>string[])
 */
function dimension_release_gate_catalog()
{
	return array(
		'G0'=>array('title'=>'Baseline facts, schemas, writer ownership, finance semantics and disposable fixture',
			'checks'=>array('version_chain_contiguous', 'schema_surface_frozen', 'predecessor_tasks_complete',
				'disposable_fixture_owned')),
		'G1'=>array('title'=>'P0 calculations, lifecycle and historical reads with unchanged posted history',
			'checks'=>array('p0_regression_sweep', 'history_digests_unchanged')),
		'G2'=>array('title'=>'Resolver, snapshot, approval and new-posting validation with protected inverse regressions',
			'checks'=>array('resolver_snapshot_regression', 'approval_authority_regression')),
		'G3'=>array('title'=>'Each supported family passes its full lifecycle; unsupported families are explicit',
			'checks'=>array('supported_families_complete', 'unsupported_families_explicit')),
		'G4'=>array('title'=>'UI, report, export, GL and P&L reconcile; privacy, tenancy and performance',
			'checks'=>array('reconciliation_regression', 'privacy_tenancy_regression', 'php56_syntax', 'browser_gate')),
		'G5'=>array('title'=>'Populated migration and recovery rehearsal and scoped pilot acceptance',
			'checks'=>array('fresh_install_rehearsal', 'demo_install_rehearsal', 'populated_upgrade_rehearsal',
				'interruption_rerun_rehearsal', 'paired_recovery_rehearsal', 'restricted_pilot_policy',
				'human_pilot_acceptance', 'release_artifact_1_1_0')),
		'G6'=>array('title'=>'Separate authorized production activation', 'checks'=>array()));
}

/**
 * Normalize one reported check status; anything unknown is treated as not proven.
 *
 * @param mixed $status
 * @return string
 */
function dimension_release_normalize_status($status)
{
	$status = is_string($status) ? strtoupper(trim($status)) : '';
	return in_array($status, dimension_release_status_names(), true) ? $status : 'NOT_RUN';
}

/**
 * Evaluate the gates from reported check statuses.
 *
 * A check that is absent counts as NOT_RUN. A gate passes only when every required check passes; a
 * FAIL fails the gate, otherwise the gate takes the least favourable non-PASS status. G6 is never
 * derived from evidence.
 *
 * @param array $evidence check id => status string (extra ids are ignored)
 * @return array gate id => array('status'=>string, 'blocking'=>string[])
 */
function dimension_release_evaluate_gates(array $evidence)
{
	$order = array('FAIL'=>0, 'BLOCKED'=>1, 'NOT_RUN'=>2, 'PENDING'=>3, 'PARTIAL'=>4, 'PASS'=>5);
	$report = array();
	foreach (dimension_release_gate_catalog() as $gate=>$definition) {
		if ($gate === 'G6') {
			$report[$gate] = array('status'=>'REQUIRES_SEPARATE_AUTHORIZATION',
				'blocking'=>array('production_activation_authorization'));
			continue;
		}
		$worst = 'PASS';
		$blocking = array();
		foreach ($definition['checks'] as $check) {
			$status = dimension_release_normalize_status(isset($evidence[$check]) ? $evidence[$check] : null);
			if ($status !== 'PASS') {
				$blocking[] = $check;
				if ($order[$status] < $order[$worst])
					$worst = $status;
			}
		}
		$report[$gate] = array('status'=>$worst, 'blocking'=>$blocking);
	}
	return $report;
}

/**
 * The release decision. It can only say that G0-G5 are proven so authorization may be requested; it
 * never reports production activation.
 *
 * @param array $gate_report Result of dimension_release_evaluate_gates()
 * @return array decision, blocking gates and the (always false) activation flag
 */
function dimension_release_decision(array $gate_report)
{
	$blocking = array();
	foreach ($gate_report as $gate=>$row)
		if ($gate !== 'G6' && (!isset($row['status']) || $row['status'] !== 'PASS'))
			$blocking[$gate] = isset($row['blocking']) ? $row['blocking'] : array();
	return array(
		'decision'=>count($blocking) ? 'NOT_RELEASE_READY' : 'READY_TO_REQUEST_AUTHORIZATION',
		'blocking'=>$blocking,
		'production_activation'=>false);
}

/**
 * Production activation decision. Nothing in a request, token, flag or development evidence can grant it.
 *
 * @param mixed $request Ignored on purpose
 * @return array
 */
function dimension_release_production_activation_decision($request = null)
{
	return array('allowed'=>false, 'gate'=>'G6',
		'reason'=>'Production activation needs a separate human authorization outside this development evidence.');
}

/**
 * Dimension families, their owner tasks and the declared support level.
 *
 * `supported` needs every owner task COMPLETE in the backlog; `limited` is supported with the stated
 * boundary; `unsupported` is explicit and every owner task must still be open or blocked, so a family
 * can never be silently treated as part of the release.
 *
 * @return array family => array('level'=>string, 'tasks'=>string[], 'statement'=>string)
 */
function dimension_release_family_matrix()
{
	return array(
		'gl_journals_and_banking'=>array('level'=>'supported', 'tasks'=>array('DIM-GL-001', 'DIM-GL-002'),
			'statement'=>'Journals, payments and deposits validate and snapshot both axes through the shared writer.'),
		'sales'=>array('level'=>'supported', 'tasks'=>array('DIM-SAL-001', 'DIM-SAL-002', 'DIM-SAL-003', 'DIM-CRM-001'),
			'statement'=>'Orders, deliveries, invoices, credits, receipts and lead conversion keep the resolved classification.'),
		'purchasing'=>array('level'=>'supported', 'tasks'=>array('DIM-PUR-001', 'DIM-PUR-002', 'DIM-PUR-003', 'DIM-PUR-004'),
			'statement'=>'Requisition to payment and credit keep the resolved classification and procurement scope.'),
		'inventory'=>array('level'=>'supported', 'tasks'=>array('DIM-INV-001', 'DIM-INV-002'),
			'statement'=>'Stock movements, adjustments and warehouse events keep their classification lineage.'),
		'manufacturing'=>array('level'=>'supported', 'tasks'=>array('DIM-MFG-001'),
			'statement'=>'Work-order costs carry the order classification into GL.'),
		'fixed_assets'=>array('level'=>'supported', 'tasks'=>array('DIM-FA-001'),
			'statement'=>'Asset responsibility and depreciation keep the asset classification.'),
		'tax_discount_rounding'=>array('level'=>'supported', 'tasks'=>array('DIM-TAX-001'),
			'statement'=>'Tagged tax, discount and rounding amounts reconcile to the tagged document.'),
		'integration_and_export'=>array('level'=>'supported', 'tasks'=>array('DIM-INT-001', 'DIM-REP-006'),
			'statement'=>'CSV import, export, hooks and saved comparisons use the same classification contract.'),
		'reporting_and_budget_comparison'=>array('level'=>'supported', 'tasks'=>array('DIM-REP-003', 'DIM-REP-004',
			'DIM-REP-005', 'DIM-BUD-001', 'DIM-ARC-001'),
			'statement'=>'Tracking, inquiry, customer economics, monthly budget comparison and archive retrieval reconcile to GL.'),
		'business_modes'=>array('level'=>'supported', 'tasks'=>array('DIM-CFG-001', 'DIM-UX-001'),
			'statement'=>'Declared category purposes drive wording and overdue behavior without changing stored meaning.'),
		'allocation_journals'=>array('level'=>'supported', 'tasks'=>array('DIM-ALC-001'),
			'statement'=>'Posted rows are allocated or reclassified by fixed percentages or amounts through the journal approval path, with an exact inverse.'),
		'driver_allocation'=>array('level'=>'supported', 'tasks'=>array('DIM-ALC-002'),
			'statement'=>'Approved, effective-dated measured drivers (including minimized payroll aggregates) allocate posted rows through the same journal approval path; a stale driver cannot post and the fully allocated label needs a complete driver.'),
		'payroll_and_hrm'=>array('level'=>'limited', 'tasks'=>array('DIM-HRM-001', 'DIM-PAY-002'),
			'statement'=>'Native payroll classification is read and explained; the effective-dated Finance mapping command, '
				.'recalculation and any Dimension axis on HRM employees are not part of this release.'),
		'trade_finance_extension'=>array('level'=>'unsupported', 'tasks'=>array('DIM-EXT-001'),
			'statement'=>'The inactive Trade Finance extension stays blocked from activation; Dimension parity is characterized only.'),
		'budget_baselines'=>array('level'=>'supported', 'tasks'=>array('DIM-BUD-002'),
			'statement'=>'An exact approved baseline and independently approved revisions with commentary sit beside the editable budget; the active comparison is an explicit, audited choice and a tampered version is never shown.'),
		'commitments_and_hierarchy'=>array('level'=>'supported', 'tasks'=>array('DIM-ADV-001', 'DIM-ADV-002'),
			'statement'=>'Purchase commitments, forecasts and effective hierarchy rollups are advisory evidence beside the ledger: commitments and reservations never post, reservations stay off until separately activated, and a hierarchy is an optional approved structure whose as-posted view is never restated.'),
		'additional_categories'=>array('level'=>'unsupported', 'tasks'=>array('DIM-AXS-001'),
			'statement'=>'Only the two native categories exist; an independent third category is not evaluated.'));
}

/**
 * Families a restricted pilot may admit.
 *
 * @return string[]
 */
function dimension_release_pilot_families()
{
	$families = array();
	foreach (dimension_release_family_matrix() as $family=>$row)
		if ($row['level'] === 'supported')
			$families[] = $family;
	return $families;
}

/**
 * Authority a pilot user must hold for a restricted pilot; read access alone never reaches a writer.
 *
 * @return string[]
 */
function dimension_release_pilot_required_areas()
{
	return array('SA_DIMTRANSVIEW');
}

/**
 * Decide whether one request is inside a restricted pilot.
 *
 * The profile names the single company prefix, the explicitly invited users and the families. Anything
 * not named is denied and the production flag in a profile or request is never honored.
 *
 * @param array $profile company, users, families
 * @param array $request company, user, family, areas (granted security areas)
 * @return array allowed flag and the reasons for a denial
 */
function dimension_release_pilot_admission($profile, $request)
{
	$reasons = array();
	if (!is_array($profile) || !is_array($request))
		return array('allowed'=>false, 'reasons'=>array('profile_or_request_malformed'));
	$company = isset($profile['company']) ? $profile['company'] : null;
	$users = isset($profile['users']) && is_array($profile['users']) ? $profile['users'] : array();
	$families = isset($profile['families']) && is_array($profile['families']) ? $profile['families'] : array();
	if (!is_string($company) || !preg_match('/^[0-9]+$/D', $company))
		$reasons[] = 'pilot_company_not_named';
	if (!count($users))
		$reasons[] = 'pilot_users_not_named';
	if (!count($families))
		$reasons[] = 'pilot_families_not_named';
	foreach ($families as $family)
		if (!in_array($family, dimension_release_pilot_families(), true))
			$reasons[] = 'pilot_family_not_supported:'.(is_string($family) ? $family : 'invalid');
	if (!isset($request['company']) || (string)$request['company'] !== (string)$company)
		$reasons[] = 'company_outside_pilot';
	if (!isset($request['user']) || !in_array($request['user'], $users, true))
		$reasons[] = 'user_not_invited';
	if (!isset($request['family']) || !in_array($request['family'], $families, true))
		$reasons[] = 'family_outside_pilot';
	$areas = isset($request['areas']) && is_array($request['areas']) ? $request['areas'] : array();
	foreach (dimension_release_pilot_required_areas() as $area)
		if (!in_array($area, $areas, true))
			$reasons[] = 'authority_missing:'.$area;
	if (!empty($profile['production']) || !empty($request['production']))
		$reasons[] = 'production_flag_refused';
	return array('allowed'=>!count($reasons), 'reasons'=>array_values(array_unique($reasons)));
}

/**
 * Verify that the registered migration edges form one unbroken sequential chain to the head.
 *
 * @param array $registry Result of hrm_migration_head_registry()
 * @return string[] Problems; empty when the chain is contiguous and the head is the newest edge
 */
function dimension_release_version_chain_problems(array $registry)
{
	$problems = array();
	if (!isset($registry['edges']) || !is_array($registry['edges']) || !count($registry['edges']))
		return array('no_edges_registered');
	$by_previous = array();
	$versions = array();
	foreach ($registry['edges'] as $name=>$edge) {
		if (!isset($edge['previous']) || !isset($edge['version'])) {
			$problems[] = 'edge_incomplete:'.$name;
			continue;
		}
		if (isset($by_previous[$edge['previous']]))
			$problems[] = 'edge_fork_at:'.$edge['previous'];
		$by_previous[$edge['previous']] = $edge['version'];
		$versions[$edge['version']] = $edge['previous'];
	}
	$head = isset($registry['active_database_version']) ? $registry['active_database_version'] : '';
	if (!isset($versions[$head]))
		$problems[] = 'head_is_not_an_edge_version:'.$head;
	if (in_array($head, array_keys($by_previous), true))
		$problems[] = 'head_has_a_successor:'.$head;
	$roots = array();
	foreach ($by_previous as $previous=>$version)
		if (!isset($versions[$previous]))
			$roots[] = $previous;
	if (count($roots) !== 1)
		$problems[] = 'chain_has_'.count($roots).'_roots';
	elseif (!count($problems)) {
		$cursor = $roots[0];
		$steps = 0;
		while (isset($by_previous[$cursor]) && $steps <= count($registry['edges'])) {
			$cursor = $by_previous[$cursor];
			$steps++;
		}
		if ($steps !== count($registry['edges']) || $cursor !== $head)
			$problems[] = 'chain_does_not_reach_head';
	}
	return $problems;
}

/**
 * SHA-256 of a file in its LF form, so a CRLF checkout and an LF checkout agree.
 *
 * @param string $path
 * @return string|false
 */
function dimension_release_file_digest($path)
{
	$content = is_file($path) ? file_get_contents($path) : false;
	return $content === false ? false : hash('sha256', str_replace("\r\n", "\n", $content));
}

/**
 * Compare a frozen manifest with the tree.
 *
 * `schema_surface` files are the release's migration surface and must match exactly. `runtime_source`
 * files are frozen evidence of the candidate: a later accepted slice may change them, so a changed one
 * is reported as drift (informational) while a deleted one is a problem.
 *
 * @param string $root Repository root
 * @param array $manifest Decoded release manifest
 * @return array 'problems' (blocking) and 'drift' (informational) path lists
 */
function dimension_release_manifest_compare($root, array $manifest)
{
	$result = array('problems'=>array(), 'drift'=>array());
	foreach (array('schema_surface'=>true, 'runtime_source'=>false) as $group=>$strict) {
		if (!isset($manifest['files'][$group]) || !is_array($manifest['files'][$group])
			|| !count($manifest['files'][$group])) {
			$result['problems'][] = 'group_missing:'.$group;
			continue;
		}
		foreach ($manifest['files'][$group] as $path=>$digest) {
			if (!is_string($path) || strpos($path, '..') !== false || $path[0] === '/' || !is_string($digest)) {
				$result['problems'][] = 'invalid_entry:'.$group;
				continue;
			}
			$actual = dimension_release_file_digest($root.'/'.$path);
			if ($actual === false)
				$result['problems'][] = 'missing:'.$path;
			elseif ($actual !== $digest) {
				if ($strict)
					$result['problems'][] = 'changed:'.$path;
				else
					$result['drift'][] = $path;
			}
		}
	}
	return $result;
}

/**
 * Parse every task's declared completion from the implementation backlog text.
 *
 * @param string $backlog
 * @return array task id => array('status'=>'COMPLETE'|'OPEN'|'BLOCKED'|'OTHER', 'predecessors'=>string[])
 */
function dimension_release_backlog_tasks($backlog)
{
	$tasks = array();
	$sections = preg_split('/^## (DIM-[A-Z]+-[0-9]+[A-Za-z0-9]*) — /m', str_replace("\r\n", "\n", (string)$backlog), -1,
		PREG_SPLIT_DELIM_CAPTURE);
	for ($index = 1; $index + 1 < count($sections); $index += 2) {
		$task = $sections[$index];
		$body = $sections[$index + 1];
		$status = 'OTHER';
		if (preg_match('/^- \*\*Completion status:\*\* (\S+)/m', $body, $match)) {
			$word = strtoupper(trim($match[1], '*:/'));
			if ($word === 'COMPLETE')
				$status = 'COMPLETE';
			elseif ($word === 'OPEN')
				$status = 'OPEN';
			elseif ($word === 'BLOCKED')
				$status = 'BLOCKED';
		}
		$predecessors = array();
		if (preg_match('/^- \*\*Explicit predecessors:\*\* (.+)$/m', $body, $match)
			&& preg_match_all('/DIM-[A-Z]+-[0-9]+/', $match[1], $found))
			$predecessors = $found[0];
		$tasks[$task] = array('status'=>$status, 'predecessors'=>$predecessors);
	}
	return $tasks;
}

/**
 * Check the family matrix against the backlog's own statements.
 *
 * @param array $tasks Result of dimension_release_backlog_tasks()
 * @return string[] Problems; empty when every supported owner is complete and every unsupported one is not
 */
function dimension_release_family_problems(array $tasks)
{
	$problems = array();
	foreach (dimension_release_family_matrix() as $family=>$row) {
		if (!in_array($row['level'], array('supported', 'limited', 'unsupported'), true) || $row['statement'] === '')
			$problems[] = 'family_undeclared:'.$family;
		foreach ($row['tasks'] as $task) {
			if (!isset($tasks[$task])) {
				$problems[] = 'task_not_in_backlog:'.$family.':'.$task;
				continue;
			}
			$complete = $tasks[$task]['status'] === 'COMPLETE';
			if ($row['level'] === 'unsupported' && $complete && $family !== 'trade_finance_extension')
				$problems[] = 'unsupported_family_has_complete_task:'.$family.':'.$task;
			if ($row['level'] !== 'unsupported' && !$complete)
				$problems[] = 'supported_family_has_open_task:'.$family.':'.$task;
		}
	}
	return $problems;
}
