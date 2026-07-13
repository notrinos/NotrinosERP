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
 * DesignerFieldMetadata — value object for palette field metadata.
 *
 * @package FormulaDesigner\Registry
 * @since   2.0.0
 */
class FormulaDesigner_Registry_DesignerFieldMetadata
    implements FormulaDesigner_Contracts_DesignerFieldInterface
{
    /** @var string */
    public $name;

    /** @var string */
    public $type;

    /** @var string */
    public $namespace;

    /** @var string */
    public $label;

    /** @var string */
    public $description;

    /** @var string|null */
    public $requiredPermission;

    /** @var string */
    public $module;

    /**
     * Construct field metadata.
     *
     * @param array $data
     */
    public function __construct(array $data = array())
    {
        $this->name = isset($data['name']) ? (string)$data['name'] : '';
        $this->type = isset($data['type']) ? (string)$data['type'] : 'mixed';
        $this->namespace = isset($data['namespace']) ? (string)$data['namespace'] : '';
        $this->label = isset($data['label']) ? (string)$data['label'] : $this->humanize($this->name);
        $this->description = isset($data['description']) ? (string)$data['description'] : $this->label;
        $this->requiredPermission = isset($data['requiredPermission']) ? (string)$data['requiredPermission'] : null;
        $this->module = isset($data['module']) ? (string)$data['module'] : '*';
    }

    /**
     * Convert the field metadata into an array.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'name' => $this->name,
            'type' => $this->type,
            'namespace' => $this->namespace,
            'label' => $this->label,
            'description' => $this->description,
            'requiredPermission' => $this->requiredPermission,
            'module' => $this->module,
        );
    }

    /**
     * Get the fully qualified field name.
     *
     * @return string
     */
    public function getQualifiedName()
    {
        if ($this->namespace === '') {
            return $this->name;
        }

        return $this->namespace . '.' . $this->name;
    }

    /**
     * Get the field label.
     *
     * @return string
     */
    public function getLabel()
    {
        return $this->label;
    }

    /**
     * Rebuild metadata from a serialized array.
     *
     * @param array $data
     * @return FormulaDesigner_Registry_DesignerFieldMetadata
     */
    public static function fromArray(array $data)
    {
        return new self($data);
    }

    /**
     * Convert a raw identifier into a human-readable label.
     *
     * @param string $value
     * @return string
     */
    private function humanize($value)
    {
        $value = preg_replace('/([a-z])([A-Z])/', '$1 $2', (string)$value);
        $value = str_replace(array('_', '-'), ' ', $value);

        return trim($value);
    }
}