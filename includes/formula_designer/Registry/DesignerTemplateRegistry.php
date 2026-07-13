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
 * DesignerTemplateRegistry — frozen-once registry for formula templates.
 *
 * Templates are organized by module and category. Each template implements
 * FormulaDesigner_Contracts_DesignerTemplateInterface so the browser can
 * discover available starting points for formula authoring.
 *
 * The registry follows the same freeze lifecycle as FunctionRegistry and
 * VariableRegistry in the Formula Framework.
 *
 * @package FormulaDesigner\Registry
 * @since   2.0.0
 */
class FormulaDesigner_Registry_DesignerTemplateRegistry
{
    /** @var FormulaDesigner_Contracts_DesignerTemplateInterface[] */
    private $templates = array();

    /** @var bool */
    private $frozen = false;

    /**
     * Register a formula template.
     *
     * @param FormulaDesigner_Contracts_DesignerTemplateInterface $template
     * @return void
     * @throws RuntimeException
     */
    public function register(FormulaDesigner_Contracts_DesignerTemplateInterface $template)
    {
        if ($this->frozen) {
            throw new RuntimeException(
                'DesignerTemplateRegistry is frozen. Cannot register template: ' . $template->getId()
            );
        }

        $id = strtolower($template->getId());
        if (isset($this->templates[$id])) {
            throw new RuntimeException(
                'Designer template already registered: ' . $template->getId()
            );
        }

        $this->templates[$id] = $template;
    }

    /**
     * Get a template by its unique identifier.
     *
     * @param string $id
     * @return FormulaDesigner_Contracts_DesignerTemplateInterface|null
     */
    public function get($id)
    {
        $key = strtolower((string)$id);
        return isset($this->templates[$key]) ? $this->templates[$key] : null;
    }

    /**
     * Get all registered templates.
     *
     * @return FormulaDesigner_Contracts_DesignerTemplateInterface[]
     */
    public function all()
    {
        return $this->templates;
    }

    /**
     * Get templates filtered by module identifier.
     *
     * @param string $module
     * @return FormulaDesigner_Contracts_DesignerTemplateInterface[]
     */
    public function byModule($module)
    {
        $module = strtolower((string)$module);
        $result = array();

        foreach ($this->templates as $template) {
            $metadata = $template->getMetadata();
            $template_module = isset($metadata['module'])
                ? strtolower((string)$metadata['module'])
                : '';

            if ($template_module === $module || $template_module === '*') {
                $result[] = $template;
            }
        }

        return $result;
    }

    /**
     * Get templates filtered by category within a module.
     *
     * @param string $module
     * @param string $category
     * @return FormulaDesigner_Contracts_DesignerTemplateInterface[]
     */
    public function byModuleAndCategory($module, $category)
    {
        $module = strtolower((string)$module);
        $category = strtolower((string)$category);
        $result = array();

        foreach ($this->templates as $template) {
            $metadata = $template->getMetadata();

            $template_module = isset($metadata['module'])
                ? strtolower((string)$metadata['module'])
                : '';

            $template_category = isset($metadata['category'])
                ? strtolower((string)$metadata['category'])
                : '';

            if (
                ($template_module === $module || $template_module === '*')
                && ($template_category === $category || $template_category === '*')
            ) {
                $result[] = $template;
            }
        }

        return $result;
    }

    /**
     * Get all unique category names present in the registry.
     *
     * @return string[]
     */
    public function getCategories()
    {
        $categories = array();

        foreach ($this->templates as $template) {
            $metadata = $template->getMetadata();
            if (isset($metadata['category']) && is_string($metadata['category']) && $metadata['category'] !== '') {
                $categories[] = $metadata['category'];
            }
        }

        $categories = array_unique($categories);
        sort($categories);

        return $categories;
    }

    /**
     * Freeze the registry to prevent further mutation.
     *
     * @return void
     */
    public function freeze()
    {
        $this->frozen = true;
    }

    /**
     * Check whether the registry is frozen.
     *
     * @return bool
     */
    public function isFrozen()
    {
        return $this->frozen;
    }

    /**
     * Get the number of registered templates.
     *
     * @return int
     */
    public function count()
    {
        return count($this->templates);
    }
}
