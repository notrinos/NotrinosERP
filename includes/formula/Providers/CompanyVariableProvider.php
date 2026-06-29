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
 * CompanyVariableProvider — Resolves Company.* namespace variables.
 *
 * Provides formula access to company-level settings and configuration
 * through the "Company" namespace. Variables are resolved lazily —
 * the provider only fetches data when a specific variable is referenced.
 *
 * Supported variables:
 *   Company.Id                 — Company database ID
 *   Company.Name               — Company name
 *   Company.Currency           — Default currency code (e.g., 'USD', 'SGD')
 *   Company.Locale             — Locale identifier (e.g., 'en_US')
 *   Company.Country            — Country code
 *   Company.FiscalYearStart    — Fiscal year start date
 *   Company.TaxId              — Company tax ID / registration number
 *   Company.Address            — Company address
 *   Company.Phone              — Company phone
 *   Company.Email              — Company email
 *   Company.Website            — Company website URL
 *   Company.Timezone           — Company timezone
 *
 * All Company variables are public (no special permission required)
 * as company identity is not considered sensitive.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_CompanyVariableProvider implements Formula_Contracts_VariableProviderInterface
{
    /**
     * {@inheritdoc}
     */
    public function supports($namespace)
    {
        return strcasecmp((string)$namespace, 'Company') === 0;
    }

    /**
     * {@inheritdoc}
     *
     * @param string                        $identifier The company attribute name
     * @param Formula_Context_FormulaContext $context    The execution context
     * @return mixed The resolved attribute value
     * @throws Formula_Exceptions_UnknownVariableException If the attribute is not recognized
     */
    public function resolve($identifier, Formula_Context_FormulaContext $context)
    {
        $key = strtoupper((string)$identifier);

        // Primary source: CompanyContext attached to FormulaContext
        $companyCtx = $context->getCompanyContext();

        if ($companyCtx !== null) {
            $value = $this->resolveFromCompanyContext($key, $companyCtx);
            if ($value !== null) {
                return $value;
            }
        }

        // Fallback: business data in context (pre-loaded by calling module)
        $companyData = $context->getBusinessData('company', array());

        if (!empty($companyData)) {
            $value = $this->resolveFromData($key, $companyData);
            if ($value !== null) {
                return $value;
            }
        }

        throw new Formula_Exceptions_UnknownVariableException(
            'Unknown company attribute: ' . $identifier,
            'Company',
            $identifier
        );
    }

    /**
     * Resolve from the CompanyContext sub-context.
     *
     * @param string                           $key      The uppercase attribute name
     * @param Formula_Context_CompanyContext $companyCtx The company context
     * @return mixed|null
     */
    private function resolveFromCompanyContext($key, Formula_Context_CompanyContext $companyCtx)
    {
        switch ($key) {
            case 'ID':
                $id = $companyCtx->getCompanyId();
                return $id > 0 ? $id : null;

            case 'CURRENCY':
                return $companyCtx->getCurrency();

            case 'LOCALE':
                return $companyCtx->getLocale();

            case 'COUNTRY':
                return $companyCtx->getCountry();

            case 'FISCALYEARSTART':
                return $companyCtx->getFiscalYearStart();

            case 'NAME':
                return $companyCtx->getMetadataValue('name');

            case 'TAXID':
                return $companyCtx->getMetadataValue('tax_id');

            case 'ADDRESS':
                return $companyCtx->getMetadataValue('address');

            case 'PHONE':
                return $companyCtx->getMetadataValue('phone');

            case 'EMAIL':
                return $companyCtx->getMetadataValue('email');

            case 'WEBSITE':
                return $companyCtx->getMetadataValue('website');

            case 'TIMEZONE':
                return $companyCtx->getMetadataValue('timezone');

            default:
                return $companyCtx->getMetadataValue(strtolower($key));
        }
    }

    /**
     * Resolve from a raw company data array (fallback).
     *
     * @param string $key  The uppercase attribute name
     * @param array  $data The company data array
     * @return mixed|null
     */
    private function resolveFromData($key, array $data)
    {
        $map = array(
            'ID'               => array('id', 'company_id'),
            'NAME'             => array('name', 'company_name', 'coy_name'),
            'CURRENCY'         => array('currency', 'curr_default', 'currency_code'),
            'LOCALE'           => array('locale'),
            'COUNTRY'          => array('country', 'country_code'),
            'FISCALYEARSTART'  => array('fiscal_year_start', 'fiscal_year_begin'),
            'TAXID'            => array('tax_id', 'gst_no', 'tax_number'),
            'ADDRESS'          => array('address', 'postal_address'),
            'PHONE'            => array('phone', 'phone_no', 'telephone'),
            'EMAIL'            => array('email', 'email_address'),
            'WEBSITE'          => array('website', 'web_url'),
            'TIMEZONE'         => array('timezone', 'time_zone'),
        );

        if (isset($map[$key])) {
            foreach ($map[$key] as $candidate) {
                if (isset($data[$candidate])) {
                    return $data[$candidate];
                }
            }
        }

        return null;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_ProviderMetadata(array(
            'namespaces'  => array('Company'),
            'version'     => '1.0',
            'description' => 'Resolves company attributes: Id, Name, Currency, Locale, Country, FiscalYearStart, and more.',
        ));
    }
}
