# HRM Extension Hooks

This document defines the supported hook API for HRM extensions in NotrinosERP-1.0 Phase 4.

## Runtime API

- Register a callback: `hrm_register_hook(string $event, callable $callback, int $priority = 10)`
- Fire callbacks: `hrm_fire_hook(string $event, &...$args)`
- Check registration: `hrm_has_hook(string $event)`
- Remove callbacks: `hrm_remove_hook(string $event, $callback = null)`

Hook functions are implemented in `hrm/includes/hrm_hooks.inc`. Callbacks run in ascending priority order. Lower numbers run earlier.

## Supported Events

### Payroll

- `before_calculate_payslip(&$employee, $from_date, $to_date)`
  Purpose: adjust employee context before any payroll calculation begins.
- `after_calculate_basic(&$payslip_doc, $employee)`
  Purpose: add or override values immediately after basic salary is resolved.
- `after_calculate_earnings(&$payslip_doc, $employee)`
  Purpose: inject earnings such as bonuses, allowances, or country-specific benefits.
- `before_calculate_deductions(&$payslip_doc, $employee)`
  Purpose: add pre-deduction adjustments before tax, statutory, and loan logic runs.
- `after_calculate_payslip(&$payslip_doc, $employee)`
  Purpose: final payroll mutation point before posting or persistence.
- `before_post_payslip(&$payslip_doc)`
  Purpose: inspect or adjust posting-ready payroll data.
- `after_post_payslip($payslip_id, $gl_trans_no)`
  Purpose: trigger downstream integrations after a payslip is stored and posted.
- `on_void_payslip($payslip_id)`
  Purpose: clean up extension-owned data for voided payslips.

### Leave

- `on_leave_request_submitted($request_id, $employee_id)`
- `on_leave_status_change($request_id, $new_status)`

### Employee

- `on_employee_created($employee_id)`
- `on_employee_updated($employee_id, $old_data, $new_data)`
- `on_employee_hired($employee_id)`
- `on_employee_transferred($employee_id, $history_id)`
- `on_employee_separated($employee_id, $reason)`

### Attendance

- `on_attendance_saved($employee_id, $date)`
- `on_attendance_import($rows)`

### Documents

- `on_document_uploaded($doc_id, $employee_id)`
- `on_document_expiring($doc_id, $employee_id, $days_remaining)`

## Callback Rules

- Mutate only the arguments that are explicitly passed by reference.
- Do not echo output inside hook callbacks.
- Throwing fatal errors from a hook will break the parent HRM transaction.
- Keep DB writes idempotent when hooking into payroll or import operations.
- Validate any extension-owned GL account before appending payroll lines.

## Example

```php
hrm_register_hook('after_calculate_earnings', 'my_country_bonus_hook', 20);

/**
 * Add a country-specific payroll bonus after standard earnings are calculated.
 *
 * @param payslip_doc $payslip_doc
 * @param array $employee
 * @return void
 */
function my_country_bonus_hook(&$payslip_doc, $employee) {
    if (empty($employee['nationality']) || $employee['nationality'] !== 'SA')
        return;

    $bonus = round($payslip_doc->basic_salary * 0.05, 2);
    if ($bonus <= 0)
        return;

    $payslip_doc->add_line(
        0,
        _('Localization Bonus'),
        HRM_ELEM_ALLOWANCE,
        0,
        HRM_AMTTYPE_FIXED,
        $bonus,
        0,
        $bonus,
        $bonus,
        '6000',
        null,
        1,
        9500
    );

    $payslip_doc->recalculate_totals();
}
```

## Recommended Phase 4 Workflow

1. Register hooks from an extension bootstrap file loaded after HRM includes.
2. Validate the hook on a non-production payroll period first.
3. Run `php hrm/tools/extract_translations.php` after adding new user-facing strings.
4. Run the migration validation page and payroll regression smoke test before release.
