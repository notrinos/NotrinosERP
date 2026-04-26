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
 * Notification provider for sales-focused alerts.
 *
 * Provides pending quote approvals, expiring quotes, overdue invoices,
 * and expiring agreements counts for the topbar notification bell.
 */
class SalesNotificationProvider implements NotificationProvider
{
    /** @var int */
    private $user_id;

    /** @var int */
    private $role_id;

    /** @var int|null */
    private $count_cache = null;

    /**
     * Constructor.
     *
     * @param int $user_id
     * @param int $role_id
     */
    public function __construct($user_id, $role_id)
    {
        $this->user_id = (int)$user_id;
        $this->role_id = (int)$role_id;
    }

    /**
     * {@inheritdoc}
     */
    public function getSectionId()
    {
        return 'sales';
    }

    /**
     * {@inheritdoc}
     */
    public function getSectionLabel()
    {
        return _('Sales Alerts');
    }

    /**
     * {@inheritdoc}
     */
    public function getSectionIcon()
    {
        return 'trending-up';
    }

    /**
     * {@inheritdoc}
     */
    public function getCount()
    {
        if ($this->count_cache !== null)
            return $this->count_cache;

        $stats = $this->get_sales_alert_counts();
        $this->count_cache = array_sum($stats);
        return $this->count_cache;
    }

    /**
     * {@inheritdoc}
     */
    public function getItems($limit)
    {
        global $path_to_root;

        $stats = $this->get_sales_alert_counts();
        $items = array(
            array(
                'id'      => 'sales-quote-approvals',
                'title'   => _('Pending Quote Approvals'),
                'summary' => sprintf(_('%d quotations awaiting approval'), (int)$stats['pending_quote_approvals']),
                'time'    => '',
                'url'     => $path_to_root . '/modules/approval/dashboard.php',
                'type'    => 'pending',
                'is_read' => false,
            ),
            array(
                'id'      => 'sales-expiring-quotes',
                'title'   => _('Expiring Quotations'),
                'summary' => sprintf(_('%d quotations expiring in next 14 days'), (int)$stats['expiring_quotes']),
                'time'    => '',
                'url'     => $path_to_root . '/sales/inquiry/sales_orders_view.php?type=32',
                'type'    => 'warning',
                'is_read' => false,
            ),
            array(
                'id'      => 'sales-overdue-invoices',
                'title'   => _('Overdue Invoices'),
                'summary' => sprintf(_('%d customer invoices are overdue'), (int)$stats['overdue_invoices']),
                'time'    => '',
                'url'     => $path_to_root . '/sales/inquiry/customer_inquiry.php',
                'type'    => 'danger',
                'is_read' => false,
            ),
            array(
                'id'      => 'sales-agreements-expiring',
                'title'   => _('Expiring Sales Agreements'),
                'summary' => sprintf(_('%d agreements expire in next 30 days'), (int)$stats['agreements_expiring']),
                'time'    => '',
                'url'     => $path_to_root . '/sales/inquiry/sales_agreements_view.php',
                'type'    => 'warning',
                'is_read' => false,
            ),
            array(
                'id'      => 'sales-commission-pending',
                'title'   => _('Unpaid Commissions'),
                'summary' => sprintf(_('%d commission entries pending payment'), (int)$stats['commission_pending']),
                'time'    => '',
                'url'     => $path_to_root . '/sales/inquiry/commission_inquiry.php',
                'type'    => 'pending',
                'is_read' => false,
            ),
        );

        $filtered = array();
        foreach ($items as $item) {
            if (preg_match('/(\d+)/', $item['summary'], $matches) && (int)$matches[1] <= 0)
                continue;
            $filtered[] = $item;
        }

        if ((int)$limit > 0)
            $filtered = array_slice($filtered, 0, (int)$limit);

        return $filtered;
    }

    /**
     * {@inheritdoc}
     */
    public function getViewAllUrl()
    {
        global $path_to_root;
        return $path_to_root . '/sales/dashboard/sales_dashboard.php';
    }

    /**
     * Check table existence.
     *
     * @param  string $table_name
     * @return bool
     */
    private function table_exists($table_name)
    {
        $sql = "SHOW TABLES LIKE " . db_escape($table_name);
        $result = db_query($sql, 'could not check sales notification table');
        return db_num_rows($result) > 0;
    }

    /**
     * Check if a column exists in a table.
     *
     * @param  string $table_name  Table name including TB_PREF prefix.
     * @param  string $column_name Column name.
     * @return bool
     */
    private function column_exists($table_name, $column_name)
    {
        $sql = "SHOW COLUMNS FROM `" . str_replace('`', '', $table_name) . "` LIKE " . db_escape($column_name);
        $result = @db_query($sql, 'could not check column existence');
        return ($result && db_num_rows($result) > 0);
    }

    /**
     * Count sales alert categories.
     *
     * @return array
     */
    private function get_sales_alert_counts()
    {
        $today            = date2sql(Today());
        $fourteen_days    = date('Y-m-d', strtotime($today . ' +14 days'));
        $thirty_days      = date('Y-m-d', strtotime($today . ' +30 days'));

        $counts = array(
            'pending_quote_approvals' => 0,
            'expiring_quotes'         => 0,
            'overdue_invoices'        => 0,
            'agreements_expiring'     => 0,
            'commission_pending'      => 0,
        );

        // Pending quote approvals
        if ($this->table_exists(TB_PREF . 'approval_drafts') && $this->table_exists(TB_PREF . 'approval_levels')) {
            $sql = "SELECT COUNT(DISTINCT d.id)
                FROM " . TB_PREF . "approval_drafts d
                INNER JOIN " . TB_PREF . "approval_levels al
                    ON al.workflow_id = d.workflow_id
                    AND al.level = d.current_level
                    AND al.role_id = " . db_escape($this->role_id) . "
                    AND al.is_active = 1
                WHERE d.status = 0
                    AND d.trans_type IN (" . ST_SALESQUOTE . ", " . ST_SALESORDER . ")";
            $counts['pending_quote_approvals'] = (int)$this->scalar($sql, 'could not count pending quote approvals');
        }

        // Expiring quotations — only if validity_date column exists (Phase 2)
        if ($this->column_exists(TB_PREF . 'sales_orders', 'validity_date')) {
            $sql = "SELECT COUNT(*)
                FROM " . TB_PREF . "sales_orders
                WHERE trans_type = " . ST_SALESQUOTE . "
                    AND validity_date IS NOT NULL
                    AND validity_date >= " . db_escape($today) . "
                    AND validity_date <= " . db_escape($fourteen_days);
            $counts['expiring_quotes'] = (int)$this->scalar($sql, 'could not count expiring quotes');
        }

        // Overdue invoices
        $sql = "SELECT COUNT(*)
            FROM " . TB_PREF . "debtor_trans trans
            LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
            WHERE trans.type = " . ST_SALESINVOICE . "
                AND v.id IS NULL
                AND trans.due_date < " . db_escape($today) . "
                AND (trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount - trans.alloc) > " . FLOAT_COMP_DELTA;
        $counts['overdue_invoices'] = (int)$this->scalar($sql, 'could not count overdue invoices');

        // Expiring agreements
        if ($this->table_exists(TB_PREF . 'sales_agreements')) {
            $sql = "SELECT COUNT(*)
                FROM " . TB_PREF . "sales_agreements
                WHERE inactive = 0
                    AND status IN ('active')
                    AND date_end IS NOT NULL
                    AND date_end >= " . db_escape($today) . "
                    AND date_end <= " . db_escape($thirty_days);
            $counts['agreements_expiring'] = (int)$this->scalar($sql, 'could not count expiring agreements');
        }

        // Unpaid commissions
        if ($this->table_exists(TB_PREF . 'sales_commission_entries')) {
            $sql = "SELECT COUNT(*)
                FROM " . TB_PREF . "sales_commission_entries
                WHERE status IN ('calculated', 'approved')";
            $counts['commission_pending'] = (int)$this->scalar($sql, 'could not count pending commissions');
        }

        return $counts;
    }

    /**
     * Execute scalar query helper.
     *
     * @param  string $sql
     * @param  string $error_message
     * @return float
     */
    private function scalar($sql, $error_message)
    {
        $result = db_query($sql, $error_message);
        $row = db_fetch_row($result);
        if (!$row)
            return 0;
        return (float)$row[0];
    }
}
