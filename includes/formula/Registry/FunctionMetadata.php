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
 * FunctionMetadata — Value object describing a registered formula function.
 *
 * Carries all metadata about a function: name, argument counts, return type,
 * determinism, cacheability, required permissions, and deprecation status.
 * This metadata is used by the parser for argument validation, by the runtime
 * for permission checks, and by the IDE/editor for autocomplete.
 *
 * @package Formula\Registry
 * @since   2.0.0
 */
class Formula_Registry_FunctionMetadata
{
    /** @var string Unique function name (case-insensitive, uppercase) */
    public $name;

    /** @var string Human-readable description */
    public $description;

    /** @var string Category: 'Math', 'Logical', 'Date', 'Text', 'Financial', 'ERP' */
    public $category;

    /** @var string Version when this function was introduced */
    public $version;

    /** @var int Minimum argument count (-1 = variable) */
    public $minArgs;

    /** @var int Maximum argument count (-1 = variable) */
    public $maxArgs;

    /** @var string Return type: 'decimal', 'boolean', 'string', 'date', 'mixed' */
    public $returnType;

    /** @var bool True if same arguments always produce the same result */
    public $isDeterministic;

    /** @var bool True if the result can be cached for the session */
    public $isCacheable;

    /** @var string|null SA_* security permission required, or null if public */
    public $requiredPermission;

    /** @var string|null Version since which this function is deprecated */
    public $deprecatedSince;

    /** @var string|null Replacement function name (if deprecated) */
    public $replacedBy;

    /**
     * Construct function metadata.
     *
     * @param array $data Associative array of properties (any missing keys get defaults)
     */
    public function __construct(array $data = array())
    {
        $this->name               = isset($data['name']) ? (string)$data['name'] : '';
        $this->description        = isset($data['description']) ? (string)$data['description'] : '';
        $this->category           = isset($data['category']) ? (string)$data['category'] : 'Math';
        $this->version            = isset($data['version']) ? (string)$data['version'] : '1.0';
        $this->minArgs            = isset($data['minArgs']) ? (int)$data['minArgs'] : 0;
        $this->maxArgs            = isset($data['maxArgs']) ? (int)$data['maxArgs'] : 0;
        $this->returnType         = isset($data['returnType']) ? (string)$data['returnType'] : 'mixed';
        $this->isDeterministic    = isset($data['isDeterministic']) ? (bool)$data['isDeterministic'] : true;
        $this->isCacheable        = isset($data['isCacheable']) ? (bool)$data['isCacheable'] : true;
        $this->requiredPermission = isset($data['requiredPermission']) ? $data['requiredPermission'] : null;
        $this->deprecatedSince    = isset($data['deprecatedSince']) ? $data['deprecatedSince'] : null;
        $this->replacedBy         = isset($data['replacedBy']) ? $data['replacedBy'] : null;
    }

    /**
     * Convert to array for serialization/caching.
     *
     * @return array
     */
    public function toArray()
    {
        return array(
            'name'               => $this->name,
            'description'        => $this->description,
            'category'           => $this->category,
            'version'            => $this->version,
            'minArgs'            => $this->minArgs,
            'maxArgs'            => $this->maxArgs,
            'returnType'         => $this->returnType,
            'isDeterministic'    => $this->isDeterministic,
            'isCacheable'        => $this->isCacheable,
            'requiredPermission' => $this->requiredPermission,
            'deprecatedSince'    => $this->deprecatedSince,
            'replacedBy'         => $this->replacedBy,
        );
    }

    /**
     * Create from array (deserialization).
     *
     * @param array $data
     * @return Formula_Registry_FunctionMetadata
     */
    public static function fromArray(array $data)
    {
        return new self($data);
    }
}
