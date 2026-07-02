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
 * Purchasing Module Formula Provider — Phase 10 integration.
 *
 * Registers Purchasing-specific fields (Supplier.*, Agreement.* namespaces)
 * and templates with the Visual Formula Designer via the provider
 * bridge introduced in Phase 10.
 *
 * @package Purchasing
 * @since   2.0.0 Phase 10
 */

// ---------------------------------------------------------------------------
// Field provider registration
// ---------------------------------------------------------------------------

/**
 * Register Purchasing fields with the designer field registry.
 *
 * @param FormulaDesigner_Registry_DesignerFieldRegistry $registry
 * @param string                                         $module
 * @return void
 */
function purchasing_formula_designer_register_fields(
    FormulaDesigner_Registry_DesignerFieldRegistry $registry,
    $module
) {
    if ($module !== 'purchasing') {
        return;
    }

    $fields = array(
        // Supplier namespace
        array('namespace' => 'Supplier',  'name' => 'Id',            'label' => 'Supplier ID',         'type' => 'text',    'description' => 'Supplier creditor identifier.'),
        array('namespace' => 'Supplier',  'name' => 'Name',          'label' => 'Supplier Name',       'type' => 'text',    'description' => 'Supplier name.'),
        array('namespace' => 'Supplier',  'name' => 'GroupId',       'label' => 'Supplier Group',      'type' => 'text',    'description' => 'Supplier group identifier.'),
        array('namespace' => 'Supplier',  'name' => 'Currency',      'label' => 'Currency',            'type' => 'text',    'description' => 'Supplier default currency.'),
        array('namespace' => 'Supplier',  'name' => 'TaxId',         'label' => 'Tax ID',              'type' => 'text',    'description' => 'Supplier tax identifier.'),
        array('namespace' => 'Supplier',  'name' => 'PaymentTerms',  'label' => 'Payment Terms',       'type' => 'text',    'description' => 'Supplier payment terms identifier.'),
        array('namespace' => 'Supplier',  'name' => 'CreditLimit',   'label' => 'Credit Limit',        'type' => 'number',  'description' => 'Credit limit from this supplier.'),
        array('namespace' => 'Supplier',  'name' => 'Rating',        'label' => 'Rating',              'type' => 'number',  'description' => 'Supplier performance rating.'),
        // Agreement namespace
        array('namespace' => 'Agreement', 'name' => 'Id',            'label' => 'Agreement ID',        'type' => 'text',    'description' => 'Purchase agreement identifier.'),
        array('namespace' => 'Agreement', 'name' => 'StartDate',     'label' => 'Start Date',          'type' => 'date',    'description' => 'Agreement start date.'),
        array('namespace' => 'Agreement', 'name' => 'EndDate',       'label' => 'End Date',            'type' => 'date',    'description' => 'Agreement end date.'),
        array('namespace' => 'Agreement', 'name' => 'MinQty',        'label' => 'Min Quantity',        'type' => 'number',  'description' => 'Minimum order quantity.'),
        array('namespace' => 'Agreement', 'name' => 'MaxQty',        'label' => 'Max Quantity',        'type' => 'number',  'description' => 'Maximum order quantity.'),
        array('namespace' => 'Agreement', 'name' => 'Price',         'label' => 'Agreement Price',     'type' => 'number',  'description' => 'Negotiated price per unit.'),
        array('namespace' => 'Agreement', 'name' => 'Discount',      'label' => 'Discount %',          'type' => 'number',  'description' => 'Negotiated discount percentage.'),
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
 * Register Purchasing templates with the designer template registry.
 *
 * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
 * @param string                                            $module
 * @return void
 */
function purchasing_formula_designer_register_templates(
    FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
    $module
) {
    // Purchasing templates are deferred to Phase 14 cross-module rollout.
    // No built-in templates exist yet for this module.
}

// ---------------------------------------------------------------------------
// Provider registration with the designer bridge
// ---------------------------------------------------------------------------

if (!isset($GLOBALS['formula_designer_provider_fields'])) {
    $GLOBALS['formula_designer_provider_fields'] = array();
}
$GLOBALS['formula_designer_provider_fields']['purchasing'][] = 'purchasing_formula_designer_register_fields';

if (!isset($GLOBALS['formula_designer_provider_templates'])) {
    $GLOBALS['formula_designer_provider_templates'] = array();
}
$GLOBALS['formula_designer_provider_templates']['purchasing'][] = 'purchasing_formula_designer_register_templates';

if (function_exists('formula_designer_register_provider')) {
    $provider_file = __FILE__;
    if (!defined('FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS')) {
        require_once dirname(dirname(dirname(__FILE__)))
            . '/includes/formula_designer/designer_bootstrap.inc';
    }
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS,
        'purchasing',
        $provider_file
    );
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES,
        'purchasing',
        $provider_file
    );
}
