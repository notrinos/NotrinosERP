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
 * DIM-CRM-001: lead conversion follow-up queue.
 *
 * Lists the customers created from a lead whose company-required Dimension category was explicitly left
 * unassigned at conversion. The queue is read-only and derived from the verified conversion revisions and the
 * customer's current categories, so a customer disappears from it as soon as it is classified through the
 * normal customer maintenance page. No category is guessed, copied or written here.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_LEAD';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_conversion_classification_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

page(_($help_context = 'Lead Conversion Follow-up'));

if (!crm_conversion_ready()) {
    display_note(_('The Dimension source revision schema is not installed, so conversion follow-ups are not recorded for this company.'));
    end_page();
    exit;
}

$queue = crm_conversion_follow_up_queue();

if (!count($queue)) {
    display_note(_('No converted customer is waiting for a required category.'));
} else {
    display_note(_('These customers were created from leads while a required category was left unassigned. Classify each customer in customer maintenance; it leaves this list once every owed category has a value.'));
    start_table(TABLESTYLE, "width='80%'");
    table_header(array(_('Lead'), _('Customer'), _('Converted'), _('Category still unassigned')));
    $k = 0;
    foreach ($queue as $entry) {
        alt_table_row_color($k);
        label_cell($entry['lead_ref']);
        if ($entry['customer_name'] !== null)
            label_cell('<a href="' . $path_to_root . '/sales/manage/customers.php?debtor_no=' . (int)$entry['customer_id'] . '">'
                . htmlspecialchars('#' . $entry['customer_id'] . ' ' . $entry['customer_name']) . '</a>');
        else
            label_cell('#' . (int)$entry['customer_id']);
        label_cell(sql2date($entry['converted']));
        $names = array();
        foreach ($entry['pending_axes'] as $axis)
            $names[] = dimension_category_policy_label($axis);
        label_cell(htmlspecialchars(implode(', ', $names)));
        end_row();
    }
    end_table(1);
}

end_page();
