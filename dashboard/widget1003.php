<?php
/**
 * CRM Dashboard Widget 1003 — Recent Leads/Opportunities (table).
 */
$width = 100;
$limit = 10;
$result = dashboard_crm_recent_leads($limit);
$title = sprintf(_('Recent %s Leads & Opportunities'), $limit);

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	if ($result) {
		$th = array(_('Ref'), _('Title'), _('Company'), _('Status'), _('Stage'), _('Revenue'), _('Created'));
		start_table(TABLESTYLE, "width='$width%'");
		table_header($th);
		$k = 0;
		while ($myrow = db_fetch($result)) {
			alt_table_row_color($k);
			label_cell($myrow['lead_ref']);
			label_cell($myrow['title']);
			label_cell($myrow['company_name']);
			$status_label = $myrow['is_opportunity'] ? _('Opportunity') : _('Lead');
			$status_label .= ' / '.ucfirst($myrow['lead_status']);
			label_cell($status_label);
			label_cell($myrow['stage_name']);
			amount_cell($myrow['expected_revenue']);
			label_cell(sql2date(substr($myrow['date_created'], 0, 10)));
			end_row();
		}
		end_table();
	} else {
		display_note(_('CRM tables not available.'));
	}
}

$widget->End();
