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

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');

header('Content-Type: application/json');

$action      = isset($_POST['action'])      ? $_POST['action']            : '';
$activity_id = isset($_POST['activity_id']) ? (int)$_POST['activity_id'] : 0;

if ($activity_id <= 0) {
    echo json_encode(array('error' => 'Invalid activity ID'));
    exit;
}

$activity = get_crm_activity($activity_id);
if (!$activity) {
    echo json_encode(array('error' => 'Activity not found'));
    exit;
}

begin_transaction();

if ($action === 'complete') {
    complete_crm_activity($activity_id);
} elseif ($action === 'cancel') {
    update_crm_activity($activity_id, array('status' => CRM_ACTIVITY_CANCELLED));
} else {
    echo json_encode(array('error' => 'Unknown action'));
    exit;
}

commit_transaction();

echo json_encode(array('success' => true, 'activity_id' => $activity_id, 'action' => $action));
