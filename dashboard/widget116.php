<?php

$widget = new Widget();
$widget->setTitle(_('Open Quotations'));
$widget->Start();

if ($widget->checkSecurity('SA_SALESDASHBOARD')) {
    // Count all open quotations (sales type 32), excluding those with matching
    // sales orders (type 30) that share the same reference or have been converted
    $sql = "SELECT COUNT(*)
            FROM ".TB_PREF."sales_orders q
            LEFT JOIN ".TB_PREF."sales_orders so
                ON so.debtor_no = q.debtor_no
                AND so.trans_type = ".ST_SALESORDER."
                AND so.reference LIKE CONCAT('%', q.reference, '%')
            WHERE q.trans_type = ".ST_SALESQUOTE."
                AND so.order_no IS NULL";
    $value = (int)get_dashboard_scalar_value($sql, _('Could not count open quotations'));
    render_dashboard_small_stat_card(_('Open Quotes'), $value, 'file-text', 'info');
}

$widget->End();
