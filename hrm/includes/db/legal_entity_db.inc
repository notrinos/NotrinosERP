<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * HRM-FND-003 bounded Legal Entity foundation.
 *
 * One company-default Legal Entity is seeded from explicit company preferences.
 * This helper does not infer country, establishment, branch, department, cost
 * centre, dimension, work-location, Employment or payroll-relationship meaning.
 */

/** @return bool */
function hrm_legal_entity_table_ready()
{
    if (!function_exists('db_query') || !function_exists('db_escape_function') || !defined('TB_PREF'))
        return false;
    $result = db_query("SHOW TABLES LIKE '".TB_PREF.db_escape_function('hrm_legal_entities')."'");
    return $result && db_num_rows($result) === 1;
}

/**
 * Build the exact legacy-company seed projection. Empty optional legal identifiers
 * remain NULL and domicile is deliberately not interpreted as a country code.
 *
 * @param array $prefs
 * @return array|false
 */
function hrm_legal_entity_projection_from_company_prefs($prefs)
{
    if (!is_array($prefs))
        return false;
    $name = isset($prefs['coy_name']) ? trim((string)$prefs['coy_name']) : '';
    $currency = isset($prefs['curr_default']) ? strtoupper(trim((string)$prefs['curr_default'])) : '';
    if ($name === '' || strlen($name) > 100 || preg_match('/^[A-Z0-9]{3}$/D', $currency) !== 1)
        return false;
    $registration = isset($prefs['coy_no']) ? trim((string)$prefs['coy_no']) : '';
    $tax = isset($prefs['gst_no']) ? trim((string)$prefs['gst_no']) : '';
    if (strlen($registration) > 55 || strlen($tax) > 55)
        return false;
    return array(
        'entity_code' => 'COMPANY',
        'registered_name' => $name,
        'registration_number' => $registration === '' ? null : $registration,
        'tax_registration_number' => $tax === '' ? null : $tax,
        'country_code' => null,
        'base_currency' => $currency,
        'entity_status' => 'active',
        'legacy_default' => 1,
        'source' => 'legacy_company_seed',
    );
}

/** @return array|false|null */
function get_hrm_default_legal_entity($for_update = false)
{
    if (!hrm_legal_entity_table_ready())
        return false;
    $sql = "SELECT legal_entity_id,entity_code,registered_name,registration_number,"
        ."tax_registration_number,country_code,base_currency,entity_status,legacy_default,source,"
        ."created_at,created_by,updated_at,updated_by FROM ".TB_PREF."hrm_legal_entities "
        ."WHERE legacy_default=1";
    if ($for_update)
        $sql .= ' FOR UPDATE';
    $result = db_query($sql, 'could not read default HRM Legal Entity');
    if (!$result)
        return false;
    if (db_num_rows($result) > 1)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}
