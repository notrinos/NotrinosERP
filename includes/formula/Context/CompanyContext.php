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
 * CompanyContext — Immutable company-specific data for formula execution.
 *
 * Carries the current company's configuration: currency, locale, fiscal
 * year, and any other company-level settings that formulas may reference.
 * This is a lightweight value object assembled by the calling module.
 *
 * The Formula Framework contains NO business logic about companies —
 * it simply stores whatever data the calling module provides.
 *
 * @package Formula\Context
 * @since   2.0.0
 */
class Formula_Context_CompanyContext
{
    /** @var int Company database ID */
    private $companyId;

    /** @var string|null Default currency code (e.g., 'USD', 'SGD') */
    private $currency;

    /** @var string|null Locale identifier (e.g., 'en_US', 'fr_FR') */
    private $locale;

    /** @var string|null Country code (e.g., 'US', 'SG') */
    private $country;

    /** @var string|null Fiscal year start date in SQL format */
    private $fiscalYearStart;

    /** @var array Additional company metadata (extensible) */
    private $metadata;

    /**
     * Construct company context.
     *
     * @param array $data Associative array with optional keys:
     *                    companyId, currency, locale, country,
     *                    fiscalYearStart, metadata
     */
    public function __construct(array $data = array())
    {
        $this->companyId       = isset($data['companyId']) ? (int)$data['companyId'] : 0;
        $this->currency        = isset($data['currency']) ? (string)$data['currency'] : null;
        $this->locale          = isset($data['locale']) ? (string)$data['locale'] : null;
        $this->country         = isset($data['country']) ? (string)$data['country'] : null;
        $this->fiscalYearStart = isset($data['fiscalYearStart']) ? (string)$data['fiscalYearStart'] : null;
        $this->metadata        = isset($data['metadata']) ? (array)$data['metadata'] : array();
    }

    /**
     * Get the company database ID.
     *
     * @return int
     */
    public function getCompanyId()
    {
        return $this->companyId;
    }

    /**
     * Get the default currency code.
     *
     * @return string|null Currency code or null if not set
     */
    public function getCurrency()
    {
        return $this->currency;
    }

    /**
     * Get the locale identifier.
     *
     * @return string|null Locale or null if not set
     */
    public function getLocale()
    {
        return $this->locale;
    }

    /**
     * Get the country code.
     *
     * @return string|null Country code or null if not set
     */
    public function getCountry()
    {
        return $this->country;
    }

    /**
     * Get the fiscal year start date in SQL format (YYYY-MM-DD).
     *
     * @return string|null Fiscal year start or null if not set
     */
    public function getFiscalYearStart()
    {
        return $this->fiscalYearStart;
    }

    /**
     * Get additional company metadata by key.
     *
     * @param string $key     Metadata key
     * @param mixed  $default Default value if key not found
     * @return mixed
     */
    public function getMetadata($key, $default = null)
    {
        return isset($this->metadata[$key]) ? $this->metadata[$key] : $default;
    }

    /**
     * Get all company metadata.
     *
     * @return array
     */
    public function getAllMetadata()
    {
        return $this->metadata;
    }
}
