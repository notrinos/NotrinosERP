<?php
/**********************************************************************
  Copyright (C) FrontAccounting, LLC.
  Released under the terms of the GNU General Public License, GPL, 
  as published by the Free Software Foundation, either version 3 
  of the License, or (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
  See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
 ***********************************************************************/

function next_depreciation_date($depreciation_date) {

  $start = strtotime($depreciation_date);

  $y = date('Y', $start);
  $m = date('n', $start) + 1;

  if ($m > 12) {
    $m = 1;
    $y++;
  }

  return strtotime("$y-$m-1");
}

function depreciation_months($depreciation_start) {

  // assume that depreciation start is the same fiscal year
  //$start = strtotime($depreciation_start);
  //$end = strtotime($year['end']);

  $start = next_depreciation_date($depreciation_start);

  return 12 - date('n', $start) + 1;

  //$d1 = date('j', $start); // day of the month
  //$d2 = date('t', $start); // number of days in month

  //if ($d2 > $d1)
    //$months++;
}

function months_between_dates($start, $end) {
  $start = strtotime($start);
  $end = strtotime($end);

  $y1 = date('Y', $start);
  $m1 = date('n', $start);

  $y2 = date('Y', $end);
  $m2 = date('n', $end);

  return 12 * ($y2 - $y1) + $m2 - $m1;
}

function compute_gl_rows_for_depreciation($item, $no_months, $period) {
  $rows = array();

  $year = get_current_fiscalyear();
  $y = date('Y', strtotime($year['end']));

  switch ($item['depreciation_method']) {
	case 'D':
		$line_value = $item['purchase_cost']*$item['depreciation_rate']/100/12;
		$value = $item['material_cost'] * $item['depreciation_rate'] * $item['depreciation_factor']/100/12;
		if ($value < $line_value)
			$value = $line_value;
		break;

    case 'S':	// purchase_cost stores start cost of item
		$done_months = months_between_dates($item['depreciation_start'], $item['depreciation_date']);
		$remaining_months = 12.0 * 100.0/$item['depreciation_rate'] - $done_months;
		$value = $item['purchase_cost']*$item['depreciation_rate']/100/12;
		break;

	case 'N':
		$N = $item['depreciation_rate'];
		$done_years = months_between_dates($item['depreciation_start'], $item['depreciation_date'])/12;
		$value = $item['purchase_cost']* ($N-$done_years)/($N*($N+1)/2)/12;
		break;

    case 'O':
		$value = $item['material_cost'];
		break;
  }

  $next = next_depreciation_date($item['depreciation_date']);
  $m = date('n', $next);

  $total = 0;
  $cnt = 0;
  for ($i=$m; $i < $m + $no_months; $i++) {

	if ($item['depreciation_method'] == 'S') {
		if ($cnt >= $remaining_months)
			$value = 0;
	}

    $date = sql2date(date("$y-$i-t", strtotime("$y-$i-1")));

    $total += $value;

    if (FA_YEARLY == $period) {
      // yearly
      if ($i == $m + $no_months - 1)
        $rows[] = array('date' => $date, 'value' => $total);
      /*else
        $rows[] = array('date' => $date, 'value' => 0);
        */
    }
    else {
      // monthly
      $rows[] = array('date' => $date, 'value' => $value);
    }

    $cnt++;

	if ($item['depreciation_method'] == 'O') {
		// depreciate only in the first month
		$value = 0;
	}
 }

  return $rows;
}
