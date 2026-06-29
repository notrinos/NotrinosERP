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
 * ERPFunctions — Built-in ERP-specific formula functions.
 *
 * Implements ERP domain functions that bridge between the formula engine
 * and NotrinosERP business modules. These functions are registered in the
 * FunctionRegistry but do NOT access the database directly — they delegate
 * to registered variable providers through the FormulaContext.
 *
 * Functions in this category:
 *   EXCHANGE_RATE, ACCOUNT_BALANCE, LEDGER_BALANCE,
 *   NET_SALARY, TAXABLE_INCOME, CURRENT_USER,
 *   STOCK_ON_HAND, ITEM_COST, ITEM_PRICE
 *
 * Security note: Functions that access financial/HR data declare a
 * requiredPermission in their metadata. The runtime checks these
 * permissions before invocation.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */

// ---------------------------------------------------------------------------
//  EXCHANGE_RATE — Get exchange rate for a currency
// ---------------------------------------------------------------------------

/**
 * EXCHANGE_RATE(currency_code, date) — Returns the exchange rate for a currency.
 *
 * Requires SA_GL_INQUIRY permission. Data is resolved through the
 * context's exchange rate provider or business data.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_ExchangeRateFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'EXCHANGE_RATE';
    }

    /**
     * Execute EXCHANGE_RATE(currency_code, date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [currency_code, date]
     * @return float Exchange rate
     * @throws Formula_Exceptions_RuntimeExecutionException If rate cannot be resolved
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $currencyCode = strtoupper((string)$arguments[0]);
        $date         = isset($arguments[1]) ? (string)$arguments[1] : date('Y-m-d');

        // Try to get rate from context business data (fast path)
        $rates = $context->getBusinessData('exchange_rates', null);

        if (is_array($rates) && isset($rates[$currencyCode])) {
            return (float)$rates[$currencyCode];
        }

        // If exchange rate is 1 (home currency), return 1
        $companyCtx = $context->getCompanyContext();
        if ($companyCtx !== null && strtoupper($companyCtx->getCurrency()) === $currencyCode) {
            return 1.0;
        }

        // Default: no rate available
        throw new Formula_Exceptions_RuntimeExecutionException(
            'Exchange rate not available for currency: ' . $currencyCode . ' on ' . $date
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'EXCHANGE_RATE',
            'description' => 'Returns the exchange rate for a given currency code on a specified date.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_GL_INQUIRY',
        ));
    }
}

// ---------------------------------------------------------------------------
//  ACCOUNT_BALANCE — Get GL account balance
// ---------------------------------------------------------------------------

/**
 * ACCOUNT_BALANCE(account_code) — Returns the current balance of a GL account.
 *
 * Requires SA_GL_INQUIRY permission.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_AccountBalanceFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ACCOUNT_BALANCE';
    }

    /**
     * Execute ACCOUNT_BALANCE(account_code).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [account_code]
     * @return float Account balance
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $accountCode = (string)$arguments[0];

        // Delegate to context business data (pre-loaded by the calling module)
        $balances = $context->getBusinessData('account_balances', null);

        if (is_array($balances) && isset($balances[$accountCode])) {
            return (float)$balances[$accountCode];
        }

        throw new Formula_Exceptions_RuntimeExecutionException(
            'Account balance not available for account: ' . $accountCode
            . '. Ensure account balances are provided in the formula context.'
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ACCOUNT_BALANCE',
            'description' => 'Returns the current balance of a GL account by account code.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_GL_INQUIRY',
        ));
    }
}

// ---------------------------------------------------------------------------
//  LEDGER_BALANCE — Get ledger balance for a period
// ---------------------------------------------------------------------------

/**
 * LEDGER_BALANCE(account_code, from_date, to_date) — Returns the ledger balance
 * for a GL account within a date range.
 *
 * Requires SA_GL_INQUIRY permission.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_LedgerBalanceFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'LEDGER_BALANCE';
    }

    /**
     * Execute LEDGER_BALANCE(account_code, from_date, to_date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [account_code, from_date, to_date]
     * @return float Ledger balance for the period
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $accountCode = (string)$arguments[0];
        $fromDate    = (string)$arguments[1];
        $toDate      = (string)$arguments[2];

        // Delegate to pre-loaded ledger data in context
        $ledgerData = $context->getBusinessData('ledger_balances', array());

        $key = $accountCode . '|' . $fromDate . '|' . $toDate;

        if (isset($ledgerData[$key])) {
            return (float)$ledgerData[$key];
        }

        throw new Formula_Exceptions_RuntimeExecutionException(
            'Ledger balance not available for account ' . $accountCode
            . ' from ' . $fromDate . ' to ' . $toDate
            . '. Ensure ledger balances are provided in the formula context.'
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'LEDGER_BALANCE',
            'description' => 'Returns the ledger balance of a GL account for a specified date range.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 3,
            'maxArgs'     => 3,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_GL_INQUIRY',
        ));
    }
}

// ---------------------------------------------------------------------------
//  NET_SALARY — Get employee net salary
// ---------------------------------------------------------------------------

/**
 * NET_SALARY(employee_id, period_end_date) — Returns the net salary of an employee.
 *
 * Requires SA_HRM_VIEW_SALARY permission.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_NetSalaryFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'NET_SALARY';
    }

    /**
     * Execute NET_SALARY(employee_id, period_end_date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [employee_id, period_end_date]
     * @return float Net salary
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $employeeId = (int)$arguments[0];
        $periodEnd  = isset($arguments[1]) ? (string)$arguments[1] : date('Y-m-d');

        // Delegate to pre-loaded salary data in context
        $salaryData = $context->getBusinessData('employee_salaries', array());
        $key = $employeeId . '|' . $periodEnd;

        if (isset($salaryData[$key])) {
            return (float)$salaryData[$key];
        }

        throw new Formula_Exceptions_RuntimeExecutionException(
            'Net salary not available for employee ' . $employeeId
            . ' for period ending ' . $periodEnd
            . '. Ensure salary data is provided in the formula context.'
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'NET_SALARY',
            'description' => 'Returns the net salary of an employee for a specified period.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_HRM_VIEW_SALARY',
        ));
    }
}

// ---------------------------------------------------------------------------
//  TAXABLE_INCOME — Get employee taxable income
// ---------------------------------------------------------------------------

/**
 * TAXABLE_INCOME(employee_id, period_end_date) — Returns the taxable income.
 *
 * Requires SA_HRM_VIEW_SALARY permission.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_TaxableIncomeFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'TAXABLE_INCOME';
    }

    /**
     * Execute TAXABLE_INCOME(employee_id, period_end_date).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [employee_id, period_end_date]
     * @return float Taxable income
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $employeeId = (int)$arguments[0];
        $periodEnd  = isset($arguments[1]) ? (string)$arguments[1] : date('Y-m-d');

        $incomeData = $context->getBusinessData('taxable_income', array());
        $key = $employeeId . '|' . $periodEnd;

        if (isset($incomeData[$key])) {
            return (float)$incomeData[$key];
        }

        throw new Formula_Exceptions_RuntimeExecutionException(
            'Taxable income not available for employee ' . $employeeId
            . ' for period ending ' . $periodEnd
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'TAXABLE_INCOME',
            'description' => 'Returns the taxable income of an employee for a specified period.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_HRM_VIEW_SALARY',
        ));
    }
}

// ---------------------------------------------------------------------------
//  CURRENT_USER — Get current user information
// ---------------------------------------------------------------------------

/**
 * CURRENT_USER() — Returns the current logged-in user's database ID.
 *
 * Resolved through the SecurityContext attached to the FormulaContext.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_CurrentUserFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'CURRENT_USER';
    }

    /**
     * Execute CURRENT_USER().
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments No arguments
     * @return int Current user ID, or 0 if not authenticated
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $securityCtx = $context->getSecurityContext();

        if ($securityCtx !== null) {
            return $securityCtx->getUserId();
        }

        return 0;
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'CURRENT_USER',
            'description' => 'Returns the database ID of the currently logged-in user. Returns 0 if no user is authenticated.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 0,
            'maxArgs'     => 0,
            'returnType'  => 'decimal',
            'isDeterministic' => false,
            'isCacheable'     => false,
        ));
    }
}

// ---------------------------------------------------------------------------
//  STOCK_ON_HAND — Get current stock quantity
// ---------------------------------------------------------------------------

/**
 * STOCK_ON_HAND(item_id, location_code) — Returns the quantity on hand.
 *
 * Requires SA_ITEMS_INQUIRY permission.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_StockOnHandFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'STOCK_ON_HAND';
    }

    /**
     * Execute STOCK_ON_HAND(item_id, location_code).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [item_id, location_code?]
     * @return float Quantity on hand
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $itemId       = (string)$arguments[0];
        $locationCode = isset($arguments[1]) ? (string)$arguments[1] : '';

        $stockData = $context->getBusinessData('stock_on_hand', array());
        $key = $itemId . ($locationCode ? '|' . $locationCode : '');

        if (isset($stockData[$key])) {
            return (float)$stockData[$key];
        }

        throw new Formula_Exceptions_RuntimeExecutionException(
            'Stock on hand not available for item: ' . $itemId
            . '. Ensure stock data is provided in the formula context.'
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'STOCK_ON_HAND',
            'description' => 'Returns the quantity on hand for a stock item, optionally filtered by location.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_ITEMS_INQUIRY',
        ));
    }
}

// ---------------------------------------------------------------------------
//  ITEM_COST — Get current item cost
// ---------------------------------------------------------------------------

/**
 * ITEM_COST(item_id) — Returns the standard cost of a stock item.
 *
 * Requires SA_ITEMS_INQUIRY permission.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_ItemCostFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ITEM_COST';
    }

    /**
     * Execute ITEM_COST(item_id).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [item_id]
     * @return float Standard cost
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $itemId = (string)$arguments[0];

        $costData = $context->getBusinessData('item_costs', array());

        if (isset($costData[$itemId])) {
            return (float)$costData[$itemId];
        }

        throw new Formula_Exceptions_RuntimeExecutionException(
            'Cost not available for item: ' . $itemId
            . '. Ensure item cost data is provided in the formula context.'
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ITEM_COST',
            'description' => 'Returns the standard cost of a stock item.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 1,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_ITEMS_INQUIRY',
        ));
    }
}

// ---------------------------------------------------------------------------
//  ITEM_PRICE — Get item price from a pricelist
// ---------------------------------------------------------------------------

/**
 * ITEM_PRICE(item_id, pricelist_id) — Returns the price from a specified pricelist.
 *
 * Requires SA_SALES_PRICE permission.
 *
 * @package Formula\Providers
 * @since   2.0.0
 */
class Formula_Providers_ERP_ItemPriceFunction implements Formula_Contracts_FormulaFunctionInterface
{
    /**
     * {@inheritdoc}
     */
    public function getName()
    {
        return 'ITEM_PRICE';
    }

    /**
     * Execute ITEM_PRICE(item_id, pricelist_id).
     *
     * @param Formula_Context_FormulaContext $context   Immutable execution context
     * @param array                          $arguments [item_id, pricelist_id?]
     * @return float Price
     */
    public function execute(Formula_Context_FormulaContext $context, array $arguments)
    {
        $itemId      = (string)$arguments[0];
        $pricelistId = isset($arguments[1]) ? (string)$arguments[1] : '';

        $priceData = $context->getBusinessData('item_prices', array());
        $key = $itemId . ($pricelistId ? '|' . $pricelistId : '');

        if (isset($priceData[$key])) {
            return (float)$priceData[$key];
        }

        throw new Formula_Exceptions_RuntimeExecutionException(
            'Price not available for item: ' . $itemId
            . '. Ensure pricing data is provided in the formula context.'
        );
    }

    /**
     * {@inheritdoc}
     */
    public function getMetadata()
    {
        return new Formula_Registry_FunctionMetadata(array(
            'name'        => 'ITEM_PRICE',
            'description' => 'Returns the price of a stock item from a specified pricelist.',
            'category'    => 'ERP',
            'version'     => '1.0',
            'minArgs'     => 1,
            'maxArgs'     => 2,
            'returnType'  => 'decimal',
            'isDeterministic' => true,
            'isCacheable'     => true,
            'requiredPermission' => 'SA_SALES_PRICE',
        ));
    }
}
