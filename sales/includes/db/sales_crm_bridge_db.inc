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
 * Sales–CRM Bridge Database Functions
 *
 * Provides integration between the Sales module (quotations/orders) and the
 * CRM module (opportunities/pipeline). Pipeline and stage management is
 * delegated entirely to the CRM module. This bridge handles:
 *  - Linking quotes to CRM opportunities (opportunity_id FK)
 *  - Syncing quote Won/Lost status back to CRM
 *  - Querying CRM pipeline data for sales reports
 *
 * All CRM functions require the 'use_crm' sys_pref to be enabled.
 * When use_crm is disabled all functions in this file are no-ops or return null.
 *
 * @package NotrinosERP
 * @subpackage Sales
 */

// ============================================================================
// QUOTE ↔ OPPORTUNITY LINKING
// ============================================================================

/**
 * Link a sales quotation to a CRM opportunity.
 *
 * Sets opportunity_id on the sales_orders row. Also updates the CRM
 * opportunity's expected_revenue with the quotation total if the opportunity
 * exists and has no revenue yet.
 *
 * @param int $order_no      Sales order / quotation number.
 * @param int $opportunity_id CRM opportunity (lead) ID.
 * @return bool True on success.
 */
function link_quote_to_opportunity($order_no, $opportunity_id)
{
	global $SysPrefs;

	if (empty($SysPrefs->prefs['use_crm']))
		return false;

	$sql = "UPDATE " . TB_PREF . "sales_orders
		SET opportunity_id = " . db_escape((int)$opportunity_id) . "
		WHERE order_no = " . db_escape((int)$order_no) . "
		AND trans_type = " . ST_SALESQUOTE;

	return db_query($sql, "Could not link quote to opportunity");
}

/**
 * Get all quotations linked to a CRM opportunity.
 *
 * @param int $opportunity_id CRM opportunity ID.
 * @return resource|false Database result resource or false if CRM disabled.
 */
function get_quotes_for_opportunity($opportunity_id)
{
	global $SysPrefs;

	if (empty($SysPrefs->prefs['use_crm']))
		return false;

	$sql = "SELECT so.order_no, so.reference, so.ord_date, so.delivery_date,
		so.total, so.debtor_no, c.name AS customer_name
		FROM " . TB_PREF . "sales_orders so
		LEFT JOIN " . TB_PREF . "debtors_master c ON c.debtor_no = so.debtor_no
		WHERE so.opportunity_id = " . db_escape((int)$opportunity_id) . "
		AND so.trans_type = " . ST_SALESQUOTE . "
		ORDER BY so.ord_date DESC";

	return db_query($sql, "Could not get quotes for opportunity");
}

// ============================================================================
// WON / LOST SYNC
// ============================================================================

/**
 * Sync a Won status back to the linked CRM opportunity when a quote converts
 * to a sales order (type 32 → 30).
 *
 * Calls mark_opportunity_won() from the CRM module, then fires the
 * sales_quote_won hook.
 *
 * @param int $order_no      The quotation order number.
 * @param int $customer_id   The customer debtor_no (for CRM update).
 * @return bool True if synced, false if CRM disabled or no linked opportunity.
 */
function sync_quote_won_to_crm($order_no, $customer_id = 0)
{
	global $SysPrefs, $path_to_root;

	if (empty($SysPrefs->prefs['use_crm']))
		return false;

	// Read opportunity_id from the old quote row
	$sql = "SELECT opportunity_id FROM " . TB_PREF . "sales_orders
		WHERE order_no = " . db_escape((int)$order_no) . "
		AND trans_type = " . ST_SALESQUOTE;
	$result = db_query($sql, "Could not read quote opportunity_id");
	$row = db_fetch($result);
	if (!$row || !$row['opportunity_id'])
		return false;

	$opportunity_id = (int)$row['opportunity_id'];

	// Include CRM db functions lazily
	if (!function_exists('mark_opportunity_won')) {
		include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
	}

	begin_transaction();
	mark_opportunity_won($opportunity_id, $customer_id > 0 ? $customer_id : null);
	commit_transaction();

	hook_invoke_all('sales_quote_won', $order_no, $opportunity_id);
	return true;
}

/**
 * Mark the linked CRM opportunity as Lost when a quote is discarded.
 *
 * @param int $order_no      The quotation order number.
 * @param int $lost_reason_id CRM lost reason ID (0 = no reason specified).
 * @param string $notes      Optional notes about the loss.
 * @return bool True if synced, false if CRM disabled or no linked opportunity.
 */
function sync_quote_lost_to_crm($order_no, $lost_reason_id = 0, $notes = '')
{
	global $SysPrefs, $path_to_root;

	if (empty($SysPrefs->prefs['use_crm']))
		return false;

	$sql = "SELECT opportunity_id FROM " . TB_PREF . "sales_orders
		WHERE order_no = " . db_escape((int)$order_no) . "
		AND trans_type = " . ST_SALESQUOTE;
	$result = db_query($sql, "Could not read quote opportunity_id");
	$row = db_fetch($result);
	if (!$row || !$row['opportunity_id'])
		return false;

	$opportunity_id = (int)$row['opportunity_id'];

	if (!function_exists('mark_opportunity_lost')) {
		include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');
	}

	begin_transaction();
	mark_opportunity_lost($opportunity_id, $lost_reason_id > 0 ? $lost_reason_id : null, $notes);
	commit_transaction();

	hook_invoke_all('sales_quote_lost', $order_no, $opportunity_id);
	return true;
}

// ============================================================================
// PIPELINE SUMMARY (delegates to CRM module)
// ============================================================================

/**
 * Get a pipeline summary by querying CRM opportunity data.
 *
 * Returns totals grouped by opportunity stage. Filters by date range and
 * optionally by assigned salesman. Delegates entirely to the CRM module
 * tables — no duplication of pipeline state in the sales module.
 *
 * @param string $date_from Date from (dd/mm/yyyy).
 * @param string $date_to   Date to (dd/mm/yyyy).
 * @param int    $salesman  Salesman ID (0 = all).
 * @return resource|false   Database result or false if CRM disabled.
 */
function get_pipeline_summary_from_crm($date_from, $date_to, $salesman = 0)
{
	global $SysPrefs;

	if (empty($SysPrefs->prefs['use_crm']))
		return false;

	$sql = "SELECT
		IFNULL(cs.name, cl.lead_status) AS stage_name,
		COUNT(cl.id) AS opportunity_count,
		SUM(cl.expected_revenue) AS total_revenue,
		SUM(cl.expected_revenue * cl.probability / 100) AS weighted_revenue,
		AVG(cl.probability) AS avg_probability
		FROM " . TB_PREF . "crm_leads cl
		LEFT JOIN " . TB_PREF . "crm_sales_stages cs ON cs.id = cl.stage_id
		WHERE cl.is_opportunity = 1
		AND cl.inactive = 0";

	if ($date_from)
		$sql .= " AND cl.expected_close_date >= '" . date2sql($date_from) . "'";
	if ($date_to)
		$sql .= " AND cl.expected_close_date <= '" . date2sql($date_to) . "'";
	if ($salesman > 0)
		$sql .= " AND cl.assigned_to = " . db_escape((int)$salesman);

	$sql .= " GROUP BY cl.stage_id, cs.name, cl.lead_status
		ORDER BY IFNULL(cs.sequence, 99), cs.name";

	return db_query($sql, "Could not get CRM pipeline summary");
}

// ============================================================================
// HELPER: List CRM opportunities for a customer (used in dropdown selector)
// ============================================================================

/**
 * Get open CRM opportunities for a given customer for use in a dropdown.
 *
 * Returns only active (non-inactive, non-won, non-lost) opportunities.
 *
 * @param int $debtor_no Customer ID (0 = all customers).
 * @return resource Database result.
 */
function get_crm_opportunities_for_customer($debtor_no = 0)
{
	global $SysPrefs;

	if (empty($SysPrefs->prefs['use_crm']))
		return false;

	$sql = "SELECT cl.id, cl.lead_ref, cl.title, cl.company_name,
		cl.expected_revenue, cl.probability,
		IFNULL(cs.name, cl.lead_status) AS stage_name
		FROM " . TB_PREF . "crm_leads cl
		LEFT JOIN " . TB_PREF . "crm_sales_stages cs ON cs.id = cl.stage_id
		WHERE cl.is_opportunity = 1
		AND cl.inactive = 0
		AND cl.lead_status NOT IN ('won','lost')";

	if ($debtor_no > 0)
		$sql .= " AND cl.linked_customer_id = " . db_escape((int)$debtor_no);

	$sql .= " ORDER BY cl.lead_ref DESC";

	return db_query($sql, "Could not get CRM opportunities for customer");
}
