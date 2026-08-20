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

/**
 * Return the exact active compatibility Legal Entity used by legacy Employment.
 * A missing/tampered default fails closed; this helper never infers another entity.
 *
 * @param bool $for_update
 * @return int|false
 */
function get_hrm_default_legal_entity_binding_id($for_update = false)
{
    $row = get_hrm_default_legal_entity($for_update);
    if (!is_array($row) || (int)$row['legal_entity_id'] <= 0
        || (int)$row['legacy_default'] !== 1
        || (string)$row['entity_code'] !== 'COMPANY'
        || (string)$row['entity_status'] !== 'active'
        || (string)$row['source'] !== 'legacy_company_seed')
        return false;
    return (int)$row['legal_entity_id'];
}


/**
 * Seed the compatibility Legal Entity only during a brand-new company install.
 * This is initial state, not an upgrade transition: it is allowed only while
 * both Legal Entity and Employment are empty, after company preferences exist.
 *
 * @return int|false
 */
function initialize_hrm_fresh_default_legal_entity()
{
    if (!hrm_legal_entity_table_ready() || !function_exists('db_query')
        || !function_exists('db_escape') || !defined('TB_PREF'))
        return false;
    $employment = db_query("SELECT COUNT(*) row_count FROM ".TB_PREF."hrm_employments");
    $employment = $employment ? db_fetch_assoc($employment) : false;
    $entities = db_query("SELECT COUNT(*) row_count FROM ".TB_PREF."hrm_legal_entities");
    $entities = $entities ? db_fetch_assoc($entities) : false;
    if (!$employment || !$entities || (int)$employment['row_count'] !== 0
        || (int)$entities['row_count'] !== 0)
        return false;
    $prefs = array();
    $result = db_query("SELECT name,value FROM ".TB_PREF."sys_prefs "
        ."WHERE name IN ('coy_name','coy_no','gst_no','curr_default')");
    if (!$result)
        return false;
    while ($row = db_fetch_assoc($result))
        $prefs[(string)$row['name']] = (string)$row['value'];
    $projection = hrm_legal_entity_projection_from_company_prefs($prefs);
    if (!$projection)
        return false;
    $actor = function_exists('user_id') ? (int)user_id() : 0;
    $sql = "INSERT INTO ".TB_PREF."hrm_legal_entities (entity_code,registered_name,registration_number,"
        ."tax_registration_number,country_code,base_currency,entity_status,legacy_default,source,created_by) VALUES ("
        .db_escape($projection['entity_code']).",".db_escape($projection['registered_name']).","
        .($projection['registration_number'] === null ? 'NULL' : db_escape($projection['registration_number'])).","
        .($projection['tax_registration_number'] === null ? 'NULL' : db_escape($projection['tax_registration_number'])).",NULL,"
        .db_escape($projection['base_currency']).",".db_escape($projection['entity_status']).",1,"
        .db_escape($projection['source']).",".$actor.")";
    if (!db_query($sql, 'could not initialize fresh-install HRM default Legal Entity'))
        return false;
    $id = get_hrm_default_legal_entity_binding_id(false);
    return $id === false ? false : (int)$id;
}
