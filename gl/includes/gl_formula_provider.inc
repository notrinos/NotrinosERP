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
 * GL (General Ledger) Module Formula Provider — Phase 10 integration.
 *
 * Registers GL-specific fields (Account.*, Ledger.*, Budget.* namespaces)
 * and templates with the Visual Formula Designer via the provider
 * bridge introduced in Phase 10.
 *
 * @package GL
 * @since   2.0.0 Phase 10
 */

// ---------------------------------------------------------------------------
// Field provider registration
// ---------------------------------------------------------------------------

/**
 * Register GL fields with the designer field registry.
 *
 * @param FormulaDesigner_Registry_DesignerFieldRegistry $registry
 * @param string                                         $module
 * @return void
 */
function gl_formula_designer_register_fields(
    FormulaDesigner_Registry_DesignerFieldRegistry $registry,
    $module
) {
    if ($module !== 'gl') {
        return;
    }

    $fields = array(
        // Account namespace
        array('namespace' => 'Account', 'name' => 'Code',           'label' => 'Account Code',        'type' => 'text',    'description' => 'GL account code.'),
        array('namespace' => 'Account', 'name' => 'Name',           'label' => 'Account Name',        'type' => 'text',    'description' => 'GL account name.'),
        array('namespace' => 'Account', 'name' => 'Type',           'label' => 'Account Type',        'type' => 'text',    'description' => 'Account type identifier.'),
        array('namespace' => 'Account', 'name' => 'ParentCode',     'label' => 'Parent Account',      'type' => 'text',    'description' => 'Parent account code.'),
        array('namespace' => 'Account', 'name' => 'IsHeader',       'label' => 'Is Header',           'type' => 'boolean', 'description' => 'Whether this is a header account.'),
        array('namespace' => 'Account', 'name' => 'IsBank',         'label' => 'Is Bank Account',     'type' => 'boolean', 'description' => 'Whether this is a bank account.'),
        array('namespace' => 'Account', 'name' => 'IsDebtor',       'label' => 'Is Debtor',           'type' => 'boolean', 'description' => 'Whether this account tracks debtors.'),
        array('namespace' => 'Account', 'name' => 'IsCreditor',     'label' => 'Is Creditor',         'type' => 'boolean', 'description' => 'Whether this account tracks creditors.'),
        // Ledger namespace
        array('namespace' => 'Ledger',  'name' => 'Debit',          'label' => 'Debit',               'type' => 'number',  'description' => 'Total debit amount for the period.'),
        array('namespace' => 'Ledger',  'name' => 'Credit',         'label' => 'Credit',              'type' => 'number',  'description' => 'Total credit amount for the period.'),
        array('namespace' => 'Ledger',  'name' => 'Balance',        'label' => 'Balance',             'type' => 'number',  'description' => 'Current account balance.'),
        array('namespace' => 'Ledger',  'name' => 'OpeningBalance', 'label' => 'Opening Balance',     'type' => 'number',  'description' => 'Opening balance for the period.'),
        array('namespace' => 'Ledger',  'name' => 'ClosingBalance', 'label' => 'Closing Balance',     'type' => 'number',  'description' => 'Closing balance for the period.'),
        array('namespace' => 'Ledger',  'name' => 'BudgetAmount',   'label' => 'Budget Amount',       'type' => 'number',  'description' => 'Budgeted amount for the period.'),
        array('namespace' => 'Ledger',  'name' => 'ActualAmount',   'label' => 'Actual Amount',       'type' => 'number',  'description' => 'Actual amount for the period.'),
        // Budget namespace
        array('namespace' => 'Budget',  'name' => 'Amount',         'label' => 'Budget Amount',       'type' => 'number',  'description' => 'Budget amount.'),
        array('namespace' => 'Budget',  'name' => 'Utilized',       'label' => 'Budget Utilized',     'type' => 'number',  'description' => 'Amount utilized so far.'),
        array('namespace' => 'Budget',  'name' => 'Remaining',      'label' => 'Budget Remaining',    'type' => 'number',  'description' => 'Amount remaining in budget.'),
        array('namespace' => 'Budget',  'name' => 'FiscalYear',     'label' => 'Fiscal Year',         'type' => 'text',    'description' => 'Fiscal year identifier.'),
        array('namespace' => 'Budget',  'name' => 'Period',         'label' => 'Period',              'type' => 'number',  'description' => 'Fiscal period number.'),
        // Currency namespace (shared but accessible from GL context)
        array('namespace' => 'Currency','name' => 'Code',           'label' => 'Currency Code',       'type' => 'text',    'description' => 'Currency code.'),
        array('namespace' => 'Currency','name' => 'Rate',           'label' => 'Exchange Rate',       'type' => 'number',  'description' => 'Exchange rate to home currency.'),
        array('namespace' => 'Currency','name' => 'Symbol',         'label' => 'Currency Symbol',     'type' => 'text',    'description' => 'Currency symbol.'),
        array('namespace' => 'Currency','name' => 'Decimals',       'label' => 'Decimals',            'type' => 'number',  'description' => 'Decimal places for this currency.'),
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
 * Register GL templates with the designer template registry.
 *
 * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
 * @param string                                            $module
 * @return void
 */
function gl_formula_designer_register_templates(
    FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
    $module
) {
    if ($module !== 'gl') {
        return;
    }

    if (class_exists('FormulaDesigner_Templates_GLTemplateProvider')) {
        $templates = FormulaDesigner_Templates_GLTemplateProvider::all();
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
$GLOBALS['formula_designer_provider_fields']['gl'][] = 'gl_formula_designer_register_fields';

if (!isset($GLOBALS['formula_designer_provider_templates'])) {
    $GLOBALS['formula_designer_provider_templates'] = array();
}
$GLOBALS['formula_designer_provider_templates']['gl'][] = 'gl_formula_designer_register_templates';

if (function_exists('formula_designer_register_provider')) {
    $provider_file = __FILE__;
    if (!defined('FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS')) {
        require_once dirname(dirname(dirname(__FILE__)))
            . '/includes/formula_designer/designer_bootstrap.inc';
    }
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_FIELDS,
        'gl',
        $provider_file
    );
    formula_designer_register_provider(
        FORMULA_DESIGNER_PROVIDER_TYPE_TEMPLATES,
        'gl',
        $provider_file
    );
}
