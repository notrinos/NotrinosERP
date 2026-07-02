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
 * InventoryTemplateProvider — built-in formula templates for the Inventory module.
 *
 * @package FormulaDesigner\Templates
 * @since   2.0.0
 */
class FormulaDesigner_Templates_InventoryTemplateProvider
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
            'module'      => isset($this->data['module']) ? (string)$this->data['module'] : 'inventory',
            'label'       => isset($this->data['label']) ? (string)$this->data['label'] : '',
            'description' => isset($this->data['description']) ? (string)$this->data['description'] : '',
            'formula'     => $this->getFormula(),
            'category'    => isset($this->data['category']) ? (string)$this->data['category'] : 'Inventory',
            'tags'        => isset($this->data['tags']) && is_array($this->data['tags']) ? $this->data['tags'] : array(),
            'difficulty'  => isset($this->data['difficulty']) ? (string)$this->data['difficulty'] : 'beginner',
        );
    }

    /**
     * Return all 6 inventory/costing formula templates.
     *
     * @return FormulaDesigner_Templates_InventoryTemplateProvider[]
     */
    public static function all()
    {
        return array(
            // 1. Moving Average Cost
            new self(array(
                'id'          => 'inventory.moving_avg_cost',
                'module'      => 'inventory',
                'label'       => 'Moving Average Cost',
                'description' => 'Calculate moving average cost after receiving new stock.',
                'formula'     => 'ROUND((Stock.QtyOnHand * Stock.AvgCost + Receipt.Qty * Receipt.UnitCost) / (Stock.QtyOnHand + Receipt.Qty), 2)',
                'category'    => 'Costing',
                'tags'        => array('average', 'cost', 'moving', 'stock'),
                'difficulty'  => 'intermediate',
            )),

            // 2. Reorder Level Alert
            new self(array(
                'id'          => 'inventory.reorder_alert',
                'module'      => 'inventory',
                'label'       => 'Reorder Point Quantity',
                'description' => 'Suggested reorder quantity based on lead time and daily demand.',
                'formula'     => 'ROUND(Item.DailyDemand * Item.LeadTimeDays + Stock.ReorderLevel, 0)',
                'category'    => 'Planning',
                'tags'        => array('reorder', 'alert', 'lead-time', 'demand'),
                'difficulty'  => 'beginner',
            )),

            // 3. Landed Cost
            new self(array(
                'id'          => 'inventory.landed_cost',
                'module'      => 'inventory',
                'label'       => 'Landed Cost Per Unit',
                'description' => 'Calculate total landed cost including freight and customs.',
                'formula'     => 'ROUND(Item.Cost + Item.Freight / Item.Quantity + Item.CustomsDuty / Item.Quantity, 2)',
                'category'    => 'Costing',
                'tags'        => array('landed', 'cost', 'freight', 'customs'),
                'difficulty'  => 'intermediate',
            )),

            // 4. Stock Valuation
            new self(array(
                'id'          => 'inventory.stock_valuation',
                'module'      => 'inventory',
                'label'       => 'Stock Valuation (Avg Cost)',
                'description' => 'Total stock value = quantity × average cost.',
                'formula'     => 'ROUND(Stock.QtyOnHand * Stock.AvgCost, 2)',
                'category'    => 'Valuation',
                'tags'        => array('valuation', 'stock', 'average', 'cost'),
                'difficulty'  => 'beginner',
            )),

            // 5. Economic Order Quantity
            new self(array(
                'id'          => 'inventory.eoq',
                'module'      => 'inventory',
                'label'       => 'Economic Order Quantity (EOQ)',
                'description' => 'Calculate optimal order quantity to minimize total inventory costs.',
                'formula'     => 'ROUND(SQRT(2 * Item.AnnualDemand * Item.OrderCost / Item.HoldingCost), 0)',
                'category'    => 'Planning',
                'tags'        => array('eoq', 'optimal', 'order', 'quantity'),
                'difficulty'  => 'advanced',
            )),

            // 6. Stock Turnover Ratio
            new self(array(
                'id'          => 'inventory.stock_turnover',
                'module'      => 'inventory',
                'label'       => 'Stock Turnover Ratio',
                'description' => 'Measure how quickly inventory is sold and replaced.',
                'formula'     => 'ROUND(Item.COGS / ((Stock.OpeningQty + Stock.QtyOnHand) / 2), 2)',
                'category'    => 'Valuation',
                'tags'        => array('turnover', 'ratio', 'cogs', 'efficiency'),
                'difficulty'  => 'intermediate',
            )),
        );
    }
}
