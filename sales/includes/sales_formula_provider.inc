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
 * Sales Module Formula Provider — Phase 10 integration.
 *
 * Registers Sales-specific fields (Item.*, Customer.*, Pricing.* namespaces)
 * and templates with the Visual Formula Designer via the provider
 * bridge introduced in Phase 10.
 *
 * This file is auto-discovered by {@see formula_designer_autodiscover_providers()}
 * when the designer bootstrap loads during a full NotrinosERP session.
 *
 * @package Sales
 * @since   2.0.0 Phase 10
 */

// ---------------------------------------------------------------------------
// Field provider registration
// ---------------------------------------------------------------------------

/**
 * Register Sales fields with the designer field registry.
 *
 * @param FormulaDesigner_Registry_DesignerFieldRegistry $registry
 * @param string                                         $module
 * @return void
 */
function sales_formula_designer_register_fields(
    FormulaDesigner_Registry_DesignerFieldRegistry $registry,
    $module
) {
    if ($module !== 'sales') {
        return;
    }

    $fields = array(
        // Item namespace (sales pricing context)
        array('namespace' => 'Item',    'name' => 'Id',          'label' => 'Item ID',             'type' => 'text',    'description' => 'Inventory item identifier.'),
        array('namespace' => 'Item',    'name' => 'Name',        'label' => 'Item Name',           'type' => 'text',    'description' => 'Item description/name.'),
        array('namespace' => 'Item',    'name' => 'Code',        'label' => 'Item Code',           'type' => 'text',    'description' => 'Item system code.'),
        array('namespace' => 'Item',    'name' => 'Cost',        'label' => 'Item Cost',           'type' => 'number',  'description' => 'Item standard/average cost.',         'requiredPermission' => 'SA_INVENTORY_COST'),
        array('namespace' => 'Item',    'name' => 'Price',       'label' => 'List Price',          'type' => 'number',  'description' => 'Item list/selling price.'),
        array('namespace' => 'Item',    'name' => 'CategoryId',  'label' => 'Category ID',         'type' => 'text',    'description' => 'Item category identifier.'),
        array('namespace' => 'Item',    'name' => 'TaxTypeId',   'label' => 'Tax Type ID',         'type' => 'text',    'description' => 'Item tax type identifier.'),
        // Customer namespace
        array('namespace' => 'Customer','name' => 'Id',          'label' => 'Customer ID',         'type' => 'text',    'description' => 'Customer debtor identifier.'),
        array('namespace' => 'Customer','name' => 'Name',        'label' => 'Customer Name',       'type' => 'text',    'description' => 'Customer/branch name.'),
        array('namespace' => 'Customer','name' => 'GroupId',     'label' => 'Price Group',         'type' => 'text',    'description' => 'Customer price group.'),
        array('namespace' => 'Customer','name' => 'Currency',    'label' => 'Currency',            'type' => 'text',    'description' => 'Customer default currency.'),
        array('namespace' => 'Customer','name' => 'Discount',    'label' => 'Discount %',          'type' => 'number',  'description' => 'Customer default discount percentage.'),
        array('namespace' => 'Customer','name' => 'CreditLimit', 'label' => 'Credit Limit',        'type' => 'number',  'description' => 'Customer credit limit amount.',       'requiredPermission' => 'SA_CREDIT_CONTROL'),
        // Pricing namespace
        array('namespace' => 'Pricing', 'name' => 'BasePrice',   'label' => 'Base Price',          'type' => 'number',  'description' => 'Base price for computation (list or cost).'),
        array('namespace' => 'Pricing', 'name' => 'Quantity',    'label' => 'Quantity',            'type' => 'number',  'description' => 'Order quantity for tiered pricing.'),
        array('namespace' => 'Pricing', 'name' => 'Surcharge',   'label' => 'Surcharge',           'type' => 'number',  'description' => 'Surcharge amount to add.'),
        array('namespace' => 'Pricing', 'name' => 'Rounding',    'label' => 'Rounding',            'type' => 'number',  'description' => 'Rounding precision value.'),
    );

    foreach ($fields as $field) {
        if (!$registry->has($field['namespace'] . '.' . $field['name'])) {
            $registry->register(new FormulaDesigner_Registry_DesignerFieldMetadata($field));
        }
    }
}

// ---------------------------------------------------------------------------
// Template provider registration
// ---------------------------------------------------------------------------

/**
 * Register Sales templates with the designer template registry.
 *
 * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
 * @param string                                            $module
 * @return void
 */
function sales_formula_designer_register_templates(
    FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
    $module
) {
    if ($module !== 'sales') {
        return;
    }

    if (class_exists('FormulaDesigner_Templates_PricingTemplateProvider')) {
        $templates = FormulaDesigner_Templates_PricingTemplateProvider::all();
        foreach ($templates as $template) {
            if (!$registry->has($template->getId())) {
                $registry->register($template);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Provider registration with the designer bridge
// ---------------------------------------------------------------------------

if (!isset($GLOBALS['formula_designer_provider_fields'])) {
    $GLOBALS['formula_designer_provider_fields'] = array();
}
$GLOBALS['formula_designer_provider_fields']['sales'][] = 'sales_formula_designer_register_fields';

if (!isset($GLOBALS['formula_designer_provider_templates'])) {
    $GLOBALS['formula_designer_provider_templates'] = array();
}
$GLOBALS['formula_designer_provider_templates']['sales'][] = 'sales_formula_designer_register_templates';

if (function_exists('formula_designer_register_provider')) {
    $provider_file = __FILE__;
    if (!defined('FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS')) {
        require_once dirname(dirname(dirname(__FILE__)))
            . '/includes/formula_designer/designer_bootstrap.inc';
    }
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS,
        'sales',
        $provider_file
    );
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES,
        'sales',
        $provider_file
    );
}
