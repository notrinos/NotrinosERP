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
 * Inventory Module Formula Provider — Phase 10 integration.
 *
 * Registers Inventory-specific fields (Item.*, Stock.*, Warehouse.* namespaces)
 * and templates with the Visual Formula Designer via the provider
 * bridge introduced in Phase 10.
 *
 * @package Inventory
 * @since   2.0.0 Phase 10
 */

// ---------------------------------------------------------------------------
// Field provider registration
// ---------------------------------------------------------------------------

/**
 * Register Inventory fields with the designer field registry.
 *
 * @param FormulaDesigner_Registry_DesignerFieldRegistry $registry
 * @param string                                         $module
 * @return void
 */
function inventory_formula_designer_register_fields(
    FormulaDesigner_Registry_DesignerFieldRegistry $registry,
    $module
) {
    if ($module !== 'inventory') {
        return;
    }

    $fields = array(
        // Item namespace
        array('namespace' => 'Item',      'name' => 'Id',           'label' => 'Item ID',             'type' => 'text',    'description' => 'Inventory item identifier.'),
        array('namespace' => 'Item',      'name' => 'Code',         'label' => 'Item Code',           'type' => 'text',    'description' => 'Item system code.'),
        array('namespace' => 'Item',      'name' => 'Description',  'label' => 'Description',         'type' => 'text',    'description' => 'Item description.'),
        array('namespace' => 'Item',      'name' => 'Cost',         'label' => 'Cost',                'type' => 'number',  'description' => 'Item standard/average cost.',         'requiredPermission' => 'SA_INVENTORY_COST'),
        array('namespace' => 'Item',      'name' => 'Price',        'label' => 'Price',               'type' => 'number',  'description' => 'Item list/selling price.'),
        array('namespace' => 'Item',      'name' => 'Weight',       'label' => 'Weight',              'type' => 'number',  'description' => 'Item unit weight.'),
        array('namespace' => 'Item',      'name' => 'Volume',       'label' => 'Volume',              'type' => 'number',  'description' => 'Item unit volume.'),
        array('namespace' => 'Item',      'name' => 'CategoryId',   'label' => 'Category ID',         'type' => 'text',    'description' => 'Item category identifier.'),
        array('namespace' => 'Item',      'name' => 'TaxTypeId',    'label' => 'Tax Type ID',         'type' => 'text',    'description' => 'Item tax type identifier.'),
        array('namespace' => 'Item',      'name' => 'MBFlag',       'label' => 'MB Flag',             'type' => 'text',    'description' => 'Manufactured/Purchased/Service flag.'),
        // Stock namespace
        array('namespace' => 'Stock',     'name' => 'QtyOnHand',    'label' => 'Qty On Hand',         'type' => 'number',  'description' => 'Current quantity on hand.'),
        array('namespace' => 'Stock',     'name' => 'QtyAvailable', 'label' => 'Qty Available',       'type' => 'number',  'description' => 'Current available quantity.'),
        array('namespace' => 'Stock',     'name' => 'QtyOnOrder',   'label' => 'Qty On Order',        'type' => 'number',  'description' => 'Quantity on open purchase orders.'),
        array('namespace' => 'Stock',     'name' => 'QtyReserved',  'label' => 'Qty Reserved',        'type' => 'number',  'description' => 'Quantity reserved for sales orders.'),
        array('namespace' => 'Stock',     'name' => 'ReorderLevel', 'label' => 'Reorder Level',       'type' => 'number',  'description' => 'Minimum reorder quantity.'),
        array('namespace' => 'Stock',     'name' => 'LastCost',     'label' => 'Last Cost',           'type' => 'number',  'description' => 'Last purchase cost.',                 'requiredPermission' => 'SA_INVENTORY_COST'),
        array('namespace' => 'Stock',     'name' => 'AvgCost',      'label' => 'Average Cost',        'type' => 'number',  'description' => 'Weighted average cost.',              'requiredPermission' => 'SA_INVENTORY_COST'),
        array('namespace' => 'Stock',     'name' => 'StdCost',      'label' => 'Standard Cost',       'type' => 'number',  'description' => 'Standard cost.',                      'requiredPermission' => 'SA_INVENTORY_COST'),
        // Warehouse namespace
        array('namespace' => 'Warehouse', 'name' => 'Id',           'label' => 'Warehouse ID',        'type' => 'text',    'description' => 'Warehouse identifier.'),
        array('namespace' => 'Warehouse', 'name' => 'Name',         'label' => 'Warehouse Name',      'type' => 'text',    'description' => 'Warehouse name.'),
        array('namespace' => 'Warehouse', 'name' => 'Location',     'label' => 'Location',            'type' => 'text',    'description' => 'Warehouse physical location.'),
        array('namespace' => 'Warehouse', 'name' => 'Qty',          'label' => 'Warehouse Qty',       'type' => 'number',  'description' => 'Quantity in this warehouse.'),
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
 * Register Inventory templates with the designer template registry.
 *
 * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
 * @param string                                            $module
 * @return void
 */
function inventory_formula_designer_register_templates(
    FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
    $module
) {
    if ($module !== 'inventory') {
        return;
    }

    if (class_exists('FormulaDesigner_Templates_InventoryTemplateProvider')) {
        $templates = FormulaDesigner_Templates_InventoryTemplateProvider::all();
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
$GLOBALS['formula_designer_provider_fields']['inventory'][] = 'inventory_formula_designer_register_fields';

if (!isset($GLOBALS['formula_designer_provider_templates'])) {
    $GLOBALS['formula_designer_provider_templates'] = array();
}
$GLOBALS['formula_designer_provider_templates']['inventory'][] = 'inventory_formula_designer_register_templates';

if (function_exists('formula_designer_register_provider')) {
    $provider_file = __FILE__;
    if (!defined('FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS')) {
        require_once dirname(dirname(dirname(__FILE__)))
            . '/includes/formula_designer/designer_bootstrap.inc';
    }
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS,
        'inventory',
        $provider_file
    );
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES,
        'inventory',
        $provider_file
    );
}
