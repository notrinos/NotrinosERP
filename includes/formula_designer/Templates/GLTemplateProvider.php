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
 * GLTemplateProvider — built-in formula templates for the General Ledger module.
 *
 * @package FormulaDesigner\Templates
 * @since   2.0.0
 */
class FormulaDesigner_Templates_GLTemplateProvider
    implements FormulaDesigner_Contracts_DesignerTemplateInterface
{
    /** @var array */
    private $data = array();

    /**
     * @param array $data
     */
    public function __construct(array $data = array())
    {
        $this->data = $data;
    }

    /** @return string */
    public function getId()
    {
        return isset($this->data['id']) ? (string)$this->data['id'] : '';
    }

    /** @return string */
    public function getFormula()
    {
        return isset($this->data['formula']) ? (string)$this->data['formula'] : '';
    }

    /** @return array */
    public function getMetadata()
    {
        return array(
            'id'          => $this->getId(),
            'module'      => isset($this->data['module']) ? (string)$this->data['module'] : 'gl',
            'label'       => isset($this->data['label']) ? (string)$this->data['label'] : '',
            'description' => isset($this->data['description']) ? (string)$this->data['description'] : '',
            'formula'     => $this->getFormula(),
            'category'    => isset($this->data['category']) ? (string)$this->data['category'] : 'Accounting',
            'tags'        => isset($this->data['tags']) && is_array($this->data['tags']) ? $this->data['tags'] : array(),
            'difficulty'  => isset($this->data['difficulty']) ? (string)$this->data['difficulty'] : 'beginner',
        );
    }

    /**
     * Return all 5 GL formula templates.
     *
     * @return FormulaDesigner_Templates_GLTemplateProvider[]
     */
    public static function all()
    {
        return array(
            // 1. Account Balance
            new self(array(
                'id'          => 'gl.account_balance',
                'module'      => 'gl',
                'label'       => 'Account Balance',
                'description' => 'Calculate current account balance from debits and credits.',
                'formula'     => 'Account.OpeningBalance + Ledger.Debit - Ledger.Credit',
                'category'    => 'Reporting',
                'tags'        => array('balance', 'account', 'debit', 'credit'),
                'difficulty'  => 'beginner',
            )),

            // 2. Budget Variance (Amount)
            new self(array(
                'id'          => 'gl.budget_variance_amount',
                'module'      => 'gl',
                'label'       => 'Budget Variance (Amount)',
                'description' => 'Calculate the absolute variance between budget and actual.',
                'formula'     => 'Budget.Amount - Ledger.ActualAmount',
                'category'    => 'Budget',
                'tags'        => array('budget', 'variance', 'actual', 'amount'),
                'difficulty'  => 'beginner',
            )),

            // 3. Budget Variance (Percentage)
            new self(array(
                'id'          => 'gl.budget_variance_pct',
                'module'      => 'gl',
                'label'       => 'Budget Variance (Percentage)',
                'description' => 'Calculate budget variance as a percentage of budget.',
                'formula'     => 'ROUND((Budget.Amount - Ledger.ActualAmount) / ABS(Budget.Amount) * 100, 2)',
                'category'    => 'Budget',
                'tags'        => array('budget', 'variance', 'percentage'),
                'difficulty'  => 'intermediate',
            )),

            // 4. Current Ratio
            new self(array(
                'id'          => 'gl.current_ratio',
                'module'      => 'gl',
                'label'       => 'Current Ratio',
                'description' => 'Liquidity ratio: current assets / current liabilities.',
                'formula'     => 'ROUND(Ledger.CurrentAssets / Ledger.CurrentLiabilities, 2)',
                'category'    => 'Ratios',
                'tags'        => array('current', 'ratio', 'liquidity', 'assets', 'liabilities'),
                'difficulty'  => 'intermediate',
            )),

            // 5. Net Profit Margin
            new self(array(
                'id'          => 'gl.net_profit_margin',
                'module'      => 'gl',
                'label'       => 'Net Profit Margin',
                'description' => 'Calculate net profit as a percentage of revenue.',
                'formula'     => 'ROUND((Ledger.Revenue - Ledger.Expenses) / ABS(Ledger.Revenue) * 100, 2)',
                'category'    => 'Ratios',
                'tags'        => array('profit', 'margin', 'net', 'revenue'),
                'difficulty'  => 'intermediate',
            )),
        );
    }
}
