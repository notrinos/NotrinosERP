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
 * Manufacturing Module Formula Provider — Phase 10 integration.
 *
 * Registers Manufacturing-specific fields (BOM.*, Routing.*, Production.* namespaces)
 * and templates with the Visual Formula Designer via the provider
 * bridge introduced in Phase 10.
 *
 * @package Manufacturing
 * @since   2.0.0 Phase 10
 */

// ---------------------------------------------------------------------------
// Field provider registration
// ---------------------------------------------------------------------------

/**
 * Register Manufacturing fields with the designer field registry.
 *
 * @param FormulaDesigner_Registry_DesignerFieldRegistry $registry
 * @param string                                         $module
 * @return void
 */
function manufacturing_formula_designer_register_fields(
    FormulaDesigner_Registry_DesignerFieldRegistry $registry,
    $module
) {
    if ($module !== 'manufacturing') {
        return;
    }

    $fields = array(
        // BOM namespace
        array('namespace' => 'BOM',         'name' => 'Id',           'label' => 'BOM ID',              'type' => 'text',    'description' => 'Bill of Materials identifier.'),
        array('namespace' => 'BOM',         'name' => 'ItemId',       'label' => 'Item ID',             'type' => 'text',    'description' => 'Manufactured item identifier.'),
        array('namespace' => 'BOM',         'name' => 'Qty',          'label' => 'Quantity',            'type' => 'number',  'description' => 'Quantity per assembly.'),
        array('namespace' => 'BOM',         'name' => 'UOM',          'label' => 'Unit of Measure',     'type' => 'text',    'description' => 'Unit of measure.'),
        array('namespace' => 'BOM',         'name' => 'Cost',         'label' => 'Component Cost',      'type' => 'number',  'description' => 'Component cost.',                     'requiredPermission' => 'SA_MANUFACTURING_VIEW'),
        array('namespace' => 'BOM',         'name' => 'OverheadPct',  'label' => 'Overhead %',          'type' => 'number',  'description' => 'Overhead percentage.'),
        array('namespace' => 'BOM',         'name' => 'TotalCost',    'label' => 'Total BOM Cost',      'type' => 'number',  'description' => 'Total cost of BOM assembly.',         'requiredPermission' => 'SA_MANUFACTURING_VIEW'),
        array('namespace' => 'BOM',         'name' => 'Level',        'label' => 'BOM Level',           'type' => 'number',  'description' => 'BOM nesting level.'),
        // Routing namespace
        array('namespace' => 'Routing',     'name' => 'Sequence',     'label' => 'Sequence',            'type' => 'number',  'description' => 'Routing operation sequence.'),
        array('namespace' => 'Routing',     'name' => 'WorkCenter',   'label' => 'Work Center',         'type' => 'text',    'description' => 'Work center identifier.'),
        array('namespace' => 'Routing',     'name' => 'SetupTime',    'label' => 'Setup Time',          'type' => 'number',  'description' => 'Setup time in hours.'),
        array('namespace' => 'Routing',     'name' => 'RunTime',      'label' => 'Run Time',            'type' => 'number',  'description' => 'Run time per unit in hours.'),
        array('namespace' => 'Routing',     'name' => 'LaborCost',    'label' => 'Labor Cost',          'type' => 'number',  'description' => 'Labor cost per operation.',           'requiredPermission' => 'SA_MANUFACTURING_VIEW'),
        array('namespace' => 'Routing',     'name' => 'MachineCost',  'label' => 'Machine Cost',        'type' => 'number',  'description' => 'Machine cost per operation.',         'requiredPermission' => 'SA_MANUFACTURING_VIEW'),
        // Production namespace
        array('namespace' => 'Production',  'name' => 'WorkOrderId',  'label' => 'Work Order ID',       'type' => 'text',    'description' => 'Work order identifier.'),
        array('namespace' => 'Production',  'name' => 'QtyOrdered',   'label' => 'Qty Ordered',         'type' => 'number',  'description' => 'Quantity ordered.'),
        array('namespace' => 'Production',  'name' => 'QtyCompleted', 'label' => 'Qty Completed',       'type' => 'number',  'description' => 'Quantity completed.'),
        array('namespace' => 'Production',  'name' => 'QtyRemaining', 'label' => 'Qty Remaining',       'type' => 'number',  'description' => 'Quantity remaining.'),
        array('namespace' => 'Production',  'name' => 'StdCost',      'label' => 'Standard Cost',       'type' => 'number',  'description' => 'Standard production cost.',           'requiredPermission' => 'SA_MANUFACTURING_VIEW'),
        array('namespace' => 'Production',  'name' => 'ActualCost',   'label' => 'Actual Cost',         'type' => 'number',  'description' => 'Actual production cost.',             'requiredPermission' => 'SA_MANUFACTURING_VIEW'),
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
 * Register Manufacturing templates with the designer template registry.
 *
 * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
 * @param string                                            $module
 * @return void
 */
function manufacturing_formula_designer_register_templates(
    FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
    $module
) {
    if ($module !== 'manufacturing') {
        return;
    }

    if (class_exists('FormulaDesigner_Templates_ManufacturingTemplateProvider')) {
        $templates = FormulaDesigner_Templates_ManufacturingTemplateProvider::all();
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
$GLOBALS['formula_designer_provider_fields']['manufacturing'][] = 'manufacturing_formula_designer_register_fields';

if (!isset($GLOBALS['formula_designer_provider_templates'])) {
    $GLOBALS['formula_designer_provider_templates'] = array();
}
$GLOBALS['formula_designer_provider_templates']['manufacturing'][] = 'manufacturing_formula_designer_register_templates';

if (function_exists('formula_designer_register_provider')) {
    $provider_file = __FILE__;
    if (!defined('FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS')) {
        require_once dirname(dirname(dirname(__FILE__)))
            . '/includes/formula_designer/designer_bootstrap.inc';
    }
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS,
        'manufacturing',
        $provider_file
    );
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES,
        'manufacturing',
        $provider_file
    );
}
