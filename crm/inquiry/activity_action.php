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
 * CRM Activity Action - AJAX endpoint for quick activity actions.
 *
 * Accepts POST: action (complete|cancel), activity_id
 * Returns JSON: {success: true} or {error: "message"}
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_ACTIVITY';
$path_to_root  = '../..';

ob_start();
include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/crm/includes/crm_ajax.inc');
crm_ajax_require_access($page_security);
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_entity.inc');

$action      = isset($_POST['action'])      ? $_POST['action']            : '';
$activity_id = isset($_POST['activity_id']) ? (int)$_POST['activity_id'] : 0;

if ($activity_id <= 0) {
    crm_ajax_response(array('error' => 'Invalid activity ID'), 400);
}
if ($action !== 'complete' && $action !== 'cancel') {
    crm_ajax_response(array('error' => 'Unknown action'), 400);
}

$activity = get_crm_activity($activity_id);
if (!$activity) {
    crm_ajax_response(array('error' => 'Activity not found'), 404);
}

if (!begin_transaction()) {
    crm_ajax_response(array('error' => 'Could not start activity update'), 500);
}

if ($action === 'complete') {
    $updated = complete_crm_activity($activity_id);
} else {
    $updated = crm_activities_entity::modify($activity_id, array('status' => CRM_ACTIVITY_CANCELLED));
}

if (!$updated || !commit_transaction()) {
    cancel_transaction();
    crm_ajax_response(array('error' => 'Could not update activity'), 500);
}

crm_ajax_response(array('success' => true, 'activity_id' => $activity_id, 'action' => $action), 200);
