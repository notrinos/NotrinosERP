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
 * PricingTemplateProvider — built-in formula templates for the Sales/Pricing module.
 *
 * @package FormulaDesigner\Templates
 * @since   2.0.0
 */
class FormulaDesigner_Templates_PricingTemplateProvider
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
            'module'      => isset($this->data['module']) ? (string)$this->data['module'] : 'sales',
            'label'       => isset($this->data['label']) ? (string)$this->data['label'] : '',
            'description' => isset($this->data['description']) ? (string)$this->data['description'] : '',
            'formula'     => $this->getFormula(),
            'category'    => isset($this->data['category']) ? (string)$this->data['category'] : 'Pricing',
            'tags'        => isset($this->data['tags']) && is_array($this->data['tags']) ? $this->data['tags'] : array(),
            'difficulty'  => isset($this->data['difficulty']) ? (string)$this->data['difficulty'] : 'beginner',
        );
    }

    /**
     * Return all 8 pricing/sales formula templates.
     *
     * @return FormulaDesigner_Templates_PricingTemplateProvider[]
     */
    public static function all()
    {
        return array(
            // 1. Simple Markup
            new self(array(
                'id'          => 'pricing.simple_markup',
                'module'      => 'sales',
                'label'       => 'Simple Markup Percentage',
                'description' => 'Sell price = cost + percentage markup.',
                'formula'     => 'ROUND(Item.Cost * (1 + 0.25), 2)',
                'category'    => 'Pricing',
                'tags'        => array('markup', 'cost', 'percentage'),
                'difficulty'  => 'beginner',
            )),

            // 2. Volume Discount
            new self(array(
                'id'          => 'pricing.volume_discount',
                'module'      => 'sales',
                'label'       => 'Volume Discount Rule',
                'description' => 'Apply tiered discount based on order quantity.',
                'formula'     => 'IF(Item.Quantity >= 100, Item.Price * 0.85, IF(Item.Quantity >= 50, Item.Price * 0.90, Item.Price))',
                'category'    => 'Discounts',
                'tags'        => array('volume', 'discount', 'tiered', 'quantity'),
                'difficulty'  => 'intermediate',
            )),

            // 3. Customer-Specific Discount
            new self(array(
                'id'          => 'pricing.customer_discount',
                'module'      => 'sales',
                'label'       => 'Customer-Specific Discount',
                'description' => 'Apply a customer-specific discount to the standard price.',
                'formula'     => 'ROUND(Item.Price * (1 - Customer.DiscountRate), 2)',
                'category'    => 'Discounts',
                'tags'        => array('customer', 'discount', 'specific'),
                'difficulty'  => 'beginner',
            )),

            // 4. Sales Commission
            new self(array(
                'id'          => 'pricing.sales_commission',
                'module'      => 'sales',
                'label'       => 'Salesperson Commission',
                'description' => 'Calculate commission as percentage of sale value.',
                'formula'     => 'ROUND(Item.Price * Item.Quantity * Salesperson.CommissionRate, 2)',
                'category'    => 'Commissions',
                'tags'        => array('commission', 'salesperson', 'percentage'),
                'difficulty'  => 'beginner',
            )),

            // 5. Promotional Discount
            new self(array(
                'id'          => 'pricing.promotional_discount',
                'module'      => 'sales',
                'label'       => 'Promotional Discount',
                'description' => 'Apply a fixed discount amount for promotional items.',
                'formula'     => 'IF(Item.IsPromo, Item.Price - 50, Item.Price)',
                'category'    => 'Discounts',
                'tags'        => array('promo', 'discount', 'fixed'),
                'difficulty'  => 'beginner',
            )),

            // 6. Tax-Inclusive Price
            new self(array(
                'id'          => 'pricing.tax_inclusive',
                'module'      => 'sales',
                'label'       => 'Tax-Inclusive Selling Price',
                'description' => 'Calculate selling price including tax from a tax-exclusive base.',
                'formula'     => 'ROUND(Item.Price * (1 + Item.TaxRate), 2)',
                'category'    => 'Pricing',
                'tags'        => array('tax', 'inclusive', 'gst', 'vat'),
                'difficulty'  => 'beginner',
            )),

            // 7. Tiered Commission
            new self(array(
                'id'          => 'pricing.tiered_commission',
                'module'      => 'sales',
                'label'       => 'Tiered Sales Commission',
                'description' => 'Higher commission rate for larger deals.',
                'formula'     => 'IF(Item.Price * Item.Quantity >= 100000, ROUND(Item.Price * Item.Quantity * 0.05, 2), ROUND(Item.Price * Item.Quantity * 0.03, 2))',
                'category'    => 'Commissions',
                'tags'        => array('tiered', 'commission', 'sales'),
                'difficulty'  => 'intermediate',
            )),

            // 8. Margin-Based Price Floor
            new self(array(
                'id'          => 'pricing.margin_price_floor',
                'module'      => 'sales',
                'label'       => 'Margin-Based Price Floor',
                'description' => 'Ensure sell price never goes below cost plus minimum margin.',
                'formula'     => 'IF(Item.Price < Item.Cost * 1.10, ROUND(Item.Cost * 1.10, 2), Item.Price)',
                'category'    => 'Pricing',
                'tags'        => array('margin', 'floor', 'minimum', 'cost'),
                'difficulty'  => 'intermediate',
            )),
        );
    }
}
