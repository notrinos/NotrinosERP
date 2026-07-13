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
 * DesignerFieldRegistry — scaffold registry for designer field metadata.
 *
 * The registry intentionally starts empty in Phase 1. Later phases populate it
 * from module hooks while preserving the same freeze lifecycle as the Formula
 * Framework registries.
 *
 * @package FormulaDesigner\Registry
 * @since   2.0.0
 */
class FormulaDesigner_Registry_DesignerFieldRegistry
{
    /** @var FormulaDesigner_Contracts_DesignerFieldInterface[] */
    private $fields = array();

    /** @var bool */
    private $frozen = false;

    /**
     * Register a field definition.
     *
     * @param FormulaDesigner_Contracts_DesignerFieldInterface $field
     * @return void
     * @throws RuntimeException
     */
    public function register(FormulaDesigner_Contracts_DesignerFieldInterface $field)
    {
        if ($this->frozen) {
            throw new RuntimeException(
                'DesignerFieldRegistry is frozen. Cannot register field: ' . $field->getQualifiedName()
            );
        }

        $qualified_name = strtoupper($field->getQualifiedName());
        if (isset($this->fields[$qualified_name])) {
            throw new RuntimeException('Designer field already registered: ' . $qualified_name);
        }

        $this->fields[$qualified_name] = $field;
    }

    /**
     * Retrieve a field by its qualified name.
     *
     * @param string $qualified_name
     * @return FormulaDesigner_Contracts_DesignerFieldInterface|null
     */
    public function get($qualified_name)
    {
        $key = strtoupper($qualified_name);
        return isset($this->fields[$key]) ? $this->fields[$key] : null;
    }

    /**
     * Get all registered fields.
     *
     * @return FormulaDesigner_Contracts_DesignerFieldInterface[]
     */
    public function all()
    {
        return $this->fields;
    }

    /**
     * Check whether a field with the given qualified name is registered.
     *
     * @param string $qualified_name
     * @return bool
     */
    public function has($qualified_name)
    {
        $key = strtoupper($qualified_name);
        return isset($this->fields[$key]);
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
     * Get the number of registered fields.
     *
     * @return int
     */
    public function count()
    {
        return count($this->fields);
    }
}