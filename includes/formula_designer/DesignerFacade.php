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
 * DesignerFacade — single public entry point for the Visual Formula Designer.
 *
 * The facade stays stable across all implementation phases so host pages can
 * depend on a single entry point while the package grows underneath it.
 *
 * @package FormulaDesigner
 * @since   2.0.0
 */
class DesignerFacade
{
    /** @var bool */
    private static $initialized = false;

    /** @var array|null */
    private static $functionCatalog = null;

    /**
     * Initialize the designer package state.
     *
     * @return void
     */
    public static function initialize()
    {
        self::$initialized = true;
    }

    /**
     * Check whether the designer package has been initialized.
     *
     * @return bool
     */
    public static function isInitialized()
    {
        return self::$initialized;
    }

    /**
     * Render the visual editor shell.
     *
     * @param string $formula
     * @param string $module
     * @param array  $options
     * @return string
     */
    public static function renderEditor($formula, $module, array $options = array())
    {
        if (!self::$initialized) {
            self::initialize();
        }

        $renderer = new FormulaDesigner_Renderer_DesignerRenderer($formula, $module, $options);

        return $renderer->render();
    }

    /**
     * Get the package version.
     *
     * @return string
     */
    public static function getVersion()
    {
        return defined('FORMULA_DESIGNER_VERSION') ? FORMULA_DESIGNER_VERSION : '1.0.0';
    }

    /**
     * Get the visible field palette sections for a module.
     *
     * @param string $module
     * @return array
     */
    public static function getAvailableFields($module)
    {
        if (!self::$initialized) {
            self::initialize();
        }

        return self::buildFieldSections($module);
    }

    /**
     * Get the function palette sections grouped by category.
     *
     * @param string $module
     * @return array
     */
    public static function getAvailableFunctions($module)
    {
        if (!self::$initialized) {
            self::initialize();
        }

        return self::buildFunctionSections($module);
    }

    /**
     * Build the field palette sections.
     *
     * @param string $module
     * @return array
     */
    private static function buildFieldSections($module)
    {
        $registry = self::createFieldRegistry($module);
        $sections = array();

        foreach ($registry->all() as $field) {
            $item = $field->toArray();

            if (!self::userHasPermission(isset($item['requiredPermission']) ? $item['requiredPermission'] : null)) {
                continue;
            }

            $namespace = isset($item['namespace']) ? (string)$item['namespace'] : 'General';
            if (!isset($sections[$namespace])) {
                $sections[$namespace] = array(
                    'namespace' => $namespace,
                    'label' => $namespace,
                    'count' => 0,
                    'items' => array(),
                );
            }

            $item['qualifiedName'] = $field->getQualifiedName();
            $item['enabled'] = true;
            $sections[$namespace]['items'][] = $item;
            $sections[$namespace]['count'] += 1;
        }

        foreach ($sections as $namespace => $section) {
            usort($sections[$namespace]['items'], array('DesignerFacade', 'comparePaletteItemsByLabel'));
        }

        return array_values($sections);
    }

    /**
     * Build the function palette sections.
     *
     * @param string $module
     * @return array
     */
    private static function buildFunctionSections($module)
    {
        if (self::$functionCatalog === null) {
            self::$functionCatalog = self::discoverFunctionCatalog();
        }

        $sections = array();
        foreach (self::$functionCatalog as $item) {
            $category = isset($item['category']) ? (string)$item['category'] : 'General';
            if (!isset($sections[$category])) {
                $sections[$category] = array(
                    'category' => $category,
                    'label' => $category,
                    'count' => 0,
                    'items' => array(),
                );
            }

            $item['enabled'] = self::userHasPermission(isset($item['requiredPermission']) ? $item['requiredPermission'] : null);
            $sections[$category]['items'][] = $item;
            $sections[$category]['count'] += 1;
        }

        foreach ($sections as $category => $section) {
            usort($sections[$category]['items'], array('DesignerFacade', 'comparePaletteItemsByLabel'));
        }

        return array_values($sections);
    }

    /**
     * Create and populate the designer field registry.
     *
     * @param string $module
     * @return FormulaDesigner_Registry_DesignerFieldRegistry
     */
    private static function createFieldRegistry($module)
    {
        $registry = new FormulaDesigner_Registry_DesignerFieldRegistry();
        $module = strtolower((string)$module);

        foreach (self::getBuiltInFieldDefinitions() as $definition) {
            if (!self::fieldAppliesToModule($definition, $module)) {
                continue;
            }

            $registry->register(new FormulaDesigner_Registry_DesignerFieldMetadata($definition));
        }

        if (function_exists('hook_invoke_all') && defined('FORMULA_DESIGNER_HOOK_REGISTER_FIELDS')) {
            hook_invoke_all(FORMULA_DESIGNER_HOOK_REGISTER_FIELDS, $registry, $module);
        }

        $registry->freeze();

        return $registry;
    }

    /**
     * Get the built-in field catalog used before module registration lands.
     *
     * @return array
     */
    private static function getBuiltInFieldDefinitions()
    {
        return array(
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'Id', 'label' => 'Employee ID', 'type' => 'number', 'description' => 'Employee database ID.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'Name', 'label' => 'Employee Name', 'type' => 'text', 'description' => 'Employee full name.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'FirstName', 'label' => 'First Name', 'type' => 'text', 'description' => 'Employee first name.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'LastName', 'label' => 'Last Name', 'type' => 'text', 'description' => 'Employee last name.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'BasicSalary', 'label' => 'Basic Salary', 'type' => 'number', 'description' => 'Employee basic salary.', 'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'GrossSalary', 'label' => 'Gross Salary', 'type' => 'number', 'description' => 'Employee gross salary.', 'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'Department', 'label' => 'Department', 'type' => 'text', 'description' => 'Employee department.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'Designation', 'label' => 'Designation', 'type' => 'text', 'description' => 'Employee designation.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'JoinDate', 'label' => 'Join Date', 'type' => 'date', 'description' => 'Employee joining date.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'ConfirmationDate', 'label' => 'Confirmation Date', 'type' => 'date', 'description' => 'Employee confirmation date.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'TerminationDate', 'label' => 'Termination Date', 'type' => 'date', 'description' => 'Employee termination date.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'IsActive', 'label' => 'Is Active', 'type' => 'boolean', 'description' => 'Whether the employee is active.'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'BankAccount', 'label' => 'Bank Account', 'type' => 'text', 'description' => 'Employee bank account.', 'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'TaxId', 'label' => 'Tax ID', 'type' => 'text', 'description' => 'Employee tax identifier.', 'requiredPermission' => 'SA_HRM_VIEW_SALARY'),
            array('module' => 'hrm', 'namespace' => 'Employee', 'name' => 'EmployeeCode', 'label' => 'Employee Code', 'type' => 'text', 'description' => 'Employee code.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'Basic', 'label' => 'Basic', 'type' => 'number', 'description' => 'Payroll basic amount.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'Gross', 'label' => 'Gross', 'type' => 'number', 'description' => 'Payroll gross amount.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'DaysWorked', 'label' => 'Days Worked', 'type' => 'number', 'description' => 'Days worked in the period.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'PayableDays', 'label' => 'Payable Days', 'type' => 'number', 'description' => 'Payable days in the period.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'WorkingDays', 'label' => 'Working Days', 'type' => 'number', 'description' => 'Configured working days.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'DaysInMonth', 'label' => 'Days In Month', 'type' => 'number', 'description' => 'Days in the current month.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'LeaveDays', 'label' => 'Leave Days', 'type' => 'number', 'description' => 'Leave days taken.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'PaidLeaveDays', 'label' => 'Paid Leave Days', 'type' => 'number', 'description' => 'Paid leave days.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'AbsentDays', 'label' => 'Absent Days', 'type' => 'number', 'description' => 'Absent days.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'OvertimeHours', 'label' => 'Overtime Hours', 'type' => 'number', 'description' => 'Overtime hours worked.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'UnpaidLeaveDays', 'label' => 'Unpaid Leave Days', 'type' => 'number', 'description' => 'Unpaid leave days.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'HourlyRate', 'label' => 'Hourly Rate', 'type' => 'number', 'description' => 'Calculated hourly rate.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'TaxRate', 'label' => 'Tax Rate', 'type' => 'number', 'description' => 'Payroll tax rate.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'PeriodStart', 'label' => 'Period Start', 'type' => 'date', 'description' => 'Payroll period start date.'),
            array('module' => 'hrm', 'namespace' => 'Payroll', 'name' => 'PeriodEnd', 'label' => 'Period End', 'type' => 'date', 'description' => 'Payroll period end date.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Id', 'label' => 'Company ID', 'type' => 'number', 'description' => 'Company database ID.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Name', 'label' => 'Company Name', 'type' => 'text', 'description' => 'Company name.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Currency', 'label' => 'Currency', 'type' => 'text', 'description' => 'Default currency code.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Locale', 'label' => 'Locale', 'type' => 'text', 'description' => 'Locale identifier.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Country', 'label' => 'Country', 'type' => 'text', 'description' => 'Country code.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'FiscalYearStart', 'label' => 'Fiscal Year Start', 'type' => 'date', 'description' => 'Fiscal year start date.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'TaxId', 'label' => 'Tax ID', 'type' => 'text', 'description' => 'Company tax identifier.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Address', 'label' => 'Address', 'type' => 'text', 'description' => 'Company address.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Phone', 'label' => 'Phone', 'type' => 'text', 'description' => 'Company phone number.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Email', 'label' => 'Email', 'type' => 'text', 'description' => 'Company email address.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Website', 'label' => 'Website', 'type' => 'text', 'description' => 'Company website URL.'),
            array('module' => '*', 'namespace' => 'Company', 'name' => 'Timezone', 'label' => 'Timezone', 'type' => 'text', 'description' => 'Company timezone.'),
        );
    }

    /**
     * Discover built-in function metadata directly from provider classes.
     *
     * @return array
     */
    private static function discoverFunctionCatalog()
    {
        self::ensureFormulaBootstrapLoaded();

        $catalog = array();
        foreach (self::getFunctionProviderFiles() as $file_path) {
            if (file_exists($file_path)) {
                require_once $file_path;
            }

            foreach (self::extractProviderClassNames($file_path) as $class_name) {
                if (!class_exists($class_name)) {
                    continue;
                }

                $reflection = new ReflectionClass($class_name);
                if (!$reflection->isInstantiable() || !$reflection->implementsInterface('Formula_Contracts_FormulaFunctionInterface')) {
                    continue;
                }

                $instance = $reflection->newInstance();

                $metadata = $instance->getMetadata();
                $item = $metadata->toArray();
                $item['label'] = $item['name'];
                $item['tokenValue'] = $item['name'] . '(';
                $item['signature'] = self::buildFunctionSignature($item);
                $catalog[] = $item;
            }
        }

        return $catalog;
    }

    /**
     * Ensure the formula bootstrap is available for provider metadata discovery.
     *
     * @return void
     */
    private static function ensureFormulaBootstrapLoaded()
    {
        if (!isset($GLOBALS['path_to_root']) || !$GLOBALS['path_to_root']) {
            $GLOBALS['path_to_root'] = dirname(dirname(dirname(__FILE__)));
        }

        if (!class_exists('FormulaFacade', false)) {
            require_once $GLOBALS['path_to_root'] . '/includes/formula/formula_bootstrap.inc';
        }
    }

    /**
     * Get the provider files that define built-in functions.
     *
     * @return array
     */
    private static function getFunctionProviderFiles()
    {
        $base = $GLOBALS['path_to_root'] . '/includes/formula/Providers/';

        return array(
            $base . 'MathFunctions.php',
            $base . 'LogicalFunctions.php',
            $base . 'DateFunctions.php',
            $base . 'FinancialFunctions.php',
            $base . 'ERPFunctions.php',
        );
    }

    /**
     * Extract all provider class names from one provider file.
     *
     * @param string $file_path
     * @return array
     */
    private static function extractProviderClassNames($file_path)
    {
        $contents = @file_get_contents($file_path);
        if ($contents === false) {
            return array();
        }

        $matches = array();
        preg_match_all('/class\s+(Formula_Providers_[A-Za-z0-9_]+)/', $contents, $matches);

        return isset($matches[1]) ? array_values(array_unique($matches[1])) : array();
    }

    /**
     * Build a human-readable function signature.
     *
     * @param array $item
     * @return string
     */
    private static function buildFunctionSignature(array $item)
    {
        $min_args = isset($item['minArgs']) ? (int)$item['minArgs'] : 0;
        $max_args = isset($item['maxArgs']) ? (int)$item['maxArgs'] : 0;
        $parts = array();
        $index = 1;

        if ($max_args < 0) {
            return $item['name'] . '(...)';
        }

        while ($index <= $max_args) {
            $parts[] = 'arg' . $index . ($index > $min_args ? '?' : '');
            $index += 1;
        }

        return $item['name'] . '(' . implode(', ', $parts) . ')';
    }

    /**
     * Determine whether a field definition belongs to the requested module.
     *
     * @param array  $definition
     * @param string $module
     * @return bool
     */
    private static function fieldAppliesToModule(array $definition, $module)
    {
        if (!isset($definition['module']) || $definition['module'] === '*') {
            return true;
        }

        return strtolower((string)$definition['module']) === strtolower((string)$module);
    }

    /**
     * Check whether the current user has a required permission.
     *
     * @param string|null $permission
     * @return bool
     */
    private static function userHasPermission($permission)
    {
        if ($permission === null || $permission === '') {
            return true;
        }

        if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])) {
            return true;
        }

        if (!method_exists($_SESSION['wa_current_user'], 'can_access')) {
            return true;
        }

        return (bool)$_SESSION['wa_current_user']->can_access($permission);
    }

    /**
     * Sort palette items by label.
     *
     * @param array $left
     * @param array $right
     * @return int
     */
    private static function comparePaletteItemsByLabel(array $left, array $right)
    {
        return strcasecmp(
            isset($left['label']) ? $left['label'] : '',
            isset($right['label']) ? $right['label'] : ''
        );
    }

    // -----------------------------------------------------------------------
    // Templates &amp; Favorites API
    // -----------------------------------------------------------------------

    /**
     * Get all templates registered for a module, grouped by category.
     *
     * @param string $module Module identifier (hrm, sales, inventory, etc.)
     * @return array Category-grouped template metadata arrays
     */
    public static function getAvailableTemplates($module)
    {
        if (!self::$initialized) {
            self::initialize();
        }

        return self::buildTemplateSections($module);
    }

    /**
     * Build the template palette sections grouped by category.
     *
     * @param string $module
     * @return array
     */
    private static function buildTemplateSections($module)
    {
        $registry = self::createTemplateRegistry($module);
        $sections = array();

        foreach ($registry->all() as $template) {
            $metadata = $template->getMetadata();
            $category = isset($metadata['category'])
                ? (string)$metadata['category']
                : 'General';

            if (!isset($sections[$category])) {
                $sections[$category] = array(
                    'category' => $category,
                    'label'    => $category,
                    'count'    => 0,
                    'items'    => array(),
                );
            }

            $item = array(
                'id'          => $template->getId(),
                'label'       => isset($metadata['label']) ? (string)$metadata['label'] : '',
                'description' => isset($metadata['description']) ? (string)$metadata['description'] : '',
                'formula'     => $template->getFormula(),
                'module'      => isset($metadata['module']) ? (string)$metadata['module'] : $module,
                'difficulty'  => isset($metadata['difficulty']) ? (string)$metadata['difficulty'] : 'beginner',
                'tags'        => isset($metadata['tags']) && is_array($metadata['tags']) ? $metadata['tags'] : array(),
            );

            $sections[$category]['items'][] = $item;
            $sections[$category]['count'] += 1;
        }

        // Sort sections by label
        uasort($sections, array('DesignerFacade', 'compareTemplateSectionsByLabel'));

        return array_values($sections);
    }

    /**
     * Sort template section arrays by label.
     *
     * @param array $left
     * @param array $right
     * @return int
     */
    private static function compareTemplateSectionsByLabel(array $left, array $right)
    {
        return strcasecmp(
            isset($left['label']) ? $left['label'] : '',
            isset($right['label']) ? $right['label'] : ''
        );
    }

    /**
     * Create and populate the designer template registry for a module.
     *
     * @param string $module
     * @return FormulaDesigner_Registry_DesignerTemplateRegistry
     */
    private static function createTemplateRegistry($module)
    {
        $registry = new FormulaDesigner_Registry_DesignerTemplateRegistry();
        $module   = strtolower((string)$module);

        // Load built-in templates from provider classes
        self::loadBuiltInTemplates($registry, $module);

        // Allow extensions to register additional templates
        if (function_exists('hook_invoke_all') && defined('FORMULA_DESIGNER_HOOK_REGISTER_TEMPLATES')) {
            hook_invoke_all(FORMULA_DESIGNER_HOOK_REGISTER_TEMPLATES, $registry, $module);
        }

        $registry->freeze();

        return $registry;
    }

    /**
     * Load all built-in template provider all() sets into the registry.
     *
     * @param FormulaDesigner_Registry_DesignerTemplateRegistry $registry
     * @param string                                            $module
     * @return void
     */
    private static function loadBuiltInTemplates(
        FormulaDesigner_Registry_DesignerTemplateRegistry $registry,
        $module
    ) {
        $provider_map = array(
            'hrm'           => 'FormulaDesigner_Templates_PayrollTemplateProvider',
            'sales'         => 'FormulaDesigner_Templates_PricingTemplateProvider',
            'inventory'     => 'FormulaDesigner_Templates_InventoryTemplateProvider',
            'manufacturing' => 'FormulaDesigner_Templates_ManufacturingTemplateProvider',
            'gl'            => 'FormulaDesigner_Templates_GLTemplateProvider',
        );

        $star_providers = array();

        foreach ($provider_map as $provider_module => $class_name) {
            if (!class_exists($class_name)) {
                continue;
            }

            $templates = call_user_func(array($class_name, 'all'));

            if ($provider_module === $module || $provider_module === '*') {
                foreach ($templates as $template) {
                    $registry->register($template);
                }
            }
        }
    }
}