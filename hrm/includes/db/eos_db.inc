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
 * End of Service calculation — high-level business logic.
 *
 * CRUD for eos_calculation tiers is handled by eos_rules_entity
 * via static API: ::find(), ::all(), ::create(), ::modify(), ::remove().
 */

include_once($path_to_root . '/hrm/includes/db/eos_calculation_entity.inc');

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
    $tiers = eos_calculation_entity::all_db_resource(null, array('from_years', 'eos_id'));

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

