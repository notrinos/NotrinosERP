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

/*
    Phase 7: Sales Analytics DB Functions.

    Provides KPI calculations, trend analysis, and report data for
    the Sales Dashboard and advanced reporting.
*/

// -------------------------------------------------------------------------
// KPI Cache Helpers
// -------------------------------------------------------------------------

/**
 * Store or update a cached KPI value.
 *
 * @param string $kpi_type      KPI type identifier.
 * @param string $period_key    Period key (e.g., '2026-04').
 * @param string $dimension_key Dimension key (e.g., salesman_id or '').
 * @param float  $value_1       Primary value.
 * @param float  $value_2       Secondary value.
 * @param float  $value_3       Tertiary value.
 * @param mixed  $detail        Optional detail data (will be JSON encoded).
 */
function set_kpi_cache($kpi_type, $period_key, $dimension_key, $value_1, $value_2 = 0,
    $value_3 = 0, $detail = null)
{
    $detail_json = $detail !== null ? db_escape(json_encode($detail)) : 'NULL';
    $sql = "INSERT INTO ".TB_PREF."sales_kpi_cache
                (kpi_type, period_key, dimension_key, value_1, value_2, value_3, detail_json, updated_at)
            VALUES (".db_escape($kpi_type).", ".db_escape($period_key).", ".db_escape($dimension_key).",
                ".db_escape((float)$value_1).", ".db_escape((float)$value_2).", ".db_escape((float)$value_3).",
                $detail_json, NOW())
            ON DUPLICATE KEY UPDATE
                value_1 = VALUES(value_1), value_2 = VALUES(value_2), value_3 = VALUES(value_3),
                detail_json = VALUES(detail_json), updated_at = NOW()";
    db_query($sql, 'Cannot set KPI cache');
}

/**
 * Get a cached KPI value.
 *
 * @param  string $kpi_type
 * @param  string $period_key
 * @param  string $dimension_key
 * @return array|false
 */
function get_kpi_cache($kpi_type, $period_key, $dimension_key = '')
{
    $sql = "SELECT * FROM ".TB_PREF."sales_kpi_cache
            WHERE kpi_type = ".db_escape($kpi_type)."
                AND period_key = ".db_escape($period_key)."
                AND dimension_key = ".db_escape($dimension_key);
    $result = db_query($sql, 'Cannot get KPI cache');
    return db_fetch($result);
}

// -------------------------------------------------------------------------
// Revenue & Order Metrics
// -------------------------------------------------------------------------

/**
 * Get total revenue for a date range, optionally grouped by period.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return float
 */
function get_total_revenue($date_from, $date_to)
{
    $sql = "SELECT COALESCE(SUM(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount), 0)
            FROM ".TB_PREF."debtor_trans trans
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND trans.tran_date >= ".db_escape($date_from)."
                AND trans.tran_date <= ".db_escape($date_to);
    $result = db_query($sql, 'Cannot get total revenue');
    $row = db_fetch_row($result);
    return (float)$row[0];
}

/**
 * Get order count for a date range.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return int
 */
function get_order_count($date_from, $date_to)
{
    $sql = "SELECT COUNT(*)
            FROM ".TB_PREF."debtor_trans trans
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND trans.tran_date >= ".db_escape($date_from)."
                AND trans.tran_date <= ".db_escape($date_to);
    $result = db_query($sql, 'Cannot get order count');
    $row = db_fetch_row($result);
    return (int)$row[0];
}

/**
 * Get average order value for a date range.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return float
 */
function get_avg_order_value($date_from, $date_to)
{
    $revenue = get_total_revenue($date_from, $date_to);
    $count   = get_order_count($date_from, $date_to);
    return $count > 0 ? $revenue / $count : 0;
}

/**
 * Get revenue trend grouped by month for the last N months.
 *
 * @param  string $period   'monthly' or 'quarterly'
 * @param  int    $months   Number of months to look back.
 * @return array            [['period'=>'2026-01','revenue'=>...,'count'=>...], ...]
 */
function get_revenue_trend($period = 'monthly', $months = 12)
{
    $format = $period == 'monthly' ? '%Y-%m' : '%Y-Q%q';
    $interval = $period == 'monthly' ? 'MONTH' : 'QUARTER';

    $sql = "SELECT DATE_FORMAT(tran_date, ".db_escape($format).") AS period_key,
                COALESCE(SUM(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount), 0) AS revenue,
                COUNT(*) AS order_count
            FROM ".TB_PREF."debtor_trans trans
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND trans.tran_date >= DATE_SUB(CURDATE(), INTERVAL ".((int)$months)." ".$interval.")
            GROUP BY period_key
            ORDER BY period_key ASC";
    $result = db_query($sql, 'Cannot get revenue trend');

    $trend = array();
    while ($row = db_fetch($result)) {
        $trend[] = array(
            'period' => $row['period_key'],
            'revenue' => (float)$row['revenue'],
            'count' => (int)$row['order_count'],
        );
    }
    return $trend;
}

/**
 * Get quotation conversion rate for a date range.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return array              ['total_quotes'=>int, 'converted'=>int, 'rate'=>float]
 */
function get_quote_conversion_rate($date_from, $date_to)
{
    $sql_total = "SELECT COUNT(*)
        FROM ".TB_PREF."sales_orders
        WHERE trans_type = ".ST_SALESQUOTE."
            AND ord_date >= ".db_escape($date_from)."
            AND ord_date <= ".db_escape($date_to);
    $result = db_query($sql_total, 'Cannot count quotes');
    $row = db_fetch_row($result);
    $total_quotes = (int)$row[0];

    // Quotes that were converted to SO (have a matching SO with the same reference pattern)
    $sql_converted = "SELECT COUNT(DISTINCT q.order_no)
        FROM ".TB_PREF."sales_orders q
        INNER JOIN ".TB_PREF."sales_orders so
            ON so.debtor_no = q.debtor_no
            AND so.trans_type = ".ST_SALESORDER."
            AND so.reference LIKE CONCAT('%', q.reference, '%')
        WHERE q.trans_type = ".ST_SALESQUOTE."
            AND q.ord_date >= ".db_escape($date_from)."
            AND q.ord_date <= ".db_escape($date_to);
    $result = db_query($sql_converted, 'Cannot count converted quotes');
    $row = db_fetch_row($result);
    $converted = (int)$row[0];

    return array(
        'total_quotes' => $total_quotes,
        'converted'    => $converted,
        'rate'         => $total_quotes > 0 ? round(($converted / $total_quotes) * 100, 1) : 0,
    );
}

// -------------------------------------------------------------------------
// Customer Analysis
// -------------------------------------------------------------------------

/**
 * Get top customers by revenue for a date range.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @param  int    $top_n     Number of top customers.
 * @return array
 */
function get_top_customers($date_from, $date_to, $top_n = 10)
{
    $sql = "SELECT trans.debtor_no, cust.name AS customer_name,
                COALESCE(SUM(trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount), 0) AS total_revenue,
                COUNT(*) AS order_count
            FROM ".TB_PREF."debtor_trans trans
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            INNER JOIN ".TB_PREF."debtors_master cust ON cust.debtor_no = trans.debtor_no
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND trans.tran_date >= ".db_escape($date_from)."
                AND trans.tran_date <= ".db_escape($date_to)."
            GROUP BY trans.debtor_no, cust.name
            ORDER BY total_revenue DESC
            LIMIT ".((int)$top_n);
    $result = db_query($sql, 'Cannot get top customers');

    $customers = array();
    while ($row = db_fetch($result)) {
        $customers[] = array(
            'debtor_no'   => (int)$row['debtor_no'],
            'name'        => $row['customer_name'],
            'revenue'     => (float)$row['total_revenue'],
            'order_count' => (int)$row['order_count'],
        );
    }
    return $customers;
}

/**
 * Get customer lifetime value.
 *
 * @param  int   $debtor_no
 * @return float
 */
function get_customer_lifetime_value($debtor_no)
{
    $sql = "SELECT COALESCE(SUM(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount), 0)
            FROM ".TB_PREF."debtor_trans trans
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND trans.debtor_no = ".db_escape((int)$debtor_no);
    $result = db_query($sql, 'Cannot get customer LTV');
    $row = db_fetch_row($result);
    return (float)$row[0];
}

// -------------------------------------------------------------------------
// Product Analysis
// -------------------------------------------------------------------------

/**
 * Get top products by revenue for a date range.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @param  int    $top_n     Number of top products.
 * @return array
 */
function get_top_products($date_from, $date_to, $top_n = 10)
{
    $sql = "SELECT detail.stock_id,
                COALESCE(stock.description, detail.description, detail.stock_id) AS product_name,
                SUM(detail.quantity) AS total_qty,
                SUM(detail.quantity * detail.unit_price * (1 - detail.discount_percent / 100)) AS total_revenue
            FROM ".TB_PREF."debtor_trans_details detail
            INNER JOIN ".TB_PREF."debtor_trans trans
                ON trans.trans_no = detail.debtor_trans_no AND trans.type = detail.debtor_trans_type
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            LEFT JOIN ".TB_PREF."stock_master stock ON stock.stock_id = detail.stock_id
            WHERE (trans.type = ".ST_SALESINVOICE.")
                AND v.id IS NULL
                AND trans.tran_date >= ".db_escape($date_from)."
                AND trans.tran_date <= ".db_escape($date_to)."
            GROUP BY detail.stock_id, stock.description, detail.description
            ORDER BY total_revenue DESC
            LIMIT ".((int)$top_n);
    $result = db_query($sql, 'Cannot get top products');

    $products = array();
    while ($row = db_fetch($result)) {
        $products[] = array(
            'stock_id'  => $row['stock_id'],
            'name'      => $row['product_name'],
            'qty'       => (float)$row['total_qty'],
            'revenue'   => (float)$row['total_revenue'],
        );
    }
    return $products;
}

/**
 * Get product analysis including margin where possible.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @param  int    $top_n     Number of products.
 * @return array
 */
function get_product_analysis($date_from, $date_to, $top_n = 20)
{
    $sql = "SELECT detail.stock_id,
                COALESCE(stock.description, detail.stock_id) AS product_name,
                stock.material_cost + stock.labour_cost + stock.overhead_cost AS standard_cost,
                SUM(detail.quantity) AS total_qty,
                SUM(detail.quantity * detail.unit_price * (1 - detail.discount_percent / 100)) AS total_revenue,
                SUM(detail.quantity * (detail.unit_price * (1 - detail.discount_percent / 100) - COALESCE(stock.material_cost + stock.labour_cost + stock.overhead_cost, 0))) AS total_margin
            FROM ".TB_PREF."debtor_trans_details detail
            INNER JOIN ".TB_PREF."debtor_trans trans
                ON trans.trans_no = detail.debtor_trans_no AND trans.type = detail.debtor_trans_type
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            LEFT JOIN ".TB_PREF."stock_master stock ON stock.stock_id = detail.stock_id
            WHERE (trans.type = ".ST_SALESINVOICE.")
                AND v.id IS NULL
                AND trans.tran_date >= ".db_escape($date_from)."
                AND trans.tran_date <= ".db_escape($date_to)."
            GROUP BY detail.stock_id, stock.description, stock.material_cost, stock.labour_cost, stock.overhead_cost
            ORDER BY total_revenue DESC
            LIMIT ".((int)$top_n);
    $result = db_query($sql, 'Cannot get product analysis');

    $products = array();
    while ($row = db_fetch($result)) {
        $revenue = (float)$row['total_revenue'];
        $margin  = (float)$row['total_margin'];
        $products[] = array(
            'stock_id'   => $row['stock_id'],
            'name'       => $row['product_name'],
            'qty'        => (float)$row['total_qty'],
            'revenue'    => $revenue,
            'margin'     => $margin,
            'margin_pct' => $revenue > 0 ? round(($margin / $revenue) * 100, 1) : 0,
        );
    }
    return $products;
}

// -------------------------------------------------------------------------
// Salesman Performance
// -------------------------------------------------------------------------

/**
 * Get salesman performance report for a date range.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return array
 */
function get_salesman_performance_report($date_from, $date_to)
{
    $sql = "SELECT sm.salesman_code, sm.salesman_name,
                sm.provision, sm.break_pt, sm.provision2,
                COALESCE(revenue.total_revenue, 0) AS total_revenue,
                COALESCE(revenue.order_count, 0) AS order_count,
                COALESCE(revenue.total_margin, 0) AS total_margin,
                COALESCE(comm.total_commission, 0) AS total_commission
            FROM ".TB_PREF."salesman sm
            LEFT JOIN (
                SELECT so.salesman_code,
                    SUM(dt.ov_amount + dt.ov_gst + dt.ov_freight + dt.ov_freight_tax + dt.ov_discount) AS total_revenue,
                    COUNT(*) AS order_count,
                    SUM(
                        dt.ov_amount + dt.ov_gst + dt.ov_freight + dt.ov_freight_tax + dt.ov_discount
                        - COALESCE(
                            (SELECT SUM((sm2.material_cost + sm2.labour_cost + sm2.overhead_cost) * dtd.quantity)
                             FROM ".TB_PREF."debtor_trans_details dtd
                             LEFT JOIN ".TB_PREF."stock_master sm2 ON sm2.stock_id = dtd.stock_id
                             WHERE dtd.debtor_trans_no = dt.trans_no
                               AND dtd.debtor_trans_type = dt.type
                            ), 0)
                    ) AS total_margin
                FROM ".TB_PREF."debtor_trans dt
                INNER JOIN ".TB_PREF."sales_orders so ON so.order_no = dt.order_
                    AND so.trans_type = ".ST_SALESORDER."
                LEFT JOIN ".TB_PREF."voided v ON v.type = dt.type AND v.id = dt.trans_no
                WHERE dt.type = ".ST_SALESINVOICE."
                    AND v.id IS NULL
                    AND dt.tran_date >= ".db_escape($date_from)."
                    AND dt.tran_date <= ".db_escape($date_to)."
                GROUP BY so.salesman_code
            ) revenue ON revenue.salesman_code = sm.salesman_code
            LEFT JOIN (
                SELECT salesman_id,
                    SUM(commission_amount) AS total_commission
                FROM ".TB_PREF."sales_commission_entries
                WHERE trans_date >= ".db_escape($date_from)."
                    AND trans_date <= ".db_escape($date_to)."
                GROUP BY salesman_id
            ) comm ON comm.salesman_id = sm.salesman_code
            ORDER BY total_revenue DESC";
    $result = db_query($sql, 'Cannot get salesman performance');

    $salesmen = array();
    while ($row = db_fetch($result)) {
        $salesmen[] = array(
            'code'        => (int)$row['salesman_code'],
            'name'        => $row['salesman_name'],
            'revenue'     => (float)$row['total_revenue'],
            'order_count' => (int)$row['order_count'],
            'margin'      => (float)$row['total_margin'],
            'commission'  => (float)$row['total_commission'],
            'provision'   => (float)$row['provision'],
            'break_pt'    => (float)$row['break_pt'],
            'provision2'  => (float)$row['provision2'],
        );
    }
    return $salesmen;
}

// -------------------------------------------------------------------------
// Margin Analysis
// -------------------------------------------------------------------------

/**
 * Get margin analysis grouped by dimension (product, customer, or salesman).
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @param  string $group_by  'product'|'customer'|'salesman'
 * @return array
 */
function get_margin_analysis($date_from, $date_to, $group_by = 'product')
{
    if ($group_by == 'customer') {
        $select = "cust.name AS group_name";
        $group  = "cust.name";
        $join   = "INNER JOIN ".TB_PREF."debtors_master cust ON cust.debtor_no = trans.debtor_no";
    } elseif ($group_by == 'salesman') {
        $select = "COALESCE(sm.salesman_name, 'Unassigned') AS group_name";
        $group  = "sm.salesman_name";
        $join   = "INNER JOIN ".TB_PREF."sales_orders so ON so.order_no = trans.order_
                    AND so.trans_type = ".ST_SALESORDER."
                   LEFT JOIN ".TB_PREF."salesman sm ON sm.salesman_code = so.salesman_code";
    } else {
        // product (default)
        $select = "COALESCE(stock.description, detail.stock_id) AS group_name";
        $group  = "detail.stock_id, stock.description";
        $join   = "";
    }

    $detail_join = $group_by == 'product'
        ? ""
        : "INNER JOIN ".TB_PREF."debtor_trans_details detail
              ON detail.debtor_trans_no = trans.trans_no
                AND detail.debtor_trans_type = trans.type";

    $sql = "SELECT $select,
                COALESCE(SUM(trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount), 0) AS revenue,
                COALESCE(SUM(
                    ".($group_by == 'product' ? "
                    detail.quantity * COALESCE(stock.material_cost + stock.labour_cost + stock.overhead_cost, 0)
                    " : "
                    (SELECT SUM(dtd.quantity * COALESCE(sm3.material_cost + sm3.labour_cost + sm3.overhead_cost, 0))
                     FROM ".TB_PREF."debtor_trans_details dtd
                     LEFT JOIN ".TB_PREF."stock_master sm3 ON sm3.stock_id = dtd.stock_id
                     WHERE dtd.debtor_trans_no = trans.trans_no
                       AND dtd.debtor_trans_type = trans.type)
                    ")."
                ), 0) AS cost
            FROM ".TB_PREF."debtor_trans trans
            $detail_join
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            ".($group_by == 'product'
                ? "LEFT JOIN ".TB_PREF."debtor_trans_details detail
                      ON detail.debtor_trans_no = trans.trans_no
                        AND detail.debtor_trans_type = trans.type
                   LEFT JOIN ".TB_PREF."stock_master stock ON stock.stock_id = detail.stock_id"
                : ""
              )."
            $join
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND trans.tran_date >= ".db_escape($date_from)."
                AND trans.tran_date <= ".db_escape($date_to)."
            GROUP BY $group
            ORDER BY revenue DESC";
    $result = db_query($sql, 'Cannot get margin analysis');

    $data = array();
    while ($row = db_fetch($result)) {
        $revenue = (float)$row['revenue'];
        $cost    = (float)(isset($row['cost']) ? $row['cost'] : 0);
        $margin  = $revenue - $cost;
        $data[] = array(
            'group_name' => $row['group_name'],
            'revenue'    => $revenue,
            'cost'       => $cost,
            'margin'     => $margin,
            'margin_pct' => $revenue > 0 ? round(($margin / $revenue) * 100, 1) : 0,
        );
    }
    return $data;
}

// -------------------------------------------------------------------------
// Discount Effectiveness
// -------------------------------------------------------------------------

/**
 * Get discount effectiveness report.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return array              [['program'=>..., 'usage'=>..., 'total_discount'=>..., 'revenue'=>...], ...]
 */
function get_discount_effectiveness_report($date_from, $date_to)
{
    // Check if discount tables exist (Phase 4)
    $has_discounts = check_table_exists(TB_PREF.'sales_discount_programs')
        && check_table_exists(TB_PREF.'sales_discount_usage');

    if (!$has_discounts) {
        return array();
    }

    $sql = "SELECT prog.id AS program_id, prog.name AS program_name,
                prog.program_type,
                COUNT(usage.id) AS usage_count,
                COALESCE(SUM(usage.discount_amount), 0) AS total_discount
            FROM ".TB_PREF."sales_discount_programs prog
            LEFT JOIN ".TB_PREF."sales_discount_usage usage
                ON usage.program_id = prog.id
                AND usage.applied_date >= CONCAT(".db_escape($date_from).", ' 00:00:00')
                AND usage.applied_date <= CONCAT(".db_escape($date_to).", ' 23:59:59')
            GROUP BY prog.id, prog.name, prog.program_type
            ORDER BY total_discount DESC";
    $result = db_query($sql, 'Cannot get discount effectiveness');

    $data = array();
    while ($row = db_fetch($result)) {
        $data[] = array(
            'program_id'     => (int)$row['program_id'],
            'program_name'   => $row['program_name'],
            'program_type'   => $row['program_type'],
            'usage_count'    => (int)$row['usage_count'],
            'total_discount' => (float)$row['total_discount'],
        );
    }
    return $data;
}

/**
 * Get sales price variance report — compare actual selling prices vs standard prices.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return array
 */
function get_sales_price_variance_report($date_from, $date_to)
{
    $sql = "SELECT detail.stock_id,
                COALESCE(stock.description, detail.stock_id) AS product_name,
                SUM(detail.quantity) AS total_qty,
                AVG(detail.unit_price * (1 - detail.discount_percent / 100)) AS avg_sell_price,
                COALESCE(prices.price, 0) AS standard_price
            FROM ".TB_PREF."debtor_trans_details detail
            INNER JOIN ".TB_PREF."debtor_trans trans
                ON trans.trans_no = detail.debtor_trans_no AND trans.type = detail.debtor_trans_type
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            LEFT JOIN ".TB_PREF."stock_master stock ON stock.stock_id = detail.stock_id
            LEFT JOIN ".TB_PREF."prices prices
                ON prices.stock_id = detail.stock_id
                AND prices.sales_type_id = 0
                AND prices.curr_abrev = trans.tran_date  -- fallback
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND trans.tran_date >= ".db_escape($date_from)."
                AND trans.tran_date <= ".db_escape($date_to)."
            GROUP BY detail.stock_id, stock.description, prices.price
            ORDER BY total_qty DESC
            LIMIT 100";
    $result = db_query($sql, 'Cannot get price variance');

    $data = array();
    while ($row = db_fetch($result)) {
        $avg_sell = (float)$row['avg_sell_price'];
        $std      = (float)$row['standard_price'];
        $variance = $std > 0 ? round((($avg_sell - $std) / $std) * 100, 1) : 0;
        $data[] = array(
            'stock_id'       => $row['stock_id'],
            'product_name'   => $row['product_name'],
            'total_qty'      => (float)$row['total_qty'],
            'avg_sell_price' => $avg_sell,
            'standard_price' => $std,
            'variance_pct'   => $variance,
        );
    }
    return $data;
}

// -------------------------------------------------------------------------
// Overdue / Aging Summary
// -------------------------------------------------------------------------

/**
 * Get overdue invoice aging summary.
 *
 * @return array ['current'=>float, '1_30'=>float, '31_60'=>float, '61_90'=>float, '90_plus'=>float]
 */
function get_aging_summary()
{
    $today = date2sql(Today());
    $sql = "SELECT
                SUM(CASE WHEN due_date >= ".db_escape($today)."
                    THEN ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc
                    ELSE 0 END) AS current_due,
                SUM(CASE WHEN due_date < ".db_escape($today)."
                    AND due_date >= DATE_SUB(" . db_escape($today) . ", INTERVAL 30 DAY)
                    THEN ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc
                    ELSE 0 END) AS due_1_30,
                SUM(CASE WHEN due_date < DATE_SUB(" . db_escape($today) . ", INTERVAL 30 DAY)
                    AND due_date >= DATE_SUB(" . db_escape($today) . ", INTERVAL 60 DAY)
                    THEN ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc
                    ELSE 0 END) AS due_31_60,
                SUM(CASE WHEN due_date < DATE_SUB(" . db_escape($today) . ", INTERVAL 60 DAY)
                    AND due_date >= DATE_SUB(" . db_escape($today) . ", INTERVAL 90 DAY)
                    THEN ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc
                    ELSE 0 END) AS due_61_90,
                SUM(CASE WHEN due_date < DATE_SUB(" . db_escape($today) . ", INTERVAL 90 DAY)
                    THEN ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc
                    ELSE 0 END) AS due_90_plus
            FROM ".TB_PREF."debtor_trans trans
            LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
            WHERE trans.type = ".ST_SALESINVOICE."
                AND v.id IS NULL
                AND (ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc) > ".FLOAT_COMP_DELTA;
    $result = db_query($sql, 'Cannot get aging summary');
    $row = db_fetch($result);

    if (!$row)
        return array('current'=>0, '1_30'=>0, '31_60'=>0, '61_90'=>0, '90_plus'=>0);

    return array(
        'current' => (float)$row['current_due'],
        '1_30'    => (float)$row['due_1_30'],
        '31_60'   => (float)$row['due_31_60'],
        '61_90'   => (float)$row['due_61_90'],
        '90_plus' => (float)$row['due_90_plus'],
    );
}

// -------------------------------------------------------------------------
// Pipeline Value (from CRM)
// -------------------------------------------------------------------------

/**
 * Get pipeline value summary from CRM opportunities.
 * Only works when use_crm=1 and CRM tables exist.
 *
 * @return array ['stages'=>[...], 'total_value'=>float]
 */
function get_pipeline_summary()
{
    if (!check_table_exists(TB_PREF.'crm_opportunities')) {
        return array('stages' => array(), 'total_value' => 0);
    }

    $sql = "SELECT op.stage, COALESCE(SUM(op.expected_value), 0) AS total_value,
                COUNT(*) AS opp_count
            FROM ".TB_PREF."crm_opportunities op
            WHERE op.status NOT IN ('won', 'lost')
            GROUP BY op.stage
            ORDER BY FIELD(op.stage, 'draft', 'sent', 'confirmed', 'negotiation', 'qualified', 'proposal')";
    $result = db_query($sql, 'Cannot get pipeline summary');

    $stages = array();
    $total  = 0;
    while ($row = db_fetch($result)) {
        $value = (float)$row['total_value'];
        $stages[] = array(
            'stage' => $row['stage'],
            'value' => $value,
            'count' => (int)$row['opp_count'],
        );
        $total += $value;
    }
    return array('stages' => $stages, 'total_value' => $total);
}

// -------------------------------------------------------------------------
// Return Rate
// -------------------------------------------------------------------------

/**
 * Get return rate for a date range.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return array              ['total_invoiced'=>float, 'total_returned'=>float, 'rate'=>float]
 */
function get_return_rate($date_from, $date_to)
{
    $sql_inv = "SELECT COALESCE(SUM(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount), 0)
                FROM ".TB_PREF."debtor_trans trans
                LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
                WHERE trans.type = ".ST_SALESINVOICE."
                    AND v.id IS NULL
                    AND trans.tran_date >= ".db_escape($date_from)."
                    AND trans.tran_date <= ".db_escape($date_to);
    $result = db_query($sql_inv, 'Cannot get invoiced total');
    $row = db_fetch_row($result);
    $invoiced = (float)$row[0];

    $sql_ret = "SELECT COALESCE(SUM(ABS(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount)), 0)
                FROM ".TB_PREF."debtor_trans trans
                LEFT JOIN ".TB_PREF."voided v ON v.type = trans.type AND v.id = trans.trans_no
                WHERE trans.type = ".ST_CUSTCREDIT."
                    AND v.id IS NULL
                    AND trans.tran_date >= ".db_escape($date_from)."
                    AND trans.tran_date <= ".db_escape($date_to);
    $result = db_query($sql_ret, 'Cannot get returned total');
    $row = db_fetch_row($result);
    $returned = (float)$row[0];

    return array(
        'total_invoiced' => $invoiced,
        'total_returned' => $returned,
        'rate'           => $invoiced > 0 ? round(($returned / $invoiced) * 100, 2) : 0,
    );
}

// -------------------------------------------------------------------------
// Commission Payable Total
// -------------------------------------------------------------------------

/**
 * Get total commission payable (calculated but not yet paid).
 *
 * @return float
 */
function get_commission_payable()
{
    if (!check_table_exists(TB_PREF.'sales_commission_entries'))
        return 0;

    $sql = "SELECT COALESCE(SUM(commission_amount), 0)
            FROM ".TB_PREF."sales_commission_entries
            WHERE status IN ('calculated', 'approved')";
    $result = db_query($sql, 'Cannot get commission payable');
    $row = db_fetch_row($result);
    return (float)$row[0];
}

// -------------------------------------------------------------------------
// Dashboard Data Aggregator
// -------------------------------------------------------------------------

/**
 * Get comprehensive sales dashboard data.
 *
 * @param  string $date_from SQL date.
 * @param  string $date_to   SQL date.
 * @return array
 */
function get_sales_dashboard_data($date_from, $date_to)
{
    $today     = date2sql(Today());
    $prev_from = date('Y-m-d', strtotime($today . ' -60 days'));
    $prev_to   = date('Y-m-d', strtotime($today . ' -30 days'));

    $current_revenue  = get_total_revenue($date_from, $date_to);
    $previous_revenue = get_total_revenue($prev_from, $prev_to);
    $revenue_change   = $previous_revenue > 0
        ? round((($current_revenue - $previous_revenue) / $previous_revenue) * 100, 1)
        : 0;

    $order_count    = get_order_count($date_from, $date_to);
    $avg_order      = get_avg_order_value($date_from, $date_to);
    $conversion     = get_quote_conversion_rate($date_from, $date_to);
    $top_customers  = get_top_customers($date_from, $date_to, 5);
    $top_products   = get_top_products($date_from, $date_to, 5);
    $aging          = get_aging_summary();
    $pipeline       = get_pipeline_summary();
    $return_rate    = get_return_rate($date_from, $date_to);
    $commission_pay = get_commission_payable();
    $revenue_trend  = get_revenue_trend('monthly', 6);
    $total_overdue  = $aging['1_30'] + $aging['31_60'] + $aging['61_90'] + $aging['90_plus'];

    return array(
        'current_revenue'      => $current_revenue,
        'previous_revenue'     => $previous_revenue,
        'revenue_change_pct'   => $revenue_change,
        'order_count'          => $order_count,
        'avg_order_value'      => $avg_order,
        'conversion_rate'      => $conversion['rate'],
        'top_customers'        => $top_customers,
        'top_products'         => $top_products,
        'aging'                => $aging,
        'total_overdue'        => $total_overdue,
        'pipeline'             => $pipeline,
        'return_rate'          => $return_rate['rate'],
        'commission_payable'   => $commission_pay,
        'revenue_trend'        => $revenue_trend,
    );
}

// -------------------------------------------------------------------------
// Sales Forecast (simple trend-based)
// -------------------------------------------------------------------------

/**
 * Simple trend-based sales forecast for N months ahead.
 *
 * @param  int   $months_ahead Number of months to forecast.
 * @return array               [['period'=>..., 'forecast'=>...], ...]
 */
function get_sales_forecast($months_ahead = 3)
{
    $trend = get_revenue_trend('monthly', 12);
    $count = count($trend);

    if ($count < 3) {
        // Not enough data, return flat forecast
        $avg = $count > 0 ? $trend[$count - 1]['revenue'] : 0;
        $forecast = array();
        for ($i = 1; $i <= $months_ahead; $i++) {
            $period = date('Y-m', strtotime("+{$i} months"));
            $forecast[] = array('period' => $period, 'forecast' => $avg);
        }
        return $forecast;
    }

    // Simple linear regression on last N months
    $n = min($count, 6);
    $recent = array_slice($trend, -$n);
    $sum_x = $sum_y = $sum_xy = $sum_x2 = 0;

    for ($i = 0; $i < $n; $i++) {
        $x = $i;  // 0-based index
        $y = $recent[$i]['revenue'];
        $sum_x  += $x;
        $sum_y  += $y;
        $sum_xy += $x * $y;
        $sum_x2 += $x * $x;
    }

    $denom = $n * $sum_x2 - $sum_x * $sum_x;
    $slope = $denom != 0 ? ($n * $sum_xy - $sum_x * $sum_y) / $denom : 0;
    $intercept = $n > 0 ? ($sum_y - $slope * $sum_x) / $n : 0;

    $forecast = array();
    for ($i = 1; $i <= $months_ahead; $i++) {
        $period   = date('Y-m', strtotime("+{$i} months"));
        $predicted = $intercept + $slope * ($n + $i - 1);
        if ($predicted < 0)
            $predicted = 0;
        $forecast[] = array('period' => $period, 'forecast' => round($predicted, 2));
    }

    return $forecast;
}

// -------------------------------------------------------------------------
// KPI Cache Refresh (for scheduled jobs)
// -------------------------------------------------------------------------

/**
 * Refresh materialized KPI cache for current period.
 *
 * Call this from a scheduled task or cron job to keep dashboard fast.
 */
function refresh_kpi_cache()
{
    $today  = date2sql(Today());
    $month  = date('Y-m');
    $prev_m = date('Y-m', strtotime('-1 month'));

    // Monthly revenue
    $month_start = date('Y-m-01');
    $month_end   = date('Y-m-t');
    $revenue = get_total_revenue($month_start, $month_end);
    $orders  = get_order_count($month_start, $month_end);
    set_kpi_cache('monthly_revenue', $month, '', $revenue, $orders);

    // Conversion rate
    $conv = get_quote_conversion_rate($month_start, $month_end);
    set_kpi_cache('conversion_rate', $month, '', $conv['rate'], $conv['converted'], $conv['total_quotes']);

    // Avg order value
    $aov = get_avg_order_value($month_start, $month_end);
    set_kpi_cache('avg_order_value', $month, '', $aov);

    // Aging summary
    $aging = get_aging_summary();
    set_kpi_cache('aging_summary', $month, '', $aging['1_30'] + $aging['31_60'] + $aging['61_90'] + $aging['90_plus'],
        $aging['current']);

    // Commission payable
    $comm = get_commission_payable();
    set_kpi_cache('commission_total', $month, '', $comm);

    // Top customers (cache as JSON detail)
    $top_cust = get_top_customers($month_start, $month_end, 10);
    set_kpi_cache('top_customers', $month, '', count($top_cust), 0, 0, $top_cust);

    // Top products (cache as JSON detail)
    $top_prod = get_top_products($month_start, $month_end, 10);
    set_kpi_cache('top_products', $month, '', count($top_prod), 0, 0, $top_prod);
}

// -------------------------------------------------------------------------
// Helper — check if a table exists
// -------------------------------------------------------------------------

/**
 * Check if a table exists in the current database.
 *
 * @param  string $table_name Table name including TB_PREF prefix.
 * @return bool
 */
function check_table_exists($table_name)
{
    $sql = "SHOW TABLES LIKE ".db_escape($table_name);
    $result = db_query($sql, 'Cannot check table existence');
    return db_num_rows($result) > 0;
}
