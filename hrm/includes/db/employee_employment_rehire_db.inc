<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-002 reviewed Employment-backed legacy rehire execution.
 *
 * The command consumes an already reviewed/current rehire proposal. It links
 * two existing legacy Employment episodes only after chronology is proven,
 * reuses the existing canonical Person-link snapshot tables for one bounded
 * successor Worker rebinding, and records immutable rehire relationship
 * evidence. It never creates/deletes a legacy employee or Worker and never
 * rewrites payroll/accounting history.
 */

/** @return bool */
function hrm_employment_rehire_table_ready()
{
    if (!function_exists('db_query') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE ".db_escape(TB_PREF.'hrm_employment_rehires'));
    return $result && db_num_rows($result) === 1;
}

/** @return bool */
function hrm_employment_rehire_user_can_execute()
{
    return function_exists('hrm_person_canonical_link_user_can_execute')
        && hrm_person_canonical_link_user_can_execute();
}

/**
 * Determine the exact predecessor/successor ordering for two Employment rows.
 * Legal-date intervals are inclusive, so successor start must be strictly
 * later than predecessor end. Unknown/reversed intervals fail closed.
 *
 * @param array $left
 * @param array $right
 * @return array|false
 */
function hrm_employment_rehire_order_pair($left, $right)
{
    if (!is_array($left) || !is_array($right))
        return false;
    foreach (array($left, $right) as $row) {
        if (empty($row['employment_id']) || empty($row['worker_id']) || empty($row['person_id'])
            || empty($row['legacy_employee_number']) || empty($row['start_date'])
            || (string)$row['interval_quality'] === 'legacy_invalid_order')
            return false;
    }
    $left_end = isset($left['end_date']) && $left['end_date'] !== '' ? (string)$left['end_date'] : null;
    $right_end = isset($right['end_date']) && $right['end_date'] !== '' ? (string)$right['end_date'] : null;
    $left_start = (string)$left['start_date'];
    $right_start = (string)$right['start_date'];

    if ($left_end !== null && (string)$left['employment_status'] === 'terminated'
        && strcmp($left_end, $right_start) < 0) {
        return array('predecessor'=>$left, 'successor'=>$right);
    }
    if ($right_end !== null && (string)$right['employment_status'] === 'terminated'
        && strcmp($right_end, $left_start) < 0) {
        return array('predecessor'=>$right, 'successor'=>$left);
    }
    return false;
}

/**
 * Lock the one current mapped Employment under each reviewed Person.
 * Multiple current Workers/Employments per Person are intentionally deferred
 * until the concurrent-employment policy is implemented.
 *
 * @param int $left_person_id
 * @param int $right_person_id
 * @return array|false
 */
function hrm_employment_rehire_lock_pair($left_person_id, $right_person_id)
{
    $left_person_id = (int)$left_person_id;
    $right_person_id = (int)$right_person_id;
    if ($left_person_id <= 0 || $right_person_id <= 0 || $left_person_id >= $right_person_id)
        return false;
    $sql = "SELECT w.person_id,w.worker_id,m.mapping_id,m.employee_number AS legacy_employee_number,h.employment_id,"
        ."h.employment_sequence,h.legacy_current,h.start_date,h.end_date,h.employment_status,"
        ."h.interval_quality FROM ".TB_PREF."hrm_workers w "
        ."INNER JOIN ".TB_PREF."hrm_employee_worker_map m ON m.worker_id=w.worker_id AND m.retired_at IS NULL "
        ."INNER JOIN ".TB_PREF."hrm_employments h ON h.worker_id=w.worker_id "
        ."AND h.legacy_employee_number=m.employee_number AND h.legacy_current=1 "
        ."WHERE w.person_id IN (".$left_person_id.",".$right_person_id.") "
        ."ORDER BY w.person_id,w.worker_id,h.employment_id FOR UPDATE";
    $result = db_query($sql, 'could not lock reviewed Employment rehire pair');
    if (!$result)
        return false;
    $by_person = array($left_person_id=>array(), $right_person_id=>array());
    while ($row = db_fetch_assoc($result)) {
        $person_id = (int)$row['person_id'];
        if (!isset($by_person[$person_id]) || (int)$row['employment_sequence'] !== 1
            || (int)$row['legacy_current'] !== 1)
            return false;
        $by_person[$person_id][] = $row;
    }
    if (count($by_person[$left_person_id]) !== 1 || count($by_person[$right_person_id]) !== 1)
        return false;
    return hrm_employment_rehire_order_pair(
        $by_person[$left_person_id][0], $by_person[$right_person_id][0]
    );
}

/** @return array|false|null */
function get_hrm_employment_rehire_for_proposal($proposal_id, $for_update = false)
{
    $proposal_id = (int)$proposal_id;
    if ($proposal_id <= 0 || !hrm_employment_rehire_table_ready())
        return false;
    $sql = "SELECT rehire_id,proposal_id,canonical_link_id,predecessor_employment_id,"
        ."successor_employment_id,canonical_person_id,alias_person_id,predecessor_end_date,"
        ."successor_start_date,executed_at,executed_by,rehire_key FROM "
        .TB_PREF."hrm_employment_rehires WHERE proposal_id=".$proposal_id;
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read Employment rehire evidence');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Revalidate persisted Employment chronology on an exact retry. The canonical
 * link snapshot already protects the successor Worker/mapping rebinding; this
 * companion check protects the Employment IDs and dates/status that justified
 * the rehire relationship itself.
 *
 * @param array $rehire
 * @return bool
 */
function hrm_employment_rehire_existing_evidence_is_intact($rehire)
{
    if (!is_array($rehire) || empty($rehire['rehire_id']))
        return false;
    $sql = "SELECT ep.employment_id AS predecessor_employment_id,ep.end_date AS predecessor_end_date,"
        ."ep.employment_status AS predecessor_status,es.employment_id AS successor_employment_id,"
        ."es.start_date AS successor_start_date,es.worker_id AS successor_worker_id,s.worker_id AS snapshot_worker_id "
        ."FROM ".TB_PREF."hrm_employment_rehires r "
        ."INNER JOIN ".TB_PREF."hrm_employments ep ON ep.employment_id=r.predecessor_employment_id "
        ."INNER JOIN ".TB_PREF."hrm_employments es ON es.employment_id=r.successor_employment_id "
        ."INNER JOIN ".TB_PREF."hrm_person_canonical_link_workers s ON s.link_id=r.canonical_link_id "
        ."WHERE r.rehire_id=".(int)$rehire['rehire_id']." FOR UPDATE";
    $result = db_query($sql, 'could not revalidate Employment rehire evidence');
    if (!$result || db_num_rows($result) !== 1)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row)
        && (int)$row['predecessor_employment_id'] === (int)$rehire['predecessor_employment_id']
        && (int)$row['successor_employment_id'] === (int)$rehire['successor_employment_id']
        && (string)$row['predecessor_end_date'] === (string)$rehire['predecessor_end_date']
        && (string)$row['successor_start_date'] === (string)$rehire['successor_start_date']
        && (string)$row['predecessor_status'] === 'terminated'
        && strcmp((string)$row['predecessor_end_date'], (string)$row['successor_start_date']) < 0
        && (int)$row['successor_worker_id'] === (int)$row['snapshot_worker_id'];
}

/**
 * Protect the chronology used by an executed rehire from later flat-row edits.
 * Successor end/status may continue to evolve, but the predecessor end and
 * successor start that established the rehire relationship are immutable.
 *
 * @param int $employment_id
 * @param array $projection
 * @return bool
 */
function hrm_employment_rehire_projection_change_allowed($employment_id, $projection)
{
    $employment_id = (int)$employment_id;
    if ($employment_id <= 0 || !is_array($projection) || !hrm_employment_rehire_table_ready())
        return true;
    $sql = "SELECT predecessor_employment_id,successor_employment_id,predecessor_end_date,"
        ."successor_start_date FROM ".TB_PREF."hrm_employment_rehires WHERE predecessor_employment_id="
        .$employment_id." OR successor_employment_id=".$employment_id." FOR UPDATE";
    $result = db_query($sql, 'could not verify immutable Employment rehire chronology');
    if (!$result)
        return false;
    while ($row = db_fetch_assoc($result)) {
        if ((int)$row['predecessor_employment_id'] === $employment_id) {
            if ((string)$projection['end_date'] !== (string)$row['predecessor_end_date']
                || (string)$projection['employment_status'] !== 'terminated')
                return false;
        }
        if ((int)$row['successor_employment_id'] === $employment_id
            && (string)$projection['start_date'] !== (string)$row['successor_start_date'])
            return false;
    }
    return true;
}

/** @return array|false */
function append_hrm_employment_rehire_domain_audit($rehire_id, $proposal, $link_id,
    $predecessor, $successor, $canonical_person_id, $alias_person_id)
{
    if (!function_exists('append_domain_audit_event') || (int)$rehire_id <= 0
        || !is_array($proposal) || (int)$link_id <= 0 || !is_array($predecessor)
        || !is_array($successor) || (int)$canonical_person_id <= 0 || (int)$alias_person_id <= 0)
        return false;
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => 0,
        'entity_type' => 'hrm_employment_rehire',
        'entity_key' => array(
            'rehire_id'=>(int)$rehire_id,
            'proposal_id'=>(int)$proposal['proposal_id'],
            'review_id'=>(int)$proposal['review_id'],
            'canonical_link_id'=>(int)$link_id,
            'predecessor_employment_id'=>(int)$predecessor['employment_id'],
            'successor_employment_id'=>(int)$successor['employment_id'],
            'canonical_person_id'=>(int)$canonical_person_id,
            'alias_person_id'=>(int)$alias_person_id,
        ),
        'action' => 'hrm.employment.rehire.executed',
        'reason_code' => 'reviewed_potential_rehire_maker_checker',
        'before' => null,
        'after' => array(
            'rehire_executed'=>true,
            'physical_person_merge_executed'=>false,
            'person_rows_deleted'=>0,
            'worker_rebinding_count'=>1,
            'legacy_mapping_rows_changed'=>0,
            'employment_rows_rewritten'=>0,
        ),
        'restricted_diff' => array('restricted_values_stored'=>false),
    ));
}

/**
 * Execute one reviewed cross-legacy-employee rehire relationship.
 *
 * The earlier Employment's Person becomes canonical deterministically. The
 * successor Worker ID and legacy mapping remain unchanged and are rebound to
 * that Person through the pre-existing canonical-link evidence tables. Exact
 * retries are accepted only while every persisted snapshot remains intact.
 *
 * @param int $proposal_id
 * @return int|false Rehire evidence ID.
 */
function execute_hrm_employment_rehire($proposal_id)
{
    $proposal_id = (int)$proposal_id;
    if ($proposal_id <= 0 || !hrm_employment_rehire_user_can_execute()
        || !hrm_employment_rehire_table_ready()
        || !function_exists('get_hrm_person_link_proposal_by_id')
        || !function_exists('hrm_person_canonical_link_tables_ready')
        || !hrm_person_canonical_link_tables_ready())
        return false;
    $actor = function_exists('hrm_person_worker_actor_id') ? (int)hrm_person_worker_actor_id() : 0;
    if ($actor <= 0)
        return false;

    begin_transaction();
    $proposal = get_hrm_person_link_proposal_by_id($proposal_id, true);
    if (!is_array($proposal) || (string)$proposal['proposal_type'] !== 'rehire_candidate'
        || (int)$proposal['proposed_by'] <= 0 || (int)$proposal['proposed_by'] === $actor) {
        cancel_transaction();
        return false;
    }
    $left = (int)$proposal['person_id_low'];
    $right = (int)$proposal['person_id_high'];
    if ($left <= 0 || $right <= 0 || $left >= $right
        || !hrm_person_duplicate_review_lock_pair($left, $right)) {
        cancel_transaction();
        return false;
    }

    $existing = get_hrm_employment_rehire_for_proposal($proposal_id, true);
    if ($existing === false) {
        cancel_transaction();
        return false;
    }
    if (is_array($existing)) {
        $link = get_hrm_person_canonical_link_for_proposal($proposal_id, true);
        $expected_key = hash('sha256', implode("\0", array(
            'employment-rehire-v1', $proposal_id,
            (int)$existing['predecessor_employment_id'], (int)$existing['successor_employment_id'],
            (int)$existing['canonical_person_id'], (int)$existing['alias_person_id'],
        )));
        $ok = is_array($link)
            && (int)$link['link_id'] === (int)$existing['canonical_link_id']
            && (int)$link['canonical_person_id'] === (int)$existing['canonical_person_id']
            && (int)$link['alias_person_id'] === (int)$existing['alias_person_id']
            && (int)$link['worker_count'] === 1
            && domain_audit_hash_equals((string)$existing['rehire_key'], $expected_key)
            && hrm_person_canonical_link_snapshot_is_intact($link, true)
            && hrm_employment_rehire_existing_evidence_is_intact($existing);
        if (!$ok) {
            cancel_transaction();
            return false;
        }
        commit_transaction();
        return (int)$existing['rehire_id'];
    }

    $review = get_hrm_person_duplicate_review_by_id((int)$proposal['review_id'], true);
    if (!is_array($review) || (int)$review['person_id_low'] !== $left
        || (int)$review['person_id_high'] !== $right
        || (string)$review['disposition'] !== 'possible_same_person'
        || (string)$review['reason_code'] !== 'potential_rehire') {
        cancel_transaction();
        return false;
    }
    $current = hrm_person_duplicate_review_collect_pairs($left, $right, 1);
    if (!is_array($current) || count($current) !== 1
        || !domain_audit_hash_equals((string)$current[0]['candidate_fingerprint'],
            (string)$review['candidate_fingerprint'])) {
        cancel_transaction();
        return false;
    }
    $latest = get_hrm_person_duplicate_latest_review(
        $left, $right, (string)$review['candidate_fingerprint'], true
    );
    $expected_proposal_key = hash('sha256', implode("\0", array(
        'person-link-proposal-v1', (int)$proposal['review_id'], $left, $right, 'rehire_candidate',
    )));
    if (!is_array($latest) || (int)$latest['review_id'] !== (int)$review['review_id']
        || (string)$latest['disposition'] !== 'possible_same_person'
        || (string)$latest['reason_code'] !== 'potential_rehire'
        || !domain_audit_hash_equals((string)$proposal['proposal_key'], $expected_proposal_key)) {
        cancel_transaction();
        return false;
    }

    $ordered = hrm_employment_rehire_lock_pair($left, $right);
    if (!$ordered) {
        cancel_transaction();
        return false;
    }
    $predecessor = $ordered['predecessor'];
    $successor = $ordered['successor'];
    $canonical_person_id = (int)$predecessor['person_id'];
    $alias_person_id = (int)$successor['person_id'];
    if (!hrm_person_canonical_link_pair_is_available($canonical_person_id, $alias_person_id)) {
        cancel_transaction();
        return false;
    }
    $workers = hrm_person_canonical_link_lock_alias_workers($alias_person_id);
    if (!is_array($workers) || count($workers) !== 1
        || (int)$workers[0]['worker_id'] !== (int)$successor['worker_id']
        || (int)$workers[0]['mapping_id'] !== (int)$successor['mapping_id']) {
        cancel_transaction();
        return false;
    }

    $link_key = hash('sha256', implode("\0", array(
        'person-canonical-link-v1', $proposal_id, $canonical_person_id, $alias_person_id,
    )));
    $sql = "INSERT INTO ".TB_PREF."hrm_person_canonical_links "
        ."(proposal_id,canonical_person_id,alias_person_id,worker_count,linked_at,linked_by,link_key) VALUES ("
        .$proposal_id.','.$canonical_person_id.','.$alias_person_id.",1,UTC_TIMESTAMP(),".$actor.','
        .db_escape($link_key).')';
    if (!db_query($sql, 'could not append rehire canonical Person link')) {
        cancel_transaction();
        return false;
    }
    $link_id = (int)db_insert_id();
    if ($link_id <= 0) {
        cancel_transaction();
        return false;
    }
    $worker = $workers[0];
    $sql = "INSERT INTO ".TB_PREF."hrm_person_canonical_link_workers "
        ."(link_id,worker_id,mapping_id,previous_person_id,canonical_person_id) VALUES ("
        .$link_id.','.(int)$worker['worker_id'].','.(int)$worker['mapping_id'].','.$alias_person_id.','.$canonical_person_id.')';
    if (!db_query($sql, 'could not snapshot rehire Worker rebinding')
        || !db_query("UPDATE ".TB_PREF."hrm_workers SET person_id=".$canonical_person_id
            ." WHERE worker_id=".(int)$worker['worker_id']." AND person_id=".$alias_person_id,
            'could not rebind rehire Worker to canonical Person')
        || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1) {
        cancel_transaction();
        return false;
    }

    $rehire_key = hash('sha256', implode("\0", array(
        'employment-rehire-v1', $proposal_id, (int)$predecessor['employment_id'],
        (int)$successor['employment_id'], $canonical_person_id, $alias_person_id,
    )));
    $sql = "INSERT INTO ".TB_PREF."hrm_employment_rehires "
        ."(proposal_id,canonical_link_id,predecessor_employment_id,successor_employment_id,"
        ."canonical_person_id,alias_person_id,predecessor_end_date,successor_start_date,executed_at,executed_by,rehire_key) VALUES ("
        .$proposal_id.','.$link_id.','.(int)$predecessor['employment_id'].','.(int)$successor['employment_id'].','
        .$canonical_person_id.','.$alias_person_id.','.db_escape((string)$predecessor['end_date']).','
        .db_escape((string)$successor['start_date']).",UTC_TIMESTAMP(),".$actor.','.db_escape($rehire_key).')';
    if (!db_query($sql, 'could not append Employment rehire relationship')) {
        cancel_transaction();
        return false;
    }
    $rehire_id = (int)db_insert_id();
    $link = get_hrm_person_canonical_link_for_proposal($proposal_id, true);
    if ($rehire_id <= 0 || !is_array($link)
        || !hrm_person_canonical_link_snapshot_is_intact($link, true)
        || !append_hrm_employment_rehire_domain_audit(
            $rehire_id, $proposal, $link_id, $predecessor, $successor,
            $canonical_person_id, $alias_person_id
        )) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $rehire_id;
}
