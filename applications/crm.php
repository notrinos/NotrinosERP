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
 * CRM (Customer Relationship Management) Application Module
 *
 * Provides comprehensive CRM functionality including:
 * - Lead management with source tracking and qualification
 * - Opportunity pipeline with configurable stages
 * - Activity scheduling and chaining (suggest/trigger next)
 * - Sales team management with assignment rules
 * - Campaign management with ROI tracking
 * - Contract management with renewal alerts
 * - Appointment scheduling
 * - Communication logging and timeline
 * - Pipeline analytics and reporting
 *
 * This module can be enabled/disabled via Company Setup → Optional Modules → CRM.
 */
class CrmApp extends application {

	function __construct() {
		parent::__construct('crm', _($this->help_context = '&CRM'));

		// ═══════════════════════════════════════════════════════════
		// Module 0: Transactions
		// ═══════════════════════════════════════════════════════════
		$this->add_module(_('Transactions'));
		$this->add_lapp_function(0, _('New &Lead'),                    'crm/transactions/lead_entry.php?',                    'SA_CRM_LEAD',         MENU_TRANSACTION);
		$this->add_lapp_function(0, _('New &Opportunity'),             'crm/transactions/opportunity_entry.php?',             'SA_CRM_OPPORTUNITY',  MENU_TRANSACTION);
		$this->add_lapp_function(0, _('&Schedule Activity'),           'crm/transactions/schedule_activity.php?',             'SA_CRM_ACTIVITY',     MENU_TRANSACTION);
		$this->add_lapp_function(0, _('&Convert Lead'),                'crm/transactions/convert_lead.php?',                  'SA_CRM_LEAD',         MENU_TRANSACTION);

		$this->add_rapp_function(0, _('New C&ampaign'),                'crm/transactions/campaign_entry.php?',                'SA_CRM_CAMPAIGN',     MENU_TRANSACTION);
		$this->add_rapp_function(0, _('New Co&ntract'),                'crm/transactions/contract_entry.php?',                'SA_CRM_CONTRACT',     MENU_TRANSACTION);
		$this->add_rapp_function(0, _('New A&ppointment'),             'crm/transactions/appointment_entry.php?',             'SA_CRM_APPOINTMENT',  MENU_TRANSACTION);
		$this->add_rapp_function(0, _('&Bulk Operations'),             'crm/transactions/bulk_operations.php?',               'SA_CRM_LEAD',         MENU_TRANSACTION);

		// ═══════════════════════════════════════════════════════════
		// Module 1: Inquiries and Reports
		// ═══════════════════════════════════════════════════════════
		$this->add_module(_('Inquiries and Reports'));
		$this->add_lapp_function(1, _('&Pipeline View'),               'crm/inquiry/pipeline_view.php?',                      'SA_CRM_PIPELINE',     MENU_INQUIRY);
		$this->add_lapp_function(1, _('&Lead Inquiry'),                'crm/inquiry/lead_inquiry.php?',                       'SA_CRM_PIPELINE',     MENU_INQUIRY);
		$this->add_lapp_function(1, _('&Activity Inquiry'),            'crm/inquiry/activity_inquiry.php?',                   'SA_CRM_PIPELINE',     MENU_INQUIRY);
		$this->add_lapp_function(1, _('C&ampaign Inquiry'),            'crm/inquiry/campaign_inquiry.php?',                   'SA_CRM_REPORT',       MENU_INQUIRY);

		$this->add_rapp_function(1, _('Pipe&line Analysis'),           'crm/reporting/pipeline_analysis.php?',                'SA_CRM_REPORT',       MENU_REPORT);
		$this->add_rapp_function(1, _('&Win/Loss Report'),             'crm/reporting/win_loss_report.php?',                  'SA_CRM_REPORT',       MENU_REPORT);
		$this->add_rapp_function(1, _('&Expected Revenue'),            'crm/reporting/expected_revenue.php?',                 'SA_CRM_REPORT',       MENU_REPORT);
		$this->add_rapp_function(1, _('&Forecast Report'),             'crm/reporting/forecast_report.php?',                  'SA_CRM_REPORT',       MENU_REPORT);
		$this->add_rapp_function(1, _('Lead &Source Report'),          'crm/reporting/lead_source_report.php?',               'SA_CRM_REPORT',       MENU_REPORT);
		$this->add_rapp_function(1, _('&Team Performance'),            'crm/reporting/team_performance.php?',                 'SA_CRM_REPORT',       MENU_REPORT);

		// ═══════════════════════════════════════════════════════════
		// Module 2: Maintenance
		// ═══════════════════════════════════════════════════════════
		$this->add_module(_('Maintenance'));
		$this->add_lapp_function(2, _('Manage &Leads'),                'crm/manage/leads.php?',                               'SA_CRM_LEAD',         MENU_ENTRY);
		$this->add_lapp_function(2, _('Manage &Opportunities'),        'crm/manage/opportunities.php?',                       'SA_CRM_OPPORTUNITY',  MENU_ENTRY);
		$this->add_lapp_function(2, _('Manage C&ampaigns'),            'crm/manage/campaigns.php?',                           'SA_CRM_CAMPAIGN',     MENU_ENTRY);
		$this->add_lapp_function(2, _('Manage Co&ntracts'),            'crm/manage/contracts.php?',                           'SA_CRM_CONTRACT',     MENU_ENTRY);
		$this->add_lapp_function(2, _('Manage A&ppointments'),         'crm/manage/appointments.php?',                        'SA_CRM_APPOINTMENT',  MENU_ENTRY);
		$this->add_lapp_function(2, _('Sa&les Teams'),                 'crm/manage/sales_teams.php?',                         'SA_CRM_TEAM',         MENU_MAINTENANCE);

		$this->add_rapp_function(2, _('Lead &Sources'),                'crm/manage/lead_sources.php?',                        'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Pipeline &Stages'),             'crm/manage/sales_stages.php?',                        'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Lost &Reasons'),                'crm/manage/lost_reasons.php?',                        'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Acti&vity Types'),              'crm/manage/activity_types.php?',                      'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Activity P&lans'),              'crm/manage/activity_plans.php?',                      'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Appointment T&ypes'),           'crm/manage/appointment_types.php?',                   'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('Email &Templates'),             'crm/manage/email_templates.php?',                     'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('&Tags'),                        'crm/manage/tags.php?',                                'SA_CRM_SETTINGS',     MENU_MAINTENANCE);
		$this->add_rapp_function(2, _('CRM Se&ttings'),                'crm/manage/crm_settings.php?',                        'SA_CRM_SETTINGS',     MENU_SETTINGS);

		$this->add_extensions();
	}
}
