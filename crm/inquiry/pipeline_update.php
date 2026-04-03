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

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');

header('Content-Type: application/json');

$lead_id  = isset($_POST['lead_id'])  ? (int)$_POST['lead_id']  : 0;
$stage_id = isset($_POST['stage_id']) ? (int)$_POST['stage_id'] : 0;

if ($lead_id <= 0 || $stage_id <= 0) {
    echo json_encode(array('error' => 'Invalid parameters'));
    exit;
}

$lead = get_crm_lead($lead_id);
if (!$lead || !$lead['is_opportunity']) {
    echo json_encode(array('error' => 'Opportunity not found'));
    exit;
}

begin_transaction();
update_opportunity_stage($lead_id, $stage_id);
commit_transaction();

echo json_encode(array('success' => true, 'lead_id' => $lead_id, 'stage_id' => $stage_id));

