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
 * CRM Pipeline Update - AJAX endpoint for drag-and-drop stage changes
 *
 * Accepts POST: lead_id, stage_id
 * Returns JSON: {success: true} or {error: "message"}
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_PIPELINE';
$path_to_root  = '../..';

ob_start();
include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/crm/includes/crm_ajax.inc');
crm_ajax_require_access($page_security);
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_entity.inc');
include_once($path_to_root . '/crm/includes/db/crm_sales_stages_entity.inc');

$lead_id  = isset($_POST['lead_id'])  ? (int)$_POST['lead_id']  : 0;
$stage_id = isset($_POST['stage_id']) ? (int)$_POST['stage_id'] : 0;

if ($lead_id <= 0 || $stage_id <= 0) {
    crm_ajax_response(array('error' => 'Invalid parameters'), 400);
}

$lead = crm_leads_entity::find_joined($lead_id);
if (!$lead || !$lead['is_opportunity']) {
    crm_ajax_response(array('error' => 'Opportunity not found'), 404);
}

$stage = crm_sales_stages_entity::find($stage_id);
if (!$stage || !$stage['active']) {
    crm_ajax_response(array('error' => 'Active pipeline stage not found'), 400);
}

if (!begin_transaction()) {
    crm_ajax_response(array('error' => 'Could not start pipeline update'), 500);
}
if (!crm_leads_entity::update_stage($lead_id, $stage_id) || !commit_transaction()) {
    cancel_transaction();
    crm_ajax_response(array('error' => 'Could not update pipeline stage'), 500);
}

crm_ajax_response(array('success' => true, 'lead_id' => $lead_id, 'stage_id' => $stage_id), 200);

