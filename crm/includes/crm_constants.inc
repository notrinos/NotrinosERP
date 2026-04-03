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
 * CRM Module Constants
 *
 * Defines all constants for priority levels, lead statuses, opportunity
 * stages, activity statuses, campaign statuses, contract statuses,
 * appointment statuses, and entity types used throughout the CRM module.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

// ================================================================
// LEAD / OPPORTUNITY STATUSES
// ================================================================
define('CRM_LEAD_NEW',        'new');
define('CRM_LEAD_CONTACTED',  'contacted');
define('CRM_LEAD_QUALIFIED',  'qualified');
define('CRM_LEAD_CONVERTED',  'converted');
define('CRM_LEAD_LOST',       'lost');

/**
 * Returns an array of all lead status labels keyed by status code.
 *
 * @return array
 */
function crm_lead_statuses()
{
    return array(
        CRM_LEAD_NEW       => _('New'),
        CRM_LEAD_CONTACTED => _('Contacted'),
        CRM_LEAD_QUALIFIED => _('Qualified'),
        CRM_LEAD_CONVERTED => _('Converted'),
        CRM_LEAD_LOST      => _('Lost'),
    );
}

// ================================================================
// PRIORITY LEVELS
// ================================================================
define('CRM_PRIORITY_NORMAL', 0);
define('CRM_PRIORITY_LOW',    1);
define('CRM_PRIORITY_MEDIUM', 2);
define('CRM_PRIORITY_HIGH',   3);

/**
 * Returns an array of all priority labels keyed by priority code.
 *
 * @return array
 */
function crm_priority_levels()
{
    return array(
        CRM_PRIORITY_NORMAL => _('Normal'),
        CRM_PRIORITY_LOW    => _('Low'),
        CRM_PRIORITY_MEDIUM => _('Medium'),
        CRM_PRIORITY_HIGH   => _('High'),
    );
}

// ================================================================
// ACTIVITY STATUSES
// ================================================================
define('CRM_ACTIVITY_PLANNED',   'planned');
define('CRM_ACTIVITY_DONE',      'done');
define('CRM_ACTIVITY_CANCELLED', 'cancelled');
define('CRM_ACTIVITY_OVERDUE',   'overdue');

/**
 * Returns an array of all activity status labels.
 *
 * @return array
 */
function crm_activity_statuses()
{
    return array(
        CRM_ACTIVITY_PLANNED   => _('Planned'),
        CRM_ACTIVITY_DONE      => _('Done'),
        CRM_ACTIVITY_CANCELLED => _('Cancelled'),
        CRM_ACTIVITY_OVERDUE   => _('Overdue'),
    );
}

// ================================================================
// ACTIVITY CATEGORIES
// ================================================================
define('CRM_ACT_CALL',    'call');
define('CRM_ACT_EMAIL',   'email');
define('CRM_ACT_MEETING', 'meeting');
define('CRM_ACT_TODO',    'todo');
define('CRM_ACT_UPLOAD',  'upload');

/**
 * Returns an array of activity category labels.
 *
 * @return array
 */
function crm_activity_categories()
{
    return array(
        CRM_ACT_CALL    => _('Call'),
        CRM_ACT_EMAIL   => _('Email'),
        CRM_ACT_MEETING => _('Meeting'),
        CRM_ACT_TODO    => _('To-Do'),
        CRM_ACT_UPLOAD  => _('Upload Document'),
    );
}

// ================================================================
// CAMPAIGN STATUSES
// ================================================================
define('CRM_CAMPAIGN_DRAFT',     'draft');
define('CRM_CAMPAIGN_ACTIVE',    'active');
define('CRM_CAMPAIGN_COMPLETED', 'completed');
define('CRM_CAMPAIGN_CANCELLED', 'cancelled');

/**
 * Returns an array of campaign status labels.
 *
 * @return array
 */
function crm_campaign_statuses()
{
    return array(
        CRM_CAMPAIGN_DRAFT     => _('Draft'),
        CRM_CAMPAIGN_ACTIVE    => _('Active'),
        CRM_CAMPAIGN_COMPLETED => _('Completed'),
        CRM_CAMPAIGN_CANCELLED => _('Cancelled'),
    );
}

// ================================================================
// CONTRACT STATUSES
// ================================================================
define('CRM_CONTRACT_DRAFT',     'draft');
define('CRM_CONTRACT_ACTIVE',    'active');
define('CRM_CONTRACT_EXPIRED',   'expired');
define('CRM_CONTRACT_CANCELLED', 'cancelled');
define('CRM_CONTRACT_RENEWED',   'renewed');

/**
 * Returns an array of contract status labels.
 *
 * @return array
 */
function crm_contract_statuses()
{
    return array(
        CRM_CONTRACT_DRAFT     => _('Draft'),
        CRM_CONTRACT_ACTIVE    => _('Active'),
        CRM_CONTRACT_EXPIRED   => _('Expired'),
        CRM_CONTRACT_CANCELLED => _('Cancelled'),
        CRM_CONTRACT_RENEWED   => _('Renewed'),
    );
}

// ================================================================
// APPOINTMENT STATUSES
// ================================================================
define('CRM_APPT_SCHEDULED',   'scheduled');
define('CRM_APPT_CONFIRMED',   'confirmed');
define('CRM_APPT_COMPLETED',   'completed');
define('CRM_APPT_CANCELLED',   'cancelled');
define('CRM_APPT_NO_SHOW',     'no_show');
define('CRM_APPT_RESCHEDULED', 'rescheduled');

/**
 * Returns an array of appointment status labels.
 *
 * @return array
 */
function crm_appointment_statuses()
{
    return array(
        CRM_APPT_SCHEDULED   => _('Scheduled'),
        CRM_APPT_CONFIRMED   => _('Confirmed'),
        CRM_APPT_COMPLETED   => _('Completed'),
        CRM_APPT_CANCELLED   => _('Cancelled'),
        CRM_APPT_NO_SHOW     => _('No Show'),
        CRM_APPT_RESCHEDULED => _('Rescheduled'),
    );
}

// ================================================================
// COMMUNICATION TYPES
// ================================================================
define('CRM_COMM_EMAIL',   'email');
define('CRM_COMM_CALL',    'call');
define('CRM_COMM_MEETING', 'meeting');
define('CRM_COMM_NOTE',    'note');
define('CRM_COMM_SMS',     'sms');

/**
 * Returns an array of communication type labels.
 *
 * @return array
 */
function crm_communication_types()
{
    return array(
        CRM_COMM_EMAIL   => _('Email'),
        CRM_COMM_CALL    => _('Phone Call'),
        CRM_COMM_MEETING => _('Meeting'),
        CRM_COMM_NOTE    => _('Note'),
        CRM_COMM_SMS     => _('SMS'),
    );
}

// ================================================================
// COMMUNICATION DIRECTION
// ================================================================
define('CRM_DIR_INBOUND',  'inbound');
define('CRM_DIR_OUTBOUND', 'outbound');

// ================================================================
// ENTITY TYPES (for polymorphic associations)
// ================================================================
define('CRM_ENTITY_LEAD',     'lead');
define('CRM_ENTITY_CONTACT',  'contact');
define('CRM_ENTITY_CUSTOMER', 'customer');
define('CRM_ENTITY_CAMPAIGN', 'campaign');

/**
 * Returns an array of entity type labels.
 *
 * @return array
 */
function crm_entity_types()
{
    return array(
        CRM_ENTITY_LEAD     => _('Lead'),
        CRM_ENTITY_CONTACT  => _('Contact'),
        CRM_ENTITY_CUSTOMER => _('Customer'),
    );
}

// ================================================================
// CHAINING TYPES (activity auto-chain)
// ================================================================
define('CRM_CHAIN_NONE',    'none');
define('CRM_CHAIN_SUGGEST', 'suggest');
define('CRM_CHAIN_TRIGGER', 'trigger');

// ================================================================
// LEAD ASSIGNMENT MODES
// ================================================================
define('CRM_ASSIGN_MANUAL', 'manual');
define('CRM_ASSIGN_AUTO',   'auto');

// ================================================================
// LEAD REFERENCE NUMBERING
// ================================================================

/**
 * Generates the next lead reference number.
 *
 * @param string $prefix
 * @return string  e.g. "LD-000001"
 */
function crm_next_lead_ref($prefix = null)
{
    if ($prefix === null) {
        $prefix = get_crm_setting('lead_ref_prefix', 'LD');
    }
    $sql = "SELECT MAX(CAST(SUBSTRING(lead_ref, LENGTH(" . db_escape($prefix . '-') . ")+1) AS UNSIGNED)) as maxnum
            FROM " . TB_PREF . "crm_leads
            WHERE lead_ref LIKE " . db_escape($prefix . '-%');
    $result = db_query($sql, "Could not get max lead ref");
    $row = db_fetch($result);
    $next = ($row && $row['maxnum']) ? $row['maxnum'] + 1 : 1;
    return $prefix . '-' . str_pad($next, 6, '0', STR_PAD_LEFT);
}

/**
 * Generates the next opportunity reference number.
 *
 * @param string $prefix
 * @return string  e.g. "OP-000001"
 */
function crm_next_opportunity_ref($prefix = null)
{
    if ($prefix === null) {
        $prefix = get_crm_setting('opportunity_ref_prefix', 'OP');
    }
    $sql = "SELECT MAX(CAST(SUBSTRING(lead_ref, LENGTH(" . db_escape($prefix . '-') . ")+1) AS UNSIGNED)) as maxnum
            FROM " . TB_PREF . "crm_leads
            WHERE lead_ref LIKE " . db_escape($prefix . '-%');
    $result = db_query($sql, "Could not get max opportunity ref");
    $row = db_fetch($result);
    $next = ($row && $row['maxnum']) ? $row['maxnum'] + 1 : 1;
    return $prefix . '-' . str_pad($next, 6, '0', STR_PAD_LEFT);
}

/**
 * Generates the next contract reference number.
 *
 * @param string $prefix
 * @return string  e.g. "CT-000001"
 */
function crm_next_contract_ref($prefix = null)
{
    if ($prefix === null) {
        $prefix = get_crm_setting('contract_ref_prefix', 'CT');
    }
    $sql = "SELECT MAX(CAST(SUBSTRING(contract_ref, LENGTH(" . db_escape($prefix . '-') . ")+1) AS UNSIGNED)) as maxnum
            FROM " . TB_PREF . "crm_contracts
            WHERE contract_ref LIKE " . db_escape($prefix . '-%');
    $result = db_query($sql, "Could not get max contract ref");
    $row = db_fetch($result);
    $next = ($row && $row['maxnum']) ? $row['maxnum'] + 1 : 1;
    return $prefix . '-' . str_pad($next, 6, '0', STR_PAD_LEFT);
}
