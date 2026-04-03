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
/**
 * CRM Contracts Database Layer
 *
 * CRUD and query functions for customer contracts, renewal alerts,
 * and contract lifecycle management.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * Add a new contract.
 *
 * @param array $data
 * @return int|false
 */
function add_crm_contract($data)
{
    $sql = "INSERT INTO " . TB_PREF . "crm_contracts
            (contract_ref, customer_id, lead_id, title, description,
             contract_value, start_date, end_date, renewal_alert_days,
             status, signed_by, signed_date, created_by) VALUES ("
        . db_escape(@$data['contract_ref']) . ", "
        . (@$data['customer_id'] ? db_escape($data['customer_id']) : "NULL") . ", "
        . (@$data['lead_id'] ? db_escape($data['lead_id']) : "NULL") . ", "
        . db_escape($data['title']) . ", "
        . db_escape(@$data['description']) . ", "
        . db_escape(@$data['contract_value'] ?: 0) . ", "
        . (!empty($data['start_date']) ? db_escape($data['start_date']) : "NULL") . ", "
        . (!empty($data['end_date']) ? db_escape($data['end_date']) : "NULL") . ", "
        . db_escape(@$data['renewal_alert_days'] ?: 30) . ", "
        . db_escape(@$data['status'] ?: CRM_CONTRACT_DRAFT) . ", "
        . db_escape(@$data['signed_by']) . ", "
        . (!empty($data['signed_date']) ? db_escape($data['signed_date']) : "NULL") . ", "
        . (@$data['created_by'] ? db_escape($data['created_by']) : "NULL") . ")";

    db_query($sql, "Could not add CRM contract");
    return db_insert_id();
}

/**
 * Update a contract.
 *
 * @param int   $id
 * @param array $data
 * @return bool
 */
function update_crm_contract($id, $data)
{
    $fields = array(
        'contract_ref', 'customer_id', 'lead_id', 'title', 'description',
        'contract_value', 'start_date', 'end_date', 'renewal_alert_days',
        'status', 'signed_by', 'signed_date',
    );

    $sets = array();
    foreach ($fields as $f) {
        if (array_key_exists($f, $data)) {
            $sets[] = "$f = " . (($data[$f] === null || $data[$f] === '')
                ? "NULL"
                : db_escape($data[$f]));
        }
    }

    if (empty($sets)) {
        return true;
    }

    $sql = "UPDATE " . TB_PREF . "crm_contracts SET "
        . implode(', ', $sets)
        . " WHERE id = " . db_escape($id);

    return db_query($sql, "Could not update CRM contract");
}

/**
 * Delete a contract.
 *
 * @param int $id
 * @return bool
 */
function delete_crm_contract($id)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_contracts WHERE id = " . db_escape($id);
    return db_query($sql, "Could not delete CRM contract");
}

/**
 * Retrieve a single contract by ID.
 *
 * @param int $id
 * @return array|false
 */
function get_crm_contract($id)
{
    $sql = "SELECT c.*, d.name AS customer_name
            FROM " . TB_PREF . "crm_contracts c
            LEFT JOIN " . TB_PREF . "debtors_master d ON c.customer_id = d.debtor_no
            WHERE c.id = " . db_escape($id);
    return db_fetch(db_query($sql, "Could not get CRM contract"));
}

/**
 * List contracts with optional filtering.
 *
 * @param array $filters
 * @param int   $limit
 * @param int   $offset
 * @return resource
 */
function get_crm_contracts($filters = array(), $limit = 0, $offset = 0)
{
    $sql = "SELECT c.*, d.name AS customer_name
            FROM " . TB_PREF . "crm_contracts c
            LEFT JOIN " . TB_PREF . "debtors_master d ON c.customer_id = d.debtor_no
            WHERE 1=1";

    if (!empty($filters['status'])) {
        $sql .= " AND c.status = " . db_escape($filters['status']);
    }
    if (!empty($filters['customer_id'])) {
        $sql .= " AND c.customer_id = " . db_escape($filters['customer_id']);
    }
    if (!empty($filters['search'])) {
        $search = db_escape('%' . $filters['search'] . '%');
        $sql .= " AND (c.title LIKE $search OR c.contract_ref LIKE $search
                  OR d.name LIKE $search)";
    }
    if (!empty($filters['expiring_within_days'])) {
        $days = (int)$filters['expiring_within_days'];
        $sql .= " AND c.status = 'active'
                  AND c.end_date IS NOT NULL
                  AND c.end_date <= DATE_ADD(CURDATE(), INTERVAL $days DAY)
                  AND c.end_date >= CURDATE()";
    }

    $sql .= " ORDER BY c.created_date DESC";

    if ($limit > 0) {
        $sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;
    }

    return db_query($sql, "Could not get CRM contracts");
}

/**
 * Get contracts expiring within N days (for renewal alerts).
 *
 * @param int $days  Default 30.
 * @return resource
 */
function get_expiring_contracts($days = null)
{
    if ($days === null) {
        $days = 30;
    }
    return get_crm_contracts(array('expiring_within_days' => $days));
}

/**
 * Renew a contract by cloning it with new dates and marking old as renewed.
 *
 * @param int    $id
 * @param string $new_start_date
 * @param string $new_end_date
 * @param float  $new_value  Optional new value, defaults to original.
 * @return int|false  New contract ID.
 */
function renew_crm_contract($id, $new_start_date, $new_end_date, $new_value = null)
{
    $old = get_crm_contract($id);
    if (!$old) {
        return false;
    }

    begin_transaction();

    // Mark old contract as renewed
    update_crm_contract($id, array('status' => CRM_CONTRACT_RENEWED));

    // Create new contract
    $new_id = add_crm_contract(array(
        'contract_ref' => crm_next_contract_ref(),
        'customer_id' => $old['customer_id'],
        'lead_id' => $old['lead_id'],
        'title' => $old['title'] . ' (Renewal)',
        'description' => $old['description'],
        'contract_value' => ($new_value !== null) ? $new_value : $old['contract_value'],
        'start_date' => $new_start_date,
        'end_date' => $new_end_date,
        'renewal_alert_days' => $old['renewal_alert_days'],
        'status' => CRM_CONTRACT_DRAFT,
        'created_by' => $old['created_by'],
    ));

    commit_transaction();
    return $new_id;
}
