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
 * CRM Module Settings Database Layer
 *
 * Provides functions to read and write CRM-specific settings stored
 * in the 0_crm_module_settings table.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

/**
 * Retrieve a single CRM setting value.
 *
 * @param string $key     The setting key.
 * @param mixed  $default Default value if the key does not exist.
 * @return mixed
 */
function get_crm_setting($key, $default = null)
{
    $sql = "SELECT setting_value FROM " . TB_PREF . "crm_module_settings
            WHERE setting_key = " . db_escape($key);
    $result = db_query($sql, "Could not retrieve CRM setting");
    $row = db_fetch($result);
    return $row ? $row['setting_value'] : $default;
}

/**
 * Update or insert a CRM setting.
 *
 * @param string $key         The setting key.
 * @param string $value       The setting value.
 * @param string $description Optional description.
 * @return bool
 */
function set_crm_setting($key, $value, $description = null)
{
    $existing = get_crm_setting($key);
    if ($existing !== null) {
        $sql = "UPDATE " . TB_PREF . "crm_module_settings SET "
            . "setting_value = " . db_escape($value);
        if ($description !== null) {
            $sql .= ", description = " . db_escape($description);
        }
        $sql .= " WHERE setting_key = " . db_escape($key);
    } else {
        $sql = "INSERT INTO " . TB_PREF . "crm_module_settings
                (setting_key, setting_value, description) VALUES ("
            . db_escape($key) . ", "
            . db_escape($value) . ", "
            . db_escape($description) . ")";
    }
    return db_query($sql, "Could not save CRM setting");
}

/**
 * Retrieve all CRM settings as an associative array.
 *
 * @return array  key => value pairs
 */
function get_all_crm_settings()
{
    $sql = "SELECT setting_key, setting_value, description
            FROM " . TB_PREF . "crm_module_settings ORDER BY setting_key";
    $result = db_query($sql, "Could not retrieve CRM settings");
    $settings = array();
    while ($row = db_fetch($result)) {
        $settings[$row['setting_key']] = $row;
    }
    return $settings;
}

/**
 * Delete a CRM setting.
 *
 * @param string $key
 * @return bool
 */
function delete_crm_setting($key)
{
    $sql = "DELETE FROM " . TB_PREF . "crm_module_settings
            WHERE setting_key = " . db_escape($key);
    return db_query($sql, "Could not delete CRM setting");
}
