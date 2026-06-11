# NotrinosERP 1.0

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![PHP](https://img.shields.io/badge/PHP-5.6%20–%208.4-blue.svg)](https://php.net)
[![MySQL](https://img.shields.io/badge/MySQL-4.1%2B%20%2F%20MariaDB-blue.svg)](https://mariadb.org)

[<img src="https://github.com/notrinos/NotrinosERP/raw/master/themes/default/images/notrinos_erp.jpg" width="350" />](http://notrinos.com)

NotrinosERP is an open-source, web-based Enterprise Resource Planning (ERP) system written in PHP and MySQL. Version 1.0 adds a full **CRM**, **HRM/Payroll**, and **multi-level Approval Workflow** module — everything a small-to-medium business needs in a single, self-hosted application.

| | |
|---|---|
| **Demo** | [demo.notrinos.com/erp1.0](https://demo.notrinos.com/erp1.0) |
| **Forum** | [forums.notrinos.com](https://forums.notrinos.com) |
| **Wiki / Docs** | [support.notrinos.com/1.0](https://support.notrinos.com/1.0/index.php?n=Help.Help) |
| **License** | GNU GPL v3 or later |

![Entry screen](https://notrinos.com/misc/1.0-entry.jpg)
![Dashboard](https://notrinos.com/misc/1.0-dashboard.png)

---

## Table of Contents

1. [Features](#features)
2. [Requirements](#requirements)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Modules Overview](#modules-overview)
6. [Optional Modules](#optional-modules)
7. [Debugging](#debugging)
8. [Security](#security)
9. [Contributing](#contributing)

---

## Features

- **Multi-company** — manage multiple companies from a single installation
- **Multi-currency** — full foreign currency support with exchange rate management
- **Multi-language** — i18n/l10n via `.po`/`.mo` gettext files
- **Multi-user** — role-based access control with granular per-module permissions
- **Extensible** — hook/extension system; add functionality without patching core files
- **PHP 5.6 – 8.4 compatible** — runs on modern and legacy server stacks
- **Composer support** — installable as a Composer project

---

## Requirements

| Component | Minimum |
|---|---|
| Web server | Apache, Nginx, or IIS with `mod_rewrite` or equivalent |
| PHP | 5.6 – 8.4 (InnoDB PDO extension required) |
| Database | MySQL 4.1+ or any MariaDB version (InnoDB engine enabled) |
| Browser | Any modern browser with HTML5 support |

**Optional PHP extensions:** `gd` (chart/graph reports), `mbstring` (UTF-8 handling), `openssl` (TLS mail)

---

## Installation

### Manual

1. [Download the latest snapshot](https://github.com/notrinos/NotrinosERP/archive/refs/heads/main.zip) and unzip it.
2. Copy the entire contents into your web server document root (e.g. `public_html/`, `www/`, `htdocs/`).
3. Open your site in a browser: `https://yourdomain.com` (or `https://yourdomain.com/NotrinosERP` for a subdirectory).
4. Follow the on-screen installation wizard.
5. **After successful installation, delete the `install/` folder** — it is a security risk to leave it in place.

### Via Composer

```bash
composer create-project notrinos/notrinos-erp:dev-main my-erp-folder
```

---

## Configuration

Core settings are split across two files:

| File | Purpose |
|---|---|
| `config.default.php` | Shipped defaults — do **not** edit directly |
| `config.php` | Your local overrides — committed or excluded per your workflow |
| `config_db.php` | Database connection(s) — **never commit real credentials** |

Key variables in `config.default.php` (override in `config.php`):

```php
$debug          = 0;    // 1 = show SQL on DB errors
$show_sql       = 0;    // 1 = dump all SQL to page footer
$go_debug       = 0;    // 1 = basic debug; 2 = backtrace on failure
```

Multi-company database connections are defined as the `$db_connections` array in `config_db.php`.

---

## Modules Overview

NotrinosERP 1.0 ships the following built-in application modules:

### Sales (Customers)
Complete agreement-to-cash cycle with advanced pricing, discounting, commission, credit control, and returns management:

**Transactions**
- **Sales Agreements & Contracts** — blanket orders, framework agreements, and contracts with Draft → Confirmed → Active lifecycle; inline line-item editor; "Create Sales Order" from active agreements
- **Sales Quotations** — create and send price quotes with validity dates; quote templates for one-click quoting (required + optional/upsell products); mark won/lost with CRM pipeline integration
- **Sales Orders** — full order entry with configurable sales types, price lists, and multi-currency support
- **Direct Deliveries & Invoices** — shortcut paths bypassing the full quote→order→delivery→invoice flow
- **Template Deliveries & Invoices** — create from recurring invoice templates; batch-generate recurrent invoices
- **Prepaid Orders** — invoice against prepaid sales orders
- **Returns (RMA)** — customer-facing Return Material Authorization with reason tracking, authorization workflow, and downstream actions: WH Return, Credit Note, or Replacement Order
- **Customer Payments & Credit Notes** — freehand credits, invoice-linked credits, payment entry with multi-currency
- **Payment/Credit Allocation** — allocate customer payments and credit notes against outstanding invoices

**Pricing, Discounts & Commissions**
- **Sales Pricelists** — multiple price lists per currency with effective dates
- **Discount Programs** — rule-based discount engine supporting percentage, fixed-amount, quantity-tier, and buy-X-get-Y rewards; coupon generation with validity windows; stackable promotions
- **Commission Plans** — multi-tier commission structures (flat, tiered, quota-based); configurable calculation base (revenue, margin, quantity); period-based resets; sales person assignment
- **Sales Types** — configurable transaction categories with price and tax factors

**Credit Control**
- **Credit Control Dashboard** — at-risk customer identification; manual/automatic credit holds with configurable scope (orders, deliveries, invoices); hold release with audit trail
- **Credit Reviews** — record credit limit changes and risk score adjustments; batch risk evaluation
- **Credit Status Setup** — define credit status levels with reason codes

**Inquiries & Reports**
- **Sales Dashboard** — KPI overview: revenue, orders, conversion rate, receivables, pipeline; period comparison with trend indicators; date-range filtering
- **Transaction Inquiries** — quotation, order, agreement, RMA, and customer transaction histories; outstanding order/delivery views for dispatch planning
- **Analytics** — Sales Performance, Margin Analysis, Discount Effectiveness reports
- **Commission Inquiry** — track earned commissions and payouts per sales person

**Maintenance**
- Customers & Branches | Sales Groups | Sales People | Sales Areas
- Quotation Templates | Sales Pricelists | Sales Types
- Commission Plans | Discount Programs | Credit Status | Credit Control

### Purchases (Suppliers)
Complete purchase-to-pay cycle with sourcing workflow, vendor management, matching controls, and procurement planning:

**Transactions**
- **Purchase Requisitions** — formalize internal demand with requester, department, required-by dates, and estimated pricing; inline line-item editor
- **Request for Quotation (RFQ)** — issue competitive RFQs to suppliers with deadline/validity date controls; RFQ comparison matrix for side-by-side vendor response evaluation
- **Purchase Agreements** — blanket agreements and framework contracts with configurable buyer, delivery location, and payment terms; Draft → Confirmed → Active lifecycle
- **Purchase Orders** — full PO entry; outstanding PO maintenance with search; direct GRN/Invoice shortcuts
- **Goods Received Notes (GRN)** — receive items against POs or standalone; PO receive items workflow
- **Supplier Invoices & Credit Notes** — direct entry or PO-linked; fixed asset purchase support
- **Supplier Payments & Allocation** — multi-currency payments; allocation of payments and credit notes

**Sourcing & Vendor Management**
- **Vendor Evaluation** — configurable evaluation criteria with scoring methods (manual, calculated, formula); weighted scorecards with 0–100 scoring; evaluator assignment; period-based assessments
- **Vendor Scorecard** — aggregate performance view per vendor across evaluation periods
- **Vendor Pricelists** — maintain and compare supplier pricing across items and dates
- **Purchase Order Templates** — pre-define PO lines for frequently ordered items with default quantities and preferred suppliers

**Matching & Bill Control**
- **3-Way / 2-Way Matching** — PO → GRN → Invoice matching with configurable tolerances (percentage or fixed amount) per matching type (price, quantity, total variance)
- **Matching Actions** — warn, block, or require approval when tolerances are exceeded
- **Matching Exceptions** — central exception dashboard to review, approve, reject, or resolve matching variances with core approval workflow integration

**Procurement Planning**
- **Procurement Plan** — auto-generate replenishment suggestions (auto-reorder, demand-based, or manual); approve line-by-line; batch create POs from approved plan
- **Reorder Rules** — per-item replenishment rules with min/max levels, reorder quantities, preferred suppliers, lead times, and auto-create RFQ/PO flags

**Inquiries & Reports**
- **Purchase Dashboard** — spend overview, vendor performance, price variance analysis
- **Transaction Inquiries** — requisition, RFQ, agreement, PO, and supplier transaction histories
- **Analytics** — Purchase Spend Analysis, Vendor Performance, Purchase Price Variance reports

**Maintenance**
- Suppliers | Vendor Evaluations | Vendor Evaluation Criteria | Vendor Pricelists
- Purchase Order Templates | Purchase Matching Configuration | Reorder Rules

### Inventory
Comprehensive inventory management with full warehouse management system (WMS), advanced item tracking, and pricing/costing engines:

**Transactions**
- **Inventory Location Transfers** — move stock between locations with full audit trail
- **Inventory Adjustments** — quantity and value adjustments with reason codes

**Warehouse Management (WMS)**
- **Warehouse Dashboard** — real-time visibility of stock levels, movements, and warehouse KPIs
- **Warehouse Locations** — hierarchical location structure with zones, aisles, racks, and bins
- **Storage Categories** — capacity-aware storage classification with attribute-based bin assignment rules
- **Putaway Rules Engine** — configurable putaway strategies by item, category, or storage type with sequence ordering; "Test Putaway" preview tool for bin assignment validation
- **Removal Strategies** — FEFO, FIFO, LIFO, and custom removal rule configuration per storage category
- **Receipt Operations** — guided receiving workflows with quality check gates
- **Dispatch Operations** — outbound processing with shipment consolidation
- **Transfer Orders** — warehouse-to-warehouse transfer planning and execution
- **Routes & Rules** — configurable walking paths and routing logic for optimized pick paths
- **Wave/Batch Picking** — create, release, start, complete, and cancel picking waves; pick lists sorted by walking path; per-line confirmation with short-pick handling and alternate bin suggestions — 30%+ productivity improvement
- **Packing Stations** — structured pack operations with container tracking and shipment verification
- **Shipping** — carrier integration, label generation, and shipment tracking
- **Cycle Counting** — rolling count plans (ABC-class based, by location, or blind); count sessions with variance review; replace annual physical counts with continuous verification
- **Replenishment Engine** — demand-driven bin replenishment with min/max triggers
- **Material Requests** — formalize internal demand for non-sales stock consumption
- **Scrap Entry** — record and track scrap/waste with reason codes
- **Return Orders** — structured return processing with condition assessment and disposition routing
- **Cross-Dock / Drop-Ship** — monitor cross-dock candidates from pending SO lines; create drop-ship POs linked to sales orders; item eligibility and location configuration
- **Consignment & VMI** — receive, consume, and return vendor-owned stock; VMI min/max level configuration with alerts and stock level export
- **ABC Classification** — item ranking by value, velocity, or quantity with dynamic class thresholds
- **Mobile Scanner** — web-based barcode scanning for receive, putaway, pick, count, transfer, and ship operations; serial number lookup

**Advanced Item Management**
- **Serial Number Tracking** — full lifecycle tracking with serial inquiry and lifecycle audit; customer equipment register for installed-base management
- **Batch/Lot Management** — batch number assignment, inquiry, and lifecycle traceability
- **Expiry Date Tracking** — expiry dashboard with FEFO picking support and shelf-life monitoring
- **Quality Inspection** — configurable quality parameters per item; inspection entry with pass/fail disposition; link to batches and serials
- **Warranty Management** — warranty claim entry, tracking, and resolution; warranty provision settings and financial provisioning inquiry
- **Recall Campaigns** — manage product recalls with batch/serial targeting and status tracking
- **Full Lifecycle Traceability** — serial and batch lifecycle views with forward/backward trace
- **Regulatory Compliance** — compliance rule configuration for pharma, food, and regulated industries (e.g. GS1, FDA 21 CFR Part 11 readiness)
- **Barcode/QR/RFID Labels** — label generation and printing with configurable formats

**Pricing and Costs**
- **Sales Pricing** — manage selling prices by item, customer group, or currency
- **Purchasing Pricing** — manage purchase costs by item and supplier
- **Standard Costs** — update and maintain standard costing with variance tracking

**Inquiries & Reports**
- **Stock Movements & Status** — real-time quantity-on-hand, available-to-promise, and movement history
- **Serial & Batch Inquiries** — search by serial/batch number with full chain of custody
- **Expiry Dashboard** — visual expiry status with date-range filtering
- **Warranty Provision Inquiry** — track warranty exposure and provision balances
- **Inventory Reports** — valuation, stock status, and analytical reports

**Maintenance**
- Items | Foreign Item Codes | Sales Kits | Item Categories
- Inventory Locations | Units of Measure | Reorder Levels
- Serial Numbers | Batch/Lot Numbers | Quality Parameters
- Tracking Settings | Barcode Labels | Provision Settings | Regulatory Compliance

### General Ledger
Complete double-entry accounting with multi-currency support, budgeting, accruals, and full financial reporting:

**Transactions**
- **Payments** — record outgoing payments with multi-account allocation
- **Deposits** — record incoming deposits linked to bank accounts
- **Bank Account Transfers** — transfer funds between bank accounts with currency handling
- **Journal Entry** — general purpose journal vouchers with multi-line debit/credit entries; quick entry templates for recurring journal patterns
- **Budget Entry** — define fiscal year budgets per GL account with period-level detail
- **Revenue / Cost Accruals** — schedule and post accrual entries for revenue and expense recognition across periods
- **Bank Account Reconciliation** — reconcile bank statements against GL transactions with starting/ending balance verification

**Currency Management**
- **Currencies & Exchange Rates** — define currencies with ISO codes, symbols, and decimal places; maintain exchange rate history
- **Revaluation of Currency Accounts** — process currency revaluation for bank and GL accounts, generating automatic journal entries for unrealized gains/losses

**Inquiries & Reports**
- **Journal Inquiry** — search and review all journal entries with drill-down to source transactions
- **GL Account Inquiry** — view transaction history for any GL account with running balance
- **Bank Account Inquiry** — view all transactions affecting a bank account with current balance
- **Tax Inquiry** — review tax collected/paid with transaction-level detail
- **Trial Balance** — period-end trial balance with drill-down to account transactions
- **Balance Sheet Drilldown** — interactive balance sheet with account-level drill-down
- **Profit and Loss Drilldown** — interactive P&L with period comparison and drill-down
- **Banking Reports** — bank statement, reconciliation, and transaction reports
- **General Ledger Reports** — chart of accounts, journal reports, and GL analytics

**Maintenance**
- GL Accounts | GL Account Groups | GL Account Classes
- Bank Accounts | Quick Entries | Account Tags
- Currencies | Exchange Rates | Close Period | Revaluation

### Manufacturing
Complete production management from BOM definition through work order lifecycle:

**Transactions**
- **Work Order Entry** — create production orders with configurable types (standard, advanced); specify required quantity, start date, and delivery location
- **Outstanding Work Orders** — search and manage open work orders with status filtering
- **Work Order Release** — release approved work orders to the production floor; validates BOM components and work centre availability
- **Issue Items to Work Order** — record material issues against work orders with serial/batch tracking support for component traceability
- **Add Finished Products** — receive completed goods into inventory with optional QC acceptance
- **Work Order Costs** — review actual vs. standard costs per work order with material, labour, and overhead breakdowns

**Inquiries & Reports**
- **Costed Bill of Materials Inquiry** — view BOM structure with rolled-up material, labour, and overhead costs
- **Where-Used Inquiry** — trace where a component item is used across all BOMs and work orders
- **Work Order Inquiry** — search and view work orders by status, date range, item, or location
- **Manufacturing Reports** — work order status, production variance, and BOM reports

**Maintenance**
- **Bills of Material** — define multi-level BOMs with component quantities, work centres, and locations; supports manufactured items and sales kits
- **Work Centres** — define production centres with capacity and cost rate configuration

### Fixed Assets
Complete asset lifecycle management from acquisition through disposal with depreciation processing:

**Transactions**
- **Fixed Assets Purchase** — acquire assets via supplier invoice with automatic asset register creation
- **Fixed Assets Location Transfers** — move assets between locations while maintaining cost centre history
- **Fixed Assets Disposal** — write off or dispose of assets with gain/loss calculation
- **Fixed Assets Sale** — sell assets via sales invoice with automatic disposal and gain/loss posting
- **Process Depreciation** — run periodic depreciation (straight-line or diminishing balance) per asset class; generates GL journal entries automatically

**Inquiries & Reports**
- **Fixed Assets Movements** — view asset transaction history with acquisition, transfer, depreciation, and disposal events
- **Fixed Assets Inquiry** — detailed asset register view with current book value, accumulated depreciation, and net book value
- **Fixed Assets Reports** — depreciation schedule, asset register, and disposal reports

**Maintenance**
- **Fixed Assets** — manage asset master records (capitalization date, cost, salvage value, depreciation method, useful life)
- **Fixed Assets Locations** — define physical locations for asset tracking
- **Fixed Assets Categories** — classify assets by type for default depreciation settings
- **Fixed Assets Classes** — define depreciation parameters: method (straight-line / declining balance), rate, and GL accounts for cost, accumulated depreciation, and depreciation expense

### Dimensions (optional)
Project and cost-centre tagging system for cross-module transaction analysis. Enabled via Company Setup → `use_dimension`.

**Transactions**
- **Dimension Entry** — create dimensions (projects, cost centres, departments, campaigns) with reference, name, dates, and tags
- **Outstanding Dimensions** — view and close active dimensions

**Inquiries & Reports**
- **Dimension Inquiry** — search dimensions by reference, name, date range, or tag; view all transactions linked to a dimension
- **Dimension Reports** — dimension-level profitability and cost analysis reports

**Maintenance**
- **Dimension Tags** — tag dimensions for categorization and filtering

Dimensions can be tagged on transactions across Sales, Purchasing, Inventory, GL, and Manufacturing modules for consolidated reporting.

---

## Optional Modules

### CRM — Customer Relationship Management

Enable via **Company Setup → Optional Modules → CRM**.

| Area | Features |
|---|---|
| **Transactions** | New Lead, New Opportunity, Schedule Activity, Convert Lead, New Campaign, New Contract, New Appointment, Bulk Operations |
| **Reports & Inquiry** | Pipeline View, Lead Inquiry, Activity Inquiry, Campaign Inquiry, Pipeline Analysis, Win/Loss Report, Expected Revenue, Forecast Report, Lead Source Report, Team Performance |
| **Maintenance** | Manage Leads/Opportunities/Campaigns/Contracts/Appointments, Sales Teams, Lead Sources, Pipeline Stages, Lost Reasons, Activity Types, Activity Plans, Appointment Types |

Highlights:
- Unified Lead + Opportunity model (single table, `is_opportunity` flag)
- Configurable Kanban-style pipeline stages with probability
- Activity chaining — suggest or auto-trigger a next activity on completion
- Activity plans (pre-configured multi-step sequences)
- Campaign management with email sequences; ROI tracking
- Contract management with renewal alerts
- Self-service appointment scheduling
- Rule-based lead auto-assignment to sales teams
- Communication log survives Lead → Customer conversion
- Predictive lead scoring configuration

### HRM — Human Resources & Payroll

Enable via **Company Setup → Optional Modules → HR**.

| Area | Features |
|---|---|
| **Transactions** | Attendance Entry, Attendance Sheet, Leave Request/Approval, Overtime Request/Approval, Salary Revision, Employee Transfer, Payslip Entry, Payroll Processing, Payroll Approval, Payment Advice, Payment Batch, Loan Request/Repayment, Employee Separation |
| **Reports & Inquiry** | Attendance Report, Leave Balance, Payslip History, Payroll Summary, Employee Directory, Employee History, Department Costs, Loan Outstanding, Employee Transactions |
| **Maintenance** | Employees, Departments, Job Classifications, Positions, Pay Grades, Overtime Types, Leave Types & Policies, Deduction Codes, Attendance Deduction Rules, Work Shifts, Working Days, Holiday Calendar, End-of-Service Tiers, Pay Elements, Salary Structure, Tax Brackets, Statutory Deductions, Loan Types, Import/Export Employees |

Highlights:
- Complete attendance-to-payslip-to-GL payroll flow
- Configurable pay elements (fixed, percentage, formula)
- Country-agnostic salary structure with versioning
- Leave accrual, balance, and workflow
- Overtime request with approval chain
- Payroll batch processing and bank payment advice (WPS-compatible output)
- Employee loan management
- End-of-service (gratuity) calculation tiers
- Hook system for country-specific statutory deductions (e.g. GOSI, EPF)
- Translations extraction tool: `php hrm/tools/extract_translations.php`

### Approval Workflow System

A **universal, multi-level approval workflow** available for any transaction type.

- User-defined multi-level approval chains with role-based routing
- Value-based automatic approval thresholds
- Complete audit trail of all approval actions, edits, and comments
- Transaction number reserved at draft creation (no separate draft numbering)
- Approval dashboard across all modules
- HRM approvals (leave, overtime, payroll) run through the unified engine

---

### Debugging

Set these in `config.php` (or toggle via Company Setup → System Preferences):

```php
$debug    = 1;  // Show SQL on DB errors
$show_sql = 1;  // Dump all queries to page footer
$go_debug = 2;  // Show backtrace on failure
```

## Security

- Always delete the `install/` folder after installation.
- Never commit `config_db.php` with real credentials — add it to `.gitignore`.
- Pages are protected by `check_page_security()` using numeric access-level constants.
- CSRF protection is applied centrally via `includes/`.
- To report a vulnerability, see [SECURITY.md](SECURITY.md) or email **support@notrinos.com**.

---

## Contributing

1. Fork the repository and create a feature branch.
2. Follow existing code conventions (procedural style, full descriptive names, docblock comments on new functions).
3. Test against PHP 5.6 and PHP 8.x.
4. Submit a pull request with a clear description of the change.

Bug reports and feature requests: [forums.notrinos.com/forums/bugs-problems](https://forums.notrinos.com/forums/bugs-problems)

---
