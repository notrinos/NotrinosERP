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
 * CRM Contract Entry - Create / Edit Contract
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_CONTRACT';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_contracts_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

//--------------------------------------------------------------------------
// Determine mode
//--------------------------------------------------------------------------

$contract_id = 0;
$is_new      = true;
$contract    = null;

$raw_id = isset($_GET['ContractID']) ? $_GET['ContractID'] : get_post('ContractID', 0);
if ((int)$raw_id > 0) {
    $contract_id = (int)$raw_id;
    $contract = get_crm_contract($contract_id);
    if (!$contract) {
        display_error(_('Contract not found.'));
        hyperlink_params($path_to_root . '/crm/manage/contracts.php', _('Back to Contracts'), 'sel_app=crm');
        end_page();
        exit;
    }
    $is_new = false;
}

$page_title = $is_new ? _('New Contract') : sprintf(_('Edit Contract #%s'), $contract['contract_ref']);
page(_($help_context = 'CRM Contract Entry'), false, false, '', $js);

//--------------------------------------------------------------------------
// Handle Save
//--------------------------------------------------------------------------

if (isset($_POST['Save'])) {
    $input_error = 0;

    if (strlen(trim($_POST['contract_name'])) < 1) {
        display_error(_('Contract name is required.'));
        set_focus('contract_name');
        $input_error = 1;
    }

    if ((int)$_POST['customer_id'] <= 0) {
        display_error(_('Customer is required.'));
        set_focus('customer_id');
        $input_error = 1;
    }

    if (!is_date($_POST['start_date'])) {
        display_error(_('Start date is required.'));
        $input_error = 1;
    }

    if (!is_date($_POST['end_date'])) {
        display_error(_('End date is required.'));
        $input_error = 1;
    }

    if (is_date($_POST['start_date']) && is_date($_POST['end_date'])
        && date1_greater_date2($_POST['start_date'], $_POST['end_date'])) {
        display_error(_('End date must be after start date.'));
        $input_error = 1;
    }

    if ($input_error == 0) {
        $data = array(
            'title'          => $_POST['contract_name'],
            'customer_id'    => (int)$_POST['customer_id'],
            'status'         => $_POST['status'],
            'start_date'     => date2sql($_POST['start_date']),
            'end_date'       => date2sql($_POST['end_date']),
            'contract_value' => input_num('contract_value', 0),
            'signed_by'      => trim($_POST['signed_by']),
            'signed_date'    => $_POST['signed_date'] ? date2sql($_POST['signed_date']) : null,
            'description'    => $_POST['description'],
        );

        begin_transaction();
        if ($is_new) {
            $data['contract_ref'] = crm_next_contract_ref();
            $data['created_by']   = $_SESSION['wa_current_user']->user;
            $contract_id = add_crm_contract($data);
            display_notification(_('Contract has been created.'));
        } else {
            update_crm_contract($contract_id, $data);
            display_notification(_('Contract has been updated.'));
        }
        commit_transaction();

        if ($is_new) {
            meta_forward($_SERVER['PHP_SELF'], 'ContractID=' . $contract_id . crm_sel_app_param());
        }
        $contract = get_crm_contract($contract_id);
    }
}

//--------------------------------------------------------------------------
// Handle Renew
//--------------------------------------------------------------------------

if (isset($_POST['Renew']) && !$is_new) {
    if (!is_date($_POST['renew_start']) || !is_date($_POST['renew_end'])) {
        display_error(_('Valid renewal dates are required.'));
    } else {
        $new_value = $_POST['renew_value'] != '' ? input_num('renew_value', 0) : (float)$contract['contract_value'];
        begin_transaction();
        $new_id = renew_crm_contract($contract_id,
            date2sql($_POST['renew_start']),
            date2sql($_POST['renew_end']),
            $new_value
        );
        commit_transaction();
        display_notification(_('Contract has been renewed.'));
        meta_forward($_SERVER['PHP_SELF'], 'ContractID=' . $new_id . crm_sel_app_param());
        exit;
    }
}

//--------------------------------------------------------------------------
// Load data into POST
//--------------------------------------------------------------------------

if (!$is_new && !isset($_POST['Save'])) {
    $_POST['contract_name']  = $contract['title'];
    $_POST['customer_id']    = $contract['customer_id'];
    $_POST['status']         = $contract['status'];
    $_POST['start_date']     = sql2date($contract['start_date']);
    $_POST['end_date']       = sql2date($contract['end_date']);
    $_POST['contract_value'] = $contract['contract_value'];
    $_POST['signed_by']      = $contract['signed_by'];
    $_POST['signed_date']    = $contract['signed_date'] ? sql2date($contract['signed_date']) : '';
    $_POST['description']    = $contract['description'];
}

//--------------------------------------------------------------------------
// Display Form
//--------------------------------------------------------------------------

start_form();

if (!$is_new) {
    hidden('ContractID', $contract_id);
}

display_heading(_('Contract Details'));
start_table(TABLESTYLE2);

if (!$is_new) {
    label_row(_('Reference:'), $contract['contract_ref']);
}

text_row(_('Contract Name:'), 'contract_name', null, 60, 100);

// Customer selector
$cust_sql = "SELECT debtor_no, name FROM " . TB_PREF . "debtors_master ORDER BY name";
$cust_res = db_query($cust_sql);
$cust_options = array(0 => _('-- Select Customer --'));
while ($c = db_fetch($cust_res)) {
    $cust_options[$c['debtor_no']] = $c['name'];
}
array_selector_row(_('Customer:'), 'customer_id', null, $cust_options);

$statuses = array(
    CRM_CONTRACT_DRAFT     => _('Draft'),
    CRM_CONTRACT_ACTIVE    => _('Active'),
    CRM_CONTRACT_SIGNED    => _('Signed'),
    CRM_CONTRACT_EXPIRED   => _('Expired'),
    CRM_CONTRACT_CANCELLED => _('Cancelled'),
    CRM_CONTRACT_RENEWED   => _('Renewed'),
);
array_selector_row(_('Status:'), 'status', null, $statuses);

date_row(_('Start Date:'), 'start_date');
date_row(_('End Date:'), 'end_date');
amount_row(_('Contract Value:'), 'contract_value', null, null, '', 0);
text_row(_('Signed By:'), 'signed_by', null, 60, 200);
date_row(_('Signed Date:'), 'signed_date', '', null, 0, 0, 0);
textarea_row(_('Description:'), 'description', null, 60, 4);

end_table(1);

echo "<center>";
submit('Save', $is_new ? _('Create Contract') : _('Update Contract'), true, '', 'default');
echo "</center><br>";

//--------------------------------------------------------------------------
// Renewal section (existing only)
//--------------------------------------------------------------------------

if (!$is_new && in_array($contract['status'], array(CRM_CONTRACT_ACTIVE, CRM_CONTRACT_SIGNED, CRM_CONTRACT_EXPIRED))) {
    display_heading(_('Renew Contract'));
    start_table(TABLESTYLE2);
    date_row(_('New Start Date:'), 'renew_start', '', null, 0, 0, 0);
    date_row(_('New End Date:'), 'renew_end', '', null, 0, 0, 0);
    amount_row(_('New Value (blank = same):'), 'renew_value', '', null, '', 0);
    end_table(1);

    echo "<center>";
    submit('Renew', _('Renew Contract'), true);
    echo "</center><br>";
}

end_form();
end_page();

