<?php

include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

$widget = new Widget();
$widget->setTitle(_('AR Overdue'));
$widget->Start();

if ($widget->checkSecurity('SA_SALESDASHBOARD')) {
    $aging = get_aging_summary();
    $total_overdue = $aging['1_30'] + $aging['31_60'] + $aging['61_90'] + $aging['90_plus'];

    echo "<table class='dashboard-mini-table'>";
    echo "<thead><tr>
        <th>"._('Current')."</th>
        <th>"._('1-30')."</th>
        <th>"._('31-60')."</th>
        <th>"._('61-90')."</th>
        <th>"._('90+')."</th>
        <th>"._('Total')."</th>
    </tr></thead>";
    echo "<tbody><tr>";
    echo "<td class='right'>".price_format($aging['current'])."</td>";
    echo "<td class='right".(($aging['1_30'] > 0) ? ' text-warning' : '')."'>".price_format($aging['1_30'])."</td>";
    echo "<td class='right".(($aging['31_60'] > 0) ? ' text-warning' : '')."'>".price_format($aging['31_60'])."</td>";
    echo "<td class='right".(($aging['61_90'] > 0) ? ' text-danger' : '')."'>".price_format($aging['61_90'])."</td>";
    echo "<td class='right".(($aging['90_plus'] > 0) ? ' text-danger' : '')."'>".price_format($aging['90_plus'])."</td>";
    echo "<td class='right font-bold'>".price_format($total_overdue)."</td>";
    echo "</tr></tbody></table>";
}

$widget->End();
