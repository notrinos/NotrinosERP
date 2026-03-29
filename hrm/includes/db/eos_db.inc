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
 * End of Service calculation tiers CRUD
 */

/**
 * Add EOS tier.
 *
 * @param float $from_years
 * @param float|null $to_years
 * @param float $termination_rate
 * @param float $resignation_rate
 * @param string $description
 * @return int
 */
function add_eos_tier($from_years, $to_years, $termination_rate, $resignation_rate, $description='') {
    $sql = "INSERT INTO ".TB_PREF."eos_calculation
        (from_years, to_years, termination_rate, resignation_rate, description)
        VALUES (
            ".db_escape((float)$from_years).",
            ".($to_years === null || $to_years === '' ? 'NULL' : db_escape((float)$to_years)).",
            ".db_escape((float)$termination_rate).",
            ".db_escape((float)$resignation_rate).",
            ".db_escape($description)."
        )";
    db_query($sql, 'could not add eos tier');

    return (int)db_insert_id();
}

/**
 * Update EOS tier.
 *
 * @param int $eos_id
 * @param float $from_years
 * @param float|null $to_years
 * @param float $termination_rate
 * @param float $resignation_rate
 * @param string $description
 * @return bool
 */
function update_eos_tier($eos_id, $from_years, $to_years, $termination_rate, $resignation_rate, $description='') {
    $sql = "UPDATE ".TB_PREF."eos_calculation SET
        from_years = ".db_escape((float)$from_years).",
        to_years = ".($to_years === null || $to_years === '' ? 'NULL' : db_escape((float)$to_years)).",
        termination_rate = ".db_escape((float)$termination_rate).",
        resignation_rate = ".db_escape((float)$resignation_rate).",
        description = ".db_escape($description)."
        WHERE eos_id = ".db_escape((int)$eos_id);

    return (bool)db_query($sql, 'could not update eos tier');
}

/**
 * Get one EOS tier.
 *
 * @param int $eos_id
 * @return array|false
 */
function get_eos_tier($eos_id) {
    $sql = "SELECT * FROM ".TB_PREF."eos_calculation WHERE eos_id = ".db_escape((int)$eos_id);
    $result = db_query($sql, 'could not get eos tier');
    return db_fetch_assoc($result);
}

/**
 * Get EOS tiers.
 *
 * @return resource
 */
function get_eos_tiers() {
    $sql = "SELECT * FROM ".TB_PREF."eos_calculation ORDER BY from_years, eos_id";
    return db_query($sql, 'could not get eos tiers');
}

/**
 * Delete EOS tier.
 *
 * @param int $eos_id
 * @return bool
 */
function delete_eos_tier($eos_id) {
    $sql = "DELETE FROM ".TB_PREF."eos_calculation WHERE eos_id = ".db_escape((int)$eos_id);
    return (bool)db_query($sql, 'could not delete eos tier');
}

/**
 * Calculate EOS amount from years of service and monthly salary.
 *
 * @param float $monthly_salary
 * @param float $years_of_service
 * @param int $is_resignation
 * @return float
 */
function calculate_eos_amount($monthly_salary, $years_of_service, $is_resignation=0) {
    $monthly_salary = (float)$monthly_salary;
    $years_of_service = max((float)$years_of_service, 0);
    if (floatcmp($monthly_salary, 0) <= 0 || floatcmp($years_of_service, 0) <= 0)
        return 0.0;

    $rate_field = (int)$is_resignation ? 'resignation_rate' : 'termination_rate';
    $tiers = get_eos_tiers();

    $amount = 0.0;
    while ($tier = db_fetch_assoc($tiers)) {
        $from = (float)$tier['from_years'];
        $to = ($tier['to_years'] === null || $tier['to_years'] === '') ? null : (float)$tier['to_years'];
        if (floatcmp($years_of_service, $from) < 0)
            continue;

        $segment_years = 0.0;
        if ($to === null)
            $segment_years = max($years_of_service - $from, 0);
        else
            $segment_years = max(min($years_of_service, $to) - $from, 0);

        if (floatcmp($segment_years, 0) <= 0)
            continue;

        $rate = (float)$tier[$rate_field] / 100;
        $amount += ($monthly_salary * $segment_years * $rate);
    }

    return round2($amount, user_price_dec());
}

