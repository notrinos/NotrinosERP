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
 * ProviderMetadata — Value object describing a registered variable provider.
 *
 * Carries metadata about a variable provider: the namespaces it owns,
 * its version, and a human-readable description.
 *
 * @package Formula\Registry
 * @since   2.0.0
 */
class Formula_Registry_ProviderMetadata
{
    /** @var string[] Namespaces this provider handles */
    public $namespaces;

    /** @var string Provider version */
    public $version;

    /** @var string|null Human-readable description */
    public $description;

    /**
     * @param array $data Associative array with keys: namespaces, version, description
     */
    public function __construct(array $data = array())
    {
        $this->namespaces  = isset($data['namespaces']) ? (array)$data['namespaces'] : array();
        $this->version     = isset($data['version']) ? (string)$data['version'] : '1.0';
        $this->description = isset($data['description']) ? (string)$data['description'] : null;
    }

    /**
     * @return array
     */
    public function toArray()
    {
        return array(
            'namespaces'  => $this->namespaces,
            'version'     => $this->version,
            'description' => $this->description,
        );
    }

    /**
     * @param array $data
     * @return Formula_Registry_ProviderMetadata
     */
    public static function fromArray(array $data)
    {
        return new self($data);
    }
}
