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
 * Return HR settings metadata and default values.
 *
 * @return array
 */
function hr_setting_definitions() {
    return array(
        'weekend_day' => array(
            'category' => 'hrm.settings',
            'type' => 'tinyint',
            'length' => 1,
            'default' => '5'
        ),
        'default_work_hours' => array(
            'category' => 'hrm.settings',
            'type' => 'double',
            'length' => 10,
            'default' => '8'
        ),
        'payroll_attendance_calculate' => array(
            'category' => 'hrm.payroll',
            'type' => 'tinyint',
            'length' => 1,
            'default' => '0'
        ),
        'payroll_month_work_days' => array(
            'category' => 'hrm.payroll',
            'type' => 'smallint',
            'length' => 6,
            'default' => '30'
        ),
        'attendance_deduction_type' => array(
            'category' => 'hrm.attendance',
            'type' => 'tinyint',
            'length' => 1,
            'default' => '0'
        ),
        'hrm_absence_deduct_from' => array(
            'category' => 'hrm.attendance',
            'type' => 'tinyint',
            'length' => 1,
            'default' => '0'
        ),
        'payroll_payable_act' => array(
            'category' => 'hrm.payroll',
            'type' => 'varchar',
            'length' => 15,
            'default' => ''
        )
    );
}

/**
 * Ensure one HR preference key exists in sys_prefs.
 *
 * @param string $setting_key
 * @return bool
 */
function ensure_hr_setting_exists($setting_key) {
    $definitions = hr_setting_definitions();
    if (!isset($definitions[$setting_key]))
        return false;

    $current = get_company_pref($setting_key);
    if ($current !== null)
        return true;

    $meta = $definitions[$setting_key];
    return (bool)set_company_pref(
        $setting_key,
        $meta['category'],
        $meta['type'],
        $meta['length'],
        $meta['default']
    );
}

/**
 * Get one HR setting value with fallback default.
 *
 * @param string $setting_key
 * @param mixed $fallback
 * @return mixed
 */
function get_hr_setting_value($setting_key, $fallback=null) {
    $definitions = hr_setting_definitions();
    if (isset($definitions[$setting_key]))
        ensure_hr_setting_exists($setting_key);

    $value = get_company_pref($setting_key);
    if ($value !== null)
        return $value;

    if (isset($definitions[$setting_key]))
        return $definitions[$setting_key]['default'];

    return $fallback;
}

/**
 * Get all or selected HR settings as key-value array.
 *
 * @param array $setting_keys
 * @return array
 */
function get_hr_settings_values($setting_keys=array()) {
    $definitions = hr_setting_definitions();
    if (empty($setting_keys))
        $setting_keys = array_keys($definitions);

    $result = array();
    foreach ($setting_keys as $setting_key)
        $result[$setting_key] = get_hr_setting_value($setting_key);

    return $result;
}

/**
 * Save HR settings using existing company preference infrastructure.
 *
 * @param array $settings
 * @return bool
 */
function save_hr_settings_values($settings) {
    if (!is_array($settings) || empty($settings))
        return false;

    $definitions = hr_setting_definitions();
    $to_update = array();

    foreach ($settings as $setting_key => $value) {
        if (!isset($definitions[$setting_key]))
            continue;

        ensure_hr_setting_exists($setting_key);
        $to_update[$setting_key] = $value;
    }

    if (empty($to_update))
        return false;

    return update_company_prefs($to_update);
}

