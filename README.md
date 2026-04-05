# NotrinosERP 1.0

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![PHP](https://img.shields.io/badge/PHP-5.6%20–%208.4-blue.svg)](https://php.net)
[![MySQL](https://img.shields.io/badge/MySQL-4.1%2B%20%2F%20MariaDB-blue.svg)](https://mariadb.org)

[<img src="https://github.com/notrinos/NotrinosERP/raw/master/themes/default/images/notrinos_erp.jpg" width="350" />](http://notrinos.com)

NotrinosERP is an open-source, web-based Enterprise Resource Planning (ERP) system written in PHP and MySQL. Version 1.0 adds a full **CRM**, **HRM/Payroll**, and **multi-level Approval Workflow** module — everything a small-to-medium business needs in a single, self-hosted application.

| | |
|---|---|
| **Demo** | [demo.notrinos.com/erp1.0](http://demo.notrinos.com/erp1.0) |
| **Forum** | [forums.notrinos.com](http://forums.notrinos.com) |
| **Wiki / Docs** | [support.notrinos.com/1.0](http://support.notrinos.com/1.0/index.php?n=Help.Help) |
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
10. [License](#license)

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
composer create-project notrinos/notrinos-erp:dev-master my-erp
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
$app_title      = 'NotrinosERP';
$use_popup_windows = 1;
```

Multi-company database connections are defined as the `$db_connections` array in `config_db.php`.

---

## Modules Overview

NotrinosERP 1.0 ships the following built-in application modules:

### Sales (Customers)
Full order-to-cash cycle:
- Sales Quotations → Orders → Deliveries → Invoices
- Direct Delivery and Direct Invoice shortcuts
- Recurrent (template) invoices
- Customer Payments, Credit Notes, and Allocation
- Reports: Customer & Sales Reports

### Purchases (Suppliers)
Purchase-to-pay cycle:
- Purchase Orders → GRNs → Supplier Invoices
- Payments, Credit Notes, and Allocation
- Reports: Supplier & Purchasing Reports

### Inventory
- Location Transfers and Adjustments
- Item Categories, Units of Measure, Reorder Levels
- Sales Kits and Foreign Item Codes
- Sales Pricing, Purchasing Pricing, Standard Costs
- Reports: Inventory Reports

### General Ledger
- Payments, Deposits, Bank Transfers
- Journal Entry, Budget Entry, Accruals
- Bank Account Reconciliation
- Currency management and Revaluation
- Drilldown: Trial Balance, Balance Sheet, Profit & Loss
- Reports: Banking Reports, GL Reports

### Manufacturing
- Work Order Entry and tracking
- Costed Bill of Materials Inquiry
- Where-Used Inquiry
- Reports: Manufacturing Reports

### Fixed Assets
- Asset Purchase, Location Transfer, Disposal, Sale
- Depreciation processing
- Asset Categories and Classes
- Reports: Fixed Assets Reports

### Dimensions (optional)
- Project/cost-centre tagging on transactions
- Dimension Inquiry and Reports
- Enabled via Company Setup → `use_dimension`

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

Install approval tables: `php install_approval_tables.php` (requires DB credentials in `config_db.php`).

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

Bug reports and feature requests: [forums.notrinos.com/t/bugs-problems](http://forums.notrinos.com/t/bugs-problems)

---

## License

NotrinosERP is released under the **GNU General Public License v3 or later**.  
See [LICENSE](LICENSE) or <https://www.gnu.org/licenses/gpl-3.0.html>.
